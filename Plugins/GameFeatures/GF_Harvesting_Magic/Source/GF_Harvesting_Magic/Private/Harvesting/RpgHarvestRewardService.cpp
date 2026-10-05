#include "Harvesting/RpgHarvestRewardService.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Harvesting/RpgHarvestRewardProfile.h"
#include "Harvesting/RpgHarvestStockRules.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgGatheringSet.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootResolver.h"
#include "SurvivalRpg/Inventory/RpgDroppedInventoryActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillProgressionComponent.h"

namespace
{
	bool HasPickupContents(const FInventoryPickup& Pickup)
	{
		return !Pickup.Templates.IsEmpty() || !Pickup.Instances.IsEmpty();
	}
}

ARpgPlayerState* FRpgHarvestRewardService::ResolveHarvesterPlayerState(AActor* Harvester)
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

bool FRpgHarvestRewardService::MeetsSkillGate(
	const URpgHarvestRewardProfile* Profile,
	AActor* Harvester)
{
	if (!Profile || !Profile->SkillTag.IsValid())
	{
		return true;
	}

	const ARpgPlayerState* PlayerState = ResolveHarvesterPlayerState(Harvester);
	const URpgTradeSkillProgressionComponent* TradeSkills =
		PlayerState ? PlayerState->GetTradeSkillProgressionComponent() : nullptr;
	return TradeSkills &&
		TradeSkills->GetSkillLevelByTag(Profile->SkillTag) >=
			FMath::Clamp(Profile->MinimumSkillLevel, 1, 100);
}

ERpgHarvestRewardDeliveryResult FRpgHarvestRewardService::DeliverReward(
	const URpgHarvestRewardProfile* Profile,
	const FRpgHarvestRewardRequest& Request)
{
	AActor* SourceActor = Request.SourceActor.Get();
	AActor* Harvester = Request.Harvester.Get();
	UWorld* World = SourceActor ? SourceActor->GetWorld() : nullptr;
	if (!Profile || !Profile->LootTable || !SourceActor || !SourceActor->HasAuthority() || !Harvester || !World ||
		!FMath::IsFinite(Request.HarvestPower) || Request.HarvestPower <= 0.0f || Request.RollCount < 1)
	{
		return ERpgHarvestRewardDeliveryResult::Failed;
	}

	ARpgPlayerState* PlayerState = ResolveHarvesterPlayerState(Harvester);
	URpgTradeSkillProgressionComponent* TradeSkills =
		PlayerState ? PlayerState->GetTradeSkillProgressionComponent() : nullptr;
	const int32 SkillLevel = Profile->SkillTag.IsValid() && TradeSkills
		? TradeSkills->GetSkillLevelByTag(Profile->SkillTag)
		: 0;

	float YieldMultiplier = Profile->SkillTag.IsValid() && TradeSkills
		? TradeSkills->GetSkillYieldMultiplier(Profile->SkillTag)
		: 1.0f;
	float RareFindMultiplier = Profile->SkillTag.IsValid() && TradeSkills
		? TradeSkills->GetSkillRareFindMultiplier(Profile->SkillTag)
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
	LootContext.SourceActor = SourceActor;
	LootContext.RecipientActor = Harvester;
	LootContext.SourceTags = Profile->SourceTags;
	LootContext.SourceLevel = FMath::Max(1, SkillLevel);
	LootContext.SkillId = Profile->SkillTag;
	LootContext.SkillLevel = SkillLevel;
	LootContext.HarvestPower = Request.HarvestPower;
	LootContext.YieldMultiplier = YieldMultiplier;
	LootContext.RareFindMultiplier = RareFindMultiplier;
	const uint64 Entropy = FPlatformTime::Cycles64() ^
		(static_cast<uint64>(GetTypeHash(SourceActor)) << 32) ^
		static_cast<uint32>(Request.SeedSalt);
	LootContext.Seed = static_cast<int32>(Entropy ^ (Entropy >> 32));

	// Every stock section rolls independently; the rows are merged before materialization so the
	// complete multi-section reward is delivered as one atomic batch. The first roll keeps the base seed.
	const int32 BaseSeed = LootContext.Seed;
	FRpgLootRollResult CombinedRoll;
	CombinedRoll.Seed = BaseSeed;
	for (int32 RollIndex = 0; RollIndex < Request.RollCount; ++RollIndex)
	{
		LootContext.Seed = RollIndex == 0
			? BaseSeed
			: static_cast<int32>(HashCombine(static_cast<uint32>(BaseSeed), static_cast<uint32>(RollIndex)));
		FRpgLootRollResult SectionRoll;
		if (!FRpgLootResolver::RollLoot(Profile->LootTable, LootContext, SectionRoll))
		{
			return ERpgHarvestRewardDeliveryResult::Failed;
		}
		CombinedRoll.Items.Append(MoveTemp(SectionRoll.Items));
	}

	FInventoryPickup Reward;
	if (!FRpgLootResolver::MaterializeLoot(SourceActor, CombinedRoll, Reward))
	{
		return ERpgHarvestRewardDeliveryResult::Failed;
	}
	if (!HasPickupContents(Reward))
	{
		return ERpgHarvestRewardDeliveryResult::Empty;
	}

	if (FRpgHarvestRewardBatch* Batch = FRpgHarvestRewardBatch::FindOpen(Harvester))
	{
		// The batch delivers this reward together with the other targets of the same harvest.
		return Batch->Append(Reward, Profile->OverflowDropClass)
			? ERpgHarvestRewardDeliveryResult::Batched
			: ERpgHarvestRewardDeliveryResult::Failed;
	}
	return DeliverPickup(*World, *Harvester, Reward, Request.DeliveryTransform, Profile->OverflowDropClass, true);
}

