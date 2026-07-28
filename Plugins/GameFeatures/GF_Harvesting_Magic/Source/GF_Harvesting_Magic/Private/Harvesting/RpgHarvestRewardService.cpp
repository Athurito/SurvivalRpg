#include "Harvesting/RpgHarvestRewardService.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Harvesting/RpgHarvestRewardProfile.h"
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
		!FMath::IsFinite(Request.HarvestPower) || Request.HarvestPower <= 0.0f)
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

	FInventoryPickup Reward;
	if (!FRpgLootResolver::RollAndMaterialize(Profile->LootTable, LootContext, SourceActor, Reward))
	{
		return ERpgHarvestRewardDeliveryResult::Failed;
	}
	if (!HasPickupContents(Reward))
	{
		return ERpgHarvestRewardDeliveryResult::Empty;
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
			return ERpgHarvestRewardDeliveryResult::Inventory;
		}
		// A successful preflight followed by a failed commit is a transient mutation failure,
		// not capacity overflow. Keep the target retryable instead of duplicating the roll into a drop.
		return ERpgHarvestRewardDeliveryResult::Failed;
	}

	FTransform DropTransform = Request.DeliveryTransform;
	DropTransform.AddToTranslation(FVector(0.0, 0.0, 40.0));
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = Harvester;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	TSubclassOf<ARpgDroppedInventoryActor> DropClass = Profile->OverflowDropClass;
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
	AActor* Harvester)
{
	if (!Profile || !Profile->SkillTag.IsValid() || Profile->SkillExperience <= 0)
	{
		return;
	}

	ARpgPlayerState* PlayerState = ResolveHarvesterPlayerState(Harvester);
	if (URpgTradeSkillProgressionComponent* TradeSkills =
			PlayerState ? PlayerState->GetTradeSkillProgressionComponent() : nullptr)
	{
		TradeSkills->AddSkillXPByTag(Profile->SkillTag, Profile->SkillExperience);
	}
}
