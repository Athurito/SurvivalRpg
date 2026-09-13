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
#include "UObject/UObjectHash.h"
#include "UObject/UObjectGlobals.h"

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
