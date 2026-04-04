#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Animation/AnimMontage.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Equipment/AnimNotify_RpgWeaponToolPresentation.h"
#include "SurvivalRpg/Equipment/RpgEquipmentComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentRuleset.h"
#include "SurvivalRpg/GameplayTags/GameplayTags.h"
#include "SurvivalRpg/Items/RpgItemDefinition.h"
#include "SurvivalRpg/Items/RpgItemInstance.h"
#include "SurvivalRpg/Items/Fragments/RpgItemFragment_Equipment.h"
#include "SurvivalRpg/Items/Fragments/RpgItemFragment_Visual.h"
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
		const FGameplayTagContainer& ActiveLooseTags = FGameplayTagContainer(),
		const FRpgWeaponToolCameraSettings& CameraSettings = FRpgWeaponToolCameraSettings(),
		const FRpgWeaponToolCharacterSettings& CharacterSettings = FRpgWeaponToolCharacterSettings(),
		UAnimMontage* EquipMontage = nullptr,
		UAnimMontage* UnequipMontage = nullptr)
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

		if (CameraSettings.bEnabled || CharacterSettings.bEnabled || EquipMontage != nullptr || UnequipMontage != nullptr)
		{
			URpgItemFragment_Visual* VisualFragment = NewObject<URpgItemFragment_Visual>(ItemDefinition);
			VisualFragment->SetWeaponToolCameraSettings(CameraSettings);
			VisualFragment->SetWeaponToolCharacterSettings(CharacterSettings);
			VisualFragment->SetEquipMontage(EquipMontage);
			VisualFragment->SetUnequipMontage(UnequipMontage);
			ItemDefinition->AddFragment(VisualFragment);
		}

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

	TestEqual(TEXT("No weapon set starts active by default."), EquipmentComponent->GetActiveWeaponSetIndex(), INDEX_NONE);
	TestNull(TEXT("The active weapon set is empty while everything is holstered."), EquipmentComponent->GetActiveWeaponSet().MainHandItem);
	TestFalse(TEXT("Weapon set 1 loose tags stay inactive while nothing is drawn."), AbilitySystemComponent->HasMatchingGameplayTag(RpgGameplayTags::InputTag_WeaponSet_1));
	TestFalse(TEXT("Weapon set 2 loose tags stay inactive while nothing is drawn."), AbilitySystemComponent->HasMatchingGameplayTag(RpgGameplayTags::InputTag_WeaponSet_2));

	TestTrue(TEXT("Activating weapon set 1 succeeds."), EquipmentComponent->TryActivateWeaponSet(0));
	TestEqual(TEXT("Weapon set 1 becomes the active set."), EquipmentComponent->GetActiveWeaponSetIndex(), 0);
	TestTrue(TEXT("Weapon set 1 loose tags are applied after activation."), AbilitySystemComponent->HasMatchingGameplayTag(RpgGameplayTags::InputTag_WeaponSet_1));
	TestFalse(TEXT("Weapon set 2 loose tags are still inactive after activating set 1."), AbilitySystemComponent->HasMatchingGameplayTag(RpgGameplayTags::InputTag_WeaponSet_2));

	TestTrue(TEXT("Pressing the same slot again holsters the currently active set."), EquipmentComponent->TryActivateWeaponSet(0));
	TestEqual(TEXT("Holstering clears the active weapon set index."), EquipmentComponent->GetActiveWeaponSetIndex(), INDEX_NONE);
	TestNull(TEXT("The active weapon set reports as empty when holstered."), EquipmentComponent->GetActiveWeaponSet().MainHandItem);
	TestFalse(TEXT("Weapon set 1 loose tags are removed while holstered."), AbilitySystemComponent->HasMatchingGameplayTag(RpgGameplayTags::InputTag_WeaponSet_1));
	TestFalse(TEXT("Weapon set 2 loose tags remain inactive while holstered."), AbilitySystemComponent->HasMatchingGameplayTag(RpgGameplayTags::InputTag_WeaponSet_2));
	TestNotNull(TEXT("Holstering does not remove the item from slot 1 main hand."), EquipmentComponent->GetItemInSlot(RpgGameplayTags::Equipment_Slot_WeaponSet_1_MainHand));

	TestTrue(TEXT("Activating weapon set 1 a second time redraws the same set."), EquipmentComponent->TryActivateWeaponSet(0));
	TestTrue(TEXT("Weapon set 1 loose tags are re-applied after redrawing."), AbilitySystemComponent->HasMatchingGameplayTag(RpgGameplayTags::InputTag_WeaponSet_1));

	TestTrue(TEXT("Switching to weapon set 2 succeeds."), EquipmentComponent->TryActivateWeaponSet(1));
	TestEqual(TEXT("Weapon set 2 becomes the active set after the switch."), EquipmentComponent->GetActiveWeaponSetIndex(), 1);
	TestFalse(TEXT("Weapon set 1 loose tags are removed after the switch."), AbilitySystemComponent->HasMatchingGameplayTag(RpgGameplayTags::InputTag_WeaponSet_1));
	TestTrue(TEXT("Weapon set 2 loose tags are applied after the switch."), AbilitySystemComponent->HasMatchingGameplayTag(RpgGameplayTags::InputTag_WeaponSet_2));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgEquipmentAutoEquipHolsteredOrderTest,
	"SurvivalRpg.Items.Equipment.AutoEquipHolsteredOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgEquipmentAutoEquipHolsteredOrderTest::RunTest(const FString& Parameters)
{
	using namespace RpgEquipmentAutomationTests;

	URpgEquipmentRuleset* Ruleset = CreateRuleset();
	Ruleset->AddAllowedPairing(Tag(TEXT("Weapon.Family.Sword")), Tag(TEXT("Weapon.Family.Shield")));

	URpgEquipmentComponent* EquipmentComponent = CreateEquipmentComponent(Ruleset);
	URpgItemInstance* Sword = CreateWeapon(GetTransientPackage(), Tag(TEXT("Weapon.Family.Sword")), RpgGameplayTags::Equipment_HandUsage_MainHand);

	FGameplayTagContainer ShieldTraits;
	ShieldTraits.AddTag(RpgGameplayTags::Equipment_Trait_Shield);
	URpgItemInstance* Shield = CreateWeapon(GetTransientPackage(), Tag(TEXT("Weapon.Family.Shield")), RpgGameplayTags::Equipment_HandUsage_OffHand, ShieldTraits);
	URpgItemInstance* SecondSword = CreateWeapon(GetTransientPackage(), Tag(TEXT("Weapon.Family.Sword")), RpgGameplayTags::Equipment_HandUsage_MainHand);

	TestEqual(TEXT("No set starts active for auto-equip ordering."), EquipmentComponent->GetActiveWeaponSetIndex(), INDEX_NONE);
	TestTrue(TEXT("The first main-hand weapon auto-equips into weapon set 1 main hand."), EquipmentComponent->TryAutoEquipItem(Sword));
	URpgItemInstance* FirstMainHandItem = EquipmentComponent->GetItemInSlot(RpgGameplayTags::Equipment_Slot_WeaponSet_1_MainHand);
	TestNotNull(TEXT("Weapon set 1 main hand now holds an item."), FirstMainHandItem);
	TestTrue(TEXT("The first sword lands in weapon set 1 main hand."), FirstMainHandItem != nullptr && FirstMainHandItem->GetInstanceId() == Sword->GetInstanceId());

	TestTrue(TEXT("An off-hand shield prefers weapon set 1 off hand before set 2 main hand."), EquipmentComponent->TryAutoEquipItem(Shield));
	URpgItemInstance* FirstOffHandItem = EquipmentComponent->GetItemInSlot(RpgGameplayTags::Equipment_Slot_WeaponSet_1_OffHand);
	TestNotNull(TEXT("Weapon set 1 off hand now holds an item."), FirstOffHandItem);
	TestTrue(TEXT("The shield lands in weapon set 1 off hand."), FirstOffHandItem != nullptr && FirstOffHandItem->GetInstanceId() == Shield->GetInstanceId());

	TestTrue(TEXT("A second main-hand weapon then falls through to weapon set 2 main hand."), EquipmentComponent->TryAutoEquipItem(SecondSword));
	URpgItemInstance* SecondMainHandItem = EquipmentComponent->GetItemInSlot(RpgGameplayTags::Equipment_Slot_WeaponSet_2_MainHand);
	TestNotNull(TEXT("Weapon set 2 main hand now holds an item."), SecondMainHandItem);
	TestTrue(TEXT("The second sword lands in weapon set 2 main hand."), SecondMainHandItem != nullptr && SecondMainHandItem->GetInstanceId() == SecondSword->GetInstanceId());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgEquipmentActiveCameraSettingsTest,
	"SurvivalRpg.Items.Equipment.ActiveCameraSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgEquipmentActiveCameraSettingsTest::RunTest(const FString& Parameters)
{
	using namespace RpgEquipmentAutomationTests;

	URpgEquipmentRuleset* Ruleset = CreateRuleset();
	URpgEquipmentComponent* EquipmentComponent = CreateEquipmentComponent(Ruleset);
	FRpgWeaponToolCameraSettings FirstCameraSettings;
	FirstCameraSettings.bEnabled = true;
	FirstCameraSettings.FOV = 72.0f;
	FirstCameraSettings.SpringArmSocketOffset = FVector(0.0f, 55.0f, 12.0f);
	FirstCameraSettings.BlendTime = 0.2f;

	FRpgWeaponToolCameraSettings SecondCameraSettings;
	SecondCameraSettings.bEnabled = true;
	SecondCameraSettings.FOV = 64.0f;
	SecondCameraSettings.SpringArmSocketOffset = FVector(0.0f, -40.0f, 8.0f);
	SecondCameraSettings.BlendTime = 0.35f;

	URpgItemInstance* FirstWeapon = CreateWeapon(
		GetTransientPackage(),
		Tag(TEXT("Weapon.Family.Sword")),
		RpgGameplayTags::Equipment_HandUsage_MainHand,
		FGameplayTagContainer(),
		nullptr,
		FGameplayTagContainer(),
		FirstCameraSettings);

	URpgItemInstance* SecondWeapon = CreateWeapon(
		GetTransientPackage(),
		Tag(TEXT("Weapon.Family.Wand")),
		RpgGameplayTags::Equipment_HandUsage_MainHand,
		FGameplayTagContainer(),
		nullptr,
		FGameplayTagContainer(),
		SecondCameraSettings);

	TestTrue(TEXT("Weapon set 1 weapon equips for the camera settings test."), EquipmentComponent->TryEquipItem(FirstWeapon, RpgGameplayTags::Equipment_Slot_WeaponSet_1_MainHand));
	TestTrue(TEXT("Weapon set 2 weapon equips for the camera settings test."), EquipmentComponent->TryEquipItem(SecondWeapon, RpgGameplayTags::Equipment_Slot_WeaponSet_2_MainHand));
	TestFalse(TEXT("Camera settings stay disabled while everything is holstered."), EquipmentComponent->GetActiveCameraSettings().bEnabled);

	TestTrue(TEXT("Activating weapon set 1 succeeds for the camera settings test."), EquipmentComponent->TryActivateWeaponSet(0));
	TestTrue(TEXT("Weapon set 1 exposes its inline camera settings when active."), EquipmentComponent->GetActiveCameraSettings() == FirstCameraSettings);

	TestTrue(TEXT("Holstering weapon set 1 succeeds for the camera settings test."), EquipmentComponent->TryActivateWeaponSet(0));
	TestFalse(TEXT("Holstering clears the active camera settings override."), EquipmentComponent->GetActiveCameraSettings().bEnabled);

	TestTrue(TEXT("Activating weapon set 2 succeeds for the camera settings test."), EquipmentComponent->TryActivateWeaponSet(1));
	TestTrue(TEXT("Weapon set 2 exposes its inline camera settings when active."), EquipmentComponent->GetActiveCameraSettings() == SecondCameraSettings);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgEquipmentActiveCharacterSettingsTest,
	"SurvivalRpg.Items.Equipment.ActiveCharacterSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgEquipmentActiveCharacterSettingsTest::RunTest(const FString& Parameters)
{
	using namespace RpgEquipmentAutomationTests;

	URpgEquipmentRuleset* Ruleset = CreateRuleset();
	URpgEquipmentComponent* EquipmentComponent = CreateEquipmentComponent(Ruleset);

	FRpgWeaponToolCharacterSettings FirstCharacterSettings;
	FirstCharacterSettings.bEnabled = true;
	FirstCharacterSettings.MaxWalkSpeed = 430.0f;
	FirstCharacterSettings.bOrientRotationToMovement = false;
	FirstCharacterSettings.bUseControllerDesiredRotation = true;

	FRpgWeaponToolCharacterSettings SecondCharacterSettings;
	SecondCharacterSettings.bEnabled = true;
	SecondCharacterSettings.MaxWalkSpeed = 320.0f;
	SecondCharacterSettings.bOrientRotationToMovement = true;
	SecondCharacterSettings.bUseControllerDesiredRotation = false;

	URpgItemInstance* FirstWeapon = CreateWeapon(
		GetTransientPackage(),
		Tag(TEXT("Weapon.Family.Sword")),
		RpgGameplayTags::Equipment_HandUsage_MainHand,
		FGameplayTagContainer(),
		nullptr,
		FGameplayTagContainer(),
		FRpgWeaponToolCameraSettings(),
		FirstCharacterSettings);

	URpgItemInstance* SecondWeapon = CreateWeapon(
		GetTransientPackage(),
		Tag(TEXT("Weapon.Family.Wand")),
		RpgGameplayTags::Equipment_HandUsage_MainHand,
		FGameplayTagContainer(),
		nullptr,
		FGameplayTagContainer(),
		FRpgWeaponToolCameraSettings(),
		SecondCharacterSettings);

	TestTrue(TEXT("Weapon set 1 weapon equips for the character settings test."), EquipmentComponent->TryEquipItem(FirstWeapon, RpgGameplayTags::Equipment_Slot_WeaponSet_1_MainHand));
	TestTrue(TEXT("Weapon set 2 weapon equips for the character settings test."), EquipmentComponent->TryEquipItem(SecondWeapon, RpgGameplayTags::Equipment_Slot_WeaponSet_2_MainHand));
	TestFalse(TEXT("Character settings stay disabled while everything is holstered."), EquipmentComponent->GetActiveWeaponToolCharacterSettings().bEnabled);

	TestTrue(TEXT("Activating weapon set 1 succeeds for the character settings test."), EquipmentComponent->TryActivateWeaponSet(0));
	TestTrue(TEXT("Weapon set 1 exposes its inline character settings when active."), EquipmentComponent->GetActiveWeaponToolCharacterSettings() == FirstCharacterSettings);

	TestTrue(TEXT("Holstering weapon set 1 succeeds for the character settings test."), EquipmentComponent->TryActivateWeaponSet(0));
	TestFalse(TEXT("Holstering clears the active character settings override."), EquipmentComponent->GetActiveWeaponToolCharacterSettings().bEnabled);

	TestTrue(TEXT("Activating weapon set 2 succeeds for the character settings test."), EquipmentComponent->TryActivateWeaponSet(1));
	TestTrue(TEXT("Weapon set 2 exposes its inline character settings when active."), EquipmentComponent->GetActiveWeaponToolCharacterSettings() == SecondCharacterSettings);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgEquipmentWeaponToolPresentationNotifyDetectionTest,
	"SurvivalRpg.Items.Equipment.WeaponToolPresentationNotifyDetection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgEquipmentWeaponToolPresentationNotifyDetectionTest::RunTest(const FString& Parameters)
{
	using namespace RpgEquipmentAutomationTests;

	URpgEquipmentRuleset* Ruleset = CreateRuleset();
	URpgEquipmentComponent* EquipmentComponent = CreateEquipmentComponent(Ruleset);

	UAnimMontage* MontageWithNewNotify = NewObject<UAnimMontage>(GetTransientPackage());
	FAnimNotifyEvent& NewNotifyEvent = MontageWithNewNotify->Notifies.AddDefaulted_GetRef();
	NewNotifyEvent.Notify = NewObject<UAnimNotify_RpgWeaponToolPresentation>(MontageWithNewNotify);

	UAnimMontage* MontageWithoutNotify = NewObject<UAnimMontage>(GetTransientPackage());

	URpgItemInstance* WeaponWithNewNotify = CreateWeapon(
		GetTransientPackage(),
		Tag(TEXT("Weapon.Family.Sword")),
		RpgGameplayTags::Equipment_HandUsage_MainHand,
		FGameplayTagContainer(),
		nullptr,
		FGameplayTagContainer(),
		FRpgWeaponToolCameraSettings(),
		FRpgWeaponToolCharacterSettings(),
		MontageWithNewNotify);

	URpgItemInstance* WeaponWithoutNotify = CreateWeapon(
		GetTransientPackage(),
		Tag(TEXT("Weapon.Family.Dagger")),
		RpgGameplayTags::Equipment_HandUsage_MainHand,
		FGameplayTagContainer(),
		nullptr,
		FGameplayTagContainer(),
		FRpgWeaponToolCameraSettings(),
		FRpgWeaponToolCharacterSettings(),
		MontageWithoutNotify);

	TestTrue(TEXT("Weapon with a new weapon-tool presentation notify equips into set 1."), EquipmentComponent->TryEquipItem(WeaponWithNewNotify, RpgGameplayTags::Equipment_Slot_WeaponSet_1_MainHand));
	TestTrue(TEXT("The new weapon-tool notify is detected on the equip montage."), EquipmentComponent->UsesWeaponToolPresentationNotifyForTests(0, true));

	TestTrue(TEXT("Replacing the set 1 weapon with one that has no presentation notify succeeds."), EquipmentComponent->TryEquipItem(WeaponWithoutNotify, RpgGameplayTags::Equipment_Slot_WeaponSet_1_MainHand));
	TestFalse(TEXT("Montages without a weapon-tool presentation notify fall back to immediate presentation updates."), EquipmentComponent->UsesWeaponToolPresentationNotifyForTests(0, true));

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgItemInstanceStatTagStackTest,
	"SurvivalRpg.Items.ItemInstance.StatTagStacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgItemInstanceStatTagStackTest::RunTest(const FString& Parameters)
{
	using namespace RpgEquipmentAutomationTests;

	URpgItemDefinition* ItemDefinition = NewObject<URpgItemDefinition>(GetTransientPackage());
	URpgItemInstance* ItemInstance = NewObject<URpgItemInstance>(GetTransientPackage());
	ItemInstance->InitializeItemInstance(ItemDefinition, FRpgItemSourceHandle(), 1337);

	const FGameplayTag StrengthTag = Tag(TEXT("InputTag.WeaponSet.1"));
	const FGameplayTag ShieldTag = Tag(TEXT("Equipment.Trait.Shield"));

	ItemInstance->AddStatTagStack(StrengthTag, 2);
	ItemInstance->AddStatTagStack(StrengthTag, 3);
	TestEqual(TEXT("Adding the same tag twice accumulates its stack count."), ItemInstance->GetStatTagStackCount(StrengthTag), 5);
	TestTrue(TEXT("The item reports a tag as present when its stack count is above zero."), ItemInstance->HasStatTag(StrengthTag));

	ItemInstance->SetStatTagStackCount(StrengthTag, 1);
	TestEqual(TEXT("Setting the stack count replaces the previous amount."), ItemInstance->GetStatTagStackCount(StrengthTag), 1);

	ItemInstance->AddStatTagStack(ShieldTag, 4);
	TestEqual(TEXT("A second tag stack is tracked independently."), ItemInstance->GetStatTagStackCount(ShieldTag), 4);

	ItemInstance->RemoveStatTagStack(StrengthTag, 1);
	TestEqual(TEXT("Removing the final stack clears the original tag."), ItemInstance->GetStatTagStackCount(StrengthTag), 0);
	TestFalse(TEXT("The cleared tag is no longer reported as present."), ItemInstance->HasStatTag(StrengthTag));

	URpgItemInstance* DuplicatedItem = ItemInstance->DuplicateItemInstance(GetTransientPackage());
	TestNotNull(TEXT("Duplicating an item with fast-array tag stacks succeeds."), DuplicatedItem);
	TestEqual(TEXT("Duplicated items preserve the remaining shield stack count."), DuplicatedItem ? DuplicatedItem->GetStatTagStackCount(ShieldTag) : INDEX_NONE, 4);
	TestTrue(TEXT("Duplicated items still answer tag presence queries correctly."), DuplicatedItem && DuplicatedItem->HasStatTag(ShieldTag));

	return true;
}

#endif
