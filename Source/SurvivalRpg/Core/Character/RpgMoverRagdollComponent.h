#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "RpgMoverRagdollTypes.h"
#include "RpgMoverRagdollComponent.generated.h"

class UGameplayAbility;
class URpgAbilitySystemComponent;
class URpgCharacterMoverComponent;
class USkeletalMeshComponent;
enum class EMoverSmoothingMode : uint8;
namespace EPhysicsTransformUpdateMode { enum Type : int; }
struct FAbilityEndedData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FRpgRagdollPresentationChanged,
	ERpgMoverRagdollPhase, PreviousPhase, ERpgMoverRagdollPhase, CurrentPhase, bool, bForDeath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgGetUpSelectionRequested, UGameplayAbility*, Ability);
DECLARE_MULTICAST_DELEGATE(FRpgRagdollStateChanged);

/**
 * Optional authority/lifecycle seam for the Blueprint-authored living-ragdoll pilot.
 * Uses the pawn's existing PlayerState ASC and HealthComponent. Blueprint owns PhysicsControl,
 * pose snapshots and the source chooser; Mover owns capsule motion and its historical commands.
 */
UCLASS(ClassGroup = (Rpg), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgMoverRagdollComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	URpgMoverRagdollComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Authority-only entry for the current ServerInitiated GAS activation; rejects unsupported movement. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Ragdoll")
	bool BeginRagdoll(UGameplayAbility* Ability);
	/** Requests the Blueprint source chooser on authority while its physical pose is still active. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Ragdoll")
	bool RequestGetUpSelection(UGameplayAbility* Ability);
	/** Authority publishes the source chooser selection for this exact activation, after native validation. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Ragdoll")
	bool PublishGetUpSelection(UGameplayAbility* Ability, UAnimMontage* Montage, float StartTimeSeconds);
	/** Call once after Blueprint creates PhysicsControl bodies/controls and binds presentation events. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Ragdoll")
	bool InitializeRagdollPresentation(bool bControlsCreated);
	/** Current replicated gameplay phase; presentation and animation cannot mutate it. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Ragdoll")
	ERpgMoverRagdollPhase GetRagdollPhase() const { return State.Phase; }
	/** Includes getup until the owning GAS activation ends. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Ragdoll")
	bool IsRagdollActive() const { return State.IsActive() && !bDeathStarted; }
	/** Read-only authoritative selection and activation revision, also valid for late join. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Ragdoll")
	FRpgMoverRagdollState GetRagdollState() const { return State; }
	/** Reports whether the authored PhysicsControl setup has completed. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Ragdoll")
	bool IsRagdollPresentationReady() const { return bPresentationReady; }

	/** Apply source PhysicsControl/snapshot changes only; never write capsule collision or movement mode here. */
	UPROPERTY(BlueprintAssignable, Category = "Rpg|Ragdoll")
	FRpgRagdollPresentationChanged OnRagdollPresentationChanged;
	/** Authority only: capture source PoseHistory and evaluate the source chooser, then publish its selection. */
	UPROPERTY(BlueprintAssignable, Category = "Rpg|Ragdoll")
	FRpgGetUpSelectionRequested OnGetUpSelectionRequested;
	/** Designer-owned source getup whitelist. Arbitrary client-selected montage assets are never accepted. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Ragdoll")
	TArray<TObjectPtr<UAnimMontage>> GetUpMontages;
	/** Maximum capsule speed at entry in cm/s; this pilot starts stationary on supported flat ground. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Ragdoll", meta = (ClampMin = "0", Units = "cm/s"))
	float MaximumEntrySpeed = 5.f;
	/** Minimum time to generate a physical pose before requesting getup, in seconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Ragdoll", meta = (ClampMin = "0", Units = "s"))
	float MinimumRagdollDuration = .35f;

	/** Local readiness/state notification for GAS tasks; does not itself replicate or start playback. */
	FRpgRagdollStateChanged OnStateChanged;
	/** Allows this activation to consume its selection only after physical presentation ownership returns. */
	bool CanConsumeSelection(const UGameplayAbility* Ability) const;
	/** Limits Mover root motion to the current live episode's selected getup. */
	bool AllowsRootMotion(const UAnimMontage* Montage) const;
	/** Whether the montage belongs to this pilot's designer-authored source whitelist. */
	bool IsGetUpMontage(const UAnimMontage* Montage) const;
	/** Gates replicated GAS playback on the matching authority play token and returned mesh ownership. */
	bool IsReadyForReplicatedGetUp(const UAnimMontage* Montage, uint8 PlayId) const;
	/** Captures the actual authority GAS play token only after the configured getup starts successfully. */
	void NotifyGetUpMontageStarted(const UGameplayAbility* Ability, uint8 PlayId);
	/** Existing Health/Death remains terminal even if a selection or ability end arrives later. */
	void HandleMovementDeath();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	/** Returns mesh ownership only after PhysicsControl has made every actual body kinematic. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Server-owned episode/selection command, replicated for owners and late observers; never saved. */
	UPROPERTY(ReplicatedUsing = OnRep_State)
	FRpgMoverRagdollState State;
	/** Local last-applied presentation command; physics and animation never overwrite replicated truth. */
	UPROPERTY(Transient) FRpgMoverRagdollState PresentedState;
	TWeakObjectPtr<URpgAbilitySystemComponent> BoundAbilitySystem;
	TWeakObjectPtr<UGameplayAbility> OwningAbility;
	FDelegateHandle AbilityEndedHandle;
	double RagdollStartTime = 0.0;
	bool bPresentationReady = false;
	bool bDeathStarted = false;
	bool bEndingEpisode = false;
	bool bAvatarDetached = false;
	bool bEndingPlay = false;
	// Local presentation scope: never replicated and never changes the capsule or Mover's base visual offset.
	TWeakObjectPtr<USkeletalMeshComponent> ScopedMesh;
	TWeakObjectPtr<URpgCharacterMoverComponent> ScopedMover;
	FCollisionResponseContainer SavedMeshResponses;
	FName SavedMeshCollisionProfile;
	ECollisionEnabled::Type SavedMeshCollisionEnabled = ECollisionEnabled::NoCollision;
	EPhysicsTransformUpdateMode::Type SavedPhysicsTransformUpdateMode{};
	EMoverSmoothingMode SavedSmoothingMode{};
	bool bSavedMeshSimulatePhysics = false;
	bool bSavedMeshBlendPhysics = false;
	bool bPresentationScopeActive = false;
	bool bPresentationReturnPending = false;
	bool bChangingPresentationScope = false;

	UFUNCTION() void OnRep_State();
	UFUNCTION() void OnDeathStarted(AActor* OwningActor);
	void OnAbilitySystemInitialized();
	void OnAbilitySystemUninitialized();
	void OnAbilityEnded(const FAbilityEndedData& EndedData);
	void EndEpisode(bool bCancelled);
	void ApplyState();
	void ApplyPresentation();
	bool EnterPresentationScope();
	void TryReturnPresentationScope(bool bForEndPlay = false);
	bool IsAlive() const;
	bool ValidateSupport(FRpgMoverRagdollState& Candidate) const;
	URpgCharacterMoverComponent* FindMover() const;
};
