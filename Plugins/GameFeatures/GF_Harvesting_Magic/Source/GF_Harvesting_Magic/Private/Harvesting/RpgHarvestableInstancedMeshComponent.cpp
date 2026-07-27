#include "Harvesting/RpgHarvestableInstancedMeshComponent.h"

#include "GameFramework/Actor.h"
#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/Interaction/Abilities/RpgGameplayAbility_ExecuteInteraction.h"
#include "SurvivalRpg/Interaction/InteractionOption.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"
#include "SurvivalRpg/Physics/RpgCollisionChannels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgHarvestableInstancedMeshComponent)

namespace
{
	constexpr int32 DefaultHarvestInteractionPriority = 30;
}

const FRpgHarvestedInstanceStateEntry* FRpgHarvestedInstanceStateList::Find(const int32 InstanceIndex) const
{
	return Entries.FindByPredicate([InstanceIndex](const FRpgHarvestedInstanceStateEntry& Entry)
	{
		return Entry.InstanceIndex == InstanceIndex;
	});
}

FRpgHarvestedInstanceStateEntry* FRpgHarvestedInstanceStateList::FindMutable(const int32 InstanceIndex)
{
	return Entries.FindByPredicate([InstanceIndex](const FRpgHarvestedInstanceStateEntry& Entry)
	{
		return Entry.InstanceIndex == InstanceIndex;
	});
}

void FRpgHarvestedInstanceStateList::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, const int32 FinalSize)
{
	if (!OwnerComponent)
	{
		return;
	}

	for (const int32 ArrayIndex : RemovedIndices)
	{
		if (Entries.IsValidIndex(ArrayIndex))
		{
			OwnerComponent->ApplyReplicatedInstanceState(Entries[ArrayIndex].InstanceIndex, true, 0);
		}
	}
}

void FRpgHarvestedInstanceStateList::PostReplicatedAdd(const TArrayView<int32> AddedIndices, const int32 FinalSize)
{
	if (!OwnerComponent)
	{
		return;
	}

	for (const int32 ArrayIndex : AddedIndices)
	{
		if (Entries.IsValidIndex(ArrayIndex))
		{
			const FRpgHarvestedInstanceStateEntry& Entry = Entries[ArrayIndex];
			OwnerComponent->ApplyReplicatedInstanceState(Entry.InstanceIndex, Entry.bActive, Entry.Revision);
		}
	}
}

void FRpgHarvestedInstanceStateList::PostReplicatedChange(const TArrayView<int32> ChangedIndices, const int32 FinalSize)
{
	if (!OwnerComponent)
	{
		return;
	}

	for (const int32 ArrayIndex : ChangedIndices)
	{
		if (Entries.IsValidIndex(ArrayIndex))
		{
			const FRpgHarvestedInstanceStateEntry& Entry = Entries[ArrayIndex];
			OwnerComponent->ApplyReplicatedInstanceState(Entry.InstanceIndex, Entry.bActive, Entry.Revision);
		}
	}
}

URpgHarvestableInstancedMeshComponent::URpgHarvestableInstancedMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ReplicatedInstanceStates(this)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SetCollisionResponseToChannel(Rpg_TraceChannel_Interaction, ECR_Block);

	InteractionPrompt.ActionText = NSLOCTEXT("RpgHarvestingMagic", "ManualHarvestAction", "Harvest");
	InteractionPrompt.TargetText = NSLOCTEXT("RpgHarvestingMagic", "ResourceInstanceTarget", "Resource");
	InteractionPrompt.AwarenessRange = 800.0f;
	InteractionPrompt.FocusRange = 500.0f;
	InteractionPrompt.InteractionRange = 350.0f;
	InteractionPrompt.InteractionPriority = DefaultHarvestInteractionPriority;
	InteractionPrompt.bShowNearbyIndicator = true;
	InteractionPrompt.bRequiresLineOfSight = true;
}

int32 URpgHarvestableInstancedMeshComponent::AddInstance(const FTransform& InstanceTransform, const bool bWorldSpace)
{
	return CanMutateInstanceTopology(TEXT("AddInstance"))
		? Super::AddInstance(InstanceTransform, bWorldSpace)
		: INDEX_NONE;
}

