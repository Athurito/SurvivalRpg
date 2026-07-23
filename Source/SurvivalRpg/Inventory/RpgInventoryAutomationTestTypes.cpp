#include "RpgInventoryAutomationTestTypes.h"

#include "RpgInventoryFragment_EquippableItem.h"
#include "RpgInventoryFragment_ItemContainer.h"
#include "RpgInventoryFragment_ItemTraits.h"
#include "RpgInventoryItemInstance.h"
#include "RpgPlayerInventoryLayoutComponent.h"
#include "RpgPlayerInventoryLayoutDefinition.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Equipment/RpgEquipmentAutomationTestTypes.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryAutomationTestTypes)

namespace
{
	const FName StatefulFragmentPayloadId(TEXT("Automation.Stateful"));
	constexpr int32 StatefulFragmentPayloadVersion = 1;

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

	FRpgInventorySlotGroupDefinition MakeTestStaticGroup(
		FName ContainerId,
		const FText& DisplayName,
		int32 GridWidth,
		int32 GridHeight,
		const TArray<ERpgInventoryItemCategory>& AllowedCategories,
		bool bActionbarBindable,
		ERpgInventorySlotGroupKind GroupKind,
		ERpgEquipmentSlot EquipmentSlotRole = ERpgEquipmentSlot::None,
		FGameplayTag SemanticRole = FGameplayTag())
	{
		FRpgInventorySlotGroupDefinition Group;
		Group.ContainerId = ContainerId;
		Group.SemanticRole = SemanticRole;
		Group.DisplayName = DisplayName;
		Group.GroupKind = GroupKind;
		Group.EquipmentSlotRole = EquipmentSlotRole;
		Group.GridSize.Width = FMath::Max(1, GridWidth);
		Group.GridSize.Height = FMath::Max(1, GridHeight);
		Group.Rule.AllowedCategories = AllowedCategories;
		Group.Rule.bActionbarBindable = bActionbarBindable;
		return Group;
	}
}

void URpgInventoryAutomationTestStatefulFragment::OnInstanceCreated(
	URpgInventoryItemInstance* Instance) const
{
	for (auto It = TestValues.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}

	if (Instance)
	{
		const TWeakObjectPtr<URpgInventoryItemInstance> Key(Instance);
		TestValues.FindOrAdd(Key) = 0;
	}
}

FName URpgInventoryAutomationTestStatefulFragment::
	GetRuntimeStateIdentifier() const
{
	return StatefulFragmentPayloadId;
}

int32 URpgInventoryAutomationTestStatefulFragment::
	GetRuntimeStateVersion() const
{
	return StatefulFragmentPayloadVersion;
}

bool URpgInventoryAutomationTestStatefulFragment::ExportRuntimeState(
	const URpgInventoryItemInstance* Instance,
	FRpgInventoryFragmentStatePayload& OutPayload) const
{
	if (!Instance)
	{
		return false;
	}

	OutPayload.FragmentId = StatefulFragmentPayloadId;
	OutPayload.Version = StatefulFragmentPayloadVersion;
	OutPayload.Payload = { GetTestValue(Instance) };
	return true;
}

bool URpgInventoryAutomationTestStatefulFragment::ValidateRuntimeState(
	const URpgInventoryItemInstance* Instance,
	const FRpgInventoryFragmentStatePayload& Payload) const
{
	return Instance &&
		Payload.FragmentId == StatefulFragmentPayloadId &&
		Payload.Version == StatefulFragmentPayloadVersion &&
		Payload.Payload.Num() == 1;
}

bool URpgInventoryAutomationTestStatefulFragment::ImportRuntimeState(
	URpgInventoryItemInstance* Instance,
	const FRpgInventoryFragmentStatePayload& Payload) const
{
	if (!ValidateRuntimeState(Instance, Payload))
	{
		return false;
	}

	SetTestValue(Instance, Payload.Payload[0]);
	return true;
}

void URpgInventoryAutomationTestStatefulFragment::CopyRuntimeState(
	const URpgInventoryItemInstance* Source,
	URpgInventoryItemInstance* Target) const
{
	if (Source && Target)
	{
		SetTestValue(Target, GetTestValue(Source));
	}
}

