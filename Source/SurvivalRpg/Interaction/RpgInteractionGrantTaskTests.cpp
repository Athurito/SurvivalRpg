// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "RpgInteractionGrantAutomationTestTypes.h"

#include "Abilities/GameplayAbility.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Interaction/Tasks/AbilityTask_GrantNearbyInteraction.h"

namespace RpgInteractionGrantTaskTests
{
	class FScopedTestWorld
	{
	public:
		FScopedTestWorld()
		{
			GameInstance = NewObject<UGameInstance>(
				GEngine,
				NAME_None,
				RF_Transient);
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

		UWorld* GetWorld() const { return World; }

	private:
		TObjectPtr<UGameInstance> GameInstance = nullptr;
		TObjectPtr<UWorld> World = nullptr;
	};

	template <typename TActor>
	TActor* SpawnFixture(UWorld* World, const FVector& Location)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags = RF_Transient;
		SpawnParameters.Name = MakeUniqueObjectName(
			World,
			TActor::StaticClass(),
			TActor::StaticClass()->GetFName());
		return World->SpawnActor<TActor>(
			Location,
			FRotator::ZeroRotator,
			SpawnParameters);
	}

	int32 CountTemporaryInteractionSpecs(
		const URpgAbilitySystemComponent& AbilitySystem)
	{
		int32 Count = 0;
		for (const FGameplayAbilitySpec& Spec :
			AbilitySystem.GetActivatableAbilities())
		{
			if (Spec.Ability &&
				Spec.Ability->GetClass() ==
					URpgInteractionGrantAutomationGrantedAbility::
						StaticClass())
			{
				++Count;
			}
		}
		return Count;
	}

