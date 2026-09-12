#pragma once

#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "RpgTraversalQueryComponent.h"
#include "RpgGameplayAbility_Mantle.generated.h"

class UAnimMontage;
class UMotionWarpingComponent;
class UPrimitiveComponent;
struct FGameplayAbilityTargetDataHandle;

/** Predicted animation proposal; the server reconstructs every world-space traversal location itself. */
USTRUCT()
struct SURVIVALRPG_API FRpgMantleTargetData : public FGameplayAbilityTargetData
{
	GENERATED_BODY()
	/** Collider identity only; no client-authored ledge or landing coordinates are serialized. */
	UPROPERTY() TObjectPtr<UPrimitiveComponent> HitComponent = nullptr;
	/** Predicted presentation choice, validated against server-owned chooser eligibility rows. */
	UPROPERTY() TObjectPtr<UAnimMontage> Montage = nullptr;
	/** Predicted entry time in seconds, constrained by the server-owned chooser sampling range. */
	UPROPERTY() float StartTime = 0.0f;

	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<> struct TStructOpsTypeTraits<FRpgMantleTargetData> : TStructOpsTypeTraitsBase2<FRpgMantleTargetData>
{
	enum { WithNetSerializer = true, WithCopy = true };
};

/** Predicted GAS lifecycle and authoritative geometry validation for source GASP mantle queries. */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgGameplayAbility_Mantle : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	URpgGameplayAbility_Mantle(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Runtime-selected root-motion montage. The concrete Blueprint plays it with PlayMontageAndWait after validation. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Mantle|Animation")
	TObjectPtr<UAnimMontage> Montage;

	/** Runtime-selected playback multiplier consumed by the concrete Blueprint montage task. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Mantle|Animation")
	float PlayRate = 1.0f;

	/** Runtime-selected pose-search entry position in seconds, consumed by the concrete Blueprint montage task. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Mantle|Animation", meta = (Units = "s"))
	float StartTimeSeconds = 0.0f;

	/** Motion-warping target addressed by the selected montage. This ability exclusively owns it while active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Animation")
	FName WarpTargetName;

	/** Small vertical offset from the authored front ledge, in centimeters, matching the montage's hand target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Animation", meta = (Units = "cm"))
	float LedgeVerticalOffset = 0.5f;

	/** Maximum independently accepted query reach in centimeters; 400 covers GASP's 350 cm forward sweep plus its 30 cm radius. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Validation", meta = (ClampMin = "1", Units = "cm"))
	float CandidateSearchDistance = 400.0f;

	/** Extra centimeters above the ledge for the conservative capsule clearance route. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Validation", meta = (ClampMin = "0", Units = "cm"))
	float PathClearance = 10.0f;

	/** Maximum seconds to wait for the remote client's correlated target-data request before cancellation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mantle|Validation", meta = (ClampMin = "0.1", Units = "s"))
	float TargetDataTimeout = 2.0f;

	/** Runs the copied GASP query and validates its geometry and animation without activating gameplay. */
	bool FindTraversalCandidate(const ACharacter& Character, FRpgTraversalQueryResult& OutResult) const;

	/** Derives supported feet at the source montage's configured handoff time from its last front-ledge warp and root motion. */
	bool GetMantleLandingLocation(const ACharacter& Character, const FRpgTraversalQueryResult& Result, FVector& OutLocation) const;

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	/** Records gameplay cancellation before montage-task callbacks can report an ordinary blend-out end. */
	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	bool IsCharacterReady(const ACharacter& Character) const;
	void HandleTargetData(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag);
	void ResolveTraversal(const FRpgTraversalQueryResult& Result);
	void LogTraversalRejection(const TCHAR* Stage, const FRpgTraversalQueryResult& Result) const;
	void BeginMantle(const FRpgTraversalQueryResult& Result);
	bool ValidateTraversal(const ACharacter& Character, const FRpgTraversalQueryResult& Result) const;
	void CancelCurrentMantle();
	void CleanupMovement(bool bWasCancelled);
	bool IsCapsuleClear(const ACharacter& Character, const FVector& Location) const;
	bool RestoreSafeCapsuleLocation(ACharacter& Character) const;

	UFUNCTION()
	void HandleMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);

	UFUNCTION()
	void HandleMovementModeChanged(ACharacter* Character, EMovementMode PreviousMode, uint8 PreviousCustomMode);

	TWeakObjectPtr<ACharacter> ActiveCharacter;
	TWeakObjectPtr<UPrimitiveComponent> IgnoredComponent;
	TWeakObjectPtr<UMotionWarpingComponent> ActiveWarping;
	FTransform ColliderAtActivation;
	FVector LandingAtActivation = FVector::ZeroVector;
	FVector LastClearLocation = FVector::ZeroVector;
	FVector EntryLocation = FVector::ZeroVector;
	FDelegateHandle TargetDataDelegate;
	FTimerHandle TimeoutHandle;
	int32 ActiveMontageInstanceId = INDEX_NONE;
	float FinalWarpEndTime = 0.0f;
	bool bOwnsMovement = false;
	bool bEnding = false;
	bool bReceivedTargetData = false;
	bool bGameplayCancellationRequested = false;
};
