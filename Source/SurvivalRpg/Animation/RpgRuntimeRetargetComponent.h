#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Engine/TimerHandle.h"
#include "RpgRuntimeRetargetComponent.generated.h"

class UIKRetargeter;
class UGameFrameworkComponentManager;
class URpgRuntimeRetargetProfile;
class USkeletalMeshComponent;
struct FActorInitStateChangedParams;

/** Optional local presentation follower. PawnData selects content; Character::GetMesh() retains every gameplay responsibility. */
UCLASS(ClassGroup = (Rpg), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgRuntimeRetargetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URpgRuntimeRetargetComponent();

	/** Current local cosmetic configuration, available before its AnimBP initializes; null means no applied profile. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Retarget")
	const URpgRuntimeRetargetProfile* GetRetargetProfile() const { return ActiveProfile; }

	/** Direct retargeter reference consumed by the presentation AnimBP; does not select or mutate an asset. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Retarget")
	UIKRetargeter* GetRetargeter() const;

	/** Locally created cosmetic follower, or null when disabled. Never use this mesh for GAS, traces, equipment or corpse physics. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Retarget")
	USkeletalMeshComponent* GetRetargetMesh() const { return RetargetMesh; }

	/** Reapplies the static appearance selected by this pawn's replicated PawnData; no skin-selection RPC or saved state is created. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Retarget")
	void RefreshFromPawnData();

	/** Applies a local cosmetic profile only. Null/disabled restores the source; invalid configuration returns false with the source restored. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Retarget")
	bool ApplyProfile(const URpgRuntimeRetargetProfile* Profile);

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnUnregister() override;

private:
	void InitializeFromPawnData();
	void HandlePawnDataAvailable(const FActorInitStateChangedParams& Params);
	void HandleTargetPoseFinalized();
	void RejectPendingPresentation();
	void ClearPresentation();
	void StopListening();

	UPROPERTY(Transient)
	TObjectPtr<const URpgRuntimeRetargetProfile> ActiveProfile;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> RetargetMesh;

	TWeakObjectPtr<USkeletalMeshComponent> SourceMesh;
	TWeakObjectPtr<UGameFrameworkComponentManager> ComponentManager;
	FDelegateHandle PawnDataDelegate;
	FDelegateHandle TargetPoseDelegate;
	FTimerHandle DeferredCleanupHandle;
	uint64 FirstPoseFrame = MAX_uint64;
	EVisibilityBasedAnimTickOption PreviousSourceTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	bool bPreviousSourceURO = false;
	bool bPreviousSourceVisible = true;
	bool bSourceVisibilityOverridden = false;
	bool bProfileApplied = false;
};