TArray<int32> URpgHarvestableInstancedMeshComponent::AddInstances(
	const TArray<FTransform>& InstanceTransforms,
	const bool bShouldReturnIndices,
	const bool bWorldSpace,
	const bool bUpdateNavigation)
{
	return CanMutateInstanceTopology(TEXT("AddInstances"))
		? Super::AddInstances(InstanceTransforms, bShouldReturnIndices, bWorldSpace, bUpdateNavigation)
		: TArray<int32>();
}

bool URpgHarvestableInstancedMeshComponent::RemoveInstance(const int32 InstanceIndex)
{
	return CanMutateInstanceTopology(TEXT("RemoveInstance")) && Super::RemoveInstance(InstanceIndex);
}

bool URpgHarvestableInstancedMeshComponent::RemoveInstances(const TArray<int32>& InstancesToRemove)
{
	return CanMutateInstanceTopology(TEXT("RemoveInstances")) && Super::RemoveInstances(InstancesToRemove);
}

bool URpgHarvestableInstancedMeshComponent::RemoveInstances(
	const TArray<int32>& InstancesToRemove,
	const bool bInstanceArrayAlreadySortedInReverseOrder)
{
	return CanMutateInstanceTopology(TEXT("RemoveInstances")) &&
		Super::RemoveInstances(InstancesToRemove, bInstanceArrayAlreadySortedInReverseOrder);
}

void URpgHarvestableInstancedMeshComponent::RemoveInstancesById(
	const TArrayView<const FPrimitiveInstanceId>& InstanceIds,
	const bool bUpdateNavigation)
{
	if (CanMutateInstanceTopology(TEXT("RemoveInstancesById")))
	{
		Super::RemoveInstancesById(InstanceIds, bUpdateNavigation);
	}
}

void URpgHarvestableInstancedMeshComponent::ClearInstances()
{
	if (CanMutateInstanceTopology(TEXT("ClearInstances")))
	{
		Super::ClearInstances();
	}
}

void URpgHarvestableInstancedMeshComponent::OnRegister()
{
	Super::OnRegister();
	ReplicatedInstanceStates.OwnerComponent = this;
}

void URpgHarvestableInstancedMeshComponent::BeginPlay()
{
	Super::BeginPlay();

	ReplicatedInstanceStates.OwnerComponent = this;
	CacheAuthoredInstanceTransforms();
	ApplyAllReplicatedInstanceStates();
}

void URpgHarvestableInstancedMeshComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, ReplicatedInstanceStates);
}

void URpgHarvestableInstancedMeshComponent::GatherInteractionOptions(
	const FInteractionQuery& InteractQuery,
	FInteractionOptionBuilder& OptionBuilder)
{
	if (InteractQuery.QueryMode == ERpgInteractionQueryMode::Nearby)
	{
		const float QueryRadius = InteractQuery.QueryRadius > 0.0f
			? FMath::Min(InteractQuery.QueryRadius, FMath::Max(0.0f, InteractionPrompt.AwarenessRange))
			: FMath::Max(0.0f, InteractionPrompt.AwarenessRange);
		if (QueryRadius <= 0.0f)
		{
			return;
		}

		const TArray<int32> InstanceIndices = GetInstancesOverlappingSphere(InteractQuery.QueryOrigin, QueryRadius, true);
		for (const int32 InstanceIndex : InstanceIndices)
		{
			FInteractionOption Option;
			if (BuildInteractionOption(InstanceIndex, InteractQuery, Option))
			{
				OptionBuilder.AddInteractionOption(Option);
			}
		}
		return;
	}

	if (InteractQuery.CandidateHit.GetComponent() != this)
	{
		return;
	}

	FInteractionOption Option;
	if (BuildInteractionOption(InteractQuery.CandidateHit.Item, InteractQuery, Option))
	{
		OptionBuilder.AddInteractionOption(Option);
	}
}

