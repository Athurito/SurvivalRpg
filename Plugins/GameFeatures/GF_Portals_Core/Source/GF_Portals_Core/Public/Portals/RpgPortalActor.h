#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/OnlineReplStructs.h"
#include "GameplayEffectTypes.h"
#include "SurvivalRpg/Interaction/IInteractableTarget.h"
#include "RpgPortalActor.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UStaticMeshComponent;
class USphereComponent;
class ULevelStreamingDynamic;
class ARpgPortalExitActor;
class ARpgPortalRealmMarkerActor;
class ARpgPortalRealmEventDirector;
class URpgGameplayAbility_ClosePortal;
class URpgGameplayAbility_EnterPortal;
class URpgPortalEncounterDefinition;
class URpgPortalTravelComponent;
class AController;
enum class ERpgPortalTravelState : uint8;
struct FRpgCombatActorKilledMessage;

/**
 * Runtime-only per-player realm participation data for one portal instance.
 *
 * This intentionally is not replicated or saved. It lets the live server recover
 * a disconnected/reconnected player into the same streamed realm instance while
 * the portal is still alive and not closed.
 */
struct FRpgPortalRealmParticipantState
{
	FUniqueNetIdRepl PlayerNetId;
	FTransform LastSafeRealmTransform = FTransform::Identity;
	TWeakObjectPtr<UAbilitySystemComponent> RealmAbilitySystem;
	TArray<FActiveGameplayEffectHandle> ActiveRealmEffectHandles;
	FActiveGameplayEffectHandle ActiveRealmTagEffectHandle;
	bool bHasLastSafeRealmTransform = false;
	bool bInsideRealm = false;
	bool bResumeAllowed = false;
};

UENUM(BlueprintType)
enum class ERpgPortalState : uint8
{
	/** Encounter has not started yet and can still be configured by a spawner. */
	Dormant,

	/** Encounter is available in the overworld. Realm portals can be entered from here. */
	Active,

	/** Realm level instance is streaming in; entering players are queued until markers resolve. */
	RealmLoading,

	/** Realm level is loaded and at least the boss phase is active. */
	RealmInProgress,

	/** Realm boss is defeated and the realm exit portal is available. */
	ExitOpen,

	/** Encounter objectives are complete and the overworld portal can be closed. */
	Sealable,

	/** Portal has been closed and no longer offers interactions. */
	Closed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgPortalStateChanged, ERpgPortalState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRpgPortalStabilityChanged, float, CurrentStability, float, MaxStability);

/**
 * Runtime portal encounter actor.
 *
 * The actor owns authoritative encounter state, exposes Lyra-style interaction
 * options, streams realm level instances when needed, tracks kills/occupants,
 * and broadcasts completion for later reward or world-state systems.
 */
