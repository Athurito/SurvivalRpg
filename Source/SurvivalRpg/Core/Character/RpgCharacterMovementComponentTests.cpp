// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "RpgCharacterMovementComponent.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "RpgCharacter.h"
#include "RpgPawnData.h"
#include "RpgPawnExtensionComponent.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Interaction/RpgInteractionGrantAutomationTestTypes.h"
#include "UObject/StrongObjectPtr.h"

namespace RpgCharacterBlockReplayTests
{
	class FFixture
	{
	public:
		FFixture()
		{
			GameInstance.Reset(NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient));
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}
		~FFixture()
		{
			if (Character) Character->bClientUpdating = false;
			if (ASC)
			{
				ASC->EndBlockMovement(Ability, Lease);
				ASC->ClearAllAbilities();
			}
			if (Extension) Extension->UninitializeAbilitySystem();
			GameInstance->Shutdown();
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}

		bool Initialize(FAutomationTestBase& Test)
		{
			if (!Test.TestNotNull(TEXT("Native replay world exists"), World)) return false;
			Character = World->SpawnActor<ARpgCharacter>();
			Controller = World->SpawnActor<APlayerController>();
			if (!Test.TestNotNull(TEXT("RPG character exists"), Character)
				|| !Test.TestNotNull(TEXT("Real control rotation owner exists"), Controller)) return false;
			Controller->Possess(Character);
			Movement = CastChecked<URpgCharacterMovementComponent>(Character->GetCharacterMovement());
			// Isolate the native rotation/replay seam from floor geometry and animation assets.
			Movement->SetMovementMode(MOVE_Flying);
			Movement->RotationRate = FRotator(0.f, 2000.f, 0.f);
			ASC = NewObject<URpgAbilitySystemComponent>(Character, NAME_None, RF_Transient);
			Character->AddInstanceComponent(ASC);
			ASC->RegisterComponent();
			ASC->AddAttributeSetSubobject(NewObject<URpgHealthSet>(Character, NAME_None, RF_Transient));
			Extension = URpgPawnExtensionComponent::FindPawnExtensionComponent(Character);
			Extension->SetPawnData(NewObject<URpgPawnData>(Character, NAME_None, RF_Transient));
			Extension->InitializeAbilitySystemComponent(ASC, Character);

			// Reuse an inert instanced GAS fixture to acquire the public activation-scoped movement lease.
			// Equipment/input behavior is covered separately; this regression exercises the saved-move mechanism.
			const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(
				FGameplayAbilitySpec(URpgInteractionGrantAutomationGrantedAbility::StaticClass(), 1));
			if (!Test.TestTrue(TEXT("The real GAS owner is active"), ASC->TryActivateAbility(Handle))) return false;
			Ability = ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance();
			Lease = ASC->BeginBlockMovement(Ability, 150.f);
			return Test.TestTrue(TEXT("A validated block movement lease is active"), Lease != 0 && ASC->IsBlockMovementActive());
		}

		TStrongObjectPtr<UGameInstance> GameInstance;
		UWorld* World = nullptr;
		ARpgCharacter* Character = nullptr;
		APlayerController* Controller = nullptr;
		URpgCharacterMovementComponent* Movement = nullptr;
		URpgPawnExtensionComponent* Extension = nullptr;
		URpgAbilitySystemComponent* ASC = nullptr;
		UGameplayAbility* Ability = nullptr;
		uint32 Lease = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgBlockRepeatedSavedMoveYawTest,
	"SurvivalRpg.Combat.Block.Prediction.CMCRepeatedReplayPreservesOriginalControlYaw",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgBlockRepeatedSavedMoveYawTest::RunTest(const FString& Parameters)
{
	using namespace RpgCharacterBlockReplayTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	FNetworkPredictionData_Client_Character* Prediction = static_cast<FNetworkPredictionData_Client_Character*>(
		Fixture.Movement->GetPredictionData_Client());
	const FSavedMovePtr Move = Prediction->AllocateNewMove();
	Move->Clear();
	Fixture.Controller->SetControlRotation(FRotator(0.f, 90.f, 0.f));
	Move->SetMoveFor(Fixture.Character, .1f, FVector::ZeroVector, *Prediction);
	Fixture.Movement->PerformMovement(Move->DeltaTime);
	Move->PostUpdate(Fixture.Character, FSavedMove_Character::PostUpdate_Record);
	TestTrue(TEXT("Original block movement faces the recorded control yaw"),
		FMath::IsNearlyEqual(Fixture.Character->GetActorRotation().Yaw, 90.0, .01));

	Fixture.ASC->EndBlockMovement(Fixture.Ability, Fixture.Lease);
	TestFalse(TEXT("Today's activation has ended before historical movement replays"), Fixture.ASC->IsBlockMovementActive());
	for (double LiveYaw : { -60.0, 155.0 })
	{
		Fixture.Controller->SetControlRotation(FRotator(0.0, LiveYaw, 0.0));
		Fixture.Character->SetActorRotation(FRotator::ZeroRotator);
		// These are the real saved-move callbacks and movement scope used by
		// ClientUpdatePositionAfterServerUpdate, without manufacturing a network correction witness.
		Fixture.Character->bClientUpdating = true;
		Fixture.Movement->SetCurrentReplayedSavedMove(Move.Get());
		Move->PrepMoveFor(Fixture.Character);
		Fixture.Movement->PerformMovement(Move->DeltaTime);
		Move->PostUpdate(Fixture.Character, FSavedMove_Character::PostUpdate_Replay);
		Fixture.Movement->SetCurrentReplayedSavedMove(nullptr);
		Fixture.Character->bClientUpdating = false;
		TestTrue(TEXT("Each replay keeps the original block-facing yaw after the live camera changes"),
			FMath::IsNearlyEqual(Fixture.Character->GetActorRotation().Yaw, 90.0, .01));
		TestTrue(TEXT("Replay never rewrites the live camera"),
			FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Fixture.Controller->GetControlRotation().Yaw, LiveYaw), .01));
		TestTrue(TEXT("The engine really overwrites its stock saved rotation after replay"),
			FMath::IsNearlyEqual(Move->SavedControlRotation.Yaw, FRotator(0.0, LiveYaw, 0.0).Clamp().Yaw, .01));
	}
	float SpeedLimit = -1.f;
	TestFalse(TEXT("Historical block state does not leak outside replay"), Fixture.Movement->GetBlockMovementForMove(SpeedLimit));
	TestEqual(TEXT("Released movement has no stale cap"), SpeedLimit, 0.f);
	return true;
}

#endif
