#include "Blueprint/RpgBlueprintAssetTools.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Components/ActorComponent.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Engine.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogRpgBlueprintAssetTools, Log, All);

namespace
{
	/** Also visits owned objects that are not currently reachable through a serialized reference. */
	class FOwnedObjectReferenceArchive final : public FArchiveReplaceObjectRef<UObject>
	{
	public:
		using FArchiveReplaceObjectRef<UObject>::operator<<;

		FOwnedObjectReferenceArchive(UObject* Asset, const TMap<UObject*, UObject*>& Replacements,
			const TArray<UObject*>& OwnedObjects)
			: FArchiveReplaceObjectRef<UObject>(Asset, Replacements,
				EArchiveReplaceObjectFlags::DelayStart | EArchiveReplaceObjectFlags::IgnoreOuterRef
				| EArchiveReplaceObjectFlags::IgnoreArchetypeRef | EArchiveReplaceObjectFlags::TrackReplacedReferences)
		{
			// Seed the traversal once so shared children cannot be processed repeatedly.
			for (UObject* Object : OwnedObjects)
			{
				if (Object != Asset && IsValid(Object) && Object->IsIn(Asset))
				{
					SerializedObjects.Add(Object);
					PendingSerializationObjects.Add(Object);
				}
			}
			SerializeSearchObject();
		}

		virtual FArchive& operator<<(FSoftObjectPath& Path) override
		{
			UObject* Object = Path.ResolveObject();
			if (Object && ReplacementMap.Contains(Object))
			{
				FArchiveReplaceObjectRef<UObject>::operator<<(Object);
				Path = FSoftObjectPath(Object);
			}
			// The base archive rewrites every resolved path, even when no mapping matches.
			// Preserve unrelated paths, including redirector spelling, byte for byte.
			return *this;
		}

		virtual FArchive& operator<<(FSoftObjectPtr& Pointer) override
		{
			FSoftObjectPath Path = Pointer.ToSoftObjectPath();
			*this << Path;
			if (Path != Pointer.ToSoftObjectPath())
			{
				Pointer = Path;
			}
			return *this;
		}
	};
}

bool URpgBlueprintAssetTools::ImplementInterface(UBlueprint* Blueprint, TSubclassOf<UInterface> InterfaceClass)
{
	if (!IsValid(Blueprint) || !IsValid(InterfaceClass.Get()) || !InterfaceClass->HasAnyClassFlags(CLASS_Interface))
	{
		return false;
	}

	if ((Blueprint->GeneratedClass && Blueprint->GeneratedClass->ImplementsInterface(InterfaceClass)) ||
		(Blueprint->ParentClass && Blueprint->ParentClass->ImplementsInterface(InterfaceClass)))
	{
		return true;
	}

	for (const FBPInterfaceDescription& Existing : Blueprint->ImplementedInterfaces)
	{
		if (Existing.Interface == InterfaceClass)
		{
			return true;
		}
	}

	const FScopedTransaction Transaction(NSLOCTEXT("RpgBlueprintAssetTools", "ImplementInterface", "Implement Blueprint Interface"));
	Blueprint->Modify();
	return FBlueprintEditorUtils::ImplementNewInterface(Blueprint, InterfaceClass->GetClassPathName());
}

