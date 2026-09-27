#include "Blueprint/RpgAnimationAssetTools.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimLayerInterface.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "AnimationGraph.h"
#include "AnimationGraphSchema.h"
#include "AnimGraphNode_LinkedAnimLayer.h"
#include "AnimGraphNode_LinkedInputPose.h"
#include "Chooser.h"
#include "ChooserPropertyAccess.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "JsonObjectConverter.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogRpgAnimationAssetTools, Log, All);

namespace
{
	bool CanEdit(const UObject* Asset)
	{
		if (!IsInGameThread() || !GIsEditor || !GEditor || !GEngine || !IsValid(Asset) || !Asset->IsAsset()
			|| !URpgAnimationAssetTools::IsProjectContentPackage(Asset->GetOutermost()->GetName())) return false;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE) return false;
		}
		const UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
		return !Blueprint || (!Blueprint->bBeingCompiled && !Blueprint->bIsRegeneratingOnLoad && !Blueprint->bQueuedForCompilation);
	}

	bool Fail(const FString& Message)
	{
		UE_LOG(LogRpgAnimationAssetTools, Error, TEXT("%s"), *Message);
		return false;
	}

	// JsonObjectConverter supports partial objects, but silently ignores unknown keys. Check those
	// recursively first, including concrete instanced structs, rather than accepting misspelled tuning.
	bool CheckJsonFields(const TSharedPtr<FJsonValue>& Value, const FProperty* Property);
	bool CheckJsonObject(const TSharedPtr<FJsonObject>& Object, const UStruct* Type)
	{
		if (!Object || !Type) return false;
		for (const auto& Pair : Object->Values)
		{
			if (Pair.Key == TEXT("_structType")) continue;
			const FProperty* Property = FindFProperty<FProperty>(Type, FName(Pair.Key));
			if (!Property || !CheckJsonFields(Pair.Value, Property)) return Fail(TEXT("Unknown or invalid JSON field: ") + FString(Pair.Key));
		}
		return true;
	}

	bool CheckJsonFields(const TSharedPtr<FJsonValue>& Value, const FProperty* Property)
	{
		if (!Value) return false;
		if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
		{
			const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
			if (!Value->TryGetArray(Values)) return false;
			for (const auto& Element : *Values) if (!CheckJsonFields(Element, Array->Inner)) return false;
		}
		else if (const FStructProperty* Struct = CastField<FStructProperty>(Property); Struct && Value->Type == EJson::Object)
		{
			const TSharedPtr<FJsonObject> Object = Value->AsObject();
			const UStruct* Type = Struct->Struct;
			if (Type == FInstancedStruct::StaticStruct())
			{
				if (Object->Values.IsEmpty()) return true;
				FString Path;
				if (!Object->TryGetStringField(TEXT("_structType"), Path)) return false;
				Type = LoadObject<UScriptStruct>(nullptr, *Path);
			}
			return CheckJsonObject(Object, Type);
		}
		else if (CastField<FObjectPropertyBase>(Property) && Value->Type == EJson::Object)
		{
			// These tools reference existing assets/classes, never construct arbitrary nested UObjects.
			return false;
		}
		return true;
	}

	bool ValidateChooser(UChooserTable* Chooser)
	{
		if (Chooser->RootChooser || !Chooser->NestedChoosers.IsEmpty() || !Chooser->NestedObjects.IsEmpty())
			return Fail(TEXT("Only root Choosers without embedded tables/objects are supported"));
		if (Chooser->ResultType != EObjectChooserResultType::NoPrimaryResult && !Chooser->OutputObjectType)
			return Fail(TEXT("Chooser requires an output class"));
		for (const FInstancedStruct& Context : Chooser->ContextData)
			if (!Context.IsValid() || !Context.GetScriptStruct()->IsChildOf(FContextObjectTypeBase::StaticStruct())) return Fail(TEXT("Invalid Chooser context struct"));
		for (const FInstancedStruct& Result : Chooser->ResultsStructs)
			if (!Result.IsValid() || !Result.GetScriptStruct()->IsChildOf(FObjectChooserBase::StaticStruct())) return Fail(TEXT("Invalid Chooser result struct"));
		if (Chooser->FallbackResult.IsValid() && !Chooser->FallbackResult.GetScriptStruct()->IsChildOf(FObjectChooserBase::StaticStruct()))
			return Fail(TEXT("Invalid Chooser fallback struct"));
		if (!Chooser->DisabledRows.IsEmpty() && Chooser->DisabledRows.Num() != Chooser->ResultsStructs.Num())
			return Fail(TEXT("DisabledRows must be empty or match ResultsStructs"));
		for (FInstancedStruct& Column : Chooser->ColumnsStructs)
		{
			if (!Column.IsValid() || !Column.GetScriptStruct()->IsChildOf(FChooserColumnBase::StaticStruct())) return Fail(TEXT("Invalid Chooser column struct"));
			FChooserColumnBase& Base = Column.GetMutable<FChooserColumnBase>();
			const FName RowsName = Base.RowValuesPropertyName();
			if (!RowsName.IsNone())
			{
				const FArrayProperty* Rows = FindFProperty<FArrayProperty>(Column.GetScriptStruct(), RowsName);
				if (!Rows || FScriptArrayHelper(Rows, Rows->ContainerPtrToValuePtr<void>(Column.GetMutableMemory())).Num() != Chooser->ResultsStructs.Num())
					return Fail(TEXT("Chooser column row count differs from result count"));
			}
			if (const FInstancedStruct* Input = Base.GetInputValuePtr(); Input && Input->IsValid())
			{
				const UScriptStruct* Expected = Base.GetInputBaseType();
				if (!Expected || !Input->GetScriptStruct()->IsChildOf(Expected)) return Fail(TEXT("Invalid Chooser column input family"));
			}
		}
		Chooser->Compile(true);
		if (Chooser->FallbackResult.IsValid()) Chooser->FallbackResult.GetMutable<FObjectChooserBase>().Compile(Chooser, true);
		FText Reason;
		for (FInstancedStruct& Column : Chooser->ColumnsStructs)
		{
			if (const FChooserParameterBase* Input = Column.GetMutable<FChooserColumnBase>().GetInputValue(); Input && Input->HasCompileErrors(Reason)) return Fail(Reason.ToString());
		}
		for (FInstancedStruct& Result : Chooser->ResultsStructs)
			if (Result.GetMutable<FObjectChooserBase>().HasCompileErrors(Reason)) return Fail(Reason.ToString());
		if (Chooser->FallbackResult.IsValid() && Chooser->FallbackResult.GetMutable<FObjectChooserBase>().HasCompileErrors(Reason)) return Fail(Reason.ToString());
		return true;
	}
}