UCLASS(Blueprintable)
class GF_PORTALS_CORE_API ARpgPortalActor : public AActor, public IInteractableTarget
{
	GENERATED_BODY()

public:
	ARpgPortalActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& InteractionBuilder) override;
	virtual void CustomizeInteractionEventData(const FGameplayTag& InteractionEventTag, FGameplayEventData& InOutEventData) override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Portal")
	void StartEncounter();

	/** Assigns the encounter definition before BeginPlay/StartEncounter; used by GameFeature spawners. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Portal|Encounter")
	void ConfigureEncounterDefinition(const URpgPortalEncounterDefinition* InEncounterDefinition);

	/** Sets the technical level-instance transform before BeginPlay; gameplay positions still come from realm markers. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Portal|Encounter")
	void ConfigureRealmLevelInstanceTransform(const FTransform& InRealmLevelInstanceTransform);

	/** Attempts to close a sealable portal and broadcast the completion message. Server-only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Portal")
	bool TryClosePortal(AActor* ClosingActor);

	/** Attempts to enter a realm portal, streaming the realm first if necessary. Server-only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Portal")
	bool TryEnterPortal(AActor* EnteringActor);

	/** Attempts to leave a completed realm through its spawned exit portal. Server-only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Portal")
	bool TryExitPortal(AActor* ExitingActor);

	/** Completes a client-ready travel request after the owning connection has confirmed realm visibility. */
	bool CompletePortalTravel(URpgPortalTravelComponent* TravelComponent, int32 RequestId);

	/** Cleans up a pending travel request that failed before teleporting into the realm. */
	void HandlePortalTravelFailed(URpgPortalTravelComponent* TravelComponent, int32 RequestId);

	/** Restores a reconnecting controller if this live portal still has resume data for its player. */
	bool TryRestoreReconnectController(AController* Controller);

	UFUNCTION(BlueprintPure, Category = "Portal")
	ERpgPortalState GetPortalState() const { return PortalState; }

	UFUNCTION(BlueprintPure, Category = "Portal|Encounter")
	const URpgPortalEncounterDefinition* GetEncounterDefinition() const { return EncounterDefinition; }

	UFUNCTION(BlueprintPure, Category = "Portal")
	float GetCurrentStability() const { return CurrentStability; }

	UFUNCTION(BlueprintPure, Category = "Portal")
	float GetMaxStability() const;

	UFUNCTION(BlueprintPure, Category = "Portal")
	int32 GetDefeatedTrackedEnemyCount() const { return DefeatedTrackedEnemyCount; }

	UFUNCTION(BlueprintPure, Category = "Portal")
	int32 GetTotalTrackedEnemyCount() const { return TotalTrackedEnemyCount; }

	UFUNCTION(BlueprintPure, Category = "Portal")
	int32 GetRemainingTrackedEnemyCount() const;

	UFUNCTION(BlueprintPure, Category = "Portal")
	bool IsSealable() const { return PortalState == ERpgPortalState::Sealable; }

	UFUNCTION(BlueprintPure, Category = "Portal")
	bool IsExitOpen() const { return PortalState == ERpgPortalState::ExitOpen || PortalState == ERpgPortalState::Sealable; }

	UFUNCTION(BlueprintPure, Category = "Portal|Interaction")
	FText GetExitInteractionText() const;

	UFUNCTION(BlueprintPure, Category = "Portal|Interaction")
	FText GetExitInteractionSubText() const;

	/** Broadcast locally when the portal changes interaction/encounter phase. */
	UPROPERTY(BlueprintAssignable, Category = "Portal")
	FRpgPortalStateChanged OnPortalStateChanged;

	/** Broadcast locally when replicated/current stability changes. */
	UPROPERTY(BlueprintAssignable, Category = "Portal")
	FRpgPortalStabilityChanged OnPortalStabilityChanged;

