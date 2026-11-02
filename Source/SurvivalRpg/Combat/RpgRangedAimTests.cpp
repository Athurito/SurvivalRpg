#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/AbilitySystem/Effects/RpgAbilityTuningEffectComponent.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Tasks/RpgAbilityTask_RangedAim.h"
#include "SurvivalRpg/Camera/RpgCameraMode.h"
#include "SurvivalRpg/Camera/RpgCameraMode_ThirdPerson.h"
#include "SurvivalRpg/Combat/RpgRangedAimAutomationTestTypes.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMovementComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnGameplayComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeDefinition.h"
#include "UObject/StrongObjectPtr.h"

namespace RpgRangedAimTests
{
	/** A standalone RPG character with its own ability system and controller, as the block replay tests use. */
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
			if (ASC) ASC->ClearAllAbilities();
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
			if (!Test.TestNotNull(TEXT("Standalone aim world exists"), World)) return false;
			Character = World->SpawnActor<ARpgCharacter>();
			Controller = World->SpawnActor<APlayerController>();
			if (!Test.TestTrue(TEXT("Character and controller exist"), Character && Controller)) return false;
			// Without a net driver a player controller is local only with a local player, as on a real client or host.
			Controller->Player = NewObject<ULocalPlayer>(GEngine, NAME_None, RF_Transient);
			Controller->Possess(Character);
			Movement = CastChecked<URpgCharacterMovementComponent>(Character->GetCharacterMovement());
			// Flying keeps the seam free of floor geometry; it is not airborne for the stance.
			Movement->SetMovementMode(MOVE_Flying);
			Movement->MaxFlySpeed = 600.f;
			Movement->RotationRate = FRotator(0.f, 2000.f, 0.f);
			ASC = NewObject<URpgAbilitySystemComponent>(Character, NAME_None, RF_Transient);
			Character->AddInstanceComponent(ASC);
			ASC->RegisterComponent();
			ASC->AddAttributeSetSubobject(NewObject<URpgHealthSet>(Character, NAME_None, RF_Transient));
			Extension = URpgPawnExtensionComponent::FindPawnExtensionComponent(Character);
			Extension->SetPawnData(NewObject<URpgPawnData>(Character, NAME_None, RF_Transient));
			Extension->InitializeAbilitySystemComponent(ASC, Character);
			return Test.TestTrue(TEXT("The fixture can grant abilities"), ASC->HasGrantAuthority());
		}

		FGameplayAbilitySpecHandle Grant(UClass* AbilityClass, const FGameplayTag InputTag = FGameplayTag(),
			const float SpeedLimit = 0.f, TSubclassOf<URpgCameraMode> CameraMode = nullptr)
		{
			FGameplayAbilitySpec Spec(AbilityClass, 1);
			if (InputTag.IsValid())
			{
				Spec.GetDynamicSpecSourceTags().AddTag(InputTag);
			}
			const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
			if (URpgRangedAimAutomationAbility* Aim = Instance(Handle))
			{
				Aim->AimDefinition.MovementSpeedLimit = SpeedLimit;
				Aim->AimDefinition.CameraMode = CameraMode;
			}
			return Handle;
		}

		URpgRangedAimAutomationAbility* Instance(const FGameplayAbilitySpecHandle Handle) const
		{
			const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
			return Spec ? Cast<URpgRangedAimAutomationAbility>(Spec->GetPrimaryInstance()) : nullptr;
		}

		bool IsActive(const FGameplayAbilitySpecHandle Handle) const
		{
			const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
			return Spec && Spec->IsActive();
		}

		void Press(const FGameplayTag InputTag) const
		{
			ASC->AbilityInputTagPressed(InputTag);
			ASC->ProcessAbilityInput(0.f, false);
		}

		void Release(const FGameplayTag InputTag) const
		{
			ASC->AbilityInputTagReleased(InputTag);
			ASC->ProcessAbilityInput(0.f, false);
		}

		/** Runs the current activation's aim task once, as its ticking task component would. */
		void TickAim(const FGameplayAbilitySpecHandle Handle) const
		{
			URpgRangedAimAutomationAbility* Aim = Instance(Handle);
			if (Aim && Aim->AimTask && IsActive(Handle))
			{
				Aim->AimTask->TickTask(1.f / 60.f);
			}
		}

		bool HasAimTag() const { return ASC->HasMatchingGameplayTag(RpgGameplayTags::State_Ranged_Aiming); }

		TStrongObjectPtr<UGameInstance> GameInstance;
		UWorld* World = nullptr;
		ARpgCharacter* Character = nullptr;
		APlayerController* Controller = nullptr;
		URpgCharacterMovementComponent* Movement = nullptr;
		URpgPawnExtensionComponent* Extension = nullptr;
		URpgAbilitySystemComponent* ASC = nullptr;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgRangedAimTokensTest,
	"SurvivalRpg.Combat.Ranged.Aim.TokensHoldTheCombinedStance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgRangedAimTokensTest::RunTest(const FString& Parameters)
{
	using namespace RpgRangedAimTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	const UClass* AimCamera = URpgCameraMode_ThirdPerson::StaticClass();
	const UClass* DrawCamera = URpgCameraMode::StaticClass();
	const FGameplayAbilitySpecHandle Aim = Fixture.Grant(URpgRangedAimAutomationAbility::StaticClass(),
		RpgGameplayTags::InputTag_Weapon_Block, 200.f, URpgCameraMode_ThirdPerson::StaticClass());
	const FGameplayAbilitySpecHandle Draw = Fixture.Grant(URpgRangedAimAutomationDrawAbility::StaticClass(),
		RpgGameplayTags::InputTag_Weapon_Primary, 150.f, URpgCameraMode::StaticClass());
	URpgPawnGameplayComponent* PawnGameplay = NewObject<URpgPawnGameplayComponent>(Fixture.Character, NAME_None, RF_Transient);
	Fixture.Character->AddInstanceComponent(PawnGameplay);
	PawnGameplay->RegisterComponent();

	// Holding the aim input enters the stance with its camera and movement cap.
	Fixture.Press(RpgGameplayTags::InputTag_Weapon_Block);
	if (!TestTrue(TEXT("Holding the aim input aims"), Fixture.IsActive(Aim))) return false;
	TestTrue(TEXT("The stance is active"), Fixture.ASC->IsRangedAimActive());
	TestTrue(TEXT("The stance replicates through its state tag"), Fixture.HasAimTag());
	TestEqual(TEXT("The aim caps movement"), Fixture.ASC->GetRangedAimMovementSpeedLimit(), 200.f);
	TestTrue(TEXT("The aim owns the camera"), Fixture.ASC->GetRangedAimCameraMode().Get() == AimCamera);
	TestTrue(TEXT("The pawn's camera follows the stance"), PawnGameplay->DetermineCameraMode().Get() == AimCamera);
	const uint32 FirstAimToken = Fixture.Instance(Aim)->AimTask->GetTokenForTests();

	// Drawing adds its own token; the stance takes the lower cap and keeps the older camera.
	Fixture.Press(RpgGameplayTags::InputTag_Weapon_Primary);
	if (!TestTrue(TEXT("The primary input draws beside the held aim"), Fixture.IsActive(Draw) && Fixture.IsActive(Aim))) return false;
	TestEqual(TEXT("Both tokens take the lower cap"), Fixture.ASC->GetRangedAimMovementSpeedLimit(), 150.f);
	TestTrue(TEXT("The oldest token keeps the camera"), Fixture.ASC->GetRangedAimCameraMode().Get() == AimCamera);

	// Ending the held aim keeps the draw's stance and hands the camera to it.
	Fixture.Release(RpgGameplayTags::InputTag_Weapon_Block);
	Fixture.ASC->CancelAbilityHandle(Aim);
	TestFalse(TEXT("The aim ended"), Fixture.IsActive(Aim));
	TestTrue(TEXT("The draw still holds the stance"), Fixture.ASC->IsRangedAimActive() && Fixture.HasAimTag());
	TestEqual(TEXT("The draw keeps its cap"), Fixture.ASC->GetRangedAimMovementSpeedLimit(), 150.f);
	TestTrue(TEXT("The camera reflects the remaining draw"), PawnGameplay->DetermineCameraMode().Get() == DrawCamera);

	// A late cleanup of the old aim activation cannot retire a newer one.
	Fixture.Press(RpgGameplayTags::InputTag_Weapon_Block);
	if (!TestTrue(TEXT("Aiming again starts a new activation"), Fixture.IsActive(Aim))) return false;
	TestTrue(TEXT("The new activation holds a new token"), Fixture.Instance(Aim)->AimTask->GetTokenForTests() != FirstAimToken);
	Fixture.ASC->EndRangedAim(Fixture.Instance(Aim), FirstAimToken);
	Fixture.ASC->CancelAbilityHandle(Draw);
	TestTrue(TEXT("A stale token leaves the new aim's camera"), Fixture.ASC->GetRangedAimCameraMode().Get() == AimCamera);

	// Ending the draw kept the held aim.
	TestTrue(TEXT("The held aim keeps the stance after the draw"), Fixture.ASC->IsRangedAimActive() && Fixture.HasAimTag());
	TestEqual(TEXT("The held aim's cap returns"), Fixture.ASC->GetRangedAimMovementSpeedLimit(), 200.f);
	TestTrue(TEXT("The held aim's camera returns"), PawnGameplay->DetermineCameraMode().Get() == AimCamera);

	// Without a token the stance, its tag, cap and camera are gone.
	Fixture.Release(RpgGameplayTags::InputTag_Weapon_Block);
	Fixture.ASC->CancelAbilityHandle(Aim);
	TestFalse(TEXT("No token leaves no stance"), Fixture.ASC->IsRangedAimActive());
	TestFalse(TEXT("The state tag clears"), Fixture.HasAimTag());
	TestEqual(TEXT("No cap remains"), Fixture.ASC->GetRangedAimMovementSpeedLimit(), 0.f);
	TestNull(TEXT("No aim camera remains"), Fixture.ASC->GetRangedAimCameraMode().Get());
	TestTrue(TEXT("The pawn's camera leaves the aim camera"), PawnGameplay->DetermineCameraMode().Get() != AimCamera);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgRangedAimMovementTest,
	"SurvivalRpg.Combat.Ranged.Aim.CMCLeaseCapsSpeedAndFacesTheCamera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgRangedAimMovementTest::RunTest(const FString& Parameters)
{
	using namespace RpgRangedAimTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	const FGameplayAbilitySpecHandle Aim = Fixture.Grant(URpgRangedAimAutomationAbility::StaticClass(), RpgGameplayTags::InputTag_Weapon_Block, 150.f);
	TestEqual(TEXT("Without a stance the ordinary speed applies"), Fixture.Movement->GetMaxSpeed(), 600.f);
	Fixture.Press(RpgGameplayTags::InputTag_Weapon_Block);
	if (!TestTrue(TEXT("The stance starts"), Fixture.IsActive(Aim))) return false;
	TestEqual(TEXT("The stance caps the movement speed"), Fixture.Movement->GetMaxSpeed(), 150.f);
	float Limit = 0.f;
	TestTrue(TEXT("Movement samples the stance"), Fixture.Movement->GetRangedAimMovementForMove(Limit));
	TestEqual(TEXT("Movement samples the cap"), Limit, 150.f);

	// The live move records the stance; it faces the camera's yaw.
	FNetworkPredictionData_Client_Character* Prediction = static_cast<FNetworkPredictionData_Client_Character*>(
		Fixture.Movement->GetPredictionData_Client());
	const FSavedMovePtr Move = Prediction->AllocateNewMove();
	Move->Clear();
	Fixture.Controller->SetControlRotation(FRotator(0.f, 90.f, 0.f));
	Move->SetMoveFor(Fixture.Character, .1f, FVector::ZeroVector, *Prediction);
	Fixture.Movement->PerformMovement(Move->DeltaTime);
	Move->PostUpdate(Fixture.Character, FSavedMove_Character::PostUpdate_Record);
	TestTrue(TEXT("The aim stance faces the camera"), FMath::IsNearlyEqual(Fixture.Character->GetActorRotation().Yaw, 90.0, .01));

	// After the stance ended, a replay of that move still uses the recorded stance, and nothing leaks outside it.
	Fixture.Release(RpgGameplayTags::InputTag_Weapon_Block);
	Fixture.ASC->CancelAbilityHandle(Aim);
	TestFalse(TEXT("Today's stance has ended"), Fixture.ASC->IsRangedAimActive());
	Fixture.Controller->SetControlRotation(FRotator(0.f, -60.f, 0.f));
	Fixture.Character->SetActorRotation(FRotator::ZeroRotator);
	Fixture.Character->bClientUpdating = true;
	Fixture.Movement->SetCurrentReplayedSavedMove(Move.Get());
	Move->PrepMoveFor(Fixture.Character);
	Fixture.Movement->PerformMovement(Move->DeltaTime);
	Move->PostUpdate(Fixture.Character, FSavedMove_Character::PostUpdate_Replay);
	Fixture.Movement->SetCurrentReplayedSavedMove(nullptr);
	Fixture.Character->bClientUpdating = false;
	TestTrue(TEXT("The replay keeps the recorded aim facing"), FMath::IsNearlyEqual(Fixture.Character->GetActorRotation().Yaw, 90.0, .01));
	Limit = -1.f;
	TestFalse(TEXT("The recorded stance does not leak outside replay"), Fixture.Movement->GetRangedAimMovementForMove(Limit));
	TestEqual(TEXT("No stale cap remains"), Limit, 0.f);
	TestEqual(TEXT("The ordinary speed returns"), Fixture.Movement->GetMaxSpeed(), 600.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgRangedAimInterruptionTest,
	"SurvivalRpg.Combat.Ranged.Aim.InterruptionsCancelAndNeedFreshInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgRangedAimInterruptionTest::RunTest(const FString& Parameters)
{
	using namespace RpgRangedAimTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	const FGameplayTag AimInput = RpgGameplayTags::InputTag_Weapon_Block;
	const FGameplayAbilitySpecHandle Aim = Fixture.Grant(URpgRangedAimAutomationAbility::StaticClass(), AimInput, 150.f);
	const FGameplayAbilitySpecHandle Draw = Fixture.Grant(URpgRangedAimAutomationDrawAbility::StaticClass(), RpgGameplayTags::InputTag_Weapon_Primary, 150.f);
	const FGameplayAbilitySpecHandle Exclusive = Fixture.Grant(URpgRangedAimAutomationExclusiveAbility::StaticClass());
	const FGameplayAbilitySpecHandle Idle = Fixture.Grant(URpgRangedAimAutomationIdleAbility::StaticClass());

	// The shot's own exclusive draw never interrupts the held aim.
	Fixture.Press(AimInput);
	Fixture.Press(RpgGameplayTags::InputTag_Weapon_Primary);
	Fixture.TickAim(Aim);
	TestTrue(TEXT("Aim and draw hold the stance together"), Fixture.IsActive(Aim) && Fixture.IsActive(Draw));
	Fixture.Release(RpgGameplayTags::InputTag_Weapon_Primary);
	Fixture.ASC->CancelAbilityHandle(Draw);

	// A dodge-like exclusive ability interrupts the held aim; the held button does not restart it.
	TestTrue(TEXT("The exclusive ability starts"), Fixture.ASC->TryActivateAbility(Exclusive));
	Fixture.TickAim(Aim);
	TestFalse(TEXT("An exclusive ability interrupts the stance"), Fixture.IsActive(Aim));
	TestEqual(TEXT("The task reported one interruption"), Fixture.Instance(Aim)->InterruptedCount, 1);
	TestEqual(TEXT("It names the exclusive ability"), Fixture.Instance(Aim)->LastInterruption, FName(TEXT("ExclusiveAbility")));
	TestFalse(TEXT("The interrupted stance leaves no tag"), Fixture.HasAimTag());
	Fixture.ASC->CancelAbilityHandle(Exclusive);
	Fixture.Press(AimInput);
	TestFalse(TEXT("The still held button does not aim again"), Fixture.IsActive(Aim));
	Fixture.Release(AimInput);
	Fixture.Press(AimInput);
	TestTrue(TEXT("A fresh press aims again"), Fixture.IsActive(Aim));

	// A sprint that starts during the stance interrupts it.
	TestTrue(TEXT("The lease holder starts"), Fixture.ASC->TryActivateAbility(Idle));
	UGameplayAbility* IdleInstance = Fixture.ASC->FindAbilitySpecFromHandle(Idle)->GetPrimaryInstance();
	const uint32 SprintLease = Fixture.ASC->BeginSprintMovement(IdleInstance, 500.f);
	TestTrue(TEXT("A sprint lease starts"), SprintLease != 0);
	Fixture.TickAim(Aim);
	TestFalse(TEXT("A sprint interrupts the stance"), Fixture.IsActive(Aim));
	TestEqual(TEXT("It names the sprint"), Fixture.Instance(Aim)->LastInterruption, FName(TEXT("Sprint")));

	// A sprint held before the stance began does not.
	Fixture.Release(AimInput);
	Fixture.Press(AimInput);
	Fixture.TickAim(Aim);
	TestTrue(TEXT("An earlier sprint leaves a new stance alone"), Fixture.IsActive(Aim));
	Fixture.ASC->EndSprintMovement(IdleInstance, SprintLease);
	Fixture.TickAim(Aim);
	TestTrue(TEXT("Its end leaves the stance alone"), Fixture.IsActive(Aim));
	// Once that sprint ended, a new one interrupts the stance.
	const uint32 LaterSprintLease = Fixture.ASC->BeginSprintMovement(IdleInstance, 500.f);
	Fixture.TickAim(Aim);
	TestFalse(TEXT("A sprint started after the earlier one ended interrupts the stance"), Fixture.IsActive(Aim));
	Fixture.ASC->EndSprintMovement(IdleInstance, LaterSprintLease);
	Fixture.Release(AimInput);
	Fixture.Press(AimInput);
	TestTrue(TEXT("A fresh press aims again after the sprint"), Fixture.IsActive(Aim));

	// Leaving the ground, by a jump, a fall or traversal, interrupts it.
	Fixture.Movement->SetMovementMode(MOVE_Falling);
	Fixture.TickAim(Aim);
	TestFalse(TEXT("Leaving the ground interrupts the stance"), Fixture.IsActive(Aim));
	TestEqual(TEXT("It names the airborne avatar"), Fixture.Instance(Aim)->LastInterruption, FName(TEXT("Airborne")));
	TestFalse(TEXT("No stance remains"), Fixture.ASC->IsRangedAimActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgRangedAimBlockedPressTest,
	"SurvivalRpg.Combat.Ranged.Aim.PressDuringABlockingStateWaits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgRangedAimBlockedPressTest::RunTest(const FString& Parameters)
{
	using namespace RpgRangedAimTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	const FGameplayTag AimInput = RpgGameplayTags::InputTag_Weapon_Block;
	const FGameplayAbilitySpecHandle Aim = Fixture.Grant(URpgRangedAimAutomationAbility::StaticClass(), AimInput, 150.f);
	const FGameplayAbilitySpecHandle Exclusive = Fixture.Grant(URpgRangedAimAutomationExclusiveAbility::StaticClass());

	// Pressed during a dodge-like ability, the aim waits instead of cancelling and eating the press.
	TestTrue(TEXT("The dodge-like ability runs"), Fixture.ASC->TryActivateAbility(Exclusive));
	Fixture.Press(AimInput);
	Fixture.TickAim(Aim);
	TestTrue(TEXT("The press is kept"), Fixture.IsActive(Aim));
	TestFalse(TEXT("The stance waits while the dodge lasts"), Fixture.ASC->IsRangedAimActive());
	TestEqual(TEXT("Waiting is no interruption"), Fixture.Instance(Aim)->InterruptedCount, 0);
	Fixture.ASC->CancelAbilityHandle(Exclusive);
	Fixture.TickAim(Aim);
	TestTrue(TEXT("The stance begins when the dodge ends"), Fixture.ASC->IsRangedAimActive() && Fixture.HasAimTag());

	// Pressed in the air, it begins on landing.
	Fixture.Release(AimInput);
	Fixture.ASC->CancelAbilityHandle(Aim);
	Fixture.Movement->SetMovementMode(MOVE_Falling);
	Fixture.Press(AimInput);
	Fixture.TickAim(Aim);
	TestTrue(TEXT("An aim pressed in the air is kept"), Fixture.IsActive(Aim) && !Fixture.ASC->IsRangedAimActive());
	Fixture.Movement->SetMovementMode(MOVE_Flying);
	Fixture.TickAim(Aim);
	TestTrue(TEXT("It aims after landing"), Fixture.ASC->IsRangedAimActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgRangedAimTeardownTest,
	"SurvivalRpg.Combat.Ranged.Aim.StaggerRemovalDeathAndAvatarChangeEndTheStance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgRangedAimTeardownTest::RunTest(const FString& Parameters)
{
	using namespace RpgRangedAimTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	const FGameplayTag AimInput = RpgGameplayTags::InputTag_Weapon_Block;
	FGameplayAbilitySpecHandle Aim = Fixture.Grant(URpgRangedAimAutomationAbility::StaticClass(), AimInput, 150.f);
	const FGameplayAbilitySpecHandle Switching = Fixture.Grant(URpgRangedAimAutomationSwitchingAbility::StaticClass());

	// A stagger starts independent and turns exclusive afterwards; the stance still ends.
	Fixture.Press(AimInput);
	TestTrue(TEXT("The switching ability starts beside the stance"), Fixture.ASC->TryActivateAbility(Switching));
	Fixture.TickAim(Aim);
	TestTrue(TEXT("An independent start leaves the stance"), Fixture.IsActive(Aim));
	URpgGameplayAbility* SwitchingInstance = Cast<URpgGameplayAbility>(Fixture.ASC->FindAbilitySpecFromHandle(Switching)->GetPrimaryInstance());
	TestTrue(TEXT("It turns exclusive"), SwitchingInstance && SwitchingInstance->ChangeActivationGroup(ERpgAbilityActivationGroup::Exclusive_Blocking));
	Fixture.TickAim(Aim);
	TestFalse(TEXT("The live exclusive group interrupts the stance"), Fixture.IsActive(Aim));
	Fixture.ASC->CancelAbilityHandle(Switching);
	Fixture.Release(AimInput);

	// Removing the ability, as an unequip does, ends the stance with its tag and camera.
	Fixture.Press(AimInput);
	TestTrue(TEXT("The stance is back"), Fixture.ASC->IsRangedAimActive());
	Fixture.ASC->ClearAbility(Aim);
	TestFalse(TEXT("Removing the ability ends the stance"), Fixture.ASC->IsRangedAimActive());
	TestFalse(TEXT("Removing the ability clears the tag"), Fixture.HasAimTag());
	TestNull(TEXT("Removing the ability clears the camera"), Fixture.ASC->GetRangedAimCameraMode().Get());
	Fixture.Release(AimInput);

	// Death cancels every ability; the stance goes with it.
	Aim = Fixture.Grant(URpgRangedAimAutomationAbility::StaticClass(), AimInput, 150.f);
	Fixture.Press(AimInput);
	TestTrue(TEXT("The re-granted aim holds the stance"), Fixture.ASC->IsRangedAimActive());
	Fixture.ASC->CancelAbilities();
	TestFalse(TEXT("Cancelling every ability ends the stance"), Fixture.ASC->IsRangedAimActive() || Fixture.HasAimTag());
	Fixture.Release(AimInput);

	// A new avatar drops the stance, and the old activation stops aiming.
	Fixture.Press(AimInput);
	TestTrue(TEXT("The stance is back again"), Fixture.ASC->IsRangedAimActive());
	ARpgCharacter* NewAvatar = Fixture.World->SpawnActor<ARpgCharacter>();
	if (!TestNotNull(TEXT("A second avatar exists"), NewAvatar)) return false;
	Fixture.ASC->InitAbilityActorInfo(Fixture.Character, NewAvatar);
	TestFalse(TEXT("A new avatar has no stance"), Fixture.ASC->IsRangedAimActive());
	Fixture.TickAim(Aim);
	TestFalse(TEXT("The old activation stops aiming"), Fixture.IsActive(Aim));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgRangedAimTuningTest,
	"SurvivalRpg.Combat.Ranged.Aim.MovementTuningIsFixedPerActivation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgRangedAimTuningTest::RunTest(const FString& Parameters)
{
	using namespace RpgRangedAimTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	const FGameplayTag AimInput = RpgGameplayTags::InputTag_Weapon_Block;
	const FGameplayAbilitySpecHandle Aim = Fixture.Grant(URpgRangedAimAutomationAbility::StaticClass(), AimInput, 200.f);
	Fixture.Press(AimInput);
	FRpgSkillTreeAbilityTuning Slower;
	Slower.TuningTag = RpgGameplayTags::Ability_Tuning_Ranged_AimMovement;
	Slower.Operation = ERpgSkillTreeTuningOperation::Multiply;
	Slower.Value = 0.5f;
	TStrongObjectPtr<UGameplayEffect> Passive(URpgAbilityTuningEffectComponent::MakeTransientEffectForTests({ Slower }, 1000.0f));
	Fixture.ASC->ApplyGameplayEffectToSelf(Passive.Get(), 1.0f, Fixture.ASC->MakeEffectContext());
	TestEqual(TEXT("A passive gained while aiming keeps this stance's cap"), Fixture.ASC->GetRangedAimMovementSpeedLimit(), 200.f);
	Fixture.Release(AimInput);
	Fixture.ASC->CancelAbilityHandle(Aim);
	Fixture.Press(AimInput);
	TestEqual(TEXT("The next stance takes the passive's cap"), Fixture.ASC->GetRangedAimMovementSpeedLimit(), 100.f);
	return true;
}

#endif
