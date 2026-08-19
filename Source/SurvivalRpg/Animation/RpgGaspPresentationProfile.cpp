// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgGaspPresentationProfile.h"

#include "Animation/AnimationAsset.h"
#include "Animation/AnimSequenceBase.h"
#include "PoseSearch/PoseSearchDatabase.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGaspPresentationProfile)

namespace
{
ERpgGaspPresentationAssetTrait ResolvePresentationTraits(
	ERpgGaspPresentationAssetCategory Category)
{
	switch (Category)
	{
	case ERpgGaspPresentationAssetCategory::GroundMoving:
		return ERpgGaspPresentationAssetTrait::GroundMoving;
	case ERpgGaspPresentationAssetCategory::JumpStart:
		return ERpgGaspPresentationAssetTrait::Airborne |
			ERpgGaspPresentationAssetTrait::JumpStart;
	case ERpgGaspPresentationAssetCategory::BackwardJumpStart:
		return ERpgGaspPresentationAssetTrait::Airborne |
			ERpgGaspPresentationAssetTrait::JumpStart |
			ERpgGaspPresentationAssetTrait::BackwardJumpStart;
	case ERpgGaspPresentationAssetCategory::AirborneFall:
		return ERpgGaspPresentationAssetTrait::Airborne |
			ERpgGaspPresentationAssetTrait::AirborneFall;
	case ERpgGaspPresentationAssetCategory::Landing:
		return ERpgGaspPresentationAssetTrait::Landing;
	default:
		return ERpgGaspPresentationAssetTrait::None;
	}
}

ERpgGaspPresentationAssetTrait GetRequiredDatabaseTrait(
	ERpgMotionMatchingDatabaseRole Role)
{
	switch (Role)
	{
	case ERpgMotionMatchingDatabaseRole::StandWalk:
	case ERpgMotionMatchingDatabaseRole::StandWalkStops:
	case ERpgMotionMatchingDatabaseRole::StandRunLoops:
	case ERpgMotionMatchingDatabaseRole::StandRunPivots:
	case ERpgMotionMatchingDatabaseRole::StandRunStarts:
	case ERpgMotionMatchingDatabaseRole::StandRunStops:
	case ERpgMotionMatchingDatabaseRole::StandSprint:
	case ERpgMotionMatchingDatabaseRole::StandSprintStops:
		return ERpgGaspPresentationAssetTrait::GroundMoving;
	case ERpgMotionMatchingDatabaseRole::Jump:
		return ERpgGaspPresentationAssetTrait::Airborne;
	case ERpgMotionMatchingDatabaseRole::StandLightLanding:
	case ERpgMotionMatchingDatabaseRole::StandHeavyLanding:
	case ERpgMotionMatchingDatabaseRole::WalkLightLanding:
	case ERpgMotionMatchingDatabaseRole::WalkHeavyLanding:
	case ERpgMotionMatchingDatabaseRole::RunLightLanding:
	case ERpgMotionMatchingDatabaseRole::RunHeavyLanding:
		return ERpgGaspPresentationAssetTrait::Landing;
	default:
		return ERpgGaspPresentationAssetTrait::None;
	}
}
}

