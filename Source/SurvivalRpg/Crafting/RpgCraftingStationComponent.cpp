#include "RpgCraftingStationComponent.h"

#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "SurvivalRpg/Base/RpgBaseCampActor.h"
#include "SurvivalRpg/Base/RpgBaseStorageComponent.h"
#include "SurvivalRpg/Base/RpgBaseStorageStationComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgCraftingStationComponent)

URpgCraftingStationComponent::URpgCraftingStationComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URpgCraftingStationComponent::BeginPlay()
{
	Super::BeginPlay();

	if (OutputInventoryComponent)
	{
		SetOutputInventoryManager(OutputInventoryComponent);
	}
}

namespace
{
	const URpgInventoryFragment_ItemTraits* GetItemTraitsForDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		const URpgInventoryItemDefinition* ItemCDO = ItemDefinition ? GetDefault<URpgInventoryItemDefinition>(ItemDefinition) : nullptr;
		return ItemCDO ? Cast<URpgInventoryFragment_ItemTraits>(ItemCDO->FindFragmentByClass(URpgInventoryFragment_ItemTraits::StaticClass())) : nullptr;
	}

	bool IsMaterialDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		const URpgInventoryFragment_ItemTraits* Traits = GetItemTraitsForDefinition(ItemDefinition);
		return Traits && Traits->IsMaterial();
	}
}

TArray<URpgInventoryManagerComponent*> URpgCraftingStationComponent::GetResourceInventories(AActor* RequestingActor) const
{
	TArray<URpgInventoryManagerComponent*> Results;

	if (RequestingActor == nullptr)
	{
		return Results;
	}

	if (URpgInventoryManagerComponent* DirectInventory = RequestingActor->FindComponentByClass<URpgInventoryManagerComponent>())
	{
		Results.AddUnique(DirectInventory);
	}

	if (const APawn* RequestingPawn = Cast<APawn>(RequestingActor))
	{
		if (APlayerState* PlayerState = RequestingPawn->GetPlayerState())
		{
			if (URpgInventoryManagerComponent* PlayerInventory = PlayerState->FindComponentByClass<URpgInventoryManagerComponent>())
			{
				Results.AddUnique(PlayerInventory);
			}
		}

		if (AController* Controller = RequestingPawn->GetController())
		{
			if (APlayerState* PlayerState = Controller->PlayerState)
			{
				if (URpgInventoryManagerComponent* PlayerInventory = PlayerState->FindComponentByClass<URpgInventoryManagerComponent>())
				{
					Results.AddUnique(PlayerInventory);
				}
			}
		}
	}

	if (!bUseNearbyCraftingContainers)
	{
		return Results;
	}

	const AActor* StationOwner = GetOwner();
	const UWorld* World = GetWorld();
	if (!StationOwner || !World)
	{
		return Results;
	}

	const float SearchRadiusSq = FMath::Square(StorageSearchRadius);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* CandidateActor = *It;
		if (!CandidateActor || CandidateActor == StationOwner)
		{
			continue;
		}

		URpgInventoryContainerComponent* Container = CandidateActor->FindComponentByClass<URpgInventoryContainerComponent>();
		URpgInventoryManagerComponent* Inventory = CandidateActor->FindComponentByClass<URpgInventoryManagerComponent>();
		if (!Container || !Inventory || !Container->AllowsCraftingAccess() || !Container->IsContainerAccessible())
		{
			continue;
		}

		const bool bSameStorageGroup = !StorageGroupId.IsNone() && Container->GetStorageGroupId() == StorageGroupId;
		const bool bWithinRadius = StorageSearchRadius <= 0.0f ||
			FVector::DistSquared(StationOwner->GetActorLocation(), CandidateActor->GetActorLocation()) <= SearchRadiusSq;

		if (bSameStorageGroup || bWithinRadius)
		{
			Results.AddUnique(Inventory);
		}
	}

	return Results;
}

int32 URpgCraftingStationComponent::GetAvailableResourceCount(AActor* RequestingActor, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	if (!ItemDefinition)
	{
		return 0;
	}

	int32 TotalCount = 0;
	const TArray<URpgInventoryManagerComponent*> ResourceInventories = GetResourceInventories(RequestingActor);
	if (ResourceConsumeOrder != ERpgCraftingResourceConsumeOrder::BaseOnly)
	{
		TotalCount += GetAvailableInventoryResourceCount(ItemDefinition, ResourceInventories);
	}

	if (ResourceConsumeOrder != ERpgCraftingResourceConsumeOrder::PlayerOnly)
	{
		if (const URpgBaseStorageComponent* BaseStorage = GetLinkedBaseStorage())
		{
			TotalCount += BaseStorage->GetResourceCount(ItemDefinition);
		}
	}

	return TotalCount;
}

