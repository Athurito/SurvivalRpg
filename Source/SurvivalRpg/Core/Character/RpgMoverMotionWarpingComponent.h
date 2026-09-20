#pragma once

#include "CoreMinimal.h"
#include "MotionWarpingAdapter.h"
#include "MotionWarpingComponent.h"
#include "RpgMoverTraversalTypes.h"
#include "RpgMoverMotionWarpingComponent.generated.h"

class URpgCharacterMoverComponent;

/** Supplies stock MotionWarping with the simulation capsule and fixed gameplay-mesh offset, including during rollback. */
UCLASS()
class SURVIVALRPG_API URpgMoverMotionWarpingAdapter : public UMotionWarpingBaseAdapter
{
	GENERATED_BODY()
public:
	void SetMover(URpgCharacterMoverComponent* InMover);
	void SetSimulationTransform(const FTransform& ActorTransform, const FTransform& BaseVisualTransform);
	void ClearSimulationTransform();
	virtual AActor* GetActor() const override;
	virtual USkeletalMeshComponent* GetMesh() const override;
	virtual FTransform GetCurrentTransform() const override;
	virtual FVector GetVisualRootLocation() const override;
	virtual FVector GetBaseVisualTranslationOffset() const override;
	virtual FQuat GetBaseVisualRotationOffset() const override;
private:
	FTransform GetBaseVisualTransform() const;
	UPROPERTY(Transient) TObjectPtr<URpgCharacterMoverComponent> Mover;
	FTransform SimulationTransform = FTransform::Identity;
	FTransform SimulationBaseVisual = FTransform::Identity;
	bool bInSimulation = false;
};

/**
 * Scoped stock SkewWarp execution for the synchronous Mover backend. Mutable modifier state belongs to
 * NP sync frames; designer-authored notify templates and GASP translation/rotation/bone math remain unchanged.
 */
UCLASS(ClassGroup = (Movement), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgMoverMotionWarpingComponent : public UMotionWarpingComponent
{
	GENERATED_BODY()
public:
	/** Checks fixed front/optional rear targets and the standard SkewWarp montage contract before GAS playback. */
	bool SupportsTraversal(const FRpgMoverTraversalRequest& Request) const;
	/** Adapter for query-time root-to-warp-point calculations, outside the mutable simulation scope. */
	const UMotionWarpingBaseAdapter* GetTraversalAdapter(URpgCharacterMoverComponent* Mover);
	/** Restores this frame's values and temporarily redirects the engine's root-motion conversion delegates. */
	bool BeginSimulationWarp(URpgCharacterMoverComponent* Mover, FRpgMoverTraversalSyncState& State, const FTransform& ActorTransform);
	/** Captures all mutable modifier values and restores the ordinary component/delegate state. */
	void EndSimulationWarp();
private:
	FTransform ProcessSimulationRootMotion(const FTransform& LocalRootMotion, float DeltaSeconds, const FMotionWarpingUpdateContext* Context);
	FTransform ConvertSimulationRootMotion(const FTransform& WorldRootMotion, float DeltaSeconds, const FMotionWarpingUpdateContext* Context);
	UPROPERTY(Transient) TObjectPtr<URpgMoverMotionWarpingAdapter> SimulationAdapter;
	UPROPERTY(Transient) TObjectPtr<UMotionWarpingBaseAdapter> SavedOwnerAdapter;
	UPROPERTY(Transient) TArray<TObjectPtr<URootMotionModifier>> SavedModifiers;
	UPROPERTY(Transient) TArray<FMotionWarpingTarget> SavedTargets;
	UPROPERTY(Transient) TArray<TObjectPtr<URootMotionModifier>> SimulationModifierCache;
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> CachedMontage;
	TArray<FMotionWarpingWindowData> SimulationWindows;
	TWeakObjectPtr<URpgCharacterMoverComponent> SimulationMover;
	FRpgMoverTraversalSyncState* SimulationState = nullptr;
	FOnWarpLocalspaceRootMotionWithContext SavedLocalDelegate;
	FOnWarpWorldspaceRootMotionWithContext SavedWorldDelegate;
	FTransform WarpedLocalRootMotion = FTransform::Identity;
};
