// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgCombatAnimationProfile.h"

#include "Animation/AnimSequence.h"
#include "Animation/BlendProfile.h"
#include "Animation/Skeleton.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgCombatAnimationProfile)

namespace
{
FRpgResolvedCombatAnimationProfile MakeResolvedProfile(
	const FRpgCombatAnimationPoseProfile& Profile,
	bool bIsFallback)
{
	FRpgResolvedCombatAnimationProfile Result;
	Result.ProfileName = Profile.ProfileName;
	Result.EquippedUpperBodyAnimation = Profile.EquippedUpperBodyAnimation.Get();
	Result.CombatReadyUpperBodyAnimation = Profile.CombatReadyUpperBodyAnimation.Get();
	Result.EquipBlendInTime = Profile.EquipBlendInTime;
	Result.EquipBlendOutTime = Profile.EquipBlendOutTime;
	Result.CombatModeBlendTime = Profile.CombatModeBlendTime;
	Result.bIsFallback = bIsFallback;
	return Result;
}

bool IsFiniteNonNegative(float Value)
{
	return FMath::IsFinite(Value) && Value >= 0.0f;
}

bool HasValidBlendTimes(const FRpgCombatAnimationPoseProfile& Profile)
{
	return IsFiniteNonNegative(Profile.EquipBlendInTime) &&
		IsFiniteNonNegative(Profile.EquipBlendOutTime) &&
		IsFiniteNonNegative(Profile.CombatModeBlendTime);
}

bool IsForbiddenCombatAnimationPackageName(const FString& PackageName)
{
	const FString LowerPackageName = PackageName.ToLower();
	if (LowerPackageName.StartsWith(TEXT("/game/")) &&
		!LowerPackageName.StartsWith(TEXT("/game/survivalrpg/")))
	{
		return true;
	}

	static const TCHAR* const ForbiddenMarkers[] = {
		TEXT("sandboxcharacter"),
		TEXT("bpi_sandbox"),
		TEXT("experimentalstatemachine"),
		TEXT("psd_sm_"),
		TEXT("/traversal/"),
		TEXT("/locomotor/"),
		TEXT("/mover/"),
		TEXT("foley"),
	};

	for (const TCHAR* Marker : ForbiddenMarkers)
	{
		if (LowerPackageName.Contains(Marker))
		{
			return true;
		}
	}
	return false;
}

bool IsForbiddenCombatAnimationDependency(const UAnimSequence* Animation)
{
	if (!Animation)
	{
		return false;
	}

	if (IsForbiddenCombatAnimationPackageName(Animation->GetOutermost()->GetName()))
	{
		return true;
	}

#if WITH_EDITOR
	const TSoftObjectPtr<USkeletalMesh>& RetargetSourceAsset =
		Animation->GetRetargetSourceAsset();
	if (!RetargetSourceAsset.IsNull() &&
		IsForbiddenCombatAnimationPackageName(
			RetargetSourceAsset.ToSoftObjectPath().GetLongPackageName()))
	{
		return true;
	}
#endif

	return false;
}

void ValidateAnimation(
	const UAnimSequence* Animation,
	const USkeleton* TargetSkeleton,
	FRpgCombatAnimationProfileValidation& Validation)
{
	if (!Animation)
	{
		Validation.bHasMissingAnimation = true;
		return;
	}

	Validation.bHasSkeletonMismatch |=
		TargetSkeleton && Animation->GetSkeleton() != TargetSkeleton;
	Validation.bHasAdditiveAnimation |= Animation->GetAdditiveAnimType() != AAT_None;
	Validation.bHasRootMotionAnimation |= Animation->HasRootMotion();
	Validation.bHasNonLoopingAnimation |= !Animation->bLoop;
	Validation.bHasInvalidAnimationLength |=
		!FMath::IsFinite(Animation->GetPlayLength()) ||
		Animation->GetPlayLength() <= 0.0f;
	Validation.bHasForbiddenDependency |=
		IsForbiddenCombatAnimationDependency(Animation);
}

bool CouldMatchSameLoadout(
	const FRpgCombatAnimationPoseProfile& A,
	const FRpgCombatAnimationPoseProfile& B)
{
	return !A.BlockedEquipmentTraits.HasAny(B.RequiredEquipmentTraits) &&
		!B.BlockedEquipmentTraits.HasAny(A.RequiredEquipmentTraits);
}

template <typename EntryType, typename MatchPredicate, typename ResolvePredicate>
FRpgResolvedCombatAnimationProfile ResolveMostSpecific(
	TConstArrayView<EntryType> Entries,
	const FGameplayTagContainer& EquipmentTraits,
	const FRpgResolvedCombatAnimationProfile& Fallback,
	MatchPredicate&& Matches,
	ResolvePredicate&& Resolve)
{
	const EntryType* BestEntry = nullptr;
	int32 BestSpecificity = INDEX_NONE;
	bool bHasTie = false;
	for (const EntryType& Entry : Entries)
	{
		if (!Matches(Entry, EquipmentTraits))
		{
			continue;
		}

		const int32 Specificity = Entry.RequiredEquipmentTraits.Num();
		if (Specificity > BestSpecificity)
		{
			BestEntry = &Entry;
			BestSpecificity = Specificity;
			bHasTie = false;
		}
		else if (Specificity == BestSpecificity)
		{
			bHasTie = true;
		}
	}

	return BestEntry && !bHasTie ? Resolve(*BestEntry) : Fallback;
}
}

