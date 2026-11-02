#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "InputAction.h"
#include "Misc/AutomationTest.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/Combat/RpgCombatProjectile.h"
#include "SurvivalRpg/Combat/RpgProjectileShot.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Equipment/RpgRangedWeaponInstance.h"
#include "SurvivalRpg/Input/RpgInputConfig.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_Ammunition.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_EquippableItem.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_SkillTree.h"
#include "UObject/UnrealType.h"

namespace RpgBowAssetContractTests
{
	const TCHAR* BowPath = TEXT("/GF_Combat_Ranged/Items/ID_Bow.ID_Bow_C");
	const TCHAR* ArrowPath = TEXT("/GF_Combat_Ranged/Items/ID_Arrow_Standard.ID_Arrow_Standard_C");
	const TCHAR* ArrowProjectilePath = TEXT("/GF_Combat_Ranged/Projectiles/BP_Projectile_Arrow.BP_Projectile_Arrow_C");
	const TCHAR* CombatInputConfigPath = TEXT("/GF_Combat_Core/Input/DA_InputConfig_Combat.DA_InputConfig_Combat");

	template <typename FragmentType>
	const FragmentType* FindFragment(const URpgInventoryItemDefinition* Definition)
	{
		return Definition ? Cast<FragmentType>(Definition->FindFragmentByClass(FragmentType::StaticClass())) : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgBowAssetContractTest,
	"SurvivalRpg.Combat.Ranged.BowAssetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgBowAssetContractTest::RunTest(const FString& Parameters)
{
	using namespace RpgBowAssetContractTests;

	const UClass* BowClass = LoadClass<URpgInventoryItemDefinition>(nullptr, BowPath);
	if (!TestNotNull(TEXT("The bow item loads"), BowClass)) return false;
	const URpgInventoryItemDefinition* Bow = BowClass->GetDefaultObject<URpgInventoryItemDefinition>();
	TestNull(TEXT("The first bow carries no combat skill tree, so melee tunings never reach it"),
		FindFragment<URpgInventoryFragment_SkillTree>(Bow));
	const URpgInventoryFragment_ItemTraits* BowTraits = FindFragment<URpgInventoryFragment_ItemTraits>(Bow);
	TestTrue(TEXT("The bow is a single weapon item, not a material"),
		BowTraits && BowTraits->ItemCategory == ERpgInventoryItemCategory::Weapon && !BowTraits->bCanStack && !BowTraits->bTreatAsMaterial);

	const URpgInventoryFragment_EquippableItem* Equippable = FindFragment<URpgInventoryFragment_EquippableItem>(Bow);
	const URpgEquipmentDefinition* Equipment = Equippable && Equippable->EquipmentDefinition
		? Equippable->EquipmentDefinition->GetDefaultObject<URpgEquipmentDefinition>()
		: nullptr;
	if (!TestNotNull(TEXT("The bow equips through an equipment definition"), Equipment)) return false;
	TestEqual(TEXT("The bow takes both hands, so no shield pairs with it"), Equipment->HandOccupancy, ERpgEquipmentHandOccupancy::BothHands);
	TestTrue(TEXT("The bow equips only in the main hand"),
		Equipment->AllowedSlots.Num() == 1 && Equipment->AllowedSlots[0] == ERpgEquipmentSlot::MainHand);
	TestTrue(TEXT("The bow's hand rule is valid"), Equipment->HasValidHandOccupancyContract());

	const URpgRangedWeaponInstance* Weapon = Equipment->InstanceType
		? Cast<URpgRangedWeaponInstance>(Equipment->InstanceType->GetDefaultObject())
		: nullptr;
	if (!TestNotNull(TEXT("The bow's equipment instance is a ranged weapon"), Weapon)) return false;
	TestEqual(TEXT("The bow is of the bow family"), Weapon->GetWeaponFamilyTag(),
		FGameplayTag::RequestGameplayTag(TEXT("Weapon.Family.Bow")));
	TestFalse(TEXT("The bow cannot block"), Weapon->CanBlock());
	TestTrue(TEXT("The bow has no melee attack"), Weapon->GetAttackDefinitionTags().IsEmpty());
	TArray<FText> ShotErrors;
	RpgProjectileShot::ValidateDefinition(Weapon->GetShotDefinition(), ShotErrors);
	TestTrue(TEXT("The bow's shot definition is valid"), ShotErrors.IsEmpty());
	TestEqual(TEXT("The bow shoots the standard arrow"), Weapon->GetShotDefinition().AmmunitionItemDefinition.Get(),
		LoadClass<URpgInventoryItemDefinition>(nullptr, ArrowPath));

	// The main hand grants exactly the bow's shot on the primary attack input.
	TArray<const FRpgAbilitySet_GameplayAbility*> Shots;
	for (const FRpgEquipmentSlotAbilitySet& SlotSet : Equipment->SlotAbilitySetsToGrant)
	{
		if (SlotSet.EquippedSlot == ERpgEquipmentSlot::MainHand && SlotSet.AbilitySet)
		{
			for (const FRpgAbilitySet_GameplayAbility& Grant : SlotSet.AbilitySet->GetGrantedGameplayAbilities())
			{
				Shots.Add(&Grant);
			}
		}
	}
	if (!TestEqual(TEXT("The bow grants one ability"), Shots.Num(), 1)) return false;
	const UClass* ShotClass = Shots[0]->Ability.Get();
	TestTrue(TEXT("The bow's shot is a Blueprint ability on the project's ability base"),
		ShotClass && ShotClass->IsChildOf(URpgGameplayAbility::StaticClass()) && !ShotClass->IsNative());
	TestEqual(TEXT("The shot is on the primary attack input"), Shots[0]->InputTag,
		FGameplayTag::RequestGameplayTag(TEXT("InputTag.Weapon.Primary")));
	TestEqual(TEXT("The shot has its stable ability id for ranged tunings"), Shots[0]->AbilityIdTag,
		FGameplayTag::RequestGameplayTag(TEXT("Ability.Ranged.BowShot")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgArrowAssetContractTest,
	"SurvivalRpg.Combat.Ranged.ArrowAssetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgArrowAssetContractTest::RunTest(const FString& Parameters)
{
	using namespace RpgBowAssetContractTests;

	UClass* ArrowClass = LoadClass<URpgInventoryItemDefinition>(nullptr, ArrowPath);
	if (!TestNotNull(TEXT("The standard arrow loads"), ArrowClass)) return false;
	const URpgInventoryItemDefinition* Arrow = ArrowClass->GetDefaultObject<URpgInventoryItemDefinition>();
	const URpgInventoryFragment_ItemTraits* Traits = FindFragment<URpgInventoryFragment_ItemTraits>(Arrow);
	TestTrue(TEXT("Arrows are ordinary stackable ammunition, never a crafting material"),
		Traits && Traits->bCanStack && Traits->MaxStackSize > 1 && !Traits->bTreatAsMaterial);

	const TSubclassOf<ARpgCombatProjectile> ProjectileClass = URpgInventoryFragment_Ammunition::FindProjectileClass(ArrowClass);
	TestEqual(TEXT("The arrow launches the arrow projectile"), ProjectileClass.Get(),
		LoadClass<ARpgCombatProjectile>(nullptr, ArrowProjectilePath));
	const ARpgCombatProjectile* Projectile = ProjectileClass ? ProjectileClass->GetDefaultObject<ARpgCombatProjectile>() : nullptr;
	if (!TestNotNull(TEXT("The arrow projectile loads"), Projectile)) return false;
	TestEqual(TEXT("A standard arrow is recoverable"), Projectile->GetImpactRecovery(), ERpgProjectileImpactRecovery::Recoverable);
	const FFloatProperty* Gravity = FindFProperty<FFloatProperty>(ProjectileClass, TEXT("GravityScale"));
	TestTrue(TEXT("An arrow falls, so the player judges its drop"), Gravity && Gravity->GetPropertyValue_InContainer(Projectile) > 0.0f);
	TestNotNull(TEXT("The arrow projectile applies damage"), Projectile->GetDamageEffectClass().Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgPrimaryInputHoldContractTest,
	"SurvivalRpg.Combat.Ranged.PrimaryInputHoldsUntilRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPrimaryInputHoldContractTest::RunTest(const FString& Parameters)
{
	using namespace RpgBowAssetContractTests;

	// The bow draws while the primary attack is held and shoots on its release, and a click is still one melee press.
	const URpgInputConfig* Config = LoadObject<URpgInputConfig>(nullptr, CombatInputConfigPath);
	if (!TestNotNull(TEXT("The combat input config loads"), Config)) return false;
	const FGameplayTag PrimaryTag = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Weapon.Primary"));
	const FRpgInputAction* Primary = Config->AbilityInputActions.FindByPredicate(
		[&PrimaryTag](const FRpgInputAction& Action) { return Action.InputTag == PrimaryTag; });
	if (!TestTrue(TEXT("The combat input maps the primary attack"), Primary && Primary->InputAction)) return false;
	TestTrue(TEXT("The primary attack presses once per click"), Primary->bPressOnStartOnly);
	TestTrue(TEXT("The primary attack has no trigger that ends a held press early"), Primary->InputAction->Triggers.IsEmpty());
	return true;
}

#endif
