#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Equipment/RpgWeaponAbilityLoadoutComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"

namespace RpgWeaponAbilityLoadoutTests
{
	class FScopedTestWorld
	{
	public:
		FScopedTestWorld()
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

		~FScopedTestWorld()
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

	/** Owner actor, ability system and loadout used by one test. */
	struct FFixture
	{
		AActor* Owner = nullptr;
		URpgAbilitySystemComponent* AbilitySystem = nullptr;
		URpgWeaponAbilityLoadoutComponent* Loadout = nullptr;
	};

	bool MakeFixture(FAutomationTestBase& Test, UWorld* World, FFixture& OutFixture)
	{
		OutFixture.Owner = World ? World->SpawnActor<AActor>() : nullptr;
		if (!Test.TestNotNull(TEXT("Loadout owner actor spawns"), OutFixture.Owner))
		{
			return false;
		}

		OutFixture.AbilitySystem = NewObject<URpgAbilitySystemComponent>(OutFixture.Owner, NAME_None, RF_Transient);
		OutFixture.Owner->AddInstanceComponent(OutFixture.AbilitySystem);
		OutFixture.AbilitySystem->RegisterComponent();
		OutFixture.AbilitySystem->InitAbilityActorInfo(OutFixture.Owner, OutFixture.Owner);
		OutFixture.AbilitySystem->SetForceGrantAuthorityForTests(true);
		OutFixture.Loadout = NewObject<URpgWeaponAbilityLoadoutComponent>(OutFixture.Owner, NAME_None, RF_Transient);
		return Test.TestTrue(TEXT("Ability actor info is initialized"), OutFixture.AbilitySystem->AbilityActorInfo.IsValid());
	}

	URpgAbilitySet* MakeAbilitySet(const FGameplayTag InputTag, const FGameplayTag AbilityId)
	{
		URpgAbilitySet* AbilitySet = NewObject<URpgAbilitySet>(GetTransientPackage(), NAME_None, RF_Transient);
		AbilitySet->AddGrantedGameplayAbility(URpgInventoryAutomationTestUseAbility::StaticClass(), 1, InputTag, AbilityId);
		return AbilitySet;
	}

