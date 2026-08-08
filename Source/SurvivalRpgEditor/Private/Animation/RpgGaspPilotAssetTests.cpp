#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AlphaBlend.h"
#include "AnimGraph/AnimGraphNode_OrientationWarping.h"
#include "AnimGraph/AnimGraphNode_Steering.h"
#include "AnimGraphNode_BlendStackInput.h"
#include "AnimGraphNode_BlendStackResult.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/InputScaleBias.h"
#include "AnimationBlendStackGraph.h"
#include "AnimGraphNode_MotionMatching.h"
#include "AnimGraphNode_PoseSearchHistoryCollector.h"
#include "AnimGraphNode_ResetRoot.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_Slot.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "BoneControllers/AnimNode_OrientationWarping.h"
#include "BoneControllers/AnimNode_OffsetRootBone.h"
#include "BoneControllers/AnimNode_ResetRoot.h"
#include "BoneControllers/AnimNode_Steering.h"
#include "Components/SkeletalMeshComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/MemberReference.h"
#include "GameFeatureAction.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "K2Node_AnimNodeReference.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "Modules/ModuleManager.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "PoseSearch/PoseSearchIndex.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "SurvivalRpg/Camera/RpgCameraMode.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UnrealType.h"

namespace RpgGaspPilotAssetTests
{
	constexpr TCHAR PilotAnimBlueprintObject[] =
		TEXT("/Game/SurvivalRpg/Characters/Mannequins/Anims/GASP/ABP_RpgCharacter_GASP.ABP_RpgCharacter_GASP");
	constexpr TCHAR BaseCharacterBlueprintObject[] =
		TEXT("/Game/SurvivalRpg/Core/Character/BP_Rpg_Character.BP_Rpg_Character");
	constexpr TCHAR PilotCharacterBlueprintObject[] =
		TEXT("/Game/SurvivalRpg/Core/Character/GASP/BP_Rpg_Character_GASP.BP_Rpg_Character_GASP");
	constexpr TCHAR BasePawnDataObject[] =
		TEXT("/Game/SurvivalRpg/Core/Character/DA_PawnData.DA_PawnData");
	constexpr TCHAR PilotPawnDataObject[] =
		TEXT("/Game/SurvivalRpg/Core/Character/GASP/DA_PawnData_GASP.DA_PawnData_GASP");
	constexpr TCHAR BaseExperienceObject[] =
		TEXT("/Game/SurvivalRpg/System/Experiences/RpgPrototypeExperience.RpgPrototypeExperience");
	constexpr TCHAR PilotExperienceObject[] =
		TEXT("/Game/SurvivalRpg/System/Experiences/RpgGaspPilotExperience.RpgGaspPilotExperience");
	constexpr TCHAR TargetSkeletonObject[] =
		TEXT("/Game/SurvivalRpg/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin");
	constexpr TCHAR TurnInPlaceDatabaseObject[] =
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_TurnInPlace.PSD_Rpg_Stand_TurnInPlace");

	constexpr TCHAR PilotAnimBlueprintPackage[] =
		TEXT("/Game/SurvivalRpg/Characters/Mannequins/Anims/GASP/ABP_RpgCharacter_GASP");
	constexpr TCHAR PilotCharacterBlueprintPackage[] =
		TEXT("/Game/SurvivalRpg/Core/Character/GASP/BP_Rpg_Character_GASP");
	constexpr TCHAR PilotPawnDataPackage[] =
		TEXT("/Game/SurvivalRpg/Core/Character/GASP/DA_PawnData_GASP");
	constexpr TCHAR PilotExperiencePackage[] =
		TEXT("/Game/SurvivalRpg/System/Experiences/RpgGaspPilotExperience");

	template <typename TObjectType>
	TObjectType* LoadRequiredAsset(
		FAutomationTestBase& Test,
		const TCHAR* ObjectPath,
		const TCHAR* Description)
	{
		TObjectType* Asset = LoadObject<TObjectType>(nullptr, ObjectPath);
		Test.TestNotNull(Description, Asset);
		return Asset;
	}

	UEdGraph* FindAnimGraph(UAnimBlueprint* AnimBlueprint)
	{
		if (!AnimBlueprint)
		{
			return nullptr;
		}

		TArray<UEdGraph*> Graphs;
		AnimBlueprint->GetAllGraphs(Graphs);
		for (UEdGraph* Graph : Graphs)
		{
			if (Graph && Graph->GetFName() == TEXT("AnimGraph"))
			{
				return Graph;
			}
		}

		return nullptr;
	}

	template <typename TNodeType>
	TNodeType* FindUniqueNode(
		FAutomationTestBase& Test,
		const UEdGraph* Graph,
		const TCHAR* Description)
	{
		TArray<TNodeType*> Matches;
		if (Graph)
		{
			for (UEdGraphNode* GraphNode : Graph->Nodes)
			{
				if (TNodeType* Match = Cast<TNodeType>(GraphNode))
				{
					Matches.Add(Match);
				}
			}
		}

		Test.TestEqual(
			*FString::Printf(TEXT("%s occurs exactly once"), Description),
			Matches.Num(),
			1);
		return Matches.Num() == 1 ? Matches[0] : nullptr;
	}

	UEdGraphNode* FindUniqueNodeByClassName(
		FAutomationTestBase& Test,
		const UEdGraph* Graph,
		FName ClassName,
		const TCHAR* Description)
	{
		TArray<UEdGraphNode*> Matches;
		if (Graph)
		{
			for (UEdGraphNode* GraphNode : Graph->Nodes)
			{
				if (GraphNode && GraphNode->GetClass()->GetFName() == ClassName)
				{
					Matches.Add(GraphNode);
				}
			}
		}

		Test.TestEqual(
			*FString::Printf(TEXT("%s occurs exactly once"), Description),
			Matches.Num(),
			1);
		return Matches.Num() == 1 ? Matches[0] : nullptr;
	}

	UK2Node_VariableGet* FindUniqueVariableGetter(
		FAutomationTestBase& Test,
		const UEdGraph* Graph,
		FName VariableName)
	{
		TArray<UK2Node_VariableGet*> Matches;
		if (Graph)
		{
			for (UEdGraphNode* GraphNode : Graph->Nodes)
			{
				UK2Node_VariableGet* VariableGetter = Cast<UK2Node_VariableGet>(GraphNode);
				if (VariableGetter && VariableGetter->VariableReference.GetMemberName() == VariableName)
				{
					Matches.Add(VariableGetter);
				}
			}
		}

		Test.TestEqual(
			*FString::Printf(TEXT("%s getter occurs exactly once"), *VariableName.ToString()),
			Matches.Num(),
			1);
		return Matches.Num() == 1 ? Matches[0] : nullptr;
	}

