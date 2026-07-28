#include "Harvesting/RpgHarvestableInstancedMeshComponent.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystemGlobals.h"
#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
#include "Harvesting/RpgHarvestProfile.h"
#include "Misc/ScopeExit.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgGatheringSet.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Interaction/Abilities/RpgGameplayAbility_ExecuteInteraction.h"
#include "SurvivalRpg/Interaction/InteractionOption.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"
#include "SurvivalRpg/Physics/RpgCollisionChannels.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillProgressionComponent.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootResolver.h"
#include "SurvivalRpg/Inventory/RpgDroppedInventoryActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgHarvestableInstancedMeshComponent)

namespace
{
	constexpr int32 DefaultHarvestInteractionPriority = 30;

	ARpgPlayerState* ResolveHarvesterPlayerState(AActor* Harvester)
	{
		if (ARpgPlayerState* PlayerState = Cast<ARpgPlayerState>(Harvester))
		{
			return PlayerState;
		}
		if (const APawn* Pawn = Cast<APawn>(Harvester))
		{
			return Pawn->GetPlayerState<ARpgPlayerState>();
		}
		if (const AController* Controller = Cast<AController>(Harvester))
		{
			return Controller->GetPlayerState<ARpgPlayerState>();
		}
		return nullptr;
	}

	bool HasPickupContents(const FInventoryPickup& Pickup)
	{
		return !Pickup.Templates.IsEmpty() || !Pickup.Instances.IsEmpty();
	}
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

void URpgHarvestableInstancedMeshComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimerHandle);
	}
	RespawnDeadlines.Reset();
	Super::EndPlay(EndPlayReason);
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
	Request.ExpectedRevision = ValidatedOption.TargetRef.Revision;
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
		FMath::IsFinite(Request.HarvestPower) &&
		Request.HarvestPower > 0.0f &&
		Request.AbilityId.MatchesTag(RpgHarvestingMagicGameplayTags::Ability_Harvesting) &&
		Request.Hit.GetActor() == OwningActor &&
		Request.Hit.GetComponent() == this &&
		Request.ExpectedRevision != INDEX_NONE &&
		Request.ExpectedRevision == GetResourceInstanceRevision(Request.Hit.Item) &&
		IsResourceInstanceActive(Request.Hit.Item) &&
		!HarvestsInProgress.Contains(Request.Hit.Item) &&
		CanHarvesterMeetSkillGate(Request);
}

