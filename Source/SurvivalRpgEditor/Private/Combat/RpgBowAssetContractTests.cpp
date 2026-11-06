#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimInstance.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Blueprint/BlueprintSupport.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SkinnedAsset.h"
#include "InputAction.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "StateTree.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/Animation/RpgWieldedItemAnimInstance.h"
#include "SurvivalRpg/Combat/RpgCombatProjectile.h"
#include "SurvivalRpg/Combat/RpgEnemyCombatLoadout.h"
#include "SurvivalRpg/Combat/RpgProjectileLaunchPresenter.h"
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
	const TCHAR* BowActorBasePath = TEXT("/GF_Combat_Ranged/Equipment/Bow/BP_BowActor.BP_BowActor_C");
	const TCHAR* BowAnimBlueprintPath = TEXT("/GF_Combat_Ranged/Equipment/Bow/ABP_Bow.ABP_Bow_C");
	const TCHAR* BowSkeletonPath = TEXT("/GF_Combat_Ranged/Equipment/Bow/SKEL_Bow.SKEL_Bow");

	template <typename FragmentType>
	const FragmentType* FindFragment(const URpgInventoryItemDefinition* Definition)
	{
		return Definition ? Cast<FragmentType>(Definition->FindFragmentByClass(FragmentType::StaticClass())) : nullptr;
	}

	/**
	 * The bow's spawned actor hands its nocked arrow over: it implements the presenter in its own graph, and the projectile
	 * its ammunition launches is one it takes over.
	 */
	void TestBowActorPresentsLaunches(FAutomationTestBase& Test, const TCHAR* BowName, const URpgEquipmentDefinition& Equipment,
		const FRpgProjectileShotDefinition& Shot)
	{
		const UClass* BowActorClass = Equipment.ActorsToSpawn.Num() > 0 ? Equipment.ActorsToSpawn[0].ActorToSpawn.Get() : nullptr;
		Test.TestTrue(FString::Printf(TEXT("%s: its actor hands its nocked arrow to the projectile"), BowName),
			BowActorClass && BowActorClass->ImplementsInterface(URpgProjectileLaunchPresenter::StaticClass()));
		// Without its own implementation the interface event answers false, and no arrow is ever handed over.
		const UFunction* HandOff = BowActorClass
			? BowActorClass->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(IRpgProjectileLaunchPresenter, HandOffProjectileLaunch))
			: nullptr;
		Test.TestTrue(FString::Printf(TEXT("%s: its actor implements the hand-off"), BowName),
			HandOff && HandOff->GetOwnerClass() && HandOff->GetOwnerClass()->IsChildOf(AActor::StaticClass()));

		const FClassProperty* HandOffClassProperty = BowActorClass ? FindFProperty<FClassProperty>(BowActorClass, TEXT("HandOffProjectileClass")) : nullptr;
		const UClass* HandOffClass = HandOffClassProperty
			? Cast<UClass>(HandOffClassProperty->GetObjectPropertyValue_InContainer(BowActorClass->GetDefaultObject()))
			: nullptr;
		const UClass* AmmunitionProjectile = URpgInventoryFragment_Ammunition::FindProjectileClass(Shot.AmmunitionItemDefinition).Get();
		Test.TestTrue(FString::Printf(TEXT("%s: the projectile its ammunition launches takes over the nocked arrow"), BowName),
			HandOffClass && AmmunitionProjectile && AmmunitionProjectile->IsChildOf(HandOffClass));
	}

	/** Class default of one of the bow actor's floating-point tuning variables; 0 when the variable is missing. */
	double ReadBowTuning(const UClass& BowActorClass, const TCHAR* Name)
	{
		const FNumericProperty* Property = FindFProperty<FNumericProperty>(&BowActorClass, Name);
		return Property && Property->IsFloatingPoint()
			? Property->GetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(BowActorClass.GetDefaultObject()))
			: 0.0;
	}

	/**
	 * Every bow is content on one shared foundation, as Lyra's weapons are on B_Weapon: its actor is a spawnable child of
	 * the bow actor base, its mesh uses the shared bow skeleton with the Bow_String bone, the shared bow AnimBP family pulls
	 * that bone from the drawing hand after the wielder's pose is final, and the bow sets its own tuning and nocked arrow.
	 */
	void TestBowActorUsesBowFamily(FAutomationTestBase& Test, const FString& BowName, const UClass* BowActorClass)
	{
		const UClass* BowActorBase = LoadClass<AActor>(nullptr, BowActorBasePath);
		if (!Test.TestTrue(FString::Printf(TEXT("%s: its actor is a spawnable child of the shared bow actor"), *BowName),
			BowActorClass && BowActorBase && BowActorClass->IsChildOf(BowActorBase) && !BowActorClass->HasAnyClassFlags(CLASS_Abstract)))
		{
			return;
		}

		const USkeleton* BowSkeleton = LoadObject<USkeleton>(nullptr, BowSkeletonPath);
		const USkeletalMeshComponent* BowMesh = nullptr;
		bool bHasNockedArrowMesh = false;
		// Class defaults include a child Blueprint's overrides of inherited components.
		AActor::ForEachComponentOfActorClassDefault<UMeshComponent>(const_cast<UClass*>(BowActorClass), [&](const UMeshComponent* Component)
		{
			const USkeletalMeshComponent* Skeletal = Cast<USkeletalMeshComponent>(Component);
			const USkinnedAsset* Asset = Skeletal ? Skeletal->GetSkinnedAsset() : nullptr;
			BowMesh = Asset && BowSkeleton && Asset->GetSkeleton() == BowSkeleton ? Skeletal : BowMesh;
			const UStaticMeshComponent* Static = Cast<UStaticMeshComponent>(Component);
			bHasNockedArrowMesh |= Static && Static->GetStaticMesh();
			return true;
		});
		if (!Test.TestNotNull(FString::Printf(TEXT("%s: its bow mesh uses the shared bow skeleton"), *BowName), BowMesh))
		{
			return;
		}
		Test.TestTrue(FString::Printf(TEXT("%s: its bow mesh has the Bow_String bone the draw pulls"), *BowName),
			BowMesh->GetSkinnedAsset()->GetRefSkeleton().FindBoneIndex(TEXT("Bow_String")) != INDEX_NONE);
		Test.TestTrue(FString::Printf(TEXT("%s: it shows a nocked arrow mesh"), *BowName), bHasNockedArrowMesh);

		// The string is animated: the shared AnimBP on the wielded item base reads the drawing hand after the wielder's pose.
		const UClass* BowAnimBlueprint = LoadClass<UAnimInstance>(nullptr, BowAnimBlueprintPath);
		const UClass* StringAnimClass = BowMesh->AnimClass.Get();
		const URpgWieldedItemAnimInstance* StringAnim = StringAnimClass ? Cast<URpgWieldedItemAnimInstance>(StringAnimClass->GetDefaultObject()) : nullptr;
		Test.TestTrue(FString::Printf(TEXT("%s: the shared bow AnimBP family drives the string"), *BowName),
			BowMesh->GetAnimationMode() == EAnimationMode::AnimationBlueprint && StringAnim && BowAnimBlueprint
			&& StringAnimClass->IsChildOf(BowAnimBlueprint));
		const auto ReadName = [StringAnim](const TCHAR* Name)
		{
			const FNameProperty* Property = StringAnim ? FindFProperty<FNameProperty>(StringAnim->GetClass(), Name) : nullptr;
			return Property ? Property->GetPropertyValue_InContainer(StringAnim) : FName();
		};
		const USkeleton* Manny = LoadObject<USkeleton>(nullptr, TEXT("/Game/SurvivalRpg/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin"));
		const FName DrawHand = ReadName(TEXT("WielderBone"));
		Test.TestTrue(FString::Printf(TEXT("%s: the string follows a bone of the Manny skeleton"), *BowName),
			Manny && !DrawHand.IsNone() && Manny->GetReferenceSkeleton().FindBoneIndex(DrawHand) != INDEX_NONE);
		Test.TestEqual(FString::Printf(TEXT("%s: the hand is measured from the string at rest"), *BowName),
			ReadName(TEXT("LocalFrameBone")), FName(TEXT("Bow_String")));
		Test.TestTrue(FString::Printf(TEXT("%s: the string updates after the wielder's animation"), *BowName),
			BowMesh->PrimaryComponentTick.TickGroup >= TG_PostUpdateWork);

		// Per-bow data the AnimBP and the hand-off read: a bow that forgets one draws no string or nocks no arrow.
		for (const TCHAR* Tuning : { TEXT("MaxStringPull"), TEXT("MaxHandOffset"), TEXT("StringPullSpeed"), TEXT("ArrowTipOffset") })
		{
			Test.TestTrue(FString::Printf(TEXT("%s: it sets a positive %s"), *BowName, Tuning), ReadBowTuning(*BowActorClass, Tuning) > 0.0);
		}
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

	// The bow actor pulls its string with the drawing hand and hands its nocked arrow to the launched projectile.
	TestBowActorPresentsLaunches(*this, TEXT("Player bow"), *Equipment, Weapon->GetShotDefinition());
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
	TestBowActorPresentsLaunches(*this, TEXT("Enemy bow"), *EnemyBow, EnemyShot);

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgBowFamilyContractTest,
	"SurvivalRpg.Combat.Ranged.BowFamilyContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgBowFamilyContractTest::RunTest(const FString& Parameters)
{
	using namespace RpgBowAssetContractTests;

	// Every equipment definition of the bow family, found in the registry, so a new bow is covered without a new test.
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.WaitForCompletion();
	TSet<FTopLevelAssetPath> DefinitionClassPaths;
	Registry.GetDerivedClassNames({ URpgEquipmentDefinition::StaticClass()->GetClassPathName() }, {}, DefinitionClassPaths);

	FARFilter Filter;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(TEXT("/Game"));
	for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPluginsWithContent())
	{
		if (Plugin->IsMounted() && Plugin->GetType() == EPluginType::Project)
		{
			FString Root = Plugin->GetMountedAssetPath();
			Root.RemoveFromEnd(TEXT("/"));
			Filter.PackagePaths.AddUnique(FName(*Root));
		}
	}
	TArray<FAssetData> Blueprints;
	Registry.GetAssets(Filter, Blueprints);

	const FGameplayTag BowFamily = FGameplayTag::RequestGameplayTag(TEXT("Weapon.Family.Bow"));
	int32 Bows = 0;
	for (const FAssetData& Asset : Blueprints)
	{
		// Inspect the generated class tag first so unrelated Blueprints stay unloaded.
		const FTopLevelAssetPath ClassPath(FPackageName::ExportTextPathToObjectPath(Asset.GetTagValueRef<FString>(FBlueprintTags::GeneratedClassPath)));
		if (!DefinitionClassPaths.Contains(ClassPath))
		{
			continue;
		}
		const UClass* DefinitionClass = LoadObject<UClass>(nullptr, *ClassPath.ToString());
		const URpgEquipmentDefinition* Equipment = DefinitionClass ? Cast<URpgEquipmentDefinition>(DefinitionClass->GetDefaultObject()) : nullptr;
		const URpgRangedWeaponInstance* Weapon = Equipment && Equipment->InstanceType
			? Cast<URpgRangedWeaponInstance>(Equipment->InstanceType->GetDefaultObject())
			: nullptr;
		if (!Weapon || Weapon->GetWeaponFamilyTag() != BowFamily)
		{
			continue;
		}

		++Bows;
		const FString BowName = Asset.AssetName.ToString();
		if (TestEqual(FString::Printf(TEXT("%s: the bow spawns one actor"), *BowName), Equipment->ActorsToSpawn.Num(), 1))
		{
			TestBowActorUsesBowFamily(*this, BowName, Equipment->ActorsToSpawn[0].ActorToSpawn.Get());
		}
	}
	TestTrue(TEXT("The registry finds the player bow and the enemy bow"), Bows >= 2);
	AddInfo(FString::Printf(TEXT("Checked %d bows."), Bows));
	return !HasAnyErrors();
}

#endif
