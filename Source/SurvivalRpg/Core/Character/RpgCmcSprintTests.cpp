// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "RpgCharacterMovementComponent.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "Misc/AutomationTest.h"
#include "RpgCharacter.h"
#include "RpgPawnData.h"
#include "RpgPawnExtensionComponent.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgStaminaSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Tasks/RpgAbilityTask_Sprint.h"
#include "SurvivalRpg/AbilitySystem/Tasks/RpgAbilityTask_StaminaRegen.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Interaction/RpgInteractionGrantAutomationTestTypes.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"

namespace RpgCmcSprintTests
{
	// These are deliberately fixture values, not assertions about a particular designer speed profile.
	constexpr float RunSpeed = 310.f;
	constexpr float SprintSpeed = 530.f;
	constexpr float StepSeconds = .05f;
	constexpr uint8 SprintRequest = FSavedMove_Character::FLAG_Custom_0;
	constexpr const TCHAR* DeltaEffectPath = TEXT("/Game/SurvivalRpg/Characters/GASP/CMC/RPG/Sprint/GE_RpgGasp_StaminaDelta.GE_RpgGasp_StaminaDelta_C");

	/** Real GAS activation and CMC floor physics, without an Experience, animation graph or network receipt claim. */
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
			// Enhanced Input's world processor visits all subsystem objects, including deinitialized ones
			// until collection. Finish the owned world's teardown before another latent fixture ticks Slate.
			// No subsystem settings or diagnostics are disabled; live editor/PIE worlds remain referenced.
			GameInstance.Reset();
			CollectGarbage(RF_NoFlags, true);
		}

		bool Initialize(FAutomationTestBase& Test)
		{
			if (!Test.TestNotNull(TEXT("Standalone sprint world exists"), World)) return false;
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
			if (!Test.TestNotNull(TEXT("Sprint avatar exists"), Character)
				|| !Test.TestNotNull(TEXT("Sprint controller exists"), Controller)) return false;
			Controller->SetAsLocalPlayerController();
			Controller->Possess(Character);
			Character->SetActorLocation(FVector(0.f, 0.f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f));
			Character->SetActorTickEnabled(false);
			Controller->SetActorTickEnabled(false);
			Movement = CastChecked<URpgCharacterMovementComponent>(Character->GetCharacterMovement());
			Movement->SetComponentTickEnabled(false); // Only explicit real MoveAutonomous calls advance movement.
			Movement->MaxWalkSpeed = RunSpeed;
			Movement->SprintSpeed = SprintSpeed;
			Movement->bEnableGASSprint = true;
			Movement->SetMovementMode(MOVE_Walking);
			ASC = NewObject<URpgAbilitySystemComponent>(Character, NAME_None, RF_Transient);
			Character->AddInstanceComponent(ASC);
			ASC->RegisterComponent();
			ASC->AddAttributeSetSubobject(NewObject<URpgHealthSet>(Character, NAME_None, RF_Transient));
			Stamina = NewObject<URpgStaminaSet>(Character, NAME_None, RF_Transient);
			ASC->AddAttributeSetSubobject(Stamina);
			Extension = URpgPawnExtensionComponent::FindPawnExtensionComponent(Character);
			Extension->SetPawnData(NewObject<URpgPawnData>(Character, NAME_None, RF_Transient));
			Extension->InitializeAbilitySystemComponent(ASC, Character);
			Handle = Grant();
			if (!Test.TestTrue(TEXT("Inert GAS owner really activates"), ASC->TryActivateAbility(Handle))) return false;
			Ability = ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance();
			Move(false, 0);
			return Test.TestTrue(TEXT("CMC finds the real support surface"), Movement->IsMovingOnGround() && Movement->CurrentFloor.IsWalkableFloor());
		}

		FGameplayAbilitySpecHandle Grant()
		{
			return ASC->GiveAbility(FGameplayAbilitySpec(URpgInteractionGrantAutomationGrantedAbility::StaticClass(), 1));
		}
		void Move(bool bInput = true, uint8 Flags = SprintRequest, float Seconds = StepSeconds, float InputStrength = 1.f)
		{
			TimeStamp += Seconds;
			Movement->MoveAutonomous(TimeStamp, Seconds, Flags,
				bInput ? FVector(Movement->GetMaxAcceleration() * InputStrength, 0.f, 0.f) : FVector::ZeroVector);
		}
		void MoveSteps(int32 Count, bool bInput = true, uint8 Flags = SprintRequest)
		{
			for (int32 Index = 0; Index < Count; ++Index) Move(bInput, Flags);
		}
		bool LoadEffect(FAutomationTestBase& Test)
		{
			DeltaEffect = LoadClass<UGameplayEffect>(nullptr, DeltaEffectPath);
			return Test.TestNotNull(TEXT("Designer-owned Instant Stamina effect is available"), DeltaEffect.Get());
		}
		bool ApplyDelta(float Value)
		{
			FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(DeltaEffect, 1.f, ASC->MakeEffectContext());
			if (!Spec.IsValid()) return false;
			Spec.Data->SetSetByCallerMagnitude(RpgGameplayTags::Data_StaminaDelta, Value);
			return ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()).WasSuccessfullyApplied();
		}

		TStrongObjectPtr<UGameInstance> GameInstance;
		UWorld* World = nullptr;
		ARpgCharacter* Character = nullptr;
		APlayerController* Controller = nullptr;
		URpgCharacterMovementComponent* Movement = nullptr;
		URpgAbilitySystemComponent* ASC = nullptr;
		URpgPawnExtensionComponent* Extension = nullptr;
		URpgStaminaSet* Stamina = nullptr;
		UGameplayAbility* Ability = nullptr;
		FGameplayAbilitySpecHandle Handle;
		TSubclassOf<UGameplayEffect> DeltaEffect;
		float TimeStamp = 0.f;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgSprintLeaseLifecycleTest,
	"SurvivalRpg.Movement.CMC.Sprint.LeaseSnapshotsSpeedAndRejectsRetiredOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRpgSprintLeaseLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace RpgCmcSprintTests;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	TestEqual(TEXT("Invalid speed never authorizes movement"), F.ASC->BeginSprintMovement(F.Ability, -1.f), 0u);
	const uint32 First = F.ASC->BeginSprintMovement(F.Ability, SprintSpeed);
	if (!TestTrue(TEXT("Active ability receives a lease"), First != 0)) return false;
	F.Movement->SprintSpeed += 77.f;
	TestEqual(TEXT("Repeated acquire is idempotent"), F.ASC->BeginSprintMovement(F.Ability, F.Movement->SprintSpeed), First);
	TestEqual(TEXT("A running activation retains its original speed"), F.ASC->GetSprintMovementSpeed(), SprintSpeed);
	const FGameplayAbilitySpecHandle OtherHandle = F.Grant();
	TestTrue(TEXT("Second genuine GAS activation exists"), F.ASC->TryActivateAbility(OtherHandle));
	UGameplayAbility* Other = F.ASC->FindAbilitySpecFromHandle(OtherHandle)->GetPrimaryInstance();
	TestEqual(TEXT("Another activation cannot steal the lease"), F.ASC->BeginSprintMovement(Other, 900.f), 0u);
	F.ASC->EndSprintMovement(Other, First);
	TestTrue(TEXT("Wrong owner cannot release it"), F.ASC->IsSprintMovementActive());
	F.ASC->CancelAbilityHandle(F.Handle);
	TestFalse(TEXT("GAS cancellation retires the lease synchronously"), F.ASC->IsSprintMovementActive());
	TestEqual(TEXT("Cancelled speed is cleared"), F.ASC->GetSprintMovementSpeed(), 0.f);
	TestTrue(TEXT("The same spec can start a new activation"), F.ASC->TryActivateAbility(F.Handle));
	F.Ability = F.ASC->FindAbilitySpecFromHandle(F.Handle)->GetPrimaryInstance();
	const uint32 Second = F.ASC->BeginSprintMovement(F.Ability, F.Movement->SprintSpeed);
	TestTrue(TEXT("New activation gets a distinct nonzero token"), Second && Second != First);
	F.ASC->EndSprintMovement(F.Ability, First);
	TestTrue(TEXT("Delayed old cleanup cannot release the new activation"), F.ASC->IsSprintMovementActive());
	F.Extension->UninitializeAbilitySystem();
	TestFalse(TEXT("Avatar teardown retires sprint authorization"), F.ASC->IsSprintMovementActive());
	TestEqual(TEXT("Avatar teardown cannot expose old speed"), F.ASC->GetSprintMovementSpeed(), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgSprintAuthorityGaitTest,
	"SurvivalRpg.Movement.CMC.Sprint.CompressedDesireNeedsAuthorityLeaseAndYieldsToBlock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRpgSprintAuthorityGaitTest::RunTest(const FString& Parameters)
{
	using namespace RpgCmcSprintTests;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	// Exercise the received movement entry point, not a test-only setter for the effective gait.
	F.MoveSteps(16);
	TestFalse(TEXT("A client request bit alone cannot authorize authority sprint"), F.Movement->IsSprinting());
	TestTrue(TEXT("Unauthenticated desire remains at the ordinary speed"), FMath::IsNearlyEqual(F.Movement->Velocity.Size2D(), RunSpeed, .1f));
	const uint32 SprintLease = F.ASC->BeginSprintMovement(F.Ability, SprintSpeed);
	F.MoveSteps(16);
	TestTrue(TEXT("A valid authority activation enables effective sprint"), F.Movement->IsSprinting());
	TestTrue(TEXT("Actual CMC motion reaches the captured sprint speed"), FMath::IsNearlyEqual(F.Movement->Velocity.Size2D(), SprintSpeed, .1f));
	const float PartialInput = F.Movement->MinimumSprintInput * .5f;
	if (!TestTrue(TEXT("The analog fixture exercises a nonzero value below the authored sprint threshold"),
		PartialInput > 0.f && PartialInput < 1.f)) return false;
	for (int32 Index = 0; Index < 16; ++Index) F.Move(true, SprintRequest, StepSeconds, PartialInput);
	TestTrue(TEXT("Partial analog movement preserves the held GAS authorization"), F.ASC->IsSprintMovementActive());
	TestFalse(TEXT("Input below the sprint threshold uses the ordinary gait"), F.Movement->IsSprinting());
	TestEqual(TEXT("Partial analog movement selects the normal running speed contract"), F.Movement->GetMaxSpeed(), RunSpeed);
	TestTrue(TEXT("Partial analog input still produces actual ordinary movement"),
		F.Movement->Velocity.Size2D() > 0.f && F.Movement->Velocity.Size2D() <= RunSpeed + .1f);
	F.MoveSteps(16);
	TestTrue(TEXT("Full analog input resumes the same held sprint activation"), F.Movement->IsSprinting());
	TestTrue(TEXT("Restored full input reaches the activation's sprint speed"), FMath::IsNearlyEqual(F.Movement->Velocity.Size2D(), SprintSpeed, .1f));
	const uint32 BlockLease = F.ASC->BeginBlockMovement(F.Ability, RunSpeed * .5f);
	F.MoveSteps(20);
	TestFalse(TEXT("Block suppresses effective sprint without ending its hold"), F.Movement->IsSprinting());
	TestTrue(TEXT("Held sprint authorization survives block"), F.ASC->IsSprintMovementActive());
	TestTrue(TEXT("Movement obeys the independent block cap"), FMath::IsNearlyEqual(F.Movement->Velocity.Size2D(), RunSpeed * .5f, .1f));
	F.ASC->EndBlockMovement(F.Ability, BlockLease);
	F.MoveSteps(16);
	TestTrue(TEXT("Release of block resumes the still-held sprint"), F.Movement->IsSprinting());
	// MoveAutonomous decodes the actual wire flags before physics; a direct bWantsToCrouch write
	// would be overwritten by UpdateFromCompressedFlags and would never exercise crouching.
	F.Move(true, SprintRequest | FSavedMove_Character::FLAG_WantsToCrouch);
	TestTrue(TEXT("The decoded crouch request actually changes the capsule stance"), F.Movement->IsCrouching());
	TestFalse(TEXT("Crouching yields effective sprint"), F.Movement->IsSprinting());
	F.ASC->EndSprintMovement(F.Ability, SprintLease);
	TestFalse(TEXT("Ending the lease clears final gait without waiting for another tick"), F.Movement->IsSprinting());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgSprintSavedMoveTest,
	"SurvivalRpg.Movement.CMC.Sprint.RepeatedSavedMoveReplayKeepsHistoricalAuthorizationAndSpeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRpgSprintSavedMoveTest::RunTest(const FString& Parameters)
{
	using namespace RpgCmcSprintTests;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	const uint32 Lease = F.ASC->BeginSprintMovement(F.Ability, SprintSpeed);
	F.MoveSteps(16);
	FNetworkPredictionData_Client_Character* Prediction = static_cast<FNetworkPredictionData_Client_Character*>(F.Movement->GetPredictionData_Client());
	const FSavedMovePtr Move = Prediction->AllocateNewMove();
	Move->Clear();
	const FVector Accel(F.Movement->GetMaxAcceleration(), 0.f, 0.f);
	Move->SetMoveFor(F.Character, StepSeconds, Accel, *Prediction);
	F.Movement->MoveAutonomous(1.f, Move->DeltaTime, Move->GetCompressedFlags(), Accel);
	Move->PostUpdate(F.Character, FSavedMove_Character::PostUpdate_Record);
	TestTrue(TEXT("Saved movement transmits only sprint desire"), (Move->GetCompressedFlags() & SprintRequest) != 0);
	F.ASC->EndSprintMovement(F.Ability, Lease);
	F.Movement->SprintSpeed = SprintSpeed + 123.f;
	const FSavedMovePtr Released = Prediction->AllocateNewMove();
	Released->Clear();
	Released->SetMoveFor(F.Character, StepSeconds, Accel, *Prediction);
	TestFalse(TEXT("Released movement cannot combine across authorization boundary"), Move->CanCombineWith(Released, F.Character, 1.f));
	for (int32 Replay = 0; Replay < 2; ++Replay)
	{
		F.Movement->Velocity = FVector(SprintSpeed, 0.f, 0.f);
		F.Character->bClientUpdating = true;
		F.Movement->SetCurrentReplayedSavedMove(Move.Get());
		Move->PrepMoveFor(F.Character);
		const FVector Before = F.Character->GetActorLocation();
		F.Movement->MoveAutonomous(1.f, Move->DeltaTime, Move->GetCompressedFlags(), Accel);
		Move->PostUpdate(F.Character, FSavedMove_Character::PostUpdate_Replay);
		F.Movement->SetCurrentReplayedSavedMove(nullptr);
		F.Character->bClientUpdating = false;
		TestTrue(TEXT("Real replay movement uses the captured speed, not released GAS or new tuning"),
			FMath::IsNearlyEqual(FVector::Dist2D(Before, F.Character->GetActorLocation()), SprintSpeed * Move->DeltaTime, .05f));
		TestFalse(TEXT("Historical replay never leaks as the current local sprint gait"), F.Movement->IsSprinting());
		TestEqual(TEXT("Current speed queries use the released lease"), F.Movement->GetMaxSpeed(), RunSpeed);
	}
	Move->Clear();
	TestEqual(TEXT("Reused saved-move storage clears sprint desire"), static_cast<uint8>(Move->GetCompressedFlags() & SprintRequest), static_cast<uint8>(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgSprintStaminaTaskTest,
	"SurvivalRpg.Movement.CMC.Sprint.AuthorityTaskDrainsOnlyEffectiveDisplacementAndCleansUp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRpgSprintStaminaTaskTest::RunTest(const FString& Parameters)
{
	using namespace RpgCmcSprintTests;
	FFixture F;
	if (!F.Initialize(*this) || !F.LoadEffect(*this)) return false;
	constexpr float Drain = 8.f;
	TStrongObjectPtr<URpgAbilityTask_Sprint> Task(URpgAbilityTask_Sprint::StartSprint(F.Ability, F.DeltaEffect, RpgGameplayTags::Data_StaminaDelta, Drain, 3.f));
	Task->ReadyForActivation();
	if (!TestTrue(TEXT("The real task acquired authorization"), F.ASC->IsSprintMovementActive())) return false;
	const float Before = F.Stamina->GetStamina();
	F.MoveSteps(4, false);
	TestEqual(TEXT("A held but idle activation costs no stamina"), F.Stamina->GetStamina(), Before);
	F.MoveSteps(12);
	TestTrue(TEXT("Authority pays the configured rate through the actual Instant effect"),
		FMath::IsNearlyEqual(F.Stamina->GetStamina(), Before - Drain * StepSeconds * 12.f, .002f));
	const uint32 BlockLease = F.ASC->BeginBlockMovement(F.Ability, RunSpeed * .5f);
	const float AfterSprint = F.Stamina->GetStamina();
	F.MoveSteps(8);
	TestEqual(TEXT("Moving block suspends sprint cost"), F.Stamina->GetStamina(), AfterSprint);
	F.ASC->EndBlockMovement(F.Ability, BlockLease);
	F.MoveSteps(2);
	TestTrue(TEXT("Held sprint resumes consumption after block"), F.Stamina->GetStamina() < AfterSprint);
	Task->EndTask();
	const float AfterEnd = F.Stamina->GetStamina();
	F.MoveSteps(8);
	TestFalse(TEXT("Task cleanup releases authorization"), F.ASC->IsSprintMovementActive());
	TestEqual(TEXT("Old movement callbacks cannot spend after task end"), F.Stamina->GetStamina(), AfterEnd);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgSprintExhaustionTaskTest,
	"SurvivalRpg.Movement.CMC.Sprint.ExhaustionStopsTaskAndRejectsInsufficientRestart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRpgSprintExhaustionTaskTest::RunTest(const FString& Parameters)
{
	using namespace RpgCmcSprintTests;
	FFixture F;
	if (!F.Initialize(*this) || !F.LoadEffect(*this)) return false;
	F.ASC->SetNumericAttributeBase(URpgStaminaSet::GetStaminaAttribute(), .2f);
	TStrongObjectPtr<URpgAbilityTask_Sprint> Task(URpgAbilityTask_Sprint::StartSprint(F.Ability, F.DeltaEffect, RpgGameplayTags::Data_StaminaDelta, 8.f, .1f));
	Task->ReadyForActivation();
	F.Move();
	TestEqual(TEXT("The final expense clamps at zero"), F.Stamina->GetStamina(), 0.f);
	TestFalse(TEXT("Exhaustion synchronously retires movement authority"), F.ASC->IsSprintMovementActive());
	TestTrue(TEXT("Exhausted task has completed"), Task->IsFinished());
	TestTrue(TEXT("An unrelated resource grant succeeds"), F.ApplyDelta(1.f));
	F.MoveSteps(4);
	TestEqual(TEXT("Old task never restarts because stamina became positive"), F.Stamina->GetStamina(), 1.f);
	TStrongObjectPtr<URpgAbilityTask_Sprint> Insufficient(URpgAbilityTask_Sprint::StartSprint(F.Ability, F.DeltaEffect, RpgGameplayTags::Data_StaminaDelta, 8.f, 3.f));
	Insufficient->ReadyForActivation();
	TestTrue(TEXT("Start threshold rejects the task before a lease exists"), Insufficient->IsFinished());
	TestFalse(TEXT("Insufficient stamina cannot leave movement authority active"), F.ASC->IsSprintMovementActive());
	return true;
}

namespace RpgCmcSprintTests
{
	/** Advances an isolated real world once per engine frame; never rewrites the global frame counter or task clock. */
	class FRegenCommand final : public IAutomationLatentCommand
	{
	public:
		FRegenCommand(FAutomationTestBase& InTest, TSharedPtr<FFixture> InFixture)
			: Test(InTest), Fixture(MoveTemp(InFixture)) {}
		virtual bool Update() override
		{
			if (LastFrame == GFrameCounter) return false;
			LastFrame = GFrameCounter;
			FFixture& F = *Fixture;
			if (!Task.IsValid())
			{
				F.ASC->SetNumericAttributeBase(URpgStaminaSet::GetStaminaAttribute(), 50.f);
				F.ASC->SetNumericAttributeBase(URpgStaminaSet::GetStaminaRegenAttribute(), 10.f);
				Task.Reset(URpgAbilityTask_StaminaRegen::RegenerateStamina(F.Ability, F.DeltaEffect, RpgGameplayTags::Data_StaminaDelta, .3f));
				Task->ReadyForActivation();
			}
			F.World->Tick(LEVELTICK_All, StepSeconds);
			++Frames;
			if (Frames == 4)
			{
				Test.TestEqual(TEXT("Regeneration does not bypass its initial delay"), F.Stamina->GetStamina(), 50.f);
				Test.TestTrue(TEXT("A different ability's actual stamina effect applies"), F.ApplyDelta(-7.f));
			}
			if (Frames == 8) Test.TestEqual(TEXT("Every negative stamina effect restarts the full delay"), F.Stamina->GetStamina(), 43.f);
			if (Frames == 20)
			{
				// Last timer sample can lag at most one 100ms period; there is no credit for either delay.
				Test.TestTrue(TEXT("Actual delayed recovery uses StaminaRegen and elapsed eligible world time"),
					F.Stamina->GetStamina() >= 47.f - .02f && F.Stamina->GetStamina() <= 48.f + .02f);
				F.ASC->SetNumericAttributeBase(URpgStaminaSet::GetStaminaAttribute(), 99.8f);
			}
			if (Frames == 26)
			{
				Test.TestEqual(TEXT("Regeneration clamps at MaxStamina"), F.Stamina->GetStamina(), F.Stamina->GetMaxStamina());
				F.ASC->AddLooseGameplayTag(RpgGameplayTags::State_Dead);
				Test.TestTrue(TEXT("External damage-like resource loss remains a real effect"), F.ApplyDelta(-10.f));
			}
			if (Frames == 38)
			{
				Test.TestEqual(TEXT("Dead avatars do not regenerate"), F.Stamina->GetStamina(), 90.f);
				F.ASC->RemoveLooseGameplayTag(RpgGameplayTags::State_Dead);
				F.ASC->CancelAbilityHandle(F.Handle);
				Test.TestTrue(TEXT("Owning ability cancellation destroys its task"), Task->IsFinished());
			}
			if (Frames == 48)
			{
				Test.TestEqual(TEXT("Cancelled task leaves no timer that can write stamina"), F.Stamina->GetStamina(), 90.f);
				return true;
			}
			return false;
		}
	private:
		FAutomationTestBase& Test;
		TSharedPtr<FFixture> Fixture;
		TStrongObjectPtr<URpgAbilityTask_StaminaRegen> Task;
		uint64 LastFrame = MAX_uint64;
		int32 Frames = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgSprintRegenTaskTest,
	"SurvivalRpg.Movement.CMC.Sprint.RegenDelayRestartsForOtherCostsAndStopsWithOwner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRpgSprintRegenTaskTest::RunTest(const FString& Parameters)
{
	TSharedPtr<RpgCmcSprintTests::FFixture> Fixture = MakeShared<RpgCmcSprintTests::FFixture>();
	if (!Fixture->Initialize(*this) || !Fixture->LoadEffect(*this)) return false;
	ADD_LATENT_AUTOMATION_COMMAND(RpgCmcSprintTests::FRegenCommand(*this, Fixture));
	return true;
}

namespace RpgCmcSprintTests
{
	/** Exercises the real grant-batch deferral and effect ownership; never writes the task's private state. */
	class FRegenTuningCommand final : public IAutomationLatentCommand
	{
	public:
		FRegenTuningCommand(FAutomationTestBase& InTest, int32 InExitCase)
			: Test(InTest), ExitCase(InExitCase) {}

		virtual bool Update() override
		{
			if (LastFrame == GFrameCounter) return false;
			LastFrame = GFrameCounter;
			if (!Fixture)
			{
				Fixture = MakeShared<FFixture>();
				FFixture& F = *Fixture;
				if (!F.Initialize(Test) || !F.LoadEffect(Test)) return true;
				TuningClass = LoadClass<UGameplayEffect>(nullptr,
					TEXT("/Game/SurvivalRpg/Characters/GASP/CMC/RPG/Sprint/GE_RpgGasp_StaminaDefaults.GE_RpgGasp_StaminaDefaults_C"));
				if (!Test.TestNotNull(TEXT("Designer recovery tuning exists"), TuningClass.Get())) return true;
				const UGameplayEffect* Tuning = TuningClass->GetDefaultObject<UGameplayEffect>();
				if (!Test.TestTrue(TEXT("Tuning supplies one finite positive static recovery rate"),
					Tuning->Modifiers.Num() == 1
					&& Tuning->Modifiers[0].ModifierMagnitude.GetStaticMagnitudeIfPossible(1.f, TuningRate)
					&& FMath::IsFinite(TuningRate) && TuningRate > 0.f)) return true;
				OriginalSet.Reset(F.Stamina);
				F.ASC->RemoveSpawnedAttribute(F.Stamina);
				Task.Reset(URpgAbilityTask_StaminaRegen::RegenerateStamina(F.Ability, F.DeltaEffect,
					RpgGameplayTags::Data_StaminaDelta, 0.f, TuningClass));
				Task->ReadyForActivation();
				Test.TestFalse(TEXT("An initial missing attribute set defers rather than ending the task"), Task->IsFinished());
				Test.TestTrue(TEXT("No tuning effect is installed before its attribute set exists"),
					F.ASC->GetActiveEffects(FGameplayEffectQuery{}).IsEmpty());
				F.ASC->AddAttributeSetSubobject(F.Stamina);
			}
			FFixture& F = *Fixture;
			F.World->Tick(LEVELTICK_All, StepSeconds);
			++Frames;
			if (Frames == 4)
			{
				const TArray<FActiveGameplayEffectHandle> Handles = F.ASC->GetActiveEffects(FGameplayEffectQuery{});
				if (!Test.TestTrue(TEXT("The deferred task owns exactly one real active tuning effect"),
					!Task->IsFinished() && Handles.Num() == 1)) return true;
				OwnedHandle = Handles[0];
				Test.TestTrue(TEXT("Late attribute registration receives the actual authored current rate"),
					FMath::IsNearlyEqual(F.Stamina->GetStaminaRegen(), TuningRate));
				ForeignHandle = F.ASC->ApplyGameplayEffectToSelf(TuningClass->GetDefaultObject<UGameplayEffect>(),
					1.f, F.ASC->MakeEffectContext());
				if (!Test.TestTrue(TEXT("Another owner can hold a distinct effect of the same class"),
					ForeignHandle.IsValid() && ForeignHandle != OwnedHandle
					&& F.ASC->GetActiveEffects(FGameplayEffectQuery{}).Num() == 2)) return true;
				Test.TestTrue(TEXT("Both independently owned effects contribute before cleanup"),
					FMath::IsNearlyEqual(F.Stamina->GetStaminaRegen(), 2.f * TuningRate));
				if (ExitCase == 0)
				{
					F.ASC->CancelAbilityHandle(F.Handle);
					Test.TestTrue(TEXT("Real owning ability cancellation ends the recovery task synchronously"), Task->IsFinished());
				}
				else
				{
					F.ASC->RemoveSpawnedAttribute(F.Stamina);
					if (ExitCase == 2)
					{
						ReplacementSet.Reset(NewObject<URpgStaminaSet>(F.Character, NAME_None, RF_Transient));
						ReplacementSet->InitStamina(41.f);
						F.ASC->AddAttributeSetSubobject(ReplacementSet.Get());
					}
				}
			}
			if (Frames == 8 || Frames == 12)
			{
				Test.TestTrue(TEXT("Owner end or the exact captured set's loss retires the task"), Task->IsFinished());
				Test.TestNull(TEXT("Only the task-owned tuning handle was removed"), F.ASC->GetActiveGameplayEffect(OwnedHandle));
				Test.TestNotNull(TEXT("The unrelated effect survives despite sharing the tuning class"), F.ASC->GetActiveGameplayEffect(ForeignHandle));
				Test.TestEqual(TEXT("Cleanup leaves exactly the unrelated active effect"), F.ASC->GetActiveEffects(FGameplayEffectQuery{}).Num(), 1);
				if (ExitCase == 0)
					Test.TestTrue(TEXT("Normal cleanup retains the unrelated effect's rate"), FMath::IsNearlyEqual(F.Stamina->GetStaminaRegen(), TuningRate));
				else if (ExitCase == 1)
					Test.TestNull(TEXT("Cleanup never recreates a removed resource set"), F.ASC->GetSet<URpgStaminaSet>());
				else
				{
					Test.TestTrue(TEXT("A replacement remains the canonical set"), F.ASC->GetSet<URpgStaminaSet>() == ReplacementSet.Get());
					Test.TestEqual(TEXT("The old task never regenerates the replacement set"), ReplacementSet->GetStamina(), 41.f);
				}
				Task->EndTask(); // A repeated old cleanup must not touch the unrelated handle.
				if (Frames == 12)
				{
					F.ASC->RemoveActiveGameplayEffect(ForeignHandle);
					return true;
				}
			}
			return false;
		}

	private:
		FAutomationTestBase& Test;
		int32 ExitCase;
		TSharedPtr<FFixture> Fixture;
		TStrongObjectPtr<URpgStaminaSet> OriginalSet;
		TStrongObjectPtr<URpgStaminaSet> ReplacementSet;
		TStrongObjectPtr<URpgAbilityTask_StaminaRegen> Task;
		TSubclassOf<UGameplayEffect> TuningClass;
		FActiveGameplayEffectHandle OwnedHandle;
		FActiveGameplayEffectHandle ForeignHandle;
		float TuningRate = 0.f;
		uint64 LastFrame = MAX_uint64;
		int32 Frames = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgSprintRegenTuningOwnershipTest,
	"SurvivalRpg.Movement.CMC.Sprint.RegenTuningWaitsForAttributesAndRemovesOnlyItsOwnEffect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRpgSprintRegenTuningOwnershipTest::RunTest(const FString& Parameters)
{
	for (int32 ExitCase = 0; ExitCase < 3; ++ExitCase)
	{
		ADD_LATENT_AUTOMATION_COMMAND(RpgCmcSprintTests::FRegenTuningCommand(*this, ExitCase));
	}
	return true;
}

#endif
