#include "RpgInventoryAutomationTestTypes.h"

#include "RpgInventoryFragment_EquippableItem.h"
#include "RpgInventoryFragment_ItemContainer.h"
#include "RpgInventoryFragment_ItemTraits.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryAutomationTestTypes)

namespace
{
	void ConfigureSpatialFragment(
		URpgInventoryFragment_SpatialItem* SpatialFragment,
		int32 Width,
		int32 Height,
		bool bAllowRotation)
	{
		check(SpatialFragment);
		SpatialFragment->Footprint.Width = Width;
		SpatialFragment->Footprint.Height = Height;
		SpatialFragment->bAllowRotation = bAllowRotation;
	}

	void ConfigureNonStackingTraits(URpgInventoryFragment_ItemTraits* TraitsFragment)
	{
		check(TraitsFragment);
		TraitsFragment->ItemCategory = ERpgInventoryItemCategory::Misc;
		TraitsFragment->bCanStack = false;
		TraitsFragment->MaxStackSize = 1;
	}
}

URpgInventoryAutomationTestUnitItemDefinition::URpgInventoryAutomationTestUnitItemDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Automation Unit"));

	URpgInventoryFragment_SpatialItem* SpatialFragment =
		CreateDefaultSubobject<URpgInventoryFragment_SpatialItem>(TEXT("Spatial"));
	ConfigureSpatialFragment(SpatialFragment, 1, 1, true);
	Fragments.Add(SpatialFragment);

	URpgInventoryFragment_ItemTraits* TraitsFragment =
		CreateDefaultSubobject<URpgInventoryFragment_ItemTraits>(TEXT("Traits"));
	ConfigureNonStackingTraits(TraitsFragment);
	Fragments.Add(TraitsFragment);
}

URpgInventoryAutomationTestStackItemDefinition::URpgInventoryAutomationTestStackItemDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Automation Stack"));

	URpgInventoryFragment_SpatialItem* SpatialFragment =
		CreateDefaultSubobject<URpgInventoryFragment_SpatialItem>(TEXT("Spatial"));
	ConfigureSpatialFragment(SpatialFragment, 1, 1, true);
	Fragments.Add(SpatialFragment);

	URpgInventoryFragment_ItemTraits* TraitsFragment =
		CreateDefaultSubobject<URpgInventoryFragment_ItemTraits>(TEXT("Traits"));
	TraitsFragment->ItemCategory = ERpgInventoryItemCategory::Consumable;
	TraitsFragment->bCanStack = true;
	TraitsFragment->MaxStackSize = 10;
	Fragments.Add(TraitsFragment);
}

URpgInventoryAutomationTestUsableItemDefinition::URpgInventoryAutomationTestUsableItemDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Automation Usable"));

	URpgInventoryFragment_SpatialItem* SpatialFragment =
		CreateDefaultSubobject<URpgInventoryFragment_SpatialItem>(TEXT("Spatial"));
	ConfigureSpatialFragment(SpatialFragment, 1, 1, true);
	Fragments.Add(SpatialFragment);

	URpgInventoryFragment_ItemTraits* TraitsFragment =
		CreateDefaultSubobject<URpgInventoryFragment_ItemTraits>(TEXT("Traits"));
	TraitsFragment->ItemCategory = ERpgInventoryItemCategory::Consumable;
	TraitsFragment->bCanStack = true;
	TraitsFragment->MaxStackSize = 10;
	Fragments.Add(TraitsFragment);

	URpgInventoryFragment_UsableItem* UsableFragment =
		CreateDefaultSubobject<URpgInventoryFragment_UsableItem>(TEXT("Usable"));
	UsableFragment->UseAbility =
		URpgInventoryAutomationTestUseAbility::StaticClass();
	UsableFragment->ConsumeCount = 1;
	UsableFragment->bOnlyFromPlayerInventory = true;
	Fragments.Add(UsableFragment);
}

URpgInventoryAutomationTestNoDropItemDefinition::URpgInventoryAutomationTestNoDropItemDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Automation No Drop"));

	URpgInventoryFragment_SpatialItem* SpatialFragment =
		CreateDefaultSubobject<URpgInventoryFragment_SpatialItem>(TEXT("Spatial"));
	ConfigureSpatialFragment(SpatialFragment, 1, 1, true);
	Fragments.Add(SpatialFragment);

	URpgInventoryFragment_ItemTraits* TraitsFragment =
		CreateDefaultSubobject<URpgInventoryFragment_ItemTraits>(TEXT("Traits"));
	ConfigureNonStackingTraits(TraitsFragment);
	TraitsFragment->ManualDropPolicy =
		ERpgInventoryManualDropPolicy::Disabled;
	Fragments.Add(TraitsFragment);
}

