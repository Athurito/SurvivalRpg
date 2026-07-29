#pragma once

#include "Components/SphereComponent.h"
#include "Engine/NetSerialization.h"
#include "GameplayTagContainer.h"

#include "RpgCorpseLifecycleComponent.generated.h"

class URpgCorpseLifecycleComponent;
class URpgCorpseProfile;
class USkeletalMeshComponent;

/** Server-owned lifecycle phase for a dead, interactable actor. */
UENUM(BlueprintType)
enum class ERpgCorpseLifecycleState : uint8
{
	Inactive,
	Dying,
	Settling,
	Available,
	Completed,
	Expiring
};

/**
 * Compact replicated input used to start cosmetic ragdoll simulation on every role.
 * Bone transforms are intentionally not replicated; Revision makes application idempotent.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgCorpseRagdollState
{
	GENERATED_BODY()

	/** Monotonically increasing server revision; zero means no ragdoll has been requested. */
	UPROPERTY(BlueprintReadOnly, Category = "Corpse|Ragdoll")
	int32 Revision = 0;

	/** Quantized server-observed velocity in centimeters per second applied to all simulated bodies. */
	UPROPERTY(BlueprintReadOnly, Category = "Corpse|Ragdoll")
	FVector_NetQuantize10 LinearVelocity = FVector::ZeroVector;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FRpgCorpseAvailabilityChangedNative,
	URpgCorpseLifecycleComponent*,
	bool);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FRpgCorpseAvailabilityChanged,
	URpgCorpseLifecycleComponent*, CorpseComponent,
	bool, bIsAvailable);

/**
 * Replicated, tick-free anchor that owns corpse presentation, completion gates and despawn timers.
 *
 * The server owns all lifecycle transitions. Each role simulates its own skeletal ragdoll from the
 * same replicated starting velocity while interaction and reward authority use this bone-following
 * query sphere rather than a network-identical physics pose.
 */
