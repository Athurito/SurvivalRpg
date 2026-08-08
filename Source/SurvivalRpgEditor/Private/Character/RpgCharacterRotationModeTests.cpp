#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GameplayAbilitiesDeveloperSettings.h"
#include "Misc/AutomationTest.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
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

#endif // WITH_DEV_AUTOMATION_TESTS