bool URpgBlueprintAssetTools::ChangeOwnSCSComponentClass(UBlueprint* Blueprint, FName ComponentVariableName,
	TSubclassOf<UActorComponent> NewComponentClass)
{
	UClass* NewClass = NewComponentClass.Get();
	if (!IsInGameThread() || !GIsEditor || !GEditor || !GEngine || !IsValid(Blueprint) || !Blueprint->IsAsset()
		|| (Blueprint->HasAllFlags(RF_WasLoaded) && !Blueprint->HasAllFlags(RF_LoadCompleted))
		|| Blueprint->bBeingCompiled || Blueprint->bIsRegeneratingOnLoad || Blueprint->bQueuedForCompilation
		|| Blueprint->bSuppressStructurallyModified || ComponentVariableName.IsNone()
		|| !IsValid(NewClass) || NewClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)) return false;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE) return false;
	}
	USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
	if (!SCS || SCS->GetBlueprint() != Blueprint || !Blueprint->GeneratedClass) return false;
	USCS_Node* Node = nullptr;
	for (USCS_Node* Candidate : SCS->GetAllNodes())
	{
		if (Candidate && Candidate->GetOuter() == SCS && Candidate->GetVariableName() == ComponentVariableName)
		{
			if (Node) return false;
			Node = Candidate;
		}
	}
	UActorComponent* OldTemplate = Node ? Node->ComponentTemplate.Get() : nullptr;
	if (!IsValid(OldTemplate) || !OldTemplate->IsTemplate() || OldTemplate->GetOuter() != Blueprint->GeneratedClass
		|| OldTemplate->IsRegistered() || Node->ComponentClass != OldTemplate->GetClass()
		|| !NewClass->IsChildOf(OldTemplate->GetClass())
		|| !OldTemplate->GetOuter()->IsA(NewClass->ClassWithin)) return false;

	// DestClass duplication supports inherited reflected data and instanced UObjects. Nested component
	// hierarchies need a different reinstancing operation; do not silently apply this narrower contract to them.
	for (UObject* TemplateRoot : {static_cast<UObject*>(OldTemplate), NewClass->GetDefaultObject()})
	{
		TArray<UObject*> Children;
		GetObjectsWithOuter(TemplateRoot, Children, EGetObjectsFlags::IncludeNestedObjects);
		for (UObject* Child : Children)
		{
			if (Child->IsA<UActorComponent>()) return false;
		}
	}

	TArray<UObject*> Roots{Blueprint, Blueprint->GeneratedClass.Get()};
	if (Blueprint->SkeletonGeneratedClass && Blueprint->SkeletonGeneratedClass != Blueprint->GeneratedClass)
	{
		Roots.Add(Blueprint->SkeletonGeneratedClass);
	}
	TSet<UObject*> ExistingObjects;
	for (UObject* Root : Roots)
	{
		TArray<UObject*> Owned{Root};
		GetObjectsWithOuter(Root, Owned, EGetObjectsFlags::IncludeNestedObjects);
		for (UObject* Object : Owned) ExistingObjects.Add(Object);
	}
	auto RecordExistingObjects = [&ExistingObjects]()
	{
		for (UObject* Object : ExistingObjects)
		{
			if (IsValid(Object))
			{
				Object->SetFlags(RF_Transactional);
				Object->Modify(false);
			}
		}
	};
	auto RefreshComponentReferences = [Blueprint]()
	{
		// SCS now supplies the new property type. Regenerate that skeleton before reconstructing GET,
		// call and delegate nodes whose serialized pins can still carry the previous component class.
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		// Unreal refreshes structural entry/event nodes first, rebuilds their signatures, then their users.
		FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
	};
	if (NewClass == OldTemplate->GetClass())
	{
		FScopedTransaction Transaction(NSLOCTEXT("RpgBlueprintAssetTools", "RefreshOwnSCSComponentClass", "Refresh Blueprint Component Class References"));
		RecordExistingObjects();
		RefreshComponentReferences();
		return true;
	}

	const FName TemplateName = OldTemplate->GetFName();
	UObject* TemplateOuter = OldTemplate->GetOuter();
	TMap<UObject*, UObject*> Replacements;
	FObjectDuplicationParameters Parameters(OldTemplate, TemplateOuter);
	Parameters.DestName = MakeUniqueObjectName(TemplateOuter, NewClass, TEXT("RpgSCSTemplateReplacement"));
	Parameters.DestClass = NewClass;
	Parameters.ApplyFlags = RF_Transactional;
	Parameters.CreatedObjects = &Replacements;
	UActorComponent* NewTemplate = Cast<UActorComponent>(StaticDuplicateObjectEx(Parameters));
	if (!NewTemplate) return false;
	Replacements.Add(OldTemplate, NewTemplate);

	FScopedTransaction Transaction(NSLOCTEXT("RpgBlueprintAssetTools", "ChangeOwnSCSComponentClass", "Change Blueprint Component Class"));
	RecordExistingObjects();
	const ERenameFlags RenameFlags = REN_DontCreateRedirectors | REN_AllowPackageLinkerMismatch | REN_DoNotDirty;
	const FName RetiredName = MakeUniqueObjectName(GetTransientPackage(), OldTemplate->GetClass(), TEXT("RpgSCSPreviousTemplate"));
	if (!OldTemplate->Rename(*RetiredName.ToString(), GetTransientPackage(), RenameFlags))
	{
		Transaction.Cancel();
		NewTemplate->MarkAsGarbage();
		return false;
	}
	// Record the new object's temporary name after the old object's original name. Undo can vacate the
	// canonical name before restoring the original template; neither object is destroyed by the transaction.
	for (const TPair<UObject*, UObject*>& Pair : Replacements)
	{
		Pair.Value->SetFlags(RF_Transactional);
		Pair.Value->Modify(false);
	}
	if (!NewTemplate->Rename(*TemplateName.ToString(), TemplateOuter, RenameFlags))
	{
		OldTemplate->Rename(*TemplateName.ToString(), TemplateOuter, RenameFlags);
		Transaction.Cancel();
		NewTemplate->MarkAsGarbage();
		return false;
	}

	// Component templates are package-sibling generated-class objects, not children of UBlueprint.
	// Include both generated roots while keeping unrelated assets and live actor instances out of the archive.
	for (UObject* Root : Roots)
	{
		TArray<UObject*> Owned{Root};
		GetObjectsWithOuter(Root, Owned, EGetObjectsFlags::IncludeNestedObjects);
		FOwnedObjectReferenceArchive Archive(Root, Replacements, Owned);
	}
	Node->ComponentClass = NewClass;
	Node->ComponentTemplate = NewTemplate;
	RefreshComponentReferences();
	return true;
}

