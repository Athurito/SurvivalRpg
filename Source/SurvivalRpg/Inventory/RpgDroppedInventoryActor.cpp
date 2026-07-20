#include "RpgDroppedInventoryActor.h"

#include "SurvivalRpg/Interaction/Abilities/RpgGameplayAbility_Collect.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemContainer.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgDroppedInventoryActor)

ARpgDroppedInventoryActor::ARpgDroppedInventoryActor(const FObjectInitializer& ObjectInitializer)
	: Super()
{
	(void)ObjectInitializer;

	LootInventoryComponent = CreateDefaultSubobject<URpgInventoryManagerComponent>(TEXT("LootInventoryComponent"));
	ContainerComponent = CreateDefaultSubobject<URpgInventoryContainerComponent>(TEXT("ContainerComponent"));
}

void ARpgDroppedInventoryActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	EnsureDefaultPickupInteractionOption();

	if (LootInventoryComponent)
	{
		if (HasAuthority())
		{
			LootInventoryComponent->SetCapacityMode(
				ERpgInventoryCapacityMode::Unlimited);
			PopulateLootInventoryFromPickup(StaticInventory);
			StaticInventory = FInventoryPickup();
		}

		// From PostInitializeComponents onward the replicated manager is canonical on every
		// role. A client must not briefly resurrect local StaticInventory while waiting for
		// FastArray entries (including empty inventories and late joins).
		bLootInventoryInitialized = true;
	}
}

FInventoryPickup ARpgDroppedInventoryActor::GetPickupInventory() const
{
	if (LootInventoryComponent &&
		(bLootInventoryInitialized ||
			LootInventoryComponent->GetUsedEntryCount() > 0))
	{
		return BuildPickupInventoryFromLootInventory();
	}

	return StaticInventory;
}

void ARpgDroppedInventoryActor::SetPickupInventory(const FInventoryPickup& NewPickupInventory)
{
	if (HasAuthority())
	{
		EnsureDefaultPickupInteractionOption();
		if (LootInventoryComponent)
		{
			const TArray<FRpgInventoryEntryView> ExistingEntries = LootInventoryComponent->GetAllEntries();
			for (const FRpgInventoryEntryView& Entry : ExistingEntries)
			{
				LootInventoryComponent->RemoveItemInstance(Entry.Instance);
			}
		}
		PopulateLootInventoryFromPickup(NewPickupInventory);
		StaticInventory = FInventoryPickup();
		bLootInventoryInitialized = true;
		ForceNetUpdate();
	}
}

FRpgInventoryMutationResult ARpgDroppedInventoryActor::TransferItemFromInventory(
	URpgInventoryManagerComponent* SourceInventory,
	FRpgInventoryItemId ItemId,
	int32 StackCount,
	FGuid RequestId,
	bool bPreventStackMerge)
{
	FRpgInventoryMutationRequest Request;
	Request.Operation = ERpgInventoryMutationOperation::Drop;
	Request.ItemId = ItemId;
	Request.Quantity = StackCount;
	Request.RequestId = RequestId;
	Request.EnsureRequestId();

	FRpgInventoryMutationResult Result;
	Result.RequestId = Request.RequestId;
	Result.Operation = Request.Operation;
	Result.RequestedQuantity = StackCount;
	if (!HasAuthority())
	{
		Result.Code = ERpgInventoryMutationResultCode::AuthorityRequired;
		return Result;
	}
	if (!SourceInventory || SourceInventory == LootInventoryComponent ||
		!LootInventoryComponent || !ItemId.IsValid() || StackCount <= 0)
	{
		Result.Code = ERpgInventoryMutationResultCode::InvalidRequest;
		return Result;
	}

	URpgInventoryItemInstance* Item = SourceInventory->FindItemById(ItemId);
	FRpgInventoryGridPlacement SourcePlacement;
	if (!Item ||
		!SourceInventory->GetItemPlacement(Item, SourcePlacement))
	{
		Result.Code = ERpgInventoryMutationResultCode::ItemNotFound;
		return Result;
	}

	Request.Source = SourcePlacement.GetContainerHandle();
	Request.Target = FRpgInventoryContainerHandle::MakeRoot(
		LootInventoryComponent->GetDefaultContainerId());
	if (bPreventStackMerge)
	{
		auto TryFindConcretePlacement =
			[this, &Request, Item, StackCount](
				const FRpgInventoryGridSize& GridSize)
			{
				for (int32 RotationIndex = 0;
					RotationIndex < 2 &&
						!Request.TargetPlacement.IsValid();
					++RotationIndex)
				{
					for (int32 Y = 0;
						Y < GridSize.Height &&
							!Request.TargetPlacement.IsValid();
						++Y)
					{
						for (int32 X = 0;
							X < GridSize.Width;
							++X)
						{
							FRpgInventoryGridPlacement Candidate;
							Candidate.SetContainerHandle(
								Request.Target);
							Candidate.X = X;
							Candidate.Y = Y;
							Candidate.bRotated =
								RotationIndex == 1;
							if (LootInventoryComponent
									->CanReceiveTransferredItemInstanceToPlacement(
										Item,
										StackCount,
										Candidate))
							{
								Request.TargetPlacement =
									Candidate;
								break;
							}
						}
					}
				}
			};

		FRpgInventoryGridSize GridSize;
		if (LootInventoryComponent->GetGridSizeForContainerHandle(
				Request.Target,
				GridSize))
		{
			TryFindConcretePlacement(GridSize);
		}
		if (!Request.TargetPlacement.IsValid())
		{
			const FRpgInventoryGridSize ItemSize =
				SourcePlacement.GetUnrotatedSize();
			FRpgInventoryGridSize ExpandedSize;
			ExpandedSize.Width =
				FMath::Max(GridSize.Width, ItemSize.Width);
			ExpandedSize.Height =
				GridSize.Height + FMath::Max(1, ItemSize.Height);
			if (LootInventoryComponent
					->ExpandDefaultGridToMinimum(ExpandedSize) &&
				LootInventoryComponent->GetGridSizeForContainerHandle(
					Request.Target,
					GridSize))
			{
				TryFindConcretePlacement(GridSize);
			}
		}
		if (!Request.TargetPlacement.IsValid())
		{
			Result.Code = ERpgInventoryMutationResultCode::NoSpace;
			return Result;
		}
	}
	Result = SourceInventory->ExecuteCrossInventoryTransfer(
		LootInventoryComponent,
		Request,
		false);
	if (Result.IsSuccess())
	{
		EnsureDefaultPickupInteractionOption();
		bLootInventoryInitialized = true;
		ForceNetUpdate();
	}
	return Result;
}