bool URpgHarvestableInstancedMeshComponent::CommitHarvest_Implementation(const FRpgHarvestRequest& Request)
{
	// Re-run the complete validation after any ability cost/commit work. This includes
	// the exact resource revision observed when the authoritative target was selected.
	if (!CanAcceptHarvest_Implementation(Request))
	{
		return false;
	}
	const int32 InstanceIndex = Request.Hit.Item;
	HarvestsInProgress.Add(InstanceIndex);
	ON_SCOPE_EXIT
	{
		HarvestsInProgress.Remove(InstanceIndex);
	};

	// Existing reference nodes without a profile retain their deplete-only behavior.
	// Profile-backed nodes must establish an inventory grant or complete world-drop fallback first.
	if (HarvestProfile && !TryDeliverHarvestReward(Request))
	{
		return false;
	}
	if (!SetResourceInstanceActive(InstanceIndex, false))
	{
		return false;
	}

	AwardHarvestExperience(Request);
	ScheduleResourceRespawn(InstanceIndex);

	OnResourceInstanceHarvested.Broadcast(InstanceIndex, GetResourceInstanceRevision(InstanceIndex), Request);
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

bool URpgHarvestableInstancedMeshComponent::CanHarvesterMeetSkillGate(const FRpgHarvestRequest& Request) const
{
	if (!HarvestProfile || !HarvestProfile->SkillTag.IsValid())
	{
		return true;
	}

	const ARpgPlayerState* PlayerState = ResolveHarvesterPlayerState(Request.Harvester);
	const URpgTradeSkillProgressionComponent* TradeSkills =
		PlayerState ? PlayerState->GetTradeSkillProgressionComponent() : nullptr;
	return TradeSkills &&
		TradeSkills->GetSkillLevelByTag(HarvestProfile->SkillTag) >=
			FMath::Clamp(HarvestProfile->MinimumSkillLevel, 1, 100);
}

bool URpgHarvestableInstancedMeshComponent::TryDeliverHarvestReward(const FRpgHarvestRequest& Request)
{
	AActor* OwningActor = GetOwner();
	UWorld* World = GetWorld();
	if (!HarvestProfile || !HarvestProfile->LootTable || !OwningActor ||
		!OwningActor->HasAuthority() || !World)
	{
		return false;
	}

	ARpgPlayerState* PlayerState = ResolveHarvesterPlayerState(Request.Harvester);
	URpgTradeSkillProgressionComponent* TradeSkills =
		PlayerState ? PlayerState->GetTradeSkillProgressionComponent() : nullptr;
	const int32 SkillLevel = HarvestProfile->SkillTag.IsValid() && TradeSkills
		? TradeSkills->GetSkillLevelByTag(HarvestProfile->SkillTag)
		: 0;

	float YieldMultiplier = HarvestProfile->SkillTag.IsValid() && TradeSkills
		? TradeSkills->GetSkillYieldMultiplier(HarvestProfile->SkillTag)
		: 1.0f;
	float RareFindMultiplier = HarvestProfile->SkillTag.IsValid() && TradeSkills
		? TradeSkills->GetSkillRareFindMultiplier(HarvestProfile->SkillTag)
		: 1.0f;
	if (const UAbilitySystemComponent* AbilitySystem = PlayerState
			? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState)
			: nullptr)
	{
		if (const URpgGatheringSet* GatheringSet = AbilitySystem->GetSet<URpgGatheringSet>())
		{
			YieldMultiplier *= FMath::Max(0.05f, 1.0f + GatheringSet->GetYieldBonus());
			RareFindMultiplier *= FMath::Max(0.05f, 1.0f + GatheringSet->GetRareFindBonus());
		}
	}

	FRpgLootRollContext LootContext;
	LootContext.SourceActor = OwningActor;
	LootContext.RecipientActor = Request.Harvester;
	LootContext.SourceTags = HarvestProfile->SourceTags;
	LootContext.SourceLevel = FMath::Max(1, SkillLevel);
	LootContext.SkillId = HarvestProfile->SkillTag;
	LootContext.SkillLevel = SkillLevel;
	LootContext.HarvestPower = Request.HarvestPower;
	LootContext.YieldMultiplier = YieldMultiplier;
	LootContext.RareFindMultiplier = RareFindMultiplier;
	const uint64 Entropy = FPlatformTime::Cycles64() ^
		(static_cast<uint64>(GetTypeHash(OwningActor)) << 32) ^
		static_cast<uint32>(Request.Hit.Item) ^
		static_cast<uint32>(GetResourceInstanceRevision(Request.Hit.Item));
	LootContext.Seed = static_cast<int32>(Entropy ^ (Entropy >> 32));

	FInventoryPickup Reward;
	if (!FRpgLootResolver::RollAndMaterialize(
		HarvestProfile->LootTable,
		LootContext,
		OwningActor,
		Reward))
	{
		return false;
	}
	if (!HasPickupContents(Reward))
	{
		return true;
	}

	URpgInventoryManagerComponent* PlayerInventory =
		PlayerState ? PlayerState->GetInventoryManagerComponent() : nullptr;
	if (PlayerInventory && PlayerInventory->CanAddPickupBatch(Reward))
	{
		TArray<FRpgInventoryItemId> AffectedItemIds;
		const FRpgInventoryMutationResult GrantResult =
			PlayerInventory->AddPickupBatch(Reward, AffectedItemIds);
		if (GrantResult.IsSuccess())
		{
			return true;
		}
	}

	FTransform DropTransform = FTransform::Identity;
	if (!GetInstanceTransform(Request.Hit.Item, DropTransform, true))
	{
		DropTransform.SetLocation(Request.Hit.ImpactPoint);
	}
	DropTransform.AddToTranslation(FVector(0.0, 0.0, 40.0));
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = Request.Harvester;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	TSubclassOf<ARpgDroppedInventoryActor> DropClass = HarvestProfile->OverflowDropClass;
	if (!DropClass)
	{
		DropClass = ARpgDroppedInventoryActor::StaticClass();
	}
	ARpgDroppedInventoryActor* Drop = World->SpawnActor<ARpgDroppedInventoryActor>(
		DropClass,
		DropTransform,
		SpawnParameters);
	if (!Drop)
	{
		return false;
	}

	Drop->SetPickupInventory(Reward);
	URpgInventoryManagerComponent* DropInventory = Drop->GetLootInventoryManager();
	if (!Drop->IsLootInventoryCanonical() || !DropInventory || DropInventory->GetUsedEntryCount() <= 0)
	{
		Drop->Destroy();
		return false;
	}
	return true;
}