bool URpgCraftingStationComponent::ConsumeResources(AActor* RequestingActor, const TArray<FRpgCraftingResourceCost>& RequiredItems)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return false;
	}

	for (const FRpgCraftingResourceCost& RequiredItem : RequiredItems)
	{
		if (!RequiredItem.ItemDefinition || RequiredItem.Count <= 0)
		{
			return false;
		}

		if (GetAvailableResourceCount(RequestingActor, RequiredItem.ItemDefinition) < RequiredItem.Count)
		{
			return false;
		}
	}

	TArray<URpgInventoryManagerComponent*> ResourceInventories = GetResourceInventories(RequestingActor);
	for (const FRpgCraftingResourceCost& RequiredItem : RequiredItems)
	{
		int32 RemainingCount = RequiredItem.Count;

		auto ConsumeFromBase = [&]()
		{
			if (RemainingCount <= 0)
			{
				return true;
			}

			const URpgBaseStorageComponent* BaseStorage = GetLinkedBaseStorage();
			const int32 AvailableInBase = BaseStorage ? BaseStorage->GetResourceCount(RequiredItem.ItemDefinition) : 0;
			const int32 CountToConsume = FMath::Min(AvailableInBase, RemainingCount);
			if (ConsumeBaseResources(RequiredItem.ItemDefinition, CountToConsume))
			{
				RemainingCount -= CountToConsume;
				return true;
			}

			return false;
		};

		auto ConsumeFromInventories = [&]()
		{
			if (RemainingCount <= 0)
			{
				return true;
			}

			const int32 AvailableInInventories = GetAvailableInventoryResourceCount(RequiredItem.ItemDefinition, ResourceInventories);
			const int32 CountToConsume = FMath::Min(AvailableInInventories, RemainingCount);
			if (ConsumeInventoryResources(RequiredItem.ItemDefinition, CountToConsume, ResourceInventories))
			{
				RemainingCount -= CountToConsume;
				return true;
			}

			return false;
		};

		switch (ResourceConsumeOrder)
		{
		case ERpgCraftingResourceConsumeOrder::BaseThenPlayer:
			if (!ConsumeFromBase() || !ConsumeFromInventories())
			{
				return false;
			}
			break;

		case ERpgCraftingResourceConsumeOrder::PlayerThenBase:
			if (!ConsumeFromInventories() || !ConsumeFromBase())
			{
				return false;
			}
			break;

		case ERpgCraftingResourceConsumeOrder::BaseOnly:
			if (!ConsumeFromBase())
			{
				return false;
			}
			break;

		case ERpgCraftingResourceConsumeOrder::PlayerOnly:
			if (!ConsumeFromInventories())
			{
				return false;
			}
			break;
		}

		if (RemainingCount > 0)
		{
			return false;
		}
	}

	return true;
}

bool URpgCraftingStationComponent::CanActorAccess(const AActor* RequestingActor) const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !RequestingActor)
	{
		return false;
	}

	const APawn* RequestingPawn = Cast<APawn>(RequestingActor);
	const AController* RequestingController = Cast<AController>(RequestingActor);
	if (!RequestingController && RequestingPawn)
	{
		RequestingController = RequestingPawn->GetController();
	}

	if (!RequestingController || !RequestingController->IsPlayerController())
	{
		return false;
	}

	if (InteractionRadius <= 0.0f)
	{
		return true;
	}

	return FVector::DistSquared(OwnerActor->GetActorLocation(), RequestingActor->GetActorLocation()) <= FMath::Square(InteractionRadius);
}

void URpgCraftingStationComponent::SetOutputInventoryManager(URpgInventoryManagerComponent* InOutputInventory)
{
	OutputInventoryComponent = InOutputInventory;
	if (OutputInventoryComponent)
	{
		OutputInventoryComponent->SetCapacityMode(ERpgInventoryCapacityMode::FixedEntries);
		OutputInventoryComponent->SetFixedMaxEntries(OutputSlotCount);
	}
}

