#pragma once

#include "RpgGameplayAbility.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"

#include "RpgGameplayAbility_Dodge.generated.h"

class UAbilityTask_PlayMontageAndWait;

/** Designer tuning applied when an equipment dodge profile selects a semantic root-motion profile name. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgDodgeRootMotionTuning
{
	GENERATED_BODY()

	/** Name referenced by FRpgEquipmentDodgeProfile. None selects the ability's default tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge|Root Motion")
	FName ProfileName = NAME_None;

	/** Montage playback multiplier for this profile. This does not alter global movement speed or stamina. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge|Root Motion", meta = (ClampMin = "0.01", ClampMax = "4.0", UIMin = "0.5", UIMax = "2.0"))
	float MontagePlayRate = 1.0f;

	/** Translation multiplier applied only to root motion extracted from the selected dodge montage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge|Root Motion", meta = (ClampMin = "0.0", ClampMax = "4.0", UIMin = "0.0", UIMax = "2.0"))
	float TranslationScale = 1.0f;

	/** Optional montage section used by this root-motion profile. None starts at the montage default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge|Root Motion")
	FName StartSection = NAME_None;
};

/** Runtime selection resolved once at dodge activation from the controller-owned equipment loadout. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgResolvedDodgeProfile
{
	GENERATED_BODY()

	/** Equipment-load tier that selected this dodge presentation. */
	UPROPERTY(BlueprintReadOnly, Category = "Dodge")
	ERpgEquipmentLoadTier LoadTier = ERpgEquipmentLoadTier::Light;

	/** Tier-specific montage selected by the loadout, or the ability fallback montage. */
	UPROPERTY(BlueprintReadOnly, Category = "Dodge")
	TSoftObjectPtr<UAnimMontage> Montage;

	/** Semantic root-motion profile selected by the current load tier. */
	UPROPERTY(BlueprintReadOnly, Category = "Dodge")
	FName RootMotionProfile = NAME_None;

	/** Resolved montage playback rate for this activation. */
	UPROPERTY(BlueprintReadOnly, Category = "Dodge")
	float MontagePlayRate = 1.0f;

	/** Resolved montage root-motion translation multiplier for this activation. */
	UPROPERTY(BlueprintReadOnly, Category = "Dodge")
	float TranslationScale = 1.0f;

	/** Resolved montage section for this activation. */
	UPROPERTY(BlueprintReadOnly, Category = "Dodge")
	FName StartSection = NAME_None;
};

/**
 * Local-predicted GAS dodge that consumes the authoritative Gear+Carry load tier.
 *
 * The ability changes only dodge montage/root-motion presentation. Existing costs, cooldowns, i-frame effects,
 * stamina attributes, walk speed, and sprint speed remain configured by their existing GAS/character paths.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Equipment Load Dodge Ability"))
class SURVIVALRPG_API URpgGameplayAbility_Dodge : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	explicit URpgGameplayAbility_Dodge(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Returns the profile selected for the current activation, or the default value while inactive. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Ability|Dodge")
	FRpgResolvedDodgeProfile GetResolvedDodgeProfile() const { return ResolvedDodgeProfile; }

	/** Pure lookup used by validation/tests; unmatched or None names use the supplied defaults. */
	static FRpgDodgeRootMotionTuning ResolveRootMotionTuning(
		FName ProfileName,
		TConstArrayView<FRpgDodgeRootMotionTuning> Tunings,
		float DefaultPlayRate = 1.0f,
		float DefaultTranslationScale = 1.0f);

protected:
	//~ UGameplayAbility interface
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;
	//~ End UGameplayAbility interface

	/** Called after tier/profile resolution and before the montage task starts. Cosmetic Blueprint logic only. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rpg|Ability|Dodge", meta = (DisplayName = "On Dodge Profile Selected"))
	void K2_OnDodgeProfileSelected(const FRpgResolvedDodgeProfile& Profile);

	/** Fallback used when the controller has no loadout component or a tier has no montage configured. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Ability|Dodge")
	FRpgEquipmentDodgeProfile DefaultDodgeProfile;

	/** Root-motion tuning records addressed by Light/Medium/Heavy profile names on the loadout component. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Ability|Dodge", meta = (TitleProperty = "ProfileName"))
	TArray<FRpgDodgeRootMotionTuning> RootMotionTunings;

	/** Playback rate used when the selected profile name has no explicit tuning row. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Ability|Dodge", meta = (ClampMin = "0.01", ClampMax = "4.0", UIMin = "0.5", UIMax = "2.0"))
	float DefaultMontagePlayRate = 1.0f;

	/** Root-motion translation scale used when the selected profile name has no explicit tuning row. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Ability|Dodge", meta = (ClampMin = "0.0", ClampMax = "4.0", UIMin = "0.0", UIMax = "2.0"))
	float DefaultRootMotionTranslationScale = 1.0f;

private:
	UFUNCTION()
	void HandleDodgeMontageCompleted();

	UFUNCTION()
	void HandleDodgeMontageInterrupted();

	FRpgResolvedDodgeProfile ResolveDodgeProfile(const FGameplayAbilityActorInfo& ActorInfo) const;
	void FinishCurrentDodge(bool bWasCancelled);

	/** Per-activation profile cached for animation/presentation reads; never replicated or saved. */
	UPROPERTY(Transient)
	FRpgResolvedDodgeProfile ResolvedDodgeProfile;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> ActiveMontageTask = nullptr;
};
