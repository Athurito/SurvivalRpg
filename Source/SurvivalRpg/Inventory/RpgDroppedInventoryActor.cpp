#include "RpgDroppedInventoryActor.h"

#include "SurvivalRpg/Interaction/Abilities/RpgGameplayAbility_Collect.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerComponent.h"
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

	if (HasAuthority() && LootInventoryComponent)
	{
		LootInventoryComponent->SetCapacityMode(ERpgInventoryCapacityMode::Unlimited);
		PopulateLootInventoryFromPickup(StaticInventory);
		StaticInventory = FInventoryPickup();
		bLootInventoryInitialized = true;
	}
}

FInventoryPickup ARpgDroppedInventoryActor::GetPickupInventory() const
{
	if (LootInventoryComponent && bLootInventoryInitialized)
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

		const URpgInventoryFragment_ItemTraits* Traits = ItemInstance->FindFragmentByClass<URpgInventoryFragment_ItemTraits>();
		if (Traits && Traits->GetMaxStackSize() > 1)
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