FRpgGaspPresentationProfileValidation URpgGaspPresentationProfile::ValidateProfile() const
{
	FRpgGaspPresentationProfileValidation Validation;
	TSet<const UAnimSequenceBase*> SeenAssets;
	TMap<const UAnimationAsset*, ERpgGaspPresentationAssetTrait> TraitsByAsset;
	Validation.bIsEmpty = AssetMemberships.IsEmpty();

	for (const FRpgGaspPresentationAssetMembership& Membership : AssetMemberships)
	{
		const UAnimSequenceBase* Asset = Membership.Asset.Get();
		if (!Asset)
		{
			Validation.bHasNullAsset = true;
			continue;
		}

		if (SeenAssets.Contains(Asset))
		{
			Validation.bHasDuplicateAsset = true;
		}
		else
		{
			SeenAssets.Add(Asset);
		}

		switch (Membership.Category)
		{
		case ERpgGaspPresentationAssetCategory::GroundMoving:
			break;
		case ERpgGaspPresentationAssetCategory::JumpStart:
			Validation.bHasLoopingJumpStart |= Asset->bLoop;
			break;
		case ERpgGaspPresentationAssetCategory::BackwardJumpStart:
			Validation.bHasLoopingJumpStart |= Asset->bLoop;
			break;
		case ERpgGaspPresentationAssetCategory::AirborneFall:
			Validation.bHasNonLoopingAirborneFall |= !Asset->bLoop;
			break;
		case ERpgGaspPresentationAssetCategory::Landing:
			Validation.bHasLoopingLanding |= Asset->bLoop;
			break;
		default:
			Validation.bHasUnassignedCategory = true;
			break;
		}

		TraitsByAsset.Add(Asset, ResolvePresentationTraits(Membership.Category));
	}

	Validation.bHasEmptyRuntimeDatabases = RuntimeMotionMatchingDatabases.IsEmpty();
	TSet<const UPoseSearchDatabase*> SeenDatabases;
	constexpr int32 RoleCount = static_cast<int32>(ERpgMotionMatchingDatabaseRole::Count);
	int32 CountsByRole[RoleCount] = {};
	for (UPoseSearchDatabase* Database : RuntimeMotionMatchingDatabases)
	{
		if (!Database)
		{
			Validation.bHasNullRuntimeDatabase = true;
			continue;
		}

		if (SeenDatabases.Contains(Database))
		{
			Validation.bHasDuplicateRuntimeDatabase = true;
		}
		else
		{
			SeenDatabases.Add(Database);
		}

		const ERpgMotionMatchingDatabaseRole Role =
			RpgGaspLocomotionConfig::ResolveDatabaseRoleTag(Database->Tags);
		const int32 RoleIndex = static_cast<int32>(Role);
		if (RoleIndex <= static_cast<int32>(ERpgMotionMatchingDatabaseRole::None) ||
			RoleIndex >= RoleCount)
		{
			Validation.bHasInvalidRuntimeDatabaseRoleTag = true;
			continue;
		}
		++CountsByRole[RoleIndex];
		const int32 AnimationAssetCount = Database->GetNumAnimationAssets();
		Validation.bHasRuntimeDatabaseWithoutAssets |= AnimationAssetCount <= 0;

		const ERpgGaspPresentationAssetTrait RequiredTrait = GetRequiredDatabaseTrait(Role);
		if (RequiredTrait == ERpgGaspPresentationAssetTrait::None)
		{
			continue;
		}

		bool bCoverageValid = AnimationAssetCount > 0;
		for (int32 AssetIndex = 0; AssetIndex < AnimationAssetCount; ++AssetIndex)
		{
			const UAnimationAsset* Asset =
				Cast<UAnimationAsset>(Database->GetAnimationAsset(AssetIndex));
			const ERpgGaspPresentationAssetTrait* Traits = TraitsByAsset.Find(Asset);
			bCoverageValid &= Traits && EnumHasAllFlags(*Traits, RequiredTrait);
		}

		switch (RequiredTrait)
		{
		case ERpgGaspPresentationAssetTrait::GroundMoving:
			Validation.bHasGroundMovingCoverageMismatch |= !bCoverageValid;
			break;
		case ERpgGaspPresentationAssetTrait::Airborne:
			Validation.bHasAirborneCoverageMismatch |= !bCoverageValid;
			break;
		case ERpgGaspPresentationAssetTrait::Landing:
			Validation.bHasLandingCoverageMismatch |= !bCoverageValid;
			break;
		default:
			break;
		}
	}

	for (int32 RoleIndex = static_cast<int32>(ERpgMotionMatchingDatabaseRole::None) + 1;
		RoleIndex < RoleCount;
		++RoleIndex)
	{
		Validation.bHasMissingRuntimeDatabaseRole |= CountsByRole[RoleIndex] == 0;
		Validation.bHasDuplicateRuntimeDatabaseRole |= CountsByRole[RoleIndex] > 1;
	}
	Validation.bHasInvalidTuning =
		!RpgGaspLocomotionConfig::IsTuningRuntimeValid(LocomotionTuning);

	return Validation;
}

