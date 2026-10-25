#pragma once

#include "Abilities/GameplayAbilityTargetTypes.h"
#include "RpgGameplayAbility.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"

#include "RpgGameplayAbility_Dodge.generated.h"

/**
 * Eight-way dodge direction relative to the avatar's facing, clockwise from forward. The owner picks it from its movement
 * input; without input it dodges backward. Dodge montages name one section per direction (see GetDodgeDirectionSectionName).
 */
UENUM(BlueprintType)
enum class ERpgDodgeDirection : uint8
{
	Forward,
	ForwardRight,
	Right,
	BackwardRight,
	Backward,
	BackwardLeft,
	Left,
	ForwardLeft
};

/** Owner-chosen dodge direction, sent to the server with the predicted activation so both play the same section. */
USTRUCT()
struct SURVIVALRPG_API FRpgDodgeDirectionTargetData : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

	/** Direction the owner dodges in; the server accepts any of the eight, like ordinary movement input. */
	UPROPERTY()
	ERpgDodgeDirection Direction = ERpgDodgeDirection::Backward;

	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<> struct TStructOpsTypeTraits<FRpgDodgeDirectionTargetData> : TStructOpsTypeTraitsBase2<FRpgDodgeDirectionTargetData>
{
	enum { WithNetSerializer = true, WithCopy = true };
};

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

	/**
	 * Optional montage for this profile, overriding the loadout tier's and the default montage. Lets a GameFeature ability
	 * map load tiers to its own content without the core controller referencing it. A directional dodge montage holds one
	 * section per ERpgDodgeDirection.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge|Root Motion")
	TSoftObjectPtr<UAnimMontage> Montage;
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

	/** Resolved montage section for this activation: the direction's section once BeginDodge has resolved the direction. */
	UPROPERTY(BlueprintReadOnly, Category = "Dodge")
	FName StartSection = NAME_None;

	/** Direction of this activation, chosen by the owner and shared with the server. */
	UPROPERTY(BlueprintReadOnly, Category = "Dodge")
	ERpgDodgeDirection Direction = ERpgDodgeDirection::Backward;
};

/**
 * Native snapshot of the authoritative Gear+Carry load tier for a predicted dodge activation.
 *
 * BeginDodge resolves the profile and the owner's direction, commits and opens the server's invulnerability window;
 * Blueprint assets own montage playback from OnDodgeReady. Resolving once prevents an equipment change during an
 * activation from changing its selected root-motion profile; native end cleanup releases that snapshot.
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Equipment Load Dodge Ability"))
class SURVIVALRPG_API URpgGameplayAbility_Dodge : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	explicit URpgGameplayAbility_Dodge(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Returns the profile selected for the current activation, or the default value while inactive. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Ability|Dodge")
	FRpgResolvedDodgeProfile GetResolvedDodgeProfile() const { return ResolvedDodgeProfile; }

	/**
	 * Caches this predicted/server activation's equipment profile and holds the avatar's facing for the roll;
	 * rejects an invalid avatar or an inactive/ending activation.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Ability|Dodge")
	bool ResolveDodgeProfileForActivation();

	/**
	 * Starts this activation's dodge: resolves the load profile and the direction, commits the ability, opens the server's
	 * invulnerability window and calls OnDodgeReady to play the montage. The owner picks the direction from its movement
	 * input and sends it with the activation; the server waits up to DirectionTimeout for it and cancels without it. Host
	 * and AI pick it locally. Cancels when the commit fails. Call once from ActivateAbility.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Ability|Dodge")
	void BeginDodge();

	/** Blueprint continuation after the commit, once profile and direction are known; the profile's StartSection is set. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rpg|Ability|Dodge")
	void OnDodgeReady();

	/** Section name a directional dodge montage uses for Direction: F, FR, R, BR, B, BL, L or FL. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Ability|Dodge")
	static FName GetDodgeDirectionSectionName(ERpgDodgeDirection Direction);

	/** Pure direction choice from world-space movement input and facing; input shorter than 0.1 dodges backward. */
	static ERpgDodgeDirection ResolveDodgeDirection(const FVector& MovementInput, const FRotator& Facing);

#if WITH_EDITOR
	/** Every configured dodge montage must hold one section per direction. */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** Pure lookup used by validation/tests; unmatched or None names use the supplied defaults. */
	static FRpgDodgeRootMotionTuning ResolveRootMotionTuning(
		FName ProfileName,
		TConstArrayView<FRpgDodgeRootMotionTuning> Tunings,
		float DefaultPlayRate = 1.0f,
		float DefaultTranslationScale = 1.0f);

protected:
	//~ UGameplayAbility interface
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;
	//~ End UGameplayAbility interface

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

	/**
	 * Montage seconds of invulnerability from the dodge's start, scaled by the resolved play rate. The server adds
	 * State.Evading for this time once the dodge commits; the damage execution ignores hits meanwhile. Zero dodges
	 * without invulnerability. Server-authoritative, replicated as a loose tag.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Ability|Dodge", meta = (ClampMin = "0.0", Units = "s"))
	float EvadeDuration = 0.35f;

	/**
	 * Montage seconds after which the dodge's recovery may be replaced by another exclusive ability, such as an attack or a
	 * block, scaled by the resolved play rate. Before it the dodge blocks them when its activation group is Exclusive_Blocking.
	 * Zero or less never opens the recovery.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Ability|Dodge", meta = (Units = "s"))
	float RecoveryCancelTime = 0.55f;

	/** Seconds the server waits for a remote owner's direction before it cancels the dodge. */
	UPROPERTY(EditDefaultsOnly, Category = "Rpg|Ability|Dodge", meta = (ClampMin = "0.05", Units = "s"))
	float DirectionTimeout = 0.5f;

	/** Rejects activation while the avatar's movement component is not on the ground; dodges are grounded. */
	UPROPERTY(EditDefaultsOnly, Category = "Rpg|Ability|Dodge")
	bool bRequiresGround = true;

private:
	FRpgResolvedDodgeProfile ResolveDodgeProfile(const FGameplayAbilityActorInfo& ActorInfo) const;
	void HandleDirectionTargetData(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag);
	void FinishDodgeDirection(ERpgDodgeDirection Direction);
	void OnDirectionTimedOut();
	void OnRecoveryCancelOpened();
	void ClearDodgeRuntimeState();

	/** Per-activation profile cached for animation/presentation reads; never replicated or saved. */
	UPROPERTY(Transient)
	FRpgResolvedDodgeProfile ResolvedDodgeProfile;

	bool bHasResolvedDodgeProfile = false;
	bool bDirectionResolved = false;
	FDelegateHandle DirectionTargetDataHandle;
	FTimerHandle DirectionTimeoutHandle;
	FTimerHandle RecoveryCancelHandle;
};