int32 URpgBlueprintAssetTools::RemapOwnedObjectReferences(UObject* Asset, const TMap<UObject*, UObject*>& Replacements)
{
	if (!IsInGameThread() || !IsValid(Asset) || !Asset->IsAsset() || Asset->IsA<UPackage>())
	{
		return -1;
	}

	TMap<UObject*, UObject*> EffectiveReplacements;
	for (const TPair<UObject*, UObject*>& Replacement : Replacements)
	{
		if (!IsValid(Replacement.Key) || !IsValid(Replacement.Value)
			|| Replacement.Key->IsA<UPackage>() || Replacement.Value->IsA<UPackage>())
		{
			return -1;
		}
		if (Replacement.Key != Replacement.Value)
		{
			EffectiveReplacements.Add(Replacement.Key, Replacement.Value);
		}
	}
	if (EffectiveReplacements.IsEmpty())
	{
		return 0;
	}
	for (const TPair<UObject*, UObject*>& Replacement : EffectiveReplacements)
	{
		// Native Serialize and AddReferencedObjects may both visit a reference. Disallow chains/cycles
		// so a second visit cannot turn an A -> B mapping into an unintended A -> C replacement.
		if (EffectiveReplacements.Contains(Replacement.Value))
		{
			return -1;
		}
	}

	TArray<UObject*> OwnedObjects;
	OwnedObjects.Add(Asset);
	GetObjectsWithOuter(Asset, OwnedObjects, EGetObjectsFlags::IncludeNestedObjects);
	FScopedTransaction Transaction(NSLOCTEXT("RpgBlueprintAssetTools", "RemapOwnedObjectReferences", "Remap Asset Object References"));
	TArray<UObject*> NewlyTransactionalObjects;
	for (UObject* Object : OwnedObjects)
	{
		if (IsValid(Object))
		{
			// Capture children before serialization changes their properties; no-op calls stay clean.
			if (!Object->HasAnyFlags(RF_Transactional))
			{
				NewlyTransactionalObjects.Add(Object);
				Object->SetFlags(RF_Transactional);
			}
			Object->Modify(false);
		}
	}

	FOwnedObjectReferenceArchive Archive(Asset, EffectiveReplacements, OwnedObjects);
	const int32 ReplacementCount = static_cast<int32>(Archive.GetCount());
	if (ReplacementCount == 0)
	{
		Transaction.Cancel();
		for (UObject* Object : NewlyTransactionalObjects)
		{
			Object->ClearFlags(RF_Transactional);
		}
		return 0;
	}

	for (const TPair<UObject*, TArray<FProperty*>>& ChangedObject : Archive.GetReplacedReferences())
	{
		if (ChangedObject.Key != Asset && IsValid(ChangedObject.Key) && ChangedObject.Key->IsIn(Asset))
		{
			ChangedObject.Key->PostEditChange();
		}
	}
	if (UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
	{
		// MarkBlueprintAsModified also updates external derived classes; keep this operation asset-local.
		Blueprint->bCachedDependenciesUpToDate = false;
		Blueprint->Status = BS_Dirty;
	}
	Asset->MarkPackageDirty();
	Asset->PostEditChange();
	return ReplacementCount;
}

int32 URpgBlueprintAssetTools::CopyAnimationNotifies(UAnimSequenceBase* Source, UAnimSequenceBase* Target)
{
	if (!IsInGameThread() || !GIsEditor || !GEditor || !GEngine || !IsValid(Source) || !IsValid(Target)
		|| Source == Target || !Source->IsAsset() || !Target->IsAsset() || Source->GetClass() != Target->GetClass()
		|| Source->GetSkeleton() != Target->GetSkeleton() || Source->GetPlayLength() != Target->GetPlayLength()) return -1;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE) return -1;
	}
	if (const UAnimMontage* SourceMontage = Cast<UAnimMontage>(Source))
	{
		const UAnimMontage* TargetMontage = CastChecked<UAnimMontage>(Target);
		if (SourceMontage->SlotAnimTracks.Num() != TargetMontage->SlotAnimTracks.Num()) return -1;
		for (int32 SlotIndex = 0; SlotIndex < SourceMontage->SlotAnimTracks.Num(); ++SlotIndex)
		{
			const FSlotAnimationTrack& SourceSlot = SourceMontage->SlotAnimTracks[SlotIndex];
			const FSlotAnimationTrack& TargetSlot = TargetMontage->SlotAnimTracks[SlotIndex];
			if (SourceSlot.SlotName != TargetSlot.SlotName || SourceSlot.AnimTrack.AnimSegments.Num() != TargetSlot.AnimTrack.AnimSegments.Num()) return -1;
			for (int32 SegmentIndex = 0; SegmentIndex < SourceSlot.AnimTrack.AnimSegments.Num(); ++SegmentIndex)
			{
				const FAnimSegment& A = SourceSlot.AnimTrack.AnimSegments[SegmentIndex];
				const FAnimSegment& B = TargetSlot.AnimTrack.AnimSegments[SegmentIndex];
				if (A.GetAnimReference() != B.GetAnimReference() || A.StartPos != B.StartPos || A.AnimStartTime != B.AnimStartTime
					|| A.AnimEndTime != B.AnimEndTime || A.AnimPlayRate != B.AnimPlayRate || A.LoopingCount != B.LoopingCount) return -1;
			}
		}
	}
	for (const FAnimNotifyEvent& Event : Source->Notifies)
	{
		if (!FMath::IsFinite(Event.GetTime()) || !FMath::IsFinite(Event.EndLink.GetTime())
			|| !Source->AnimNotifyTracks.IsValidIndex(Event.TrackIndex)
			|| (Event.Notify && !IsValid(Event.Notify)) || (Event.NotifyStateClass && !IsValid(Event.NotifyStateClass))) return -1;
	}

	FScopedTransaction Transaction(NSLOCTEXT("RpgBlueprintAssetTools", "CopyAnimationNotifies", "Copy Animation Notifies"));
	Target->SetFlags(RF_Transactional);
	Target->Modify(false);
	TArray<FAnimNotifyEvent> Copied = Source->Notifies;
	TMap<UObject*, UObject*> Replacements;
	Replacements.Add(Source, Target);
	auto DuplicateNotify = [&Replacements, Target](UObject* Original) -> UObject*
	{
		if (!Original) return nullptr;
		if (UObject** Existing = Replacements.Find(Original)) return *Existing;
		FName Name = Original->GetFName();
		if (FindObjectFast<UObject>(Target, Name)) Name = MakeUniqueObjectName(Target, Original->GetClass(), Name);
		UObject* Duplicate = DuplicateObject<UObject>(Original, Target, Name);
		if (Duplicate)
		{
			Duplicate->SetFlags(RF_Transactional);
			Replacements.Add(Original, Duplicate);
		}
		return Duplicate;
	};
	for (FAnimNotifyEvent& Event : Copied)
	{
		UAnimNotify* Notify = Cast<UAnimNotify>(DuplicateNotify(Event.Notify));
		UAnimNotifyState* State = Cast<UAnimNotifyState>(DuplicateNotify(Event.NotifyStateClass));
		if ((Event.Notify && !Notify) || (Event.NotifyStateClass && !State))
		{
			Transaction.Cancel();
			return -1;
		}
		Event.Notify = Notify;
		Event.NotifyStateClass = State;
	}
	for (FAnimNotifyEvent& Event : Copied)
	{
		// Serialize only the copied struct: unrelated target properties (e.g. a parent montage) must not be remapped.
		// Replacing its cached linked asset preserves timing beyond the final segment without Link()/Update() clamping it.
		FArchiveReplaceObjectRef<UObject> Archive(Target, Replacements, EArchiveReplaceObjectFlags::DelayStart
			| EArchiveReplaceObjectFlags::IgnoreOuterRef | EArchiveReplaceObjectFlags::IgnoreArchetypeRef);
		FAnimNotifyEvent::StaticStruct()->SerializeItem(Archive, &Event, nullptr);
	}
	for (const TPair<UObject*, UObject*>& Pair : Replacements)
	{
		if (Pair.Key == Source) continue;
		FArchiveReplaceObjectRef<UObject> Archive(Pair.Value, Replacements,
			EArchiveReplaceObjectFlags::IgnoreOuterRef | EArchiveReplaceObjectFlags::IgnoreArchetypeRef);
	}
	while (Target->AnimNotifyTracks.Num() < Source->AnimNotifyTracks.Num())
	{
		const FAnimNotifyTrack& SourceTrack = Source->AnimNotifyTracks[Target->AnimNotifyTracks.Num()];
		FAnimNotifyTrack& NewTrack = Target->AnimNotifyTracks.AddDefaulted_GetRef();
		NewTrack.TrackName = SourceTrack.TrackName;
		NewTrack.TrackColor = SourceTrack.TrackColor;
	}
	Target->Notifies = MoveTemp(Copied);
	Target->RefreshCacheData();
	Target->MarkPackageDirty();
	return Target->Notifies.Num();
}

