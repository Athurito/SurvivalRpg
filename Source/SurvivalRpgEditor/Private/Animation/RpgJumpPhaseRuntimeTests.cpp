// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "SurvivalRpg/Animation/RpgGaspLocomotionConfig.h"
#include "SurvivalRpg/Animation/RpgGaspPresentationProfile.h"
#include "SurvivalRpg/Animation/RpgJumpRuntime.h"
#include "SurvivalRpg/Animation/RpgLandingRuntime.h"
#include "SurvivalRpg/Animation/RpgMotionMatchingRuntime.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgJumpPhaseRuntimeTest,
	"SurvivalRpg.Animation.Jump.Runtime.PhaseAndProceduralGates",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgGaspPresentationProfileValidationTest,
	"SurvivalRpg.Animation.Gasp.PresentationProfileValidation",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgGaspPresentationProfileValidationTest::RunTest(const FString& Parameters)
{
	URpgGaspPresentationProfile* Profile = NewObject<URpgGaspPresentationProfile>();
	TestTrue(TEXT("An empty presentation profile is invalid"), Profile->ValidateProfile().bIsEmpty);

	UAnimSequence* Ground = NewObject<UAnimSequence>();
	UAnimSequence* Jump = NewObject<UAnimSequence>();
	UAnimSequence* Backward = NewObject<UAnimSequence>();
	UAnimSequence* Fall = NewObject<UAnimSequence>();
	UAnimSequence* Landing = NewObject<UAnimSequence>();
	Fall->bLoop = true;

	const auto AddMembership = [Profile](
		UAnimSequenceBase* Asset,
		ERpgGaspPresentationAssetCategory Category)
	{
		FRpgGaspPresentationAssetMembership& Membership =
			Profile->AssetMemberships.AddDefaulted_GetRef();
		Membership.Asset = Asset;
		Membership.Category = Category;
	};
	AddMembership(Ground, ERpgGaspPresentationAssetCategory::GroundMoving);
	AddMembership(Jump, ERpgGaspPresentationAssetCategory::JumpStart);
	AddMembership(Backward, ERpgGaspPresentationAssetCategory::BackwardJumpStart);
	AddMembership(Fall, ERpgGaspPresentationAssetCategory::AirborneFall);
	AddMembership(Landing, ERpgGaspPresentationAssetCategory::Landing);

	for (uint8 RoleValue = static_cast<uint8>(ERpgMotionMatchingDatabaseRole::None) + 1;
		RoleValue < static_cast<uint8>(ERpgMotionMatchingDatabaseRole::Count);
		++RoleValue)
	{
		const ERpgMotionMatchingDatabaseRole Role =
			static_cast<ERpgMotionMatchingDatabaseRole>(RoleValue);
		UPoseSearchDatabase* Database = NewObject<UPoseSearchDatabase>();
		Database->Tags.Add(RpgGaspLocomotionConfig::GetDatabaseRoleTag(Role));
		FPoseSearchDatabaseAnimationAsset DatabaseEntry;
		switch (Role)
		{
		case ERpgMotionMatchingDatabaseRole::Jump:
			DatabaseEntry.AnimAsset = Jump;
			break;
		case ERpgMotionMatchingDatabaseRole::StandLightLanding:
		case ERpgMotionMatchingDatabaseRole::StandHeavyLanding:
		case ERpgMotionMatchingDatabaseRole::WalkLightLanding:
		case ERpgMotionMatchingDatabaseRole::WalkHeavyLanding:
		case ERpgMotionMatchingDatabaseRole::RunLightLanding:
		case ERpgMotionMatchingDatabaseRole::RunHeavyLanding:
			DatabaseEntry.AnimAsset = Landing;
			break;
		default:
			DatabaseEntry.AnimAsset = Ground;
			break;
		}
		Database->AddAnimationAsset(DatabaseEntry);
		Profile->RuntimeMotionMatchingDatabases.Add(Database);
	}
	TestTrue(
		TEXT("A complete presentation, database, and compatibility-tuning profile passes"),
		Profile->ValidateProfile().IsValid());
	const auto TestCoverageMismatch = [this, Profile](
		ERpgMotionMatchingDatabaseRole Role,
		UAnimationAsset* InvalidAsset,
		bool FRpgGaspPresentationProfileValidation::* MismatchFlag,
		const TCHAR* ContractDescription)
	{
		const int32 DatabaseIndex = static_cast<int32>(Role) - 1;
		UPoseSearchDatabase* SavedDatabase =
			Profile->RuntimeMotionMatchingDatabases[DatabaseIndex];
		UPoseSearchDatabase* InvalidDatabase = NewObject<UPoseSearchDatabase>();
		InvalidDatabase->Tags.Add(RpgGaspLocomotionConfig::GetDatabaseRoleTag(Role));
		FPoseSearchDatabaseAnimationAsset InvalidEntry;
		InvalidEntry.AnimAsset = InvalidAsset;
		InvalidDatabase->AddAnimationAsset(InvalidEntry);
		Profile->RuntimeMotionMatchingDatabases[DatabaseIndex] = InvalidDatabase;

		const FRpgGaspPresentationProfileValidation Validation =
			Profile->ValidateProfile();
		TestTrue(ContractDescription, Validation.*MismatchFlag);
		TestFalse(
			TEXT("A presentation coverage mismatch invalidates the runtime database set"),
			Validation.IsRuntimeDatabaseConfigValid());
		Profile->RuntimeMotionMatchingDatabases[DatabaseIndex] = SavedDatabase;
	};
	TestCoverageMismatch(
		ERpgMotionMatchingDatabaseRole::StandWalk,
		Landing,
		&FRpgGaspPresentationProfileValidation::bHasGroundMovingCoverageMismatch,
		TEXT("A moving Ground role rejects an asset without GroundMoving membership"));
	TestCoverageMismatch(
		ERpgMotionMatchingDatabaseRole::Jump,
		Ground,
		&FRpgGaspPresentationProfileValidation::bHasAirborneCoverageMismatch,
		TEXT("The Jump role rejects an asset without Airborne membership"));
	TestCoverageMismatch(
		ERpgMotionMatchingDatabaseRole::StandLightLanding,
		Ground,
		&FRpgGaspPresentationProfileValidation::bHasLandingCoverageMismatch,
		TEXT("A Landing role rejects an asset without Landing membership"));
	TestTrue(
		TEXT("Restoring all three presentation domains restores profile validity"),
		Profile->ValidateProfile().IsValid());

	FRpgGaspMotionMatchingDatabaseLookup DatabaseLookup;
	TestTrue(TEXT("A valid profile builds a database-role lookup"), DatabaseLookup.Build(Profile));
	TestEqual(
		TEXT("The database lookup resolves a tagged Jump database in both directions"),
		static_cast<uint8>(DatabaseLookup.FindRole(
			DatabaseLookup.FindDatabase(ERpgMotionMatchingDatabaseRole::Jump))),
		static_cast<uint8>(ERpgMotionMatchingDatabaseRole::Jump));

	UPoseSearchDatabase* SavedFirstDatabase =
		Profile->RuntimeMotionMatchingDatabases[0];
	UPoseSearchDatabase* EmptyTaggedDatabase = NewObject<UPoseSearchDatabase>();
	EmptyTaggedDatabase->Tags.Add(RpgGaspLocomotionConfig::GetDatabaseRoleTag(
		ERpgMotionMatchingDatabaseRole::StandIdle));
	Profile->RuntimeMotionMatchingDatabases[0] = EmptyTaggedDatabase;
	TestTrue(
		TEXT("A correctly tagged runtime database without animation assets is rejected"),
		Profile->ValidateProfile().bHasRuntimeDatabaseWithoutAssets);
	TestFalse(
		TEXT("An empty tagged runtime database fails lookup construction"),
		DatabaseLookup.Build(Profile));
	Profile->RuntimeMotionMatchingDatabases[0] = SavedFirstDatabase;
	TestTrue(
		TEXT("Restoring the covered Stand Idle database rebuilds the complete lookup"),
		DatabaseLookup.Build(Profile));

	UPoseSearchDatabase* SavedLastDatabase = Profile->RuntimeMotionMatchingDatabases.Last();
	Profile->RuntimeMotionMatchingDatabases.Last() = nullptr;
	TestTrue(
		TEXT("A null profile runtime database is rejected"),
		Profile->ValidateProfile().bHasNullRuntimeDatabase);
	TestFalse(TEXT("A null runtime database fails lookup construction"), DatabaseLookup.Build(Profile));
	TestNull(
		TEXT("A failed database rebuild clears the previous lookup"),
		DatabaseLookup.FindDatabase(ERpgMotionMatchingDatabaseRole::Jump));
	Profile->RuntimeMotionMatchingDatabases.Last() = SavedLastDatabase;

	Profile->RuntimeMotionMatchingDatabases.Last() =
		Profile->RuntimeMotionMatchingDatabases[0];
	const FRpgGaspPresentationProfileValidation DuplicateDatabaseValidation =
		Profile->ValidateProfile();
	TestTrue(
		TEXT("A duplicate profile database pointer is rejected"),
		DuplicateDatabaseValidation.bHasDuplicateRuntimeDatabase);
	TestTrue(
		TEXT("A duplicate profile database also exposes the missing role"),
		DuplicateDatabaseValidation.bHasMissingRuntimeDatabaseRole);
	Profile->RuntimeMotionMatchingDatabases.Last() = SavedLastDatabase;

	const FName SavedLastRoleTag = SavedLastDatabase->Tags[0];
	SavedLastDatabase->Tags[0] = FName(TEXT("Rpg.MotionMatching.Role.RunHeavyLandng"));
	TestTrue(
		TEXT("An unknown project role tag is rejected"),
		Profile->ValidateProfile().bHasInvalidRuntimeDatabaseRoleTag);
	SavedLastDatabase->Tags[0] = SavedLastRoleTag;

	FRpgGaspLocomotionTuning InvalidTuning = Profile->LocomotionTuning;
	InvalidTuning.WalkStopMinimumSpeed = InvalidTuning.RunStopMinimumSpeed + 1.0f;
	Profile->LocomotionTuning = InvalidTuning;
	TestTrue(
		TEXT("An unordered designer stop threshold is rejected"),
		Profile->ValidateProfile().bHasInvalidTuning);
	Profile->LocomotionTuning = FRpgGaspLocomotionTuning();
	Profile->LocomotionTuning.TurnActivationThreshold =
		std::numeric_limits<float>::quiet_NaN();
	TestTrue(
		TEXT("Non-finite designer tuning is rejected"),
		Profile->ValidateProfile().bHasInvalidTuning);
	Profile->LocomotionTuning = FRpgGaspLocomotionTuning();
	TestTrue(
		TEXT("Restoring compatibility tuning restores the complete profile"),
		Profile->ValidateProfile().IsValid());

	FRpgGaspPresentationAssetLookup Lookup;
	TestTrue(TEXT("A valid profile builds a worker-safe lookup"), Lookup.Build(Profile));
	TestTrue(TEXT("Backward membership implies JumpStart"), Lookup.HasTrait(Backward, ERpgGaspPresentationAssetTrait::JumpStart));
	TestTrue(TEXT("Backward membership implies Airborne"), Lookup.HasTrait(Backward, ERpgGaspPresentationAssetTrait::Airborne));
	TestTrue(TEXT("Backward membership keeps its specialized trait"), Lookup.HasTrait(Backward, ERpgGaspPresentationAssetTrait::BackwardJumpStart));
	TestTrue(TEXT("AirborneFall membership implies Airborne"), Lookup.HasTrait(Fall, ERpgGaspPresentationAssetTrait::Airborne));
	TestFalse(TEXT("The None trait never matches a mapped asset"), Lookup.HasTrait(Ground, ERpgGaspPresentationAssetTrait::None));

	AddMembership(Ground, ERpgGaspPresentationAssetCategory::Landing);
	TestTrue(TEXT("Duplicate assets are rejected"), Profile->ValidateProfile().bHasDuplicateAsset);
	TestFalse(TEXT("A duplicate profile fails closed"), Lookup.Build(Profile));
	TestFalse(TEXT("A failed build retains no previous traits"), Lookup.HasTrait(Backward, ERpgGaspPresentationAssetTrait::Airborne));
	Profile->AssetMemberships.Pop();

	Profile->AssetMemberships[0].Category = ERpgGaspPresentationAssetCategory::None;
	TestTrue(TEXT("Unassigned categories are rejected"), Profile->ValidateProfile().bHasUnassignedCategory);
	Profile->AssetMemberships[0].Category = ERpgGaspPresentationAssetCategory::GroundMoving;

	Jump->bLoop = true;
	TestTrue(TEXT("Looping JumpStart entries are rejected"), Profile->ValidateProfile().bHasLoopingJumpStart);
	Jump->bLoop = false;
	Fall->bLoop = false;
	TestTrue(TEXT("Non-looping AirborneFall entries are rejected"), Profile->ValidateProfile().bHasNonLoopingAirborneFall);
	Fall->bLoop = true;
	Landing->bLoop = true;
	TestTrue(TEXT("Looping Landing entries are rejected"), Profile->ValidateProfile().bHasLoopingLanding);

	return true;
}