bool URpgHarvestableInstancedMeshComponent::CommitInteraction(
	const FInteractionQuery& AuthoritativeQuery,
	const FInteractionOption& ValidatedOption)
{
	AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority() ||
		AuthoritativeQuery.QueryMode != ERpgInteractionQueryMode::AuthorityValidation ||
		ValidatedOption.InteractionTag != RpgHarvestingMagicGameplayTags::Rpg_Interaction_Action_Harvest_Manual ||
		ValidatedOption.TargetRef.TargetActor.Get() != OwningActor ||
		ValidatedOption.TargetRef.TargetComponent.Get() != this ||
		AuthoritativeQuery.CandidateHit.GetComponent() != this ||
		AuthoritativeQuery.CandidateHit.Item != ValidatedOption.TargetRef.InstanceIndex ||
		ValidatedOption.TargetRef.Revision != GetResourceInstanceRevision(ValidatedOption.TargetRef.InstanceIndex))
	{
		return false;
	}

	FRpgHarvestRequest Request;
	Request.Harvester = AuthoritativeQuery.RequestingAvatar.Get();
	Request.AbilityId = RpgHarvestingMagicGameplayTags::Ability_Harvesting_Manual;
	Request.TraceOrigin = AuthoritativeQuery.QueryOrigin;
	Request.Hit = AuthoritativeQuery.CandidateHit;
	Request.HarvestPower = 1.0f;

	return IRpgHarvestableTarget::Execute_CanAcceptHarvest(this, Request) &&
		IRpgHarvestableTarget::Execute_CommitHarvest(this, Request);
}

bool URpgHarvestableInstancedMeshComponent::CanAcceptHarvest_Implementation(const FRpgHarvestRequest& Request) const
{
	const AActor* OwningActor = GetOwner();
	return OwningActor &&
		OwningActor->HasAuthority() &&
		Request.Harvester != nullptr &&
		Request.AbilityId.MatchesTag(RpgHarvestingMagicGameplayTags::Ability_Harvesting) &&
		Request.Hit.GetActor() == OwningActor &&
		Request.Hit.GetComponent() == this &&
		IsResourceInstanceActive(Request.Hit.Item);
}

bool URpgHarvestableInstancedMeshComponent::CommitHarvest_Implementation(const FRpgHarvestRequest& Request)
{
	if (!CanAcceptHarvest_Implementation(Request) || !SetResourceInstanceActive(Request.Hit.Item, false))
	{
		return false;
	}

	OnResourceInstanceHarvested.Broadcast(Request.Hit.Item, GetResourceInstanceRevision(Request.Hit.Item), Request);
	return true;
}

bool URpgHarvestableInstancedMeshComponent::IsResourceInstanceActive(const int32 InstanceIndex) const
{
	if (!IsValidResourceInstanceIndex(InstanceIndex))
	{
		return false;
	}

	if (const FRpgHarvestedInstanceStateEntry* Entry = ReplicatedInstanceStates.Find(InstanceIndex))
	{
		return Entry->bActive;
	}
	return true;
}

int32 URpgHarvestableInstancedMeshComponent::GetResourceInstanceRevision(const int32 InstanceIndex) const
{
	if (!IsValidResourceInstanceIndex(InstanceIndex))
	{
		return INDEX_NONE;
	}

	if (const FRpgHarvestedInstanceStateEntry* Entry = ReplicatedInstanceStates.Find(InstanceIndex))
	{
		return Entry->Revision;
	}
	return 0;
}

bool URpgHarvestableInstancedMeshComponent::SetResourceInstanceActive(const int32 InstanceIndex, const bool bNewActive)
{
	AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority() || !IsValidResourceInstanceIndex(InstanceIndex) ||
		IsResourceInstanceActive(InstanceIndex) == bNewActive)
	{
		return false;
	}

	FRpgHarvestedInstanceStateEntry* Entry = ReplicatedInstanceStates.FindMutable(InstanceIndex);
	if (!Entry)
	{
		Entry = &ReplicatedInstanceStates.Entries.AddDefaulted_GetRef();
		Entry->InstanceIndex = InstanceIndex;
	}

	Entry->bActive = bNewActive;
	Entry->Revision = FMath::Max(1, Entry->Revision + 1);
	ReplicatedInstanceStates.MarkItemDirty(*Entry);

	ApplyReplicatedInstanceState(InstanceIndex, bNewActive, Entry->Revision);
	OwningActor->FlushNetDormancy();
	OwningActor->ForceNetUpdate();
	return true;
}

bool URpgHarvestableInstancedMeshComponent::IsValidResourceInstanceIndex(const int32 InstanceIndex) const
{
	return InstanceIndex >= 0 && InstanceIndex < GetInstanceCount();
}

