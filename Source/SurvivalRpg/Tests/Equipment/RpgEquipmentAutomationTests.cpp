#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentRuleset.h"
#include "SurvivalRpg/GameplayTags/GameplayTags.h"
#include "SurvivalRpg/Items/RpgItemDefinition.h"
#include "SurvivalRpg/Items/RpgItemInstance.h"
#include "SurvivalRpg/Items/Fragments/RpgItemFragment_Equipment.h"
#include "SurvivalRpg/Items/Fragments/RpgItemFragment_Weapon.h"

namespace RpgEquipmentAutomationTests
{
	static FGameplayTag Tag(const TCHAR* TagName)
	{
		return FGameplayTag::RequestGameplayTag(FName(TagName));
	}

	static URpgEquipmentRuleset* CreateRuleset()
	{
		URpgEquipmentRuleset* Ruleset = NewObject<URpgEquipmentRuleset>(GetTransientPackage());
		Ruleset->SetNumWeaponSets(2);
		Ruleset->SetLimitTwoHandedWeaponsToOne(true);
		Ruleset->SetAllowOffHandWithoutMainHand(false);
		return Ruleset;
	}

	static URpgItemInstance* CreateWeapon(
		UObject* Outer,
		FGameplayTag WeaponFamilyTag,
		FGameplayTag HandUsageTag,
		const FGameplayTagContainer& TraitTags = FGameplayTagContainer(),
		const URpgAbilitySet* ActiveAbilitySet = nullptr,
		const FGameplayTagContainer& ActiveLooseTags = FGameplayTagContainer())
	{
		URpgItemDefinition* ItemDefinition = NewObject<URpgItemDefinition>(Outer);

		URpgItemFragment_Equipment* EquipmentFragment = NewObject<URpgItemFragment_Equipment>(ItemDefinition);
		ItemDefinition->AddFragment(EquipmentFragment);

		URpgItemFragment_Weapon* WeaponFragment = NewObject<URpgItemFragment_Weapon>(ItemDefinition);
		WeaponFragment->SetWeaponFamilyTag(WeaponFamilyTag);
		WeaponFragment->SetHandUsageTag(HandUsageTag);
		WeaponFragment->SetEquipmentTraitTags(TraitTags);
		if (ActiveAbilitySet != nullptr)
		{
			WeaponFragment->AddActiveAbilitySet(ActiveAbilitySet);
		}
		if (!ActiveLooseTags.IsEmpty())
		{
			WeaponFragment->SetActiveLooseTags(ActiveLooseTags);
		}
		ItemDefinition->AddFragment(WeaponFragment);

		URpgItemInstance* ItemInstance = NewObject<URpgItemInstance>(Outer);
		ItemInstance->InitializeItemInstance(ItemDefinition, FRpgItemSourceHandle(), FMath::Rand());
		return ItemInstance;
	}