UCLASS(BlueprintType, ClassGroup = (Rpg), meta = (BlueprintSpawnableComponent, DisplayName = "RPG Corpse Lifecycle"))
class SURVIVALRPG_API URpgCorpseLifecycleComponent : public USphereComponent
{
	GENERATED_BODY()

public:
	explicit URpgCorpseLifecycleComponent(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Records authoritative pre-stop movement and enters the non-interactable death phase. */
	void NotifyDeathStarted(const FVector& AuthoritativeVelocity);

	/** Starts replicated ragdoll, settle and hard-expiration timers. Server authority only. */
	void NotifyDeathFinished();

	/** Updates the inventory-empty completion gate after a committed server inventory mutation. */
	void SetInventoryRequirementComplete(bool bIsComplete);

	/** Completes one configured external gate once; returns false for stale, unknown or client calls. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rpg|Corpse")
	bool CompleteExternalRequirement(FGameplayTag RequirementTag);

	/** True only while interactions may authoritatively start against this corpse. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Corpse")
	bool IsCorpseAvailable() const { return LifecycleState == ERpgCorpseLifecycleState::Available; }

	/** Current replicated phase; UI may read this value but never drive it. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Corpse")
	ERpgCorpseLifecycleState GetLifecycleState() const { return LifecycleState; }

	/** Replicated ragdoll start input, primarily useful to presentation and diagnostics. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Corpse")
	const FRpgCorpseRagdollState& GetRagdollState() const { return RagdollState; }

	/** Interaction/access location that follows the configured corpse bone on this machine. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Corpse")
	FVector GetInteractionWorldLocation() const { return GetComponentLocation(); }

	/** Resolved profile interaction radius in centimeters. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Corpse")
	float GetCorpseInteractionRadius() const;

	/** Remaining hard lifetime in seconds based on synchronized server world time. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Corpse")
	float GetRemainingLifetimeSeconds() const;

	/** Sanitizes, scales and clamps authoritative movement for deterministic ragdoll startup. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Corpse")
	static FVector CalculateRagdollStartVelocity(
		const FVector& AuthoritativeVelocity,
		float VelocityMultiplier,
		float MaximumSpeed);

	/** Native event used by inventory and harvesting features to enable or disable interaction. */
	FRpgCorpseAvailabilityChangedNative& OnCorpseAvailabilityChangedNative()
	{
		return CorpseAvailabilityChangedNative;
	}

	/** Presentation-facing signal emitted when authoritative availability changes. */
	UPROPERTY(BlueprintAssignable, Category = "Rpg|Corpse")
	FRpgCorpseAvailabilityChanged OnCorpseAvailabilityChanged;

	/** Static designer profile. Null uses safe native defaults so legacy enemies remain functional. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Corpse")
	TObjectPtr<URpgCorpseProfile> CorpseProfile;

#if WITH_DEV_AUTOMATION_TESTS
	int32 GetLastAppliedRagdollRevisionForTesting() const { return LastAppliedRagdollRevision; }
	void ApplyReplicatedPresentationForTesting() { ApplyReplicatedPresentation(); }
	void HandleSettleElapsedForTesting() { HandleSettleElapsed(); }
#endif

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	/** Replicated lifecycle phase; OnRep reconstructs interaction collision and late-join presentation. */
	UPROPERTY(ReplicatedUsing = OnRep_LifecycleState, BlueprintReadOnly, Category = "Rpg|Corpse")
	ERpgCorpseLifecycleState LifecycleState = ERpgCorpseLifecycleState::Inactive;

	/** Replicated ragdoll startup input; server increments Revision exactly once per death finish. */
	UPROPERTY(ReplicatedUsing = OnRep_RagdollState, BlueprintReadOnly, Category = "Rpg|Corpse")
	FRpgCorpseRagdollState RagdollState;

	/** Absolute synchronized server time at which the hard corpse lifetime ends. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Rpg|Corpse")
	float ExpirationServerTimeSeconds = 0.0f;

	UFUNCTION()
	void OnRep_LifecycleState(ERpgCorpseLifecycleState PreviousState);

	UFUNCTION()
	void OnRep_RagdollState();

private:
	void AttachAnchorToConfiguredBone();
	USkeletalMeshComponent* ResolveSkeletalMesh() const;
	void ApplyReplicatedPresentation();
	bool ApplyRagdollIfNeeded();
	void SleepRagdollWhenReady();
	void SetLifecycleState(ERpgCorpseLifecycleState NewState);
	void HandleLifecycleStateChanged(ERpgCorpseLifecycleState PreviousState);
	void HandleSettleElapsed();
	void HandleHardLifetimeElapsed();
	void EvaluateCompletionRequirements();
	bool HasCompletionRequirements() const;
	bool AreCompletionRequirementsSatisfied() const;
	void BeginCompletedState();
	void BeginExpiration();
	void DestroyOwnerAuthority();
	float GetSynchronizedServerTimeSeconds() const;

	FName ResolveRagdollBoneName() const;
	FName ResolveAnchorBoneName() const;
	FName ResolveRagdollCollisionProfileName() const;
	float ResolveVelocityMultiplier() const;
	float ResolveMaximumRagdollSpeed() const;
	float ResolveSettleDelaySeconds() const;
	float ResolveEmptyDespawnDelaySeconds() const;
	float ResolveMaximumLifetimeSeconds() const;
	bool RequiresInventoryEmpty() const;
	const FGameplayTagContainer& ResolveRequiredExternalCompletionTags() const;

	FRpgCorpseAvailabilityChangedNative CorpseAvailabilityChangedNative;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> CachedSkeletalMesh;

	FGameplayTagContainer CompletedExternalRequirements;
	FVector CapturedAuthoritativeVelocity = FVector::ZeroVector;
	int32 LastAppliedRagdollRevision = 0;
	float LocalRagdollAppliedWorldTimeSeconds = -1.0f;
	bool bInventoryRequirementComplete = false;
	bool bLastBroadcastAvailability = false;

	FTimerHandle SettleTimerHandle;
	FTimerHandle MaximumLifetimeTimerHandle;
	FTimerHandle CompletedDespawnTimerHandle;
	FTimerHandle ExpirationDestroyTimerHandle;
	FTimerHandle PresentationSettleTimerHandle;
};
