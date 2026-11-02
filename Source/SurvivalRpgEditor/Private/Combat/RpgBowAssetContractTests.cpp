#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Components/SkeletalMeshComponent.h"
#include "InputAction.h"
#include "Misc/AutomationTest.h"
#include "StateTree.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/Combat/RpgCombatProjectile.h"
#include "SurvivalRpg/Combat/RpgEnemyCombatLoadout.h"
#include "SurvivalRpg/Combat/RpgProjectileShot.h"
#include "SurvivalRpg/Core/AI/RpgAICharacter.h"
#include "SurvivalRpg/Core/AI/RpgAIController.h"
#include "SurvivalRpg/Core/AI/RpgAIPawnData.h"
#include "SurvivalRpg/Core/AI/RpgStateTreeCombatTasks.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Equipment/RpgRangedWeaponInstance.h"
#include "SurvivalRpg/Input/RpgInputConfig.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_Ammunition.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_EquippableItem.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_SkillTree.h"
#include "Animation/Skeleton.h"
#include "UObject/UnrealType.h"

namespace RpgBowAssetContractTests
{
	const TCHAR* BowPath = TEXT("/GF_Combat_Ranged/Items/ID_Bow.ID_Bow_C");
	const TCHAR* ArrowPath = TEXT("/GF_Combat_Ranged/Items/ID_Arrow_Standard.ID_Arrow_Standard_C");
	const TCHAR* ArrowProjectilePath = TEXT("/GF_Combat_Ranged/Projectiles/BP_Projectile_Arrow.BP_Projectile_Arrow_C");
	const TCHAR* CombatInputConfigPath = TEXT("/GF_Combat_Core/Input/DA_InputConfig_Combat.DA_InputConfig_Combat");
	const TCHAR* EnemyBowPath = TEXT("/GF_Combat_Ranged/AI/EnemyWeapons/ID_EnemyBow.ID_EnemyBow_C");
	const TCHAR* ArcherPath = TEXT("/GF_AI_RiftMonsters/AI/Characters/BP_RiftGrunt_Bow.BP_RiftGrunt_Bow_C");

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

	// The main hand grants the bow's shot on the primary attack input and its held aim on the aim input.
	TArray<const FRpgAbilitySet_GameplayAbility*> Grants;
	for (const FRpgEquipmentSlotAbilitySet& SlotSet : Equipment->SlotAbilitySetsToGrant)
	{
		if (SlotSet.EquippedSlot == ERpgEquipmentSlot::MainHand && SlotSet.AbilitySet)
		{
			for (const FRpgAbilitySet_GameplayAbility& Grant : SlotSet.AbilitySet->GetGrantedGameplayAbilities())
			{
				Grants.Add(&Grant);
			}
		}
	}
	TestEqual(TEXT("The bow grants its shot and its aim"), Grants.Num(), 2);
	const auto FindGrant = [&Grants](const TCHAR* InputTag) -> const FRpgAbilitySet_GameplayAbility*
	{
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(InputTag);
		const FRpgAbilitySet_GameplayAbility* const* Found = Grants.FindByPredicate(
			[&Tag](const FRpgAbilitySet_GameplayAbility* Grant) { return Grant->InputTag == Tag; });
		return Found ? *Found : nullptr;
	};
	const FGameplayTag RangedAimTag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Type.RangedAim"));
	const FRpgAbilitySet_GameplayAbility* Shot = FindGrant(TEXT("InputTag.Weapon.Primary"));
	const FRpgAbilitySet_GameplayAbility* Aim = FindGrant(TEXT("InputTag.Weapon.Block"));
	if (!TestTrue(TEXT("The shot is on the primary attack input and the aim on the aim input"), Shot && Aim)) return false;
	for (const FRpgAbilitySet_GameplayAbility* Grant : { Shot, Aim })
	{
		const UClass* AbilityClass = Grant->Ability.Get();
		const URpgGameplayAbility* AbilityDefaults = AbilityClass ? AbilityClass->GetDefaultObject<URpgGameplayAbility>() : nullptr;
		TestTrue(TEXT("Each bow ability is a Blueprint on the project's ability base"),
			AbilityClass && AbilityClass->IsChildOf(URpgGameplayAbility::StaticClass()) && !AbilityClass->IsNative());
		TestTrue(TEXT("Shot and aim hold one aim stance together"), AbilityDefaults && AbilityDefaults->GetAssetTags().HasTag(RangedAimTag));
	}
	TestEqual(TEXT("The shot has its stable ability id for ranged tunings"), Shot->AbilityIdTag,
		FGameplayTag::RequestGameplayTag(TEXT("Ability.Ranged.BowShot")));
	TestEqual(TEXT("The aim has its own stable ability id"), Aim->AbilityIdTag, FGameplayTag::RequestGameplayTag(TEXT("Ability.Ranged.BowAim")));
	const URpgGameplayAbility* AimDefaults = Aim->Ability ? Aim->Ability->GetDefaultObject<URpgGameplayAbility>() : nullptr;
	TestTrue(TEXT("The aim lasts while its input is held and runs beside the shot"), AimDefaults
		&& AimDefaults->GetActivationPolicy() == ERpgAbilityActivationPolicy::WhileInputActive
		&& AimDefaults->GetActivationGroup() == ERpgAbilityActivationGroup::Independent);

