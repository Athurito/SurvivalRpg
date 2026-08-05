#include "Portals/RpgPortalStorageProgressionHook.h"

#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Misc/Crc.h"
#include "SurvivalRpg/Base/RpgWorldStorageKnowledgeComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameStateBase.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootResolver.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootTable.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "Portals/RpgPortalEncounterDefinition.h"
#include "Portals/RpgPortalMessages.h"

DEFINE_LOG_CATEGORY_STATIC(LogRpgPortalStorageProgression, Log, All);

namespace RpgPortalStorageProgression
{
	ARpgPlayerState* ResolveRecipientPlayerState(AActor* Recipient)
	{
		if (ARpgPlayerState* PlayerState = Cast<ARpgPlayerState>(Recipient))
		{
			return PlayerState;
		}
		if (const APawn* Pawn = Cast<APawn>(Recipient))
		{
			return Pawn->GetPlayerState<ARpgPlayerState>();
		}
		if (const AController* Controller = Cast<AController>(Recipient))
		{
			return Controller->GetPlayerState<ARpgPlayerState>();
		}
		return nullptr;
	}

	bool HasPickupContents(const FInventoryPickup& Pickup)
	{
		return !Pickup.Templates.IsEmpty() || !Pickup.Instances.IsEmpty();
	}

	bool DeliverReward(
		UWorld* World,
		ARpgGameStateBase* GameState,
		const FRpgPortalCompletedMessage& Message,
		const URpgLootTable* RewardTable)
	{
		AActor* Recipient = Message.Instigator.Get();
		ARpgPlayerState* PlayerState = ResolveRecipientPlayerState(Recipient);
		URpgInventoryManagerComponent* Inventory = PlayerState
			? PlayerState->GetInventoryManagerComponent()
			: nullptr;
		AActor* SourceActor = Message.Portal ? Message.Portal.Get() : GameState;
		if (!World || !GameState || !GameState->HasAuthority() ||
			!SourceActor || !SourceActor->HasAuthority() || !Recipient ||
			!PlayerState || !Inventory)
		{
			return false;
		}

		FRpgLootRollContext LootContext;
		LootContext.SourceActor = SourceActor;
		LootContext.RecipientActor = Recipient;
		LootContext.SourceTags = Message.CompletionTags;
		LootContext.SourceLevel = 1;
		LootContext.HarvestPower = 1.0f;
		LootContext.YieldMultiplier = 1.0f;
		LootContext.RareFindMultiplier = 1.0f;
		const FString SeedKey = FString::Printf(
			TEXT("%s|Storage.Knowledge.RiftContainment"),
			*GetPathNameSafe(Message.EncounterDefinition.Get()));
		LootContext.Seed = static_cast<int32>(FCrc::StrCrc32(*SeedKey));

		FInventoryPickup Reward;
		if (!FRpgLootResolver::RollAndMaterialize(
				RewardTable,
				LootContext,
				SourceActor,
				Reward) ||
			!HasPickupContents(Reward))
		{
			return false;
		}

		// The first-knowledge reward and the knowledge tag are one durable
		// transaction. Ordinary dropped-inventory actors are intentionally not
		// accepted as delivery because they have no stable world-save identity;
		// a full inventory therefore rolls the grant back and a later eligible
		// completion can retry without losing or duplicating the reward.
		if (!Inventory->CanAddPickupBatch(Reward))
		{
			return false;
		}

		TArray<FRpgInventoryItemId> AffectedItemIds;
		return Inventory->AddPickupBatch(Reward, AffectedItemIds).IsSuccess();
	}
}

