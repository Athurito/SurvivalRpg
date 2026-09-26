#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimMontage.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "DefaultMovementSet/LayeredMoves/AnimRootMotionLayeredMove.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayPrediction.h"
#include "MoverDataModelTypes.h"
#include "RpgMoverTraversalTypes.h"
#include "RpgMoverRagdollTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "RpgCharacterMoverComponent.generated.h"

class UAbilitySystemComponent;
class UAnimInstance;
class UGameplayAbility;
class UMotionWarpingBaseAdapter;
class URpgAbilitySystemComponent;
class URpgMoverMotionWarpingComponent;

/**
 * Engine montage root motion with a GAS activation identity. The identity survives Mover rollback and
 * distinguishes successive plays of the same asset; skeletal montage instance IDs remain local.
 * Extraction, motion warping, collision movement and simulation timing stay in the engine implementation.
 */
USTRUCT()
struct SURVIVALRPG_API FRpgMoverAbilityRootMotion : public FLayeredMove_AnimRootMotion
{
	GENERATED_BODY()

	/** Granted ability responsible for this move, serialized with the Mover simulation state. */
	UPROPERTY()
	FGameplayAbilitySpecHandle AbilityHandle;

	/** GAS activation key identity; this is correlation data, never authority to activate an ability. */
	UPROPERTY()
	int16 ActivationPredictionKey = 0;

	/** Separates server-created activation keys from client prediction keys with the same integer. */
	UPROPERTY()
	bool bServerInitiatedKey = false;

	/** One-based play number within the activation, keeping repeated uses of one montage distinct. */
	UPROPERTY()
	uint32 MontageSequence = 0;

	virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep,
		const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;
	virtual FLayeredMoveBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
};

template<>
struct TStructOpsTypeTraits<FRpgMoverAbilityRootMotion> : public TStructOpsTypeTraitsBase2<FRpgMoverAbilityRootMotion>
{
	enum { WithCopy = true };
};

/**
 * Local GAS playback sampled into the existing NetworkPrediction input history. Each entry describes
 * one simulation tick, including an empty entry after stop. Playback data is local-only, never sent to authority.
 * Retained input frames can therefore reconstruct starts, cancellations and replays during correction.
 */
USTRUCT()
struct SURVIVALRPG_API FRpgMoverAbilityRootMotionInputs : public FMoverDataStructBase
{
	GENERATED_BODY()

	/** Immutable playback interval for this input frame; a null montage means no GAS root motion. */
	UPROPERTY()
	FRpgMoverAbilityRootMotion RootMotion;

	/** Immutable local validation/end command. Authority never accepts this context from a client payload. */
	UPROPERTY()
	FRpgMoverTraversalCommand Traversal;
	/** Server-approved living-ragdoll command sampled at this local frame; never serialized from client to server. */
	UPROPERTY() FRpgMoverRagdollState Ragdoll;

	/** Pins the sampled montage until all copies of this local input frame have left NP history. */
	void RetainMontageForHistory();

	virtual FMoverDataStructBase* Clone() const override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
	virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;

private:
	// NP's native frame buffer is outside UObject GC traversal. Copy/Clone retain this explicit reference;
	// empty input, network loading, frame overwrite and buffer destruction release it automatically.
	TStrongObjectPtr<UAnimMontage> MontageLifetime;
};

template<>
struct TStructOpsTypeTraits<FRpgMoverAbilityRootMotionInputs> : public TStructOpsTypeTraitsBase2<FRpgMoverAbilityRootMotionInputs>
{
	enum { WithNetSerializer = true, WithCopy = true };
};

/**
 * Opt-in bridge for GAS montages on the game-thread Character Mover backend. GAS owns montage playback,
 * prediction and callbacks; Mover consumes the corresponding root motion and replicates actor movement.
 * Authored movement modes, input and presentation remain on the composed GASP pawn Blueprint.
 */
