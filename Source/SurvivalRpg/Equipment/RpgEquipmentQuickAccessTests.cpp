#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "RpgAbilityBindingResolver.h"
#include "RpgEquipmentAutomationTestTypes.h"
#include "RpgEquipmentInstance.h"
#include "RpgEquipmentLoadoutComponent.h"
#include "RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

namespace RpgEquipmentAutomationTests
{
	class FScopedEquipmentWorld
	{
	public:
		FScopedEquipmentWorld()
		{
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			if (!GameInstance)
			{
				return;
			}

			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}

		~FScopedEquipmentWorld()
		{
			UWorld* WorldToDestroy = World;
			if (GameInstance)
			{
				GameInstance->Shutdown();
			}

			if (WorldToDestroy)
			{
				GEngine->DestroyWorldContext(WorldToDestroy);
				WorldToDestroy->DestroyWorld(false);
			}

			if (GameInstance)
			{
				GameInstance->RemoveFromRoot();
			}
		}

		UWorld* GetWorld() const
		{
			return World;
		}

	private:
		TObjectPtr<UGameInstance> GameInstance = nullptr;
		TObjectPtr<UWorld> World = nullptr;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgUniqueAbilityBindingResolverTest,
	"SurvivalRpg.Equipment.AbilityBinding.RequiresUniqueSpec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgUniqueAbilityBindingResolverTest::RunTest(const FString& Parameters)
{
	URpgAbilitySystemComponent* AbilitySystemComponent = NewObject<URpgAbilitySystemComponent>();
	const FGameplayTag AbilityId = RpgGameplayTags::Ability_Attack_Basic;

	FRpgUniqueAbilityBindingResolution Resolution = FRpgAbilityBindingResolver::ResolveUniqueAbilityId(
		AbilitySystemComponent,
		AbilityId);
	TestEqual(TEXT("An ungranted ability id is missing"), Resolution.Result, ERpgAbilityBindingResolveResult::Missing);

	FGameplayAbilitySpec FirstSpec(URpgGameplayAbility::StaticClass());
	FirstSpec.GetDynamicSpecSourceTags().AddTag(AbilityId);
	AbilitySystemComponent->GetActivatableAbilities().Add(FirstSpec);
	Resolution = FRpgAbilityBindingResolver::ResolveUniqueAbilityId(AbilitySystemComponent, AbilityId);
	TestEqual(TEXT("One matching spec resolves uniquely"), Resolution.Result, ERpgAbilityBindingResolveResult::Unique);
	TestEqual(TEXT("Unique resolution reports one match"), Resolution.MatchCount, 1);
	TestTrue(TEXT("Unique resolution returns a concrete spec handle"), Resolution.SpecHandle.IsValid());

	FGameplayAbilitySpec DuplicateSpec(URpgGameplayAbility::StaticClass());
	DuplicateSpec.GetDynamicSpecSourceTags().AddTag(AbilityId);
	AbilitySystemComponent->GetActivatableAbilities().Add(DuplicateSpec);
	AddExpectedError(TEXT("Ability binding blocked"), EAutomationExpectedErrorFlags::Contains, 1);
	Resolution = FRpgAbilityBindingResolver::ResolveUniqueAbilityId(AbilitySystemComponent, AbilityId);
	TestEqual(TEXT("Duplicate ids are blocked as ambiguous"), Resolution.Result, ERpgAbilityBindingResolveResult::Ambiguous);
	TestEqual(TEXT("Ambiguous resolution reports every match"), Resolution.MatchCount, 2);
	TestFalse(TEXT("Ambiguous resolution never returns an arbitrary handle"), Resolution.SpecHandle.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgQuickAccessBindingContractTest,
	"SurvivalRpg.Inventory.QuickAccess.FixedEightBindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgQuickAccessBindingContractTest::RunTest(const FString& Parameters)
{
	const URpgActionBarComponent* ActionBar = NewObject<URpgActionBarComponent>();
	const TArray<FRpgQuickAccessBinding> Bindings = ActionBar->GetQuickAccessBindings();
	TestEqual(TEXT("Keyboard and radial share exactly eight entries"), Bindings.Num(), 8);
	for (int32 Index = 0; Index < Bindings.Num(); ++Index)
	{
		TestEqual(
			*FString::Printf(TEXT("Binding %d defaults to Empty"), Index),
			Bindings[Index].SlotType,
			ERpgActionBarSlotType::Empty);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgQuickAccessLegacySlotAddressRestoreTest,
	"SurvivalRpg.Inventory.QuickAccess.LegacySlotAddressRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgQuickAccessLegacySlotAddressRestoreTest::RunTest(const FString& Parameters)
{
	RpgEquipmentAutomationTests::FScopedEquipmentWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	if (!TestNotNull(TEXT("Standalone Quick Access restore world is available"), World))
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = MakeUniqueObjectName(
		World,
		AActor::StaticClass(),
		TEXT("QuickAccessRestoreOwner"));
	SpawnParameters.ObjectFlags = RF_Transient;
	AActor* OwnerActor = World->SpawnActor<AActor>(SpawnParameters);
	if (!TestNotNull(TEXT("Quick Access restore owner is spawned"), OwnerActor) ||
		!TestTrue(TEXT("Quick Access restore owner has server authority"), OwnerActor->HasAuthority()))
	{
		return false;
	}

	URpgActionBarComponent* ActionBar = NewObject<URpgActionBarComponent>(
		OwnerActor,
		MakeUniqueObjectName(
			OwnerActor,
			URpgActionBarComponent::StaticClass(),
			TEXT("ActionBar")),
		RF_Transient);
	if (!TestNotNull(TEXT("Actor-owned Quick Access component is created"), ActionBar))
	{
		return false;
	}

	OwnerActor->AddInstanceComponent(ActionBar);
	ActionBar->RegisterComponent();
	if (!TestTrue(TEXT("Actor-owned Quick Access component is registered"), ActionBar->IsRegistered()))
	{
		return false;
	}

	FNameProperty* LegacyContainerIdProperty = FindFProperty<FNameProperty>(
		FRpgInventorySlotAddress::StaticStruct(),
		TEXT("ContainerId"));
	if (!TestNotNull(
		TEXT("Historical SlotAddress ContainerId is available to the restore converter"),
		LegacyContainerIdProperty))
	{
		return false;
	}

	auto SetLegacyContainerId = [LegacyContainerIdProperty](
		FRpgInventorySlotAddress& Address,
		FName ContainerId)
	{
		*LegacyContainerIdProperty->ContainerPtrToValuePtr<FName>(&Address) = ContainerId;
	};
	auto GetLegacyContainerId = [LegacyContainerIdProperty](
		const FRpgInventorySlotAddress& Address)
	{
		return LegacyContainerIdProperty->GetPropertyValue_InContainer(&Address);
	};

	const FName LegacyOnlyRoot(TEXT("Pockets"));
	const FName MatchingRoot(TEXT("Backpack"));
	const FName MatchingItemOwnedLocalId(TEXT("Main"));
	const FRpgInventoryContainerHandle MatchingItemOwnedHandle =
		FRpgInventoryContainerHandle::MakeItemOwned(
			FRpgInventoryItemId(FGuid(11, 22, 33, 44)),
			MatchingItemOwnedLocalId,
			1);
	const FName ConflictingCanonicalRoot(TEXT("Belt"));
	const FName ConflictingLegacyRoot(TEXT("Gear.Head"));

	TArray<FRpgQuickAccessBinding> SavedBindings;
	SavedBindings.SetNum(4);

	FRpgQuickAccessBinding& LegacyOnlyBinding = SavedBindings[0];
	LegacyOnlyBinding.SlotType = ERpgActionBarSlotType::Empty;
	LegacyOnlyBinding.SlotAddress.X = 2;
	LegacyOnlyBinding.SlotAddress.Y = 3;
	SetLegacyContainerId(LegacyOnlyBinding.SlotAddress, LegacyOnlyRoot);

	FRpgQuickAccessBinding& MatchingBinding = SavedBindings[1];
	MatchingBinding.SlotType = ERpgActionBarSlotType::Empty;
	MatchingBinding.SlotAddress.SetContainerHandle(
		FRpgInventoryContainerHandle::MakeRoot(MatchingRoot));
	MatchingBinding.SlotAddress.X = 4;
	MatchingBinding.SlotAddress.Y = 5;
	SetLegacyContainerId(MatchingBinding.SlotAddress, MatchingRoot);

	FRpgQuickAccessBinding& MatchingItemOwnedBinding = SavedBindings[2];
	MatchingItemOwnedBinding.SlotType = ERpgActionBarSlotType::Empty;
	MatchingItemOwnedBinding.SlotAddress.SetContainerHandle(
		MatchingItemOwnedHandle);
	MatchingItemOwnedBinding.SlotAddress.X = 1;
	MatchingItemOwnedBinding.SlotAddress.Y = 2;
	SetLegacyContainerId(
		MatchingItemOwnedBinding.SlotAddress,
		MatchingItemOwnedLocalId);

	FRpgQuickAccessBinding& ConflictingBinding = SavedBindings[3];
	ConflictingBinding.SlotType = ERpgActionBarSlotType::Empty;
	ConflictingBinding.SlotAddress.SetContainerHandle(
		FRpgInventoryContainerHandle::MakeRoot(ConflictingCanonicalRoot));
	ConflictingBinding.SlotAddress.X = 6;
	ConflictingBinding.SlotAddress.Y = 7;
	SetLegacyContainerId(ConflictingBinding.SlotAddress, ConflictingLegacyRoot);

	ActionBar->RestoreQuickAccessBindings(SavedBindings, true);
	const TArray<FRpgQuickAccessBinding> RestoredBindings = ActionBar->GetQuickAccessBindings();
	if (!TestEqual(TEXT("Restore preserves the fixed eight-binding contract"), RestoredBindings.Num(), 8))
	{
		return false;
	}

	const FRpgQuickAccessBinding& RestoredLegacyOnly = RestoredBindings[0];
	TestEqual(
		TEXT("Legacy-only root address is promoted to its canonical handle"),
		RestoredLegacyOnly.SlotAddress.GetContainerHandle(),
		FRpgInventoryContainerHandle::MakeRoot(LegacyOnlyRoot));
	TestEqual(TEXT("Legacy-only restore preserves X"), RestoredLegacyOnly.SlotAddress.X, 2);
	TestEqual(TEXT("Legacy-only restore preserves Y"), RestoredLegacyOnly.SlotAddress.Y, 3);
	TestTrue(TEXT("Promoted legacy-only address is runtime-valid"), RestoredLegacyOnly.SlotAddress.IsValid());
	TestTrue(
		TEXT("Legacy-only restore clears the historical ContainerId shadow"),
		GetLegacyContainerId(RestoredLegacyOnly.SlotAddress).IsNone());

	const FRpgQuickAccessBinding& RestoredMatching = RestoredBindings[1];
	TestEqual(
		TEXT("Matching canonical and legacy roots preserve the exact canonical handle"),
		RestoredMatching.SlotAddress.GetContainerHandle(),
		FRpgInventoryContainerHandle::MakeRoot(MatchingRoot));
	TestEqual(TEXT("Matching-root restore preserves X"), RestoredMatching.SlotAddress.X, 4);
	TestEqual(TEXT("Matching-root restore preserves Y"), RestoredMatching.SlotAddress.Y, 5);
	TestTrue(
		TEXT("Matching-root restore clears the historical ContainerId shadow"),
		GetLegacyContainerId(RestoredMatching.SlotAddress).IsNone());

	const FRpgQuickAccessBinding& RestoredMatchingItemOwned = RestoredBindings[2];
	TestEqual(
		TEXT("Matching item-owned legacy shadow preserves the exact canonical owner"),
		RestoredMatchingItemOwned.SlotAddress.GetContainerHandle(),
		MatchingItemOwnedHandle);
	TestEqual(
		TEXT("Matching item-owned restore preserves X"),
		RestoredMatchingItemOwned.SlotAddress.X,
		1);
	TestEqual(
		TEXT("Matching item-owned restore preserves Y"),
		RestoredMatchingItemOwned.SlotAddress.Y,
		2);
	TestTrue(
		TEXT("Matching item-owned restore clears only the historical local-id shadow"),
		GetLegacyContainerId(RestoredMatchingItemOwned.SlotAddress).IsNone());

	const FRpgQuickAccessBinding& RestoredConflict = RestoredBindings[3];
	TestEqual(
		TEXT("Conflicting canonical and legacy roots reset the binding"),
		RestoredConflict.SlotType,
		ERpgActionBarSlotType::Empty);
	TestFalse(
		TEXT("Conflicting roots cannot leave a runtime-valid slot address"),
		RestoredConflict.SlotAddress.IsValid());
	TestFalse(
		TEXT("Conflicting roots cannot preserve either container handle"),
		RestoredConflict.SlotAddress.GetContainerHandle().IsValid());
	TestTrue(
		TEXT("Reset conflict does not retain the historical ContainerId shadow"),
		GetLegacyContainerId(RestoredConflict.SlotAddress).IsNone());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgEquipmentLoadTierThresholdTest,
	"SurvivalRpg.Equipment.Load.ExactThresholds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgEquipmentLoadTierThresholdTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("12.99 kg is Light"),
		URpgEquipmentLoadoutComponent::ResolveLoadTierForWeight(12.99f),
		ERpgEquipmentLoadTier::Light);
	TestEqual(
		TEXT("13 kg enters Medium"),
		URpgEquipmentLoadoutComponent::ResolveLoadTierForWeight(13.0f),
		ERpgEquipmentLoadTier::Medium);
	TestEqual(
		TEXT("22.99 kg stays Medium"),
		URpgEquipmentLoadoutComponent::ResolveLoadTierForWeight(22.99f),
		ERpgEquipmentLoadTier::Medium);
	TestEqual(
		TEXT("23 kg enters Heavy"),
		URpgEquipmentLoadoutComponent::ResolveLoadTierForWeight(23.0f),
		ERpgEquipmentLoadTier::Heavy);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgEquipmentEmptyAllowedSlotsTest,
	"SurvivalRpg.Equipment.Definition.EmptyAllowedSlotsAreDisabled",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgEquipmentEmptyAllowedSlotsTest::RunTest(const FString& Parameters)
{
	URpgEquipmentAutomationTestSwordDefinition* DisabledDefinition =
		NewObject<URpgEquipmentAutomationTestSwordDefinition>();
	if (!TestNotNull(TEXT("A mutable equipment-definition fixture exists"), DisabledDefinition))
	{
		return false;
	}

	DisabledDefinition->AllowedSlots.Reset();
	TestFalse(
		TEXT("An empty AllowedSlots array does not silently fall back to MainHand"),
		DisabledDefinition->CanEquipInSlot(ERpgEquipmentSlot::MainHand));
	TestEqual(
		TEXT("An empty AllowedSlots array has no default equipment destination"),
		DisabledDefinition->GetDefaultEquipSlot(),
		ERpgEquipmentSlot::None);

	DisabledDefinition->AllowedSlots = { ERpgEquipmentSlot::OffHand };
	DisabledDefinition->HandOccupancy = ERpgEquipmentHandOccupancy::BothHands;
	TestFalse(
		TEXT("A BothHands definition cannot activate from OffHand even when stale data lists that slot"),
		DisabledDefinition->CanEquipInSlot(ERpgEquipmentSlot::OffHand));
	TestEqual(
		TEXT("An OffHand-only BothHands definition has no unsafe default destination"),
		DisabledDefinition->GetDefaultEquipSlot(),
		ERpgEquipmentSlot::None);

	DisabledDefinition->AllowedSlots =
	{
		ERpgEquipmentSlot::MainHand,
		ERpgEquipmentSlot::OffHand
	};
	TestFalse(
		TEXT("A malformed BothHands definition fails closed even for its listed MainHand slot"),
		DisabledDefinition->CanEquipInSlot(
			ERpgEquipmentSlot::MainHand));
	TestEqual(
		TEXT("The malformed BothHands definition does not expose an unsafe default"),
		DisabledDefinition->GetDefaultEquipSlot(),
		ERpgEquipmentSlot::None);

	DisabledDefinition->AllowedSlots =
	{
		ERpgEquipmentSlot::MainHand
	};
	DisabledDefinition->HandOccupancy =
		static_cast<ERpgEquipmentHandOccupancy>(255);
	TestFalse(
		TEXT("An unknown HandOccupancy value fails closed during runtime equip"),
		DisabledDefinition->CanEquipInSlot(
			ERpgEquipmentSlot::MainHand));
	TestEqual(
		TEXT("An unknown HandOccupancy value has no default equipment destination"),
		DisabledDefinition->GetDefaultEquipSlot(),
		ERpgEquipmentSlot::None);
	TestFalse(
		TEXT("An unknown HandOccupancy value cannot claim a runtime slot"),
		DisabledDefinition->OccupiesSlot(
			ERpgEquipmentSlot::MainHand,
			ERpgEquipmentSlot::MainHand));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgEquipmentPersistentHealthGrantTest,
	"SurvivalRpg.Equipment.Grants.UnchangedMaxHealthEffectSurvivesWeaponEquip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgEquipmentPersistentHealthGrantTest::RunTest(const FString& Parameters)
{
	RpgEquipmentAutomationTests::FScopedEquipmentWorld TestWorld;
	if (!TestNotNull(TEXT("Standalone equipment test world is available"), TestWorld.GetWorld()))
	{
		return false;
	}

	ARpgEquipmentAutomationTestPawn* Pawn =
		TestWorld.GetWorld()->SpawnActor<ARpgEquipmentAutomationTestPawn>();
	if (!TestNotNull(TEXT("Authoritative GAS equipment pawn is spawned"), Pawn))
	{
		return false;
	}

	URpgAbilitySystemComponent* AbilitySystemComponent = Pawn->GetRpgAbilitySystemComponent();
	URpgEquipmentManagerComponent* EquipmentManager = Pawn->GetEquipmentManagerComponent();
	URpgHealthSet* HealthSet = Pawn->GetHealthSet();
	if (!TestNotNull(TEXT("Pawn owns an ability system"), AbilitySystemComponent) ||
		!TestNotNull(TEXT("Pawn owns an equipment manager"), EquipmentManager) ||
		!TestNotNull(TEXT("Pawn owns a health set"), HealthSet))
	{
		return false;
	}
	TestNull(
		TEXT("The direct definition-driven runtime seam does not require a player loadout component"),
		Pawn->FindComponentByClass<URpgEquipmentLoadoutComponent>());

	// The standalone automation world does not run the complete PlayerState component lifecycle.
	AbilitySystemComponent->AddAttributeSetSubobject(HealthSet);
	AbilitySystemComponent->InitAbilityActorInfo(Pawn, Pawn);
	if (!TestNotNull(TEXT("Health set is registered with GAS"), AbilitySystemComponent->GetSet<URpgHealthSet>()))
	{
		return false;
	}

	TestEqual(TEXT("Fixture starts with 100 MaxHealth"), HealthSet->GetMaxHealth(), 100.0f);

	URpgEquipmentInstance* Helmet = EquipmentManager->EquipItemInSlot(
		URpgEquipmentAutomationTestHelmetDefinition::StaticClass(),
		ERpgEquipmentSlot::Head);
	if (!TestNotNull(TEXT("Helmet equips in the Head slot"), Helmet))
	{
		return false;
	}
	TestNull(
		TEXT("Definition-driven NPC/runtime equipment intentionally has no inventory item instigator"),
		Helmet->GetInstigator());
	TestEqual(
		TEXT("The direct manager seam owns the created runtime instance"),
		EquipmentManager->GetEquipmentInstanceInSlot(ERpgEquipmentSlot::Head),
		Helmet);

	TestEqual(TEXT("Helmet persistent effect raises MaxHealth to 600"), HealthSet->GetMaxHealth(), 600.0f);
	AbilitySystemComponent->SetNumericAttributeBase(URpgHealthSet::GetHealthAttribute(), 600.0f);
	TestEqual(TEXT("Fixture can heal to the helmet-adjusted maximum"), HealthSet->GetHealth(), 600.0f);

	URpgEquipmentInstance* Sword = EquipmentManager->EquipItemInSlot(
		URpgEquipmentAutomationTestSwordDefinition::StaticClass(),
		ERpgEquipmentSlot::MainHand);
	TestNotNull(TEXT("Sword equips in the MainHand slot"), Sword);
	TestEqual(TEXT("Unrelated weapon equip keeps helmet MaxHealth active"), HealthSet->GetMaxHealth(), 600.0f);
	TestEqual(TEXT("Unrelated weapon equip does not clamp current Health"), HealthSet->GetHealth(), 600.0f);

	return true;
}

#endif