ERpgHarvestRewardDeliveryResult FRpgHarvestRewardService::DeliverPickup(
	UWorld& World,
	AActor& Harvester,
	const FInventoryPickup& Reward,
	const FTransform& DropTransform,
	TSubclassOf<ARpgDroppedInventoryActor> DropClass,
	const bool bTryInventory)
{
	ARpgPlayerState* PlayerState = ResolveHarvesterPlayerState(&Harvester);
	URpgInventoryManagerComponent* PlayerInventory =
		PlayerState ? PlayerState->GetInventoryManagerComponent() : nullptr;
	if (bTryInventory && PlayerInventory && PlayerInventory->CanAddPickupBatch(Reward))
	{
		TArray<FRpgInventoryItemId> AffectedItemIds;
		const FRpgInventoryMutationResult GrantResult =
			PlayerInventory->AddPickupBatch(Reward, AffectedItemIds);
		if (GrantResult.IsSuccess())
		{
			return ERpgHarvestRewardDeliveryResult::Inventory;
		}
		// A successful preflight followed by a failed commit is a transient mutation failure,
		// not capacity overflow. Keep the target retryable instead of duplicating the roll into a drop.
		return ERpgHarvestRewardDeliveryResult::Failed;
	}

	FTransform SpawnTransform = DropTransform;
	SpawnTransform.AddToTranslation(FVector(0.0, 0.0, 40.0));
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = &Harvester;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	if (!DropClass)
	{
		DropClass = ARpgDroppedInventoryActor::StaticClass();
	}
	ARpgDroppedInventoryActor* Drop = World.SpawnActor<ARpgDroppedInventoryActor>(
		DropClass,
		SpawnTransform,
		SpawnParameters);
	if (!Drop)
	{
		return ERpgHarvestRewardDeliveryResult::Failed;
	}

	if (!Drop->TrySetPickupInventory(Reward))
	{
		// Population is all-or-nothing from the reward service's perspective. Destroy
		// even a partially mutated custom drop so callers cannot award XP or complete
		// the source for a batch that was not materialized in full.
		if (IsValid(Drop))
		{
			Drop->Destroy();
		}
		return ERpgHarvestRewardDeliveryResult::Failed;
	}
	if (!IsValid(Drop) || Drop->IsActorBeingDestroyed())
	{
		return ERpgHarvestRewardDeliveryResult::Failed;
	}
	URpgInventoryManagerComponent* DropInventory = Drop->GetLootInventoryManager();
	if (!Drop->IsLootInventoryCanonical() || !DropInventory || DropInventory->GetUsedEntryCount() <= 0)
	{
		Drop->Destroy();
		return ERpgHarvestRewardDeliveryResult::Failed;
	}
	return ERpgHarvestRewardDeliveryResult::WorldDrop;
}

void FRpgHarvestRewardService::AwardExperience(
	const URpgHarvestRewardProfile* Profile,
	AActor* Harvester,
	const int32 HarvestedUnits)
{
	if (!Profile || !Profile->SkillTag.IsValid() || Profile->SkillExperience <= 0 || HarvestedUnits <= 0)
	{
		return;
	}

	ARpgPlayerState* PlayerState = ResolveHarvesterPlayerState(Harvester);
	if (URpgTradeSkillProgressionComponent* TradeSkills =
			PlayerState ? PlayerState->GetTradeSkillProgressionComponent() : nullptr)
	{
		TradeSkills->AddSkillXPByTag(
			Profile->SkillTag,
			static_cast<float>(Profile->SkillExperience) * static_cast<float>(HarvestedUnits));
	}
}

TArray<FRpgHarvestRewardBatch*> FRpgHarvestRewardBatch::OpenBatches;