bool URpgAnimationAssetTools::IsProjectContentPackage(const FString& PackageName)
{
	FString Filename;
	return FPackageName::IsValidLongPackageName(PackageName) && !PackageName.StartsWith(TEXT("/Script/"))
		&& FPackageName::TryConvertLongPackageNameToFilename(PackageName, Filename)
		&& FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(Filename), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
}

UEdGraph* URpgAnimationAssetTools::AddAnimationLayer(UAnimBlueprint* Blueprint, FName LayerName, FName InputPoseName, const FString& InputsJson)
{
	if (!CanEdit(Blueprint) || LayerName.IsNone() || UAnimBlueprint::FindRootAnimBlueprint(Blueprint)
		|| FKismetNameValidator(Blueprint).IsValid(LayerName.ToString()) != EValidatorResult::Ok) return nullptr;
	TArray<FAnimBlueprintFunctionPinInfo> Inputs;
	TArray<TSharedPtr<FJsonValue>> JsonInputs;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(InputsJson), JsonInputs) || (InputPoseName.IsNone() && !JsonInputs.IsEmpty())) return nullptr;
	TSet<FName> Names;
	if (!InputPoseName.IsNone()) Names.Add(InputPoseName);
	for (const auto& Value : JsonInputs)
	{
		FAnimBlueprintFunctionPinInfo Input;
		if (Value->Type != EJson::Object || !CheckJsonObject(Value->AsObject(), FAnimBlueprintFunctionPinInfo::StaticStruct())
			|| !FJsonObjectConverter::JsonObjectToUStruct(Value->AsObject().ToSharedRef(), &Input)
			|| Input.Name.IsNone() || Names.Contains(Input.Name) || Input.Type.PinCategory.IsNone()
			|| Input.Type.PinCategory == UEdGraphSchema_K2::PC_Exec || Input.Type.PinCategory == UEdGraphSchema_K2::PC_Wildcard
			|| UAnimationGraphSchema::IsPosePin(Input.Type)) return nullptr;
		Names.Add(Input.Name);
		Inputs.Add(Input);
	}
	TArray<UAnimGraphNode_LinkedInputPose*> ExistingInputs;
	FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, ExistingInputs);
	for (const UAnimGraphNode_LinkedInputPose* Existing : ExistingInputs)
	{
		if (Names.Contains(Existing->Node.Name)) return nullptr;
		for (const FAnimBlueprintFunctionPinInfo& Input : Existing->Inputs) if (Names.Contains(Input.Name)) return nullptr;
	}
	const FScopedTransaction Transaction(NSLOCTEXT("RpgAnimationAssetTools", "AddLayer", "Add Animation Layer"));
	Blueprint->Modify();
	UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, LayerName, UAnimationGraph::StaticClass(), UAnimationGraphSchema::StaticClass());
	FBlueprintEditorUtils::AddDomainSpecificGraph(Blueprint, Graph);
	if (!InputPoseName.IsNone())
	{
		FGraphNodeCreator<UAnimGraphNode_LinkedInputPose> Creator(*Graph);
		UAnimGraphNode_LinkedInputPose* Input = Creator.CreateNode();
		Creator.Finalize(); // PostPlacedNewNode assigns a default name; set the requested signature afterwards.
		Input->Node.Name = InputPoseName;
		Input->Inputs = MoveTemp(Inputs);
		Input->NodePosX = -300;
		Input->ReconstructNode();
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	return Graph;
}

