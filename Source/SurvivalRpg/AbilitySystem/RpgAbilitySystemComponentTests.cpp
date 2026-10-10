#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"

namespace RpgAbilitySetAuthorityTests
{
	class FFixture
	{
	public:
		FFixture()
		{
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}

		~FFixture()
		{
			GameInstance->Shutdown();
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
			GameInstance->RemoveFromRoot();
		}

		bool Initialize(FAutomationTestBase& Test)
		{
			Owner = World ? World->SpawnActor<AActor>() : nullptr;
			if (!Test.TestNotNull(TEXT("The ability system owner spawns"), Owner))
			{
				return false;
			}

			AbilitySystem = NewObject<URpgAbilitySystemComponent>(Owner, NAME_None, RF_Transient);
			Owner->AddInstanceComponent(AbilitySystem);
			AbilitySystem->RegisterComponent();
			AbilitySystem->InitAbilityActorInfo(Owner, Owner);

			AbilitySet = NewObject<URpgAbilitySet>(GetTransientPackage(), NAME_None, RF_Transient);
			AbilitySet->AddGrantedGameplayAbility(URpgInventoryAutomationTestUseAbility::StaticClass());
			return Test.TestTrue(TEXT("The server-owned ability system has grant authority"), AbilitySystem->HasGrantAuthority());
		}

		/** Switches the owner between server and client view of the same ability system. */
		void SetOwnerRole(const ENetRole Role)
		{
			Owner->SetRole(Role);
			AbilitySystem->CacheIsNetSimulated();
		}

		bool HasGrantedAbility() const
		{
			for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
			{
				if (Spec.Ability && Spec.Ability->GetClass() == URpgInventoryAutomationTestUseAbility::StaticClass())
				{
					return true;
				}
			}
			return false;
		}

		URpgAbilitySystemComponent* AbilitySystem = nullptr;
		URpgAbilitySet* AbilitySet = nullptr;

	private:
		UGameInstance* GameInstance = nullptr;
		UWorld* World = nullptr;
		AActor* Owner = nullptr;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgAbilitySetBlueprintAuthorityOnlyTest,
	"SurvivalRpg.AbilitySystem.AbilitySet.BlueprintCallsRequireAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgAbilitySetBlueprintAuthorityOnlyTest::RunTest(const FString& Parameters)
{
	// Blueprint calls on clients are absorbed before they reach native code; no server RPC carries a grant request.
	for (const TCHAR* FunctionName : { TEXT("GrantAbilitySet"), TEXT("RemoveAbilitySet") })
	{
		const UFunction* Function = URpgAbilitySystemComponent::StaticClass()->FindFunctionByName(FunctionName);
		if (TestNotNull(FString::Printf(TEXT("%s is reflected"), FunctionName), Function))
		{
			TestTrue(FString::Printf(TEXT("%s is BlueprintAuthorityOnly"), FunctionName), Function->HasAnyFunctionFlags(FUNC_BlueprintAuthorityOnly));
			TestFalse(FString::Printf(TEXT("%s is not a network function"), FunctionName), Function->HasAnyFunctionFlags(FUNC_Net));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgAbilitySetNonAuthorityGrantTest,
	"SurvivalRpg.AbilitySystem.AbilitySet.NonAuthorityCannotGrantOrRemove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgAbilitySetNonAuthorityGrantTest::RunTest(const FString& Parameters)
{
	RpgAbilitySetAuthorityTests::FFixture Fixture;
	if (!Fixture.Initialize(*this))
	{
		return false;
	}

	Fixture.SetOwnerRole(ROLE_AutonomousProxy);
	if (!TestFalse(TEXT("The owning client has no grant authority"), Fixture.AbilitySystem->HasGrantAuthority()))
	{
		return false;
	}
	AddExpectedError(TEXT("GrantAbilitySet: ignored"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("A client grant request is rejected"), Fixture.AbilitySystem->GrantAbilitySet(Fixture.AbilitySet, nullptr));
	TestFalse(TEXT("The rejected set is not tracked"), Fixture.AbilitySystem->HasAbilitySet(Fixture.AbilitySet));
	TestFalse(TEXT("The rejected set grants no ability"), Fixture.HasGrantedAbility());

	Fixture.SetOwnerRole(ROLE_Authority);
	if (!TestTrue(TEXT("The server grants the set"), Fixture.AbilitySystem->GrantAbilitySet(Fixture.AbilitySet, nullptr)))
	{
		return false;
	}
	TestTrue(TEXT("The server grant adds the ability"), Fixture.HasGrantedAbility());

	Fixture.SetOwnerRole(ROLE_AutonomousProxy);
	AddExpectedError(TEXT("RemoveAbilitySet: ignored"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("A client remove request is rejected"), Fixture.AbilitySystem->RemoveAbilitySet(Fixture.AbilitySet));
	TestTrue(TEXT("The granted set stays tracked"), Fixture.AbilitySystem->HasAbilitySet(Fixture.AbilitySet));
	TestTrue(TEXT("The granted ability stays"), Fixture.HasGrantedAbility());

	Fixture.SetOwnerRole(ROLE_Authority);
	TestTrue(TEXT("The server removes the set"), Fixture.AbilitySystem->RemoveAbilitySet(Fixture.AbilitySet));
	TestFalse(TEXT("The removed ability is gone"), Fixture.HasGrantedAbility());
	return true;
}

#endif
