// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "SurvivalRpg/Animation/RpgCombatAnimationProfile.h"
#include "UObject/UObjectGlobals.h"

namespace
{
FGameplayTag RequireTag(const TCHAR* TagName)
{
	return FGameplayTag::RequestGameplayTag(FName(TagName), false);
}

FRpgCombatAnimationPoseProfile MakeProfile(
	FName ProfileName,
	std::initializer_list<FGameplayTag> RequiredTraits,
	std::initializer_list<FGameplayTag> BlockedTraits,
	UAnimSequence* Animation)
{
	FRpgCombatAnimationPoseProfile Result;
	Result.ProfileName = ProfileName;
	for (const FGameplayTag Tag : RequiredTraits)
	{
		Result.RequiredEquipmentTraits.AddTag(Tag);
	}
	for (const FGameplayTag Tag : BlockedTraits)
	{
		Result.BlockedEquipmentTraits.AddTag(Tag);
	}
	Result.EquippedUpperBodyAnimation = Animation;
	Result.CombatReadyUpperBodyAnimation = Animation;
	return Result;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCombatAnimationProfileResolverTest,
	"SurvivalRpg.Animation.Combat.ProfileResolver",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgCombatAnimationProfileResolverTest::RunTest(const FString& Parameters)
{
	const FGameplayTag OneHandSword =
		RequireTag(TEXT("Equipment.AnimationProfile.OneHandSword"));
	const FGameplayTag TwoHandedSword =
		RequireTag(TEXT("Equipment.AnimationProfile.TwoHandedSword"));
	const FGameplayTag Shield =
		RequireTag(TEXT("Equipment.AnimationProfile.Shield"));
	const FGameplayTag UnknownWeaponFamily =
		RequireTag(TEXT("Weapon.Family.Sword"));
	if (!TestTrue(TEXT("One-handed presentation tag exists"), OneHandSword.IsValid()) ||
		!TestTrue(TEXT("Two-handed presentation tag exists"), TwoHandedSword.IsValid()) ||
		!TestTrue(TEXT("Shield presentation tag exists"), Shield.IsValid()) ||
		!TestTrue(TEXT("Unknown-family fixture tag exists"), UnknownWeaponFamily.IsValid()))
	{
		return false;
	}

	UAnimSequence* OneHandAnimation = NewObject<UAnimSequence>();
	UAnimSequence* TwoHandAnimation = NewObject<UAnimSequence>();
	UAnimSequence* SwordShieldAnimation = NewObject<UAnimSequence>();
	URpgCombatAnimationProfile* Profile = NewObject<URpgCombatAnimationProfile>();
	if (!TestNotNull(TEXT("Profile fixture exists"), Profile))
	{
		return false;
	}

	Profile->UnarmedFallback.ProfileName = TEXT("Unarmed");
	Profile->WeaponProfiles = {
		MakeProfile(
			TEXT("OneHandSword"),
			{OneHandSword},
			{TwoHandedSword, Shield},
			OneHandAnimation),
		MakeProfile(
			TEXT("TwoHandedSword"),
			{TwoHandedSword},
			{OneHandSword, Shield},
			TwoHandAnimation),
		MakeProfile(
			TEXT("SwordShield"),
			{OneHandSword, Shield},
			{TwoHandedSword},
			SwordShieldAnimation),
	};

	const FRpgResolvedCombatAnimationProfile EmptyResult =
		Profile->ResolveProfile(FGameplayTagContainer());
	TestTrue(TEXT("Empty loadout resolves Unarmed"), EmptyResult.bIsFallback);
	TestEqual(TEXT("Unarmed fallback keeps its label"), EmptyResult.ProfileName, FName(TEXT("Unarmed")));

	FGameplayTagContainer UnknownTraits;
	UnknownTraits.AddTag(UnknownWeaponFamily);
	const FRpgResolvedCombatAnimationProfile UnknownResult =
		Profile->ResolveProfile(UnknownTraits);
	TestTrue(TEXT("Unknown equipment traits fail closed to Unarmed"), UnknownResult.bIsFallback);

	FGameplayTagContainer OneHandTraits;
	OneHandTraits.AddTag(OneHandSword);
	const FRpgResolvedCombatAnimationProfile OneHandResult =
		Profile->ResolveProfile(OneHandTraits);
	TestFalse(TEXT("One-handed sword resolves a weapon profile"), OneHandResult.bIsFallback);
	TestEqual(TEXT("One-handed sword keeps its authored identity"), OneHandResult.ProfileName, FName(TEXT("OneHandSword")));
	TestEqual(TEXT("One-handed sword resolves its hard animation"), OneHandResult.EquippedUpperBodyAnimation, OneHandAnimation);

	FGameplayTagContainer TwoHandTraits;
	TwoHandTraits.AddTag(TwoHandedSword);
	const FRpgResolvedCombatAnimationProfile TwoHandResult =
		Profile->ResolveProfile(TwoHandTraits);
	TestEqual(TEXT("Two-handed sword is not confused with the shared sword family"), TwoHandResult.ProfileName, FName(TEXT("TwoHandedSword")));
	TestEqual(TEXT("Two-handed sword resolves its own animation"), TwoHandResult.EquippedUpperBodyAnimation, TwoHandAnimation);

	FGameplayTagContainer SwordShieldTraits;
	SwordShieldTraits.AddTag(OneHandSword);
	SwordShieldTraits.AddTag(Shield);
	const FRpgResolvedCombatAnimationProfile SwordShieldResult =
		Profile->ResolveProfile(SwordShieldTraits);
	TestEqual(TEXT("MainHand plus OffHand traits select the composed profile"), SwordShieldResult.ProfileName, FName(TEXT("SwordShield")));
	TestEqual(TEXT("The most-specific composed profile wins"), SwordShieldResult.EquippedUpperBodyAnimation, SwordShieldAnimation);

	URpgCombatAnimationProfile* AmbiguousProfile = NewObject<URpgCombatAnimationProfile>();
	AmbiguousProfile->UnarmedFallback.ProfileName = TEXT("Unarmed");
	AmbiguousProfile->WeaponProfiles = {
		MakeProfile(TEXT("A"), {OneHandSword}, {}, OneHandAnimation),
		MakeProfile(TEXT("B"), {OneHandSword}, {}, TwoHandAnimation),
	};
	const FRpgResolvedCombatAnimationProfile AmbiguousResult =
		AmbiguousProfile->ResolveProfile(OneHandTraits);
	TestTrue(TEXT("Equally specific matches fail closed to Unarmed"), AmbiguousResult.bIsFallback);
	TestTrue(
		TEXT("Validation reports potentially ambiguous equal-specificity profiles"),
		AmbiguousProfile->ValidateProfile().bHasAmbiguousProfiles);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCombatAnimationProfileProviderLifecycleTest,
	"SurvivalRpg.Animation.Combat.FeatureProviderLifecycle",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgCombatAnimationProfileProviderLifecycleTest::RunTest(
	const FString& Parameters)
{
	URpgCombatAnimationProfile* Profile = LoadObject<URpgCombatAnimationProfile>(
		nullptr,
		TEXT("/GF_Combat_Core/Animations/Profiles/DA_RpgCombatAnimationProfile.DA_RpgCombatAnimationProfile"));
	USkeletalMeshComponent* SkeletalMeshComponent =
		NewObject<USkeletalMeshComponent>();
	URpgAnimInstance* AnimInstance =
		NewObject<URpgAnimInstance>(SkeletalMeshComponent);
	if (!TestNotNull(TEXT("The feature-owned combat profile loads"), Profile) ||
		!TestNotNull(TEXT("The skeletal-mesh lifecycle fixture exists"), SkeletalMeshComponent) ||
		!TestNotNull(TEXT("The AnimInstance lifecycle fixture exists"), AnimInstance))
	{
		return false;
	}

	FRpgAnimInstanceProxy Proxy(AnimInstance);
	TestTrue(
		TEXT("The valid feature profile builds a worker-safe lookup"),
		AnimInstance->CombatAnimationProfileLookup.Build(Profile));
	AnimInstance->ActiveCombatAnimationProfileSource = Profile;

	FGameplayTagContainer SwordShieldTraits;
	SwordShieldTraits.AddTag(RequireTag(
		TEXT("Equipment.AnimationProfile.OneHandSword")));
	SwordShieldTraits.AddTag(RequireTag(
		TEXT("Equipment.AnimationProfile.Shield")));
	const FRpgResolvedCombatAnimationProfile SwordShieldProfile =
		AnimInstance->CombatAnimationProfileLookup.Resolve(SwordShieldTraits);
	if (!TestTrue(
		TEXT("The active provider resolves a complete SwordShield overlay"),
		!SwordShieldProfile.bIsFallback && SwordShieldProfile.HasOverlay()))
	{
		return false;
	}

	Proxy.ActiveCombatAnimationProfile = SwordShieldProfile;
	Proxy.PendingCombatAnimationProfile = SwordShieldProfile;
	Proxy.bHasPendingCombatAnimationProfile = true;
	Proxy.CombatEquippedUpperBodyAnimation =
		SwordShieldProfile.EquippedUpperBodyAnimation;
	Proxy.CombatReadyUpperBodyAnimation =
		SwordShieldProfile.CombatReadyUpperBodyAnimation;
	Proxy.CombatAnimationProfileName = SwordShieldProfile.ProfileName;
	Proxy.CombatAnimationOverlayAlpha = 1.0f;
	Proxy.bCombatAnimationReady = true;
	Proxy.bCombatAnimationProfileFallback = false;
	AnimInstance->CombatEquippedUpperBodyAnimation =
		SwordShieldProfile.EquippedUpperBodyAnimation;
	AnimInstance->CombatReadyUpperBodyAnimation =
		SwordShieldProfile.CombatReadyUpperBodyAnimation;
	AnimInstance->CombatAnimationProfileName = SwordShieldProfile.ProfileName;
	AnimInstance->CombatAnimationOverlayAlpha = 1.0f;
	AnimInstance->bCombatAnimationReady = true;
	AnimInstance->bCombatAnimationProfileFallback = false;

	AnimInstance->SynchronizeCombatAnimationProfileProvider(nullptr, Proxy);
	TestNull(
		TEXT("Feature removal releases the GC-strong source profile"),
		AnimInstance->ActiveCombatAnimationProfileSource.Get());
	TestFalse(
		TEXT("Feature removal disables the cached profile lookup"),
		AnimInstance->CombatAnimationProfileLookup.IsEnabled());
	TestEqual(
		TEXT("Feature removal restores the proxy's Unarmed identity"),
		Proxy.CombatAnimationProfileName,
		FName(TEXT("Unarmed")));
	TestNull(
		TEXT("Feature removal clears the proxy's equipped animation pointer"),
		Proxy.CombatEquippedUpperBodyAnimation);
	TestNull(
		TEXT("Feature removal clears the proxy's combat-ready animation pointer"),
		Proxy.CombatReadyUpperBodyAnimation);
	TestEqual(
		TEXT("Feature removal clears the proxy overlay immediately before unload"),
		Proxy.CombatAnimationOverlayAlpha,
		0.0f);
	TestFalse(
		TEXT("Feature removal clears any pending profile transition"),
		Proxy.bHasPendingCombatAnimationProfile);
	TestEqual(
		TEXT("Feature removal restores the public Unarmed identity"),
		AnimInstance->CombatAnimationProfileName,
		FName(TEXT("Unarmed")));
	TestNull(
		TEXT("Feature removal clears the public equipped animation GC reference"),
		AnimInstance->CombatEquippedUpperBodyAnimation.Get());
	TestNull(
		TEXT("Feature removal clears the public ready animation GC reference"),
		AnimInstance->CombatReadyUpperBodyAnimation.Get());
	TestEqual(
		TEXT("Feature removal clears the public overlay alpha"),
		AnimInstance->CombatAnimationOverlayAlpha,
		0.0f);
	TestFalse(
		TEXT("Feature removal clears combat-ready presentation"),
		AnimInstance->bCombatAnimationReady);
	TestTrue(
		TEXT("Feature removal publishes the deterministic fallback flag"),
		AnimInstance->bCombatAnimationProfileFallback);

	AnimInstance->SynchronizeCombatAnimationProfileProvider(nullptr, Proxy);
	TestFalse(
		TEXT("Repeated feature-absent synchronization remains idempotently neutral"),
		AnimInstance->CombatAnimationProfileLookup.IsEnabled());

	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