URpgInventoryAutomationTestWideItemDefinition::URpgInventoryAutomationTestWideItemDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Automation Wide"));

	URpgInventoryFragment_SpatialItem* SpatialFragment =
		CreateDefaultSubobject<URpgInventoryFragment_SpatialItem>(TEXT("Spatial"));
	ConfigureSpatialFragment(SpatialFragment, 2, 1, true);
	Fragments.Add(SpatialFragment);

	URpgInventoryFragment_ItemTraits* TraitsFragment =
		CreateDefaultSubobject<URpgInventoryFragment_ItemTraits>(TEXT("Traits"));
	ConfigureNonStackingTraits(TraitsFragment);
	Fragments.Add(TraitsFragment);
}

URpgInventoryAutomationTestLargeItemDefinition::URpgInventoryAutomationTestLargeItemDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Automation Large"));

	URpgInventoryFragment_SpatialItem* SpatialFragment =
		CreateDefaultSubobject<URpgInventoryFragment_SpatialItem>(TEXT("Spatial"));
	ConfigureSpatialFragment(SpatialFragment, 3, 2, true);
	Fragments.Add(SpatialFragment);

	URpgInventoryFragment_ItemTraits* TraitsFragment =
		CreateDefaultSubobject<URpgInventoryFragment_ItemTraits>(TEXT("Traits"));
	ConfigureNonStackingTraits(TraitsFragment);
	Fragments.Add(TraitsFragment);
}

URpgInventoryAutomationTestWeaponItemDefinition::URpgInventoryAutomationTestWeaponItemDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Automation Weapon"));

	URpgInventoryFragment_SpatialItem* SpatialFragment =
		CreateDefaultSubobject<URpgInventoryFragment_SpatialItem>(TEXT("Spatial"));
	ConfigureSpatialFragment(SpatialFragment, 1, 1, true);
	Fragments.Add(SpatialFragment);

	URpgInventoryFragment_ItemTraits* TraitsFragment =
		CreateDefaultSubobject<URpgInventoryFragment_ItemTraits>(TEXT("Traits"));
	ConfigureNonStackingTraits(TraitsFragment);
	TraitsFragment->ItemCategory = ERpgInventoryItemCategory::Weapon;
	Fragments.Add(TraitsFragment);

	URpgInventoryFragment_EquippableItem* EquippableFragment =
		CreateDefaultSubobject<URpgInventoryFragment_EquippableItem>(TEXT("Equippable"));
	EquippableFragment->EquipmentDefinition = URpgInventoryAutomationTestWeaponEquipmentDefinition::StaticClass();
	Fragments.Add(EquippableFragment);
}

URpgInventoryAutomationTestStackableWeaponItemDefinition::URpgInventoryAutomationTestStackableWeaponItemDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Automation Stackable Weapon"));

	URpgInventoryFragment_SpatialItem* SpatialFragment =
		CreateDefaultSubobject<URpgInventoryFragment_SpatialItem>(TEXT("Spatial"));
	ConfigureSpatialFragment(SpatialFragment, 1, 1, true);
	Fragments.Add(SpatialFragment);

	URpgInventoryFragment_ItemTraits* TraitsFragment =
		CreateDefaultSubobject<URpgInventoryFragment_ItemTraits>(TEXT("Traits"));
	TraitsFragment->ItemCategory = ERpgInventoryItemCategory::Weapon;
	TraitsFragment->bCanStack = true;
	TraitsFragment->MaxStackSize = 10;
	Fragments.Add(TraitsFragment);

	URpgInventoryFragment_EquippableItem* EquippableFragment =
		CreateDefaultSubobject<URpgInventoryFragment_EquippableItem>(TEXT("Equippable"));
	EquippableFragment->EquipmentDefinition = URpgInventoryAutomationTestWeaponEquipmentDefinition::StaticClass();
	Fragments.Add(EquippableFragment);
}

URpgInventoryAutomationTestWeaponEquipmentDefinition::URpgInventoryAutomationTestWeaponEquipmentDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AllowedSlots = { ERpgEquipmentSlot::MainHand };
}