	bool TestExclusiveLink(
		FAutomationTestBase& Test,
		const TCHAR* Description,
		const UEdGraphNode* SourceNode,
		FName SourcePinName,
		const UEdGraphNode* TargetNode,
		FName TargetPinName)
	{
		if (!SourceNode || !TargetNode)
		{
			Test.AddError(FString::Printf(TEXT("%s cannot be checked because a graph node is missing"), Description));
			return false;
		}

		const UEdGraphPin* SourcePin = SourceNode->FindPin(SourcePinName, EGPD_Output);
		const UEdGraphPin* TargetPin = TargetNode->FindPin(TargetPinName, EGPD_Input);
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("%s source pin %s exists"), Description, *SourcePinName.ToString()),
				SourcePin) ||
			!Test.TestNotNull(
				*FString::Printf(TEXT("%s target pin %s exists"), Description, *TargetPinName.ToString()),
				TargetPin))
		{
			return false;
		}

		const bool bLinkedExclusively =
			SourcePin->LinkedTo.Num() == 1 &&
			TargetPin->LinkedTo.Num() == 1 &&
			SourcePin->LinkedTo[0] == TargetPin &&
			TargetPin->LinkedTo[0] == SourcePin;
		Test.TestTrue(Description, bLinkedExclusively);
		return bLinkedExclusively;
	}

	struct FExpectedInputPin
	{
		const UEdGraphNode* Node = nullptr;
		FName PinName;
	};

	bool TestExactOutputLinks(
		FAutomationTestBase& Test,
		const TCHAR* Description,
		const UEdGraphNode* SourceNode,
		FName SourcePinName,
		const TArray<FExpectedInputPin>& ExpectedTargets)
	{
		if (!SourceNode)
		{
			Test.AddError(FString::Printf(TEXT("%s cannot be checked because the source node is missing"), Description));
			return false;
		}

		const UEdGraphPin* SourcePin = SourceNode->FindPin(SourcePinName, EGPD_Output);
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("%s source pin %s exists"), Description, *SourcePinName.ToString()),
				SourcePin))
		{
			return false;
		}

		bool bMatches = SourcePin->LinkedTo.Num() == ExpectedTargets.Num();
		for (const FExpectedInputPin& ExpectedTarget : ExpectedTargets)
		{
			if (!ExpectedTarget.Node)
			{
				Test.AddError(FString::Printf(TEXT("%s has a missing expected target node"), Description));
				bMatches = false;
				continue;
			}

			const UEdGraphPin* TargetPin =
				ExpectedTarget.Node->FindPin(ExpectedTarget.PinName, EGPD_Input);
			if (!Test.TestNotNull(
					*FString::Printf(
						TEXT("%s target pin %s exists"),
						Description,
						*ExpectedTarget.PinName.ToString()),
					TargetPin))
			{
				bMatches = false;
				continue;
			}

			bMatches = bMatches &&
				SourcePin->LinkedTo.Contains(TargetPin) &&
				TargetPin->LinkedTo.Num() == 1 &&
				TargetPin->LinkedTo[0] == SourcePin;
		}

		Test.TestTrue(Description, bMatches);
		return bMatches;
	}

	template <typename TRuntimeNode>
	const TRuntimeNode* ReadRuntimeNode(const UEdGraphNode* EditorNode)
	{
		const FStructProperty* NodeProperty =
			EditorNode ? FindFProperty<FStructProperty>(EditorNode->GetClass(), TEXT("Node")) : nullptr;
		if (!NodeProperty || NodeProperty->Struct != TRuntimeNode::StaticStruct())
		{
			return nullptr;
		}

		return NodeProperty->ContainerPtrToValuePtr<TRuntimeNode>(EditorNode);
	}

	bool ReadBoolProperty(const UObject* Object, FName PropertyName, bool& OutValue)
	{
		const FBoolProperty* Property =
			Object ? FindFProperty<FBoolProperty>(Object->GetClass(), PropertyName) : nullptr;
		if (!Property)
		{
			return false;
		}

		OutValue = Property->GetPropertyValue_InContainer(Object);
		return true;
	}

	bool ReadMemberReferenceProperty(
		const UObject* Object,
		FName PropertyName,
		const FMemberReference*& OutValue)
	{
		const FStructProperty* Property =
			Object ? FindFProperty<FStructProperty>(Object->GetClass(), PropertyName) : nullptr;
		if (!Property || Property->Struct != FMemberReference::StaticStruct())
		{
			OutValue = nullptr;
			return false;
		}

		OutValue = Property->ContainerPtrToValuePtr<FMemberReference>(Object);
		return OutValue != nullptr;
	}

	bool ReadPropertyText(
		const UObject* Object,
		FName PropertyName,
		FString& OutValue)
	{
		const FProperty* Property =
			Object ? FindFProperty<FProperty>(Object->GetClass(), PropertyName) : nullptr;
		if (!Property)
		{
			return false;
		}

		const void* ValueAddress = Property->ContainerPtrToValuePtr<void>(Object);
		OutValue.Reset();
		Property->ExportTextItem_Direct(OutValue, ValueAddress, nullptr, nullptr, PPF_None);
		return true;
	}

	bool ReadStructFloatProperty(
		const UObject* Object,
		FName StructPropertyName,
		FName ValuePropertyName,
		float& OutValue)
	{
		const FStructProperty* StructProperty =
			Object ? FindFProperty<FStructProperty>(Object->GetClass(), StructPropertyName) : nullptr;
		const FFloatProperty* ValueProperty =
			StructProperty ? FindFProperty<FFloatProperty>(StructProperty->Struct, ValuePropertyName) : nullptr;
		if (!StructProperty || !ValueProperty)
		{
			return false;
		}

		const void* StructAddress = StructProperty->ContainerPtrToValuePtr<void>(Object);
		OutValue = ValueProperty->GetPropertyValue_InContainer(StructAddress);
		return true;
	}

	bool ReadStructBoolProperty(
		const UObject* Object,
		FName StructPropertyName,
		FName ValuePropertyName,
		bool& OutValue)
	{
		const FStructProperty* StructProperty =
			Object ? FindFProperty<FStructProperty>(Object->GetClass(), StructPropertyName) : nullptr;
		const FBoolProperty* ValueProperty =
			StructProperty ? FindFProperty<FBoolProperty>(StructProperty->Struct, ValuePropertyName) : nullptr;
		if (!StructProperty || !ValueProperty)
		{
			return false;
		}

		const void* StructAddress = StructProperty->ContainerPtrToValuePtr<void>(Object);
		OutValue = ValueProperty->GetPropertyValue_InContainer(StructAddress);
		return true;
	}

	bool ReadStructPropertyText(
		const UObject* Object,
		FName StructPropertyName,
		FName ValuePropertyName,
		FString& OutValue)
	{
		const FStructProperty* StructProperty =
			Object ? FindFProperty<FStructProperty>(Object->GetClass(), StructPropertyName) : nullptr;
		const FProperty* ValueProperty =
			StructProperty ? FindFProperty<FProperty>(StructProperty->Struct, ValuePropertyName) : nullptr;
		if (!StructProperty || !ValueProperty)
		{
			return false;
		}

		const void* StructAddress = StructProperty->ContainerPtrToValuePtr<void>(Object);
		const void* ValueAddress = ValueProperty->ContainerPtrToValuePtr<void>(StructAddress);
		OutValue.Reset();
		ValueProperty->ExportTextItem_Direct(OutValue, ValueAddress, nullptr, nullptr, PPF_None);
		return true;
	}

	bool ReadStructObjectProperty(
		const UObject* Object,
		FName StructPropertyName,
		FName ValuePropertyName,
		UObject*& OutValue)
	{
		const FStructProperty* StructProperty =
			Object ? FindFProperty<FStructProperty>(Object->GetClass(), StructPropertyName) : nullptr;
		const FObjectPropertyBase* ValueProperty =
			StructProperty ? FindFProperty<FObjectPropertyBase>(StructProperty->Struct, ValuePropertyName) : nullptr;
		if (!StructProperty || !ValueProperty)
		{
			return false;
		}

		const void* StructAddress = StructProperty->ContainerPtrToValuePtr<void>(Object);
		OutValue = ValueProperty->GetObjectPropertyValue_InContainer(StructAddress);
		return true;
	}

	bool ReadObjectArrayProperty(
		UObject* Object,
		FName PropertyName,
		TArray<FString>& OutObjectPaths)
	{
		OutObjectPaths.Reset();
		const FArrayProperty* ArrayProperty =
			Object ? FindFProperty<FArrayProperty>(Object->GetClass(), PropertyName) : nullptr;
		const FObjectPropertyBase* ObjectProperty =
			ArrayProperty ? CastField<FObjectPropertyBase>(ArrayProperty->Inner) : nullptr;
		if (!ArrayProperty || !ObjectProperty)
		{
			return false;
		}

		void* ArrayAddress = ArrayProperty->ContainerPtrToValuePtr<void>(Object);
		FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayAddress);
		OutObjectPaths.Reserve(ArrayHelper.Num());
		for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
		{
			const UObject* Value = ObjectProperty->GetObjectPropertyValue(ArrayHelper.GetRawPtr(Index));
			OutObjectPaths.Add(GetPathNameSafe(Value));
		}

		return true;
	}

	bool ReadObjectProperty(
		const UObject* Object,
		FName PropertyName,
		FString& OutObjectPath)
	{
		const FObjectPropertyBase* ObjectProperty =
			Object ? FindFProperty<FObjectPropertyBase>(Object->GetClass(), PropertyName) : nullptr;
		if (!ObjectProperty)
		{
			return false;
		}

		OutObjectPath = GetPathNameSafe(ObjectProperty->GetObjectPropertyValue_InContainer(Object));
		return true;
	}

	bool ReadGameplayTagPropertyMappings(
		const UObject* Object,
		TMap<FName, FString>& OutMappings)
	{
		OutMappings.Reset();
		const FStructProperty* MapProperty =
			Object ? FindFProperty<FStructProperty>(Object->GetClass(), TEXT("GameplayTagPropertyMap")) : nullptr;
		const FArrayProperty* MappingsProperty =
			MapProperty ? FindFProperty<FArrayProperty>(MapProperty->Struct, TEXT("PropertyMappings")) : nullptr;
		const FStructProperty* MappingProperty =
			MappingsProperty ? CastField<FStructProperty>(MappingsProperty->Inner) : nullptr;
		const FStructProperty* TagProperty =
			MappingProperty ? FindFProperty<FStructProperty>(MappingProperty->Struct, TEXT("TagToMap")) : nullptr;
		const FNameProperty* PropertyNameProperty =
			MappingProperty ? FindFProperty<FNameProperty>(MappingProperty->Struct, TEXT("PropertyName")) : nullptr;
		if (!MapProperty ||
			!MappingsProperty ||
			!MappingProperty ||
			!TagProperty ||
			TagProperty->Struct != FGameplayTag::StaticStruct() ||
			!PropertyNameProperty)
		{
			return false;
		}

		const void* MapAddress = MapProperty->ContainerPtrToValuePtr<void>(Object);
		const void* MappingsAddress = MappingsProperty->ContainerPtrToValuePtr<void>(MapAddress);
		FScriptArrayHelper MappingsHelper(
			MappingsProperty,
			const_cast<void*>(MappingsAddress));
		for (int32 Index = 0; Index < MappingsHelper.Num(); ++Index)
		{
			const void* MappingAddress = MappingsHelper.GetRawPtr(Index);
			const FGameplayTag* Tag = TagProperty->ContainerPtrToValuePtr<FGameplayTag>(MappingAddress);
			const FName PropertyName =
				PropertyNameProperty->GetPropertyValue_InContainer(MappingAddress);
			if (!Tag || PropertyName.IsNone() || OutMappings.Contains(PropertyName))
			{
				return false;
			}

			OutMappings.Add(PropertyName, Tag->ToString());
		}

		return true;
	}

	bool AreActionPropertiesEquivalent(
		const UGameFeatureAction* BaseAction,
		const UGameFeatureAction* PilotAction,
		FString& OutMismatch)
	{
		if (!BaseAction || !PilotAction)
		{
			if (BaseAction == PilotAction)
			{
				return true;
			}

			OutMismatch = TEXT("only one action is null");
			return false;
		}

		if (BaseAction->GetClass() != PilotAction->GetClass())
		{
			OutMismatch = FString::Printf(
				TEXT("class differs: %s != %s"),
				*GetPathNameSafe(BaseAction->GetClass()),
				*GetPathNameSafe(PilotAction->GetClass()));
			return false;
		}

		constexpr EPropertyFlags IgnoredFlags =
			CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient;
		constexpr uint32 ComparisonFlags = PPF_DeepComparison | PPF_DeepCompareInstances;
		for (TFieldIterator<FProperty> PropertyIt(
				BaseAction->GetClass(),
				EFieldIteratorFlags::IncludeSuper);
			PropertyIt;
			++PropertyIt)
		{
			const FProperty* Property = *PropertyIt;
			if (Property->HasAnyPropertyFlags(IgnoredFlags))
			{
				continue;
			}

			if (!Property->Identical_InContainer(
					BaseAction,
					PilotAction,
					0,
					ComparisonFlags))
			{
				OutMismatch = FString::Printf(TEXT("property %s differs"), *Property->GetName());
				return false;
			}
		}

		return true;
	}

	bool IsForbiddenPilotDependency(const FString& PackageName)
	{
		const FString LowerPackageName = PackageName.ToLower();
		if (LowerPackageName.StartsWith(TEXT("/game/")) &&
			!LowerPackageName.StartsWith(TEXT("/game/survivalrpg/")))
		{
			return true;
		}

		static const TCHAR* const ForbiddenMarkers[] = {
			TEXT("sandboxcharacter"),
			TEXT("bpi_sandbox"),
			TEXT("experimentalstatemachine"),
			TEXT("psd_sm_"),
			TEXT("/traversal/"),
			TEXT("/locomotor/"),
			TEXT("/mover/"),
			TEXT("/script/locomotor"),
			TEXT("/script/mover"),
			TEXT("/script/networkprediction"),
			TEXT("/script/metasound"),
			TEXT("/script/smartobjects"),
			TEXT("foley"),
		};

		for (const TCHAR* Marker : ForbiddenMarkers)
		{
			if (LowerPackageName.Contains(Marker))
			{
				return true;
			}
		}

		return false;
	}

	bool ShouldTraversePilotDependency(const FString& PackageName)
	{
		return
			PackageName.StartsWith(TEXT("/RpgGaspLocomotion/")) ||
			PackageName.StartsWith(TEXT("/Game/SurvivalRpg/Characters/Mannequins/Anims/GASP/")) ||
			PackageName.StartsWith(TEXT("/Game/SurvivalRpg/Core/Character/GASP/")) ||
			PackageName == PilotExperiencePackage;
	}

	void GatherPilotDependencyClosure(
		IAssetRegistry& AssetRegistry,
		TConstArrayView<FName> RootPackages,
		TSet<FName>& OutPackages)
	{
		TArray<FName> PendingPackages;
		PendingPackages.Append(RootPackages.GetData(), RootPackages.Num());
		TSet<FName> TraversedPackages;
		for (int32 PendingIndex = 0; PendingIndex < PendingPackages.Num(); ++PendingIndex)
		{
			const FName PackageName = PendingPackages[PendingIndex];
			if (TraversedPackages.Contains(PackageName))
			{
				continue;
			}

			TraversedPackages.Add(PackageName);
			OutPackages.Add(PackageName);

			TArray<FName> Dependencies;
			AssetRegistry.GetDependencies(
				PackageName,
				Dependencies,
				UE::AssetRegistry::EDependencyCategory::Package);
			for (const FName Dependency : Dependencies)
			{
				OutPackages.Add(Dependency);
				if (ShouldTraversePilotDependency(Dependency.ToString()))
				{
					PendingPackages.Add(Dependency);
				}
			}
		}
	}

	void TestPerSampleBlendStackContract(
		FAutomationTestBase& Test,
		UAnimGraphNode_MotionMatching* MotionMatchingNode)
	{
		if (!MotionMatchingNode)
		{
			return;
		}

		const TArray<UEdGraph*> SubGraphs = MotionMatchingNode->GetSubGraphs();
		Test.TestEqual(
			TEXT("Motion Matching owns exactly one nested graph"),
			SubGraphs.Num(),
			1);

		int32 BlendStackGraphCount = 0;
		UAnimationBlendStackGraph* BlendStackGraph = nullptr;
		for (UEdGraph* SubGraph : SubGraphs)
		{
			if (UAnimationBlendStackGraph* Candidate = Cast<UAnimationBlendStackGraph>(SubGraph))
			{
				++BlendStackGraphCount;
				BlendStackGraph = Candidate;
			}
		}
		Test.TestEqual(
			TEXT("Motion Matching owns exactly one AnimationBlendStackGraph"),
			BlendStackGraphCount,
			1);
		if (!Test.TestNotNull(TEXT("The Motion Matching BlendStack graph loads"), BlendStackGraph))
		{
			return;
		}

		Test.TestEqual(
			TEXT("The per-sample BlendStack graph contains exactly nine nodes"),
			BlendStackGraph->Nodes.Num(),
			9);

		UAnimGraphNode_BlendStackInput* InputNode =
			FindUniqueNode<UAnimGraphNode_BlendStackInput>(Test, BlendStackGraph, TEXT("BlendStack Input"));
		UAnimGraphNode_LocalToComponentSpace* LocalToComponentNode =
			FindUniqueNode<UAnimGraphNode_LocalToComponentSpace>(Test, BlendStackGraph, TEXT("Local To Component"));
		UAnimGraphNode_OrientationWarping* OrientationWarpingNode =
			FindUniqueNode<UAnimGraphNode_OrientationWarping>(Test, BlendStackGraph, TEXT("Per-sample Orientation Warping"));
		UAnimGraphNode_ResetRoot* ResetRootNode =
			FindUniqueNode<UAnimGraphNode_ResetRoot>(Test, BlendStackGraph, TEXT("Reset Root"));
		UAnimGraphNode_Steering* SteeringNode =
			FindUniqueNode<UAnimGraphNode_Steering>(Test, BlendStackGraph, TEXT("Steering"));
		UAnimGraphNode_ComponentToLocalSpace* ComponentToLocalNode =
			FindUniqueNode<UAnimGraphNode_ComponentToLocalSpace>(Test, BlendStackGraph, TEXT("Component To Local"));
		UK2Node_AnimNodeReference* InputReferenceNode =
			FindUniqueNode<UK2Node_AnimNodeReference>(Test, BlendStackGraph, TEXT("BlendStack Input Reference"));
		UK2Node_CallFunction* BlendStackInputsNode =
			FindUniqueNode<UK2Node_CallFunction>(Test, BlendStackGraph, TEXT("GetGaspBlendStackInputs call"));
		UAnimGraphNode_BlendStackResult* ResultNode =
			FindUniqueNode<UAnimGraphNode_BlendStackResult>(Test, BlendStackGraph, TEXT("BlendStack Result"));

		const auto TestExactNodeClass = [&Test](
			const TCHAR* Description,
			const UEdGraphNode* Node,
			UClass* ExpectedClass)
		{
			if (Node)
			{
				Test.TestEqual(Description, Node->GetClass(), ExpectedClass);
			}
		};
		TestExactNodeClass(TEXT("The input uses the exact BlendStack Input class"), InputNode, UAnimGraphNode_BlendStackInput::StaticClass());
		TestExactNodeClass(TEXT("The L2C node uses the exact conversion class"), LocalToComponentNode, UAnimGraphNode_LocalToComponentSpace::StaticClass());
		TestExactNodeClass(TEXT("The OW node uses the exact Orientation Warping class"), OrientationWarpingNode, UAnimGraphNode_OrientationWarping::StaticClass());
		TestExactNodeClass(TEXT("The reset node uses the exact Reset Root class"), ResetRootNode, UAnimGraphNode_ResetRoot::StaticClass());
		TestExactNodeClass(TEXT("The steering node uses the exact Steering class"), SteeringNode, UAnimGraphNode_Steering::StaticClass());
		TestExactNodeClass(TEXT("The C2L node uses the exact conversion class"), ComponentToLocalNode, UAnimGraphNode_ComponentToLocalSpace::StaticClass());
		TestExactNodeClass(TEXT("The reference uses the exact Anim Node Reference class"), InputReferenceNode, UK2Node_AnimNodeReference::StaticClass());
		TestExactNodeClass(TEXT("The helper uses the exact Call Function class"), BlendStackInputsNode, UK2Node_CallFunction::StaticClass());
		TestExactNodeClass(TEXT("The result uses the exact BlendStack Result class"), ResultNode, UAnimGraphNode_BlendStackResult::StaticClass());

		const FName ExpectedInputTag(TEXT("RpgGaspPilotBlendStackInput"));
		if (InputNode)
		{
			Test.TestEqual(
				TEXT("The BlendStack input has the stable pilot tag"),
				InputNode->GetTag(),
				ExpectedInputTag);
		}
		if (InputReferenceNode)
		{
			Test.TestEqual(
				TEXT("The Anim Node Reference targets the pilot input tag"),
				InputReferenceNode->GetTag(),
				ExpectedInputTag);
		}

		UFunction* HelperFunction = URpgAnimInstance::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(URpgAnimInstance, GetGaspBlendStackInputs));
		if (Test.TestNotNull(TEXT("GetGaspBlendStackInputs is reflected"), HelperFunction))
		{
			Test.TestTrue(
				TEXT("GetGaspBlendStackInputs is BlueprintCallable, pure, and const"),
				HelperFunction->HasAllFunctionFlags(
					FUNC_BlueprintCallable | FUNC_BlueprintPure | FUNC_Const));
			Test.TestTrue(
				TEXT("GetGaspBlendStackInputs is explicitly BlueprintThreadSafe"),
				HelperFunction->HasMetaData(TEXT("BlueprintThreadSafe")));
		}
		if (BlendStackInputsNode)
		{
			Test.TestTrue(
				TEXT("The nested helper call is a pure K2 node"),
				BlendStackInputsNode->IsNodePure());
			Test.TestEqual(
				TEXT("The nested helper call has the expected function name"),
				BlendStackInputsNode->GetFunctionName(),
				GET_FUNCTION_NAME_CHECKED(URpgAnimInstance, GetGaspBlendStackInputs));
			Test.TestEqual(
				TEXT("The nested helper call resolves to URpgAnimInstance"),
				BlendStackInputsNode->GetTargetFunction(),
				HelperFunction);
		}

		const FAnimNode_OrientationWarping* OrientationWarping =
			ReadRuntimeNode<FAnimNode_OrientationWarping>(OrientationWarpingNode);
		if (Test.TestNotNull(TEXT("The per-sample Orientation Warping runtime node is readable"), OrientationWarping))
		{
			Test.TestEqual(TEXT("OW evaluates from graph inputs"), OrientationWarping->Mode, EWarpingEvaluationMode::Graph);
			Test.TestEqual(TEXT("OW samples 0.8 seconds ahead"), OrientationWarping->TargetTime, 0.8f);
			Test.TestEqual(TEXT("OW ignores root motion below 10 cm/s"), OrientationWarping->MinRootMotionSpeedThreshold, 10.0f);
			Test.TestEqual(TEXT("OW uses the audited 135 degree inversion threshold"), OrientationWarping->LocomotionAngleDeltaThreshold, 135.0f);

			static const FName ExpectedSpineBones[] = {
				TEXT("spine_01"),
				TEXT("spine_02"),
				TEXT("spine_03"),
				TEXT("spine_04"),
				TEXT("spine_05"),
			};
			constexpr int32 ExpectedSpineBoneCount = static_cast<int32>(UE_ARRAY_COUNT(ExpectedSpineBones));
			Test.TestEqual(TEXT("OW distributes rotation over exactly five spine bones"), OrientationWarping->SpineBones.Num(), ExpectedSpineBoneCount);
			for (int32 Index = 0; Index < ExpectedSpineBoneCount && Index < OrientationWarping->SpineBones.Num(); ++Index)
			{
				Test.TestEqual(
					*FString::Printf(TEXT("OW spine bone %d is stable"), Index),
					OrientationWarping->SpineBones[Index].BoneName,
					ExpectedSpineBones[Index]);
			}
			Test.TestEqual(TEXT("OW uses the mannequin IK foot root"), OrientationWarping->IKFootRootBone.BoneName, FName(TEXT("ik_foot_root")));
			Test.TestEqual(TEXT("OW has exactly two IK foot bones"), OrientationWarping->IKFootBones.Num(), 2);
			if (OrientationWarping->IKFootBones.Num() == 2)
			{
				Test.TestEqual(TEXT("OW left IK foot is stable"), OrientationWarping->IKFootBones[0].BoneName, FName(TEXT("ik_foot_l")));
				Test.TestEqual(TEXT("OW right IK foot is stable"), OrientationWarping->IKFootBones[1].BoneName, FName(TEXT("ik_foot_r")));
			}

			Test.TestEqual(TEXT("OW rotates around Z"), OrientationWarping->RotationAxis.GetValue(), EAxis::Z);
			Test.TestEqual(TEXT("OW distributes half of the orientation"), OrientationWarping->DistributedBoneOrientationAlpha, 0.5f);
			Test.TestEqual(TEXT("OW interpolation speed is audited"), OrientationWarping->RotationInterpSpeed, 8.0f);
			Test.TestEqual(TEXT("OW counter-compensation speed is audited"), OrientationWarping->CounterCompensateInterpSpeed, 45.0f);
			Test.TestEqual(TEXT("OW permits full correction"), OrientationWarping->MaxCorrectionDegrees, 180.0f);
			Test.TestEqual(TEXT("OW preserves pivot root-motion deltas"), OrientationWarping->MaxRootMotionDeltaToCompensateDegrees, 45.0f);
			Test.TestTrue(TEXT("OW counter-compensates animated root motion"), OrientationWarping->bCounterCompenstateInterpolationByRootMotion);
			Test.TestFalse(TEXT("OW does not scale by global blend weight"), OrientationWarping->bScaleByGlobalBlendWeight);
			Test.TestFalse(TEXT("OW does not use manual root-motion velocity"), OrientationWarping->bUseManualRootMotionVelocity);
			Test.TestEqual(TEXT("OW evaluates in OffsetRoot-compatible root-bone space"), OrientationWarping->WarpingSpace, EOrientationWarpingSpace::RootBoneTransform);
			Test.TestEqual(TEXT("OW uses a float alpha"), OrientationWarping->AlphaInputType, EAnimAlphaInputType::Float);
			Test.TestEqual(TEXT("OW has no LOD cutoff"), OrientationWarping->LODThreshold, INDEX_NONE);
		}

		const FAnimNode_ResetRoot* ResetRoot = ReadRuntimeNode<FAnimNode_ResetRoot>(ResetRootNode);
		if (Test.TestNotNull(TEXT("The Reset Root runtime node is readable"), ResetRoot))
		{
			Test.TestEqual(TEXT("Reset Root uses a float moving gate"), ResetRoot->AlphaInputType, EAnimAlphaInputType::Float);
			Test.TestEqual(TEXT("Reset Root default alpha remains one"), ResetRoot->Alpha, 1.0f);
			Test.TestEqual(TEXT("Reset Root has no LOD cutoff"), ResetRoot->LODThreshold, INDEX_NONE);
		}

		const FAnimNode_Steering* Steering = ReadRuntimeNode<FAnimNode_Steering>(SteeringNode);
		if (Test.TestNotNull(TEXT("The Steering runtime node is readable"), Steering))
		{
			Test.TestEqual(TEXT("Steering matches the GASP 0.2-second correction target"), Steering->ProceduralTargetTime, 0.2f);
			Test.TestEqual(TEXT("Steering samples animation two seconds ahead"), Steering->AnimatedTargetTime, 2.0f);
			Test.TestEqual(TEXT("Steering root-motion threshold is audited"), Steering->RootMotionThreshold, 1.0f);
			Test.TestEqual(TEXT("Steering disables below 10 cm/s"), Steering->DisableSteeringBelowSpeed, 10.0f);
			Test.TestEqual(TEXT("Steering additive correction remains enabled"), Steering->DisableAdditiveBelowSpeed, -1.0f);
			Test.TestEqual(TEXT("Steering minimum root-motion scale is audited"), Steering->MinScaleRatio, 0.5f);
			Test.TestEqual(TEXT("Steering maximum root-motion scale is audited"), Steering->MaxScaleRatio, 1.5f);
			Test.TestEqual(TEXT("Steering uses a bool gate"), Steering->AlphaInputType, EAnimAlphaInputType::Bool);
			Test.TestTrue(TEXT("Steering's unbound bool default remains enabled"), Steering->bAlphaBoolEnabled);
			Test.TestEqual(TEXT("Steering bool blend-in is audited"), Steering->AlphaBoolBlend.BlendInTime, 0.1f);
			Test.TestEqual(TEXT("Steering bool blend-out is audited"), Steering->AlphaBoolBlend.BlendOutTime, 0.1f);
			Test.TestEqual(TEXT("Steering bool blend is linear"), Steering->AlphaBoolBlend.BlendOption, EAlphaBlendOption::Linear);
			Test.TestEqual(TEXT("Steering has no LOD cutoff"), Steering->LODThreshold, INDEX_NONE);
		}

		TestExactOutputLinks(Test, TEXT("BlendStack Input feeds L2C"), InputNode, TEXT("Pose"), {{LocalToComponentNode, TEXT("LocalPose")}});
		TestExactOutputLinks(Test, TEXT("L2C feeds per-sample OW"), LocalToComponentNode, TEXT("ComponentPose"), {{OrientationWarpingNode, TEXT("ComponentPose")}});
		TestExactOutputLinks(Test, TEXT("Per-sample OW feeds Reset Root"), OrientationWarpingNode, TEXT("Pose"), {{ResetRootNode, TEXT("ComponentPose")}});
		TestExactOutputLinks(Test, TEXT("Reset Root feeds Steering"), ResetRootNode, TEXT("Pose"), {{SteeringNode, TEXT("ComponentPose")}});
		TestExactOutputLinks(Test, TEXT("Steering feeds C2L"), SteeringNode, TEXT("Pose"), {{ComponentToLocalNode, TEXT("ComponentPose")}});
		TestExactOutputLinks(Test, TEXT("C2L feeds BlendStack Result"), ComponentToLocalNode, TEXT("Pose"), {{ResultNode, TEXT("Result")}});
		TestExactOutputLinks(Test, TEXT("The tagged reference feeds the helper"), InputReferenceNode, TEXT("Value"), {{BlendStackInputsNode, TEXT("Node")}});
		TestExactOutputLinks(
			Test,
			TEXT("The helper asset feeds both procedural nodes"),
			BlendStackInputsNode,
			TEXT("CurrentAnimAsset"),
			{{OrientationWarpingNode, TEXT("CurrentAnimAsset")}, {SteeringNode, TEXT("CurrentAnimAsset")}});
		TestExactOutputLinks(
			Test,
			TEXT("The helper asset time feeds both procedural nodes"),
			BlendStackInputsNode,
			TEXT("CurrentAnimAssetTime"),
			{{OrientationWarpingNode, TEXT("CurrentAnimAssetTime")}, {SteeringNode, TEXT("CurrentAnimAssetTime")}});
		TestExactOutputLinks(Test, TEXT("Moving alpha gates Reset Root"), BlendStackInputsNode, TEXT("MovingAlpha"), {{ResetRootNode, TEXT("Alpha")}});
		TestExactOutputLinks(Test, TEXT("Curve-gated alpha drives OW"), BlendStackInputsNode, TEXT("OrientationWarpingAlpha"), {{OrientationWarpingNode, TEXT("Alpha")}});
		TestExactOutputLinks(Test, TEXT("Desired facing drives Steering"), BlendStackInputsNode, TEXT("DesiredFacing"), {{SteeringNode, TEXT("TargetOrientation")}});
		TestExactOutputLinks(Test, TEXT("Last movement direction drives OW"), BlendStackInputsNode, TEXT("LocomotionDirection"), {{OrientationWarpingNode, TEXT("LocomotionDirection")}});
		TestExactOutputLinks(Test, TEXT("The active-sample gate drives Steering"), BlendStackInputsNode, TEXT("bEnableSteering"), {{SteeringNode, TEXT("bAlphaBoolEnabled")}});
	}

}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgGaspPilotAssetContractTest,
	"SurvivalRpg.Animation.Gasp.PilotAssetContract",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgGaspPilotAssetContractTest::RunTest(const FString& Parameters)
{
	using namespace RpgGaspPilotAssetTests;

	UAnimBlueprint* PilotAnimBlueprint = LoadRequiredAsset<UAnimBlueprint>(
		*this,
		PilotAnimBlueprintObject,
		TEXT("The isolated GASP AnimBlueprint loads"));
	UBlueprint* BaseCharacterBlueprint = LoadRequiredAsset<UBlueprint>(
		*this,
		BaseCharacterBlueprintObject,
		TEXT("The default character Blueprint loads"));
	UBlueprint* PilotCharacterBlueprint = LoadRequiredAsset<UBlueprint>(
		*this,
		PilotCharacterBlueprintObject,
		TEXT("The isolated GASP character Blueprint loads"));
	URpgPawnData* BasePawnData = LoadRequiredAsset<URpgPawnData>(
		*this,
		BasePawnDataObject,
		TEXT("The default PawnData loads"));
	URpgPawnData* PilotPawnData = LoadRequiredAsset<URpgPawnData>(
		*this,
		PilotPawnDataObject,
		TEXT("The isolated GASP PawnData loads"));
	UBlueprint* BaseExperienceBlueprint = LoadRequiredAsset<UBlueprint>(
		*this,
		BaseExperienceObject,
		TEXT("The prototype Experience Blueprint loads"));
	UBlueprint* PilotExperienceBlueprint = LoadRequiredAsset<UBlueprint>(
		*this,
		PilotExperienceObject,
		TEXT("The isolated GASP Experience Blueprint loads"));
	UPoseSearchDatabase* TurnInPlaceDatabase = LoadRequiredAsset<UPoseSearchDatabase>(
		*this,
		TurnInPlaceDatabaseObject,
		TEXT("The project-local turn-in-place database loads"));
	URpgExperienceDefinition* BaseExperience =
		BaseExperienceBlueprint && BaseExperienceBlueprint->GeneratedClass
			? Cast<URpgExperienceDefinition>(BaseExperienceBlueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
	URpgExperienceDefinition* PilotExperience =
		PilotExperienceBlueprint && PilotExperienceBlueprint->GeneratedClass
			? Cast<URpgExperienceDefinition>(PilotExperienceBlueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
	TestNotNull(TEXT("The prototype Experience defaults load"), BaseExperience);
	TestNotNull(TEXT("The isolated GASP Experience defaults load"), PilotExperience);

	if (!PilotAnimBlueprint ||
		!BaseCharacterBlueprint ||
		!PilotCharacterBlueprint ||
		!BasePawnData ||
		!PilotPawnData ||
		!BaseExperienceBlueprint ||
		!PilotExperienceBlueprint ||
		!TurnInPlaceDatabase ||
		!BaseExperience ||
		!PilotExperience)
	{
		return false;
	}

	TestEqual(
		TEXT("The GASP AnimBlueprint derives directly from URpgAnimInstance"),
		PilotAnimBlueprint->ParentClass.Get(),
		URpgAnimInstance::StaticClass());
	TestEqual(
		TEXT("The GASP AnimBlueprint targets the authoritative player skeleton"),
		GetPathNameSafe(PilotAnimBlueprint->TargetSkeleton),
		FString(TargetSkeletonObject));
	TestTrue(
		TEXT("The GASP AnimBlueprint allows multi-threaded animation update"),
		PilotAnimBlueprint->bUseMultiThreadedAnimationUpdate);
	TestTrue(
		TEXT("The GASP AnimBlueprint has no compile error"),
		PilotAnimBlueprint->Status != BS_Error);

	static const struct
	{
		const TCHAR* ObjectPath;
		float ExpectedLength;
	} ExpectedTurnSequences[] = {
		{TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/M_Neutral_Stand_Turn_045_L.M_Neutral_Stand_Turn_045_L"), 1.6666667f},
		{TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/M_Neutral_Stand_Turn_045_R.M_Neutral_Stand_Turn_045_R"), 1.6666667f},
		{TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/M_Neutral_Stand_Turn_090_L.M_Neutral_Stand_Turn_090_L"), 2.0f},
		{TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/M_Neutral_Stand_Turn_090_R.M_Neutral_Stand_Turn_090_R"), 2.0f},
		{TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/M_Neutral_Stand_Turn_135_L.M_Neutral_Stand_Turn_135_L"), 2.0f},
		{TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/M_Neutral_Stand_Turn_135_R.M_Neutral_Stand_Turn_135_R"), 2.0f},
		{TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/M_Neutral_Stand_Turn_180_L.M_Neutral_Stand_Turn_180_L"), 2.1666667f},
		{TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/M_Neutral_Stand_Turn_180_R.M_Neutral_Stand_Turn_180_R"), 2.1666667f},
	};
	TestEqual(
		TEXT("The exclusive turn-in-place database contains exactly eight clips"),
		TurnInPlaceDatabase->GetNumAnimationAssets(),
		static_cast<int32>(UE_ARRAY_COUNT(ExpectedTurnSequences)));
	for (int32 Index = 0;
		Index < UE_ARRAY_COUNT(ExpectedTurnSequences) && Index < TurnInPlaceDatabase->GetNumAnimationAssets();
		++Index)
	{
		const FPoseSearchDatabaseAnimationAsset* DatabaseEntry =
			TurnInPlaceDatabase->GetDatabaseAnimationAsset(Index);
		if (!TestNotNull(
			*FString::Printf(TEXT("Turn-in-place database entry %d is readable"), Index),
			DatabaseEntry))
		{
			continue;
		}

		UAnimSequence* Sequence = Cast<UAnimSequence>(DatabaseEntry->GetAnimationAsset());
		if (!TestNotNull(
			*FString::Printf(TEXT("Turn-in-place database entry %d references an AnimSequence"), Index),
			Sequence))
		{
			continue;
		}

		TestEqual(
			*FString::Printf(TEXT("Turn-in-place database entry %d keeps the deterministic clip order"), Index),
			GetPathNameSafe(Sequence),
			FString(ExpectedTurnSequences[Index].ObjectPath));
		TestFalse(
			*FString::Printf(TEXT("Turn-in-place clip %d is non-looping"), Index),
			Sequence->bLoop);
		TestTrue(
			*FString::Printf(TEXT("Turn-in-place clip %d keeps its authored duration"), Index),
			FMath::IsNearlyEqual(
				static_cast<float>(Sequence->GetPlayLength()),
				ExpectedTurnSequences[Index].ExpectedLength,
				0.001f));
		TestTrue(
			*FString::Printf(TEXT("Turn-in-place clip %d plays at authored speed"), Index),
			FMath::IsNearlyEqual(Sequence->RateScale, 1.0f));
		TestTrue(
			*FString::Printf(TEXT("Turn-in-place database entry %d remains enabled"), Index),
			DatabaseEntry->IsEnabled());
		TestFalse(
			*FString::Printf(TEXT("Turn-in-place database entry %d remains non-looping"), Index),
			DatabaseEntry->IsLooping());
		TestFalse(
			*FString::Printf(TEXT("Turn-in-place database entry %d keeps Epic's reselection default"), Index),
			DatabaseEntry->IsDisableReselection());
		const FFloatInterval SamplingRange = DatabaseEntry->GetSamplingRange();
		TestTrue(
			*FString::Printf(TEXT("Turn-in-place database entry %d searches only the authored start pose"), Index),
			FMath::IsNearlyZero(SamplingRange.Min) && FMath::IsNearlyEqual(SamplingRange.Max, 0.01f));
		TestTrue(
			*FString::Printf(TEXT("Turn-in-place database entry %d remains unmirrored-only"), Index),
			DatabaseEntry->GetMirrorOption() == EPoseSearchMirrorOption::UnmirroredOnly);
	}

	const UE::PoseSearch::EAsyncBuildIndexResult TurnInPlaceBuildResult =
		UE::PoseSearch::FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(
			TurnInPlaceDatabase,
			UE::PoseSearch::ERequestAsyncBuildFlag::NewRequest |
				UE::PoseSearch::ERequestAsyncBuildFlag::WaitForCompletion);
	if (TestTrue(
		TEXT("The exclusive turn-in-place search index finishes building"),
		TurnInPlaceBuildResult == UE::PoseSearch::EAsyncBuildIndexResult::Success))
	{
		const UE::PoseSearch::FSearchIndex& TurnInPlaceSearchIndex = TurnInPlaceDatabase->GetSearchIndex();
		TestEqual(
			TEXT("The exclusive turn-in-place database indexes exactly eight poses"),
			TurnInPlaceSearchIndex.GetNumPoses(),
			8);
		TestEqual(
			TEXT("The exclusive turn-in-place database creates exactly eight search-index assets"),
			TurnInPlaceSearchIndex.Assets.Num(),
			8);
		for (int32 Index = 0; Index < TurnInPlaceSearchIndex.Assets.Num(); ++Index)
		{
			TestEqual(
				*FString::Printf(TEXT("Turn-in-place search-index asset %d contains only t=0"), Index),
				TurnInPlaceSearchIndex.Assets[Index].GetNumPoses(),
				1);
		}
	}

	URpgAnimInstance* PilotAnimDefaults =
		PilotAnimBlueprint->GeneratedClass
			? Cast<URpgAnimInstance>(PilotAnimBlueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
	if (TestNotNull(TEXT("The GASP AnimBlueprint generated-class defaults load"), PilotAnimDefaults))
	{
		FDataValidationContext ValidationContext;
		TestEqual(
			TEXT("The GASP AnimBlueprint defaults pass native data validation"),
			PilotAnimDefaults->IsDataValid(ValidationContext),
			EDataValidationResult::Valid);
		TestEqual(
			TEXT("The GASP AnimBlueprint extracts root motion only from montages"),
			PilotAnimDefaults->RootMotionMode.GetValue(),
			ERootMotionMode::RootMotionFromMontagesOnly);

		bool bGeneratesTrajectory = false;
		if (TestTrue(
				TEXT("The trajectory-generation setting exists on URpgAnimInstance"),
				ReadBoolProperty(PilotAnimDefaults, TEXT("bGeneratePoseSearchTrajectory"), bGeneratesTrajectory)))
		{
			TestTrue(TEXT("The GASP AnimBlueprint generates its thread-safe trajectory snapshot"), bGeneratesTrajectory);
		}

		FString OffsetRootRotationMode;
		if (TestTrue(
			TEXT("The runtime Offset Root rotation mode is readable"),
			ReadPropertyText(PilotAnimDefaults, TEXT("OffsetRootRotationMode"), OffsetRootRotationMode)))
		{
			TestEqual(
				TEXT("The runtime Offset Root rotation mode defaults to interpolation"),
				OffsetRootRotationMode,
				FString(TEXT("Interpolate")));
		}
		bool bResetOffsetRootEveryFrame = true;
		if (TestTrue(
			TEXT("The runtime Offset Root reset pulse is readable"),
			ReadBoolProperty(
				PilotAnimDefaults,
				TEXT("bResetOffsetRootEveryFrame"),
				bResetOffsetRootEveryFrame)))
		{
			TestFalse(
				TEXT("The runtime Offset Root reset pulse is disabled by default"),
				bResetOffsetRootEveryFrame);
		}

		TArray<FString> GroundDatabasePaths;
		if (TestTrue(
				TEXT("The grounded database property is readable"),
				ReadObjectArrayProperty(
					PilotAnimDefaults,
					TEXT("GroundMotionMatchingDatabases"),
					GroundDatabasePaths)))
		{
			static const TCHAR* const ExpectedGroundDatabases[] = {
				TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Idle.PSD_Rpg_Stand_Idle"),
				TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Walk.PSD_Rpg_Stand_Walk"),
				TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run.PSD_Rpg_Stand_Run"),
				TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Sprint.PSD_Rpg_Stand_Sprint"),
			};
			TestEqual(TEXT("Exactly four grounded databases are configured"), GroundDatabasePaths.Num(), 4);
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedGroundDatabases) && Index < GroundDatabasePaths.Num(); ++Index)
			{
				TestEqual(
					*FString::Printf(TEXT("Ground database %d remains project-local"), Index),
					GroundDatabasePaths[Index],
					FString(ExpectedGroundDatabases[Index]));
			}
		}

		FString CrouchingDatabasePath;
		if (TestTrue(
			TEXT("The crouching database property is readable"),
			ReadObjectProperty(
				PilotAnimDefaults,
				TEXT("CrouchingMotionMatchingDatabase"),
				CrouchingDatabasePath)))
		{
			TestEqual(
				TEXT("Crouching uses the project-local crouch database"),
				CrouchingDatabasePath,
				FString(TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Crouch.PSD_Rpg_Crouch")));
		}

		FString TurnInPlaceDatabasePath;
		if (TestTrue(
			TEXT("The turn-in-place database property is readable"),
			ReadObjectProperty(
				PilotAnimDefaults,
				TEXT("TurnInPlaceMotionMatchingDatabase"),
				TurnInPlaceDatabasePath)))
		{
			TestEqual(
				TEXT("Turn-in-place uses the exclusive project-local database"),
				TurnInPlaceDatabasePath,
				FString(TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_TurnInPlace.PSD_Rpg_Stand_TurnInPlace")));
		}

		TMap<FName, FString> GameplayTagMappings;
		if (TestTrue(
			TEXT("The gameplay-tag property map is readable"),
			ReadGameplayTagPropertyMappings(PilotAnimDefaults, GameplayTagMappings)))
		{
			static const TPair<FName, FString> ExpectedMappings[] = {
				{TEXT("bGameplayMovementStopped"), TEXT("Gameplay.MovementStopped")},
				{TEXT("bStateBlocking"), TEXT("State.Blocking")},
				{TEXT("bStateDead"), TEXT("State.Dead")},
				{TEXT("bStateStaggered"), TEXT("State.Staggered")},
				{TEXT("bStateGuardBroken"), TEXT("State.GuardBroken")},
			};
			TestEqual(
				TEXT("Exactly five gameplay tags gate cosmetic turn-in-place"),
				GameplayTagMappings.Num(),
				static_cast<int32>(UE_ARRAY_COUNT(ExpectedMappings)));
			for (const TPair<FName, FString>& ExpectedMapping : ExpectedMappings)
			{
				const FString* ActualTag = GameplayTagMappings.Find(ExpectedMapping.Key);
				if (TestNotNull(
					*FString::Printf(TEXT("%s has a gameplay-tag mapping"), *ExpectedMapping.Key.ToString()),
					ActualTag))
				{
					TestEqual(
						*FString::Printf(TEXT("%s mirrors the intended gameplay tag"), *ExpectedMapping.Key.ToString()),
						*ActualTag,
						ExpectedMapping.Value);
				}
			}
		}

		TArray<FString> AirborneDatabasePaths;
		if (TestTrue(
				TEXT("The airborne database property is readable"),
				ReadObjectArrayProperty(
					PilotAnimDefaults,
					TEXT("AirborneMotionMatchingDatabases"),
					AirborneDatabasePaths)))
		{
			TestEqual(TEXT("Exactly one airborne database is configured"), AirborneDatabasePaths.Num(), 1);
			if (AirborneDatabasePaths.Num() == 1)
			{
				TestEqual(
					TEXT("The airborne database is the project-local jump database"),
					AirborneDatabasePaths[0],
					FString(TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Jump.PSD_Rpg_Jump")));
			}
		}
	}

	UEdGraph* AnimGraph = FindAnimGraph(PilotAnimBlueprint);
	if (TestNotNull(TEXT("The GASP AnimBlueprint has an AnimGraph"), AnimGraph))
	{
		UAnimGraphNode_MotionMatching* MotionMatchingNode =
			FindUniqueNode<UAnimGraphNode_MotionMatching>(*this, AnimGraph, TEXT("Motion Matching"));
		UEdGraphNode* OffsetRootBoneNode = FindUniqueNodeByClassName(
			*this,
			AnimGraph,
			TEXT("AnimGraphNode_OffsetRootBone"),
			TEXT("Offset Root Bone"));
		UAnimGraphNode_Slot* SlotNode =
			FindUniqueNode<UAnimGraphNode_Slot>(*this, AnimGraph, TEXT("Montage Slot"));
		UAnimGraphNode_PoseSearchHistoryCollector* PoseHistoryNode =
			FindUniqueNode<UAnimGraphNode_PoseSearchHistoryCollector>(*this, AnimGraph, TEXT("Pose History"));
		UAnimGraphNode_Root* RootNode =
			FindUniqueNode<UAnimGraphNode_Root>(*this, AnimGraph, TEXT("AnimGraph Root"));
		UK2Node_VariableGet* TrajectoryGetter =
			FindUniqueVariableGetter(*this, AnimGraph, TEXT("LocomotionTrajectory"));
		UK2Node_VariableGet* OffsetRootRotationModeGetter =
			FindUniqueVariableGetter(*this, AnimGraph, TEXT("OffsetRootRotationMode"));
		UK2Node_VariableGet* ResetOffsetRootGetter =
			FindUniqueVariableGetter(*this, AnimGraph, TEXT("bResetOffsetRootEveryFrame"));
		TestEqual(
			TEXT("The top-level pilot graph contains only the five pose nodes and three property getters"),
			AnimGraph->Nodes.Num(),
			8);
		int32 FootPlacementNodeCount = 0;
		int32 LegIkNodeCount = 0;
		int32 OrientationWarpingNodeCount = 0;
		for (const UEdGraphNode* GraphNode : AnimGraph->Nodes)
		{
			if (!GraphNode)
			{
				continue;
			}

			FootPlacementNodeCount += GraphNode->GetClass()->GetFName() == TEXT("AnimGraphNode_FootPlacement");
			LegIkNodeCount += GraphNode->GetClass()->GetFName() == TEXT("AnimGraphNode_LegIK");
			OrientationWarpingNodeCount += GraphNode->GetClass()->GetFName() == TEXT("AnimGraphNode_OrientationWarping");
		}
		TestEqual(
			TEXT("The pilot does not silently use Epic's worker-thread-unsafe Foot Placement node"),
			FootPlacementNodeCount,
			0);
		TestEqual(
			TEXT("Leg IK remains part of the thread-safe Foot Placement follow-up"),
			LegIkNodeCount,
			0);
		TestEqual(
			TEXT("The pilot does not apply incomplete top-level Orientation Warping"),
			OrientationWarpingNodeCount,
			0);

		if (MotionMatchingNode)
		{
			TestEqual(
				TEXT("Motion Matching runs the RPG database selector on update"),
				MotionMatchingNode->UpdateFunction.GetMemberName(),
				GET_FUNCTION_NAME_CHECKED(URpgAnimInstance, UpdateGaspMotionMatching));
			TestTrue(
				TEXT("The Motion Matching update binding uses the AnimInstance self context"),
				MotionMatchingNode->UpdateFunction.IsSelfContext());
			TestNotNull(
				TEXT("The Motion Matching update binding resolves on the generated AnimInstance class"),
				MotionMatchingNode->UpdateFunction.ResolveMember<UFunction>(PilotAnimBlueprint->GeneratedClass));
			const FMemberReference* MotionMatchingStateUpdatedFunction = nullptr;
			if (TestTrue(
				TEXT("The specialized post-search Motion Matching callback binding is readable"),
				ReadMemberReferenceProperty(
					MotionMatchingNode,
					TEXT("OnMotionMatchingStateUpdatedFunction"),
					MotionMatchingStateUpdatedFunction)))
			{
				TestTrue(
					TEXT("The specialized post-search Motion Matching callback remains deliberately unbound"),
					MotionMatchingStateUpdatedFunction->GetMemberName().IsNone());
				TestNull(
					TEXT("No specialized post-search Motion Matching callback resolves on the generated class"),
					MotionMatchingStateUpdatedFunction->ResolveMember<UFunction>(
						PilotAnimBlueprint->GeneratedClass));
			}
			float MotionMatchingBlendTime = 0.0f;
			if (TestTrue(
				TEXT("The Motion Matching blend time is readable"),
				ReadStructFloatProperty(
					MotionMatchingNode,
					TEXT("Node"),
					TEXT("BlendTime"),
					MotionMatchingBlendTime)))
			{
				TestEqual(
					TEXT("Motion Matching uses the responsive pilot blend time"),
					MotionMatchingBlendTime,
					0.2f);
			}

			UObject* BlendProfileObject = nullptr;
			if (TestTrue(
				TEXT("The Motion Matching blend profile is readable"),
				ReadStructObjectProperty(
					MotionMatchingNode,
					TEXT("Node"),
					TEXT("BlendProfile"),
					BlendProfileObject)))
			{
				TestNull(
					TEXT("Motion Matching uses uniform 0.2-second blending after the FastFeet TIR regression"),
					BlendProfileObject);
			}

			TestPerSampleBlendStackContract(*this, MotionMatchingNode);
		}

		if (OffsetRootBoneNode)
		{
			bool bResetEveryFrame = true;
			if (TestTrue(
				TEXT("The Offset Root Bone reset default is readable"),
				ReadStructBoolProperty(
					OffsetRootBoneNode,
					TEXT("Node"),
					TEXT("bResetEveryFrame"),
					bResetEveryFrame)))
			{
				TestFalse(TEXT("Offset Root Bone keeps reset disabled unless the runtime requests one frame"), bResetEveryFrame);
			}

			FString RotationMode;
			if (TestTrue(
				TEXT("The Offset Root Bone rotation mode is readable"),
				ReadStructPropertyText(
					OffsetRootBoneNode,
					TEXT("Node"),
					TEXT("RotationMode"),
					RotationMode)))
			{
				TestEqual(
					TEXT("Controller-facing locomotion interpolates visual root rotation"),
					RotationMode,
					FString(TEXT("Interpolate")));
			}

			float RotationHalfLife = 0.0f;
			if (TestTrue(
				TEXT("The Offset Root Bone rotation half-life is readable"),
				ReadStructFloatProperty(
					OffsetRootBoneNode,
					TEXT("Node"),
					TEXT("RotationHalfLife"),
					RotationHalfLife)))
			{
				TestEqual(
					TEXT("Visual root rotation catches controller-facing movement promptly"),
					RotationHalfLife,
					0.1f);
			}

			const UEdGraphPin* RotationModePin =
				OffsetRootBoneNode->FindPin(TEXT("RotationMode"), EGPD_Input);
			if (TestNotNull(TEXT("Offset Root Bone exposes its RotationMode input"), RotationModePin))
			{
				TestFalse(TEXT("The dynamic RotationMode input is visible"), RotationModePin->bHidden);
				TestEqual(
					TEXT("RotationMode uses an enum pin"),
					RotationModePin->PinType.PinCategory,
					UEdGraphSchema_K2::PC_Byte);
				TestEqual(
					TEXT("RotationMode uses EOffsetRootBoneMode"),
					RotationModePin->PinType.PinSubCategoryObject.Get(),
					static_cast<UObject*>(StaticEnum<EOffsetRootBoneMode>()));
			}

			const UEdGraphPin* ResetEveryFramePin =
				OffsetRootBoneNode->FindPin(TEXT("bResetEveryFrame"), EGPD_Input);
			if (TestNotNull(TEXT("Offset Root Bone exposes its bResetEveryFrame input"), ResetEveryFramePin))
			{
				TestFalse(TEXT("The dynamic reset input is visible"), ResetEveryFramePin->bHidden);
				TestEqual(
					TEXT("bResetEveryFrame uses a boolean pin"),
					ResetEveryFramePin->PinType.PinCategory,
					UEdGraphSchema_K2::PC_Boolean);
			}
		}

		if (SlotNode)
		{
			TestEqual(
				TEXT("Combat and harvesting montages keep the authoritative DefaultSlot"),
				SlotNode->Node.SlotName,
				FName(TEXT("DefaultSlot")));
		}

		if (OffsetRootRotationModeGetter)
		{
			const UEdGraphPin* RotationModeOutput =
				OffsetRootRotationModeGetter->FindPin(TEXT("OffsetRootRotationMode"), EGPD_Output);
			if (TestNotNull(TEXT("OffsetRootRotationMode getter exposes its value"), RotationModeOutput))
			{
				TestEqual(
					TEXT("The runtime rotation-mode output uses an enum pin"),
					RotationModeOutput->PinType.PinCategory,
					UEdGraphSchema_K2::PC_Byte);
				TestEqual(
					TEXT("The runtime rotation-mode output uses EOffsetRootBoneMode"),
					RotationModeOutput->PinType.PinSubCategoryObject.Get(),
					static_cast<UObject*>(StaticEnum<EOffsetRootBoneMode>()));
			}
		}
		if (ResetOffsetRootGetter)
		{
			const UEdGraphPin* ResetOutput =
				ResetOffsetRootGetter->FindPin(TEXT("bResetOffsetRootEveryFrame"), EGPD_Output);
			if (TestNotNull(TEXT("bResetOffsetRootEveryFrame getter exposes its value"), ResetOutput))
			{
				TestEqual(
					TEXT("The runtime reset output uses a boolean pin"),
					ResetOutput->PinType.PinCategory,
					UEdGraphSchema_K2::PC_Boolean);
			}
		}

		TestExclusiveLink(
			*this,
			TEXT("Motion Matching feeds Offset Root Bone"),
			MotionMatchingNode,
			TEXT("Pose"),
			OffsetRootBoneNode,
			TEXT("Source"));
		TestExclusiveLink(
			*this,
			TEXT("Offset Root Bone feeds DefaultSlot"),
			OffsetRootBoneNode,
			TEXT("Pose"),
			SlotNode,
			TEXT("Source"));
		TestExclusiveLink(
			*this,
			TEXT("DefaultSlot feeds Pose History"),
			SlotNode,
			TEXT("Pose"),
			PoseHistoryNode,
			TEXT("Source"));
		TestExclusiveLink(
			*this,
			TEXT("Pose History feeds the AnimGraph root"),
			PoseHistoryNode,
			TEXT("Pose"),
			RootNode,
			TEXT("Result"));
		TestExclusiveLink(
			*this,
			TEXT("The game-thread trajectory snapshot feeds Pose History"),
			TrajectoryGetter,
			TEXT("LocomotionTrajectory"),
			PoseHistoryNode,
			TEXT("TransformTrajectory"));
		TestExclusiveLink(
			*this,
			TEXT("The runtime turn state drives Offset Root Bone rotation mode"),
			OffsetRootRotationModeGetter,
			TEXT("OffsetRootRotationMode"),
			OffsetRootBoneNode,
			TEXT("RotationMode"));
		TestExclusiveLink(
			*this,
			TEXT("The one-frame hard-reset request drives Offset Root Bone"),
			ResetOffsetRootGetter,
			TEXT("bResetOffsetRootEveryFrame"),
			OffsetRootBoneNode,
			TEXT("bResetEveryFrame"));
	}

	TestEqual(
		TEXT("The pilot character is a direct child of the existing player Blueprint"),
		PilotCharacterBlueprint->ParentClass.Get(),
		BaseCharacterBlueprint->GeneratedClass.Get());
	TestTrue(
		TEXT("The pilot PawnClass differs from the default PawnClass"),
		PilotPawnData->PawnClass != BasePawnData->PawnClass);
	TestEqual(
		TEXT("The pilot PawnData selects the isolated character"),
		PilotPawnData->PawnClass.Get(),
		PilotCharacterBlueprint->GeneratedClass.Get());
	TestEqual(TEXT("PawnData TeamId is unchanged"), PilotPawnData->TeamId, BasePawnData->TeamId);
	TestEqual(
		TEXT("PawnData TagRelationshipMapping is unchanged"),
		PilotPawnData->TagRelationshipMapping.Get(),
		BasePawnData->TagRelationshipMapping.Get());
	TestEqual(
		TEXT("PawnData InputConfig is unchanged"),
		PilotPawnData->InputConfig.Get(),
		BasePawnData->InputConfig.Get());
	TestTrue(TEXT("PawnData AbilitySets are unchanged"), PilotPawnData->AbilitySets == BasePawnData->AbilitySets);
	TestEqual(
		TEXT("PawnData camera mode is unchanged"),
		PilotPawnData->DefaultCameraMode,
		BasePawnData->DefaultCameraMode);
	TestEqual(
		TEXT("PawnData inventory layout is unchanged"),
		PilotPawnData->InventoryLayoutDefinition.Get(),
		BasePawnData->InventoryLayoutDefinition.Get());

	ACharacter* BaseCharacterDefaults =
		BaseCharacterBlueprint->GeneratedClass
			? Cast<ACharacter>(BaseCharacterBlueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
	ACharacter* PilotCharacterDefaults =
		PilotCharacterBlueprint->GeneratedClass
			? Cast<ACharacter>(PilotCharacterBlueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
	if (TestNotNull(TEXT("The default character CDO loads"), BaseCharacterDefaults) &&
		TestNotNull(TEXT("The pilot character CDO loads"), PilotCharacterDefaults))
	{
		USkeletalMeshComponent* BaseMesh = BaseCharacterDefaults->GetMesh();
		USkeletalMeshComponent* PilotMesh = PilotCharacterDefaults->GetMesh();
		if (TestNotNull(TEXT("The default CharacterMesh0 exists"), BaseMesh) &&
			TestNotNull(TEXT("The pilot CharacterMesh0 exists"), PilotMesh))
		{
			TestEqual(TEXT("The pilot keeps the authoritative skeletal mesh"), PilotMesh->GetSkeletalMeshAsset(), BaseMesh->GetSkeletalMeshAsset());
			TestTrue(TEXT("The pilot keeps the mesh transform"), PilotMesh->GetRelativeTransform().Equals(BaseMesh->GetRelativeTransform()));
			TestEqual(TEXT("The pilot keeps the mesh collision profile"), PilotMesh->GetCollisionProfileName(), BaseMesh->GetCollisionProfileName());
			TestEqual(
				TEXT("The pilot keeps the mesh visibility tick policy"),
				PilotMesh->VisibilityBasedAnimTickOption,
				BaseMesh->VisibilityBasedAnimTickOption);
			TestEqual(
				TEXT("The pilot keeps animation update-rate optimization policy"),
				static_cast<bool>(PilotMesh->bEnableUpdateRateOptimizations),
				static_cast<bool>(BaseMesh->bEnableUpdateRateOptimizations));
			TestEqual(
				TEXT("Only the pilot mesh AnimClass changes"),
				PilotMesh->GetAnimClass(),
				PilotAnimBlueprint->GeneratedClass.Get());
		}
	}

	TestEqual(
		TEXT("The prototype Experience still selects the default PawnData"),
		BaseExperience->DefaultPawnData.Get(),
		static_cast<const URpgPawnData*>(BasePawnData));
	TestEqual(
		TEXT("The pilot Experience selects only the pilot PawnData"),
		PilotExperience->DefaultPawnData.Get(),
		static_cast<const URpgPawnData*>(PilotPawnData));
	TestTrue(
		TEXT("The pilot Experience keeps the prototype GameFeature order"),
		PilotExperience->GameFeaturesToEnable == BaseExperience->GameFeaturesToEnable);
	TestTrue(
		TEXT("The pilot Experience keeps the prototype ActionSets"),
		PilotExperience->ActionSets == BaseExperience->ActionSets);
	TestEqual(
		TEXT("The pilot Experience keeps the prototype action count"),
		PilotExperience->Actions.Num(),
		BaseExperience->Actions.Num());
	for (int32 Index = 0;
		Index < PilotExperience->Actions.Num() && Index < BaseExperience->Actions.Num();
		++Index)
	{
		FString Mismatch;
		const bool bActionsEquivalent = AreActionPropertiesEquivalent(
			BaseExperience->Actions[Index],
			PilotExperience->Actions[Index],
			Mismatch);
		TestTrue(
			*FString::Printf(
				TEXT("Experience action %d is an unchanged duplicate%s%s"),
				Index,
				Mismatch.IsEmpty() ? TEXT("") : TEXT(": "),
				*Mismatch),
			bActionsEquivalent);
	}
	TestEqual(
		TEXT("The pilot Experience is registered with the expected primary-asset identity"),
		PilotExperience->GetPrimaryAssetId().ToString(),
		FString(TEXT("RpgExperienceDefinition:RpgGaspPilotExperience")));

	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.ScanPathsSynchronous(
		{
			FString(TEXT("/Game/SurvivalRpg/Characters/Mannequins/Anims/GASP")),
			FString(TEXT("/Game/SurvivalRpg/Core/Character/GASP")),
			FString(TEXT("/Game/SurvivalRpg/System/Experiences")),
			FString(TEXT("/RpgGaspLocomotion")),
		},
		true,
		false);
	AssetRegistry.WaitForCompletion();

	const FName PilotRootPackages[] = {
		FName(PilotAnimBlueprintPackage),
		FName(PilotCharacterBlueprintPackage),
		FName(PilotPawnDataPackage),
		FName(PilotExperiencePackage),
	};
	TSet<FName> DependencyClosure;
	GatherPilotDependencyClosure(AssetRegistry, PilotRootPackages, DependencyClosure);
	for (const FName PackageName : DependencyClosure)
	{
		TestFalse(
			*FString::Printf(
				TEXT("The isolated pilot has no GASP sample dependency on %s"),
				*PackageName.ToString()),
			IsForbiddenPilotDependency(PackageName.ToString()));
	}

	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