	// The aim stance slows movement and has a camera; the hold layer presents it on the Manny skeleton.
	TestTrue(TEXT("Aiming slows movement"), Weapon->GetAimDefinition().MovementSpeedLimit > 0.0f);
	TestNotNull(TEXT("Aiming has its own camera"), Weapon->GetAimDefinition().CameraMode.Get());
	const USkeleton* Manny = LoadObject<USkeleton>(nullptr, TEXT("/Game/SurvivalRpg/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin"));
	TestNotNull(TEXT("The bow has a hold layer for the Manny skeleton"), Equipment->ResolveHoldLayer(Manny, ERpgEquipmentSlot::MainHand).Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgEnemyBowAssetContractTest,
	"SurvivalRpg.Combat.Ranged.EnemyBowAssetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgEnemyBowAssetContractTest::RunTest(const FString& Parameters)
{
	using namespace RpgBowAssetContractTests;
	const auto LoadEquipment = [](const TCHAR* ItemPath) -> const URpgEquipmentDefinition*
	{
		const UClass* ItemClass = LoadClass<URpgInventoryItemDefinition>(nullptr, ItemPath);
		const URpgInventoryFragment_EquippableItem* Equippable =
			ItemClass ? FindFragment<URpgInventoryFragment_EquippableItem>(ItemClass->GetDefaultObject<URpgInventoryItemDefinition>()) : nullptr;
		return Equippable && Equippable->EquipmentDefinition
			? Equippable->EquipmentDefinition->GetDefaultObject<URpgEquipmentDefinition>()
			: nullptr;
	};
	const auto MainHandSets = [](const URpgEquipmentDefinition& Equipment)
	{
		TSet<const URpgAbilitySet*> Sets;
		for (const FRpgEquipmentSlotAbilitySet& SlotSet : Equipment.SlotAbilitySetsToGrant)
		{
			if (SlotSet.EquippedSlot == ERpgEquipmentSlot::MainHand && SlotSet.AbilitySet)
			{
				Sets.Add(SlotSet.AbilitySet);
			}
		}
		return Sets;
	};

	const URpgEquipmentDefinition* PlayerBow = LoadEquipment(BowPath);
	const URpgEquipmentDefinition* EnemyBow = LoadEquipment(EnemyBowPath);
	if (!TestTrue(TEXT("Player and enemy bow equip through equipment definitions"), PlayerBow && EnemyBow)) return false;
	const URpgRangedWeaponInstance* PlayerWeapon = PlayerBow->InstanceType ? Cast<URpgRangedWeaponInstance>(PlayerBow->InstanceType->GetDefaultObject()) : nullptr;
	const URpgRangedWeaponInstance* EnemyWeapon = EnemyBow->InstanceType ? Cast<URpgRangedWeaponInstance>(EnemyBow->InstanceType->GetDefaultObject()) : nullptr;
	if (!TestTrue(TEXT("Both are ranged weapons"), PlayerWeapon && EnemyWeapon)) return false;

	// One bow execution: the enemy bow grants the player's shot ability; only its ammunition policy differs.
	TestTrue(TEXT("The enemy bow grants the player bow's abilities"), MainHandSets(*EnemyBow).Num() > 0
		&& MainHandSets(*EnemyBow).Difference(MainHandSets(*PlayerBow)).IsEmpty()
		&& MainHandSets(*PlayerBow).Difference(MainHandSets(*EnemyBow)).IsEmpty());
	TestEqual(TEXT("A player's bow always consumes arrows"), PlayerWeapon->GetAmmunitionPolicy(), ERpgProjectileAmmunitionPolicy::ShooterInventory);
	TestEqual(TEXT("The enemy bow fires without items"), EnemyWeapon->GetAmmunitionPolicy(), ERpgProjectileAmmunitionPolicy::Unlimited);
	const FRpgProjectileShotDefinition& EnemyShot = EnemyWeapon->GetShotDefinition();
	TArray<FText> ShotErrors;
	RpgProjectileShot::ValidateDefinition(EnemyShot, ShotErrors);
	TestTrue(TEXT("The enemy bow's shot definition is valid"), ShotErrors.IsEmpty());
	TestTrue(TEXT("Its unlimited fire returns arrows for a finite budget per life"), EnemyShot.UnlimitedRecoveryBudget > 0);
	TestEqual(TEXT("It shoots the standard arrow"), EnemyShot.AmmunitionItemDefinition.Get(), LoadClass<URpgInventoryItemDefinition>(nullptr, ArrowPath));
	const UClass* ProjectileClass = URpgInventoryFragment_Ammunition::FindProjectileClass(EnemyShot.AmmunitionItemDefinition);
	TestTrue(TEXT("Its arrows can be recovered"), ProjectileClass
		&& ProjectileClass->GetDefaultObject<ARpgCombatProjectile>()->GetImpactRecovery() == ERpgProjectileImpactRecovery::Recoverable);
	const USkeleton* Manny = LoadObject<USkeleton>(nullptr, TEXT("/Game/SurvivalRpg/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin"));
	TestNotNull(TEXT("The enemy bow has a hold layer for the Manny skeleton"), EnemyBow->ResolveHoldLayer(Manny, ERpgEquipmentSlot::MainHand).Get());

	// The bow enemy: its archetype's loadout equips the enemy bow, its AnimBP hosts the hold layer, its tree shoots.
	const UClass* ArcherClass = LoadClass<ARpgAICharacter>(nullptr, ArcherPath);
	const ARpgAICharacter* Archer = ArcherClass ? ArcherClass->GetDefaultObject<ARpgAICharacter>() : nullptr;
	if (!TestNotNull(TEXT("The bow enemy loads"), Archer)) return false;
	const URpgEnemyCombatArchetypeComponent* Archetype = Archer->FindComponentByClass<URpgEnemyCombatArchetypeComponent>();
	const URpgEnemyCombatLoadoutComponent* Loadouts = Archer->FindComponentByClass<URpgEnemyCombatLoadoutComponent>();
	if (!TestTrue(TEXT("The bow enemy has an archetype and loadouts"), Archetype && Loadouts)) return false;
	const FGameplayTag ArchetypeTag = Archetype->GetEnemyCombatArchetypeTag();
	TestEqual(TEXT("It is of the Bow archetype"), ArchetypeTag, FGameplayTag::RequestGameplayTag(TEXT("Enemy.Archetype.Bow")));
	const URpgEnemyCombatLoadoutDefinition* Loadout = nullptr;
	for (const TSoftObjectPtr<const URpgEnemyCombatLoadoutDefinition>& Definition : Loadouts->GetLoadoutDefinitionsForEditor())
	{
		const URpgEnemyCombatLoadoutDefinition* Loaded = Definition.LoadSynchronous();
		Loadout = Loaded && Loaded->GetArchetypeTag() == ArchetypeTag ? Loaded : Loadout;
	}
	TestTrue(TEXT("Its archetype's loadout equips the enemy bow in the main hand"), Loadout && Loadout->GetEquipmentItems().ContainsByPredicate(
		[](const FRpgEnemyCombatLoadoutItem& Item)
		{
			return Item.EquipmentSlot == ERpgEquipmentSlot::MainHand
				&& Item.ItemDefinition.ToSoftObjectPath() == FSoftObjectPath(EnemyBowPath);
		}));
	const UClass* AnimClass = Archer->GetMesh() ? Archer->GetMesh()->GetAnimClass() : nullptr;
	const UClass* MainHandHold = LoadClass<UObject>(nullptr,
		TEXT("/Game/SurvivalRpg/Characters/GASP/Shared/RPG/WeaponHold/ALI_RpgMainHandHold.ALI_RpgMainHandHold_C"));
	TestTrue(TEXT("Its AnimBP hosts the main-hand hold layer the bow links"), AnimClass && MainHandHold && AnimClass->ImplementsInterface(MainHandHold));

	const UClass* ControllerClass = Archer->AIControllerClass.Get();
	const ARpgAIController* Controller = ControllerClass ? Cast<ARpgAIController>(ControllerClass->GetDefaultObject()) : nullptr;
	const UStateTree* Tree = Controller && Controller->GetDefaultPawnData() ? Controller->GetDefaultPawnData()->StateTree.Get() : nullptr;
	bool bShoots = false;
	if (Tree)
	{
		const FInstancedStructContainer& Nodes = Tree->GetNodes();
		for (int32 Index = 0; Index < Nodes.Num(); ++Index)
		{
			bShoots |= Nodes[Index].GetScriptStruct() == FRpgStateTreeRangedAttackTask::StaticStruct();
		}
	}
	TestTrue(TEXT("Its controller runs a StateTree that shoots with the Ranged Attack task"), bShoots);
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
