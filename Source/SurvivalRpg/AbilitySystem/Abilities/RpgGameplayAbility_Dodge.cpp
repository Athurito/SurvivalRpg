#include "RpgGameplayAbility_Dodge.h"

#include "GameFramework/Controller.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGameplayAbility_Dodge)

URpgGameplayAbility_Dodge::URpgGameplayAbility_Dodge(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationGroup = ERpgAbilityActivationGroup::Exclusive_Replaceable;
}

FRpgDodgeRootMotionTuning URpgGameplayAbility_Dodge::ResolveRootMotionTuning(
	FName ProfileName,
	TConstArrayView<FRpgDodgeRootMotionTuning> Tunings,
	float DefaultPlayRate,
	float DefaultTranslationScale)
{
	if (!ProfileName.IsNone())
	{
		for (const FRpgDodgeRootMotionTuning& Tuning : Tunings)
		{
			if (Tuning.ProfileName == ProfileName)
			{
				FRpgDodgeRootMotionTuning Sanitized = Tuning;
				Sanitized.MontagePlayRate = FMath::Max(0.01f, Sanitized.MontagePlayRate);
				Sanitized.TranslationScale = FMath::Max(0.0f, Sanitized.TranslationScale);
				return Sanitized;
			}
		}
	}

	FRpgDodgeRootMotionTuning Fallback;
	Fallback.ProfileName = ProfileName;
	Fallback.MontagePlayRate = FMath::Max(0.01f, DefaultPlayRate);
	Fallback.TranslationScale = FMath::Max(0.0f, DefaultTranslationScale);
	return Fallback;
}

bool URpgGameplayAbility_Dodge::ResolveDodgeProfileForActivation()
{
	if (!IsActive() || bIsAbilityEnding || !CurrentActorInfo || !CurrentActorInfo->AvatarActor.IsValid())
	{
		return false;
	}
	if (!bHasResolvedDodgeProfile)
	{
		ResolvedDodgeProfile = ResolveDodgeProfile(*CurrentActorInfo);
		bHasResolvedDodgeProfile = true;
	}
	return true;
}

void URpgGameplayAbility_Dodge::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (!IsEndAbilityValid(Handle, ActorInfo))
	{
		return;
	}
	if (ScopeLockCount > 0)
	{
		WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &ThisClass::EndAbility,
			Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled));
		return;
	}
	ResolvedDodgeProfile = FRpgResolvedDodgeProfile();
	bHasResolvedDodgeProfile = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

FRpgResolvedDodgeProfile URpgGameplayAbility_Dodge::ResolveDodgeProfile(const FGameplayAbilityActorInfo& ActorInfo) const
{
	FRpgResolvedDodgeProfile Result;
	FRpgEquipmentDodgeProfile EquipmentProfile = DefaultDodgeProfile;
	AController* Controller = ActorInfo.PlayerController.Get();
	if (!Controller)
	{
		Controller = GetControllerFromActorInfo();
	}

	if (Controller)
	{
		if (const URpgEquipmentLoadoutComponent* Loadout = Controller->FindComponentByClass<URpgEquipmentLoadoutComponent>())
		{
			Result.LoadTier = Loadout->GetEquipmentLoadTier();
			const FRpgEquipmentDodgeProfile TierProfile = Loadout->GetDodgeProfileForCurrentLoad();
			if (!TierProfile.Montage.IsNull())
			{
				EquipmentProfile.Montage = TierProfile.Montage;
			}
			EquipmentProfile.RootMotionProfile = TierProfile.RootMotionProfile;
		}
	}

	const FRpgDodgeRootMotionTuning Tuning = ResolveRootMotionTuning(
		EquipmentProfile.RootMotionProfile,
		RootMotionTunings,
		DefaultMontagePlayRate,
		DefaultRootMotionTranslationScale);
	Result.Montage = EquipmentProfile.Montage;
	Result.RootMotionProfile = EquipmentProfile.RootMotionProfile;
	Result.MontagePlayRate = Tuning.MontagePlayRate;
	Result.TranslationScale = Tuning.TranslationScale;
	Result.StartSection = Tuning.StartSection;
	return Result;
}