bool FRpgPortalStorageProgressionHook::IsDeterministicGuaranteedRewardTable(
	const URpgLootTable* RewardTable,
	FString& OutError)
{
	OutError.Reset();
	if (!RewardTable || !RewardTable->HasValidConfiguration(&OutError) ||
		RewardTable->Groups.IsEmpty())
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Reward table is null or has no groups.");
		}
		return false;
	}

	for (const FRpgLootGroup& Group : RewardTable->Groups)
	{
		if (Group.Mode != ERpgLootGroupMode::Independent ||
			Group.GroupChancePercent != 100.0f ||
			Group.Entries.IsEmpty())
		{
			OutError = TEXT("Every first-knowledge reward group must be non-empty, Independent, and run at 100 percent.");
			return false;
		}

		for (const FRpgLootEntry& Entry : Group.Entries)
		{
			if (!Entry.ItemDefinition || Entry.ChancePercent != 100.0f ||
				Entry.MinimumQuantity <= 0 ||
				Entry.MinimumQuantity != Entry.MaximumQuantity ||
				Entry.bScaleChanceWithRareFind ||
				Entry.bScaleQuantityWithYield)
			{
				OutError = TEXT("Every first-knowledge reward row must be an exact fixed quantity at 100 percent without runtime scaling.");
				return false;
			}
		}
	}
	return true;
}

bool FRpgPortalStorageProgressionHook::ApplyFirstEligibleCompletion(
	UWorld* World,
	FRpgPortalCompletedMessage& InOutMessage)
{
	InOutMessage.NewlyGrantedWorldKnowledgeTags.Reset();
	InOutMessage.FirstEligibleKnowledgeRewardTable.Reset();

	if (!World || !InOutMessage.bRewardsEligible)
	{
		return false;
	}

	ARpgGameStateBase* GameState = World->GetGameState<ARpgGameStateBase>();
	URpgWorldStorageKnowledgeComponent* KnowledgeComponent = GameState
		? GameState->GetWorldStorageKnowledgeComponent()
		: nullptr;
	if (!GameState || !GameState->HasAuthority() || !KnowledgeComponent ||
		KnowledgeComponent->HasKnowledgeTag(
			RpgGameplayTags::Storage_Knowledge_RiftContainment) ||
		!InOutMessage.EncounterDefinition ||
		InOutMessage.EncounterDefinition->FirstEligibleKnowledgeRewardTable.IsNull())
	{
		return false;
	}

	const URpgLootTable* RewardTable =
		InOutMessage.EncounterDefinition->FirstEligibleKnowledgeRewardTable.LoadSynchronous();
	FString ValidationError;
	if (!IsDeterministicGuaranteedRewardTable(
			RewardTable,
			ValidationError))
	{
		UE_LOG(
			LogRpgPortalStorageProgression,
			Error,
			TEXT("Portal encounter [%s] cannot unlock Rift containment: %s"),
			*GetNameSafe(InOutMessage.EncounterDefinition.Get()),
			*ValidationError);
		return false;
	}

	const FRpgWorldStorageKnowledgeSaveData KnowledgeCheckpoint =
		KnowledgeComponent->ExportSaveData();
	if (!KnowledgeComponent->GrantKnowledgeTag(
			RpgGameplayTags::Storage_Knowledge_RiftContainment))
	{
		UE_LOG(
			LogRpgPortalStorageProgression,
			Error,
			TEXT("Portal encounter [%s] could not atomically stage Rift-containment knowledge."),
			*GetNameSafe(InOutMessage.EncounterDefinition.Get()));
		return false;
	}

	if (!RpgPortalStorageProgression::DeliverReward(
			World,
			GameState,
			InOutMessage,
			RewardTable))
	{
		const bool bKnowledgeRolledBack =
			KnowledgeComponent->ImportSaveData(KnowledgeCheckpoint);
		if (bKnowledgeRolledBack)
		{
			UE_LOG(
				LogRpgPortalStorageProgression,
				Warning,
				TEXT("Portal encounter [%s] kept Rift containment locked because its guaranteed gate reward could not be delivered without loss. Knowledge rollback succeeded."),
				*GetNameSafe(InOutMessage.EncounterDefinition.Get()));
		}
		else
		{
			UE_LOG(
				LogRpgPortalStorageProgression,
				Error,
				TEXT("Portal encounter [%s] failed guaranteed reward delivery AND knowledge rollback."),
				*GetNameSafe(InOutMessage.EncounterDefinition.Get()));
		}
		return false;
	}

	InOutMessage.NewlyGrantedWorldKnowledgeTags.AddTag(
		RpgGameplayTags::Storage_Knowledge_RiftContainment);
	InOutMessage.FirstEligibleKnowledgeRewardTable =
		InOutMessage.EncounterDefinition->FirstEligibleKnowledgeRewardTable;
	return true;
}