	FGameplayAbilitySpecHandle FindTemporaryInteractionSpecHandle(
		const URpgAbilitySystemComponent& AbilitySystem)
	{
		for (const FGameplayAbilitySpec& Spec :
			AbilitySystem.GetActivatableAbilities())
		{
			if (Spec.Ability &&
				Spec.Ability->GetClass() ==
					URpgInteractionGrantAutomationGrantedAbility::
						StaticClass())
			{
				return Spec.Handle;
			}
		}
		return FGameplayAbilitySpecHandle();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgGrantNearbySharedAbilityCacheLifecycleTest,
	"SurvivalRpg.Interaction.GrantNearby.SharedAbilityCacheLifecycle",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgGrantNearbySharedAbilityCacheLifecycleTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgInteractionGrantTaskTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	if (!TestNotNull(TEXT("Standalone interaction world exists"), World))
	{
		return false;
	}

	ARpgInteractionGrantAutomationPawn* Pawn =
		SpawnFixture<ARpgInteractionGrantAutomationPawn>(
			World,
			FVector::ZeroVector);
	if (!TestNotNull(TEXT("Authoritative ASC pawn spawns"), Pawn))
	{
		return false;
	}

	URpgAbilitySystemComponent* AbilitySystem =
		Pawn->GetRpgAbilitySystemComponent();
	if (!TestNotNull(TEXT("Pawn owns an initialized ASC"), AbilitySystem))
	{
		return false;
	}
	if (!Pawn->HasActorBegunPlay())
	{
		Pawn->DispatchBeginPlay();
	}
	AbilitySystem->InitAbilityActorInfo(Pawn, Pawn);
	TestTrue(
		TEXT("The standalone pawn has grant authority"),
		AbilitySystem->IsOwnerActorAuthoritative());

	UAbilityTask_GrantNearbyInteraction* GrantTask =
		UAbilityTask_GrantNearbyInteraction::CreateForTesting(
			AbilitySystem,
			500.0f);
	if (!TestNotNull(TEXT("The grant task fixture is created"), GrantTask))
	{
		return false;
	}
	GrantTask->StartQueryTimerForTesting();
	TestTrue(
		TEXT("The periodic nearby query timer starts"),
		GrantTask->IsQueryTimerActiveForTesting());

	const TSubclassOf<UGameplayAbility> SharedAbilityClass =
		URpgInteractionGrantAutomationGrantedAbility::StaticClass();
	GrantTask->ReconcileAbilityClassesForTesting(
		{SharedAbilityClass, SharedAbilityClass});

	TestEqual(
		TEXT(
			"Two nearby targets sharing one ability class create "
			"exactly one spec"),
		CountTemporaryInteractionSpecs(*AbilitySystem),
		1);
	const FGameplayAbilitySpecHandle SharedSpecHandle =
		FindTemporaryInteractionSpecHandle(*AbilitySystem);
	TestTrue(
		TEXT("The shared temporary spec has a valid handle"),
		SharedSpecHandle.IsValid());

	GrantTask->ReconcileAbilityClassesForTesting({SharedAbilityClass});
	TestEqual(
		TEXT("Removing one of two users retains the shared spec"),
		CountTemporaryInteractionSpecs(*AbilitySystem),
		1);
	TestEqual(
		TEXT("Reference-count changes retain the same temporary spec"),
		FindTemporaryInteractionSpecHandle(*AbilitySystem),
		SharedSpecHandle);

	GrantTask->ReconcileAbilityClassesForTesting({});
	TestEqual(
		TEXT("Removing the final user clears the temporary spec"),
		CountTemporaryInteractionSpecs(*AbilitySystem),
		0);

	GrantTask->ReconcileAbilityClassesForTesting(
		{SharedAbilityClass, SharedAbilityClass});
	TestEqual(
		TEXT("A subsequent overlap recreates only one shared spec"),
		CountTemporaryInteractionSpecs(*AbilitySystem),
		1);

	const FGameplayAbilitySpecHandle ActiveSpecHandle =
		FindTemporaryInteractionSpecHandle(*AbilitySystem);
	TestTrue(
		TEXT("The temporary ability can activate on authority"),
		AbilitySystem->TryActivateAbility(ActiveSpecHandle));
	FGameplayAbilitySpec* ActiveSpec =
		AbilitySystem->FindAbilitySpecFromHandle(ActiveSpecHandle);
	TestTrue(
		TEXT("The temporary spec remains active for lifecycle testing"),
		ActiveSpec && ActiveSpec->IsActive());

	GrantTask->ReconcileAbilityClassesForTesting({});
	ActiveSpec = AbilitySystem->FindAbilitySpecFromHandle(ActiveSpecHandle);
	TestTrue(
		TEXT("An active orphaned spec is retained until activation ends"),
		ActiveSpec && ActiveSpec->IsActive());
	TestTrue(
		TEXT("An active orphaned spec is marked for removal on end"),
		ActiveSpec && ActiveSpec->RemoveAfterActivation);

	GrantTask->ReconcileAbilityClassesForTesting({SharedAbilityClass});
	ActiveSpec = AbilitySystem->FindAbilitySpecFromHandle(ActiveSpecHandle);
	TestTrue(
		TEXT("A returning target cancels deferred spec removal"),
		ActiveSpec && !ActiveSpec->RemoveAfterActivation);
	TestEqual(
		TEXT("A returning target keeps the active spec handle"),
		FindTemporaryInteractionSpecHandle(*AbilitySystem),
		ActiveSpecHandle);

	GrantTask->EndTask();
	TestFalse(
		TEXT("Task OnDestroy clears the periodic nearby query timer"),
		GrantTask->IsQueryTimerActiveForTesting());
	ActiveSpec = AbilitySystem->FindAbilitySpecFromHandle(ActiveSpecHandle);
	TestTrue(
		TEXT("Task OnDestroy defers removal of an active temporary spec"),
		ActiveSpec && ActiveSpec->RemoveAfterActivation);
	AbilitySystem->CancelAbilityHandle(ActiveSpecHandle);
	TestEqual(
		TEXT("Ending the active ability leaves no temporary spec"),
		CountTemporaryInteractionSpecs(*AbilitySystem),
		0);

	return true;
}

#endif
