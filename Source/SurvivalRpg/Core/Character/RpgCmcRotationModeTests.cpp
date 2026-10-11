// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "RpgCharacterMovementComponent.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
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
#include "SurvivalRpg/Equipment/RpgEquipmentAutomationTestTypes.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Interaction/RpgInteractionGrantAutomationTestTypes.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"

#include <limits>

namespace RpgCmcRotationModeTests
{
	// Fixture values, not assertions about a designer movement profile.
	constexpr float RunSpeed = 310.f;
	constexpr float SprintSpeed = 530.f;
	constexpr float StepSeconds = .05f;
	constexpr uint8 SprintRequest = FSavedMove_Character::FLAG_Custom_0;

	bool IsNearYaw(double Actual, double Expected, double Tolerance = .5)
	{
		return FMath::Abs(FMath::FindDeltaAngleDegrees(Actual, Expected)) <= Tolerance;
	}

	/** Real CMC floor physics, equipment and GAS leases on a rotation-mode character, without an Experience or AnimBP. */
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
			if (ASC) { ASC->ClearAllAbilities(); ASC->ClearAbilityInput(); }
			if (Extension) Extension->UninitializeAbilitySystem();
			GameInstance->Shutdown();
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
				World->MarkObjectsPendingKill();
			}
			// Finish the owned world's teardown before another latent fixture ticks Slate (see the sprint fixture).
			GameInstance.Reset();
			CollectGarbage(RF_NoFlags, true);
		}

		bool Initialize(FAutomationTestBase& Test)
		{
			if (!Test.TestNotNull(TEXT("Standalone rotation world exists"), World)) return false;
			AActor* Floor = World->SpawnActor<AActor>();
			UBoxComponent* Box = NewObject<UBoxComponent>(Floor);
			Floor->AddInstanceComponent(Box);
			Floor->SetRootComponent(Box);
			Box->SetBoxExtent(FVector(100000.f, 100000.f, 50.f));
			Box->SetCollisionProfileName(TEXT("BlockAll"));
			Floor->SetActorLocation(FVector(0.f, 0.f, -50.f));
			Box->SetMobility(EComponentMobility::Static);
			Box->RegisterComponent();
			Character = World->SpawnActor<ARpgCharacter>();
			Controller = World->SpawnActor<APlayerController>();
			if (!Test.TestNotNull(TEXT("Rotation avatar exists"), Character)
				|| !Test.TestNotNull(TEXT("Real control rotation owner exists"), Controller)) return false;
			Controller->SetAsLocalPlayerController();
			Controller->Possess(Character);
			Character->SetActorLocation(FVector(0.f, 0.f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f));
			Character->SetActorRotation(FRotator::ZeroRotator);
			Character->SetActorTickEnabled(false);
			Controller->SetActorTickEnabled(false);
			Controller->SetControlRotation(FRotator::ZeroRotator);
			Movement = CastChecked<URpgCharacterMovementComponent>(Character->GetCharacterMovement());
			Movement->SetComponentTickEnabled(false); // Only explicit MoveAutonomous calls advance movement.
			Movement->bUseRotationModes = true;
			Movement->GroundRotationRate = 0.f;
			Movement->MaxWalkSpeed = RunSpeed;
			Movement->SprintSpeed = SprintSpeed;
			Movement->bEnableGASSprint = true;
			Movement->SetMovementMode(MOVE_Walking);
			ASC = NewObject<URpgAbilitySystemComponent>(Character, NAME_None, RF_Transient);
			Character->AddInstanceComponent(ASC);
			ASC->RegisterComponent();
			ASC->AddAttributeSetSubobject(NewObject<URpgHealthSet>(Character, NAME_None, RF_Transient));
			Extension = URpgPawnExtensionComponent::FindPawnExtensionComponent(Character);
			Extension->SetPawnData(NewObject<URpgPawnData>(Character, NAME_None, RF_Transient));
			Extension->InitializeAbilitySystemComponent(ASC, Character);
			Handle = Grant();
			if (!Test.TestTrue(TEXT("Inert GAS owner really activates"), ASC->TryActivateAbility(Handle))) return false;
			Ability = ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance();
			Move(FVector::ZeroVector);
			return Test.TestTrue(TEXT("CMC finds the real support surface"), Movement->IsMovingOnGround() && Movement->CurrentFloor.IsWalkableFloor());
		}

		FGameplayAbilitySpecHandle Grant()
		{
			return ASC->GiveAbility(FGameplayAbilitySpec(URpgInteractionGrantAutomationGrantedAbility::StaticClass(), 1));
		}
		/** Runs the real received-move entry point with full input toward Direction (zero for no input). */
		void Move(const FVector& Direction, uint8 Flags = 0, float Seconds = StepSeconds)
		{
			TimeStamp += Seconds;
			Movement->MoveAutonomous(TimeStamp, Seconds, Flags, Direction.GetSafeNormal2D() * Movement->GetMaxAcceleration());
		}
		void MoveSteps(int32 Count, const FVector& Direction, uint8 Flags = 0)
		{
			for (int32 Index = 0; Index < Count; ++Index) Move(Direction, Flags);
		}
		void SetCameraYaw(double Yaw) { Controller->SetControlRotation(FRotator(0.0, Yaw, 0.0)); }
		double GetYaw() const { return Character->GetActorRotation().Yaw; }
		URpgEquipmentInstance* EquipHands()
		{
			return Character->GetEquipmentManagerComponent()->EquipItemInSlot(
				URpgEquipmentAutomationTestSwordDefinition::StaticClass(), ERpgEquipmentSlot::MainHand);
		}
		/** Records one local saved move exactly as ReplicateMoveToServer would before sending it. */
		FSavedMovePtr RecordMove(const FVector& Direction)
		{
			FNetworkPredictionData_Client_Character* Prediction = static_cast<FNetworkPredictionData_Client_Character*>(
				Movement->GetPredictionData_Client());
			FSavedMovePtr Saved = Prediction->AllocateNewMove();
			Saved->Clear();
			const FVector Accel = Direction.GetSafeNormal2D() * Movement->GetMaxAcceleration();
			Saved->SetMoveFor(Character, StepSeconds, Accel, *Prediction);
			TimeStamp += StepSeconds;
			Movement->MoveAutonomous(TimeStamp, StepSeconds, Saved->GetCompressedFlags(), Accel);
			Saved->PostUpdate(Character, FSavedMove_Character::PostUpdate_Record);
			return Saved;
		}
		/** Replays a recorded move through the saved-move callbacks used by ClientUpdatePositionAfterServerUpdate. */
		void ReplayMove(const FSavedMovePtr& Saved)
		{
			Character->bClientUpdating = true;
			Movement->SetCurrentReplayedSavedMove(Saved.Get());
			Saved->PrepMoveFor(Character);
			Movement->MoveAutonomous(Saved->TimeStamp, Saved->DeltaTime, Saved->GetCompressedFlags(), Saved->Acceleration);
			Saved->PostUpdate(Character, FSavedMove_Character::PostUpdate_Replay);
			Movement->SetCurrentReplayedSavedMove(nullptr);
			Character->bClientUpdating = false;
		}

		TStrongObjectPtr<UGameInstance> GameInstance;
		UWorld* World = nullptr;
		ARpgCharacter* Character = nullptr;
		APlayerController* Controller = nullptr;
		URpgCharacterMovementComponent* Movement = nullptr;
		URpgAbilitySystemComponent* ASC = nullptr;
		URpgPawnExtensionComponent* Extension = nullptr;
		UGameplayAbility* Ability = nullptr;
		FGameplayAbilitySpecHandle Handle;
		float TimeStamp = 0.f;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgRotationModeFreeMovementTest,
	"SurvivalRpg.Movement.CMC.RotationModes.FreeMovementFacesInputAndIgnoresCameraYaw",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRpgRotationModeFreeMovementTest::RunTest(const FString& Parameters)
{
	using namespace RpgCmcRotationModeTests;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	TestFalse(TEXT("Empty hands are free movement"), F.Movement->IsInCombatStance());
	F.SetCameraYaw(90.0);
	F.Character->FaceRotation(F.Controller->GetControlRotation(), StepSeconds);
	TestTrue(TEXT("Controller yaw never writes the rotation-mode facing"), IsNearYaw(F.GetYaw(), 0.0));
	F.Move(FVector(0.0, -1.0, 0.0));
	TestTrue(TEXT("Free movement turns to the input at once on the ground"), IsNearYaw(F.GetYaw(), -90.0));
	F.MoveSteps(4, FVector(-1.0, 0.0, 0.0));
	TestTrue(TEXT("Facing follows a new input direction, not the camera"), IsNearYaw(F.GetYaw(), 180.0));
	F.MoveSteps(4, FVector::ZeroVector);
	TestTrue(TEXT("Without input the last facing is kept"), IsNearYaw(F.GetYaw(), 180.0));

	F.Movement->bUseRotationModes = false;
	F.Character->FaceRotation(F.Controller->GetControlRotation(), StepSeconds);
	TestTrue(TEXT("Characters without rotation modes keep their authored controller yaw"), IsNearYaw(F.GetYaw(), 90.0));
	TestFalse(TEXT("Characters without rotation modes never report a stance"), F.Movement->IsInCombatStance());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgRotationModeCombatStanceTest,
	"SurvivalRpg.Movement.CMC.RotationModes.HandEquipmentFacesCameraAndReplaysRecordedStance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRpgRotationModeCombatStanceTest::RunTest(const FString& Parameters)
{
	using namespace RpgCmcRotationModeTests;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	URpgEquipmentInstance* Sword = F.EquipHands();
	if (!TestNotNull(TEXT("Authority applies main-hand equipment"), Sword)) return false;
	TestTrue(TEXT("The pawn's replicated equipment list reports hands in use"), F.Character->GetEquipmentManagerComponent()->HasEquipmentInHands());
	TestTrue(TEXT("Equipment in hands is the combat stance"), F.Movement->IsInCombatStance());
	F.SetCameraYaw(90.0);
	F.MoveSteps(4, FVector(1.0, 0.0, 0.0));
	TestTrue(TEXT("The stance faces the camera while strafing"), IsNearYaw(F.GetYaw(), 90.0));
	F.MoveSteps(2, FVector::ZeroVector);
	TestTrue(TEXT("The stance keeps facing the camera while standing"), IsNearYaw(F.GetYaw(), 90.0));

	F.SetCameraYaw(45.0);
	const FSavedMovePtr Saved = F.RecordMove(FVector(-1.0, 0.0, 0.0));
	TestTrue(TEXT("The recorded move faces its camera yaw"), IsNearYaw(F.GetYaw(), 45.0));
	F.Character->GetEquipmentManagerComponent()->UnequipItem(Sword);
	TestFalse(TEXT("Holstering ends today's stance"), F.Movement->IsInCombatStance());
	for (double LiveYaw : { -60.0, 155.0 })
	{
		F.SetCameraYaw(LiveYaw);
		F.Character->SetActorRotation(FRotator::ZeroRotator);
		F.ReplayMove(Saved);
		TestTrue(TEXT("Replay uses the recorded stance and camera yaw, not today's state"), IsNearYaw(F.GetYaw(), 45.0));
		TestTrue(TEXT("Replay never rewrites the live camera"), IsNearYaw(F.Controller->GetControlRotation().Yaw, LiveYaw, .01));
	}
	F.SetCameraYaw(-30.0);
	F.MoveSteps(4, FVector(0.0, 1.0, 0.0));
	TestTrue(TEXT("After holstering, live moves return to free movement"), IsNearYaw(F.GetYaw(), 90.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgRotationModeFacingCommitmentTest,
	"SurvivalRpg.Movement.CMC.RotationModes.FacingCommitmentSteersThenHoldsAndReplays",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRpgRotationModeFacingCommitmentTest::RunTest(const FString& Parameters)
{
	using namespace RpgCmcRotationModeTests;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	if (!TestNotNull(TEXT("Authority applies main-hand equipment"), F.EquipHands())) return false;
	F.Move(FVector::ZeroVector);
	TestEqual(TEXT("Invalid steering rates never create a lease"), F.ASC->BeginFacingCommitment(F.Ability, std::numeric_limits<float>::quiet_NaN()), 0u);
	const uint32 Lease = F.ASC->BeginFacingCommitment(F.Ability, 100.f);
	if (!TestTrue(TEXT("The active ability receives a facing lease"), Lease != 0)) return false;
	F.SetCameraYaw(90.0);
	F.Move(FVector::ZeroVector);
	TestTrue(TEXT("Steering turns toward the camera at the leased rate"), IsNearYaw(F.GetYaw(), 100.0 * StepSeconds, .05));
	const FSavedMovePtr Steering = F.RecordMove(FVector::ZeroVector);
	TestTrue(TEXT("A recorded steering move advances at the same rate"), IsNearYaw(F.GetYaw(), 200.0 * StepSeconds, .05));
	F.ASC->HoldFacingCommitment(F.Ability, Lease);
	F.MoveSteps(3, FVector(1.0, 0.0, 0.0));
	TestTrue(TEXT("A held commitment keeps the facing while the camera differs"), IsNearYaw(F.GetYaw(), 200.0 * StepSeconds, .05));

	const FGameplayAbilitySpecHandle OtherHandle = F.Grant();
	TestTrue(TEXT("Second genuine GAS activation exists"), F.ASC->TryActivateAbility(OtherHandle));
	UGameplayAbility* Other = F.ASC->FindAbilitySpecFromHandle(OtherHandle)->GetPrimaryInstance();
	F.ASC->EndFacingCommitment(Other, Lease);
	float Rate = -1.f;
	TestTrue(TEXT("Another activation cannot release the lease"), F.ASC->GetFacingCommitment(Rate));
	const uint32 Chained = F.ASC->BeginFacingCommitment(Other, 0.f);
	TestTrue(TEXT("A chained activation replaces the commitment"), Chained != 0 && Chained != Lease);
	F.ASC->EndFacingCommitment(F.Ability, Lease);
	TestTrue(TEXT("Delayed cleanup of the old activation keeps the new commitment"), F.ASC->GetFacingCommitment(Rate) && Rate == 0.f);
	F.ASC->CancelAbilityHandle(OtherHandle);
	TestFalse(TEXT("GAS cancellation retires the commitment"), F.ASC->GetFacingCommitment(Rate));
	F.Move(FVector::ZeroVector);
	TestTrue(TEXT("Without a commitment the stance faces the camera again"), IsNearYaw(F.GetYaw(), 90.0));

	F.SetCameraYaw(-60.0);
	F.Character->SetActorRotation(FRotator(0.0, 5.0, 0.0));
	F.ReplayMove(Steering);
	TestTrue(TEXT("Replay steers toward the recorded camera at the recorded rate"), IsNearYaw(F.GetYaw(), 10.0, .05));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgRotationModeSprintDirectionTest,
	"SurvivalRpg.Movement.CMC.RotationModes.CombatStanceSprintsOnlyTowardCamera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRpgRotationModeSprintDirectionTest::RunTest(const FString& Parameters)
{
	using namespace RpgCmcRotationModeTests;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	if (!TestTrue(TEXT("A valid authority sprint lease exists"), F.ASC->BeginSprintMovement(F.Ability, SprintSpeed) != 0)) return false;
	F.MoveSteps(16, FVector(-1.0, 0.0, 0.0), SprintRequest);
	TestTrue(TEXT("Free movement sprints in any input direction"), F.Movement->IsSprinting());
	TestTrue(TEXT("Free backward input reaches sprint speed"), FMath::IsNearlyEqual(F.Movement->Velocity.Size2D(), SprintSpeed, .5f));

	if (!TestNotNull(TEXT("Authority applies main-hand equipment"), F.EquipHands())) return false;
	F.MoveSteps(16, FVector(-1.0, 0.0, 0.0), SprintRequest);
	TestFalse(TEXT("The stance cannot sprint backward"), F.Movement->IsSprinting());
	TestTrue(TEXT("Backward stance input keeps the run speed"), FMath::IsNearlyEqual(F.Movement->Velocity.Size2D(), RunSpeed, .5f));
	F.MoveSteps(16, FVector(0.0, 1.0, 0.0), SprintRequest);
	TestFalse(TEXT("The stance cannot sprint sideways"), F.Movement->IsSprinting());
	F.MoveSteps(16, FVector(1.0, 1.0, 0.0), SprintRequest);
	TestTrue(TEXT("Input within the configured angle of the camera sprints"), F.Movement->IsSprinting());
	F.MoveSteps(16, FVector(1.0, 0.0, 0.0), SprintRequest);
	TestTrue(TEXT("Forward stance input sprints"), F.Movement->IsSprinting());
	TestTrue(TEXT("Forward stance input reaches sprint speed"), FMath::IsNearlyEqual(F.Movement->Velocity.Size2D(), SprintSpeed, .5f));
	F.SetCameraYaw(180.0);
	F.MoveSteps(2, FVector(1.0, 0.0, 0.0), SprintRequest);
	TestFalse(TEXT("Turning the camera away ends the stance sprint at once"), F.Movement->IsSprinting());
	return true;
}

#endif
