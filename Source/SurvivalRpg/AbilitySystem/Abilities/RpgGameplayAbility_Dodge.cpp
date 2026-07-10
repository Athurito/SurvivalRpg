#include "RpgGameplayAbility_Dodge.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameFramework/Controller.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGameplayAbility_Dodge)

URpgGameplayAbility_Dodge::URpgGameplayAbility_Dodge(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationGroup = ERpgAbilityActivationGroup::Exclusive_Replaceable;
	AbilityDisplayName = NSLOCTEXT("RpgAbilities", "EquipmentLoadDodgeName", "Dodge");
	AbilityDescription = NSLOCTEXT(
		"RpgAbilities",
		"EquipmentLoadDodgeDescription",
		"Dodges using the montage and root-motion profile selected by current equipment load.");
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

void URpgGameplayAbility_Dodge::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ResolvedDodgeProfile = ResolveDodgeProfile(*ActorInfo);
	UAnimMontage* Montage = ResolvedDodgeProfile.Montage.LoadSynchronous();
	if (!Montage || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	K2_OnDodgeProfileSelected(ResolvedDodgeProfile);
	ActiveMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		TEXT("EquipmentLoadDodge"),
		Montage,
		ResolvedDodgeProfile.MontagePlayRate,
		ResolvedDodgeProfile.StartSection,
		true,
		ResolvedDodgeProfile.TranslationScale);
	if (!ActiveMontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveMontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleDodgeMontageCompleted);
	ActiveMontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleDodgeMontageInterrupted);
	ActiveMontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleDodgeMontageInterrupted);
	ActiveMontageTask->ReadyForActivation();
}

void URpgGameplayAbility_Dodge::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	ActiveMontageTask = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	ResolvedDodgeProfile = FRpgResolvedDodgeProfile();
}

void URpgGameplayAbility_Dodge::HandleDodgeMontageCompleted()
{
	FinishCurrentDodge(false);
}

void URpgGameplayAbility_Dodge::HandleDodgeMontageInterrupted()
{
	FinishCurrentDodge(true);
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

void URpgGameplayAbility_Dodge::FinishCurrentDodge(bool bWasCancelled)
{
	if (!IsActive())
	{
		return;
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
}