	const FGameplayAbilitySpec* FindSpecWithId(const URpgAbilitySystemComponent& AbilitySystem, const FGameplayTag AbilityId)
	{
		for (const FGameplayAbilitySpec& Spec : AbilitySystem.GetActivatableAbilities())
		{
			if (Spec.GetDynamicSpecSourceTags().HasTagExact(AbilityId))
			{
				return &Spec;
			}
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgWeaponAbilityLoadoutDefaultSlotTest,
	"SurvivalRpg.Equipment.WeaponAbilityLoadout.DefaultSlotFollowsGrants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgWeaponAbilityLoadoutDefaultSlotTest::RunTest(const FString& Parameters)
{
	using namespace RpgWeaponAbilityLoadoutTests;

	FScopedTestWorld TestWorld;
	FFixture Fixture;
	if (!MakeFixture(*this, TestWorld.GetWorld(), Fixture))
	{
		return false;
	}

	const FGameplayTag SlotInput = RpgGameplayTags::InputTag_Weapon_Ability_1;
	const FGameplayTag DefaultId = FGameplayTag::RequestGameplayTag(TEXT("Ability.Test"));
	URpgAbilitySet* AbilitySet = MakeAbilitySet(SlotInput, DefaultId);
	FRpgAbilitySet_GrantedHandles Granted;
	AbilitySet->GiveToAbilitySystem(Fixture.AbilitySystem, &Granted);

	const FGameplayAbilitySpec* Spec = FindSpecWithId(*Fixture.AbilitySystem, DefaultId);
	if (!TestNotNull(TEXT("The ability set grants the declared ability"), Spec))
	{
		return false;
	}
	TestFalse(TEXT("A slot input tag is not bound statically at grant time"), Spec->GetDynamicSpecSourceTags().HasTagExact(SlotInput));
	TestTrue(
		TEXT("The grant marks the spec as the declared default of slot 1"),
		Spec->GetDynamicSpecSourceTags().HasTagExact(RpgGameplayTags::Rpg_WeaponAbilityLoadout_DefaultSlot_1));

	Fixture.Loadout->ApplyAbilityBindings(*Fixture.AbilitySystem);
	FRpgWeaponAbilityLoadoutSlot Slot = Fixture.Loadout->GetSlot(0);
	TestEqual(TEXT("The empty slot adopts the declared default"), Slot.AbilityIdTag, DefaultId);
	TestTrue(TEXT("The slot reports a default selection"), Slot.bDefaultSelection);
	TestTrue(TEXT("The default is available"), Slot.bAvailable);
	Spec = FindSpecWithId(*Fixture.AbilitySystem, DefaultId);
	TestTrue(TEXT("The default spec receives the slot input tag"), Spec && Spec->GetDynamicSpecSourceTags().HasTagExact(SlotInput));
	TestFalse(TEXT("Other slots stay empty"), Fixture.Loadout->GetSlot(1).AbilityIdTag.IsValid());

	Fixture.Loadout->ApplyAbilityBindings(*Fixture.AbilitySystem);
	TestTrue(TEXT("A repeated refresh keeps the default bound"), Fixture.Loadout->GetSlot(0).bAvailable);

	Granted.TakeFromAbilitySystem(Fixture.AbilitySystem);
	Fixture.Loadout->ApplyAbilityBindings(*Fixture.AbilitySystem);
	Slot = Fixture.Loadout->GetSlot(0);
	TestFalse(TEXT("Removing the grant empties the default slot"), Slot.AbilityIdTag.IsValid());
	TestFalse(TEXT("An emptied slot is unavailable"), Slot.bAvailable);
	TestFalse(TEXT("An emptied slot is no default selection"), Slot.bDefaultSelection);

	FRpgAbilitySet_GrantedHandles Regranted;
	AbilitySet->GiveToAbilitySystem(Fixture.AbilitySystem, &Regranted);
	Fixture.Loadout->ApplyAbilityBindings(*Fixture.AbilitySystem);
	TestTrue(TEXT("Granting again restores the default"), Fixture.Loadout->GetSlot(0).bAvailable);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgWeaponAbilityLoadoutPlayerSelectionTest,
	"SurvivalRpg.Equipment.WeaponAbilityLoadout.PlayerSelectionOverridesDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgWeaponAbilityLoadoutPlayerSelectionTest::RunTest(const FString& Parameters)
{
	using namespace RpgWeaponAbilityLoadoutTests;

	FScopedTestWorld TestWorld;
	FFixture Fixture;
	if (!MakeFixture(*this, TestWorld.GetWorld(), Fixture))
	{
		return false;
	}

	const FGameplayTag SlotInput = RpgGameplayTags::InputTag_Weapon_Ability_1;
	const FGameplayTag DefaultId = FGameplayTag::RequestGameplayTag(TEXT("Ability.Test"));
	const FGameplayTag SelectedId = FGameplayTag::RequestGameplayTag(TEXT("Ability.Interact"));
	FRpgAbilitySet_GrantedHandles DefaultGrant;
	MakeAbilitySet(SlotInput, DefaultId)->GiveToAbilitySystem(Fixture.AbilitySystem, &DefaultGrant);
	FRpgAbilitySet_GrantedHandles SelectedGrant;
	MakeAbilitySet(FGameplayTag(), SelectedId)->GiveToAbilitySystem(Fixture.AbilitySystem, &SelectedGrant);

	// Without a controller the server RPC only records the selection; the test applies it explicitly.
	Fixture.Loadout->RequestAssignAbilityToSlot_Implementation(0, SelectedId);
	Fixture.Loadout->ApplyAbilityBindings(*Fixture.AbilitySystem);
	FRpgWeaponAbilityLoadoutSlot Slot = Fixture.Loadout->GetSlot(0);
	TestEqual(TEXT("The player selection occupies the slot"), Slot.AbilityIdTag, SelectedId);
	TestFalse(TEXT("A player selection is no default"), Slot.bDefaultSelection);
	TestTrue(TEXT("The player selection is available"), Slot.bAvailable);

	const FGameplayAbilitySpec* DefaultSpec = FindSpecWithId(*Fixture.AbilitySystem, DefaultId);
	const FGameplayAbilitySpec* SelectedSpec = FindSpecWithId(*Fixture.AbilitySystem, SelectedId);
	TestTrue(TEXT("Only the selected spec carries the slot input"), SelectedSpec && SelectedSpec->GetDynamicSpecSourceTags().HasTagExact(SlotInput));
	TestTrue(TEXT("The overridden default does not activate on the slot input"), DefaultSpec && !DefaultSpec->GetDynamicSpecSourceTags().HasTagExact(SlotInput));

	SelectedGrant.TakeFromAbilitySystem(Fixture.AbilitySystem);
	Fixture.Loadout->ApplyAbilityBindings(*Fixture.AbilitySystem);
	Slot = Fixture.Loadout->GetSlot(0);
	TestEqual(TEXT("A missing player selection stays selected"), Slot.AbilityIdTag, SelectedId);
	TestFalse(TEXT("A missing player selection is unavailable"), Slot.bAvailable);
	TestEqual(TEXT("A missing player selection reports Missing"), Slot.ResolveResult, ERpgAbilityBindingResolveResult::Missing);

	Fixture.Loadout->RequestClearSlot_Implementation(0);
	Fixture.Loadout->ApplyAbilityBindings(*Fixture.AbilitySystem);
	Slot = Fixture.Loadout->GetSlot(0);
	TestEqual(TEXT("Clearing the player selection hands the slot back to the default"), Slot.AbilityIdTag, DefaultId);
	TestTrue(TEXT("The restored default is available"), Slot.bAvailable && Slot.bDefaultSelection);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgWeaponAbilityLoadoutConflictingDefaultsTest,
	"SurvivalRpg.Equipment.WeaponAbilityLoadout.ConflictingDefaultsBlockSlot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgWeaponAbilityLoadoutConflictingDefaultsTest::RunTest(const FString& Parameters)
{
	using namespace RpgWeaponAbilityLoadoutTests;

	FScopedTestWorld TestWorld;
	FFixture Fixture;
	if (!MakeFixture(*this, TestWorld.GetWorld(), Fixture))
	{
		return false;
	}

	const FGameplayTag SlotInput = RpgGameplayTags::InputTag_Weapon_Ability_2;
	const FGameplayTag FirstId = FGameplayTag::RequestGameplayTag(TEXT("Ability.Test"));
	const FGameplayTag SecondId = FGameplayTag::RequestGameplayTag(TEXT("Ability.Interact"));
	FRpgAbilitySet_GrantedHandles FirstGrant;
	MakeAbilitySet(SlotInput, FirstId)->GiveToAbilitySystem(Fixture.AbilitySystem, &FirstGrant);
	FRpgAbilitySet_GrantedHandles SecondGrant;
	MakeAbilitySet(SlotInput, SecondId)->GiveToAbilitySystem(Fixture.AbilitySystem, &SecondGrant);

	AddExpectedError(TEXT("Weapon ability default blocked"), EAutomationExpectedErrorFlags::Contains, 1);
	Fixture.Loadout->ApplyAbilityBindings(*Fixture.AbilitySystem);
	const FRpgWeaponAbilityLoadoutSlot Slot = Fixture.Loadout->GetSlot(1);
	TestEqual(TEXT("Two different defaults for one slot are ambiguous"), Slot.ResolveResult, ERpgAbilityBindingResolveResult::Ambiguous);
	TestFalse(TEXT("An ambiguous default never binds an arbitrary ability"), Slot.bAvailable);
	for (const FGameplayAbilitySpec& Spec : Fixture.AbilitySystem->GetActivatableAbilities())
	{
		TestFalse(TEXT("No spec carries the blocked slot input"), Spec.GetDynamicSpecSourceTags().HasTagExact(SlotInput));
	}

	SecondGrant.TakeFromAbilitySystem(Fixture.AbilitySystem);
	Fixture.Loadout->ApplyAbilityBindings(*Fixture.AbilitySystem);
	TestEqual(TEXT("Removing the conflict restores the remaining default"), Fixture.Loadout->GetSlot(1).AbilityIdTag, FirstId);
	TestTrue(TEXT("The remaining default is available"), Fixture.Loadout->GetSlot(1).bAvailable);
	return true;
}

#endif