UCLASS(ClassGroup = (Movement), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgCharacterMoverComponent : public UCharacterMoverComponent
{
	GENERATED_BODY()

public:
	URpgCharacterMoverComponent();
	/** Holds proxy traversal presentation at its last finalized pose while the network interpolation buffer is starved. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	/** Applies the current cosmetic snapshot and bridges ASC avatar binding until PawnExtension exposes that ASC. */
	void RefreshTraversalPresentation(URpgAbilitySystemComponent* AbilitySystem);
	/** Preserves the GASP input producer and appends this tick's local GAS root-motion playback interval. */
	virtual void ProduceInput(int32 DeltaTimeMS, FMoverInputCmdContext* Cmd) override;

	/**
	 * Stops this pawn permanently after its authoritative health lifecycle starts death. Called by authority
	 * and replicated health notifications; the terminal movement mode travels in Mover's existing sync state.
	 * Repeated notifications preserve the original local simulation boundary and any subsequent death montage.
	 */
	void DisableMovementForDeath();

	/** Reports the terminal stop for a particular simulation frame, including historical correction frames. */
	bool IsMovementDisabledForDeath(const FMoverSyncState& SyncState, const FMoverTimeStep& TimeStep) const;

	/** Sanitizes terminal-frame input before GASP and engine handlers, then schedules the authoritative stop. */
	virtual void OnPreSimulate(const FMoverTimeStep& TimeStep, const FMoverTickStartData& StartingData) override;
	virtual void OnPostSimulate(const FMoverTimeStep& TimeStep, const FMoverTickStartData& StartingData, FMoverTickEndData& EndingData) override;

	/** Reserves a validated GAS traversal before montage playback; zero indicates rejection, otherwise exact play sequence. */
	uint32 BeginTraversal(UGameplayAbility* Ability, const FPredictionKey& ActivationKey, const FRpgMoverTraversalRequest& Request);
	/** Ends only this activation's lease. Optional validated recovery is applied in the simulation, in capsule-center world cm. */
	void EndTraversal(FGameplayAbilitySpecHandle Handle, const FPredictionKey& ActivationKey, uint32 LeaseSequence,
		bool bPreserveMomentum, TOptional<FVector> RecoveryCapsuleLocation = {});
	/** Read-only lifecycle/collision ownership for GAS cleanup and world revalidation after finalization. */
	bool OwnsTraversalLease(FGameplayAbilitySpecHandle Handle, const FPredictionKey& ActivationKey, uint32 LeaseSequence) const;
	bool HasTraversalLease() const;
	UPrimitiveComponent* GetTraversalCollider() const;
	/** Uses the same fixed gameplay-mesh base and ground slope rule as the simulation. */
	const UMotionWarpingBaseAdapter* GetTraversalWarpingAdapter() const;
	bool IsTraversalWalkable(const FHitResult& Hit) const;
	/** Opens/closes stock MotionWarping on this exact sync frame; used only by this component's GAS layered move. */
	bool BeginTraversalRootMotion(const FRpgMoverAbilityRootMotion& Move, const FMoverTickStartData& StartState);
	void EndTraversalRootMotion(float MontagePosition);

	/** Checks the supported linear montage and gameplay-mesh contract before GAS starts playback. */
	bool CanPlayAbilityRootMotion(const UAbilitySystemComponent* AbilitySystem, const UAnimMontage* Montage,
		float PlayRate, FName StartSection, float StartTimeSeconds) const;

	/** Adopts an already-playing GAS montage; the authority's GAS play token correlates cosmetic proxy presentation only. */
	bool StartAbilityRootMotion(UAbilitySystemComponent* AbilitySystem, UGameplayAbility* Ability,
		const FPredictionKey& ActivationKey, UAnimMontage* Montage, float PlayRate, uint8 PresentationPlayId);

	/** Stops only the tracked local instance; a callback for an older montage cannot cancel its replacement. */
	void StopAbilityRootMotion(const UAnimInstance* AnimInstance, int32 MontageInstanceId);

	/** Releases the local association when the ASC changes avatar or the component is removed. */
	void ClearAbilityRootMotion();

	/** Grounded stationary entry validation for the opt-in living-ragdoll component. */
	bool CanBeginRagdoll(float MaximumSpeed) const;
	/** Records a server-authored lifecycle revision for subsequent local input frames, without mutating historical frames. */
	void SetRagdollCommand(const FRpgMoverRagdollState& State);

	/** Read-only live-instance check used by authority and by fresh local input sampling. */
	bool IsAbilityRootMotionCurrent(const FRpgMoverAbilityRootMotion& Move) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void UpdateSyncedMontageState(const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState,
		const FMoverAuxStateContext& AuxState) override;

private:
	friend struct FRpgMoverAbilityRootMotion;

	/** Builds the movement contribution for a tick from authority playback or the owner's historical input. */
	UFUNCTION()
	void HandleAbilityRootMotionPreSimulation(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd);

	bool SampleAbilityRootMotion(double SimTimeMs, FRpgMoverAbilityRootMotion& OutMove) const;
	void CaptureTraversalPresentationEnd(const UAnimInstance* Animation, int32 InstanceId);
	void UpdateTraversalPresentation(const FMoverSyncState& SyncState);
	void PrepareTraversalSimulation(const FMoverTimeStep& TimeStep, const FMoverTickStartData& StartingData);
	void PrepareRagdollSimulation(const FMoverTimeStep& TimeStep, const FMoverTickStartData& StartingData);
	void ApplyTraversalCollisionLease(UPrimitiveComponent* Collider);
	/** Publishes only this corrected command's fixed targets and releases targets from the preceding visible state. */
	void UpdateTraversalWarpTargets(const FRpgMoverTraversalRequest* Request);
	const FRpgMoverTraversalCommand& GetVisibleTraversalCommand() const;
	UFUNCTION()
	void HandleTraversalPostFinalize(const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState);

	/** Live GAS command is sampled into input history, while mutable warp state is written only to sync history. */
	UPROPERTY(Transient) FRpgMoverTraversalCommand TraversalCommand;
	UPROPERTY(Transient) FRpgMoverTraversalSyncState TraversalSimulationState;
	UPROPERTY(Transient) FRpgMoverRagdollState RagdollCommand;
	UPROPERTY(Transient) FRpgMoverRagdollSyncState RagdollSimulationState;
	bool bRagdollEnabled = false;
	bool bSuppressMovementForRagdollThisTick = false;
	UPROPERTY(Transient) TObjectPtr<URpgMoverMotionWarpingComponent> TraversalWarping;
	/** Original per-tick input for GASP's animation/conditional blend-out read model, never used to move the leased capsule. */
	UPROPERTY(Transient) FCharacterDefaultInputs TraversalPresentationInputs;
	UPROPERTY(Transient) TObjectPtr<UPrimitiveComponent> LeasedCollisionComponent;
	// Presentation target names owned by this component; release replaced rear/floor targets after corrections or handoff.
	TArray<FName, TInlineAllocator<3>> PublishedTraversalWarpTargets;
	/** Cosmetic-only binding supplied by ASC initialization; weak, avatar-validated and discarded when PawnExtension is ready. */
	TWeakObjectPtr<URpgAbilitySystemComponent> TraversalPresentationAbilitySystem;
	bool bAddedCollisionIgnore = false;
	bool bTraversalRootMotionScope = false;
	bool bTraversalGeometryInvalidThisTick = false;
	bool bRestoreTraversalPresentationInputs = false;

	/** Local source descriptor, sampled into input frames; never populated from a client's network payload. */
	UPROPERTY(Transient)
	FRpgMoverAbilityRootMotion AbilityRootMotion;

	double AbilityRootMotionStartTimeMs = 0.0;
	TWeakObjectPtr<UAnimInstance> AbilityAnimInstance;
	TWeakObjectPtr<UAnimMontage> AbilityMontage;
	FGameplayAbilitySpecHandle LastAbilityHandle;
	FPredictionKey LastActivationKey;
	uint32 LastMontageSequence = 0;
	int32 AbilityMontageInstanceId = INDEX_NONE;

	// Local receipt time is a prediction boundary, not a second replicated death authority. A corrected
	// RpgDead sync state wins even before this time; older living frames may still replay their original inputs.
	double DeathMovementStartTimeMs = 0.0;
	bool bDeathMovementRequested = false;
	bool bSuppressMovementForDeathThisTick = false;
};