bool URpgCraftingStationComponent::CanAcceptCraftingOutputs(const TArray<FRpgCraftingOutputItem>& OutputItems) const
{
	TMap<TSubclassOf<URpgInventoryItemDefinition>, int32> RemainingOutputCounts;
	const bool bAutoDeposit = ShouldAutoDepositCraftingOutputs();
	URpgBaseStorageComponent* BaseStorage = GetLinkedBaseStorage();
	URpgInventoryManagerComponent* ArmoryInventory = GetLinkedArmoryInventory();

	for (const FRpgCraftingOutputItem& OutputItem : OutputItems)
	{
		if (!OutputItem.ItemDefinition || OutputItem.Count <= 0)
		{
			return false;
		}

		int32 RemainingCount = OutputItem.Count;
		if (bAutoDeposit && IsMaterialDefinition(OutputItem.ItemDefinition) && BaseStorage)
		{
			RemainingCount -= FMath::Min(RemainingCount, BaseStorage->GetFreeResourceCapacity(OutputItem.ItemDefinition));
		}
		else if (bAutoDeposit && bAutoDepositInstanceOutputsToArmory && ArmoryInventory && ArmoryInventory->CanAddItemDefinition(OutputItem.ItemDefinition, OutputItem.Count))
		{
			RemainingCount = 0;
		}

		if (RemainingCount > 0)
		{
			RemainingOutputCounts.FindOrAdd(OutputItem.ItemDefinition) += RemainingCount;
		}
	}

	if (RemainingOutputCounts.Num() == 0)
	{
		return true;
	}

	if (!OutputInventoryComponent)
	{
		return false;
	}

	if (OutputInventoryComponent->IsCapacityUnlimited())
	{
		return true;
	}

	int32 RequiredNewEntries = 0;
	for (const TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>& RemainingOutput : RemainingOutputCounts)
	{
		if (!OutputInventoryComponent->CanAddItemDefinition(RemainingOutput.Key, RemainingOutput.Value))
		{
			return false;
		}

		RequiredNewEntries += OutputInventoryComponent->GetRequiredNewEntryCountForItemDefinition(RemainingOutput.Key, RemainingOutput.Value);
	}

	return RequiredNewEntries <= OutputInventoryComponent->GetFreeEntryCount();
}

bool URpgCraftingStationComponent::AddCraftingOutputs(const TArray<FRpgCraftingOutputItem>& OutputItems)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !CanAcceptCraftingOutputs(OutputItems))
	{
		return false;
	}

	const bool bAutoDeposit = ShouldAutoDepositCraftingOutputs();
	URpgBaseStorageComponent* BaseStorage = GetLinkedBaseStorage();
	URpgInventoryManagerComponent* ArmoryInventory = GetLinkedArmoryInventory();

	for (const FRpgCraftingOutputItem& OutputItem : OutputItems)
	{
		int32 RemainingCount = OutputItem.Count;
		if (bAutoDeposit && IsMaterialDefinition(OutputItem.ItemDefinition) && BaseStorage)
		{
			const int32 CountToStore = FMath::Min(RemainingCount, BaseStorage->GetFreeResourceCapacity(OutputItem.ItemDefinition));
			if (CountToStore > 0 && BaseStorage->StoreResource(OutputItem.ItemDefinition, CountToStore))
			{
				RemainingCount -= CountToStore;
			}
		}
		else if (bAutoDeposit && bAutoDepositInstanceOutputsToArmory && ArmoryInventory && ArmoryInventory->CanAddItemDefinition(OutputItem.ItemDefinition, RemainingCount))
		{
			ArmoryInventory->AddItemDefinition(OutputItem.ItemDefinition, RemainingCount);
			RemainingCount = 0;
		}

		if (RemainingCount > 0)
		{
			if (!OutputInventoryComponent || !OutputInventoryComponent->AddItemDefinition(OutputItem.ItemDefinition, RemainingCount))
			{
				return false;
			}
		}
	}

	return true;
}

bool URpgCraftingStationComponent::CraftItems(AActor* RequestingActor, const TArray<FRpgCraftingResourceCost>& RequiredItems, const TArray<FRpgCraftingOutputItem>& OutputItems)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !CanActorAccess(RequestingActor) || !CanAcceptCraftingOutputs(OutputItems))
	{
		return false;
	}

	return ConsumeResources(RequestingActor, RequiredItems) && AddCraftingOutputs(OutputItems);
}