bool URpgAnimationAssetTools::ConfigureLinkedAnimationLayer(UAnimGraphNode_LinkedAnimLayer* Node,
	TSubclassOf<UAnimLayerInterface> InterfaceClass, FName LayerName, TSubclassOf<UAnimInstance> InstanceClass)
{
	UBlueprint* Blueprint = Node ? FBlueprintEditorUtils::FindBlueprintForNode(Node) : nullptr;
	UFunction* Function = InterfaceClass ? InterfaceClass->FindFunctionByName(LayerName) : nullptr;
	if (!CanEdit(Blueprint) || !Blueprint->IsA<UAnimBlueprint>() || !IsValid(Node) || !Function
		|| !Function->HasMetaData(TEXT("AnimBlueprintFunction"))
		|| (InstanceClass && !InstanceClass->ImplementsInterface(InterfaceClass))) return false;
	// Conformation tracks the implemented graph's interface identity, not the host graph's own GUID.
	// Require the real interface implementation before binding, just as the editor's layer picker does.
	FGuid LayerGuid;
	for (const FBPInterfaceDescription& Description : Blueprint->ImplementedInterfaces)
	{
		if (Description.Interface.Get() != InterfaceClass.Get()) continue;
		for (const UEdGraph* Graph : Description.Graphs)
		{
			if (Graph && Graph->GetFName() == LayerName) LayerGuid = Graph->InterfaceGuid;
		}
	}
	if (!LayerGuid.IsValid()) return Fail(TEXT("Implement and compile the animation layer interface on the host Blueprint before binding its node"));
	FProperty* LayerProperty = FindFProperty<FProperty>(FAnimNode_LinkedAnimLayer::StaticStruct(), GET_MEMBER_NAME_CHECKED(FAnimNode_LinkedAnimLayer, Layer));
	if (!LayerProperty) return false;
	const FScopedTransaction Transaction(NSLOCTEXT("RpgAnimationAssetTools", "BindLayer", "Configure Linked Animation Layer"));
	Blueprint->Modify();
	Node->Modify();
	Node->Node.Interface = InterfaceClass;
	Node->Node.Layer = LayerName;
	Node->Node.InstanceClass = InstanceClass;
	Node->InterfaceGuid = LayerGuid;
	// The engine's property-change path sets the protected FunctionReference before reconstructing.
	// Writing Node.Layer alone leaves that reference empty, so reconstruction loses the name and pose pins.
	FPropertyChangedEvent LayerChanged(LayerProperty, EPropertyChangeType::ValueSet);
	Node->PostEditChangeProperty(LayerChanged);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	return Node->Node.Layer == LayerName && Node->Node.Interface == InterfaceClass
		&& Node->InterfaceGuid == LayerGuid;
}