bool URpgHarvestableInstancedMeshComponent::CanMutateInstanceTopology(const TCHAR* OperationName) const
{
	if (!HasBegunPlay())
	{
		return true;
	}

	ensureMsgf(
		false,
		TEXT("%s rejected %s after BeginPlay. Harvestable HISM indices are stable runtime identities; use SetResourceInstanceActive instead."),
		*GetPathName(),
		OperationName);
	return false;
}

bool URpgHarvestableInstancedMeshComponent::BuildInteractionOption(
	const int32 InstanceIndex,
	const FInteractionQuery& InteractQuery,
	FInteractionOption& OutOption) const
{
	if (!IsResourceInstanceActive(InstanceIndex))
	{
		return false;
	}

	FTransform InstanceWorldTransform;
	if (!GetInstanceTransform(InstanceIndex, InstanceWorldTransform, true))
	{
		return false;
	}

	OutOption.InteractionTag = RpgHarvestingMagicGameplayTags::Rpg_Interaction_Action_Harvest_Manual;
	OutOption.TargetRef.TargetActor = GetOwner();
	OutOption.TargetRef.TargetComponent = const_cast<URpgHarvestableInstancedMeshComponent*>(this);
	OutOption.TargetRef.InstanceIndex = InstanceIndex;
	OutOption.TargetRef.WorldLocation = InstanceWorldTransform.GetLocation();
	OutOption.TargetRef.WorldNormal = InteractQuery.CandidateHit.GetComponent() == this &&
		InteractQuery.CandidateHit.Item == InstanceIndex &&
		!InteractQuery.CandidateHit.ImpactNormal.IsNearlyZero()
			? FVector(InteractQuery.CandidateHit.ImpactNormal)
			: InstanceWorldTransform.GetUnitAxis(EAxis::Z);
	OutOption.TargetRef.Revision = GetResourceInstanceRevision(InstanceIndex);
	OutOption.Prompt = InteractionPrompt;
	OutOption.Prompt.SanitizeRanges();
	OutOption.Availability = ERpgInteractionAvailability::Available;
	OutOption.InteractionAbilityToGrant = URpgGameplayAbility_ExecuteInteraction::StaticClass();
	return true;
}

void URpgHarvestableInstancedMeshComponent::CacheAuthoredInstanceTransforms()
{
	const int32 InstanceCount = GetInstanceCount();
	if (AuthoredInstanceTransforms.Num() == InstanceCount)
	{
		return;
	}

	ensureMsgf(
		AuthoredInstanceTransforms.IsEmpty(),
		TEXT("%s changed HISM topology after authored transforms were cached. Runtime AddInstance/RemoveInstance is unsupported because interaction indices must remain stable."),
		*GetPathName());

	AuthoredInstanceTransforms.SetNum(InstanceCount);
	for (int32 InstanceIndex = 0; InstanceIndex < InstanceCount; ++InstanceIndex)
	{
		GetInstanceTransform(InstanceIndex, AuthoredInstanceTransforms[InstanceIndex], false);
	}
}

void URpgHarvestableInstancedMeshComponent::ApplyAllReplicatedInstanceStates()
{
	for (const FRpgHarvestedInstanceStateEntry& Entry : ReplicatedInstanceStates.Entries)
	{
		ApplyReplicatedInstanceState(Entry.InstanceIndex, Entry.bActive, Entry.Revision);
	}
}

void URpgHarvestableInstancedMeshComponent::ApplyReplicatedInstanceState(
	const int32 InstanceIndex,
	const bool bInstanceActive,
	const int32 Revision)
{
	CacheAuthoredInstanceTransforms();
	if (!AuthoredInstanceTransforms.IsValidIndex(InstanceIndex))
	{
		return;
	}

	FTransform InstanceTransform = AuthoredInstanceTransforms[InstanceIndex];
	if (!bInstanceActive)
	{
		InstanceTransform.SetScale3D(FVector::ZeroVector);
	}

	UpdateInstanceTransform(InstanceIndex, InstanceTransform, false, true, true);
	OnResourceInstanceStateChanged.Broadcast(InstanceIndex, bInstanceActive, Revision);
}