void URpgInventoryAutomationTestStatefulFragment::SetTestValue(
	const URpgInventoryItemInstance* Instance,
	uint8 Value) const
{
	if (Instance)
	{
		const TWeakObjectPtr<URpgInventoryItemInstance> Key(
			const_cast<URpgInventoryItemInstance*>(Instance));
		TestValues.FindOrAdd(Key) = Value;
	}
}

uint8 URpgInventoryAutomationTestStatefulFragment::GetTestValue(
	const URpgInventoryItemInstance* Instance) const
{
	if (Instance)
	{
		const TWeakObjectPtr<URpgInventoryItemInstance> Key(
			const_cast<URpgInventoryItemInstance*>(Instance));
		if (const uint8* Value = TestValues.Find(Key))
		{
			return *Value;
		}
	}
	return 0;
}

void URpgInventoryAutomationTestCountingEquipmentInstance::OnEquipped()
{
	++EquippedCount;
	Super::OnEquipped();
}

void URpgInventoryAutomationTestCountingEquipmentInstance::OnUnequipped()
{
	++UnequippedCount;
	Super::OnUnequipped();
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

URpgInventoryAutomationTestMaterialDefinition::
	URpgInventoryAutomationTestMaterialDefinition(
		const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Automation Material"));

	URpgInventoryFragment_SpatialItem* SpatialFragment =
		CreateDefaultSubobject<URpgInventoryFragment_SpatialItem>(
			TEXT("Spatial"));
	ConfigureSpatialFragment(SpatialFragment, 1, 1, true);
	Fragments.Add(SpatialFragment);

	URpgInventoryFragment_ItemTraits* TraitsFragment =
		CreateDefaultSubobject<URpgInventoryFragment_ItemTraits>(
			TEXT("Traits"));
	TraitsFragment->ItemCategory = ERpgInventoryItemCategory::Material;
	TraitsFragment->bCanStack = true;
	TraitsFragment->MaxStackSize = 10;
	Fragments.Add(TraitsFragment);
}

URpgInventoryAutomationTestMaterialContainerDefinition::
	URpgInventoryAutomationTestMaterialContainerDefinition(
		const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(
		TEXT("Automation Material Container"));

	URpgInventoryFragment_SpatialItem* SpatialFragment =
		CreateDefaultSubobject<URpgInventoryFragment_SpatialItem>(
			TEXT("Spatial"));
	ConfigureSpatialFragment(SpatialFragment, 1, 1, true);
	Fragments.Add(SpatialFragment);

	URpgInventoryFragment_ItemTraits* TraitsFragment =
		CreateDefaultSubobject<URpgInventoryFragment_ItemTraits>(
			TEXT("Traits"));
	ConfigureNonStackingTraits(TraitsFragment);
	TraitsFragment->ItemCategory = ERpgInventoryItemCategory::Material;
	Fragments.Add(TraitsFragment);

	URpgInventoryFragment_ItemContainer* ContainerFragment =
		CreateDefaultSubobject<URpgInventoryFragment_ItemContainer>(
			TEXT("ItemContainer"));
	FRpgInventoryItemContainerDefinition& MainContainer =
		ContainerFragment->ProvidedContainers.AddDefaulted_GetRef();
	MainContainer.ContainerId = TEXT("Main");
	MainContainer.DisplayName = FText::FromString(TEXT("Main"));
	MainContainer.GridSize.Width = 2;
	MainContainer.GridSize.Height = 2;
	MainContainer.bAllowNestedContainers = false;
	Fragments.Add(ContainerFragment);
}

URpgInventoryAutomationTestStatefulMaterialDefinition::
	URpgInventoryAutomationTestStatefulMaterialDefinition(
		const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Automation Stateful Material"));

	URpgInventoryFragment_SpatialItem* SpatialFragment =
		CreateDefaultSubobject<URpgInventoryFragment_SpatialItem>(
			TEXT("Spatial"));
	ConfigureSpatialFragment(SpatialFragment, 1, 1, true);
	Fragments.Add(SpatialFragment);

	URpgInventoryFragment_ItemTraits* TraitsFragment =
		CreateDefaultSubobject<URpgInventoryFragment_ItemTraits>(
			TEXT("Traits"));
	TraitsFragment->ItemCategory = ERpgInventoryItemCategory::Material;
	TraitsFragment->bCanStack = true;
	TraitsFragment->MaxStackSize = 10;
	Fragments.Add(TraitsFragment);

	URpgInventoryAutomationTestStatefulFragment* StatefulFragment =
		CreateDefaultSubobject<URpgInventoryAutomationTestStatefulFragment>(
			TEXT("Stateful"));
	Fragments.Add(StatefulFragment);
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

URpgInventoryAutomationTestFixedWideItemDefinition::URpgInventoryAutomationTestFixedWideItemDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Automation Fixed Wide"));

	URpgInventoryFragment_SpatialItem* SpatialFragment =
		CreateDefaultSubobject<URpgInventoryFragment_SpatialItem>(TEXT("Spatial"));
	ConfigureSpatialFragment(SpatialFragment, 2, 1, false);
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
	InstanceType = URpgInventoryAutomationTestCountingEquipmentInstance::StaticClass();
	AllowedSlots = { ERpgEquipmentSlot::MainHand };
}

URpgInventoryAutomationTestOffHandEquipmentDefinition::URpgInventoryAutomationTestOffHandEquipmentDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstanceType = URpgInventoryAutomationTestCountingEquipmentInstance::StaticClass();
	AllowedSlots = { ERpgEquipmentSlot::OffHand };
	EquipLoadWeight = 4.0f;
}

