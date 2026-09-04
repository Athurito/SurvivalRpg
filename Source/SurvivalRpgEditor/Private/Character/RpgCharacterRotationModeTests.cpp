#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "GameplayAbilitiesDeveloperSettings.h"
#include "Misc/AutomationTest.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMovementComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "Tests/AutomationCommon.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCharacterRotationModeResolverTest,
	"SurvivalRpg.Character.RotationMode.ResolverAndPolicy",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgCharacterRotationModeResolverTest::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("Aim request has priority over combat strafe and PawnData"),
		ARpgCharacter::ResolveRotationMode(true, true, ERpgCharacterRotationMode::Free) ==
			ERpgCharacterRotationMode::Aim);
	TestTrue(
		TEXT("Combat strafe request has priority over PawnData"),
		ARpgCharacter::ResolveRotationMode(false, true, ERpgCharacterRotationMode::Free) ==
			ERpgCharacterRotationMode::CombatStrafe);
	TestTrue(
		TEXT("PawnData free mode is preserved without request tags"),
		ARpgCharacter::ResolveRotationMode(false, false, ERpgCharacterRotationMode::Free) ==
			ERpgCharacterRotationMode::Free);
	TestTrue(
		TEXT("PawnData aim mode is preserved without request tags"),
		ARpgCharacter::ResolveRotationMode(false, false, ERpgCharacterRotationMode::Aim) ==
			ERpgCharacterRotationMode::Aim);
	TestTrue(
		TEXT("PawnData aim mode retains priority over a combat-strafe request"),
		ARpgCharacter::ResolveRotationMode(false, true, ERpgCharacterRotationMode::Aim) ==
			ERpgCharacterRotationMode::Aim);

	const FRpgCharacterRotationPolicy FreePolicy =
		ARpgCharacter::GetRotationPolicy(ERpgCharacterRotationMode::Free);
	TestFalse(TEXT("Free mode does not consume controller yaw"), FreePolicy.bUseControllerRotationYaw);
	TestTrue(TEXT("Free mode orients to movement"), FreePolicy.bOrientRotationToMovement);
	TestFalse(TEXT("Free mode does not use controller-desired movement rotation"), FreePolicy.bUseControllerDesiredRotation);
	TestTrue(TEXT("Free mode uses immediate movement-facing yaw"), FMath::IsNearlyEqual(FreePolicy.RotationRateYaw, -1.0f));

	const FRpgCharacterRotationPolicy CuratedFreePolicy =
		ARpgCharacter::GetRotationPolicy(ERpgCharacterRotationMode::Free, 360.0f);
	TestTrue(
		TEXT("PawnData may opt Free mode into controlled yaw"),
		FMath::IsNearlyEqual(CuratedFreePolicy.RotationRateYaw, 360.0f));
	const FRpgCharacterRotationPolicy InvalidFreePolicy =
		ARpgCharacter::GetRotationPolicy(
			ERpgCharacterRotationMode::Free,
			std::numeric_limits<float>::quiet_NaN());
	TestTrue(
		TEXT("Invalid PawnData yaw falls back to the legacy immediate policy"),
		FMath::IsNearlyEqual(InvalidFreePolicy.RotationRateYaw, -1.0f));

	for (const ERpgCharacterRotationMode ControllerFacingMode : {
		ERpgCharacterRotationMode::CombatStrafe,
		ERpgCharacterRotationMode::Aim })
	{
		const FRpgCharacterRotationPolicy Policy =
			ARpgCharacter::GetRotationPolicy(ControllerFacingMode);
		TestTrue(TEXT("Controller-facing mode consumes controller yaw"), Policy.bUseControllerRotationYaw);
		TestFalse(TEXT("Controller-facing mode does not orient to movement"), Policy.bOrientRotationToMovement);
		TestFalse(TEXT("Controller-facing mode does not duplicate controller-desired rotation"), Policy.bUseControllerDesiredRotation);
		TestTrue(TEXT("Controller-facing mode preserves the 720 degree yaw contract"), FMath::IsNearlyEqual(Policy.RotationRateYaw, 720.0f));
	}

	struct FExplicitStanceGateCase
	{
		const TCHAR* Label;
		bool bEnable;
		bool bHasWeapon;
		bool bHasBlockingState;
		bool bIsMovingOnGround;
		bool bIsCrouched;
		bool bWantsToCrouch;
		bool bIsAnyMontagePlaying;
		bool bExpected;
	};
	const FExplicitStanceGateCase GateCases[] = {
		{ TEXT("Valid grounded weapon stance enables"), true, true, false, true, false, false, false, true },
		{ TEXT("Weapon is required for enable"), true, false, false, true, false, false, false, false },
		{ TEXT("Blocking gameplay state rejects enable"), true, true, true, true, false, false, false, false },
		{ TEXT("Airborne character rejects enable"), true, true, false, false, false, false, false, false },
		{ TEXT("Already crouched character rejects enable"), true, true, false, true, true, false, false, false },
		{ TEXT("Pending crouch rejects enable"), true, true, false, true, false, true, false, false },
		{ TEXT("Running montage rejects enable"), true, true, false, true, false, false, true, false },
		{ TEXT("Disable remains allowed when every enable prerequisite fails"), false, false, true, false, true, true, true, true },
	};
	for (const FExplicitStanceGateCase& GateCase : GateCases)
	{
		TestEqual(
			GateCase.Label,
			ARpgCharacter::CanApplyExplicitCombatStanceRequest(
				GateCase.bEnable,
				GateCase.bHasWeapon,
				GateCase.bHasBlockingState,
				GateCase.bIsMovingOnGround,
				GateCase.bIsCrouched,
				GateCase.bWantsToCrouch,
				GateCase.bIsAnyMontagePlaying),
			GateCase.bExpected);
	}

	const URpgPawnData* PawnDataDefaults = GetDefault<URpgPawnData>();
	TestNotNull(TEXT("PawnData CDO is available"), PawnDataDefaults);
	if (PawnDataDefaults)
	{
		TestTrue(
			TEXT("Legacy-compatible PawnData default is CombatStrafe"),
			PawnDataDefaults->DefaultRotationMode == ERpgCharacterRotationMode::CombatStrafe);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCharacterRotationModeReplicationContractTest,
	"SurvivalRpg.Character.RotationMode.ReplicationContract",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgCharacterRotationModeReplicationContractTest::RunTest(const FString& Parameters)
{
	const FProperty* RotationModeProperty =
		ARpgCharacter::StaticClass()->FindPropertyByName(TEXT("RotationMode"));
	TestNotNull(TEXT("Character exposes reflected rotation mode state"), RotationModeProperty);
	if (RotationModeProperty)
	{
		TestTrue(TEXT("Rotation mode participates in actor replication"), RotationModeProperty->HasAnyPropertyFlags(CPF_Net));
		TestEqual(TEXT("Rotation mode reconciles through OnRep_RotationMode"), RotationModeProperty->RepNotifyFunc, FName(TEXT("OnRep_RotationMode")));
	}
	const FProperty* RevisionProperty =
		ARpgCharacter::StaticClass()->FindPropertyByName(TEXT("RotationModeRevision"));
	TestNotNull(TEXT("Rotation handoffs have an authoritative replicated revision"), RevisionProperty);
	if (RevisionProperty)
	{
		TestTrue(TEXT("Rotation revisions participate in actor replication"), RevisionProperty->HasAnyPropertyFlags(CPF_Net));
		TestEqual(TEXT("A new revision retries the current mode handoff"), RevisionProperty->RepNotifyFunc, FName(TEXT("OnRep_RotationMode")));
	}

	TestTrue(TEXT("Combat-strafe request tag is registered"), RpgGameplayTags::State_Rotation_CombatStrafe.GetTag().IsValid());
	TestTrue(TEXT("Aim request tag is registered"), RpgGameplayTags::State_Rotation_Aim.GetTag().IsValid());
	TestTrue(TEXT("Combat stance input tag is registered"), RpgGameplayTags::InputTag_RotationMode_ToggleCombat.GetTag().IsValid());

	const UGameplayAbilitiesDeveloperSettings* AbilitySettings =
		GetDefault<UGameplayAbilitiesDeveloperSettings>();
	TestNotNull(TEXT("GameplayAbilities developer settings CDO is available"), AbilitySettings);
	if (AbilitySettings)
	{
		TestTrue(TEXT("Target gameplay effects remain predicted"), AbilitySettings->PredictTargetGameplayEffects);
		TestTrue(TEXT("Activation-owned rotation tags replicate for server reconciliation"), AbilitySettings->ReplicateActivationOwnedTags);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCharacterRotationHandoffTimestampTest,
	"SurvivalRpg.Character.RotationMode.HandoffTimestampBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCharacterRotationHandoffTimestampTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper WorldWrapper;
	if (!WorldWrapper.CreateTestWorld(EWorldType::Game) || !WorldWrapper.BeginPlayInTestWorld())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}
	ARpgCharacter* Character = WorldWrapper.GetTestWorld()->SpawnActor<ARpgCharacter>();
	URpgCharacterMovementComponent* Movement = Character
		? Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	if (!TestNotNull(TEXT("Rotation handoff owns a live CMC"), Movement))
	{
		return false;
	}

	Movement->RequestOwnerRotationSynchronization(10.0f);
	TestFalse(TEXT("A pre-Free move cannot complete the handoff"),
		Movement->IsOwnerRotationSynchronizationCorrection(9.9f));
	TestFalse(TEXT("The last pre-Free SavedMove itself cannot complete the handoff"),
		Movement->IsOwnerRotationSynchronizationCorrection(10.0f));
	TestTrue(TEXT("A correction for a newer Free move completes the handoff"),
		Movement->IsOwnerRotationSynchronizationCorrection(10.1f));
	TestFalse(TEXT("Invalid corrections cannot complete the handoff"),
		Movement->IsOwnerRotationSynchronizationCorrection(std::numeric_limits<float>::quiet_NaN()));
	TestFalse(TEXT("Negative corrections cannot complete the handoff"),
		Movement->IsOwnerRotationSynchronizationCorrection(-1.0f));
	Movement->CancelOwnerRotationSynchronization();
	TestFalse(TEXT("A cancelled handoff ignores later responses"),
		Movement->IsOwnerRotationSynchronizationCorrection(10.1f));

	const float ResetPeriod = Movement->MinTimeBetweenTimeStampResets;
	Movement->RequestOwnerRotationSynchronization(ResetPeriod - 0.1f);
	TestTrue(TEXT("The first newer move after a CMC timestamp reset is accepted"),
		Movement->IsOwnerRotationSynchronizationCorrection(0.1f));
	Movement->RequestOwnerRotationSynchronization(0.1f);
	TestFalse(TEXT("A delayed response from before the reset cannot confirm the new revision"),
		Movement->IsOwnerRotationSynchronizationCorrection(ResetPeriod - 0.1f));
	TestTrue(TEXT("A newer response in the current timestamp epoch is accepted"),
		Movement->IsOwnerRotationSynchronizationCorrection(0.2f));
	Movement->CancelOwnerRotationSynchronization();
	Movement->RequestOwnerRotationSynchronization(std::numeric_limits<float>::infinity());
	TestFalse(TEXT("An invalid request never arms a correction"),
		Movement->IsOwnerRotationSynchronizationCorrection(1.0f));

	// Equal positions normally produce only GoodMove acknowledgements. Exercise the actual
	// engine error/response path so a yaw-only handoff cannot silently omit its correction.
	Movement->SetMovementMode(MOVE_Flying);
	Movement->bOrientRotationToMovement = true;
	Character->SetActorRotation(FRotator(0.0, 30.0, 0.0));
	FNetworkPredictionData_Server_Character* ServerData = Movement->GetPredictionData_Server_Character();
	ServerData->LastUpdateTime = WorldWrapper.GetTestWorld()->TimeSeconds;
	const auto ProcessEqualPositionMove = [Movement, Character](float TimeStamp)
	{
		Movement->ServerMoveHandleClientError(TimeStamp, 1.0f / 60.0f, FVector::ZeroVector,
			Character->GetActorLocation(), nullptr, NAME_None, Movement->PackNetworkMovementMode());
	};
	Movement->RequestOwnerRotationSynchronization(10.0f);
	ProcessEqualPositionMove(10.0f);
	TestTrue(TEXT("The last pre-Free move remains a normal acknowledgement"),
		ServerData->PendingAdjustment.bAckGoodMove);
	ProcessEqualPositionMove(10.1f);
	TestFalse(TEXT("A newer Free move forces correction even with zero positional error"),
		ServerData->PendingAdjustment.bAckGoodMove);
	TestEqual(TEXT("The correction belongs to the eligible move timestamp"),
		ServerData->PendingAdjustment.TimeStamp, 10.1f);
	FCharacterMoveResponseDataContainer Response;
	Response.ServerFillResponseData(*Movement, ServerData->PendingAdjustment);
	TestTrue(TEXT("The normal CMC correction carries authoritative yaw"), Response.bHasRotation);
	TestTrue(TEXT("Correction yaw comes from the authoritative capsule"),
		FMath::IsNearlyEqual(ServerData->PendingAdjustment.NewRot.Yaw, 30.0));
	ProcessEqualPositionMove(10.2f);
	TestFalse(TEXT("Correction retries persist until the owner confirms receipt"),
		ServerData->PendingAdjustment.bAckGoodMove);
	Movement->CancelOwnerRotationSynchronization();
	ProcessEqualPositionMove(10.3f);
	TestTrue(TEXT("Confirmation restores ordinary acknowledgements without repeated corrections"),
		ServerData->PendingAdjustment.bAckGoodMove);
	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