bool ARpgDroppedInventoryActor::MergePickupTemplate(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 StackCount)
{
	if (!HasAuthority() || !ItemDefinition || StackCount <= 0)
	{
		return false;
	}

	EnsureDefaultPickupInteractionOption();
	if (!LootInventoryComponent || !LootInventoryComponent->CanAddItemDefinition(ItemDefinition, StackCount))
	{
		return false;
	}

	if (!LootInventoryComponent->GrantItemDefinition(ItemDefinition, StackCount))
	{
		return false;
	}
	bLootInventoryInitialized = true;
	ForceNetUpdate();
	return true;
}

bool ARpgDroppedInventoryActor::CanMergePickupTemplate(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	if (!ItemDefinition || !LootInventoryComponent)
	{
		return false;
	}

	const TArray<FRpgInventoryEntryView> Entries = LootInventoryComponent->GetAllEntries();
	for (const FRpgInventoryEntryView& Entry : Entries)
	{
		if (!Entry.Instance || Entry.Instance->GetItemDef() != ItemDefinition)
		{
			return false;
		}
	}

	return Entries.IsEmpty() || LootInventoryComponent->CanAddItemDefinition(ItemDefinition, 1);
}

void ARpgDroppedInventoryActor::EnsureDefaultPickupInteractionOption()
{
	if (!Option.InteractionAbilityToGrant)
	{
		Option.InteractionAbilityToGrant = URpgGameplayAbility_Collect::StaticClass();
	}

	if (Option.Text.IsEmpty())
	{
		Option.Text = NSLOCTEXT("RpgInventory", "PickupDroppedInventoryText", "Pick Up");
	}

	if (Option.SubText.IsEmpty())
	{
		Option.SubText = NSLOCTEXT("RpgInventory", "PickupDroppedInventorySubText", "Loot");
	}
}

void ARpgDroppedInventoryActor::PopulateLootInventoryFromPickup(const FInventoryPickup& PickupInventory)
{
	if (!HasAuthority() || !LootInventoryComponent)
	{
		return;
	}

	for (const FPickupTemplate& Template : PickupInventory.Templates)
	{
		if (Template.ItemDef && Template.StackCount > 0)
		{
			LootInventoryComponent->GrantItemDefinition(Template.ItemDef, Template.StackCount);
		}
	}

	for (const FPickupInstance& Instance : PickupInventory.Instances)
	{
		if (Instance.Item)
		{
			LootInventoryComponent->BootstrapItemInstance(Instance.Item);
		}
	}
}

FInventoryPickup ARpgDroppedInventoryActor::BuildPickupInventoryFromLootInventory() const
{
	FInventoryPickup PickupInventory;
	if (!LootInventoryComponent)
	{
		return PickupInventory;
	}

	for (const FRpgInventoryEntryView& Entry : LootInventoryComponent->GetAllEntries())
	{
		URpgInventoryItemInstance* ItemInstance = Entry.Instance;
		if (!ItemInstance)
		{
			continue;
		}

		if (URpgInventoryManagerComponent::
				GetEffectiveMaxStackSizeForDefinition(
					ItemInstance->GetItemDef()) > 1)
		{
			FPickupTemplate& Template = PickupInventory.Templates.AddDefaulted_GetRef();
			Template.ItemDef = ItemInstance->GetItemDef();
			Template.StackCount = Entry.StackCount;
		}
		else
		{
			FPickupInstance& Instance = PickupInventory.Instances.AddDefaulted_GetRef();
			Instance.Item = ItemInstance;
		}
	}

	return PickupInventory;
}