bool FRpgJumpPhaseRuntimeTest::RunTest(const FString& Parameters)
{
	const FRpgGaspLocomotionTuning DefaultTuning;

	auto ResolvePhysicalTransition = [](
		ERpgJumpPhase CurrentPhase,
		ERpgLocomotionMovementState MovementState,
		bool bIsFalling,
		float VerticalVelocity,
		bool bHardReset = false)
	{
		FRpgJumpPhysicalSnapshot Snapshot;
		Snapshot.MovementState = MovementState;
		Snapshot.VerticalVelocity = VerticalVelocity;
		Snapshot.bIsFalling = bIsFalling;
		Snapshot.bHardReset = bHardReset;
		return RpgJumpRuntime::ResolvePhysicalTransition(CurrentPhase, Snapshot);
	};
	const FRpgJumpPhysicalTransitionResult AscendingTakeoff = ResolvePhysicalTransition(
		ERpgJumpPhase::Grounded,
		ERpgLocomotionMovementState::Airborne,
		false,
		100.0f);
	TestEqual(
		TEXT("Physical airborne state enters the cosmetic airborne phase"),
		AscendingTakeoff.Transition,
		ERpgJumpPhysicalTransition::EnterAirborne);
	TestTrue(TEXT("Strictly positive takeoff velocity arms the backward-start opportunity"), AscendingTakeoff.bAscendingTakeoff);
	TestEqual(
		TEXT("Either falling signal enters the cosmetic airborne phase"),
		ResolvePhysicalTransition(
			ERpgJumpPhase::Grounded,
			ERpgLocomotionMovementState::Grounded,
			true,
			-10.0f).Transition,
		ERpgJumpPhysicalTransition::EnterAirborne);
	TestEqual(
		TEXT("A physical touchdown is exposed before landing selection changes the phase"),
		ResolvePhysicalTransition(
			ERpgJumpPhase::Airborne,
			ERpgLocomotionMovementState::Grounded,
			false,
			0.0f).Transition,
		ERpgJumpPhysicalTransition::Touchdown);
	TestEqual(
		TEXT("An active landing remains under the landing lifecycle while grounded"),
		ResolvePhysicalTransition(
			ERpgJumpPhase::Landing,
			ERpgLocomotionMovementState::Grounded,
			false,
			0.0f).Transition,
		ERpgJumpPhysicalTransition::None);
	TestEqual(
		TEXT("A stable physical airborne phase does not manufacture a new epoch"),
		ResolvePhysicalTransition(
			ERpgJumpPhase::Airborne,
			ERpgLocomotionMovementState::Airborne,
			true,
			300.0f).Transition,
		ERpgJumpPhysicalTransition::None);
	TestEqual(
		TEXT("A second jump during landing opens a fresh airborne phase"),
		ResolvePhysicalTransition(
			ERpgJumpPhase::Landing,
			ERpgLocomotionMovementState::Airborne,
			true,
			300.0f).Transition,
		ERpgJumpPhysicalTransition::EnterAirborne);
	TestEqual(
		TEXT("A hard reset restarts even an established airborne phase"),
		ResolvePhysicalTransition(
			ERpgJumpPhase::Airborne,
			ERpgLocomotionMovementState::Airborne,
			false,
			50.0f,
			true).Transition,
		ERpgJumpPhysicalTransition::EnterAirborne);
	TestEqual(
		TEXT("A grounded hard reset clears cosmetic jump presentation"),
		ResolvePhysicalTransition(
			ERpgJumpPhase::Landing,
			ERpgLocomotionMovementState::Grounded,
			false,
			0.0f,
			true).Transition,
		ERpgJumpPhysicalTransition::ResetGrounded);

	FRpgBackwardJumpStartHoldState ImmediateReleaseState;
	ImmediateReleaseState.bHoldEligible = true;
	FRpgBackwardJumpStartPlaybackSnapshot ImmediateReleaseSnapshot;
	ImmediateReleaseSnapshot.JumpPhase = ERpgJumpPhase::Airborne;
	ImmediateReleaseSnapshot.CurrentAssetTime = 1.8f;
	ImmediateReleaseSnapshot.CurrentAssetLength = 1.9f;
	ImmediateReleaseSnapshot.CurrentAssetPlayRate = 1.0f;
	ImmediateReleaseSnapshot.bCurrentAssetIsAirborne = true;
	ImmediateReleaseSnapshot.bCurrentAssetIsBackwardStart = true;
	const FRpgBackwardJumpStartHoldResult ImmediateRelease =
		RpgJumpRuntime::UpdateBackwardJumpStartHold(
			ImmediateReleaseState,
			ImmediateReleaseSnapshot);
	TestTrue(TEXT("A first backward result may be captured before its near-end release"), ImmediateRelease.bCaptureCurrentAsset);
	TestTrue(TEXT("The same near-end update explicitly clears the transient held asset"), ImmediateRelease.bClearHeldAsset);
	TestFalse(TEXT("A near-end backward result never requests Continuing Pose"), ImmediateRelease.bHoldContinuingPose);
	TestTrue(TEXT("Immediate release still consumes the one opportunity"), ImmediateRelease.State.bOpportunityConsumed);
	TestTrue(TEXT("Immediate release records that the backward path was armed"), ImmediateRelease.State.bHoldWasArmed);

	USkeletalMeshComponent* AnimInstanceOuter = NewObject<USkeletalMeshComponent>();
	URpgAnimInstance* AnimInstance = NewObject<URpgAnimInstance>(AnimInstanceOuter);
	UPoseSearchDatabase* StandLightLandingDatabase = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* StandHeavyLandingDatabase = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* WalkLightLandingDatabase = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* WalkHeavyLandingDatabase = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* RunLightLandingDatabase = NewObject<UPoseSearchDatabase>();
	UPoseSearchDatabase* RunHeavyLandingDatabase = NewObject<UPoseSearchDatabase>();
	UAnimSequence* LandingClip = NewObject<UAnimSequence>();
	UAnimSequence* PreviousAirborneClip = NewObject<UAnimSequence>();
	if (!TestNotNull(TEXT("Transient RPG AnimInstance can be created"), AnimInstance) ||
		!TestNotNull(TEXT("Transient Stand Light landing database can be created"), StandLightLandingDatabase) ||
		!TestNotNull(TEXT("Transient Stand Heavy landing database can be created"), StandHeavyLandingDatabase) ||
		!TestNotNull(TEXT("Transient Walk Light landing database can be created"), WalkLightLandingDatabase) ||
		!TestNotNull(TEXT("Transient Walk Heavy landing database can be created"), WalkHeavyLandingDatabase) ||
		!TestNotNull(TEXT("Transient Run Light landing database can be created"), RunLightLandingDatabase) ||
		!TestNotNull(TEXT("Transient Run Heavy landing database can be created"), RunHeavyLandingDatabase) ||
		!TestNotNull(TEXT("Transient landing clip can be created"), LandingClip))
	{
		return false;
	}
	AnimInstance->LandingMotionMatchingDatabase = StandLightLandingDatabase;
	AnimInstance->StandHeavyLandingMotionMatchingDatabase = StandHeavyLandingDatabase;
	AnimInstance->WalkLightLandingMotionMatchingDatabase = WalkLightLandingDatabase;
	AnimInstance->WalkHeavyLandingMotionMatchingDatabase = WalkHeavyLandingDatabase;
	AnimInstance->RunLightLandingMotionMatchingDatabase = RunLightLandingDatabase;
	AnimInstance->RunHeavyLandingMotionMatchingDatabase = RunHeavyLandingDatabase;
	AnimInstance->GroundMotionMatchingDatabaseSets.Idle[0] = NewObject<UPoseSearchDatabase>();
	AnimInstance->GroundMotionMatchingDatabaseSets.Walk[0] = NewObject<UPoseSearchDatabase>();
	AnimInstance->GroundMotionMatchingDatabaseSets.Walk[1] = NewObject<UPoseSearchDatabase>();
	AnimInstance->GroundMotionMatchingDatabaseSets.Run[0] = NewObject<UPoseSearchDatabase>();
	AnimInstance->GroundMotionMatchingDatabaseSets.Run[1] = NewObject<UPoseSearchDatabase>();
	AnimInstance->GroundMotionMatchingDatabaseSets.Run[2] = NewObject<UPoseSearchDatabase>();
	AnimInstance->GroundMotionMatchingDatabaseSets.Run[3] = NewObject<UPoseSearchDatabase>();
	AnimInstance->GroundMotionMatchingDatabaseSets.Sprint[0] = NewObject<UPoseSearchDatabase>();
	AnimInstance->GroundMotionMatchingDatabaseSets.Sprint[1] = NewObject<UPoseSearchDatabase>();
	AnimInstance->CrouchingMotionMatchingDatabase = NewObject<UPoseSearchDatabase>();
	AnimInstance->TurnInPlaceMotionMatchingDatabase = NewObject<UPoseSearchDatabase>();
	AnimInstance->AirborneMotionMatchingDatabases.Add(NewObject<UPoseSearchDatabase>());
	AnimInstance->InitializeGaspRuntimeConfiguration();
	if (!TestNotNull(
		TEXT("The complete unique legacy facade initializes its atomic runtime cache"),
		AnimInstance->GetMotionMatchingDatabaseForRole(
			ERpgMotionMatchingDatabaseRole::StandIdle)))
	{
		return false;
	}

	static const ERpgMotionMatchingDatabaseRole StationaryLandingRoles[] =
	{
		ERpgMotionMatchingDatabaseRole::StandLightLanding,
		ERpgMotionMatchingDatabaseRole::StandHeavyLanding,
	};
	struct FStationaryLandingRoleCase
	{
		const TCHAR* Name;
		ERpgMotionMatchingDatabaseRole LandingRole;
		ERpgMotionMatchingDatabaseRole ExpectedRole;
	};
	const FStationaryLandingRoleCase StationaryLandingRoleCases[] =
	{
		{
			TEXT("Stand Light remains Stand Light"),
			ERpgMotionMatchingDatabaseRole::StandLightLanding,
			ERpgMotionMatchingDatabaseRole::StandLightLanding,
		},
		{
			TEXT("Walk Light rebases to Stand Light"),
			ERpgMotionMatchingDatabaseRole::WalkLightLanding,
			ERpgMotionMatchingDatabaseRole::StandLightLanding,
		},
		{
			TEXT("Run Light rebases to Stand Light"),
			ERpgMotionMatchingDatabaseRole::RunLightLanding,
			ERpgMotionMatchingDatabaseRole::StandLightLanding,
		},
		{
			TEXT("Stand Heavy remains Stand Heavy"),
			ERpgMotionMatchingDatabaseRole::StandHeavyLanding,
			ERpgMotionMatchingDatabaseRole::StandHeavyLanding,
		},
		{
			TEXT("Walk Heavy rebases to Stand Heavy"),
			ERpgMotionMatchingDatabaseRole::WalkHeavyLanding,
			ERpgMotionMatchingDatabaseRole::StandHeavyLanding,
		},
		{
			TEXT("Run Heavy rebases to Stand Heavy"),
			ERpgMotionMatchingDatabaseRole::RunHeavyLanding,
			ERpgMotionMatchingDatabaseRole::StandHeavyLanding,
		},
		{
			TEXT("None cannot manufacture a stationary landing"),
			ERpgMotionMatchingDatabaseRole::None,
			ERpgMotionMatchingDatabaseRole::None,
		},
	};
	for (const FStationaryLandingRoleCase& RoleCase : StationaryLandingRoleCases)
	{
		TestEqual(
			RoleCase.Name,
			RpgLandingRuntime::ResolveStationaryRole(RoleCase.LandingRole),
			RoleCase.ExpectedRole);
	}
	const float QuietNaN = std::numeric_limits<float>::quiet_NaN();
	const float Infinity = std::numeric_limits<float>::infinity();
	for (const ERpgMotionMatchingDatabaseRole LandingRole : StationaryLandingRoles)
	{
		const FName RoleTag = URpgAnimInstance::GetMotionMatchingDatabaseRoleTag(LandingRole);
		TestFalse(
			*FString::Printf(TEXT("%s stays active at the inclusive 3 cm/s Idle boundary"), *RoleTag.ToString()),
			RpgLandingRuntime::ShouldReleaseStationary(LandingRole, false, 3.0f));
		TestTrue(
			*FString::Printf(TEXT("%s releases above the Idle boundary"), *RoleTag.ToString()),
			RpgLandingRuntime::ShouldReleaseStationary(LandingRole, false, 3.01f));
		TestFalse(
			*FString::Printf(TEXT("%s ignores raw intent until horizontal chooser movement begins"), *RoleTag.ToString()),
			RpgLandingRuntime::ShouldReleaseStationary(LandingRole, false, 0.0f));
		TestTrue(
			*FString::Printf(TEXT("%s releases once the horizontal chooser is Moving"), *RoleTag.ToString()),
			RpgLandingRuntime::ShouldReleaseStationary(LandingRole, true, 0.0f));
		TestTrue(
			*FString::Printf(TEXT("%s releases for a non-finite NaN speed"), *RoleTag.ToString()),
			RpgLandingRuntime::ShouldReleaseStationary(LandingRole, false, QuietNaN));
		TestTrue(
			*FString::Printf(TEXT("%s releases for a non-finite infinite speed"), *RoleTag.ToString()),
			RpgLandingRuntime::ShouldReleaseStationary(LandingRole, false, Infinity));
	}

	static const ERpgMotionMatchingDatabaseRole MovingLandingRoles[] =
	{
		ERpgMotionMatchingDatabaseRole::WalkLightLanding,
		ERpgMotionMatchingDatabaseRole::WalkHeavyLanding,
		ERpgMotionMatchingDatabaseRole::RunLightLanding,
		ERpgMotionMatchingDatabaseRole::RunHeavyLanding,
	};
	for (const ERpgMotionMatchingDatabaseRole LandingRole : MovingLandingRoles)
	{
		const FName RoleTag = URpgAnimInstance::GetMotionMatchingDatabaseRoleTag(LandingRole);
		TestFalse(
			*FString::Printf(TEXT("%s preserves its authored moving landing under live input"), *RoleTag.ToString()),
			RpgLandingRuntime::ShouldReleaseStationary(LandingRole, true, 450.0f));
		TestEqual(
			*FString::Printf(TEXT("%s cannot manufacture another moving landing handoff"), *RoleTag.ToString()),
			RpgLandingRuntime::ResolveStationaryMovementRole(
				LandingRole,
				ERpgLocomotionGait::Run),
			ERpgMotionMatchingDatabaseRole::None);
	}

	struct FStationaryLandingMovementRoleCase
	{
		const TCHAR* Name;
		ERpgMotionMatchingDatabaseRole LandingRole;
		ERpgLocomotionGait LiveGait;
		ERpgMotionMatchingDatabaseRole ExpectedRole;
	};
	const FStationaryLandingMovementRoleCase StationaryLandingMovementRoleCases[] =
	{
		{
			TEXT("Stand Light to Walk Light"),
			ERpgMotionMatchingDatabaseRole::StandLightLanding,
			ERpgLocomotionGait::Walk,
			ERpgMotionMatchingDatabaseRole::WalkLightLanding,
		},
		{
			TEXT("Stand Heavy to Walk Heavy"),
			ERpgMotionMatchingDatabaseRole::StandHeavyLanding,
			ERpgLocomotionGait::Walk,
			ERpgMotionMatchingDatabaseRole::WalkHeavyLanding,
		},
		{
			TEXT("Stand Light to Run Light"),
			ERpgMotionMatchingDatabaseRole::StandLightLanding,
			ERpgLocomotionGait::Run,
			ERpgMotionMatchingDatabaseRole::RunLightLanding,
		},
		{
			TEXT("Stand Heavy to Run Heavy"),
			ERpgMotionMatchingDatabaseRole::StandHeavyLanding,
			ERpgLocomotionGait::Run,
			ERpgMotionMatchingDatabaseRole::RunHeavyLanding,
		},
		{
			TEXT("Stand Light Sprint falls back to Run Light"),
			ERpgMotionMatchingDatabaseRole::StandLightLanding,
			ERpgLocomotionGait::Sprint,
			ERpgMotionMatchingDatabaseRole::RunLightLanding,
		},
		{
			TEXT("Stand Heavy Sprint falls back to Run Heavy"),
			ERpgMotionMatchingDatabaseRole::StandHeavyLanding,
			ERpgLocomotionGait::Sprint,
			ERpgMotionMatchingDatabaseRole::RunHeavyLanding,
		},
		{
			TEXT("Stand Light Idle has no moving handoff"),
			ERpgMotionMatchingDatabaseRole::StandLightLanding,
			ERpgLocomotionGait::Idle,
			ERpgMotionMatchingDatabaseRole::None,
		},
		{
			TEXT("Stand Heavy Idle has no moving handoff"),
			ERpgMotionMatchingDatabaseRole::StandHeavyLanding,
			ERpgLocomotionGait::Idle,
			ERpgMotionMatchingDatabaseRole::None,
		},
	};
	for (const FStationaryLandingMovementRoleCase& RoleCase : StationaryLandingMovementRoleCases)
	{
		TestEqual(
			RoleCase.Name,
			RpgLandingRuntime::ResolveStationaryMovementRole(
				RoleCase.LandingRole,
				RoleCase.LiveGait),
			RoleCase.ExpectedRole);
	}

	static const ERpgMotionMatchingDatabaseRole LandingExitRoles[] =
	{
		ERpgMotionMatchingDatabaseRole::StandLightLanding,
		ERpgMotionMatchingDatabaseRole::StandHeavyLanding,
		ERpgMotionMatchingDatabaseRole::WalkLightLanding,
		ERpgMotionMatchingDatabaseRole::WalkHeavyLanding,
		ERpgMotionMatchingDatabaseRole::RunLightLanding,
		ERpgMotionMatchingDatabaseRole::RunHeavyLanding,
	};
	for (const ERpgMotionMatchingDatabaseRole LandingRole : LandingExitRoles)
	{
		const FName RoleTag = URpgAnimInstance::GetMotionMatchingDatabaseRoleTag(LandingRole);
		TestFalse(
			*FString::Printf(TEXT("An active unarmed %s remains a Continuing Pose"), *RoleTag.ToString()),
			RpgLandingRuntime::ShouldInterruptDatabaseExit(
				ERpgJumpPhase::Landing,
				false,
				LandingRole));
		TestTrue(
			*FString::Printf(TEXT("A completed %s interrupts its database exit"), *RoleTag.ToString()),
			RpgLandingRuntime::ShouldInterruptDatabaseExit(
				ERpgJumpPhase::Landing,
				true,
				LandingRole));
		TestTrue(
			*FString::Printf(TEXT("A reset %s interrupts its handoff to gait locomotion"), *RoleTag.ToString()),
			RpgLandingRuntime::ShouldInterruptDatabaseExit(
				ERpgJumpPhase::Grounded,
				false,
				LandingRole));
	}
	TestFalse(
		TEXT("A normal grounded database never manufactures a landing-exit interrupt"),
		RpgLandingRuntime::ShouldInterruptDatabaseExit(
			ERpgJumpPhase::Grounded,
			false,
			ERpgMotionMatchingDatabaseRole::StandRunLoops));

	FRpgAnimInstanceProxy Proxy;
	Proxy.bTurnInPlaceHardReset = false;
	Proxy.MovementState = ERpgLocomotionMovementState::Grounded;
	Proxy.bIsMovingOnGround = true;
	int32 LandingAirborneEpoch = 0;
	auto SetValidLandingSelectionSnapshot =
		[&Proxy, &LandingAirborneEpoch](
			ERpgLocomotionGait Gait,
			float HorizontalSpeed,
			float DownwardSpeed,
			bool bHasMoveIntent)
	{
		FRpgLandingSelectionSnapshot& Snapshot = Proxy.LandingSelectionSnapshot;
		Snapshot = FRpgLandingSelectionSnapshot();
		Snapshot.HorizontalVelocity = FVector(HorizontalSpeed, 0.0f, 0.0f);
		Snapshot.HorizontalSpeed = HorizontalSpeed;
		Snapshot.VerticalVelocity = -DownwardSpeed;
		Snapshot.MaximumDownwardSpeed = DownwardSpeed;
		Snapshot.PredictedImpactDownwardSpeed = DownwardSpeed;
		Snapshot.Gait = Gait;
		Snapshot.PredictedLanding.LandingLocation = FVector(HorizontalSpeed * 0.1f, 0.0f, 0.0f);
		Snapshot.PredictedLanding.LandingNormal = FVector::UpVector;
		Snapshot.PredictedLanding.TimeToLand = 0.1f;
		Snapshot.PredictedLanding.bIsValid = true;
		Snapshot.AirborneEpoch = ++LandingAirborneEpoch;
		Snapshot.bHasMoveIntent = bHasMoveIntent;
		Snapshot.bIsValid = true;
	};
	AnimInstance->ResetJumpPhaseRuntime();
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(TEXT("A regular grounded snapshot stays grounded"), AnimInstance->JumpPhase, ERpgJumpPhase::Grounded);

	Proxy.MovementState = ERpgLocomotionMovementState::Airborne;
	Proxy.bIsMovingOnGround = false;
	Proxy.bIsFalling = true;
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(TEXT("Falling enters the explicit airborne phase"), AnimInstance->JumpPhase, ERpgJumpPhase::Airborne);
	TestFalse(TEXT("Airborne never owns a landing selection"), AnimInstance->bLandingSelectionLatched);

	Proxy.MovementState = ERpgLocomotionMovementState::Grounded;
	Proxy.bIsMovingOnGround = true;
	Proxy.bIsFalling = false;
	Proxy.WorldVelocity = FVector(300.0f, 0.0f, 0.0f);
	Proxy.WorldAcceleration = FVector(2400.0f, 0.0f, 0.0f);
	Proxy.GroundSpeed = 300.0f;
	Proxy.Gait = ERpgLocomotionGait::Run;
	Proxy.bHasGroundedMoveIntent = true;
	SetValidLandingSelectionSnapshot(ERpgLocomotionGait::Run, 300.0f, 500.0f, true);
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(TEXT("A moving touchdown enters the landing lifecycle"), AnimInstance->JumpPhase, ERpgJumpPhase::Landing);
	TestEqual(
		TEXT("A moving light touchdown freezes the Run Light database role"),
		AnimInstance->ActiveLandingDatabaseRole,
		ERpgMotionMatchingDatabaseRole::RunLightLanding);
	const uint32 RunLandingRequest = AnimInstance->LandingRequestSerial;
	TestTrue(TEXT("A moving touchdown creates a serialized landing request"), RunLandingRequest != 0u);
	TestFalse(
		TEXT("A moving landing cannot latch a result from another requested role"),
		AnimInstance->TryLatchLandingSelection(
			LandingClip,
			WalkLightLandingDatabase,
			0.2f,
			false,
			RunLandingRequest));
	TestTrue(
		TEXT("A moving landing latches only its exact requested Run Light role"),
		AnimInstance->TryLatchLandingSelection(
			LandingClip,
			RunLightLandingDatabase,
			0.2f,
			false,
			RunLandingRequest));
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(
		TEXT("A Run landing remains active under its normal grounded intent and speed"),
		AnimInstance->JumpPhase,
		ERpgJumpPhase::Landing);
	TestTrue(
		TEXT("Live moving input does not clear the Run landing latch"),
		AnimInstance->bLandingSelectionLatched);
	TestEqual(
		TEXT("Live moving input does not create a second Run landing request"),
		AnimInstance->LandingRequestSerial,
		RunLandingRequest);

	// The final airborne snapshot is consumed only at touchdown. Later grounded input and gait
	// changes must not reclassify an already running cosmetic landing request.
	Proxy.WorldVelocity = FVector(100.0f, 0.0f, 0.0f);
	Proxy.WorldAcceleration = FVector(1200.0f, 0.0f, 0.0f);
	Proxy.GroundSpeed = 100.0f;
	Proxy.Gait = ERpgLocomotionGait::Walk;
	Proxy.bHasGroundedMoveIntent = true;
	Proxy.LandingSelectionSnapshot = FRpgLandingSelectionSnapshot();
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(TEXT("A running landing survives live Walk chooser movement"), AnimInstance->JumpPhase, ERpgJumpPhase::Landing);
	TestEqual(
		TEXT("A moving landing role remains frozen after live gait and speed changes"),
		AnimInstance->ActiveLandingDatabaseRole,
		ERpgMotionMatchingDatabaseRole::RunLightLanding);
	TestTrue(
		TEXT("The exact moving landing asset remains active after live gait changes"),
		AnimInstance->IsActiveLandingAsset(LandingClip));
	TestEqual(TEXT("Grounded-input changes do not create another request"), AnimInstance->LandingRequestSerial, RunLandingRequest);
	AnimInstance->ResetJumpPhaseRuntime();
	TestEqual(
		TEXT("Resetting the landing lifecycle clears its frozen database role"),
		AnimInstance->ActiveLandingDatabaseRole,
		ERpgMotionMatchingDatabaseRole::None);

	struct FStationaryLandingCase
	{
		const TCHAR* Name;
		ERpgMotionMatchingDatabaseRole Role;
		UPoseSearchDatabase* Database;
		ERpgMotionMatchingDatabaseRole WalkRole;
		UPoseSearchDatabase* WalkDatabase;
		ERpgMotionMatchingDatabaseRole RunRole;
		UPoseSearchDatabase* RunDatabase;
		float DownwardSpeed;
	};
	const FStationaryLandingCase StationaryLandingCases[] =
	{
		{
			TEXT("Stand Light"),
			ERpgMotionMatchingDatabaseRole::StandLightLanding,
			StandLightLandingDatabase,
			ERpgMotionMatchingDatabaseRole::WalkLightLanding,
			WalkLightLandingDatabase,
			ERpgMotionMatchingDatabaseRole::RunLightLanding,
			RunLightLandingDatabase,
			500.0f,
		},
		{
			TEXT("Stand Heavy"),
			ERpgMotionMatchingDatabaseRole::StandHeavyLanding,
			StandHeavyLandingDatabase,
			ERpgMotionMatchingDatabaseRole::WalkHeavyLanding,
			WalkHeavyLandingDatabase,
			ERpgMotionMatchingDatabaseRole::RunHeavyLanding,
			RunHeavyLandingDatabase,
			700.0f,
		},
	};
	auto BeginStationaryAirbornePhase = [&]()
	{
		AnimInstance->ResetJumpPhaseRuntime();
		Proxy.bIsAnyMontagePlaying = false;
		Proxy.bIsCrouching = false;
		Proxy.bHasTurnInPlaceBlockingGameplayTag = false;
		Proxy.MovementState = ERpgLocomotionMovementState::Airborne;
		Proxy.bIsMovingOnGround = false;
		Proxy.bIsFalling = true;
		Proxy.WorldVelocity = FVector::ZeroVector;
		Proxy.WorldAcceleration = FVector::ZeroVector;
		Proxy.GroundSpeed = 0.0f;
		Proxy.Gait = ERpgLocomotionGait::Idle;
		Proxy.Stance = ERpgLocomotionStance::Standing;
		Proxy.bHasGroundedMoveIntent = false;
		Proxy.LandingSelectionSnapshot = FRpgLandingSelectionSnapshot();
		AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
		TestEqual(
			TEXT("The stationary regression fixture enters Airborne"),
			AnimInstance->JumpPhase,
			ERpgJumpPhase::Airborne);
	};
	auto TouchDownStationary = [&](const FStationaryLandingCase& LandingCase)
	{
		SetValidLandingSelectionSnapshot(
			ERpgLocomotionGait::Idle,
			0.0f,
			LandingCase.DownwardSpeed,
			false);
		Proxy.MovementState = ERpgLocomotionMovementState::Grounded;
		Proxy.bIsMovingOnGround = true;
		Proxy.bIsFalling = false;
		Proxy.WorldVelocity = FVector::ZeroVector;
		Proxy.WorldAcceleration = FVector::ZeroVector;
		Proxy.GroundSpeed = 0.0f;
		Proxy.Gait = ERpgLocomotionGait::Idle;
		Proxy.bHasGroundedMoveIntent = false;
	};
	auto EnterLatchedStationaryLanding = [&](
		const FStationaryLandingCase& LandingCase,
		bool bRawVerticalIntent,
		ERpgLocomotionGait LiveGait)
	{
		BeginStationaryAirbornePhase();
		TouchDownStationary(LandingCase);
		if (bRawVerticalIntent)
		{
			Proxy.WorldVelocity = FVector(0.0f, 0.0f, -50.0f);
			Proxy.WorldAcceleration = LiveGait == ERpgLocomotionGait::Walk
				? FVector(1200.0f, 0.0f, 0.0f)
				: FVector(2400.0f, 0.0f, 0.0f);
			Proxy.Gait = LiveGait;
			Proxy.bHasGroundedMoveIntent = true;
		}
		const uint32 RequestSerialBeforeTouchdown = AnimInstance->LandingRequestSerial;
		AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
		TestEqual(
			*FString::Printf(
				TEXT("%s %s"),
				LandingCase.Name,
				bRawVerticalIntent
					? TEXT("vertical-only raw intent keeps the Landing lifecycle")
					: TEXT("touchdown enters the Landing lifecycle")),
			AnimInstance->JumpPhase,
			ERpgJumpPhase::Landing);
		TestEqual(
			*FString::Printf(TEXT("%s touchdown selects its exact role"), LandingCase.Name),
			AnimInstance->ActiveLandingDatabaseRole,
			LandingCase.Role);
		const uint32 RequestSerial = AnimInstance->LandingRequestSerial;
		TestEqual(
			*FString::Printf(TEXT("%s touchdown creates exactly one request"), LandingCase.Name),
			RequestSerial,
			RequestSerialBeforeTouchdown + 1u);
		TestTrue(
			*FString::Printf(TEXT("%s touchdown latches its exact database"), LandingCase.Name),
			AnimInstance->TryLatchLandingSelection(
				LandingClip,
				LandingCase.Database,
				0.2f,
				false,
				RequestSerial));
		AnimInstance->CurrentMotionMatchingDatabaseRole = LandingCase.Role;
		return RequestSerial;
	};

	struct FStationaryLandingHandoffGaitCase
	{
		const TCHAR* Name;
		ERpgLocomotionGait Gait;
	};
	const FStationaryLandingHandoffGaitCase HandoffGaitCases[] =
	{
		{TEXT("Walk"), ERpgLocomotionGait::Walk},
		{TEXT("Run"), ERpgLocomotionGait::Run},
	};

	// Raw input and a vertical-only touchdown remainder keep the Stand request. Horizontal chooser
	// movement inside GASP's landing window performs one severity-preserving landing-database handoff.
	for (const FStationaryLandingCase& LandingCase : StationaryLandingCases)
	{
		for (const FStationaryLandingHandoffGaitCase& GaitCase : HandoffGaitCases)
		{
			const uint32 StandRequestSerial = EnterLatchedStationaryLanding(
				LandingCase,
				true,
				GaitCase.Gait);
			TestTrue(
				*FString::Printf(TEXT("Initial %s owns its one ForceInterrupt"), LandingCase.Name),
				AnimInstance->ConsumeLandingForceInterruptRequest());
			TestFalse(
				*FString::Printf(TEXT("Initial %s ForceInterrupt is one-shot"), LandingCase.Name),
				AnimInstance->ConsumeLandingForceInterruptRequest());

			const ERpgMotionMatchingDatabaseRole ExpectedHandoffRole =
				GaitCase.Gait == ERpgLocomotionGait::Walk
					? LandingCase.WalkRole
					: LandingCase.RunRole;
			UPoseSearchDatabase* ExpectedHandoffDatabase =
				GaitCase.Gait == ERpgLocomotionGait::Walk
					? LandingCase.WalkDatabase
					: LandingCase.RunDatabase;
			Proxy.WorldVelocity = FVector(
				DefaultTuning.ChooserVelocityTolerance + 0.01f,
				0.0f,
				0.0f);
			Proxy.GroundSpeed = Proxy.WorldVelocity.Size2D();
			AnimInstance->UpdateJumpPhaseRuntime(0.29f, Proxy);

			const uint32 HandoffRequestSerial = AnimInstance->LandingRequestSerial;
			TestEqual(
				*FString::Printf(TEXT("%s %s handoff remains in Landing"), LandingCase.Name, GaitCase.Name),
				AnimInstance->JumpPhase,
				ERpgJumpPhase::Landing);
			TestEqual(
				*FString::Printf(TEXT("%s maps to severity-preserving %s landing"), LandingCase.Name, GaitCase.Name),
				AnimInstance->ActiveLandingDatabaseRole,
				ExpectedHandoffRole);
			TestEqual(
				*FString::Printf(TEXT("%s %s handoff advances the serial once"), LandingCase.Name, GaitCase.Name),
				HandoffRequestSerial,
				StandRequestSerial + 1u);
			TestFalse(
				*FString::Printf(TEXT("%s %s handoff clears the old latch"), LandingCase.Name, GaitCase.Name),
				AnimInstance->bLandingSelectionLatched);
			TestNull(
				*FString::Printf(TEXT("%s %s handoff clears the old selected asset"), LandingCase.Name, GaitCase.Name),
				AnimInstance->LandingSelectedAsset.Get());
			TestEqual(
				*FString::Printf(TEXT("%s %s handoff clears the old selected serial"), LandingCase.Name, GaitCase.Name),
				AnimInstance->LandingSelectedRequestSerial,
				0u);
			TestFalse(
				*FString::Printf(TEXT("%s %s handoff uses database-change rather than ForceInterrupt"), LandingCase.Name, GaitCase.Name),
				AnimInstance->ConsumeLandingForceInterruptRequest());
			TestFalse(
				*FString::Printf(TEXT("%s %s handoff rejects the old request serial"), LandingCase.Name, GaitCase.Name),
				AnimInstance->TryLatchLandingSelection(
					LandingClip,
					LandingCase.Database,
					0.2f,
					false,
					StandRequestSerial));
			TestFalse(
				*FString::Printf(TEXT("%s %s handoff rejects the old database"), LandingCase.Name, GaitCase.Name),
				AnimInstance->TryLatchLandingSelection(
					LandingClip,
					LandingCase.Database,
					0.2f,
					false,
					HandoffRequestSerial));
			TestTrue(
				*FString::Printf(TEXT("%s %s handoff latches its exact new database"), LandingCase.Name, GaitCase.Name),
				AnimInstance->TryLatchLandingSelection(
					LandingClip,
					ExpectedHandoffDatabase,
					0.2f,
					false,
					HandoffRequestSerial));

			Proxy.WorldVelocity = FVector::ZeroVector;
			Proxy.WorldAcceleration = FVector::ZeroVector;
			Proxy.GroundSpeed = 0.0f;
			Proxy.Gait = ERpgLocomotionGait::Idle;
			Proxy.bHasGroundedMoveIntent = false;
			AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
			TestEqual(
				*FString::Printf(TEXT("%s %s moving landing stays frozen after input ends"), LandingCase.Name, GaitCase.Name),
				AnimInstance->ActiveLandingDatabaseRole,
				ExpectedHandoffRole);
			TestEqual(
				*FString::Printf(TEXT("%s %s frozen moving landing creates no third request"), LandingCase.Name, GaitCase.Name),
				AnimInstance->LandingRequestSerial,
				HandoffRequestSerial);
		}
	}

	// Reproduce the spawn/late-join sequence where W is already held in air, the frozen snapshot
	// still has momentum just outside the Idle band, but physical touchdown has zero horizontal
	// speed. Live CMC state rebases the initial role before motion begins on a later frame.
	constexpr double HeldAirborneMaximumAcceleration = 2400.0;
	constexpr float HeldAirborneFrozenHorizontalSpeed = 4.0f;
	const FVector AuthorityHeldAcceleration(HeldAirborneMaximumAcceleration, 0.0, 0.0);
	FRpgReplicatedAcceleration PackedHeldAcceleration;
	PackedHeldAcceleration.SetFromAcceleration(
		AuthorityHeldAcceleration,
		HeldAirborneMaximumAcceleration);
	const FVector SimulatedHeldAcceleration = PackedHeldAcceleration.ToAcceleration(
		HeldAirborneMaximumAcceleration);
	struct FHeldAirborneNetworkView
	{
		const TCHAR* Name;
		FVector Acceleration;
	};
	const FHeldAirborneNetworkView HeldAirborneNetworkViews[] =
	{
		{TEXT("Authority"), AuthorityHeldAcceleration},
		{TEXT("Simulated Proxy"), SimulatedHeldAcceleration},
		{TEXT("Late Join Simulated Proxy"), SimulatedHeldAcceleration},
	};
	const FVector GravityAcceleration(0.0, 0.0, -1000.0);

	for (const FStationaryLandingCase& LandingCase : StationaryLandingCases)
	{
		for (const FHeldAirborneNetworkView& NetworkView : HeldAirborneNetworkViews)
		{
			BeginStationaryAirbornePhase();
			Proxy.LastGroundedGait = ERpgLocomotionGait::Idle;
			Proxy.LandingAirborneEpoch = 0;
			Proxy.bWasAirborneForLanding = false;
			Proxy.WorldVelocity = FVector(
				HeldAirborneFrozenHorizontalSpeed,
				0.0f,
				-LandingCase.DownwardSpeed);
			Proxy.WorldAcceleration = NetworkView.Acceleration;
			Proxy.VerticalVelocity = -LandingCase.DownwardSpeed;
			Proxy.GroundSpeed = HeldAirborneFrozenHorizontalSpeed;
			Proxy.Gait = ERpgLocomotionGait::Idle;
			Proxy.bHasGroundedMoveIntent = false;
			Proxy.TrajectoryLandingPrediction = FRpgTrajectoryLandingPrediction();
			const float HeldInputMagnitude = static_cast<float>(
				NetworkView.Acceleration.Size2D() / HeldAirborneMaximumAcceleration);
			URpgAnimInstance::UpdateLandingSelectionSnapshot(
				Proxy,
				HeldInputMagnitude,
				GravityAcceleration);
			AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
			TestTrue(
				*FString::Printf(TEXT("%s %s airborne W capture retains raw intent"), NetworkView.Name, LandingCase.Name),
				Proxy.LandingSelectionSnapshot.bHasMoveIntent);
			TestEqual(
				*FString::Printf(TEXT("%s %s airborne W capture retains Run context"), NetworkView.Name, LandingCase.Name),
				Proxy.LandingSelectionSnapshot.Gait,
				ERpgLocomotionGait::Run);
			TestTrue(
				*FString::Printf(TEXT("%s %s airborne W capture retains frozen XY momentum"), NetworkView.Name, LandingCase.Name),
				FMath::IsNearlyEqual(
					Proxy.LandingSelectionSnapshot.HorizontalSpeed,
					HeldAirborneFrozenHorizontalSpeed));
			TestEqual(
				*FString::Printf(TEXT("%s %s frozen airborne role is Run before live touchdown normalization"), NetworkView.Name, LandingCase.Name),
				RpgLandingRuntime::ResolveDatabaseRole(
					Proxy.LandingSelectionSnapshot,
					DefaultTuning),
				LandingCase.RunRole);

			const uint32 RequestSerialBeforeTouchdown = AnimInstance->LandingRequestSerial;
			Proxy.MovementState = ERpgLocomotionMovementState::Grounded;
			Proxy.bIsMovingOnGround = true;
			Proxy.bIsFalling = false;
			Proxy.WorldVelocity = FVector(0.0f, 0.0f, -50.0f);
			Proxy.WorldAcceleration = NetworkView.Acceleration;
			Proxy.VerticalVelocity = -50.0f;
			Proxy.GroundSpeed = 0.0f;
			Proxy.Gait = ERpgLocomotionGait::Run;
			Proxy.bHasGroundedMoveIntent = true;
			URpgAnimInstance::UpdateLandingSelectionSnapshot(
				Proxy,
				HeldInputMagnitude,
				GravityAcceleration);
			TestTrue(
				*FString::Printf(TEXT("%s %s touchdown still owns the frozen airborne speed"), NetworkView.Name, LandingCase.Name),
				FMath::IsNearlyEqual(
					Proxy.LandingSelectionSnapshot.HorizontalSpeed,
					HeldAirborneFrozenHorizontalSpeed));
			AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
			const uint32 StandRequestSerial = AnimInstance->LandingRequestSerial;
			TestEqual(
				*FString::Printf(TEXT("%s %s zero-XY touchdown enters Landing"), NetworkView.Name, LandingCase.Name),
				AnimInstance->JumpPhase,
				ERpgJumpPhase::Landing);
			TestEqual(
				*FString::Printf(TEXT("%s %s zero-XY touchdown selects Stand"), NetworkView.Name, LandingCase.Name),
				AnimInstance->ActiveLandingDatabaseRole,
				LandingCase.Role);
			TestEqual(
				*FString::Printf(TEXT("%s %s zero-XY touchdown creates one Stand request"), NetworkView.Name, LandingCase.Name),
				StandRequestSerial,
				RequestSerialBeforeTouchdown + 1u);
			TestTrue(
				*FString::Printf(TEXT("%s %s zero-XY touchdown owns one ForceInterrupt"), NetworkView.Name, LandingCase.Name),
				AnimInstance->ConsumeLandingForceInterruptRequest());
			TestFalse(
				*FString::Printf(TEXT("%s %s zero-XY touchdown ForceInterrupt is one-shot"), NetworkView.Name, LandingCase.Name),
				AnimInstance->ConsumeLandingForceInterruptRequest());
			TestTrue(
				*FString::Printf(TEXT("%s %s zero-XY touchdown latches Stand"), NetworkView.Name, LandingCase.Name),
				AnimInstance->TryLatchLandingSelection(
					LandingClip,
					LandingCase.Database,
					0.2f,
					false,
					StandRequestSerial));

			URpgAnimInstance::UpdateLandingSelectionSnapshot(
				Proxy,
				HeldInputMagnitude,
				GravityAcceleration);
			AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
			TestEqual(
				*FString::Printf(TEXT("%s %s vertical touchdown remainder keeps Stand"), NetworkView.Name, LandingCase.Name),
				AnimInstance->ActiveLandingDatabaseRole,
				LandingCase.Role);
			TestEqual(
				*FString::Printf(TEXT("%s %s vertical touchdown remainder creates no request"), NetworkView.Name, LandingCase.Name),
				AnimInstance->LandingRequestSerial,
				StandRequestSerial);

			Proxy.WorldVelocity = FVector(
				DefaultTuning.ChooserVelocityTolerance + 0.01f,
				0.0f,
				0.0f);
			Proxy.VerticalVelocity = 0.0f;
			Proxy.GroundSpeed = Proxy.WorldVelocity.Size2D();
			URpgAnimInstance::UpdateLandingSelectionSnapshot(
				Proxy,
				HeldInputMagnitude,
				GravityAcceleration);
			AnimInstance->UpdateJumpPhaseRuntime(0.1f, Proxy);
			const uint32 RunRequestSerial = AnimInstance->LandingRequestSerial;
			TestEqual(
				*FString::Printf(TEXT("%s %s first XY frame stays in Landing"), NetworkView.Name, LandingCase.Name),
				AnimInstance->JumpPhase,
				ERpgJumpPhase::Landing);
			TestEqual(
				*FString::Printf(TEXT("%s %s first XY frame preserves severity in Run"), NetworkView.Name, LandingCase.Name),
				AnimInstance->ActiveLandingDatabaseRole,
				LandingCase.RunRole);
			TestEqual(
				*FString::Printf(TEXT("%s %s first XY frame creates exactly one Run handoff"), NetworkView.Name, LandingCase.Name),
				RunRequestSerial,
				StandRequestSerial + 1u);
			TestFalse(
				*FString::Printf(TEXT("%s %s Run handoff uses database-change interruption"), NetworkView.Name, LandingCase.Name),
				AnimInstance->ConsumeLandingForceInterruptRequest());
			TestTrue(
				*FString::Printf(TEXT("%s %s Run handoff latches its exact database"), NetworkView.Name, LandingCase.Name),
				AnimInstance->TryLatchLandingSelection(
					LandingClip,
					LandingCase.RunDatabase,
					0.2f,
					false,
					RunRequestSerial));

			AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
			TestEqual(
				*FString::Printf(TEXT("%s %s repeated XY frame creates no third request"), NetworkView.Name, LandingCase.Name),
				AnimInstance->LandingRequestSerial,
				RunRequestSerial);
		}
	}

	// Normalize severity before database fallback: unavailable Stand Heavy must use Stand Light,
	// never an available moving Heavy database retained from the frozen airborne snapshot.
	BeginStationaryAirbornePhase();
	SetValidLandingSelectionSnapshot(
		ERpgLocomotionGait::Run,
		HeldAirborneFrozenHorizontalSpeed,
		700.0f,
		true);
	TestEqual(
		TEXT("Missing-heavy fallback fixture starts from frozen Run Heavy"),
		RpgLandingRuntime::ResolveDatabaseRole(
			Proxy.LandingSelectionSnapshot,
			DefaultTuning),
		ERpgMotionMatchingDatabaseRole::RunHeavyLanding);
	FRpgLandingEligibilitySnapshot MissingHeavyEligibility;
	MissingHeavyEligibility.MovementState = ERpgLocomotionMovementState::Grounded;
	MissingHeavyEligibility.bIsMovingOnGround = true;
	FRpgLandingDatabaseAvailability MissingHeavyAvailability;
	MissingHeavyAvailability.bStandLight = true;
	MissingHeavyAvailability.bStandHeavy = false;
	MissingHeavyAvailability.bWalkLight = true;
	MissingHeavyAvailability.bWalkHeavy = true;
	MissingHeavyAvailability.bRunLight = true;
	MissingHeavyAvailability.bRunHeavy = true;
	const ERpgMotionMatchingDatabaseRole MissingHeavyRole =
		RpgLandingRuntime::ResolveTouchdownRole(
			Proxy.LandingSelectionSnapshot,
			MissingHeavyEligibility,
			MissingHeavyAvailability,
			ERpgLocomotionGait::Run,
			0.0f,
			false,
			DefaultTuning);
	TestEqual(
		TEXT("Stationary normalization precedes missing Heavy fallback"),
		MissingHeavyRole,
		ERpgMotionMatchingDatabaseRole::StandLightLanding);
	const FRpgLandingRuntimeState MissingHeavyState = AnimInstance->CaptureLandingRuntimeState();
	const FRpgLandingRuntimeResult MissingHeavyResult = RpgLandingRuntime::BeginRequest(
		MissingHeavyState,
		MissingHeavyRole,
		true,
		DefaultTuning);
	TestEqual(
		TEXT("Missing stationary Heavy creates exactly one Light fallback request"),
		MissingHeavyResult.State.RequestSerial,
		MissingHeavyState.RequestSerial + 1u);

	// Horizontal movement after the source 0.3 second window exits to normal gait locomotion.
	for (const FStationaryLandingCase& LandingCase : StationaryLandingCases)
	{
		const uint32 RequestSerial = EnterLatchedStationaryLanding(
			LandingCase,
			true,
			ERpgLocomotionGait::Run);
		TestTrue(
			*FString::Printf(TEXT("Window-expiry %s consumes its initial ForceInterrupt"), LandingCase.Name),
			AnimInstance->ConsumeLandingForceInterruptRequest());
		AnimInstance->UpdateLandingLatchedPlayback(
			LandingClip,
			0.1f,
			1.0f,
			1.0f,
			0.01f);
		AnimInstance->UpdateJumpPhaseRuntime(0.301f, Proxy);
		TestEqual(
			*FString::Printf(TEXT("%s remains active while the handoff window expires"), LandingCase.Name),
			AnimInstance->JumpPhase,
			ERpgJumpPhase::Landing);
		TestTrue(
			*FString::Printf(TEXT("%s handoff window is now expired"), LandingCase.Name),
			AnimInstance->LandingTouchdownElapsed > 0.3f);

		Proxy.WorldVelocity = FVector(
			DefaultTuning.ChooserVelocityTolerance + 0.01f,
			0.0f,
			0.0f);
		Proxy.GroundSpeed = Proxy.WorldVelocity.Size2D();
		AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
		TestEqual(
			*FString::Printf(TEXT("Expired %s returns to normal grounded locomotion"), LandingCase.Name),
			AnimInstance->JumpPhase,
			ERpgJumpPhase::Grounded);
		TestEqual(
			*FString::Printf(TEXT("Expired %s creates no handoff request"), LandingCase.Name),
			AnimInstance->LandingRequestSerial,
			RequestSerial);
		TestFalse(
			*FString::Printf(TEXT("Expired %s clears its landing latch"), LandingCase.Name),
			AnimInstance->bLandingSelectionLatched);
		TestEqual(
			*FString::Printf(TEXT("Expired %s clears its landing role"), LandingCase.Name),
			AnimInstance->ActiveLandingDatabaseRole,
			ERpgMotionMatchingDatabaseRole::None);
	}

	// The live speed gate preserves the inclusive Idle boundary and releases immediately above it.
	for (const FStationaryLandingCase& LandingCase : StationaryLandingCases)
	{
		const uint32 RequestSerial = EnterLatchedStationaryLanding(
			LandingCase,
			false,
			ERpgLocomotionGait::Idle);
		TestTrue(
			*FString::Printf(TEXT("Speed-only %s consumes its initial ForceInterrupt"), LandingCase.Name),
			AnimInstance->ConsumeLandingForceInterruptRequest());
		Proxy.WorldVelocity = FVector::ZeroVector;
		Proxy.WorldAcceleration = FVector::ZeroVector;
		Proxy.bHasGroundedMoveIntent = false;
		Proxy.GroundSpeed = 3.0f;
		AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
		TestEqual(
			*FString::Printf(TEXT("Active %s remains at exactly 3 cm/s"), LandingCase.Name),
			AnimInstance->JumpPhase,
			ERpgJumpPhase::Landing);
		TestTrue(
			*FString::Printf(TEXT("Active %s keeps its latch at exactly 3 cm/s"), LandingCase.Name),
			AnimInstance->bLandingSelectionLatched);
		TestEqual(
			*FString::Printf(TEXT("Active %s keeps its selected serial at exactly 3 cm/s"), LandingCase.Name),
			AnimInstance->LandingSelectedRequestSerial,
			RequestSerial);

		Proxy.GroundSpeed = 3.01f;
		AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
		TestEqual(
			*FString::Printf(TEXT("Speed-only mismatched %s exits above 3 cm/s"), LandingCase.Name),
			AnimInstance->JumpPhase,
			ERpgJumpPhase::Grounded);
		TestFalse(
			*FString::Printf(TEXT("Speed-released %s clears its latch"), LandingCase.Name),
			AnimInstance->bLandingSelectionLatched);
		TestEqual(
			*FString::Printf(TEXT("Speed-released %s preserves its request serial"), LandingCase.Name),
			AnimInstance->LandingRequestSerial,
			RequestSerial);
		TestEqual(
			*FString::Printf(TEXT("Speed-released %s returns to normal locomotion role selection"), LandingCase.Name),
			AnimInstance->ActiveLandingDatabaseRole,
			ERpgMotionMatchingDatabaseRole::None);
		TestEqual(
			*FString::Printf(TEXT("Speed-released %s clears its selected serial"), LandingCase.Name),
			AnimInstance->LandingSelectedRequestSerial,
			0u);
		TestTrue(
			*FString::Printf(TEXT("Speed-released %s interrupts its landing-database handoff"), LandingCase.Name),
			RpgLandingRuntime::ShouldInterruptDatabaseExit(
				AnimInstance->JumpPhase,
				AnimInstance->bLandingCompletionArmed,
				AnimInstance->CurrentMotionMatchingDatabaseRole));
	}
	AnimInstance->CurrentMotionMatchingDatabaseRole = ERpgMotionMatchingDatabaseRole::None;
	AnimInstance->ResetJumpPhaseRuntime();
	Proxy.bHasGroundedMoveIntent = false;
	Proxy.WorldVelocity = FVector::ZeroVector;
	Proxy.WorldAcceleration = FVector::ZeroVector;
	Proxy.GroundSpeed = 0.0f;

	const uint32 IdleLandingRequestBeforeTouchdown = AnimInstance->LandingRequestSerial;
	Proxy.MovementState = ERpgLocomotionMovementState::Airborne;
	Proxy.bIsMovingOnGround = false;
	Proxy.bIsFalling = true;
	Proxy.bHasGroundedMoveIntent = false;
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	Proxy.MovementState = ERpgLocomotionMovementState::Grounded;
	Proxy.bIsMovingOnGround = true;
	Proxy.bIsFalling = false;
	Proxy.GroundSpeed = 3.0f;
	Proxy.Gait = ERpgLocomotionGait::Idle;
	SetValidLandingSelectionSnapshot(ERpgLocomotionGait::Idle, 3.0f, 500.0f, false);
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(TEXT("An idle touchdown enters Landing at the inclusive 3 cm/s boundary"), AnimInstance->JumpPhase, ERpgJumpPhase::Landing);
	TestEqual(
		TEXT("The idle touchdown selects the preserved Stand Light role"),
		AnimInstance->ActiveLandingDatabaseRole,
		ERpgMotionMatchingDatabaseRole::StandLightLanding);
	TestEqual(
		TEXT("The idle touchdown creates exactly one new request"),
		AnimInstance->LandingRequestSerial,
		IdleLandingRequestBeforeTouchdown + 1u);
	TestTrue(TEXT("A touchdown requests ForceInterrupt exactly once"), AnimInstance->ConsumeLandingForceInterruptRequest());
	TestFalse(TEXT("The same touchdown cannot request ForceInterrupt twice"), AnimInstance->ConsumeLandingForceInterruptRequest());

	TestFalse(
		TEXT("A result from another database cannot latch"),
		AnimInstance->TryLatchLandingSelection(
			LandingClip,
			RunLightLandingDatabase,
			0.2f,
			false,
			AnimInstance->LandingRequestSerial));
	TestFalse(
		TEXT("A stale request serial cannot latch"),
		AnimInstance->TryLatchLandingSelection(
			LandingClip,
			StandLightLandingDatabase,
			0.2f,
			false,
			AnimInstance->LandingRequestSerial - 1));
	TestTrue(
		TEXT("The first valid exclusive landing result latches"),
		AnimInstance->TryLatchLandingSelection(
			LandingClip,
			StandLightLandingDatabase,
			0.2f,
			false,
			AnimInstance->LandingRequestSerial));
	TestTrue(TEXT("The exact landing asset is active after latching"), AnimInstance->IsActiveLandingAsset(LandingClip));
	TestFalse(
		TEXT("A second result cannot replace the latched landing"),
		AnimInstance->TryLatchLandingSelection(
			PreviousAirborneClip,
			StandLightLandingDatabase,
			0.0f,
			false,
			AnimInstance->LandingRequestSerial));

	AnimInstance->UpdateLandingLatchedPlayback(
		PreviousAirborneClip,
		0.5f,
		1.0f,
		1.0f,
		0.01f);
	TestFalse(
		TEXT("A transient outgoing airborne pose is not mistaken for observed landing playback"),
		AnimInstance->bLandingPlaybackObserved);
	TestFalse(TEXT("The outgoing pose does not end the landing hold"), AnimInstance->bLandingCompletionArmed);

	AnimInstance->UpdateLandingLatchedPlayback(LandingClip, 0.5f, 1.0f, 1.0f, 0.01f);
	TestTrue(TEXT("The Blend Stack observes the exact landing clip"), AnimInstance->bLandingPlaybackObserved);
	TestTrue(
		TEXT("The natural watchdog is bounded to the remaining clip plus safety"),
		FMath::IsNearlyEqual(AnimInstance->LandingPlaybackWatchdogDuration, 0.6f));
	AnimInstance->UpdateJumpPhaseRuntime(0.5f, Proxy);
	TestEqual(TEXT("Landing remains latched before clip end"), AnimInstance->JumpPhase, ERpgJumpPhase::Landing);

	AnimInstance->UpdateLandingLatchedPlayback(LandingClip, 0.95f, 1.0f, 1.0f, 0.01f);
	TestTrue(TEXT("The clip-end tolerance arms landing completion"), AnimInstance->bLandingCompletionArmed);
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(TEXT("Natural landing completion returns to grounded locomotion"), AnimInstance->JumpPhase, ERpgJumpPhase::Grounded);
	TestFalse(TEXT("Natural completion clears the landing selection"), AnimInstance->bLandingSelectionLatched);

	// A missing database result must release to gait locomotion after the fixed selection timeout.
	Proxy.MovementState = ERpgLocomotionMovementState::Airborne;
	Proxy.bIsMovingOnGround = false;
	Proxy.bIsFalling = true;
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	SetValidLandingSelectionSnapshot(ERpgLocomotionGait::Idle, 0.0f, 500.0f, false);
	Proxy.MovementState = ERpgLocomotionMovementState::Grounded;
	Proxy.bIsMovingOnGround = true;
	Proxy.bIsFalling = false;
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	const uint32 MissingSelectionRequest = AnimInstance->LandingRequestSerial;
	AnimInstance->UpdateJumpPhaseRuntime(0.24f, Proxy);
	TestEqual(TEXT("A missing selection remains bounded before 250 ms"), AnimInstance->JumpPhase, ERpgJumpPhase::Landing);
	AnimInstance->UpdateJumpPhaseRuntime(0.02f, Proxy);
	TestEqual(TEXT("A missing selection releases after 250 ms"), AnimInstance->JumpPhase, ERpgJumpPhase::Grounded);

	// A new touchdown owns a new one-shot interrupt even after the previous request timed out.
	Proxy.MovementState = ERpgLocomotionMovementState::Airborne;
	Proxy.bIsMovingOnGround = false;
	Proxy.bIsFalling = true;
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	SetValidLandingSelectionSnapshot(ERpgLocomotionGait::Idle, 0.0f, 500.0f, false);
	Proxy.MovementState = ERpgLocomotionMovementState::Grounded;
	Proxy.bIsMovingOnGround = true;
	Proxy.bIsFalling = false;
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestTrue(TEXT("A later touchdown advances the request serial"), AnimInstance->LandingRequestSerial > MissingSelectionRequest);
	TestTrue(TEXT("The later touchdown receives one new ForceInterrupt"), AnimInstance->ConsumeLandingForceInterruptRequest());
	TestFalse(TEXT("The later touchdown also interrupts only once"), AnimInstance->ConsumeLandingForceInterruptRequest());

	// Looping or stuck content cannot hold gameplay locomotion indefinitely.
	TestTrue(
		TEXT("A looping safety fixture can latch"),
		AnimInstance->TryLatchLandingSelection(
			LandingClip,
			StandLightLandingDatabase,
			0.0f,
			true,
			AnimInstance->LandingRequestSerial));
	AnimInstance->UpdateLandingLatchedPlayback(LandingClip, 0.1f, 1.0f, 1.0f, 0.01f);
	AnimInstance->UpdateJumpPhaseRuntime(1.24f, Proxy);
	TestEqual(TEXT("A stuck landing remains held before 1.25 seconds"), AnimInstance->JumpPhase, ERpgJumpPhase::Landing);
	AnimInstance->UpdateJumpPhaseRuntime(0.02f, Proxy);
	TestEqual(TEXT("A stuck landing releases after 1.25 seconds"), AnimInstance->JumpPhase, ERpgJumpPhase::Grounded);

	// Overrides and a second jump cancel a cosmetic landing immediately.
	auto EnterLanding = [&]()
	{
		Proxy.bIsAnyMontagePlaying = false;
		Proxy.bIsCrouching = false;
		Proxy.bHasTurnInPlaceBlockingGameplayTag = false;
		Proxy.MovementState = ERpgLocomotionMovementState::Airborne;
		Proxy.bIsMovingOnGround = false;
		Proxy.bIsFalling = true;
		AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
		SetValidLandingSelectionSnapshot(ERpgLocomotionGait::Idle, 0.0f, 500.0f, false);
		Proxy.MovementState = ERpgLocomotionMovementState::Grounded;
		Proxy.bIsMovingOnGround = true;
		Proxy.bIsFalling = false;
		Proxy.GroundSpeed = 0.0f;
		Proxy.Gait = ERpgLocomotionGait::Idle;
		Proxy.bHasGroundedMoveIntent = false;
		AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	};

	EnterLanding();
	Proxy.bIsAnyMontagePlaying = true;
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(TEXT("A montage cancels cosmetic landing playback"), AnimInstance->JumpPhase, ERpgJumpPhase::Grounded);

	EnterLanding();
	Proxy.bIsCrouching = true;
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(TEXT("Crouch cancels cosmetic landing playback"), AnimInstance->JumpPhase, ERpgJumpPhase::Grounded);

	EnterLanding();
	Proxy.bHasTurnInPlaceBlockingGameplayTag = true;
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(TEXT("A blocking gameplay tag cancels cosmetic landing playback"), AnimInstance->JumpPhase, ERpgJumpPhase::Grounded);

	EnterLanding();
	Proxy.MovementState = ERpgLocomotionMovementState::Airborne;
	Proxy.bIsMovingOnGround = false;
	Proxy.bIsFalling = true;
	AnimInstance->UpdateJumpPhaseRuntime(0.01f, Proxy);
	TestEqual(TEXT("A second jump immediately re-enters Airborne"), AnimInstance->JumpPhase, ERpgJumpPhase::Airborne);
	TestFalse(TEXT("A second jump clears the old landing selection"), AnimInstance->bLandingSelectionLatched);

	// Explicit immutable asset categories keep each Blend Stack sample stable while global movement phases change.
	UPackage* JumpStartPackage = CreatePackage(
		TEXT("/Game/RpgJumpRuntimeTest/OutsideLegacyFolders/JumpStart"));
	UAnimSequence* JumpStart = NewObject<UAnimSequence>(
		JumpStartPackage,
		MakeUniqueObjectName(JumpStartPackage, UAnimSequence::StaticClass(), TEXT("RpgJumpRuntimeTest_Start")));
	JumpStart->bLoop = false;

	UPackage* GroundPackage = CreatePackage(
		TEXT("/RpgGaspLocomotion/Animations/Jump/Starts/RpgJumpRuntimeTest_ExplicitGround"));
	UAnimSequence* GroundStart = NewObject<UAnimSequence>(
		GroundPackage,
		MakeUniqueObjectName(GroundPackage, UAnimSequence::StaticClass(), TEXT("RpgJumpRuntimeTest_Ground")));
	GroundStart->bLoop = false;

	UPackage* GroundIdlePackage = CreatePackage(
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/RpgJumpRuntimeTest_Unmapped"));
	UAnimSequence* GroundIdle = NewObject<UAnimSequence>(
		GroundIdlePackage,
		MakeUniqueObjectName(GroundIdlePackage, UAnimSequence::StaticClass(), TEXT("RpgJumpRuntimeTest_Idle")));

	UPackage* FallPackage = CreatePackage(
		TEXT("/Game/RpgJumpRuntimeTest/OutsideLegacyFolders/Fall"));
	UAnimSequence* FallClip = NewObject<UAnimSequence>(
		FallPackage,
		MakeUniqueObjectName(FallPackage, UAnimSequence::StaticClass(), TEXT("RpgJumpRuntimeTest_Fall")));
	FallClip->bLoop = true;

	UPackage* LandPackage = CreatePackage(
		TEXT("/Game/RpgJumpRuntimeTest/OutsideLegacyFolders/Land"));
	UAnimSequence* UnlatchedLandClip = NewObject<UAnimSequence>(
		LandPackage,
		MakeUniqueObjectName(LandPackage, UAnimSequence::StaticClass(), TEXT("RpgJumpRuntimeTest_Land")));

	UPackage* BackwardStartPackage = CreatePackage(
		TEXT("/Game/RpgJumpRuntimeTest/OutsideLegacyFolders/ExplicitBackward"));
	UAnimSequence* BackwardStart = NewObject<UAnimSequence>(
		BackwardStartPackage,
		MakeUniqueObjectName(BackwardStartPackage, UAnimSequence::StaticClass(), TEXT("NotNamedLikeABackwardStart")));
	BackwardStart->bLoop = false;

	URpgGaspPresentationProfile* PresentationProfile = NewObject<URpgGaspPresentationProfile>();
	const auto AddMembership = [PresentationProfile](
		UAnimSequenceBase* Asset,
		ERpgGaspPresentationAssetCategory Category)
	{
		FRpgGaspPresentationAssetMembership& Membership =
			PresentationProfile->AssetMemberships.AddDefaulted_GetRef();
		Membership.Asset = Asset;
		Membership.Category = Category;
	};
	AddMembership(JumpStart, ERpgGaspPresentationAssetCategory::JumpStart);
	AddMembership(GroundStart, ERpgGaspPresentationAssetCategory::GroundMoving);
	AddMembership(FallClip, ERpgGaspPresentationAssetCategory::AirborneFall);
	AddMembership(UnlatchedLandClip, ERpgGaspPresentationAssetCategory::Landing);
	AddMembership(BackwardStart, ERpgGaspPresentationAssetCategory::BackwardJumpStart);
	TestTrue(
		TEXT("Explicit presentation membership passes its focused validation contract"),
		PresentationProfile->ValidateProfile().IsMembershipValid());
	TestTrue(
		TEXT("A valid presentation profile builds the immutable runtime lookup"),
		AnimInstance->GaspPresentationAssetLookup.Build(PresentationProfile));

	TestTrue(TEXT("An explicitly categorized non-looping sequence is a jump start"), AnimInstance->IsAirborneJumpStartAsset(JumpStart));
	TestTrue(TEXT("A categorized Jump Start belongs to the exclusive airborne reset contract"), AnimInstance->IsAirborneJumpAsset(JumpStart));
	TestFalse(TEXT("An explicitly grounded pose is not a jump start despite its misleading package"), AnimInstance->IsAirborneJumpStartAsset(GroundStart));
	TestTrue(TEXT("Explicit GroundMoving membership keeps the moving per-sample contract"), AnimInstance->IsGroundMovingAsset(GroundStart));
	TestFalse(TEXT("Explicit GroundMoving membership never implies an airborne jump"), AnimInstance->IsAirborneJumpAsset(GroundStart));
	TestFalse(TEXT("An unmapped old Stand/Run package cannot enable moving procedural gates"), AnimInstance->IsGroundMovingAsset(GroundIdle));
	TestTrue(TEXT("An explicitly categorized looping fall belongs to the airborne reset contract"), AnimInstance->IsAirborneJumpAsset(FallClip));
	TestFalse(TEXT("The looping fall never receives Jump Start moving corrections"), AnimInstance->IsAirborneJumpStartAsset(FallClip));
	TestFalse(TEXT("An unlatched landing is not an airborne database sample"), AnimInstance->IsAirborneJumpAsset(UnlatchedLandClip));
	TestTrue(
		TEXT("An unlatched or outgoing landing keeps its immutable Landing asset contract"),
		AnimInstance->IsLandingAsset(UnlatchedLandClip));
	TestTrue(TEXT("Explicit BackwardJumpStart membership enters the bounded backward hold"), AnimInstance->IsBackwardJumpStartAsset(BackwardStart));
	TestFalse(TEXT("An ordinary JumpStart never enters the backward hold"), AnimInstance->IsBackwardJumpStartAsset(JumpStart));
	TestTrue(TEXT("The looping Airborne clip is a continuing fall"), AnimInstance->IsLoopingAirborneFallAsset(FallClip));
	TestFalse(TEXT("A non-looping Jump Start is not a continuing fall"), AnimInstance->IsLoopingAirborneFallAsset(BackwardStart));

	JumpStart->bLoop = true;
	TestTrue(
		TEXT("The worker lookup does not reread sequence loop metadata after game-thread initialization"),
		AnimInstance->IsAirborneJumpStartAsset(JumpStart));
	TestTrue(
		TEXT("Profile validation catches a looping JumpStart before the next initialization"),
		PresentationProfile->ValidateProfile().bHasLoopingJumpStart);
	FRpgGaspPresentationAssetLookup InvalidLookup;
	TestFalse(TEXT("An invalid profile fails closed while rebuilding"), InvalidLookup.Build(PresentationProfile));
	TestFalse(
		TEXT("A failed lookup build exposes no partial presentation traits"),
		InvalidLookup.HasTrait(JumpStart, ERpgGaspPresentationAssetTrait::JumpStart));
	JumpStart->bLoop = false;
	TestTrue(
		TEXT("A descending fall continues after the bounded backward-start path"),
		RpgJumpRuntime::ShouldHoldLoopingAirborneFallPlayback(
			ERpgJumpPhase::Airborne, true, -100.0f, true));
	TestFalse(
		TEXT("A side or forward fall remains searchable when no backward hold was armed"),
		RpgJumpRuntime::ShouldHoldLoopingAirborneFallPlayback(
			ERpgJumpPhase::Airborne, false, -100.0f, true));
	TestFalse(
		TEXT("An upward relaunch releases a previously held fall loop"),
		RpgJumpRuntime::ShouldHoldLoopingAirborneFallPlayback(
			ERpgJumpPhase::Airborne, true, 100.0f, true));
	TestTrue(
		TEXT("The fall loop accepts the inclusive descending epsilon boundary"),
		RpgJumpRuntime::ShouldHoldLoopingAirborneFallPlayback(
			ERpgJumpPhase::Airborne, true, UE_KINDA_SMALL_NUMBER, true));

	TestTrue(
		TEXT("A backward start remains continuing playback after its 0.565 second transition block"),
		RpgJumpRuntime::ShouldHoldBackwardJumpStartPlayback(
			ERpgJumpPhase::Airborne, true, 0.67f, 1.97f, 1.0f, 0.67f));
	TestFalse(
		TEXT("The hold releases one authored blend interval before clip end"),
		RpgJumpRuntime::ShouldHoldBackwardJumpStartPlayback(
			ERpgJumpPhase::Airborne, true, 1.78f, 1.97f, 1.0f, 1.0f));
	TestFalse(
		TEXT("The authored release lead boundary is strict"),
		RpgJumpRuntime::ShouldHoldBackwardJumpStartPlayback(
			ERpgJumpPhase::Airborne, true, 0.8f, 1.0f, 1.0f, 0.5f));
	TestTrue(
		TEXT("Paused playback remains bounded by the independent hold watchdog"),
		RpgJumpRuntime::ShouldHoldBackwardJumpStartPlayback(
			ERpgJumpPhase::Airborne, true, 0.8f, 1.0f, 0.0f, 0.5f));
	TestFalse(
		TEXT("Non-finite playback rate fails open"),
		RpgJumpRuntime::ShouldHoldBackwardJumpStartPlayback(
			ERpgJumpPhase::Airborne, true, 0.8f, 1.0f, QuietNaN, 0.5f));
	TestFalse(
		TEXT("The hold watchdog releases a genuine long fall"),
		RpgJumpRuntime::ShouldHoldBackwardJumpStartPlayback(
			ERpgJumpPhase::Airborne, true, 1.0f, 1.97f, 1.0f, 1.25f));
	TestTrue(
		TEXT("The near-end release threshold is play-rate aware"),
		RpgJumpRuntime::ShouldHoldBackwardJumpStartPlayback(
			ERpgJumpPhase::Airborne, true, 1.75f, 1.97f, 0.5f, 1.0f));
	TestFalse(
		TEXT("Grounded playback cannot retain an airborne start"),
		RpgJumpRuntime::ShouldHoldBackwardJumpStartPlayback(
			ERpgJumpPhase::Grounded, true, 0.67f, 1.97f, 1.0f, 0.67f));
	TestFalse(
		TEXT("An unexpected active asset fails open"),
		RpgJumpRuntime::ShouldHoldBackwardJumpStartPlayback(
			ERpgJumpPhase::Airborne, false, 0.67f, 1.97f, 1.0f, 0.67f));
	TestFalse(
		TEXT("Invalid playback timing fails open"),
		RpgJumpRuntime::ShouldHoldBackwardJumpStartPlayback(
			ERpgJumpPhase::Airborne, true, 0.0f, 0.0f, 1.0f, 0.0f));

	FRpgGaspLocomotionTuning CustomJumpTuning;
	CustomJumpTuning.BackwardJumpStartHoldTimeout = 0.5f;
	TestTrue(
		TEXT("Compatibility tuning still holds before its original watchdog"),
		RpgJumpRuntime::ShouldHoldBackwardJumpStartPlayback(
			ERpgJumpPhase::Airborne, true, 0.1f, 2.0f, 1.0f, 0.5f));
	TestFalse(
		TEXT("A shorter profile watchdog releases at its customized inclusive boundary"),
		RpgJumpRuntime::ShouldHoldBackwardJumpStartPlayback(
			ERpgJumpPhase::Airborne,
			true,
			0.1f,
			2.0f,
			1.0f,
			0.5f,
			CustomJumpTuning));
	CustomJumpTuning.BackwardJumpStartHoldTimeout = 1.25f;
	CustomJumpTuning.BackwardJumpStartReleaseLeadTime = 0.6f;
	TestTrue(
		TEXT("Compatibility release lead retains a half-second playback remainder"),
		RpgJumpRuntime::ShouldHoldBackwardJumpStartPlayback(
			ERpgJumpPhase::Airborne, true, 1.5f, 2.0f, 1.0f, 0.1f));
	TestFalse(
		TEXT("A larger profile release lead fails open at the same playback remainder"),
		RpgJumpRuntime::ShouldHoldBackwardJumpStartPlayback(
			ERpgJumpPhase::Airborne,
			true,
			1.5f,
			2.0f,
			1.0f,
			0.1f,
			CustomJumpTuning));

	FRpgBackwardJumpStartHoldState OrdinaryAirborneState;
	OrdinaryAirborneState.bHoldEligible = true;
	FRpgBackwardJumpStartPlaybackSnapshot OrdinaryAirborneSnapshot;
	OrdinaryAirborneSnapshot.JumpPhase = ERpgJumpPhase::Airborne;
	OrdinaryAirborneSnapshot.CurrentAssetTime = 0.1f;
	OrdinaryAirborneSnapshot.CurrentAssetLength = 1.0f;
	OrdinaryAirborneSnapshot.CurrentAssetPlayRate = 1.0f;
	OrdinaryAirborneSnapshot.bCurrentAssetIsAirborne = true;
	const FRpgBackwardJumpStartHoldResult OrdinaryAirborneResult =
		RpgJumpRuntime::UpdateBackwardJumpStartHold(
			OrdinaryAirborneState,
			OrdinaryAirborneSnapshot);
	TestTrue(
		TEXT("The first ordinary Airborne result consumes the one hold opportunity"),
		OrdinaryAirborneResult.State.bOpportunityConsumed);
	FRpgBackwardJumpStartPlaybackSnapshot LaterBackwardSnapshot = OrdinaryAirborneSnapshot;
	LaterBackwardSnapshot.bCurrentAssetIsBackwardStart = true;
	const FRpgBackwardJumpStartHoldResult LaterBackwardResult =
		RpgJumpRuntime::UpdateBackwardJumpStartHold(
			OrdinaryAirborneResult.State,
			LaterBackwardSnapshot);
	TestFalse(
		TEXT("A later backward result cannot arm after an ordinary Airborne result"),
		LaterBackwardResult.bCaptureCurrentAsset);

	AnimInstance->BeginAirbornePhase(true);
	TestFalse(
		TEXT("A first backward result already inside the release lead never holds"),
		AnimInstance->UpdateBackwardJumpStartHold(BackwardStart, 1.8f, 1.9f, 1.0f, 0.0f));
	TestNull(
		TEXT("Immediate capture-and-release leaves no stale GC-tracked held asset"),
		AnimInstance->BackwardJumpStartHeldAsset.Get());
	TestTrue(
		TEXT("Immediate release still consumes the airborne opportunity"),
		AnimInstance->bBackwardJumpStartHoldOpportunityConsumed);
	TestTrue(
		TEXT("Immediate release still scopes fall continuation to the armed path"),
		AnimInstance->bBackwardJumpStartHoldWasArmed);

	AnimInstance->BeginAirbornePhase(true);
	TestFalse(
		TEXT("The outgoing grounded sample does not consume the initial airborne selection"),
		AnimInstance->UpdateBackwardJumpStartHold(GroundStart, 0.1f, 1.0f, 1.0f, 0.01f));
	TestFalse(
		TEXT("The outgoing sample leaves the first airborne result unresolved"),
		AnimInstance->bBackwardJumpStartHoldOpportunityConsumed);
	TestTrue(
		TEXT("The first backward result is held before its database can reselect"),
		AnimInstance->UpdateBackwardJumpStartHold(BackwardStart, 0.02f, 1.97f, 1.0f, 0.01f));
	TestEqual(
		TEXT("The exact backward result owns the hold"),
		AnimInstance->BackwardJumpStartHeldAsset.Get(),
		static_cast<UAnimationAsset*>(BackwardStart));
	AnimInstance->BackwardJumpStartHoldElapsed = 1.24f;
	TestFalse(
		TEXT("The bounded watchdog releases the held start"),
		AnimInstance->UpdateBackwardJumpStartHold(BackwardStart, 1.0f, 1.97f, 1.0f, 0.02f));
	TestNull(TEXT("A released hold clears its asset"), AnimInstance->BackwardJumpStartHeldAsset.Get());
	TestFalse(
		TEXT("A later backward result cannot re-arm within the same jump"),
		AnimInstance->UpdateBackwardJumpStartHold(BackwardStart, 0.02f, 1.97f, 1.0f, 0.01f));
	AnimInstance->BeginAirbornePhase(true);
	TestTrue(
		TEXT("A new airborne phase may hold a new backward result"),
		AnimInstance->UpdateBackwardJumpStartHold(BackwardStart, 0.02f, 1.97f, 1.0f, 0.01f));
	AnimInstance->BeginAirbornePhase(false);
	TestFalse(
		TEXT("A descending ledge fall never arms the backward takeoff hold"),
		AnimInstance->UpdateBackwardJumpStartHold(BackwardStart, 0.02f, 1.97f, 1.0f, 0.01f));
	AnimInstance->ResetJumpPhaseRuntime();

	const URpgAnimInstance::FGaspProceduralGates GroundGates =
		AnimInstance->ResolveGaspProceduralGates(true, 1.0f, false, false, false, 0.5f, true, true);
	TestEqual(TEXT("Ground movement enables Reset Root"), GroundGates.ResetRootAlpha, 1.0f);
	TestEqual(TEXT("Ground Orientation Warping follows its authored curve"), GroundGates.OrientationWarpingAlpha, 0.5f);
	TestTrue(TEXT("Ground movement with an active asset and trajectory enables Steering"), GroundGates.bEnableSteering);
	const URpgAnimInstance::FGaspProceduralGates OutgoingGroundGates =
		AnimInstance->ResolveGaspProceduralGates(true, 1.0f, false, false, false, 0.5f, false, true);
	TestEqual(TEXT("An outgoing ground blend keeps Reset Root"), OutgoingGroundGates.ResetRootAlpha, 1.0f);
	TestEqual(TEXT("An outgoing ground blend keeps its authored OW curve"), OutgoingGroundGates.OrientationWarpingAlpha, 0.5f);
	TestFalse(TEXT("Only the active Blend Stack branch receives Steering"), OutgoingGroundGates.bEnableSteering);

	const URpgAnimInstance::FGaspProceduralGates StandJumpGates =
		AnimInstance->ResolveGaspProceduralGates(false, 1.0f, true, true, false, 0.0f, true, true);
	TestEqual(TEXT("A stand jump start enables Reset Root without Enable_Warping"), StandJumpGates.ResetRootAlpha, 1.0f);
	TestEqual(TEXT("A missing Enable_Warping curve keeps jump OW off"), StandJumpGates.OrientationWarpingAlpha, 0.0f);
	TestTrue(TEXT("A stand jump start still enables targeted Steering"), StandJumpGates.bEnableSteering);

	const URpgAnimInstance::FGaspProceduralGates WarpedJumpGates =
		AnimInstance->ResolveGaspProceduralGates(false, 1.0f, true, true, false, 0.75f, true, true);
	TestEqual(TEXT("A directional jump start curve gates OW independently"), WarpedJumpGates.OrientationWarpingAlpha, 0.75f);

	const URpgAnimInstance::FGaspProceduralGates FallGates =
		AnimInstance->ResolveGaspProceduralGates(false, 1.0f, true, false, false, 1.0f, true, true);
	TestEqual(TEXT("The looped fall keeps Reset Root through airborne handoff"), FallGates.ResetRootAlpha, 1.0f);
	TestEqual(TEXT("The looped fall cannot receive blanket OW"), FallGates.OrientationWarpingAlpha, 0.0f);
	TestFalse(TEXT("The looped fall cannot receive Steering"), FallGates.bEnableSteering);

	const URpgAnimInstance::FGaspProceduralGates IdleLandingGates =
		AnimInstance->ResolveGaspProceduralGates(false, 0.0f, false, false, true, 1.0f, true, true);
	TestEqual(TEXT("The exactly latched Idle landing enables Reset Root"), IdleLandingGates.ResetRootAlpha, 1.0f);
	TestEqual(TEXT("An Idle landing cannot inherit OW even if a curve is present"), IdleLandingGates.OrientationWarpingAlpha, 0.0f);
	TestFalse(TEXT("An Idle landing remains Reset Root-only without moving Steering"), IdleLandingGates.bEnableSteering);

	const URpgAnimInstance::FGaspProceduralGates OutgoingLandingGates =
		AnimInstance->ResolveGaspProceduralGates(
			false,
			0.0f,
			false,
			false,
			AnimInstance->IsLandingAsset(UnlatchedLandClip),
			1.0f,
			false,
			true);
	TestEqual(
		TEXT("An outgoing unlatched Landing sample keeps Reset Root"),
		OutgoingLandingGates.ResetRootAlpha,
		1.0f);
	TestEqual(
		TEXT("An outgoing unlatched Landing sample cannot inherit Orientation Warping"),
		OutgoingLandingGates.OrientationWarpingAlpha,
		0.0f);
	TestFalse(
		TEXT("An outgoing unlatched Landing sample cannot steer"),
		OutgoingLandingGates.bEnableSteering);

	// The moving-pose input represents the frozen Walk/Run landing role, not current
	// post-touchdown speed, gait, or move intent. Its authored corrections remain active
	// for the exact latched request even after those live grounded values change.
	const URpgAnimInstance::FGaspProceduralGates MovingLandingGates =
		AnimInstance->ResolveGaspProceduralGates(true, 1.0f, false, false, true, 0.65f, true, true);
	TestEqual(TEXT("A latched Walk or Run landing keeps Reset Root"), MovingLandingGates.ResetRootAlpha, 1.0f);
	TestEqual(
		TEXT("A latched Walk or Run landing keeps its authored Enable_Warping curve"),
		MovingLandingGates.OrientationWarpingAlpha,
		0.65f);
	TestTrue(
		TEXT("A latched Walk or Run landing keeps Steering with an active asset and trajectory"),
		MovingLandingGates.bEnableSteering);

	const URpgAnimInstance::FGaspProceduralGates MissingTrajectoryGates =
		AnimInstance->ResolveGaspProceduralGates(false, 1.0f, true, true, false, 1.0f, true, false);
	TestEqual(TEXT("Missing trajectory does not erase the sample's Reset Root gate"), MissingTrajectoryGates.ResetRootAlpha, 1.0f);
	TestFalse(TEXT("Missing trajectory disables Steering"), MissingTrajectoryGates.bEnableSteering);
	TestTrue(
		TEXT("Ground-to-start-to-fall-to-ground keeps a continuous Reset Root contract"),
		FMath::IsNearlyEqual(GroundGates.ResetRootAlpha, StandJumpGates.ResetRootAlpha) &&
		FMath::IsNearlyEqual(StandJumpGates.ResetRootAlpha, FallGates.ResetRootAlpha) &&
		FMath::IsNearlyEqual(FallGates.ResetRootAlpha, OutgoingGroundGates.ResetRootAlpha));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