bool URpgCraftingStationComponent::FlushOutputToBaseStorage()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !OutputInventoryComponent)
	{
		return false;
	}

	bool bMovedAnyOutput = false;
	URpgBaseStorageComponent* BaseStorage = GetLinkedBaseStorage();
	URpgInventoryManagerComponent* ArmoryInventory = GetLinkedArmoryInventory();
	const TArray<FRpgInventoryEntryView> OutputEntries = OutputInventoryComponent->GetAllEntries();
	for (const FRpgInventoryEntryView& OutputEntry : OutputEntries)
	{
		URpgInventoryItemInstance* OutputInstance = OutputEntry.Instance;
		if (!OutputInstance || OutputEntry.StackCount <= 0)
		{
			continue;
		}

		const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition = OutputInstance->GetItemDef();
		if (IsMaterialDefinition(ItemDefinition) && BaseStorage)
		{
			const int32 CountToStore = FMath::Min(OutputEntry.StackCount, BaseStorage->GetFreeResourceCapacity(ItemDefinition));
			if (CountToStore > 0 && OutputInventoryComponent->RemoveItemInstanceStack(OutputInstance, CountToStore))
			{
				BaseStorage->StoreResource(ItemDefinition, CountToStore);
				bMovedAnyOutput = true;
			}
			continue;
		}

		if (bAutoDepositInstanceOutputsToArmory && ArmoryInventory && ArmoryInventory->CanAddItemInstance(OutputInstance, OutputEntry.StackCount))
		{
			OutputInventoryComponent->RemoveItemInstance(OutputInstance);
			ArmoryInventory->AddItemInstanceWithStack(OutputInstance, OutputEntry.StackCount);
			bMovedAnyOutput = true;
		}
	}

	return bMovedAnyOutput;
}

bool URpgCraftingStationComponent::ShouldAutoDepositCraftingOutputs() const
{
	return bAlwaysAutoDepositCraftingOutputs ||
		(OutputAutoDepositUpgradeProvider &&
			OutputAutoDepositUpgradeProvider->HasUpgradeTag(RpgGameplayTags::Base_Storage_Upgrade_CraftingOutputAutoDeposit));
}

URpgBaseStorageComponent* URpgCraftingStationComponent::GetLinkedBaseStorage() const
{
	return bUseLinkedBaseStorage && LinkedBaseCamp ? LinkedBaseCamp->GetBaseStorageComponent() : nullptr;
}

URpgInventoryManagerComponent* URpgCraftingStationComponent::GetLinkedArmoryInventory() const
{
	return LinkedBaseCamp ? LinkedBaseCamp->GetArmoryInventoryComponent() : nullptr;
}

int32 URpgCraftingStationComponent::GetAvailableInventoryResourceCount(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, const TArray<URpgInventoryManagerComponent*>& ResourceInventories) const
{
	int32 TotalCount = 0;
	for (URpgInventoryManagerComponent* Inventory : ResourceInventories)
	{
		if (Inventory)
		{
			TotalCount += Inventory->GetTotalItemCountByDefinition(ItemDefinition);
		}
	}
	return TotalCount;
}

bool URpgCraftingStationComponent::ConsumeInventoryResources(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count, const TArray<URpgInventoryManagerComponent*>& ResourceInventories) const
{
	int32 RemainingCount = Count;
	for (URpgInventoryManagerComponent* Inventory : ResourceInventories)
	{
		if (!Inventory || RemainingCount <= 0)
		{
			break;
		}

		const int32 AvailableInInventory = Inventory->GetTotalItemCountByDefinition(ItemDefinition);
		const int32 CountToConsume = FMath::Min(AvailableInInventory, RemainingCount);
		if (CountToConsume > 0)
		{
			if (!Inventory->ConsumeItemsByDefinition(ItemDefinition, CountToConsume))
			{
				return false;
			}
			RemainingCount -= CountToConsume;
		}
	}

	return RemainingCount <= 0;
}

bool URpgCraftingStationComponent::ConsumeBaseResources(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count) const
{
	if (Count <= 0)
	{
		return true;
	}

	URpgBaseStorageComponent* BaseStorage = GetLinkedBaseStorage();
	return BaseStorage && BaseStorage->WithdrawResource(ItemDefinition, Count);
}