bool FRpgCombatAnimationPoseProfile::Matches(
	const FGameplayTagContainer& EquipmentTraits) const
{
	return EquipmentTraits.HasAll(RequiredEquipmentTraits) &&
		!EquipmentTraits.HasAny(BlockedEquipmentTraits);
}

FRpgResolvedCombatAnimationProfile URpgCombatAnimationProfile::ResolveProfile(
	const FGameplayTagContainer& EquipmentTraits) const
{
	const FRpgResolvedCombatAnimationProfile Fallback =
		MakeResolvedProfile(UnarmedFallback, true);
	return ResolveMostSpecific(
		MakeArrayView(WeaponProfiles),
		EquipmentTraits,
		Fallback,
		[](const FRpgCombatAnimationPoseProfile& Entry, const FGameplayTagContainer& Traits)
		{
			return Entry.Matches(Traits);
		},
		[](const FRpgCombatAnimationPoseProfile& Entry)
		{
			return MakeResolvedProfile(Entry, false);
		});
}

FRpgCombatAnimationProfileValidation URpgCombatAnimationProfile::ValidateProfile() const
{
	FRpgCombatAnimationProfileValidation Validation;
	Validation.bHasMissingTargetSkeleton = TargetSkeleton == nullptr;
	Validation.bHasMissingUpperBodyBlendMask = UpperBodyBlendMaskName.IsNone();
	Validation.bHasMissingDefaultSlot =
		TargetSkeleton && !TargetSkeleton->ContainsSlotName(TEXT("DefaultSlot"));
	if (TargetSkeleton && !UpperBodyBlendMaskName.IsNone())
	{
		const UBlendProfile* BlendMask =
			TargetSkeleton->GetBlendProfile(UpperBodyBlendMaskName);
		Validation.bHasMissingUpperBodyBlendMask =
			!BlendMask || !BlendMask->IsBlendMask();
		if (BlendMask)
		{
			bool bHasPositiveWeight = false;
			bool bHasInvalidWeight = false;
			for (int32 EntryIndex = 0;
				EntryIndex < BlendMask->GetNumBlendEntries();
				++EntryIndex)
			{
				const float Weight = BlendMask->GetEntryBlendScale(EntryIndex);
				bHasPositiveWeight |= Weight > UE_KINDA_SMALL_NUMBER;
				bHasInvalidWeight |=
					!FMath::IsFinite(Weight) || Weight < 0.0f || Weight > 1.0f;
			}
			Validation.bHasInvalidUpperBodyBlendMask =
				BlendMask->GetSkeleton() != TargetSkeleton ||
				BlendMask->GetNumBlendEntries() <= 0 ||
				!bHasPositiveWeight ||
				bHasInvalidWeight;
		}
	}

	Validation.bHasInvalidFallback =
		UnarmedFallback.ProfileName.IsNone() ||
		!UnarmedFallback.RequiredEquipmentTraits.IsEmpty() ||
		!UnarmedFallback.BlockedEquipmentTraits.IsEmpty() ||
		(UnarmedFallback.EquippedUpperBodyAnimation == nullptr) !=
			(UnarmedFallback.CombatReadyUpperBodyAnimation == nullptr);
	Validation.bHasInvalidBlendTime |= !HasValidBlendTimes(UnarmedFallback);
	if (UnarmedFallback.EquippedUpperBodyAnimation ||
		UnarmedFallback.CombatReadyUpperBodyAnimation)
	{
		ValidateAnimation(
			UnarmedFallback.EquippedUpperBodyAnimation,
			TargetSkeleton,
			Validation);
		ValidateAnimation(
			UnarmedFallback.CombatReadyUpperBodyAnimation,
			TargetSkeleton,
			Validation);
	}

	Validation.bHasNoWeaponProfiles = WeaponProfiles.IsEmpty();
	TSet<FName> SeenProfileNames;
	for (const FRpgCombatAnimationPoseProfile& Profile : WeaponProfiles)
	{
		Validation.bHasInvalidProfileName |= Profile.ProfileName.IsNone();
		Validation.bHasDuplicateProfileName |= SeenProfileNames.Contains(Profile.ProfileName);
		SeenProfileNames.Add(Profile.ProfileName);
		Validation.bHasEmptyRequiredTraits |= Profile.RequiredEquipmentTraits.IsEmpty();
		Validation.bHasConflictingTraits |=
			Profile.RequiredEquipmentTraits.HasAny(Profile.BlockedEquipmentTraits);
		Validation.bHasInvalidBlendTime |= !HasValidBlendTimes(Profile);
		ValidateAnimation(Profile.EquippedUpperBodyAnimation, TargetSkeleton, Validation);
		ValidateAnimation(Profile.CombatReadyUpperBodyAnimation, TargetSkeleton, Validation);
	}

	for (int32 AIndex = 0; AIndex < WeaponProfiles.Num(); ++AIndex)
	{
		for (int32 BIndex = AIndex + 1; BIndex < WeaponProfiles.Num(); ++BIndex)
		{
			const FRpgCombatAnimationPoseProfile& A = WeaponProfiles[AIndex];
			const FRpgCombatAnimationPoseProfile& B = WeaponProfiles[BIndex];
			Validation.bHasAmbiguousProfiles |=
				A.GetSpecificity() == B.GetSpecificity() &&
				CouldMatchSameLoadout(A, B);
		}
	}
	return Validation;
}

