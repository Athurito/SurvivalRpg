#include "RpgDroppedInventoryActor.h"

#include "SurvivalRpg/Interaction/Abilities/RpgGameplayAbility_Collect.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgDroppedInventoryActor)

ARpgDroppedInventoryActor::ARpgDroppedInventoryActor(const FObjectInitializer& ObjectInitializer)
	: Super()
{
	(void)ObjectInitializer;

	LootInventoryComponent = CreateDefaultSubobject<URpgInventoryManagerComponent>(TEXT("LootInventoryComponent"));
}

void ARpgDroppedInventoryActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	EnsureDefaultPickupInteractionOption();

	if (HasAuthority() && LootInventoryComponent)
	{
		LootInventoryComponent->SetCapacityMode(ERpgInventoryCapacityMode::Unlimited);
		PopulateLootInventoryFromPickup(StaticInventory);
	}
}

FInventoryPickup ARpgDroppedInventoryActor::GetPickupInventory() const
{
	if (LootInventoryComponent && !LootInventoryComponent->GetAllEntries().IsEmpty())
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
		StaticInventory = NewPickupInventory;
		PopulateLootInventoryFromPickup(StaticInventory);
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
	for (FPickupTemplate& Template : StaticInventory.Templates)
	{
		if (Template.ItemDef == ItemDefinition)
		{
			Template.StackCount += StackCount;
			if (LootInventoryComponent)
			{
				LootInventoryComponent->AddItemDefinition(ItemDefinition, StackCount);
			}
			ForceNetUpdate();
			return true;
		}
	}

	FPickupTemplate& NewTemplate = StaticInventory.Templates.AddDefaulted_GetRef();
	NewTemplate.ItemDef = ItemDefinition;
	NewTemplate.StackCount = StackCount;
	if (LootInventoryComponent)
	{
		LootInventoryComponent->AddItemDefinition(ItemDefinition, StackCount);
	}
	ForceNetUpdate();
	return true;
}

bool ARpgDroppedInventoryActor::CanMergePickupTemplate(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	if (!ItemDefinition || StaticInventory.Instances.Num() > 0)
	{
		return false;
	}

	for (const FPickupTemplate& Template : StaticInventory.Templates)
	{
		if (Template.ItemDef == ItemDefinition)
		{
			return true;
		}
	}

	return StaticInventory.Templates.Num() == 0;
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
	if (!HasAuthority() || !LootInventoryComponent || !LootInventoryComponent->GetAllEntries().IsEmpty())
	{
		return;
	}

	for (const FPickupTemplate& Template : PickupInventory.Templates)
	{
		if (Template.ItemDef && Template.StackCount > 0)
		{
			LootInventoryComponent->AddItemDefinition(Template.ItemDef, Template.StackCount);
		}
	}

	for (const FPickupInstance& Instance : PickupInventory.Instances)
	{
		if (Instance.Item)
		{
			LootInventoryComponent->AddItemInstance(Instance.Item);
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