URpgInventoryAutomationTestStackableOffHandItemDefinition::
	URpgInventoryAutomationTestStackableOffHandItemDefinition(
		const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Automation Stackable OffHand"));

	URpgInventoryFragment_SpatialItem* SpatialFragment =
		CreateDefaultSubobject<URpgInventoryFragment_SpatialItem>(TEXT("Spatial"));
	ConfigureSpatialFragment(SpatialFragment, 1, 1, true);
	Fragments.Add(SpatialFragment);

	URpgInventoryFragment_ItemTraits* TraitsFragment =
		CreateDefaultSubobject<URpgInventoryFragment_ItemTraits>(TEXT("Traits"));
	TraitsFragment->ItemCategory = ERpgInventoryItemCategory::Shield;
	TraitsFragment->bCanStack = true;
	TraitsFragment->MaxStackSize = 10;
	Fragments.Add(TraitsFragment);

	URpgInventoryFragment_EquippableItem* EquippableFragment =
		CreateDefaultSubobject<URpgInventoryFragment_EquippableItem>(TEXT("Equippable"));
	EquippableFragment->EquipmentDefinition =
		URpgInventoryAutomationTestOffHandEquipmentDefinition::StaticClass();
	Fragments.Add(EquippableFragment);
}

URpgInventoryAutomationTestTwoHandEquipmentDefinition::URpgInventoryAutomationTestTwoHandEquipmentDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstanceType = URpgInventoryAutomationTestCountingEquipmentInstance::StaticClass();
	AllowedSlots = { ERpgEquipmentSlot::MainHand };
	HandOccupancy = ERpgEquipmentHandOccupancy::BothHands;
}

URpgInventoryAutomationTestTwoHandItemDefinition::URpgInventoryAutomationTestTwoHandItemDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Automation Two-Hand Weapon"));

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
	EquippableFragment->EquipmentDefinition =
		URpgInventoryAutomationTestTwoHandEquipmentDefinition::StaticClass();
	Fragments.Add(EquippableFragment);
}

URpgInventoryAutomationTestMovableGrantEquipmentDefinition::
	URpgInventoryAutomationTestMovableGrantEquipmentDefinition(
		const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstanceType = URpgInventoryAutomationTestCountingEquipmentInstance::StaticClass();
	AllowedSlots = {
		ERpgEquipmentSlot::Chest,
		ERpgEquipmentSlot::Head
	};
	AbilitySetsToGrant.Add(
		CreateDefaultSubobject<URpgEquipmentAutomationTestHealthAbilitySet>(
			TEXT("PersistentHealthAbilitySet")));
}