#if WITH_EDITOR
EDataValidationResult URpgCombatAnimationProfile::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(
		Super::IsDataValid(Context),
		EDataValidationResult::Valid);
	const FRpgCombatAnimationProfileValidation Validation = ValidateProfile();
	if (Validation.bHasMissingTargetSkeleton)
	{
		Context.AddError(FText::FromString(
			TEXT("Combat animation profile requires one target skeleton.")));
	}
	if (Validation.bHasMissingUpperBodyBlendMask)
	{
		Context.AddError(FText::FromString(
			TEXT("Combat animation profile requires a valid project-owned BlendMask on its target skeleton.")));
	}
	if (Validation.bHasInvalidUpperBodyBlendMask)
	{
		Context.AddError(FText::FromString(
			TEXT("The combat upper-body BlendMask must belong to the target skeleton and contain finite normalized non-zero weights.")));
	}
	if (Validation.bHasMissingDefaultSlot)
	{
		Context.AddError(FText::FromString(
			TEXT("The combat animation target skeleton must contain the authoritative DefaultSlot.")));
	}
	if (Validation.bHasInvalidFallback)
	{
		Context.AddError(FText::FromString(
			TEXT("The Unarmed fallback needs a name, no match traits, and either zero or two overlay animations.")));
	}
	if (Validation.bHasNoWeaponProfiles)
	{
		Context.AddError(FText::FromString(
			TEXT("Combat animation profile requires at least one weapon/loadout profile.")));
	}
	if (Validation.bHasInvalidProfileName || Validation.bHasDuplicateProfileName)
	{
		Context.AddError(FText::FromString(
			TEXT("Every combat animation profile entry requires one unique non-empty ProfileName.")));
	}
	if (Validation.bHasEmptyRequiredTraits || Validation.bHasConflictingTraits)
	{
		Context.AddError(FText::FromString(
			TEXT("Weapon profiles need required equipment traits that do not overlap their blocked traits.")));
	}
	if (Validation.bHasAmbiguousProfiles)
	{
		Context.AddError(FText::FromString(
			TEXT("Equally specific combat animation profiles must block one another when they could match the same loadout.")));
	}
	if (Validation.bHasMissingAnimation)
	{
		Context.AddError(FText::FromString(
			TEXT("Every weapon combat animation profile requires equipped and combat-ready upper-body animations.")));
	}
	if (Validation.bHasSkeletonMismatch)
	{
		Context.AddError(FText::FromString(
			TEXT("Every combat upper-body animation must use the profile target skeleton.")));
	}
	if (Validation.bHasAdditiveAnimation)
	{
		Context.AddError(FText::FromString(
			TEXT("Combat upper-body profile animations must be non-additive sequences.")));
	}
	if (Validation.bHasRootMotionAnimation)
	{
		Context.AddError(FText::FromString(
			TEXT("Combat upper-body profile animations must not enable root motion.")));
	}
	if (Validation.bHasNonLoopingAnimation)
	{
		Context.AddError(FText::FromString(
			TEXT("Combat upper-body profile animations must be authored as looping sequences.")));
	}
	if (Validation.bHasInvalidAnimationLength)
	{
		Context.AddError(FText::FromString(
			TEXT("Combat upper-body profile animations must have a finite positive sequence length.")));
	}
	if (Validation.bHasForbiddenDependency)
	{
		Context.AddError(FText::FromString(
			TEXT("Combat upper-body profiles must not depend on excluded sample stacks or foreign retarget-source assets.")));
	}
	if (Validation.bHasInvalidBlendTime)
	{
		Context.AddError(FText::FromString(
			TEXT("Combat animation blend times must be finite and non-negative.")));
	}
	if (!Validation.IsValid())
	{
		Result = CombineDataValidationResults(
			Result,
			EDataValidationResult::Invalid);
	}
	return Result;
}
#endif

