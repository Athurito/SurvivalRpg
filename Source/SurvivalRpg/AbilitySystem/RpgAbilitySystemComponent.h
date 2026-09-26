// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "RpgAbilitySet.h"
#include "Abilities/RpgGameplayAbility.h"
#include "TimerManager.h"
#include "SurvivalRpg/Core/Character/RpgMoverTraversalTypes.h"

#include "RpgAbilitySystemComponent.generated.h"


class URpgAbilityTagRelationshipMapping;
class URpgAbilitySet;
class UGameplayAbility;
class ARpgBasePlayerState;
class UAnimInstance;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
	
	
	
public:
	explicit URpgAbilitySystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	/** Refreshes an unchanged avatar without restarting its GAS montage; replacement avatars/animation instances fully initialize. */
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;
	/** Releases any opt-in Mover montage association before this ASC loses its avatar. */
	virtual void ClearActorInfo() override;
	/** GAS remains the montage playback owner; composed RPG Mover avatars additionally consume its root motion. */
	virtual float PlayMontage(UGameplayAbility* AnimatingAbility, FGameplayAbilityActivationInfo ActivationInfo,
		UAnimMontage* Montage, float InPlayRate, FName StartSectionName = NAME_None, float StartTimeSeconds = 0.0f) override;
	/** Stops the matching Mover root-motion instance as well as the GAS montage, without affecting a replacement. */
	virtual void CurrentMontageStop(float OverrideBlendOutTime = -1.0f) override;
	/** Presents remote Mover traversal on the finalized movement clock; authority and predicting owners retain ordinary GAS playback. */
	void UpdateSimulatedMoverTraversal(const FRpgMoverTraversalSyncState* State, bool bMovementDisabled);
	/** Retries a deferred ordinary getup montage after its replicated PhysicsControl exit is ready. */
	void RefreshReplicatedRagdollMontage();
	
	typedef TFunctionRef<bool(const URpgGameplayAbility* RpgAbility, FGameplayAbilitySpecHandle Handle)> TShouldCancelAbilityFunc;
	void CancelAbilitiesByFunc(TShouldCancelAbilityFunc ShouldCancelFunc, bool bReplicateCancelAbility);
	void CancelInputActivatedAbilities(bool bReplicateCancelAbility);

	void AbilityInputTagPressed(const FGameplayTag& InputTag);
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

	void ProcessAbilityInput(float DeltaTime, bool bGamePaused);
	void ClearAbilityInput();
	
	
	bool IsActivationGroupBlocked(ERpgAbilityActivationGroup Group) const;
	void AddAbilityToActivationGroup(ERpgAbilityActivationGroup Group, URpgGameplayAbility* RpgAbility);
	void RemoveAbilityFromActivationGroup(ERpgAbilityActivationGroup Group, URpgGameplayAbility* RpgAbility);
	void CancelActivationGroupAbilities(ERpgAbilityActivationGroup Group, URpgGameplayAbility* IgnoreRpgAbility, bool bReplicateCancelAbility);

	// Uses a gameplay effect to add the specified dynamic granted tag.
	void AddDynamicTagGameplayEffect(const FGameplayTag& Tag);

	// Removes all active instances of the gameplay effect that was used to add the specified dynamic granted tag.
	void RemoveDynamicTagGameplayEffect(const FGameplayTag& Tag);

	// Adds a replicated loose tag for a fixed duration, replacing any previous timer for the same tag.
	void AddTimedLooseGameplayTag(
		const FGameplayTag& Tag,
		float Duration,
		EGameplayTagReplicationState ReplicationState = EGameplayTagReplicationState::TagAndCountToAll);

	void RemoveTimedLooseGameplayTag(
		const FGameplayTag& Tag,
		EGameplayTagReplicationState ReplicationState = EGameplayTagReplicationState::TagAndCountToAll);

	/** Gets the ability target data associated with the given ability handle and activation info */
	void GetAbilityTargetData(const FGameplayAbilitySpecHandle AbilityHandle, FGameplayAbilityActivationInfo ActivationInfo, FGameplayAbilityTargetDataHandle& OutTargetDataHandle);

	/** Sets the current tag relationship mapping, if null it will clear it out */
	void SetTagRelationshipMapping(URpgAbilityTagRelationshipMapping* NewMapping);
	
	/** Looks at ability tags and gathers additional required and blocking tags */
	void GetAdditionalActivationTagRequirements(const FGameplayTagContainer& AbilityTags, FGameplayTagContainer& OutActivationRequired, FGameplayTagContainer& OutActivationBlocked) const;


	void TryActivateAbilitiesOnSpawn();