bool URpgAnimationAssetTools::SetLocalFunctionThreadSafety(UBlueprint* Blueprint, FName FunctionName, bool bThreadSafe)
{
	if (!CanEdit(Blueprint)) return false;
	UEdGraph* Graph = nullptr;
	for (UEdGraph* Candidate : Blueprint->FunctionGraphs) if (Candidate && Candidate->GetFName() == FunctionName) Graph = Candidate;
	if (!Graph || Graph->GetOuter() != Blueprint) return false;
	TArray<UK2Node_FunctionEntry*> Entries;
	Graph->GetNodesOfClass(Entries);
	if (Entries.Num() != 1) return false;
	if (Entries[0]->MetaData.bThreadSafe == bThreadSafe) return true;
	const FScopedTransaction Transaction(NSLOCTEXT("RpgAnimationAssetTools", "ThreadSafety", "Set Blueprint Function Thread Safety"));
	Blueprint->Modify();
	Entries[0]->Modify();
	Entries[0]->MetaData.bThreadSafe = bThreadSafe;
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	return true;
}

bool URpgAnimationAssetTools::SetVariableTooltip(UBlueprint* Blueprint, FName VariableName, const FString& Tooltip)
{
	if (!CanEdit(Blueprint) || Tooltip.IsEmpty() || FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VariableName) == INDEX_NONE) return false;
	const FScopedTransaction Transaction(NSLOCTEXT("RpgAnimationAssetTools", "VariableTooltip", "Document Blueprint Variable"));
	Blueprint->Modify();
	FBlueprintEditorUtils::SetBlueprintVariableMetaData(Blueprint, VariableName, nullptr, TEXT("Tooltip"), Tooltip);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	return true;
}

UAnimGraphNode_LinkedAnimLayer* URpgAnimationAssetTools::CreateLinkedAnimationLayerNode(UEdGraph* Graph,
	TSubclassOf<UAnimLayerInterface> InterfaceClass, FName LayerName)
{
	UBlueprint* Blueprint = Graph ? FBlueprintEditorUtils::FindBlueprintForGraph(Graph) : nullptr;
	UFunction* Function = InterfaceClass ? InterfaceClass->FindFunctionByName(LayerName) : nullptr;
	if (!CanEdit(Blueprint) || !Blueprint->IsA<UAnimBlueprint>() || !Graph->GetSchema()->IsA<UAnimationGraphSchema>()
		|| !Function || !Function->HasMetaData(TEXT("AnimBlueprintFunction"))) return nullptr;
	const FScopedTransaction Transaction(NSLOCTEXT("RpgAnimationAssetTools", "CreateLinkedLayer", "Add Linked Animation Layer Node"));
	Blueprint->Modify();
	Graph->Modify();
	FGraphNodeCreator<UAnimGraphNode_LinkedAnimLayer> Creator(*Graph);
	UAnimGraphNode_LinkedAnimLayer* Node = Creator.CreateNode();
	Creator.Finalize();
	if (!ConfigureLinkedAnimationLayer(Node, InterfaceClass, LayerName, nullptr))
	{
		Graph->RemoveNode(Node);
		return nullptr;
	}
	return Node;
}

bool URpgAnimationAssetTools::RegisterAnimationSlot(USkeleton* Skeleton, FName SlotName)
{
	if (!CanEdit(Skeleton) || SlotName.IsNone()) return false;
	const FScopedTransaction Transaction(NSLOCTEXT("RpgAnimationAssetTools", "RegisterSlot", "Register Animation Slot"));
	Skeleton->Modify();
	Skeleton->RegisterSlotNode(SlotName);
	// Compilation may already have registered this slot without dirtying the skeleton.
	// Explicit authoring persists that registration even when ContainsSlotName is true.
	Skeleton->MarkPackageDirty();
	return Skeleton->ContainsSlotName(SlotName);
}