#if WITH_EDITOR
EDataValidationResult URpgGaspPresentationProfile::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context);
	const FRpgGaspPresentationProfileValidation Validation = ValidateProfile();
	const bool bHasRuntimeDatabaseConfiguration = !RuntimeMotionMatchingDatabases.IsEmpty();

	if (Validation.bIsEmpty)
	{
		Context.AddError(FText::FromString(
			TEXT("GASP presentation membership must contain at least one animation asset.")));
	}
	if (Validation.bHasNullAsset)
	{
		Context.AddError(FText::FromString(
			TEXT("GASP presentation membership contains at least one null animation asset.")));
	}
	if (Validation.bHasDuplicateAsset)
	{
		Context.AddError(FText::FromString(
			TEXT("Every GASP presentation animation asset must have exactly one category.")));
	}
	if (Validation.bHasUnassignedCategory)
	{
		Context.AddError(FText::FromString(
			TEXT("Every GASP presentation entry must select a non-None category.")));
	}
	if (Validation.bHasLoopingJumpStart)
	{
		Context.AddError(FText::FromString(
			TEXT("JumpStart and BackwardJumpStart presentation assets must be non-looping.")));
	}
	if (Validation.bHasNonLoopingAirborneFall)
	{
		Context.AddError(FText::FromString(
			TEXT("The AirborneFall presentation asset must be looping.")));
	}
	if (Validation.bHasLoopingLanding)
	{
		Context.AddError(FText::FromString(
			TEXT("Landing presentation assets must be non-looping.")));
	}
	if (bHasRuntimeDatabaseConfiguration && Validation.bHasNullRuntimeDatabase)
	{
		Context.AddError(FText::FromString(
			TEXT("GASP runtime databases contain at least one null database.")));
	}
	if (bHasRuntimeDatabaseConfiguration && Validation.bHasDuplicateRuntimeDatabase)
	{
		Context.AddError(FText::FromString(
			TEXT("A GASP runtime Pose Search database must not appear more than once.")));
	}
	if (bHasRuntimeDatabaseConfiguration && Validation.bHasRuntimeDatabaseWithoutAssets)
	{
		Context.AddError(FText::FromString(
			TEXT("Every GASP runtime Pose Search database must contain at least one animation asset.")));
	}
	if (bHasRuntimeDatabaseConfiguration && Validation.bHasInvalidRuntimeDatabaseRoleTag)
	{
		Context.AddError(FText::FromString(
			TEXT("Every GASP runtime database must carry exactly one known Rpg.MotionMatching.Role.* tag.")));
	}
	if (bHasRuntimeDatabaseConfiguration &&
		(Validation.bHasDuplicateRuntimeDatabaseRole || Validation.bHasMissingRuntimeDatabaseRole))
	{
		Context.AddError(FText::FromString(
			TEXT("GASP runtime databases must resolve exactly one database for every non-None project role.")));
	}
	if (bHasRuntimeDatabaseConfiguration && Validation.bHasGroundMovingCoverageMismatch)
	{
		Context.AddError(FText::FromString(
			TEXT("Every Walk, Run, and Sprint runtime database asset must have GroundMoving presentation membership.")));
	}
	if (bHasRuntimeDatabaseConfiguration && Validation.bHasAirborneCoverageMismatch)
	{
		Context.AddError(FText::FromString(
			TEXT("Every Jump runtime database asset must have JumpStart, BackwardJumpStart, or AirborneFall presentation membership.")));
	}
	if (bHasRuntimeDatabaseConfiguration && Validation.bHasLandingCoverageMismatch)
	{
		Context.AddError(FText::FromString(
			TEXT("Every curated runtime landing database asset must have Landing presentation membership.")));
	}
	if (bHasRuntimeDatabaseConfiguration && Validation.bHasInvalidTuning)
	{
		Context.AddError(FText::FromString(
			TEXT("GASP locomotion tuning must be finite and satisfy its normalized, ordered threshold, angle, and duration contracts.")));
	}
	return Context.GetNumErrors() > 0
		? EDataValidationResult::Invalid
		: EDataValidationResult::Valid;
}
#endif

bool FRpgGaspPresentationAssetLookup::Build(const URpgGaspPresentationProfile* Profile)
{
	const FRpgGaspPresentationProfileValidation Validation = Profile
		? Profile->ValidateProfile()
		: FRpgGaspPresentationProfileValidation();
	return BuildValidated(Profile, Validation);
}

