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
#include "RpgMovementProfile.h"
#include "RpgPawnData.h"
#include "RpgPawnExtensionComponent.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentAutomationTestTypes.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Interaction/RpgInteractionGrantAutomationTestTypes.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"

namespace RpgMovementProfileTests
{
	constexpr uint8 SprintRequest = FSavedMove_Character::FLAG_Custom_0;
	constexpr uint8 WalkToggle = FSavedMove_Character::FLAG_Custom_1;
	constexpr uint8 StickWalk = FSavedMove_Character::FLAG_Custom_2;
	const float FrameSteps[] = { 1.f / 30.f, 1.f / 60.f, 1.f / 120.f };

	/** Elapsed time and horizontal travel of one measured motion. */
	struct FMotion
	{
		float Seconds = 0.f;
		float Distance = 0.f;
		bool bReached = false;
	};

	/** Real CMC floor physics with the shared profile class defaults, a GAS sprint lease and rotation modes, without an AnimBP. */
	class FFixture
	{
	public:
		explicit FFixture(float InStepSeconds) : StepSeconds(InStepSeconds)
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
			if (!Test.TestNotNull(TEXT("Standalone profile world exists"), World)) return false;
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
			if (!Test.TestNotNull(TEXT("Profile avatar exists"), Character)
				|| !Test.TestNotNull(TEXT("Profile controller exists"), Controller)) return false;
			Controller->SetAsLocalPlayerController();
			Controller->Possess(Character);
			Character->SetActorLocation(FVector(0.f, 0.f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f));
			Character->SetActorRotation(FRotator::ZeroRotator);
			Character->SetActorTickEnabled(false);
			Controller->SetActorTickEnabled(false);
			Controller->SetControlRotation(FRotator::ZeroRotator);
			Movement = CastChecked<URpgCharacterMovementComponent>(Character->GetCharacterMovement());
			Movement->SetComponentTickEnabled(false); // Only explicit MoveAutonomous calls advance movement.
			Profile = NewObject<URpgMovementProfile>(GetTransientPackage(), NAME_None, RF_Transient);
			Movement->MovementProfile = Profile;
			Movement->ApplyMovementProfile();
			Movement->bEnableGASSprint = true;
			Movement->bUseRotationModes = true;
			Movement->SetMovementMode(MOVE_Walking);
			ASC = NewObject<URpgAbilitySystemComponent>(Character, NAME_None, RF_Transient);
			Character->AddInstanceComponent(ASC);
			ASC->RegisterComponent();
			ASC->AddAttributeSetSubobject(NewObject<URpgHealthSet>(Character, NAME_None, RF_Transient));
			Extension = URpgPawnExtensionComponent::FindPawnExtensionComponent(Character);
			Extension->SetPawnData(NewObject<URpgPawnData>(Character, NAME_None, RF_Transient));
			Extension->InitializeAbilitySystemComponent(ASC, Character);
			Handle = ASC->GiveAbility(FGameplayAbilitySpec(URpgInteractionGrantAutomationGrantedAbility::StaticClass(), 1));
			if (!Test.TestTrue(TEXT("Inert GAS owner really activates"), ASC->TryActivateAbility(Handle))) return false;
			Ability = ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance();
			Move(FVector::ZeroVector);
			return Test.TestTrue(TEXT("CMC finds the real support surface"), Movement->IsMovingOnGround() && Movement->CurrentFloor.IsWalkableFloor());
		}

		/** Runs the real received-move entry point with full input toward Direction (zero for no input). */
		void Move(const FVector& Direction, uint8 Flags = 0)
		{
			TimeStamp += StepSeconds;
			Movement->MoveAutonomous(TimeStamp, StepSeconds, Flags, Direction.GetSafeNormal2D() * Movement->GetMaxAcceleration());
		}
		float Speed() const { return static_cast<float>(Movement->Velocity.Size2D()); }
		/** Runs the owner's real input path: stick interpretation and normalization, then an authority move. */
		void Control(const FVector& Input)
		{
			TimeStamp += StepSeconds;
			Movement->ControlledCharacterMove(Input, StepSeconds);
		}
		void Hold(const FVector& Input, int32 Steps)
		{
			for (int32 Index = 0; Index < Steps; ++Index) Control(Input);
		}
		/** Controls until Done holds, at most MaxSeconds, and reports time and horizontal travel. */
		template <typename TDone>
		FMotion ControlUntil(const FVector& Input, TDone Done, float MaxSeconds = 3.f)
		{
			FMotion Motion;
			const FVector Start = Character->GetActorLocation();
			while (Motion.Seconds < MaxSeconds && !(Motion.bReached = Done()))
			{
				Control(Input);
				Motion.Seconds += StepSeconds;
			}
			Motion.Distance = static_cast<float>(FVector::Dist2D(Start, Character->GetActorLocation()));
			return Motion;
		}
		void SetStickWalk(bool bWalk) { Movement->bStickWalk = bWalk; }
		void SetReplicatedGait(ERpgMovementGait InGait) { Movement->Gait = InGait; }
		/** Moves until Done holds, at most MaxSeconds, and reports time and horizontal travel. */
		template <typename TDone>
		FMotion MoveUntil(const FVector& Direction, uint8 Flags, TDone Done, float MaxSeconds = 3.f)
		{
			FMotion Motion;
			const FVector Start = Character->GetActorLocation();
			while (Motion.Seconds < MaxSeconds && !(Motion.bReached = Done()))
			{
				Move(Direction, Flags);
				Motion.Seconds += StepSeconds;
			}
			Motion.Distance = static_cast<float>(FVector::Dist2D(Start, Character->GetActorLocation()));
			return Motion;
		}
		bool BeginSprint() { return (SprintLease = ASC->BeginSprintMovement(Ability, Movement->GetSprintSpeed())) != 0; }
		void EndSprint() { ASC->EndSprintMovement(Ability, SprintLease); SprintLease = 0; }
		double GetYaw() const { return Character->GetActorRotation().Yaw; }
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

