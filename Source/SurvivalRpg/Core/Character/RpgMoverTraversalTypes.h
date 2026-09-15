#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimMontage.h"
#include "Components/PrimitiveComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "MoverTypes.h"
#include "RootMotionModifier.h"
#include "UObject/StrongObjectPtr.h"
#include "RpgMoverTraversalTypes.generated.h"

struct FRpgMoverTraversalLineage;

/** Discrete traversal lifecycle retained in NetworkPrediction, including a terminal cancellation tombstone. */
UENUM()
enum class ERpgMoverTraversalPhase : uint8 { None, Active, Finished, Cancelled };

/** Immutable physical result of the project's GAS traversal validation; positions are capsule centers in cm. */
USTRUCT()
struct SURVIVALRPG_API FRpgMoverTraversalRequest
{
	GENERATED_BODY()

	/** Validated world-owned obstacle. History observes its lifetime and must not keep a destroyed actor/world alive. */
	UPROPERTY() TWeakObjectPtr<UPrimitiveComponent> Collider;
	/** Obstacle pose validated by this activation. Moving obstacles require a later integration. */
	UPROPERTY() FTransform ColliderTransform = FTransform::Identity;
	/** Collision-safe start and montage exit capsule centers; a Vault exit may be airborne. */
	UPROPERTY() FVector EntryCapsuleLocation = FVector::ZeroVector;
	UPROPERTY() FVector LandingCapsuleLocation = FVector::ZeroVector;
	/** GASP's fixed front ledge target in the gameplay mesh's base-space convention. */
	UPROPERTY() FTransform FrontLedgeTarget = FTransform::Identity;
	/** Optional validated rear ledge for translation-only Vault windows, in the same world/base-space convention. */
	UPROPERTY() FTransform BackLedgeTarget = FTransform::Identity;
	/** Validated designer-authored linear montage and playback interval in seconds. */
	UPROPERTY() TObjectPtr<UAnimMontage> Montage = nullptr;
	UPROPERTY() float StartTimeSeconds = 0.f;
	UPROPERTY() float PlayRate = 1.f;
	UPROPERTY() float HandoffTimeSeconds = 0.f;
	UPROPERTY() FName WarpTargetName = TEXT("FrontLedge");
	/** Name of the optional rear target; None means this montage has no rear ledge warp window. */
	UPROPERTY() FName BackLedgeWarpTargetName = NAME_None;

	void Serialize(FArchive& Ar);
	bool Equals(const FRpgMoverTraversalRequest& Other) const;
};

/** Exact GAS play identity; correlation never grants permission to activate traversal on authority. */
USTRUCT()
struct SURVIVALRPG_API FRpgMoverTraversalIdentity
{
	GENERATED_BODY()
	UPROPERTY() FGameplayAbilitySpecHandle AbilityHandle;
	UPROPERTY() int16 ActivationPredictionKey = 0;
	UPROPERTY() bool bServerInitiatedKey = false;
	UPROPERTY() uint32 MontageSequence = 0;
	bool operator==(const FRpgMoverTraversalIdentity& Other) const;
	void Serialize(FArchive& Ar);
};

/** Local immutable activation/end command. Authority creates its own command after physical validation. */
USTRUCT()
struct SURVIVALRPG_API FRpgMoverTraversalCommand
{
	GENERATED_BODY()
	UPROPERTY() FRpgMoverTraversalIdentity Identity;
	UPROPERTY() ERpgMoverTraversalPhase Phase = ERpgMoverTraversalPhase::None;
	UPROPERTY() FRpgMoverTraversalRequest Context;
	/** Fixed mesh-to-capsule offset captured before visual network smoothing. */
	UPROPERTY() FTransform BaseVisualTransform = FTransform::Identity;
	UPROPERTY() bool bPreserveMomentum = false;
	UPROPERTY() bool bHasRecoveryLocation = false;
	UPROPERTY() FVector RecoveryCapsuleLocation = FVector::ZeroVector;

	bool IsActive() const { return Phase == ERpgMoverTraversalPhase::Active; }
	bool IsTerminal() const { return Phase == ERpgMoverTraversalPhase::Finished || Phase == ERpgMoverTraversalPhase::Cancelled; }
	void Serialize(FArchive& Ar);
	void RetainObjectsForHistory();
	void AddReferencedObjects(FReferenceCollector& Collector);
	bool Equals(const FRpgMoverTraversalCommand& Other) const;
	/** Records actual local predecessors. Weak links expire with their native NP input frames, never with a timer. */
	void RecordPredecessors(const FRpgMoverTraversalCommand& Previous, const FRpgMoverTraversalIdentity& ObservedSyncIdentity);
	bool IsSuccessorOf(const FRpgMoverTraversalIdentity& Other) const;
	/** Releases applied recovery/asset resources while retaining the exact terminal identity and owned target names. */
	void CompactAppliedEnd();

private:
	// Native NP frame buffers are outside UObject GC traversal. Pin asset data, but never world-owned geometry:
	// NP data stores can outlive World::CleanupWorld, and a strong collider reference would retain the old PIE world.
	TStrongObjectPtr<UAnimMontage> MontageLifetime;
	// Local history provenance only: deliberately absent from the client payload and authority sync serialization.
	TSharedPtr<const FRpgMoverTraversalLineage> LocalLineage;
};

/** Complete mutable stock SkewWarp state for one authored notify window. No runtime UObject is shared by frames. */
USTRUCT()
struct SURVIVALRPG_API FRpgMoverWarpModifierState
{
	GENERATED_BODY()
	/** Index in the immutable montage's MotionWarping window list. */
	UPROPERTY() int32 WindowIndex = INDEX_NONE;
	FRootMotionModifier_WarpSwapState Value;
	void Serialize(FArchive& Ar);
	bool Equals(const FRpgMoverWarpModifierState& Other) const;
};

/** Authoritative traversal state corrected and replayed alongside capsule movement in Mover's existing NP backend. */
USTRUCT()
struct SURVIVALRPG_API FRpgMoverTraversalSyncState : public FMoverDataStructBase
{
	GENERATED_BODY()
	/** Approved identity, fixed targets and collision lease; simulated proxies receive this same state. */
	UPROPERTY() FRpgMoverTraversalCommand Command;
	/** Value snapshots after the preceding simulation tick, including stock bone offset and rotation caches. */
	UPROPERTY() TArray<FRpgMoverWarpModifierState> WarpModifiers;
	/** Next extraction position, in montage seconds; corrected authority state wins over today's AnimInstance. */
	UPROPERTY() float MontagePosition = 0.f;
	/** Distinguishes the initial grounded entry from a later movement-mode takeover. */
	UPROPERTY() bool bStartApplied = false;
	/** End/recovery is a one-time simulation effect, not an actor-only teleport or a repeated velocity reset. */
	UPROPERTY() bool bEndApplied = false;

	virtual FMoverDataStructBase* Clone() const override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
	virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
};

template<> struct TStructOpsTypeTraits<FRpgMoverTraversalCommand> : TStructOpsTypeTraitsBase2<FRpgMoverTraversalCommand>
{ enum { WithCopy = true }; };
template<> struct TStructOpsTypeTraits<FRpgMoverWarpModifierState> : TStructOpsTypeTraitsBase2<FRpgMoverWarpModifierState>
{ enum { WithCopy = true }; };
template<> struct TStructOpsTypeTraits<FRpgMoverTraversalSyncState> : TStructOpsTypeTraitsBase2<FRpgMoverTraversalSyncState>
{ enum { WithNetSerializer = true, WithCopy = true }; };