protected:
	/** Configured Mover traversal montages are presented from movement history, preventing an earlier GAS receipt clock. */
	virtual void OnRep_ReplicatedAnimMontage() override;
	virtual bool IsReadyForReplicatedMontage() override;
	virtual void AbilitySpecInputPressed(FGameplayAbilitySpec& Spec) override;
	virtual void AbilitySpecInputReleased(FGameplayAbilitySpec& Spec) override;
	
	
	virtual void NotifyAbilityActivated(const FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability) override;
	virtual void NotifyAbilityFailed(const FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, const FGameplayTagContainer& FailureReason) override;
	virtual void NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, bool bWasCancelled) override;
	virtual void ApplyAbilityBlockAndCancelTags(const FGameplayTagContainer& AbilityTags, UGameplayAbility* RequestingAbility, bool bEnableBlockTags, const FGameplayTagContainer& BlockTags, bool bExecuteCancelTags, const FGameplayTagContainer& CancelTags) override;
	virtual void HandleChangeAbilityCanBeCanceled(const FGameplayTagContainer& AbilityTags, UGameplayAbility* RequestingAbility, bool bCanBeCanceled) override;
	
	/** Notify client that an ability failed to activate */
	UFUNCTION(Client, Unreliable)
	void ClientNotifyAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& FailureReason);

	void HandleAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& FailureReason);
	
protected:
	// If set, this table is used to look up tag relationships for activate and cancel
	UPROPERTY()
	TObjectPtr<URpgAbilityTagRelationshipMapping> TagRelationshipMapping;

	// Handles to abilities that had their input pressed this frame.
	TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;

	// Handles to abilities that had their input released this frame.
	TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;

	// Handles to abilities that have their input held.
	TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;

	// Number of abilities running in each activation group.
	int32 ActivationGroupCounts[static_cast<uint8>(ERpgAbilityActivationGroup::MAX)];
	
public:
	
	/** BP-friendly: kann von Client aufgerufen werden, läuft server-autoritatv */
	UFUNCTION(BlueprintCallable, Category="RPG|AbilitySet")
	bool GrantAbilitySet(const URpgAbilitySet* AbilitySet, UObject* SourceObject);

	UFUNCTION(BlueprintCallable, Category="RPG|AbilitySet")
	bool RemoveAbilitySet(const URpgAbilitySet* AbilitySet);

	UFUNCTION(BlueprintCallable, Category="RPG|AbilitySet")
	bool HasAbilitySet(const URpgAbilitySet* AbilitySet) const;

	bool HasGrantAuthority() const;
	
	void ApplyDefaultAbilitySetupIfNeeded(UObject* SourceObject);
	void RemoveDefaultAbilitySetup();
	UFUNCTION(BlueprintCallable, Category="RPG|AbilitySet")
	void ActivateAbilitiesByInputTag(FGameplayTag InputTag, bool bAllowRemoteActivation);

	UFUNCTION(BlueprintCallable, Category="RPG|AbilitySet")
	bool TryActivateFirstAbilityByTag(FGameplayTag ActivationTag, bool bAllowRemoteActivation);

	UFUNCTION(BlueprintCallable, Category="RPG|AbilitySet")
	bool TryActivateFirstAbilityByInputTag(FGameplayTag InputTag, bool bAllowRemoteActivation);

	/** Returns true if the ASC currently has an ability spec identified by the given semantic ability id tag. */
	UFUNCTION(BlueprintCallable, Category="RPG|AbilitySet")
	bool HasAbilityWithAbilityId(FGameplayTag AbilityIdTag) const;

	/** Returns the granted ability CDO identified by AbilityIdTag, used by UI to read static presentation data. */
	UFUNCTION(BlueprintPure, Category="RPG|AbilitySet")
	const URpgGameplayAbility* FindAbilityCDOByAbilityId(FGameplayTag AbilityIdTag) const;

	/** Reads the longest active cooldown matching the ability's cooldown tags. Output is seconds and is UI-read-only. */
	UFUNCTION(BlueprintPure, Category="RPG|AbilitySet")
	bool GetCooldownTimeRemainingAndDurationForAbilityId(FGameplayTag AbilityIdTag, float& OutRemainingTime, float& OutDuration) const;

	/** Adds RuntimeInputTag to the first ability spec matching AbilityIdTag, replacing any previous use of that input tag. Server-authoritative. */
	UFUNCTION(BlueprintCallable, Category="RPG|AbilitySet")
	bool BindInputTagToAbilityId(FGameplayTag AbilityIdTag, FGameplayTag RuntimeInputTag);

	/** Removes RuntimeInputTag from every ability spec. Server-authoritative and used when loadout slots are cleared or rebound. */
	UFUNCTION(BlueprintCallable, Category="RPG|AbilitySet")
	void ClearRuntimeAbilityInputTag(FGameplayTag RuntimeInputTag);

	UFUNCTION(BlueprintCallable, Category="RPG|Lifecycle")
	void ResetForRevive();

	UFUNCTION(BlueprintCallable, Category="RPG|Lifecycle")
	void ResetForRespawn();

	UFUNCTION(BlueprintCallable, Category="RPG|AbilitySet")
	bool TryActivateFirstAbilityByClass(TSubclassOf<UGameplayAbility> AbilityClass, bool bAllowRemoteActivation);

