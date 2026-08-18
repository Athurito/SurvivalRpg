// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgGaspPresentationProfile.h"

#include "Animation/AnimSequenceBase.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGaspPresentationProfile)

FRpgGaspPresentationProfileValidation URpgGaspPresentationProfile::ValidateProfile() const
{
	FRpgGaspPresentationProfileValidation Validation;
	TSet<const UAnimSequenceBase*> SeenAssets;
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
	}

	return Validation;
}

#if WITH_EDITOR
EDataValidationResult URpgGaspPresentationProfile::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context);
	const FRpgGaspPresentationProfileValidation Validation = ValidateProfile();

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
	return Context.GetNumErrors() > 0
		? EDataValidationResult::Invalid
		: EDataValidationResult::Valid;
}
#endif

bool FRpgGaspPresentationAssetLookup::Build(const URpgGaspPresentationProfile* Profile)
{
	Reset();
	if (!Profile || !Profile->ValidateProfile().IsValid())
	{
		return false;
	}

	AssetTraits.Reserve(Profile->AssetMemberships.Num());
	for (const FRpgGaspPresentationAssetMembership& Membership : Profile->AssetMemberships)
	{
		ERpgGaspPresentationAssetTrait Traits = ERpgGaspPresentationAssetTrait::None;
		switch (Membership.Category)
		{
		case ERpgGaspPresentationAssetCategory::GroundMoving:
			Traits = ERpgGaspPresentationAssetTrait::GroundMoving;
			break;
		case ERpgGaspPresentationAssetCategory::JumpStart:
			Traits = ERpgGaspPresentationAssetTrait::Airborne |
				ERpgGaspPresentationAssetTrait::JumpStart;
			break;
		case ERpgGaspPresentationAssetCategory::BackwardJumpStart:
			Traits = ERpgGaspPresentationAssetTrait::Airborne |
				ERpgGaspPresentationAssetTrait::JumpStart |
				ERpgGaspPresentationAssetTrait::BackwardJumpStart;
			break;
		case ERpgGaspPresentationAssetCategory::AirborneFall:
			Traits = ERpgGaspPresentationAssetTrait::Airborne |
				ERpgGaspPresentationAssetTrait::AirborneFall;
			break;
		case ERpgGaspPresentationAssetCategory::Landing:
			Traits = ERpgGaspPresentationAssetTrait::Landing;
			break;
		default:
			break;
		}

		AssetTraits.Add(Membership.Asset.Get(), Traits);
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