int32 URpgAnimationAssetTools::SetBlendSpaceSamples(UBlendSpace* BlendSpace, const TArray<UAnimSequence*>& Animations, const TArray<FVector>& Positions)
{
	if (!CanEdit(BlendSpace) || Animations.IsEmpty() || Animations.Num() != Positions.Num()) return -1;
	TStrongObjectPtr<UBlendSpace> Candidate(DuplicateObject<UBlendSpace>(BlendSpace, GetTransientPackage()));
	if (!Candidate) return -1;
	for (int32 Index = Candidate->GetNumberOfBlendSamples() - 1; Index >= 0; --Index) Candidate->DeleteSample(Index);
	for (int32 Index = 0; Index < Animations.Num(); ++Index)
	{
		if (!IsValid(Animations[Index]) || Positions[Index].ContainsNaN()) return -1;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const FBlendParameter& Parameter = BlendSpace->GetBlendParameter(Axis);
			if (Positions[Index][Axis] < Parameter.Min || Positions[Index][Axis] > Parameter.Max) return -1;
		}
		if (Candidate->AddSample(Animations[Index], Positions[Index]) == INDEX_NONE) return -1;
	}
	Candidate->ResampleData();
	if (Candidate->GetNumberOfBlendSamples() != Animations.Num()) return -1;
	for (const FBlendSample& Sample : Candidate->GetBlendSamples()) if (!Sample.bIsValid) return -1;
	const FScopedTransaction Transaction(NSLOCTEXT("RpgAnimationAssetTools", "BlendSamples", "Set BlendSpace Samples"));
	BlendSpace->Modify();
	// Copy only the validated authoring samples, never transient ownership or generated-class references.
	FArrayProperty* Samples = FindFProperty<FArrayProperty>(UBlendSpace::StaticClass(), TEXT("SampleData"));
	check(Samples);
	Samples->CopyCompleteValue_InContainer(BlendSpace, Candidate.Get());
	BlendSpace->ResampleData();
	BlendSpace->PostEditChange();
	BlendSpace->MarkPackageDirty();
	return Animations.Num();
}

bool URpgAnimationAssetTools::ConfigureChooser(UChooserTable* Chooser, const FString& ConfigurationJson)
{
	if (!CanEdit(Chooser)) return false;
	TSharedPtr<FJsonObject> Json;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ConfigurationJson), Json) || !Json || Json->Values.IsEmpty()) return false;
	const TSet<FName> Allowed{TEXT("OutputObjectType"), TEXT("ResultType"), TEXT("ContextData"), TEXT("ColumnsStructs"), TEXT("ResultsStructs"), TEXT("FallbackResult"), TEXT("DisabledRows")};
	TStrongObjectPtr<UChooserTable> Candidate(DuplicateObject<UChooserTable>(Chooser, GetTransientPackage()));
	if (!Candidate) return false;
	TArray<FProperty*> Changed;
	for (const auto& Pair : Json->Values)
	{
		const FName Name(Pair.Key);
		FProperty* Property = FindFProperty<FProperty>(UChooserTable::StaticClass(), Name);
		if (!Allowed.Contains(Name) || !Property || !CheckJsonFields(Pair.Value, Property)) return false;
		void* Destination = Property->ContainerPtrToValuePtr<void>(Candidate.Get());
		// Empty the destination so _structType can change an existing instanced-struct type deliberately.
		Property->ClearValue(Destination);
		if (!FJsonObjectConverter::JsonValueToUProperty(Pair.Value, Property, Destination)) return false;
		Changed.Add(Property);
	}
	if (!ValidateChooser(Candidate.Get())) return false;
	const FScopedTransaction Transaction(NSLOCTEXT("RpgAnimationAssetTools", "Chooser", "Configure Chooser Table"));
	Chooser->Modify();
	for (FProperty* Property : Changed) Property->CopyCompleteValue_InContainer(Chooser, Candidate.Get());
	Chooser->Compile(true);
	if (Chooser->FallbackResult.IsValid()) Chooser->FallbackResult.GetMutable<FObjectChooserBase>().Compile(Chooser, true);
	Chooser->PostEditChange();
	Chooser->MarkPackageDirty();
	return true;
}
