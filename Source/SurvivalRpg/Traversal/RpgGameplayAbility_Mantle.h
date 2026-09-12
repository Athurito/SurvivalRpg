#pragma once

#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "RpgGameplayAbility_Mantle.generated.h"

class UAnimMontage;
class UMotionWarpingComponent;
class UPrimitiveComponent;
class URpgMantleAnchorComponent;
struct FGameplayAbilityTargetDataHandle;

/** Predicted GAS lifecycle and authoritative geometry validation for designer-authored static mantle entries. */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgGameplayAbility_Mantle : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	URpgGameplayAbility_Mantle(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Designer-selected root-motion montage. The concrete Blueprint plays it with PlayMontageAndWait after activation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Animation")
	TObjectPtr<UAnimMontage> Montage;

	/** Playback multiplier consumed by the concrete Blueprint montage task. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Animation", meta = (ClampMin = "0.1", ClampMax = "3"))
	float PlayRate = 1.0f;

	/** Montage start position in seconds, configured to match the authored entry animation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Animation", meta = (ClampMin = "0", Units = "s"))
	float StartTimeSeconds = 0.0f;

	/** Motion-warping target addressed by the selected montage. This ability exclusively owns it while active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Animation")
	FName WarpTargetName;

	/** Small vertical offset from the authored front ledge, in centimeters, matching the montage's hand target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Animation", meta = (Units = "cm"))
	float LedgeVerticalOffset = 0.5f;

	/** Maximum standing approach speed in centimeters per second for this montage family. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Validation", meta = (ClampMin = "0", Units = "cm/s"))
	float MaxGroundSpeed = 100.0f;

	/** Forward query length in centimeters; accepted range is also bounded by each authored entry. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Validation", meta = (ClampMin = "1", Units = "cm"))
	float CandidateSearchDistance = 200.0f;

	/** Extra centimeters above the ledge for the conservative capsule clearance route. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Validation", meta = (ClampMin = "0", Units = "cm"))
	float PathClearance = 10.0f;

	/** Maximum seconds to wait for the remote client's correlated target-data request before cancellation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Validation", meta = (ClampMin = "0.1", Units = "s"))
	float TargetDataTimeout = 2.0f;

	/** Validates a proposed prepared entry on the current world; no client coordinates are accepted. */
	bool ValidateAnchor(const ACharacter& Character, const URpgMantleAnchorComponent& Anchor) const;

	/** Finds a validated entry reached by the character's ordinary forward traversal query. */
	URpgMantleAnchorComponent* FindCandidate(const ACharacter& Character, FHitResult* OutHit = nullptr) const;

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	bool IsCharacterReady(const ACharacter& Character) const;
	void HandleTargetData(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag);
	void BeginMantle(URpgMantleAnchorComponent& Anchor);
	void CancelCurrentMantle();
	void CleanupMovement();
	bool IsCapsuleClear(const ACharacter& Character, const FVector& Location) const;
	bool RestoreSafeCapsuleLocation(ACharacter& Character) const;

	UFUNCTION()
	void HandleMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);

	UFUNCTION()
	void HandleMovementModeChanged(ACharacter* Character, EMovementMode PreviousMode, uint8 PreviousCustomMode);

	TWeakObjectPtr<ACharacter> ActiveCharacter;
	TWeakObjectPtr<URpgMantleAnchorComponent> ActiveAnchor;
	TWeakObjectPtr<UPrimitiveComponent> IgnoredComponent;
	TWeakObjectPtr<UMotionWarpingComponent> ActiveWarping;
	FTransform AnchorAtActivation;
	FTransform ColliderAtActivation;
	FVector LandingAtActivation = FVector::ZeroVector;
	FVector LastClearLocation = FVector::ZeroVector;
	FVector EntryLocation = FVector::ZeroVector;
	FDelegateHandle TargetDataDelegate;
	FTimerHandle TimeoutHandle;
	bool bOwnsMovement = false;
	bool bEnding = false;
	bool bReceivedTargetData = false;
};