namespace
{
	/** DestClass duplication is only safe for these explicitly equivalent reflected schemas. */
	class FNotifyRemapContract
	{
	public:
		explicit FNotifyRemapContract(const TMap<UObject*, UObject*>& InTypes) : Types(InTypes) {}
		const FString& GetFailureReason() const { return FailureReason; }

		bool EquivalentObjectClass(const UClass* A, const UClass* B) const
		{
			if (A == B) return true;
			if (!IsValid(A) || !IsValid(B) || Types.FindRef(const_cast<UClass*>(A)) != B ||
				!Cast<UBlueprintGeneratedClass>(A) || !Cast<UBlueprintGeneratedClass>(B))
				return Fail(FString::Printf(TEXT("Missing explicit Blueprint object-class mapping: %s -> %s"), *GetPathNameSafe(A), *GetPathNameSafe(B)));
			const UBlueprint* BlueprintA = Cast<UBlueprint>(A->ClassGeneratedBy);
			const UBlueprint* BlueprintB = Cast<UBlueprint>(B->ClassGeneratedBy);
			if (!BlueprintA || !BlueprintB || BlueprintA->GeneratedClass != A || BlueprintB->GeneratedClass != B ||
				BlueprintA->Status == BS_Error || BlueprintA->Status == BS_Dirty || BlueprintB->Status == BS_Error || BlueprintB->Status == BS_Dirty ||
				A->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists) || B->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists))
				return Fail(FString::Printf(TEXT("Stale/uncompiled object-class mapping: %s -> %s"), *GetPathNameSafe(A), *GetPathNameSafe(B)));
			const UClass* NativeA = A;
			const UClass* NativeB = B;
			while (!NativeA->HasAnyClassFlags(CLASS_Native)) NativeA = NativeA->GetSuperClass();
			while (!NativeB->HasAnyClassFlags(CLASS_Native)) NativeB = NativeB->GetSuperClass();
			return NativeA == NativeB || Fail(FString::Printf(TEXT("Referenced object native ancestry differs: %s -> %s"), *GetPathNameSafe(A), *GetPathNameSafe(B)));
		}

		bool EquivalentType(const UObject* A, const UObject* B, int32 Depth = 0) const
		{
			if (A == B) return true;
			if (!IsValid(A) || !IsValid(B) || Depth > 32 || Types.FindRef(const_cast<UObject*>(A)) != B)
				return Fail(FString::Printf(TEXT("Missing/invalid explicit type mapping or recursion limit: %s -> %s depth=%d"), *GetPathNameSafe(A), *GetPathNameSafe(B), Depth));
			if (const UEnum* EnumA = Cast<UEnum>(A))
			{
				const UEnum* EnumB = Cast<UEnum>(B);
				if (!EnumB || EnumA->GetCppForm() != EnumB->GetCppForm() || EnumA->NumEnums() != EnumB->NumEnums())
					return Fail(FString::Printf(TEXT("Enum kind/form/count differs: %s -> %s"), *GetPathNameSafe(A), *GetPathNameSafe(B)));
				for (int32 Index = 0; Index < EnumA->NumEnums(); ++Index)
				{
					if (EnumA->GetNameStringByIndex(Index) != EnumB->GetNameStringByIndex(Index) ||
						EnumA->GetValueByIndex(Index) != EnumB->GetValueByIndex(Index))
						return Fail(FString::Printf(TEXT("Enum member differs at %d: %s.%s=%lld -> %s.%s=%lld"), Index,
							*GetPathNameSafe(A), *EnumA->GetNameStringByIndex(Index), EnumA->GetValueByIndex(Index),
							*GetPathNameSafe(B), *EnumB->GetNameStringByIndex(Index), EnumB->GetValueByIndex(Index)));
				}
				return true;
			}
			if (const UClass* ClassA = Cast<UClass>(A))
			{
				const UClass* ClassB = Cast<UClass>(B);
				const UBlueprint* BlueprintA = Cast<UBlueprint>(ClassA->ClassGeneratedBy);
				const UBlueprint* BlueprintB = ClassB ? Cast<UBlueprint>(ClassB->ClassGeneratedBy) : nullptr;
				if (!ClassB || !Cast<UBlueprintGeneratedClass>(ClassA) || !Cast<UBlueprintGeneratedClass>(ClassB) ||
					!BlueprintA || !BlueprintB || BlueprintA->GeneratedClass != ClassA || BlueprintB->GeneratedClass != ClassB ||
					BlueprintA->Status == BS_Error || BlueprintA->Status == BS_Dirty || BlueprintB->Status == BS_Error || BlueprintB->Status == BS_Dirty)
					return Fail(FString::Printf(TEXT("Generated class is stale/uncompiled/invalid: %s -> %s; BP statuses=%d/%d"),
						*GetPathNameSafe(A), *GetPathNameSafe(B), BlueprintA ? static_cast<int32>(BlueprintA->Status) : -1, BlueprintB ? static_cast<int32>(BlueprintB->Status) : -1));
				if (!EquivalentType(ClassA->GetSuperClass(), ClassB->GetSuperClass(), Depth + 1)) return false;
				return EquivalentFields(ClassA, ClassB, Depth + 1);
			}
			const UScriptStruct* StructA = Cast<UScriptStruct>(A);
			const UScriptStruct* StructB = Cast<UScriptStruct>(B);
			// Different native serializers cannot be proved compatible by reflected fields alone.
			if (!StructA || !StructB || StructA->GetCppStructOps() || StructB->GetCppStructOps() || StructA->StructFlags != StructB->StructFlags)
				return Fail(FString::Printf(TEXT("Unsupported mapped type, native struct operations, or differing struct flags: %s -> %s; flags=%u/%u"),
					*GetPathNameSafe(A), *GetPathNameSafe(B), StructA ? static_cast<uint32>(StructA->StructFlags) : 0u, StructB ? static_cast<uint32>(StructB->StructFlags) : 0u));
			return EquivalentFields(StructA, StructB, Depth + 1);
		}

	private:
		bool Fail(FString Reason) const { if (FailureReason.IsEmpty()) FailureReason = MoveTemp(Reason); return false; }
		bool EquivalentFields(const UStruct* A, const UStruct* B, int32 Depth) const
		{
			if (Depth > 32 || A->GetPropertiesSize() != B->GetPropertiesSize() || A->GetMinAlignment() != B->GetMinAlignment())
				return Fail(FString::Printf(TEXT("Layout differs: %s size=%d align=%d -> %s size=%d align=%d depth=%d"),
					*GetPathNameSafe(A), A->GetPropertiesSize(), A->GetMinAlignment(), *GetPathNameSafe(B), B->GetPropertiesSize(), B->GetMinAlignment(), Depth));
			TFieldIterator<FProperty> Left(A), Right(B);
			for (; Left && Right; ++Left, ++Right)
			{
				if (Left->GetFName() != Right->GetFName() || Left->GetOffset_ForInternal() != Right->GetOffset_ForInternal())
					return Fail(FString::Printf(TEXT("Field name/order/offset differs: %s.%s offset=%d -> %s.%s offset=%d"),
						*GetPathNameSafe(A), *Left->GetName(), Left->GetOffset_ForInternal(), *GetPathNameSafe(B), *Right->GetName(), Right->GetOffset_ForInternal()));
				if (!EquivalentProperty(*Left, *Right, Depth + 1, Cast<UClass>(A) != nullptr))
				{
					FailureReason = FString::Printf(TEXT("%s.%s -> %s.%s: %s"), *GetPathNameSafe(A), *Left->GetName(), *GetPathNameSafe(B), *Right->GetName(), *FailureReason);
					return false;
				}
			}
			return (!Left && !Right) || Fail(FString::Printf(TEXT("Field counts differ: %s -> %s"), *GetPathNameSafe(A), *GetPathNameSafe(B)));
		}

		bool EquivalentProperty(const FProperty* A, const FProperty* B, int32 Depth, bool bDirectClassField = false) const
		{
			if (Depth > 32 || A->GetClass() != B->GetClass() || A->ArrayDim != B->ArrayDim || A->GetSize() != B->GetSize() ||
				A->GetPropertyFlags() != B->GetPropertyFlags())
				return Fail(FString::Printf(TEXT("Property kind/array/size/flags differ: %s[%d] size=%d flags=%llu -> %s[%d] size=%d flags=%llu depth=%d"),
					*A->GetClass()->GetName(), A->ArrayDim, A->GetSize(), static_cast<uint64>(A->GetPropertyFlags()),
					*B->GetClass()->GetName(), B->ArrayDim, B->GetSize(), static_cast<uint64>(B->GetPropertyFlags()), Depth));
			if (const FBoolProperty* P = CastField<FBoolProperty>(A))
			{
				const FBoolProperty* Q = CastFieldChecked<FBoolProperty>(B);
				return (P->GetFieldMask() == Q->GetFieldMask() && P->GetByteMask() == Q->GetByteMask() &&
					P->GetByteOffset() == Q->GetByteOffset() && P->GetBoolFieldSize() == Q->GetBoolFieldSize()) || Fail(TEXT("Boolean storage masks/offset/size differ"));
			}
			if (A->SameType(B)) return true;
			if (bDirectClassField && A->GetClass() == FObjectProperty::StaticClass())
			{
				return EquivalentObjectClass(CastFieldChecked<FObjectProperty>(A)->PropertyClass, CastFieldChecked<FObjectProperty>(B)->PropertyClass);
			}
			if (const FByteProperty* P = CastField<FByteProperty>(A)) return EquivalentType(P->Enum, CastFieldChecked<FByteProperty>(B)->Enum, Depth + 1);
			if (const FEnumProperty* P = CastField<FEnumProperty>(A))
			{
				const FEnumProperty* Q = CastFieldChecked<FEnumProperty>(B);
				return EquivalentType(P->GetEnum(), Q->GetEnum(), Depth + 1) && EquivalentProperty(P->GetUnderlyingProperty(), Q->GetUnderlyingProperty(), Depth + 1);
			}
			if (const FStructProperty* P = CastField<FStructProperty>(A)) return EquivalentType(P->Struct, CastFieldChecked<FStructProperty>(B)->Struct, Depth + 1);
			if (const FArrayProperty* P = CastField<FArrayProperty>(A)) return EquivalentProperty(P->Inner, CastFieldChecked<FArrayProperty>(B)->Inner, Depth + 1);
			if (const FSetProperty* P = CastField<FSetProperty>(A)) return EquivalentProperty(P->ElementProp, CastFieldChecked<FSetProperty>(B)->ElementProp, Depth + 1);
			if (const FMapProperty* P = CastField<FMapProperty>(A))
			{
				const FMapProperty* Q = CastFieldChecked<FMapProperty>(B);
				return EquivalentProperty(P->KeyProp, Q->KeyProp, Depth + 1) && EquivalentProperty(P->ValueProp, Q->ValueProp, Depth + 1);
			}
			// Nested objects and soft/class/interface/delegate constraints require identical types.
			return Fail(FString::Printf(TEXT("Unsupported differing reflected property types: %s -> %s"), *A->GetCPPType(), *B->GetCPPType()));
		}
		const TMap<UObject*, UObject*>& Types;
		mutable FString FailureReason;
	};
}