bool FRpgCombatAnimationProfileLookup::Build(
	const URpgCombatAnimationProfile* Profile)
{
	Reset();
	if (!Profile || !Profile->ValidateProfile().IsValid())
	{
		return false;
	}

	FallbackProfile = MakeResolvedProfile(Profile->UnarmedFallback, true);
	Entries.Reserve(Profile->WeaponProfiles.Num());
	for (const FRpgCombatAnimationPoseProfile& Source : Profile->WeaponProfiles)
	{
		FRuntimeEntry& RuntimeEntry = Entries.AddDefaulted_GetRef();
		RuntimeEntry.RequiredEquipmentTraits = Source.RequiredEquipmentTraits;
		RuntimeEntry.BlockedEquipmentTraits = Source.BlockedEquipmentTraits;
		RuntimeEntry.Profile = MakeResolvedProfile(Source, false);
	}
	return true;
}

void FRpgCombatAnimationProfileLookup::Reset()
{
	FallbackProfile = FRpgResolvedCombatAnimationProfile();
	FallbackProfile.ProfileName = TEXT("Unarmed");
	Entries.Reset();
}

FRpgResolvedCombatAnimationProfile FRpgCombatAnimationProfileLookup::Resolve(
	const FGameplayTagContainer& EquipmentTraits) const
{
	return ResolveMostSpecific(
		MakeArrayView(Entries),
		EquipmentTraits,
		FallbackProfile,
		[](const FRuntimeEntry& Entry, const FGameplayTagContainer& Traits)
		{
			return Traits.HasAll(Entry.RequiredEquipmentTraits) &&
				!Traits.HasAny(Entry.BlockedEquipmentTraits);
		},
		[](const FRuntimeEntry& Entry)
		{
			return Entry.Profile;
		});
}