	static URpgEquipmentComponent* CreateEquipmentComponent(const URpgEquipmentRuleset* Ruleset, URpgAbilitySystemComponent* AbilitySystemComponent = nullptr)
	{
		URpgEquipmentComponent* EquipmentComponent = NewObject<URpgEquipmentComponent>(GetTransientPackage());
		EquipmentComponent->SetEquipmentRuleset(Ruleset);
#if WITH_DEV_AUTOMATION_TESTS
		if (AbilitySystemComponent != nullptr)
		{
			EquipmentComponent->SetAbilitySystemOverrideForTests(AbilitySystemComponent);
		}
#endif
		return EquipmentComponent;
	}

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgEquipmentTwoHandedCarryLimitTest,
	"SurvivalRpg.Items.Equipment.TwoHandedCarryLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgEquipmentTwoHandedCarryLimitTest::RunTest(const FString& Parameters)
{
	using namespace RpgEquipmentAutomationTests;

	URpgEquipmentRuleset* Ruleset = CreateRuleset();
	URpgEquipmentComponent* EquipmentComponent = CreateEquipmentComponent(Ruleset);

	URpgItemInstance* FirstTwoHander = CreateWeapon(GetTransientPackage(), Tag(TEXT("Weapon.Family.Bow")), RpgGameplayTags::Equipment_HandUsage_TwoHanded);
	URpgItemInstance* SecondTwoHander = CreateWeapon(GetTransientPackage(), Tag(TEXT("Weapon.Family.Bow")), RpgGameplayTags::Equipment_HandUsage_TwoHanded);

	TestTrue(TEXT("First two-handed weapon equips into weapon set 1."), EquipmentComponent->TryEquipItem(FirstTwoHander, RpgGameplayTags::Equipment_Slot_WeaponSet_1_MainHand));
	TestFalse(TEXT("Second two-handed weapon is blocked by the global carry rule."), EquipmentComponent->TryEquipItem(SecondTwoHander, RpgGameplayTags::Equipment_Slot_WeaponSet_2_MainHand));
	TestNull(TEXT("Weapon set 2 main hand remains empty when the second two-handed equip is rejected."), EquipmentComponent->GetItemInSlot(RpgGameplayTags::Equipment_Slot_WeaponSet_2_MainHand));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgEquipmentCompatibilityRulesTest,
	"SurvivalRpg.Items.Equipment.CompatibilityRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgEquipmentCompatibilityRulesTest::RunTest(const FString& Parameters)
{
	using namespace RpgEquipmentAutomationTests;

	URpgEquipmentRuleset* Ruleset = CreateRuleset();
	Ruleset->AddAllowedPairing(Tag(TEXT("Weapon.Family.Dagger")), Tag(TEXT("Weapon.Family.Dagger")));
	Ruleset->AddAllowedPairing(Tag(TEXT("Weapon.Family.Sword")), Tag(TEXT("Weapon.Family.Shield")));
	Ruleset->AddAllowedPairing(Tag(TEXT("Weapon.Family.Wand")), Tag(TEXT("Weapon.Family.Shield")));

	{
		URpgEquipmentComponent* EquipmentComponent = CreateEquipmentComponent(Ruleset);
		URpgItemInstance* MainDagger = CreateWeapon(GetTransientPackage(), Tag(TEXT("Weapon.Family.Dagger")), RpgGameplayTags::Equipment_HandUsage_EitherHand);
		URpgItemInstance* OffDagger = CreateWeapon(GetTransientPackage(), Tag(TEXT("Weapon.Family.Dagger")), RpgGameplayTags::Equipment_HandUsage_EitherHand);

		TestTrue(TEXT("Dual daggers allow a main-hand dagger."), EquipmentComponent->TryEquipItem(MainDagger, RpgGameplayTags::Equipment_Slot_WeaponSet_1_MainHand));
		TestTrue(TEXT("Dual daggers allow an off-hand dagger."), EquipmentComponent->TryEquipItem(OffDagger, RpgGameplayTags::Equipment_Slot_WeaponSet_1_OffHand));
	}

	{
		URpgEquipmentComponent* EquipmentComponent = CreateEquipmentComponent(Ruleset);
		FGameplayTagContainer ShieldTraits;
		ShieldTraits.AddTag(RpgGameplayTags::Equipment_Trait_Shield);

		URpgItemInstance* Sword = CreateWeapon(GetTransientPackage(), Tag(TEXT("Weapon.Family.Sword")), RpgGameplayTags::Equipment_HandUsage_MainHand);
		URpgItemInstance* Shield = CreateWeapon(GetTransientPackage(), Tag(TEXT("Weapon.Family.Shield")), RpgGameplayTags::Equipment_HandUsage_OffHand, ShieldTraits);

		TestTrue(TEXT("Sword equips into main hand."), EquipmentComponent->TryEquipItem(Sword, RpgGameplayTags::Equipment_Slot_WeaponSet_1_MainHand));
		TestTrue(TEXT("Shield is compatible with sword in the off hand."), EquipmentComponent->TryEquipItem(Shield, RpgGameplayTags::Equipment_Slot_WeaponSet_1_OffHand));
	}

	{
		URpgEquipmentComponent* EquipmentComponent = CreateEquipmentComponent(Ruleset);
		URpgItemInstance* Sword = CreateWeapon(GetTransientPackage(), Tag(TEXT("Weapon.Family.Sword")), RpgGameplayTags::Equipment_HandUsage_MainHand);
		URpgItemInstance* Dagger = CreateWeapon(GetTransientPackage(), Tag(TEXT("Weapon.Family.Dagger")), RpgGameplayTags::Equipment_HandUsage_EitherHand);

		TestTrue(TEXT("Sword equips into main hand for the incompatible pair test."), EquipmentComponent->TryEquipItem(Sword, RpgGameplayTags::Equipment_Slot_WeaponSet_1_MainHand));
		TestFalse(TEXT("Sword plus off-hand dagger is rejected without an explicit pairing rule."), EquipmentComponent->TryEquipItem(Dagger, RpgGameplayTags::Equipment_Slot_WeaponSet_1_OffHand));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgEquipmentWeaponSetGrantSwitchTest,
	"SurvivalRpg.Items.Equipment.WeaponSetGrantSwitch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgEquipmentWeaponSetGrantSwitchTest::RunTest(const FString& Parameters)
{
	using namespace RpgEquipmentAutomationTests;

	URpgEquipmentRuleset* Ruleset = CreateRuleset();
	URpgAbilitySystemComponent* AbilitySystemComponent = NewObject<URpgAbilitySystemComponent>(GetTransientPackage());
	AbilitySystemComponent->SetForceGrantAuthorityForTests(true);

	URpgEquipmentComponent* EquipmentComponent = CreateEquipmentComponent(Ruleset, AbilitySystemComponent);

	FGameplayTagContainer FirstSetLooseTags;
	FirstSetLooseTags.AddTag(RpgGameplayTags::InputTag_WeaponSet_1);

	FGameplayTagContainer SecondSetLooseTags;
	SecondSetLooseTags.AddTag(RpgGameplayTags::InputTag_WeaponSet_2);

	URpgItemInstance* FirstWeapon = CreateWeapon(
		GetTransientPackage(),
		Tag(TEXT("Weapon.Family.Sword")),
		RpgGameplayTags::Equipment_HandUsage_MainHand,
		FGameplayTagContainer(),
		nullptr,
		FirstSetLooseTags);

	URpgItemInstance* SecondWeapon = CreateWeapon(
		GetTransientPackage(),
		Tag(TEXT("Weapon.Family.Wand")),
		RpgGameplayTags::Equipment_HandUsage_MainHand,
		FGameplayTagContainer(),
		nullptr,
		SecondSetLooseTags);

	TestTrue(TEXT("Weapon set 1 weapon equips."), EquipmentComponent->TryEquipItem(FirstWeapon, RpgGameplayTags::Equipment_Slot_WeaponSet_1_MainHand));
	TestTrue(TEXT("Weapon set 2 weapon equips."), EquipmentComponent->TryEquipItem(SecondWeapon, RpgGameplayTags::Equipment_Slot_WeaponSet_2_MainHand));

	TestTrue(TEXT("Only the active weapon set 1 loose tags are applied initially."), AbilitySystemComponent->HasMatchingGameplayTag(RpgGameplayTags::InputTag_WeaponSet_1));
	TestFalse(TEXT("Weapon set 2 loose tags are not active before switching."), AbilitySystemComponent->HasMatchingGameplayTag(RpgGameplayTags::InputTag_WeaponSet_2));

	TestTrue(TEXT("Switching to weapon set 2 succeeds."), EquipmentComponent->TryActivateWeaponSet(1));
	TestFalse(TEXT("Weapon set 1 loose tags are removed after the switch."), AbilitySystemComponent->HasMatchingGameplayTag(RpgGameplayTags::InputTag_WeaponSet_1));
	TestTrue(TEXT("Weapon set 2 loose tags are applied after the switch."), AbilitySystemComponent->HasMatchingGameplayTag(RpgGameplayTags::InputTag_WeaponSet_2));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgEquipmentItemIdentityPersistenceTest,
	"SurvivalRpg.Items.Equipment.ItemIdentityPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgEquipmentItemIdentityPersistenceTest::RunTest(const FString& Parameters)
{
	using namespace RpgEquipmentAutomationTests;

	URpgEquipmentRuleset* Ruleset = CreateRuleset();
	URpgEquipmentComponent* EquipmentComponent = CreateEquipmentComponent(Ruleset);

	URpgItemInstance* SourceItem = CreateWeapon(GetTransientPackage(), Tag(TEXT("Weapon.Family.Sword")), RpgGameplayTags::Equipment_HandUsage_MainHand);
	const FGuid OriginalId = SourceItem->GetInstanceId();
	const int32 OriginalSeed = SourceItem->GetRollSeed();

	URpgItemInstance* RegisteredItem = EquipmentComponent->RegisterExistingItemInstance(SourceItem);
	TestNotNull(TEXT("Registering an external item returns a managed instance."), RegisteredItem);
	TestTrue(TEXT("The managed item keeps the original instance id."), RegisteredItem && RegisteredItem->GetInstanceId() == OriginalId);
	TestEqual(TEXT("The managed item keeps the original roll seed."), RegisteredItem ? RegisteredItem->GetRollSeed() : INDEX_NONE, OriginalSeed);

	TestTrue(TEXT("The managed item can be equipped after registration."), EquipmentComponent->TryEquipItem(RegisteredItem, RpgGameplayTags::Equipment_Slot_WeaponSet_1_MainHand));
	TestTrue(TEXT("The equipped slot still references the same logical item id."), EquipmentComponent->GetItemInSlot(RpgGameplayTags::Equipment_Slot_WeaponSet_1_MainHand)->GetInstanceId() == OriginalId);

	TestTrue(TEXT("Unequipping the item succeeds."), EquipmentComponent->TryUnequipItem(RpgGameplayTags::Equipment_Slot_WeaponSet_1_MainHand));
	TestNull(TEXT("The main-hand slot is empty after unequip."), EquipmentComponent->GetItemInSlot(RpgGameplayTags::Equipment_Slot_WeaponSet_1_MainHand));

	URpgItemInstance* DuplicatedItem = RegisteredItem ? RegisteredItem->DuplicateItemInstance(GetTransientPackage()) : nullptr;
	TestNotNull(TEXT("Duplicating an item instance for transfer succeeds."), DuplicatedItem);
	TestTrue(TEXT("Duplicated item preserves the stable instance id."), DuplicatedItem && DuplicatedItem->GetInstanceId() == OriginalId);
	TestEqual(TEXT("Duplicated item preserves the roll seed."), DuplicatedItem ? DuplicatedItem->GetRollSeed() : INDEX_NONE, OriginalSeed);

	return true;
}

#endif