URpgInventoryAutomationTestMainHandShieldItemDefinition::URpgInventoryAutomationTestMainHandShieldItemDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Automation MainHand-only Shield"));

	URpgInventoryFragment_SpatialItem* SpatialFragment =
		CreateDefaultSubobject<URpgInventoryFragment_SpatialItem>(TEXT("Spatial"));
	ConfigureSpatialFragment(SpatialFragment, 1, 1, true);
	Fragments.Add(SpatialFragment);

	URpgInventoryFragment_ItemTraits* TraitsFragment =
		CreateDefaultSubobject<URpgInventoryFragment_ItemTraits>(TEXT("Traits"));
	ConfigureNonStackingTraits(TraitsFragment);
	TraitsFragment->ItemCategory = ERpgInventoryItemCategory::Shield;
	Fragments.Add(TraitsFragment);

	URpgInventoryFragment_EquippableItem* EquippableFragment =
		CreateDefaultSubobject<URpgInventoryFragment_EquippableItem>(TEXT("Equippable"));
	EquippableFragment->EquipmentDefinition = URpgInventoryAutomationTestWeaponEquipmentDefinition::StaticClass();
	Fragments.Add(EquippableFragment);
}

URpgInventoryAutomationTestBagItemDefinition::URpgInventoryAutomationTestBagItemDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Automation Bag"));

	URpgInventoryFragment_SpatialItem* SpatialFragment =
		CreateDefaultSubobject<URpgInventoryFragment_SpatialItem>(TEXT("Spatial"));
	ConfigureSpatialFragment(SpatialFragment, 1, 1, true);
	Fragments.Add(SpatialFragment);

	URpgInventoryFragment_ItemTraits* TraitsFragment =
		CreateDefaultSubobject<URpgInventoryFragment_ItemTraits>(TEXT("Traits"));
	ConfigureNonStackingTraits(TraitsFragment);
	Fragments.Add(TraitsFragment);

	URpgInventoryFragment_ItemContainer* ContainerFragment =
		CreateDefaultSubobject<URpgInventoryFragment_ItemContainer>(TEXT("ItemContainer"));
	FRpgInventoryItemContainerDefinition& MainContainer = ContainerFragment->ProvidedContainers.AddDefaulted_GetRef();
	MainContainer.ContainerId = TEXT("Main");
	MainContainer.DisplayName = FText::FromString(TEXT("Main"));
	MainContainer.GridSize.Width = 4;
	MainContainer.GridSize.Height = 4;
	MainContainer.bAllowNestedContainers = true;
	MainContainer.MaxNestingDepth = RpgInventoryMaxItemOwnedDepth;
	Fragments.Add(ContainerFragment);

	URpgInventoryFragment_EquippableItem* EquippableFragment =
		CreateDefaultSubobject<URpgInventoryFragment_EquippableItem>(TEXT("Equippable"));
	EquippableFragment->EquipmentDefinition = URpgInventoryAutomationTestBagEquipmentDefinition::StaticClass();
	Fragments.Add(EquippableFragment);
}

URpgInventoryAutomationTestBagEquipmentDefinition::URpgInventoryAutomationTestBagEquipmentDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The same test provider can stand in for Backpack or Belt so quick-transfer tests exercise ordered
	// equipment-provided content grids without introducing a second otherwise identical fixture definition.
	AllowedSlots = { ERpgEquipmentSlot::Backpack, ERpgEquipmentSlot::Belt };
	EquipLoadWeight = 7.5f;
}

URpgInventoryAutomationTestHeavyEquipmentDefinition::URpgInventoryAutomationTestHeavyEquipmentDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AllowedSlots = { ERpgEquipmentSlot::Chest };
	EquipLoadWeight = 30.0f;
}

URpgInventoryAutomationTestHeavyItemDefinition::URpgInventoryAutomationTestHeavyItemDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Automation Heavy Armor"));

	URpgInventoryFragment_SpatialItem* SpatialFragment =
		CreateDefaultSubobject<URpgInventoryFragment_SpatialItem>(TEXT("Spatial"));
	ConfigureSpatialFragment(SpatialFragment, 1, 1, true);
	Fragments.Add(SpatialFragment);

	URpgInventoryFragment_ItemTraits* TraitsFragment =
		CreateDefaultSubobject<URpgInventoryFragment_ItemTraits>(TEXT("Traits"));
	ConfigureNonStackingTraits(TraitsFragment);
	TraitsFragment->ItemCategory = ERpgInventoryItemCategory::Armor;
	Fragments.Add(TraitsFragment);

	URpgInventoryFragment_EquippableItem* EquippableFragment =
		CreateDefaultSubobject<URpgInventoryFragment_EquippableItem>(TEXT("Equippable"));
	EquippableFragment->EquipmentDefinition = URpgInventoryAutomationTestHeavyEquipmentDefinition::StaticClass();
	Fragments.Add(EquippableFragment);
}

void ARpgInventoryAutomationTestPlayerState::PostInitializeComponents()
{
	// The inventory/loadout test does not compose a full Experience-backed GameState.
	APlayerState::PostInitializeComponents();
}