void URpgHarvestableInstancedMeshComponent::AwardHarvestExperience(const FRpgHarvestRequest& Request) const
{
	if (!HarvestProfile || !HarvestProfile->SkillTag.IsValid() || HarvestProfile->SkillExperience <= 0)
	{
		return;
	}

	ARpgPlayerState* PlayerState = ResolveHarvesterPlayerState(Request.Harvester);
	if (URpgTradeSkillProgressionComponent* TradeSkills =
			PlayerState ? PlayerState->GetTradeSkillProgressionComponent() : nullptr)
	{
		TradeSkills->AddSkillXPByTag(HarvestProfile->SkillTag, HarvestProfile->SkillExperience);
	}
}

void URpgHarvestableInstancedMeshComponent::ScheduleResourceRespawn(const int32 InstanceIndex)
{
	AActor* OwningActor = GetOwner();
	UWorld* World = GetWorld();
	if (!HarvestProfile || !OwningActor || !OwningActor->HasAuthority() || !World)
	{
		return;
	}

	const float MinimumDelay = FMath::Max(0.0f, HarvestProfile->MinimumRespawnSeconds);
	const float MaximumDelay = FMath::Max(MinimumDelay, HarvestProfile->MaximumRespawnSeconds);
	if (MaximumDelay <= 0.0f)
	{
		return;
	}

	const double Delay = FMath::FRandRange(MinimumDelay, MaximumDelay);
	RespawnDeadlines.Add(InstanceIndex, World->GetTimeSeconds() + Delay);
	ArmNextRespawnTimer();
}

void URpgHarvestableInstancedMeshComponent::ArmNextRespawnTimer()
{
	UWorld* World = GetWorld();
	if (!World || RespawnDeadlines.IsEmpty())
	{
		return;
	}

	double EarliestDeadline = TNumericLimits<double>::Max();
	for (const TPair<int32, double>& Pair : RespawnDeadlines)
	{
		EarliestDeadline = FMath::Min(EarliestDeadline, Pair.Value);
	}

	const float Delay = static_cast<float>(FMath::Max(0.001, EarliestDeadline - World->GetTimeSeconds()));
	World->GetTimerManager().SetTimer(
		RespawnTimerHandle,
		this,
		&ThisClass::HandleRespawnTimer,
		Delay,
		false);
}

void URpgHarvestableInstancedMeshComponent::HandleRespawnTimer()
{
	AActor* OwningActor = GetOwner();
	UWorld* World = GetWorld();
	if (!OwningActor || !OwningActor->HasAuthority() || !World)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	TArray<int32> DueInstances;
	for (const TPair<int32, double>& Pair : RespawnDeadlines)
	{
		if (Pair.Value <= Now + UE_KINDA_SMALL_NUMBER)
		{
			DueInstances.Add(Pair.Key);
		}
	}

	for (const int32 InstanceIndex : DueInstances)
	{
		RespawnDeadlines.Remove(InstanceIndex);
		if (!IsResourceInstanceActive(InstanceIndex))
		{
			SetResourceInstanceActive(InstanceIndex, true);
		}
	}
	ArmNextRespawnTimer();
}