		const float StepSeconds;
		TStrongObjectPtr<UGameInstance> GameInstance;
		UWorld* World = nullptr;
		ARpgCharacter* Character = nullptr;
		APlayerController* Controller = nullptr;
		URpgCharacterMovementComponent* Movement = nullptr;
		URpgMovementProfile* Profile = nullptr;
		URpgAbilitySystemComponent* ASC = nullptr;
		URpgPawnExtensionComponent* Extension = nullptr;
		UGameplayAbility* Ability = nullptr;
		FGameplayAbilitySpecHandle Handle;
		uint32 SprintLease = 0;
		float TimeStamp = 0.f;
	};

	/** Analytic constant-rate time and distance; the discrete moves land within one step of time and one step of travel. */
	void TestMotion(FAutomationTestBase& Test, const FString& What, const FMotion& Motion, float Seconds, float StepSeconds,
		float Distance = -1.f, float TravelSpeed = 0.f)
	{
		Test.TestTrue(What + TEXT(" completes"), Motion.bReached);
		Test.TestTrue(FString::Printf(TEXT("%s takes %.3f s, predicted %.3f s within one %.4f s step"), *What, Motion.Seconds, Seconds, StepSeconds),
			FMath::Abs(Motion.Seconds - Seconds) <= StepSeconds + 1.e-3f);
		if (Distance >= 0.f)
		{
			Test.TestTrue(FString::Printf(TEXT("%s covers %.1f cm, predicted %.1f cm within one step of travel"), *What, Motion.Distance, Distance),
				FMath::Abs(Motion.Distance - Distance) <= TravelSpeed * StepSeconds + .5f);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgMovementProfileStaticValuesTest,
	"SurvivalRpg.Movement.CMC.Profile.AppliesStaticValuesAndValidates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRpgMovementProfileStaticValuesTest::RunTest(const FString& Parameters)
{
	using namespace RpgMovementProfileTests;
	FFixture F(FrameSteps[1]);
	if (!F.Initialize(*this)) return false;
	FText Error;
	TestTrue(TEXT("The shared start values validate"), F.Profile->ValidateConfiguration(Error));
	TestEqual(TEXT("Run speed comes from the profile"), F.Movement->MaxWalkSpeed, F.Profile->RunSpeed);
	TestEqual(TEXT("Crouch speed comes from the profile"), F.Movement->MaxWalkSpeedCrouched, F.Profile->CrouchSpeed);
	TestEqual(TEXT("Sprint speed comes from the profile"), F.Movement->GetSprintSpeed(), F.Profile->SprintSpeed);
	TestEqual(TEXT("Input acceleration is the run acceleration"), F.Movement->GetMaxAcceleration(), F.Profile->Acceleration);
	TestEqual(TEXT("Jump Z comes from the profile"), F.Movement->JumpZVelocity, F.Profile->JumpZVelocity);
	TestEqual(TEXT("Air control comes from the profile"), F.Movement->AirControl, F.Profile->AirControl);
	TestEqual(TEXT("Falling braking comes from the profile"), F.Movement->BrakingDecelerationFalling, F.Profile->FallingBrakingDeceleration);
	TestTrue(TEXT("Braking uses the profile's friction factor, not a separate friction"),
		!F.Movement->bUseSeparateBrakingFriction && F.Movement->BrakingFrictionFactor == F.Profile->BrakingFrictionFactor);
	TestEqual(TEXT("The trajectory-visible ground friction is the run value"), F.Movement->GroundFriction, F.Profile->GroundFriction);
	TestEqual(TEXT("Idle ground braking is the no-input value"), F.Movement->GetMaxBrakingDeceleration(), F.Profile->BrakingDeceleration);
	TestEqual(TEXT("The taper starts at run speed"), F.Profile->GetAccelerationAtSpeed(F.Profile->RunSpeed), F.Profile->Acceleration);
	TestEqual(TEXT("The taper ends at sprint speed"), F.Profile->GetAccelerationAtSpeed(F.Profile->SprintSpeed), F.Profile->SprintAcceleration);
	TestEqual(TEXT("Friction tapers to the sprint value"), F.Profile->GetGroundFrictionAtSpeed(F.Profile->SprintSpeed + 50.f), F.Profile->SprintGroundFriction);

	URpgMovementProfile* Invalid = NewObject<URpgMovementProfile>(GetTransientPackage(), NAME_None, RF_Transient);
	Invalid->SprintSpeed = Invalid->RunSpeed;
	TestFalse(TEXT("Sprint speed must exceed run speed"), Invalid->ValidateConfiguration(Error));
	Invalid->SprintSpeed = 585.f;
	Invalid->WalkSpeed = Invalid->RunSpeed;
	TestFalse(TEXT("Run speed must exceed walk speed"), Invalid->ValidateConfiguration(Error));
	Invalid->WalkSpeed = 165.f;
	Invalid->AirControl = 1.5f;
	TestFalse(TEXT("Air control above 1 is rejected"), Invalid->ValidateConfiguration(Error));

	F.Movement->MovementProfile = nullptr;
	const float StockBraking = F.Movement->BrakingDecelerationWalking;
	TestEqual(TEXT("Without a profile ground braking is the stock value"), F.Movement->GetMaxBrakingDeceleration(), StockBraking);
	TestEqual(TEXT("Without a profile sprint reads the profile class defaults"), F.Movement->GetSprintSpeed(), GetDefault<URpgMovementProfile>()->SprintSpeed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgMovementProfileStartStopTest,
	"SurvivalRpg.Movement.CMC.Profile.StartStopAndSprintMatchPrediction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRpgMovementProfileStartStopTest::RunTest(const FString& Parameters)
{
	using namespace RpgMovementProfileTests;
	for (const float Step : FrameSteps)
	{
		FFixture F(Step);
		if (!F.Initialize(*this)) return false;
		const URpgMovementProfile& P = *F.Profile;
		const FString Fps = FString::Printf(TEXT("%.0f FPS: "), 1.f / Step);
		const FVector Forward(1.0, 0.0, 0.0);

		TestMotion(*this, Fps + TEXT("Start to 95 % run speed"),
			F.MoveUntil(Forward, 0, [&F, &P] { return F.Speed() >= .95f * P.RunSpeed; }), .95f * P.RunSpeed / P.Acceleration, Step);
		F.MoveUntil(Forward, 0, [&F, &P] { return F.Speed() >= P.RunSpeed - .01f; });
		TestMotion(*this, Fps + TEXT("Stop from run"),
			F.MoveUntil(FVector::ZeroVector, 0, [&F] { return F.Speed() < 1.f; }), P.RunSpeed / P.BrakingDeceleration, Step,
			FMath::Square(P.RunSpeed) / (2.f * P.BrakingDeceleration), P.RunSpeed);

		// Sprint from run speed: acceleration tapers linearly with speed, so the continuous time is logarithmic.
		F.MoveUntil(Forward, 0, [&F, &P] { return F.Speed() >= P.RunSpeed - .01f; });
		if (!TestTrue(Fps + TEXT("Sprint lease starts"), F.BeginSprint())) return false;
		const float Slope = (P.Acceleration - P.SprintAcceleration) / (P.SprintSpeed - P.RunSpeed);
		const float SprintSeconds = FMath::Loge(P.Acceleration / P.GetAccelerationAtSpeed(.95f * P.SprintSpeed)) / Slope;
		TestMotion(*this, Fps + TEXT("Run to 95 % sprint speed"),
			F.MoveUntil(Forward, SprintRequest, [&F, &P] { return F.Speed() >= .95f * P.SprintSpeed; }), SprintSeconds, Step);
		const FMotion Full = F.MoveUntil(Forward, SprintRequest, [&F, &P] { return F.Speed() >= P.SprintSpeed - .01f; });
		TestTrue(Fps + TEXT("The tapered input keeps the sprint gate open up to sprint speed"), Full.bReached && F.Movement->IsSprinting());

		// Releasing sprint while still holding input brakes gently down to run speed and never below it.
		F.EndSprint();
		TestMotion(*this, Fps + TEXT("Sprint release to run speed"),
			F.MoveUntil(Forward, 0, [&F, &P] { return F.Speed() <= P.RunSpeed + .01f; }),
			(P.SprintSpeed - P.RunSpeed) / P.BrakingDecelerationWithInput, Step);
		TestTrue(Fps + TEXT("Braking with input stops at run speed"), FMath::IsNearlyEqual(F.Speed(), P.RunSpeed, 1.f));

		if (!TestTrue(Fps + TEXT("Second sprint lease starts"), F.BeginSprint())) return false;
		F.MoveUntil(Forward, SprintRequest, [&F, &P] { return F.Speed() >= P.SprintSpeed - .01f; });
		TestMotion(*this, Fps + TEXT("Stop from sprint"),
			F.MoveUntil(FVector::ZeroVector, SprintRequest, [&F] { return F.Speed() < 1.f; }), P.SprintSpeed / P.BrakingDeceleration, Step,
			FMath::Square(P.SprintSpeed) / (2.f * P.BrakingDeceleration), P.SprintSpeed);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgMovementProfileTurnAndJumpTest,
	"SurvivalRpg.Movement.CMC.Profile.TurnRatesAndJumpFollowProfile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRpgMovementProfileTurnAndJumpTest::RunTest(const FString& Parameters)
{
	using namespace RpgMovementProfileTests;
	FFixture F(FrameSteps[1]);
	if (!F.Initialize(*this)) return false;
	const float Step = F.StepSeconds;

	F.Move(FVector(0.0, 1.0, 0.0));
	TestTrue(TEXT("The shared profile turns free movement at once on the ground"), FMath::IsNearlyEqual(F.GetYaw(), 90.0, .5));

	// Finite rates prove that each gait reads its own profile value.
	F.Profile->GroundRotationRate = 90.f;
	F.Profile->SprintRotationRate = 45.f;
	double Before = F.GetYaw();
	F.Move(FVector(-1.0, 0.0, 0.0));
	TestTrue(TEXT("Free movement turns at the ground rate"), FMath::IsNearlyEqual(FMath::Abs(FMath::FindDeltaAngleDegrees(Before, F.GetYaw())), 90.0 * Step, .05));
	F.BeginSprint();
	Before = F.GetYaw();
	F.Move(FVector(0.0, -1.0, 0.0), SprintRequest);
	TestTrue(TEXT("Sprinting turns at the sprint rate"), F.Movement->IsSprinting()
		&& FMath::IsNearlyEqual(FMath::Abs(FMath::FindDeltaAngleDegrees(Before, F.GetYaw())), 45.0 * Step, .05));
	F.EndSprint();

	TestNotNull(TEXT("Hand equipment enters the combat stance"), F.Character->GetEquipmentManagerComponent()->EquipItemInSlot(
		URpgEquipmentAutomationTestSwordDefinition::StaticClass(), ERpgEquipmentSlot::MainHand));
	F.Controller->SetControlRotation(FRotator(0.0, 135.0, 0.0));
	F.Move(FVector::ZeroVector);
	TestTrue(TEXT("The combat stance turns at once despite a finite ground rate"), F.Movement->IsInCombatStance()
		&& FMath::IsNearlyEqual(FMath::FindDeltaAngleDegrees(F.GetYaw(), 135.0), 0.0, .5));
	F.Character->GetEquipmentManagerComponent()->UnequipItemInSlot(ERpgEquipmentSlot::MainHand);
	F.Profile->GroundRotationRate = 0.f;
	F.Profile->SprintRotationRate = 0.f;

	// Jump from rest: the midpoint fall integration reaches JumpZ² / 2g.
	F.MoveUntil(FVector::ZeroVector, 0, [&F] { return F.Speed() < 1.f; });
	const double StartZ = F.Character->GetActorLocation().Z;
	F.Move(FVector::ZeroVector, FSavedMove_Character::FLAG_JumpPressed);
	TestTrue(TEXT("Jump leaves the ground"), F.Movement->IsFalling());
	double Apex = StartZ;
	Before = F.GetYaw();
	F.Move(FVector(-1.0, 0.0, 0.0));
	const double AirTurn = FMath::Abs(FMath::FindDeltaAngleDegrees(Before, F.GetYaw()));
	TestTrue(FString::Printf(TEXT("Falling turns at the air rate (%.2f deg per step)"), AirTurn), FMath::IsNearlyEqual(AirTurn, F.Profile->AirRotationRate * Step, .05));
	for (int32 Index = 0; Index < 240 && F.Movement->IsFalling(); ++Index)
	{
		F.Move(FVector::ZeroVector);
		Apex = FMath::Max(Apex, F.Character->GetActorLocation().Z);
	}
	const double Predicted = FMath::Square(F.Profile->JumpZVelocity) / (2.0 * FMath::Abs(F.Movement->GetGravityZ()));
	TestTrue(FString::Printf(TEXT("Jump apex %.1f cm lies within 5 %% of %.1f cm"), Apex - StartZ, Predicted),
		FMath::Abs((Apex - StartZ) - Predicted) <= .05 * Predicted);
	TestTrue(TEXT("The jump lands again"), F.Movement->IsMovingOnGround());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgMovementProfileLoadTest,
	"SurvivalRpg.Movement.CMC.Profile.LoadTierLeavesMovementUnchanged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRpgMovementProfileLoadTest::RunTest(const FString& Parameters)
{
	using namespace RpgMovementProfileTests;
	// The load tier reaches the pawn only as this ASC tag; movement must ignore it in every value and every outcome,
	// whether the tier changes while idle or in the middle of a run.
	const TCHAR* Tiers[] = { nullptr, TEXT("Equipment.Load.Light"), TEXT("Equipment.Load.Medium"), TEXT("Equipment.Load.Heavy") };
	TArray<FString> Outcomes;
	TArray<FString> Labels;
	for (const TCHAR* Tier : Tiers)
	{
		for (const bool bWhileMoving : { false, true })
		{
			if (!Tier && bWhileMoving) continue;
			FFixture F(FrameSteps[1]);
			if (!F.Initialize(*this)) return false;
			const FGameplayTag TierTag = Tier ? FGameplayTag::RequestGameplayTag(Tier) : FGameplayTag();
			if (Tier && !bWhileMoving) { F.ASC->AddLooseGameplayTag(TierTag); }
			FString Outcome = FString::Printf(TEXT("%.3f %.3f %.3f %.3f %.3f "), F.Movement->GetMaxSpeed(), F.Movement->GetMaxAcceleration(),
				F.Movement->GetMaxBrakingDeceleration(), F.Movement->JumpZVelocity, F.Movement->GetSprintSpeed());
			const FVector Forward(1.0, 0.0, 0.0);
			for (int32 Index = 0; Index < 30; ++Index)
			{
				if (Tier && bWhileMoving && Index == 10) { F.ASC->AddLooseGameplayTag(TierTag); }
				F.Move(Forward);
			}
			F.BeginSprint();
			for (int32 Index = 0; Index < 30; ++Index) F.Move(Forward, SprintRequest);
			F.EndSprint();
			for (int32 Index = 0; Index < 20; ++Index) F.Move(Forward);
			for (int32 Index = 0; Index < 20; ++Index) F.Move(FVector(0.0, 1.0, 0.0));
			for (int32 Index = 0; Index < 40; ++Index) F.Move(FVector::ZeroVector, Index == 0 ? FSavedMove_Character::FLAG_JumpPressed : 0);
			Outcome += F.Character->GetActorLocation().ToString() + TEXT(" ") + F.Movement->Velocity.ToString();
			Outcomes.Add(Outcome);
			Labels.Add(FString::Printf(TEXT("%s %s"), Tier ? Tier : TEXT("None"), bWhileMoving ? TEXT("while moving") : TEXT("while idle")));
		}
	}
	for (int32 Index = 1; Index < Outcomes.Num(); ++Index)
	{
		TestEqual(FString::Printf(TEXT("%s moves exactly like no load tier"), *Labels[Index]), Outcomes[Index], Outcomes[0]);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgMovementProfileReplayTest,
	"SurvivalRpg.Movement.CMC.Profile.SavedMoveReplayReproducesTaper",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRpgMovementProfileReplayTest::RunTest(const FString& Parameters)
{
	using namespace RpgMovementProfileTests;
	FFixture F(FrameSteps[1]);
	if (!F.Initialize(*this)) return false;
	const FVector Forward(1.0, 0.0, 0.0);
	F.MoveUntil(Forward, 0, [&F] { return F.Speed() >= F.Profile->RunSpeed - .01f; });
	if (!TestTrue(TEXT("Sprint lease starts"), F.BeginSprint())) return false;

	// Record sprint build-up moves exactly as ReplicateMoveToServer would, then replay them from the same start.
	const FVector StartLocation = F.Character->GetActorLocation();
	const FVector StartVelocity = F.Movement->Velocity;
	TArray<FSavedMovePtr> Moves;
	for (int32 Index = 0; Index < 12; ++Index) { Moves.Add(F.RecordMove(Forward)); }
	const FVector RecordedLocation = F.Character->GetActorLocation();
	const FVector RecordedVelocity = F.Movement->Velocity;
	TestTrue(TEXT("The recorded moves ran inside the taper"), RecordedVelocity.Size2D() > F.Profile->RunSpeed + 50.f
		&& RecordedVelocity.Size2D() < F.Profile->SprintSpeed);

	// The live lease is gone during replay: only the saved authorization and the move's own velocity may decide.
	F.EndSprint();
	F.Character->SetActorLocation(StartLocation);
	F.Movement->Velocity = StartVelocity;
	for (const FSavedMovePtr& Saved : Moves) { F.ReplayMove(Saved); }
	TestTrue(TEXT("Replay reproduces the tapered velocity"), F.Movement->Velocity.Equals(RecordedVelocity, .01));
	TestTrue(TEXT("Replay reproduces the location"), F.Character->GetActorLocation().Equals(RecordedLocation, .01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgMovementGaitToggleTest,
	"SurvivalRpg.Movement.CMC.Gait.ToggleAndStickWalkAtFixedSpeedsAndMatchPrediction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRpgMovementGaitToggleTest::RunTest(const FString& Parameters)
{
	using namespace RpgMovementProfileTests;
	for (const float Step : FrameSteps)
	{
		FFixture F(Step);
		if (!F.Initialize(*this)) return false;
		const URpgMovementProfile& P = *F.Profile;
		const FString Fps = FString::Printf(TEXT("%.0f FPS: "), 1.f / Step);
		const FVector Forward(1.0, 0.0, 0.0);

		TestEqual(Fps + TEXT("A new pawn runs"), F.Movement->GetGait(), ERpgMovementGait::Run);
		F.Movement->ToggleWalk();
		TestTrue(Fps + TEXT("The toggle is owner input state"), F.Movement->IsWalkToggled());
		TestMotion(*this, Fps + TEXT("Toggle: start to 95 % walk speed"),
			F.ControlUntil(Forward, [&F, &P] { return F.Speed() >= .95f * P.WalkSpeed; }), .95f * P.WalkSpeed / P.Acceleration, Step);
		F.ControlUntil(Forward, [&F, &P] { return F.Speed() >= P.WalkSpeed - .01f; });
		TestTrue(Fps + TEXT("The toggle walks at the profile's walk speed"), F.Movement->GetGait() == ERpgMovementGait::Walk
			&& FMath::IsNearlyEqual(F.Movement->GetMaxSpeed(), P.WalkSpeed) && FMath::IsNearlyEqual(F.Speed(), P.WalkSpeed, .1f));
		TestMotion(*this, Fps + TEXT("Stop from walk"),
			F.ControlUntil(FVector::ZeroVector, [&F] { return F.Speed() < 1.f; }), P.WalkSpeed / P.BrakingDeceleration, Step,
			FMath::Square(P.WalkSpeed) / (2.f * P.BrakingDeceleration), P.WalkSpeed);
		TestEqual(Fps + TEXT("A stop keeps the walk gait"), F.Movement->GetGait(), ERpgMovementGait::Walk);

		F.ControlUntil(Forward, [&F, &P] { return F.Speed() >= P.WalkSpeed - .01f; });
		F.Movement->ToggleWalk();
		TestMotion(*this, Fps + TEXT("Toggle off: walk to 95 % run speed"),
			F.ControlUntil(Forward, [&F, &P] { return F.Speed() >= .95f * P.RunSpeed; }), (.95f * P.RunSpeed - P.WalkSpeed) / P.Acceleration, Step);
		F.ControlUntil(Forward, [&F, &P] { return F.Speed() >= P.RunSpeed - .01f; });
		TestEqual(Fps + TEXT("Without the toggle full input runs"), F.Movement->GetGait(), ERpgMovementGait::Run);
		F.Movement->ToggleWalk();
		TestMotion(*this, Fps + TEXT("Toggle on while running: run to walk speed with input held"),
			F.ControlUntil(Forward, [&F, &P] { return F.Speed() <= P.WalkSpeed + .01f; }),
			(P.RunSpeed - P.WalkSpeed) / P.BrakingDecelerationWithInput, Step);
		TestTrue(Fps + TEXT("Braking with input stops at walk speed"), FMath::IsNearlyEqual(F.Speed(), P.WalkSpeed, 1.f));
		F.Movement->ToggleWalk();
		F.ControlUntil(FVector::ZeroVector, [&F] { return F.Speed() < 1.f; });

		// Each gait moves at its own fixed speed: a weak stick walks with full acceleration, never a slow run.
		const FVector Weak = Forward * .4;
		TestMotion(*this, Fps + TEXT("Weak stick: start to 95 % walk speed at full acceleration"),
			F.ControlUntil(Weak, [&F, &P] { return F.Speed() >= .95f * P.WalkSpeed; }), .95f * P.WalkSpeed / P.Acceleration, Step);
		F.ControlUntil(Weak, [&F, &P] { return F.Speed() >= P.WalkSpeed - .01f; });
		TestTrue(Fps + TEXT("A weak stick walks at the full walk speed"), F.Movement->GetGait() == ERpgMovementGait::Walk
			&& FMath::IsNearlyEqual(F.Speed(), P.WalkSpeed, .1f));
		TestTrue(Fps + TEXT("The weak stick's acceleration is normalized for movement, server and trajectory"),
			FMath::IsNearlyEqual(F.Movement->GetCurrentAcceleration().Size2D(), F.Movement->GetMaxAcceleration(), .01)
			&& FMath::IsNearlyEqual(F.Movement->GetAnalogInputModifier(), 1.f));
		TestMotion(*this, Fps + TEXT("Full stick: walk to 95 % run speed"),
			F.ControlUntil(Forward, [&F, &P] { return F.Speed() >= .95f * P.RunSpeed; }), (.95f * P.RunSpeed - P.WalkSpeed) / P.Acceleration, Step);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgMovementGaitPriorityTest,
	"SurvivalRpg.Movement.CMC.Gait.StickHysteresisSprintCrouchAndBlockPriority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRpgMovementGaitPriorityTest::RunTest(const FString& Parameters)
{
	using namespace RpgMovementProfileTests;
	FFixture F(FrameSteps[1]);
	if (!F.Initialize(*this)) return false;
	const URpgMovementProfile& P = *F.Profile;
	const FVector Forward(1.0, 0.0, 0.0);
	const float Threshold = F.Movement->MinimumSprintInput;
	const float Band = F.Movement->WalkInputHysteresis;
	if (!TestTrue(TEXT("The authored threshold leaves room for the band"), Threshold > Band + .05f && Band > .01f)) return false;
	auto Gait = [&F] { return F.Movement->GetGait(); };

	F.Hold(Forward * .5, 10);
	TestEqual(TEXT("A weak stick walks"), Gait(), ERpgMovementGait::Walk);
	F.Hold(Forward * (Threshold - Band * .5f), 10);
	TestEqual(TEXT("Inside the band a walking stick keeps walking"), Gait(), ERpgMovementGait::Walk);
	F.Hold(Forward * (Threshold + .02f), 10);
	TestEqual(TEXT("From the threshold the stick runs"), Gait(), ERpgMovementGait::Run);
	F.Hold(Forward * (Threshold - Band * .5f), 10);
	TestEqual(TEXT("Inside the band a running stick keeps running"), Gait(), ERpgMovementGait::Run);
	F.Hold(Forward * (Threshold - Band - .02f), 10);
	TestEqual(TEXT("Below the band the stick walks again"), Gait(), ERpgMovementGait::Walk);
	F.Hold(FVector::ZeroVector, 30);
	TestEqual(TEXT("Released input keeps the walk choice"), Gait(), ERpgMovementGait::Walk);
	F.Hold(Forward, 10);
	F.Hold(FVector::ZeroVector, 30);
	TestEqual(TEXT("Released input keeps the run choice"), Gait(), ERpgMovementGait::Run);

	if (!TestTrue(TEXT("Sprint lease starts"), F.BeginSprint())) return false;
	F.Hold(Forward * .5, 40);
	TestTrue(TEXT("A weak stick keeps walking while sprint is held"), Gait() == ERpgMovementGait::Walk && !F.Movement->IsSprinting()
		&& FMath::IsNearlyEqual(F.Speed(), P.WalkSpeed, 1.f));
	F.Hold(Forward, 90);
	TestTrue(TEXT("Full input sprints"), Gait() == ERpgMovementGait::Sprint && F.Movement->IsSprinting());
	F.Movement->ToggleWalk();
	F.Hold(Forward, 5);
	TestTrue(TEXT("Holding sprint overrides the walk toggle"), Gait() == ERpgMovementGait::Sprint && F.Speed() > P.RunSpeed);
	F.EndSprint();
	TestEqual(TEXT("Ending the lease reports the toggled walk without another move"), Gait(), ERpgMovementGait::Walk);
	F.Hold(Forward, 90);
	TestTrue(TEXT("Releasing sprint returns to the toggled walk"), Gait() == ERpgMovementGait::Walk
		&& FMath::IsNearlyEqual(F.Speed(), P.WalkSpeed, 1.f));

	F.Movement->bWantsToCrouch = true;
	F.Hold(Forward, 60);
	TestTrue(TEXT("Crouch keeps its own speed and ignores the walk toggle"), F.Movement->IsCrouching()
		&& FMath::IsNearlyEqual(F.Movement->GetMaxSpeed(), P.CrouchSpeed) && FMath::IsNearlyEqual(F.Speed(), P.CrouchSpeed, 1.f));
	F.Movement->bWantsToCrouch = false;
	F.Hold(Forward, 60);
	TestTrue(TEXT("Standing up walks again"), !F.Movement->IsCrouching() && Gait() == ERpgMovementGait::Walk
		&& FMath::IsNearlyEqual(F.Speed(), P.WalkSpeed, 1.f));

	constexpr float BlockCap = 120.f;
	const uint32 BlockLease = F.ASC->BeginBlockMovement(F.Ability, BlockCap);
	F.Hold(Forward, 60);
	TestTrue(TEXT("A block cap below the walk speed still applies"), FMath::IsNearlyEqual(F.Speed(), BlockCap, 1.f));
	F.ASC->EndBlockMovement(F.Ability, BlockLease);
	F.Movement->ToggleWalk();
	F.Hold(Forward, 90);
	TestTrue(TEXT("Turning the toggle off runs again"), Gait() == ERpgMovementGait::Run && FMath::IsNearlyEqual(F.Speed(), P.RunSpeed, 1.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgMovementGaitSavedMoveTest,
	"SurvivalRpg.Movement.CMC.Gait.SavedMovesCarryWalkToggleAndStickChoice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRpgMovementGaitSavedMoveTest::RunTest(const FString& Parameters)
{
	using namespace RpgMovementProfileTests;
	FFixture F(FrameSteps[1]);
	if (!F.Initialize(*this)) return false;
	const URpgMovementProfile& P = *F.Profile;
	const FVector Forward(1.0, 0.0, 0.0);
	F.Movement->ToggleWalk();
	F.ControlUntil(Forward, [&F, &P] { return F.Speed() >= P.WalkSpeed - .01f; });
	F.SetStickWalk(true);

	const FVector StartLocation = F.Character->GetActorLocation();
	const FVector StartVelocity = F.Movement->Velocity;
	TArray<FSavedMovePtr> Moves;
	for (int32 Index = 0; Index < 12; ++Index) { Moves.Add(F.RecordMove(Forward)); }
	const FVector RecordedLocation = F.Character->GetActorLocation();
	const uint8 Flags = Moves[0]->GetCompressedFlags();
	TestTrue(TEXT("The saved move sends the walk toggle"), (Flags & WalkToggle) != 0);
	TestTrue(TEXT("The saved move sends the stick's walk choice"), (Flags & StickWalk) != 0);
	TestFalse(TEXT("Walking sends no sprint request"), (Flags & SprintRequest) != 0);

	// Today's input runs: only the recorded wishes may decide the replayed speed.
	F.Movement->ToggleWalk();
	F.SetStickWalk(false);
	FNetworkPredictionData_Client_Character* Prediction = static_cast<FNetworkPredictionData_Client_Character*>(F.Movement->GetPredictionData_Client());
	const FSavedMovePtr Running = Prediction->AllocateNewMove();
	Running->Clear();
	Running->SetMoveFor(F.Character, F.StepSeconds, Forward * F.Movement->GetMaxAcceleration(), *Prediction);
	TestFalse(TEXT("A running move never combines with a walking one"), Moves.Last()->CanCombineWith(Running, F.Character, 1.f));
	TestEqual(TEXT("A running move sends neither walk flag"), static_cast<uint8>(Running->GetCompressedFlags() & (WalkToggle | StickWalk)), static_cast<uint8>(0));

	F.Character->SetActorLocation(StartLocation);
	F.Movement->Velocity = StartVelocity;
	for (const FSavedMovePtr& Saved : Moves) { F.ReplayMove(Saved); }
	TestTrue(TEXT("Replay walks as recorded"), F.Character->GetActorLocation().Equals(RecordedLocation, .01)
		&& FMath::IsNearlyEqual(F.Speed(), P.WalkSpeed, .1f));
	TestEqual(TEXT("After replay, speed queries follow today's input"), F.Movement->GetMaxSpeed(), P.RunSpeed);
	Moves[0]->Clear();
	TestEqual(TEXT("Reused saved-move storage clears both walk flags"), static_cast<uint8>(Moves[0]->GetCompressedFlags() & (WalkToggle | StickWalk)), static_cast<uint8>(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgMovementGaitServerTest,
	"SurvivalRpg.Movement.CMC.Gait.ServerTakesWalkWishesFromReceivedMoves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRpgMovementGaitServerTest::RunTest(const FString& Parameters)
{
	using namespace RpgMovementProfileTests;
	FFixture F(FrameSteps[1]);
	if (!F.Initialize(*this)) return false;
	const URpgMovementProfile& P = *F.Profile;
	const FVector Forward(1.0, 0.0, 0.0);
	// A remote autonomous pawn on the server: its moves arrive through MoveAutonomous with the client's flags. A standalone
	// world treats every controller as local, so the pawn runs controller-less physics instead.
	F.Controller->UnPossess();
	F.Movement->bRunPhysicsWithNoController = true;
	F.Character->SetAutonomousProxy(true);
	if (!TestTrue(TEXT("The pawn is a remote autonomous proxy on the authority"), F.Character->HasAuthority()
		&& !F.Character->IsLocallyControlled() && F.Character->GetRemoteRole() == ROLE_AutonomousProxy)) return false;
	F.Movement->ToggleWalk(); // Server-side input state must never apply to a remote pawn.

	F.MoveUntil(Forward, 0, [&F, &P] { return F.Speed() >= P.RunSpeed - .01f; });
	TestEqual(TEXT("Moves without walk flags run"), F.Movement->GetGait(), ERpgMovementGait::Run);
	F.MoveUntil(Forward, WalkToggle, [&F, &P] { return F.Speed() <= P.WalkSpeed + .01f; });
	TestTrue(TEXT("The received walk toggle walks"), F.Movement->GetGait() == ERpgMovementGait::Walk
		&& FMath::IsNearlyEqual(F.Movement->GetMaxSpeed(), P.WalkSpeed));
	F.MoveUntil(Forward, 0, [&F, &P] { return F.Speed() >= P.RunSpeed - .01f; });
	F.MoveUntil(Forward, StickWalk, [&F, &P] { return F.Speed() <= P.WalkSpeed + .01f; });
	TestEqual(TEXT("The received stick choice walks"), F.Movement->GetGait(), ERpgMovementGait::Walk);

	if (!TestTrue(TEXT("Sprint lease starts"), F.BeginSprint())) return false;
	for (int32 Index = 0; Index < 30; ++Index) F.Move(Forward, SprintRequest | StickWalk);
	TestTrue(TEXT("A weak stick cannot sprint on the server"), !F.Movement->IsSprinting() && F.Speed() <= P.WalkSpeed + .01f);
	for (int32 Index = 0; Index < 60; ++Index) F.Move(Forward, SprintRequest | WalkToggle);
	TestTrue(TEXT("The sprint request overrides a received walk toggle"), F.Movement->IsSprinting() && F.Speed() > P.RunSpeed);
	F.EndSprint();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgMovementGaitProxyTest,
	"SurvivalRpg.Movement.CMC.Gait.SimulatedProxySpeedFollowsReplicatedGait",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRpgMovementGaitProxyTest::RunTest(const FString& Parameters)
{
	using namespace RpgMovementProfileTests;
	FFixture F(FrameSteps[1]);
	if (!F.Initialize(*this)) return false;
	const URpgMovementProfile& P = *F.Profile;
	// The Pose Search trajectory predicts with GetMaxSpeed on proxies too; they only know the replicated gait.
	F.Character->SetRole(ROLE_SimulatedProxy);
	const TPair<ERpgMovementGait, float> Cases[] = {
		{ ERpgMovementGait::Walk, P.WalkSpeed }, { ERpgMovementGait::Run, P.RunSpeed }, { ERpgMovementGait::Sprint, P.SprintSpeed } };
	for (const TPair<ERpgMovementGait, float>& Case : Cases)
	{
		F.SetReplicatedGait(Case.Key);
		const FString Name = StaticEnum<ERpgMovementGait>()->GetNameStringByValue(static_cast<int64>(Case.Key));
		TestEqual(Name + TEXT(": the proxy reports the replicated gait"), F.Movement->GetGait(), Case.Key);
		TestEqual(Name + TEXT(": the proxy predicts with that gait's speed"), F.Movement->GetMaxSpeed(), Case.Value);
		TestEqual(Name + TEXT(": the sprint read model agrees"), F.Movement->IsSprinting(), Case.Key == ERpgMovementGait::Sprint);
	}
	F.Movement->ToggleWalk();
	F.SetReplicatedGait(ERpgMovementGait::Run);
	TestEqual(TEXT("A proxy never applies local walk input"), F.Movement->GetMaxSpeed(), P.RunSpeed);
	F.SetReplicatedGait(ERpgMovementGait::Run);
	F.Character->SetRole(ROLE_Authority);
	return true;
}

#endif