protected:
	UFUNCTION()
	void OnRep_PortalState();

	UFUNCTION()
	void OnRep_CurrentStability();

	UFUNCTION()
	void OnRep_EncounterDefinition();

	UFUNCTION()
	void OnRep_RealmLevelStreamingConfig();

	UFUNCTION()
	void HandleTrackedEnemyDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void HandleRealmOccupantDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void HandleTrackedBossDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void HandleRealmLevelShown();

	void HandleActorKilled(FGameplayTag Channel, const FRpgCombatActorKilledMessage& Message);
	void RegisterCombatMessageListener();
	void UnregisterCombatMessageListener();
	void SpawnEncounterEnemies();
	void StartRealmEncounter();
	void EnsureRealmLevelStreamingConfig();
	bool LoadRealmLevelInstance();
	void UnloadRealmLevelInstance();
	bool ResolveRealmMarkers();
	void ClearRealmMarkers();
	void TeleportPendingRealmEntrants();
	bool BeginPortalTravelForActor(AActor* TravelActor);
	bool TeleportActorToRealm(AActor* TravelActor);
	void PrepareActorForPortalTeleport(AActor* TravelActor) const;
	void RegisterRealmOccupant(AActor* TravelActor);
	void UnregisterRealmOccupant(AActor* TravelActor);
	AController* ResolveTravelController(AActor* TravelActor) const;
	FUniqueNetIdRepl ResolvePlayerNetId(AActor* TravelActor) const;
	FUniqueNetIdRepl ResolvePlayerNetId(AController* Controller) const;
	FRpgPortalRealmParticipantState* FindParticipantState(const FUniqueNetIdRepl& PlayerNetId);
	const FRpgPortalRealmParticipantState* FindParticipantState(const FUniqueNetIdRepl& PlayerNetId) const;
	FRpgPortalRealmParticipantState* FindParticipantStateForActor(AActor* TravelActor);
	FRpgPortalRealmParticipantState* FindOrAddParticipantStateForActor(AActor* TravelActor);
	bool IsResumeStateUsable(const FRpgPortalRealmParticipantState& ParticipantState) const;
	bool GetResumeRealmTransformForActor(AActor* TravelActor, FTransform& OutTransform) const;
	void UpdateParticipantSafeTransform(AActor* TravelActor);
	void MarkParticipantEnteredRealm(AActor* TravelActor);
	void MarkParticipantExitedRealm(AActor* TravelActor);
	void MarkParticipantDiedInRealm(AActor* TravelActor);
	void MarkParticipantDisconnectedFromRealm(AActor* TravelActor);
	void InvalidateParticipantResumeStates();
	void ApplyRealmEnterEffects(AActor* TravelActor, FRpgPortalRealmParticipantState& ParticipantState);
	void ApplyRealmExitEffects(AActor* TravelActor) const;
	void RemoveRealmPersistentEffects(FRpgPortalRealmParticipantState& ParticipantState);
	void RemoveAllRealmPersistentEffects();
	void ApplyGameplayEffectsToActor(AActor* TargetActor, const TArray<TSubclassOf<UGameplayEffect>>& Effects, TArray<FActiveGameplayEffectHandle>* OutHandles = nullptr) const;
	FActiveGameplayEffectHandle ApplyRealmTagsToActor(AActor* TargetActor) const;
	UAbilitySystemComponent* ResolveAbilitySystemComponent(AActor* TargetActor) const;
	void StartParticipantLocationSamplingIfNeeded();
	void StopParticipantLocationSamplingIfIdle();
	void SampleRealmParticipantLocations();
	void NotifyKnownTravelComponentsToUnload(ERpgPortalTravelState TerminalState);
	void SpawnRealmBoss();
	void SpawnRealmEventDirector();
	void DestroyRealmEventDirector();
	void SpawnExitPortal();
	void DestroyExitPortal();
	void MarkTrackedEnemyDefeated(AActor* DefeatedEnemy);
	void MarkTrackedBossDefeated(AActor* DefeatedBoss);
	void RefreshStabilityFromProgress();
	void RefreshRealmOccupantCount();
	void SetPortalState(ERpgPortalState NewState);
	void ApplyClosedPresentation();
	AActor* ResolveTravelActor(AActor* Actor) const;
	FTransform GetDefaultRealmLevelInstanceTransform() const;
	FString GetDefaultRealmLevelInstanceName() const;
	FName GetRealmLevelNetPackageName() const;
	FTransform GetOverworldReturnTransform() const;
	bool IsTrackedEnemy(AActor* Actor) const;
	bool IsRealmEncounterMode() const;
	bool IsBrokenOutbreakMode() const;
	bool ShouldRewardsBeEligible() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	TObjectPtr<USphereComponent> InteractionCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	TObjectPtr<UStaticMeshComponent> PortalMesh;

	/** Data asset that defines this portal's mode, enemies, realm, boss, exit and text. */
	UPROPERTY(ReplicatedUsing = OnRep_EncounterDefinition, EditAnywhere, BlueprintReadOnly, Category = "Portal|Encounter")
	TObjectPtr<const URpgPortalEncounterDefinition> EncounterDefinition;

	/** Interaction ability granted by the interaction system when the portal is sealable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Interaction")
	TSubclassOf<URpgGameplayAbility_ClosePortal> ClosePortalAbilityClass;

	/** Interaction ability granted by the interaction system while the realm portal can be entered. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Interaction")
	TSubclassOf<URpgGameplayAbility_EnterPortal> EnterPortalAbilityClass;

	/** Fallback exit portal class used when the EncounterDefinition does not override it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Realm")
	TSubclassOf<ARpgPortalExitActor> ExitPortalActorClass;

	/** Distance in front of the overworld portal where exiting realm players return. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Realm", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float OverworldReturnDistance = 250.0f;

	/** Starts the encounter on BeginPlay; GameFeature-spawned portals normally keep this enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Encounter")
	bool bAutoStartOnBeginPlay = true;

	/** Replicated source of truth for portal interaction and encounter phase. */
	UPROPERTY(ReplicatedUsing = OnRep_PortalState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Portal")
	ERpgPortalState PortalState = ERpgPortalState::Dormant;

	/** Current stability value. BrokenOutbreak uses defeated enemies; Realm mode fills after boss completion. */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentStability, VisibleInstanceOnly, BlueprintReadOnly, Category = "Portal")
	float CurrentStability = 0.0f;

	/** Number of tracked BrokenOutbreak enemies defeated so far. */
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Portal")
	int32 DefeatedTrackedEnemyCount = 0;

	/** Number of BrokenOutbreak enemies that were actually spawned and tracked. */
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Portal")
	int32 TotalTrackedEnemyCount = 0;

	/** True after the realm boss death message/destruction has been processed. */
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Portal|Realm")
	bool bRealmBossDefeated = false;

	/** Number of tracked players/pawns still inside this portal's realm instance. */
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Portal|Realm")
	int32 RealmOccupantCount = 0;

	/** Technical level-instance placement chosen by the region spawner; not a gameplay marker. */
	UPROPERTY(ReplicatedUsing = OnRep_RealmLevelStreamingConfig, VisibleInstanceOnly, BlueprintReadOnly, Category = "Portal|Realm")
	FTransform RealmLevelInstanceTransform = FTransform::Identity;

	/** Stable level-instance name so server and clients load the same streamed realm instance. */
	UPROPERTY(ReplicatedUsing = OnRep_RealmLevelStreamingConfig, VisibleInstanceOnly, BlueprintReadOnly, Category = "Portal|Realm")
	FString RealmLevelInstanceName;

	/** Live BrokenOutbreak enemies whose deaths advance stability. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> TrackedEnemies;

	/** Live realm boss spawned at the BossSpawn marker. */
	UPROPERTY(Transient)
	TObjectPtr<AActor> TrackedBoss;

	/** Live realm exit portal spawned after boss defeat. */
	UPROPERTY(Transient)
	TObjectPtr<ARpgPortalExitActor> ExitPortalActor;

	/** Optional realm-local director spawned from the encounter definition. */
	UPROPERTY(Transient)
	TObjectPtr<ARpgPortalRealmEventDirector> RealmEventDirector;

	/** Runtime streaming handle for this portal's realm level instance. */
	UPROPERTY(Transient)
	TObjectPtr<ULevelStreamingDynamic> RealmLevelStreaming;

	/** Resolved Entry marker from the loaded realm level. */
	UPROPERTY(Transient)
	TObjectPtr<ARpgPortalRealmMarkerActor> RealmEntryMarker;

	/** Resolved BossSpawn marker from the loaded realm level. */
	UPROPERTY(Transient)
	TObjectPtr<ARpgPortalRealmMarkerActor> RealmBossSpawnMarker;

	/** Resolved ExitPortal marker from the loaded realm level. */
	UPROPERTY(Transient)
	TObjectPtr<ARpgPortalRealmMarkerActor> RealmExitPortalMarker;

	/** Players/pawns currently considered inside this realm instance. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> RealmOccupants;

	/** Players/pawns that interacted while the realm level was still loading. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> PendingRealmEntrants;

	/** Travel components that loaded this realm locally and must be unloaded on exit/close/cancel. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<URpgPortalTravelComponent>> KnownTravelComponents;

	TMap<FUniqueNetIdRepl, FRpgPortalRealmParticipantState> RealmParticipantStates;
	TMap<FObjectKey, FUniqueNetIdRepl> RealmOccupantPlayerNetIds;
	FTimerHandle ParticipantLocationSampleTimerHandle;
	FGameplayMessageListenerHandle ActorKilledListenerHandle;
	int32 NextPortalTravelRequestId = 0;
};