protected:
	virtual void BeginPlay() override;
	
	/** Server-RPCs */
	UFUNCTION(Server, Reliable)
	void Server_GrantAbilitySet(const URpgAbilitySet* AbilitySet, UObject* SourceObject);

	UFUNCTION(Server, Reliable)
	void Server_RemoveAbilitySet(const URpgAbilitySet* AbilitySet);
	
	virtual void OnRep_ActivateAbilities() override;
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability System")
	TObjectPtr<const URpgAbilitySet> DefaultAbilitySetup;
	
	TArray<FGameplayAbilitySpec> LastActiveAbilities;
	
private:
	bool IsSimulatedMoverTraversalMontage(const UAnimMontage* Montage) const;
	void StopPresentedMoverTraversal(const TCHAR* Reason = TEXT("reset"), const FMontageBlendSettings* BlendSettings = nullptr);
	void SetPresentedMoverTraversalPosition(FAnimMontageInstance& Instance, float Position);
	void ResetSimulatedMoverTraversalPresentation();
	// Cosmetic identity is independent of ASC's byte-sized wire play ID and survives the authored blend-out.
	FRpgMoverTraversalIdentity PresentedMoverTraversalIdentity;
	FRpgMoverTraversalIdentity BlockedMoverTraversalIdentity;
	TWeakObjectPtr<UAnimInstance> PresentedMoverTraversalAnimation;
	TWeakObjectPtr<UAnimMontage> PresentedMoverTraversalMontage;
	TWeakObjectPtr<UAnimMontage> LastReceivedMoverTraversalMontage;
	int32 PresentedMoverTraversalInstanceId = INDEX_NONE;
	float PresentedMoverTraversalNotifyPosition = 0.f;
	uint8 LastReceivedMoverTraversalPlayId = 0;
	bool bHasPresentedMoverTraversal = false;
	bool bPresentedMoverTraversalEnded = false;
	bool bHasBlockedMoverTraversal = false;
	bool bHasReceivedMoverTraversal = false;
	bool bWaitForNewMoverTraversalPlay = false;

	/** Weak identity of the last initialized animation instance; ActorInfo resolves the mesh's current instance dynamically. */
	TWeakObjectPtr<UAnimInstance> InitializedAnimInstance;

	UPROPERTY(Transient)
	bool bDefaultSetupApplied = false;

	UPROPERTY(Transient)
	FRpgAbilitySet_GrantedHandles DefaultGrantedHandles;
	
	UPROPERTY()
	TMap<TObjectPtr<const URpgAbilitySet>, FRpgAbilitySet_GrantedHandles> GrantedAbilitySets;
	
	UPROPERTY()
	TObjectPtr<ARpgBasePlayerState> OwnerPlayerState = nullptr;

	TMap<FGameplayTag, FTimerHandle> TimedLooseTagTimerHandles;

#if WITH_DEV_AUTOMATION_TESTS
public:
	void SetForceGrantAuthorityForTests(bool bInForceGrantAuthority) { bForceGrantAuthorityForTests = bInForceGrantAuthority; }

private:
	bool bForceGrantAuthorityForTests = false;
#endif

	bool GrantAbilitySet_Internal(const URpgAbilitySet* AbilitySet, UObject* SourceObject);
	bool RemoveAbilitySet_Internal(const URpgAbilitySet* AbilitySet);
	void ClearLifecycleTags();
	void ClearLifecycleEffects();
};