int32 URpgBlueprintAssetTools::RemapAnimationNotifyClasses(UAnimSequenceBase* Animation,
	const TMap<UClass*, UClass*>& Replacements, const TMap<UObject*, UObject*>& TypeReplacements,
	const TMap<UObject*, UObject*>& ObjectReplacements)
{
	auto Reject = [Animation](const FString& Reason) -> int32
	{
		UE_LOG(LogRpgBlueprintAssetTools, Warning, TEXT("Notify class remap rejected: Asset=%s Reason=%s"), *GetPathNameSafe(Animation), *Reason);
		return -1;
	};
	if (!IsInGameThread() || !GIsEditor || !GEditor || !GEngine || !IsValid(Animation) || !Animation->IsAsset() ||
		!Animation->GetOutermost()->GetName().StartsWith(TEXT("/Game/"))) return Reject(TEXT("Unavailable editor game thread or invalid project animation"));
	for (const FWorldContext& Context : GEngine->GetWorldContexts()) { if (Context.WorldType == EWorldType::PIE) return Reject(TEXT("PIE is running")); }
	TMap<UObject*, UObject*> Types = TypeReplacements;
	for (const TPair<UClass*, UClass*>& Pair : Replacements)
	{
		if (!IsValid(Pair.Key) || !IsValid(Pair.Value) ||
			(Types.Contains(Pair.Key) && Types.FindRef(Pair.Key) != Pair.Value)) return Reject(TEXT("Invalid or conflicting notify-class mapping"));
		Types.Add(Pair.Key, Pair.Value);
	}
	const FNotifyRemapContract Contract(Types);
	for (const TPair<UObject*, UObject*>& Pair : Types)
	{
		if (!IsValid(Pair.Key) || !IsValid(Pair.Value) ||
			!Pair.Value->GetOutermost()->GetName().StartsWith(TEXT("/Game/")) ||
			(Pair.Key != Pair.Value && Types.Contains(Pair.Value)))
			return Reject(FString::Printf(TEXT("Invalid/non-project/cyclic type mapping: %s -> %s"), *GetPathNameSafe(Pair.Key), *GetPathNameSafe(Pair.Value)));
		// A referenced asset is never copied into another class's memory. Notify inheritance/layout
		// is checked below; other explicit class pairs require ancestry and valid generated classes.
		const UClass* ClassA = Cast<UClass>(Pair.Key);
		if (!(ClassA ? Contract.EquivalentObjectClass(ClassA, Cast<UClass>(Pair.Value)) : Contract.EquivalentType(Pair.Key, Pair.Value))) return Reject(Contract.GetFailureReason());
	}
	for (const TPair<UObject*, UObject*>& Pair : ObjectReplacements)
	{
		if (!IsValid(Pair.Key) || !IsValid(Pair.Value) || !Pair.Key->IsAsset() || !Pair.Value->IsAsset() ||
			Pair.Key->IsA<UStruct>() || Pair.Key->IsA<UEnum>() || Pair.Value->IsA<UStruct>() || Pair.Value->IsA<UEnum>() ||
			!Pair.Value->GetOutermost()->GetName().StartsWith(TEXT("/Game/")) ||
			(Pair.Key != Pair.Value && ObjectReplacements.Contains(Pair.Value))) return Reject(TEXT("Invalid/non-project/cyclic object asset mapping"));
		if (!Contract.EquivalentObjectClass(Pair.Key->GetClass(), Pair.Value->GetClass())) return Reject(Contract.GetFailureReason());
	}
	for (const TPair<UClass*, UClass*>& Pair : Replacements)
	{
		if (!Contract.EquivalentType(Pair.Key, Pair.Value)) return Reject(Contract.GetFailureReason());
		const UBlueprint* SourceBP = Cast<UBlueprint>(Pair.Key->ClassGeneratedBy);
		const UBlueprint* TargetBP = Cast<UBlueprint>(Pair.Value->ClassGeneratedBy);
		if (!SourceBP || !TargetBP || SourceBP->GeneratedClass != Pair.Key || TargetBP->GeneratedClass != Pair.Value ||
			Pair.Key->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) ||
			Pair.Value->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) ||
			(!Pair.Key->IsChildOf(UAnimNotify::StaticClass()) && !Pair.Key->IsChildOf(UAnimNotifyState::StaticClass())) ||
			Pair.Key->IsChildOf(UAnimNotify::StaticClass()) != Pair.Value->IsChildOf(UAnimNotify::StaticClass()) ||
			Pair.Key->IsChildOf(UAnimNotifyState::StaticClass()) != Pair.Value->IsChildOf(UAnimNotifyState::StaticClass()))
			return Reject(FString::Printf(TEXT("Invalid/stale/abstract class or different notify family: %s -> %s"), *GetPathNameSafe(Pair.Key), *GetPathNameSafe(Pair.Value)));
	}
	TArray<UObject*> Originals;
	TSet<UObject*> ReflectedCopies;
	TMap<UObject*, TMap<const FObjectProperty*, UObject*>> ObjectOverrides;
	for (const FAnimNotifyEvent& Event : Animation->Notifies)
	{
		for (UObject* Instance : {static_cast<UObject*>(Event.Notify.Get()), static_cast<UObject*>(Event.NotifyStateClass.Get())})
		{
			if (!Instance) continue;
			if (!IsValid(Instance)) return Reject(TEXT("Invalid notify instance"));
			UClass* const* Target = Replacements.Find(Instance->GetClass());
			if (!Target || *Target == Instance->GetClass()) continue;
			if (Instance->GetOuter() != Animation || Instance->HasAnyFlags(RF_Public | RF_Standalone | RF_ClassDefaultObject | RF_ArchetypeObject))
				return Reject(FString::Printf(TEXT("Notify is not a private direct animation-owned instance: %s Outer=%s Flags=%u"),
					*GetPathNameSafe(Instance), *GetPathNameSafe(Instance->GetOuter()), static_cast<uint32>(Instance->GetFlags())));
			TArray<UObject*> Children;
			GetObjectsWithOuter(Instance, Children, EGetObjectsFlags::IncludeNestedObjects);
			for (UObject* Child : Children) { if (Child->IsA<UActorComponent>()) return Reject(FString::Printf(TEXT("Notify owns unsupported component: %s"), *GetPathNameSafe(Child))); }
			for (TFieldIterator<FProperty> Left(Instance->GetClass()), Right(*Target); Left && Right; ++Left, ++Right)
			{
				if (Left->SameType(*Right)) continue;
				ReflectedCopies.Add(Instance);
				if (Left->GetClass() != FObjectProperty::StaticClass()) continue;
				const FObjectProperty* SourceProperty = CastFieldChecked<FObjectProperty>(*Left);
				const FObjectProperty* TargetProperty = CastFieldChecked<FObjectProperty>(*Right);
				if (SourceProperty->ArrayDim != 1 || SourceProperty->HasAnyPropertyFlags(CPF_InstancedReference | CPF_ContainsInstancedReference))
					return Reject(FString::Printf(TEXT("Mapped object constraint is not a scalar external asset reference: %s.%s"), *GetPathNameSafe(Instance), *Left->GetName()));
				UObject* Value = SourceProperty->GetObjectPropertyValue_InContainer(Instance);
				UObject* Replacement = Value ? ObjectReplacements.FindRef(Value) : nullptr;
				if (Value && (!IsValid(Value) || !Value->IsA(SourceProperty->PropertyClass) || !IsValid(Replacement) || !Replacement->IsA(TargetProperty->PropertyClass)))
					return Reject(FString::Printf(TEXT("Missing or incompatible explicit object replacement: %s.%s Value=%s Replacement=%s ExpectedClass=%s"),
						*GetPathNameSafe(Instance), *Left->GetName(), *GetPathNameSafe(Value), *GetPathNameSafe(Replacement), *GetPathNameSafe(TargetProperty->PropertyClass)));
				ObjectOverrides.FindOrAdd(Instance).Add(TargetProperty, Replacement);
			}
			if (ReflectedCopies.Contains(Instance))
			{
				UClass* NativeBase = Instance->GetClass();
				while (!NativeBase->HasAnyClassFlags(CLASS_Native)) { NativeBase = NativeBase->GetSuperClass(); }
				// These plain native bases have no custom serialized instance payload. Different native
				// notify subclasses or instanced hierarchies require a purpose-built conversion instead.
				if (!Children.IsEmpty() || (NativeBase != UAnimNotify::StaticClass() && NativeBase != UAnimNotifyState::StaticClass()))
					return Reject(FString::Printf(TEXT("Mapped schema needs unsupported native payload/subobjects: %s NativeBase=%s Children=%d"), *GetPathNameSafe(Instance), *GetPathNameSafe(NativeBase), Children.Num()));
			}
			Originals.AddUnique(Instance);
		}
	}
	if (Originals.IsEmpty()) return 0;

	// Prepare everything off-asset so incompatible/failed duplication never changes event data or dirtiness.
	TStrongObjectPtr<UObject> Staging(NewObject<UObject>(GetTransientPackage()));
	TMap<UObject*, UObject*> Duplicates;
	for (UObject* Original : Originals)
	{
		UClass* TargetClass = Replacements.FindChecked(Original->GetClass());
		const FName Name = MakeUniqueObjectName(Staging.Get(), TargetClass, Original->GetFName());
		UObject* Duplicate;
		if (ReflectedCopies.Contains(Original))
		{
			Duplicate = NewObject<UObject>(Staging.Get(), TargetClass, Name);
		}
		else
		{
			FObjectDuplicationParameters Parameters(Original, Staging.Get());
			Parameters.DestClass = TargetClass;
			Parameters.DestName = Name;
			Parameters.FlagMask &= ~(RF_Public | RF_Standalone | RF_Transactional);
			Parameters.DuplicationSeed = Types;
			TMap<UObject*, UObject*> Created;
			Parameters.CreatedObjects = &Created;
			Duplicate = StaticDuplicateObjectEx(Parameters);
			Duplicates.Append(Created);
		}
		if (!Duplicate || Duplicate->GetClass() != TargetClass) return Reject(FString::Printf(TEXT("Instance duplication failed: %s -> %s"), *GetPathNameSafe(Original), *GetPathNameSafe(TargetClass)));
		Duplicates.Add(Original, Duplicate);
		// The exact recursive layout contract permits value copying with the destination property's
		// ops, including deliberately remapped enum/struct paths that tagged duplication would skip.
		for (TFieldIterator<FProperty> Property(TargetClass); Property; ++Property)
		{
			if (!Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient))
			{
				const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(*Property);
				const TMap<const FObjectProperty*, UObject*>* Overrides = ObjectOverrides.Find(Original);
				if (ObjectProperty && Overrides && Overrides->Contains(ObjectProperty))
				{
					ObjectProperty->SetObjectPropertyValue_InContainer(Duplicate, Overrides->FindChecked(ObjectProperty));
				}
				else { Property->CopyCompleteValue_InContainer(Duplicate, Original); }
			}
		}
	}
	TMap<UObject*, UObject*> References = Types;
	References.Append(Duplicates);
	for (const TPair<UObject*, UObject*>& Pair : Duplicates)
	{
		FArchiveReplaceObjectRef<UObject> Archive(Pair.Value, References,
			EArchiveReplaceObjectFlags::IgnoreOuterRef | EArchiveReplaceObjectFlags::IgnoreArchetypeRef);
	}
	TArray<FAnimNotifyEvent> Events = Animation->Notifies;
	for (FAnimNotifyEvent& Event : Events)
	{
		if (UObject* Replacement = Duplicates.FindRef(Event.Notify)) Event.Notify = CastChecked<UAnimNotify>(Replacement);
		if (UObject* Replacement = Duplicates.FindRef(Event.NotifyStateClass)) Event.NotifyStateClass = CastChecked<UAnimNotifyState>(Replacement);
	}
	FScopedTransaction Transaction(NSLOCTEXT("RpgBlueprintAssetTools", "RemapAnimationNotifyClasses", "Remap Animation Notify Classes"));
	Animation->SetFlags(RF_Transactional);
	Animation->Modify(false);
	for (UObject* Original : Originals)
	{
		UObject* Duplicate = Duplicates.FindChecked(Original);
		const FName Name = MakeUniqueObjectName(Animation, Duplicate->GetClass(), Original->GetFName());
		Duplicate->Rename(*Name.ToString(), Animation, REN_DontCreateRedirectors | REN_DoNotDirty | REN_NonTransactional);
		Duplicate->SetFlags(RF_Transactional);
	}
	// Keep the event array's storage/order and every metadata field intact. RefreshCacheData would
	// sort events and may move overlapping notifies between tracks. Existing cached event pointers
	// and montage branching-point indices remain valid because only these instance pointers change.
	for (int32 Index = 0; Index < Events.Num(); ++Index)
	{
		Animation->Notifies[Index].Notify = Events[Index].Notify;
		Animation->Notifies[Index].NotifyStateClass = Events[Index].NotifyStateClass;
	}
	Animation->MarkPackageDirty();
	return Originals.Num();
}