URpgInventoryAutomationTestMovableGrantItemDefinition::
	URpgInventoryAutomationTestMovableGrantItemDefinition(
		const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Automation Movable Grant Armor"));

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
	EquippableFragment->EquipmentDefinition =
		URpgInventoryAutomationTestMovableGrantEquipmentDefinition::StaticClass();
	Fragments.Add(EquippableFragment);
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

URpgInventoryAutomationTestGearNameCollisionBagItemDefinition::
	URpgInventoryAutomationTestGearNameCollisionBagItemDefinition(
		const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Automation Gear-Name Collision Bag"));

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
	FRpgInventoryItemContainerDefinition& CollidingContainer =
		ContainerFragment->ProvidedContainers.AddDefaulted_GetRef();
	CollidingContainer.ContainerId = TEXT("Gear.Head");
	CollidingContainer.DisplayName = FText::FromString(TEXT("Gear Name Collision"));
	CollidingContainer.GridSize.Width = 4;
	CollidingContainer.GridSize.Height = 4;
	CollidingContainer.bAllowNestedContainers = true;
	CollidingContainer.MaxNestingDepth = RpgInventoryMaxItemOwnedDepth;
	Fragments.Add(ContainerFragment);
}

URpgInventoryAutomationTestLegacyStackableBagItemDefinition::
	URpgInventoryAutomationTestLegacyStackableBagItemDefinition(
		const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName =
		FText::FromString(TEXT("Automation Legacy Stackable Bag"));

	URpgInventoryFragment_SpatialItem* SpatialFragment =
		CreateDefaultSubobject<URpgInventoryFragment_SpatialItem>(
			TEXT("Spatial"));
	ConfigureSpatialFragment(SpatialFragment, 1, 1, true);
	Fragments.Add(SpatialFragment);

	URpgInventoryFragment_ItemTraits* TraitsFragment =
		CreateDefaultSubobject<URpgInventoryFragment_ItemTraits>(
			TEXT("Traits"));
	TraitsFragment->bCanStack = true;
	TraitsFragment->MaxStackSize = 10;
	Fragments.Add(TraitsFragment);

	URpgInventoryFragment_ItemContainer* ContainerFragment =
		CreateDefaultSubobject<URpgInventoryFragment_ItemContainer>(
			TEXT("ItemContainer"));
	FRpgInventoryItemContainerDefinition& MainContainer =
		ContainerFragment->ProvidedContainers.AddDefaulted_GetRef();
	MainContainer.ContainerId = TEXT("Main");
	MainContainer.DisplayName = FText::FromString(TEXT("Main"));
	MainContainer.GridSize.Width = 4;
	MainContainer.GridSize.Height = 4;
	MainContainer.bAllowNestedContainers = true;
	MainContainer.MaxNestingDepth = RpgInventoryMaxItemOwnedDepth;
	Fragments.Add(ContainerFragment);
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

void ARpgInventoryAutomationTestPlayerState::PostActorCreated()
{
	Super::PostActorCreated();
	InitializeTestPawnData();
}

void ARpgInventoryAutomationTestPlayerState::PostInitializeComponents()
{
	// Keep the shared ASC lifecycle, but skip ARpgPlayerState's Experience/GameState hook.
	ARpgBasePlayerState::PostInitializeComponents();
	InitializeTestPawnData();
}

void ARpgInventoryAutomationTestPlayerState::InitializeTestPawnData()
{
	if (TestPawnData && TestInventoryLayoutDefinition)
	{
		return;
	}

	TestInventoryLayoutDefinition =
		NewObject<URpgPlayerInventoryLayoutDefinition>(
			this,
			TEXT("InventoryAutomationLayoutDefinition"),
			RF_Transient);
	check(TestInventoryLayoutDefinition);

	// Keep the compact 4x2 Pockets baseline used by transaction fixtures. The
	// production 6x6 contract is covered by the asset-composition test.
	TestInventoryLayoutDefinition->StaticSlotGroups =
	{
		MakeTestStaticGroup(URpgPlayerInventoryLayoutComponent::GearHeadGroupId, NSLOCTEXT("RpgInventoryLayout", "GearHead", "Head"), 1, 1, { ERpgInventoryItemCategory::Armor }, false, ERpgInventorySlotGroupKind::Gear, ERpgEquipmentSlot::Head),
		MakeTestStaticGroup(URpgPlayerInventoryLayoutComponent::GearChestGroupId, NSLOCTEXT("RpgInventoryLayout", "GearChest", "Chest"), 1, 1, { ERpgInventoryItemCategory::Armor }, false, ERpgInventorySlotGroupKind::Gear, ERpgEquipmentSlot::Chest),
		MakeTestStaticGroup(URpgPlayerInventoryLayoutComponent::GearHandsGroupId, NSLOCTEXT("RpgInventoryLayout", "GearHands", "Hands"), 1, 1, { ERpgInventoryItemCategory::Armor }, false, ERpgInventorySlotGroupKind::Gear, ERpgEquipmentSlot::Hands),
		MakeTestStaticGroup(URpgPlayerInventoryLayoutComponent::GearLegsGroupId, NSLOCTEXT("RpgInventoryLayout", "GearLegs", "Legs"), 1, 1, { ERpgInventoryItemCategory::Armor }, false, ERpgInventorySlotGroupKind::Gear, ERpgEquipmentSlot::Legs),
		MakeTestStaticGroup(URpgPlayerInventoryLayoutComponent::GearFeetGroupId, NSLOCTEXT("RpgInventoryLayout", "GearFeet", "Feet"), 1, 1, { ERpgInventoryItemCategory::Armor }, false, ERpgInventorySlotGroupKind::Gear, ERpgEquipmentSlot::Feet),
		MakeTestStaticGroup(URpgPlayerInventoryLayoutComponent::GearBackpackGroupId, NSLOCTEXT("RpgInventoryLayout", "GearBackpack", "Backpack"), 1, 1, {}, false, ERpgInventorySlotGroupKind::Gear, ERpgEquipmentSlot::Backpack),
		MakeTestStaticGroup(URpgPlayerInventoryLayoutComponent::GearBeltGroupId, NSLOCTEXT("RpgInventoryLayout", "GearBelt", "Belt"), 1, 1, {}, false, ERpgInventorySlotGroupKind::Gear, ERpgEquipmentSlot::Belt),
		MakeTestStaticGroup(URpgPlayerInventoryLayoutComponent::GearPouchGroupId, NSLOCTEXT("RpgInventoryLayout", "GearPouch", "Pouch"), 1, 1, {}, false, ERpgInventorySlotGroupKind::Gear, ERpgEquipmentSlot::Pouch),
		MakeTestStaticGroup(URpgPlayerInventoryLayoutComponent::GearResourceBagGroupId, NSLOCTEXT("RpgInventoryLayout", "GearResourceBag", "Resource Bag"), 1, 1, {}, false, ERpgInventorySlotGroupKind::Gear, ERpgEquipmentSlot::ResourceBag),
		MakeTestStaticGroup(URpgPlayerInventoryLayoutComponent::WeaponSlot1GroupId, NSLOCTEXT("RpgInventoryLayout", "WeaponSlot1", "Weapon 1"), 1, 1, { ERpgInventoryItemCategory::Weapon }, true, ERpgInventorySlotGroupKind::Carry, ERpgEquipmentSlot::MainHand, RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Primary),
		MakeTestStaticGroup(URpgPlayerInventoryLayoutComponent::WeaponSlot2GroupId, NSLOCTEXT("RpgInventoryLayout", "WeaponSlot2", "Weapon 2"), 1, 1, { ERpgInventoryItemCategory::Weapon }, true, ERpgInventorySlotGroupKind::Carry, ERpgEquipmentSlot::MainHand, RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Secondary),
		MakeTestStaticGroup(URpgPlayerInventoryLayoutComponent::ShieldSlotGroupId, NSLOCTEXT("RpgInventoryLayout", "ShieldSlot", "Shield"), 1, 1, { ERpgInventoryItemCategory::Shield }, true, ERpgInventorySlotGroupKind::Carry, ERpgEquipmentSlot::OffHand, RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_OffHand),
		MakeTestStaticGroup(URpgPlayerInventoryLayoutComponent::PocketsGroupId, NSLOCTEXT("RpgInventoryLayout", "Pockets", "Pockets"), 4, 2, {}, true, ERpgInventorySlotGroupKind::Content, ERpgEquipmentSlot::None, RpgGameplayTags::Rpg_Inventory_Layout_Role_Content_Primary)
	};

	TestPawnData = NewObject<URpgPawnData>(
		this,
		TEXT("InventoryAutomationPawnData"),
		RF_Transient);
	check(TestPawnData);
	TestPawnData->InventoryLayoutDefinition = TestInventoryLayoutDefinition;

	// Manually spawned PlayerState fixtures can still have ROLE_None while
	// PostActorCreated runs. Inject the fixture-owned static data directly;
	// production PawnData lifecycle and delegate behavior are covered separately.
	PawnData = TestPawnData;
}