FRpgHarvestRewardBatch::FRpgHarvestRewardBatch(AActor* InHarvester, const bool bOpen)
	: Harvester(InHarvester)
	, World(InHarvester ? InHarvester->GetWorld() : nullptr)
{
	check(IsInGameThread());
	if (bOpen)
	{
		Open();
	}
}

bool FRpgHarvestRewardBatch::Open()
{
	check(IsInGameThread());
	AActor* HarvesterActor = Harvester.Get();
	if (!HarvesterActor || IsOpen())
	{
		return IsOpen();
	}
	if (!ensureMsgf(
			!FindOpen(HarvesterActor),
			TEXT("Harvest reward batches may not overlap for %s."),
			*GetNameSafe(HarvesterActor)))
	{
		return false;
	}
	OpenBatches.Add(this);
	return true;
}

void FRpgHarvestRewardBatch::Close()
{
	OpenBatches.Remove(this);
}

bool FRpgHarvestRewardBatch::IsOpen() const
{
	return OpenBatches.Contains(this);
}

void FRpgHarvestRewardBatch::Discard()
{
	Close();
	PendingReward = FInventoryPickup();
}

FRpgHarvestRewardBatch::~FRpgHarvestRewardBatch()
{
	if (HasPendingRewards())
	{
		const AActor* HarvesterActor = Harvester.Get();
		Deliver(HarvesterActor ? HarvesterActor->GetActorTransform() : FTransform::Identity);
	}
	OpenBatches.Remove(this);
}

ERpgHarvestRewardDeliveryResult FRpgHarvestRewardBatch::Deliver(const FTransform& DropTransform)
{
	OpenBatches.Remove(this);
	if (!HasPendingRewards())
	{
		return ERpgHarvestRewardDeliveryResult::Empty;
	}

	const FInventoryPickup Reward = MoveTemp(PendingReward);
	PendingReward = FInventoryPickup();
	AActor* HarvesterActor = Harvester.Get();
	UWorld* HarvestWorld = World.Get();
	ERpgHarvestRewardDeliveryResult Result = ERpgHarvestRewardDeliveryResult::Failed;
	if (HarvesterActor && HarvestWorld)
	{
		Result = FRpgHarvestRewardService::DeliverPickup(
			*HarvestWorld,
			*HarvesterActor,
			Reward,
			DropTransform,
			DropClass,
			true);
		if (Result == ERpgHarvestRewardDeliveryResult::Failed)
		{
			// The targets already extracted their stock, so a failed inventory commit falls back to the drop.
			Result = FRpgHarvestRewardService::DeliverPickup(
				*HarvestWorld,
				*HarvesterActor,
				Reward,
				DropTransform,
				DropClass,
				false);
		}
	}
	if (Result == ERpgHarvestRewardDeliveryResult::Failed)
	{
		UE_LOG(
			LogRpgHarvesting,
			Error,
			TEXT("A batched harvest reward of %d stacks and %d item instances for %s could not be delivered."),
			Reward.Templates.Num(),
			Reward.Instances.Num(),
			*GetNameSafe(HarvesterActor));
	}
	return Result;
}

bool FRpgHarvestRewardBatch::HasPendingRewards() const
{
	return HasPickupContents(PendingReward);
}

FRpgHarvestRewardBatch* FRpgHarvestRewardBatch::FindOpen(const AActor* InHarvester)
{
	if (!InHarvester || !IsInGameThread())
	{
		return nullptr;
	}
	for (FRpgHarvestRewardBatch* Batch : OpenBatches)
	{
		if (Batch->Harvester.Get() == InHarvester)
		{
			return Batch;
		}
	}
	return nullptr;
}

bool FRpgHarvestRewardBatch::Append(const FInventoryPickup& Reward, TSubclassOf<ARpgDroppedInventoryActor> InDropClass)
{
	auto FindStack = [this](const FPickupTemplate& Template)
	{
		return PendingReward.Templates.FindByPredicate([&Template](const FPickupTemplate& Candidate)
		{
			return Candidate.ItemDef == Template.ItemDef;
		});
	};

	// Validate every stack before merging, so a rejected reward leaves the batch unchanged.
	for (const FPickupTemplate& Template : Reward.Templates)
	{
		const FPickupTemplate* Existing = FindStack(Template);
		if (Template.StackCount <= 0 || (Existing && Existing->StackCount > MAX_int32 - Template.StackCount))
		{
			return false;
		}
	}

	for (const FPickupTemplate& Template : Reward.Templates)
	{
		if (FPickupTemplate* Existing = FindStack(Template))
		{
			Existing->StackCount += Template.StackCount;
		}
		else
		{
			PendingReward.Templates.Add(Template);
		}
	}
	PendingReward.Instances.Append(Reward.Instances);
	if (!DropClass)
	{
		DropClass = InDropClass;
	}
	return true;
}