bool FRpgGaspPresentationAssetLookup::BuildValidated(
	const URpgGaspPresentationProfile* Profile,
	const FRpgGaspPresentationProfileValidation& Validation)
{
	Reset();
	if (!Profile || !Validation.IsMembershipValid())
	{
		return false;
	}

	AssetTraits.Reserve(Profile->AssetMemberships.Num());
	for (const FRpgGaspPresentationAssetMembership& Membership : Profile->AssetMemberships)
	{
		AssetTraits.Add(
			Membership.Asset.Get(),
			ResolvePresentationTraits(Membership.Category));
	}
	return true;
}

void FRpgGaspPresentationAssetLookup::Reset()
{
	AssetTraits.Reset();
}

bool FRpgGaspPresentationAssetLookup::HasTrait(
	const UAnimationAsset* Asset,
	ERpgGaspPresentationAssetTrait Trait) const
{
	if (!Asset || Trait == ERpgGaspPresentationAssetTrait::None)
	{
		return false;
	}

	const ERpgGaspPresentationAssetTrait* Traits = AssetTraits.Find(Asset);
	return Traits && EnumHasAllFlags(*Traits, Trait);
}

bool FRpgGaspMotionMatchingDatabaseLookup::Build(
	const URpgGaspPresentationProfile* Profile)
{
	const FRpgGaspPresentationProfileValidation Validation = Profile
		? Profile->ValidateProfile()
		: FRpgGaspPresentationProfileValidation();
	return BuildValidated(Profile, Validation);
}

bool FRpgGaspMotionMatchingDatabaseLookup::BuildValidated(
	const URpgGaspPresentationProfile* Profile,
	const FRpgGaspPresentationProfileValidation& Validation)
{
	Reset();
	if (!Profile || !Validation.IsRuntimeDatabaseConfigValid())
	{
		return false;
	}

	DatabaseByRole.Reserve(Profile->RuntimeMotionMatchingDatabases.Num());
	RoleByDatabase.Reserve(Profile->RuntimeMotionMatchingDatabases.Num());
	for (UPoseSearchDatabase* Database : Profile->RuntimeMotionMatchingDatabases)
	{
		const ERpgMotionMatchingDatabaseRole Role =
			RpgGaspLocomotionConfig::ResolveDatabaseRoleTag(Database->Tags);
		if (!AddResolvedBinding(Role, Database))
		{
			Reset();
			return false;
		}
	}
	return true;
}

bool FRpgGaspMotionMatchingDatabaseLookup::AddResolvedBinding(
	ERpgMotionMatchingDatabaseRole Role,
	UPoseSearchDatabase* Database)
{
	if (Role <= ERpgMotionMatchingDatabaseRole::None ||
		Role >= ERpgMotionMatchingDatabaseRole::Count ||
		!Database ||
		DatabaseByRole.Contains(Role) ||
		RoleByDatabase.Contains(Database))
	{
		return false;
	}

	DatabaseByRole.Add(Role, Database);
	RoleByDatabase.Add(Database, Role);
	return true;
}

void FRpgGaspMotionMatchingDatabaseLookup::Reset()
{
	DatabaseByRole.Reset();
	RoleByDatabase.Reset();
}

UPoseSearchDatabase* FRpgGaspMotionMatchingDatabaseLookup::FindDatabase(
	ERpgMotionMatchingDatabaseRole Role) const
{
	if (Role <= ERpgMotionMatchingDatabaseRole::None ||
		Role >= ERpgMotionMatchingDatabaseRole::Count)
	{
		return nullptr;
	}

	UPoseSearchDatabase* const* Database = DatabaseByRole.Find(Role);
	return Database ? *Database : nullptr;
}

ERpgMotionMatchingDatabaseRole FRpgGaspMotionMatchingDatabaseLookup::FindRole(
	const UPoseSearchDatabase* Database) const
{
	if (!Database)
	{
		return ERpgMotionMatchingDatabaseRole::None;
	}

	const ERpgMotionMatchingDatabaseRole* Role = RoleByDatabase.Find(Database);
	return Role ? *Role : ERpgMotionMatchingDatabaseRole::None;
}
