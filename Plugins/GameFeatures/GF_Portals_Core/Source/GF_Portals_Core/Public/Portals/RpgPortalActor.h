#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "SurvivalRpg/Interaction/IInteractableTarget.h"
#include "RpgPortalActor.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class ULevelStreamingDynamic;
class ARpgPortalExitActor;
class ARpgPortalDungeonMarkerActor;
class URpgGameplayAbility_ClosePortal;
class URpgGameplayAbility_EnterPortal;
class URpgPortalEncounterDefinition;
class URpgPortalTravelComponent;
enum class ERpgPortalTravelState : uint8;
struct FRpgCombatActorKilledMessage;

UENUM(BlueprintType)
enum class ERpgPortalState : uint8
{
	/** Encounter has not started yet and can still be configured by a spawner. */
	Dormant,

	/** Encounter is available in the overworld. Dungeon portals can be entered from here. */
	Active,

	/** Dungeon level instance is streaming in; entering players are queued until markers resolve. */
	DungeonLoading,

	/** Dungeon level is loaded and at least the boss phase is active. */
	DungeonInProgress,

	/** Dungeon boss is defeated and the dungeon exit portal is available. */
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
 * options, streams dungeon level instances when needed, tracks kills/occupants,
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

	/** Sets the technical level-instance transform before BeginPlay; gameplay positions still come from dungeon markers. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Portal|Encounter")
	void ConfigureDungeonLevelInstanceTransform(const FTransform& InDungeonLevelInstanceTransform);

	/** Attempts to close a sealable portal and broadcast the completion message. Server-only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Portal")
	bool TryClosePortal(AActor* ClosingActor);

	/** Attempts to enter a dungeon portal, streaming the dungeon first if necessary. Server-only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Portal")
	bool TryEnterPortal(AActor* EnteringActor);

	/** Attempts to leave a completed dungeon through its spawned exit portal. Server-only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Portal")
	bool TryExitPortal(AActor* ExitingActor);

	/** Completes a client-ready travel request after the owning connection has confirmed dungeon visibility. */
	bool CompletePortalTravel(URpgPortalTravelComponent* TravelComponent, int32 RequestId);

	/** Cleans up a pending travel request that failed before teleporting into the dungeon. */
	void HandlePortalTravelFailed(URpgPortalTravelComponent* TravelComponent, int32 RequestId);

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
	void OnRep_DungeonLevelStreamingConfig();

	UFUNCTION()
	void HandleTrackedEnemyDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void HandleDungeonOccupantDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void HandleTrackedBossDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void HandleDungeonLevelShown();

	void HandleActorKilled(FGameplayTag Channel, const FRpgCombatActorKilledMessage& Message);
	void RegisterCombatMessageListener();
	void UnregisterCombatMessageListener();
	void SpawnEncounterEnemies();
	void StartDungeonEncounter();
	void EnsureDungeonLevelStreamingConfig();
	bool LoadDungeonLevelInstance();
	void UnloadDungeonLevelInstance();
	bool ResolveDungeonMarkers();
	void ClearDungeonMarkers();
	void TeleportPendingDungeonEntrants();
	bool BeginPortalTravelForActor(AActor* TravelActor);
	bool TeleportActorToDungeon(AActor* TravelActor);
	void PrepareActorForPortalTeleport(AActor* TravelActor) const;
	void RegisterDungeonOccupant(AActor* TravelActor);
	void UnregisterDungeonOccupant(AActor* TravelActor);
	void NotifyKnownTravelComponentsToUnload(ERpgPortalTravelState TerminalState);
	void SpawnDungeonBoss();
	void SpawnExitPortal();
	void DestroyExitPortal();
	void MarkTrackedEnemyDefeated(AActor* DefeatedEnemy);
	void MarkTrackedBossDefeated(AActor* DefeatedBoss);
	void RefreshStabilityFromProgress();
	void RefreshDungeonOccupantCount();
	void SetPortalState(ERpgPortalState NewState);
	void ApplyClosedPresentation();
	AActor* ResolveTravelActor(AActor* Actor) const;
	FTransform GetDefaultDungeonLevelInstanceTransform() const;
	FString GetDefaultDungeonLevelInstanceName() const;
	FName GetDungeonLevelNetPackageName() const;
	FTransform GetOverworldReturnTransform() const;
	bool IsTrackedEnemy(AActor* Actor) const;
	bool IsDungeonEncounterMode() const;
	bool IsBrokenOutbreakMode() const;
	bool ShouldRewardsBeEligible() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	TObjectPtr<USphereComponent> InteractionCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	TObjectPtr<UStaticMeshComponent> PortalMesh;

	/** Data asset that defines this portal's mode, enemies, dungeon, boss, exit and text. */
	UPROPERTY(ReplicatedUsing = OnRep_EncounterDefinition, EditAnywhere, BlueprintReadOnly, Category = "Portal|Encounter")
	TObjectPtr<const URpgPortalEncounterDefinition> EncounterDefinition;

	/** Interaction ability granted by the interaction system when the portal is sealable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Interaction")
	TSubclassOf<URpgGameplayAbility_ClosePortal> ClosePortalAbilityClass;

	/** Interaction ability granted by the interaction system while the dungeon portal can be entered. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Interaction")
	TSubclassOf<URpgGameplayAbility_EnterPortal> EnterPortalAbilityClass;

	/** Fallback exit portal class used when the EncounterDefinition does not override it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Dungeon")
	TSubclassOf<ARpgPortalExitActor> ExitPortalActorClass;

	/** Distance in front of the overworld portal where exiting dungeon players return. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Dungeon", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float OverworldReturnDistance = 250.0f;

	/** Starts the encounter on BeginPlay; GameFeature-spawned portals normally keep this enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Encounter")
	bool bAutoStartOnBeginPlay = true;

	/** Replicated source of truth for portal interaction and encounter phase. */
	UPROPERTY(ReplicatedUsing = OnRep_PortalState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Portal")
	ERpgPortalState PortalState = ERpgPortalState::Dormant;

	/** Current stability value. BrokenOutbreak uses defeated enemies; Dungeon mode fills after boss completion. */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentStability, VisibleInstanceOnly, BlueprintReadOnly, Category = "Portal")
	float CurrentStability = 0.0f;

	/** Number of tracked BrokenOutbreak enemies defeated so far. */
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Portal")
	int32 DefeatedTrackedEnemyCount = 0;

	/** Number of BrokenOutbreak enemies that were actually spawned and tracked. */
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Portal")
	int32 TotalTrackedEnemyCount = 0;

	/** True after the dungeon boss death message/destruction has been processed. */
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Portal|Dungeon")
	bool bDungeonBossDefeated = false;

	/** Number of tracked players/pawns still inside this portal's dungeon instance. */
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Portal|Dungeon")
	int32 DungeonOccupantCount = 0;

	/** Technical level-instance placement chosen by the region spawner; not a gameplay marker. */
	UPROPERTY(ReplicatedUsing = OnRep_DungeonLevelStreamingConfig, VisibleInstanceOnly, BlueprintReadOnly, Category = "Portal|Dungeon")
	FTransform DungeonLevelInstanceTransform = FTransform::Identity;

	/** Stable level-instance name so server and clients load the same streamed dungeon instance. */
	UPROPERTY(ReplicatedUsing = OnRep_DungeonLevelStreamingConfig, VisibleInstanceOnly, BlueprintReadOnly, Category = "Portal|Dungeon")
	FString DungeonLevelInstanceName;

	/** Live BrokenOutbreak enemies whose deaths advance stability. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> TrackedEnemies;

	/** Live dungeon boss spawned at the BossSpawn marker. */
	UPROPERTY(Transient)
	TObjectPtr<AActor> TrackedBoss;

	/** Live dungeon exit portal spawned after boss defeat. */
	UPROPERTY(Transient)
	TObjectPtr<ARpgPortalExitActor> ExitPortalActor;

	/** Runtime streaming handle for this portal's dungeon level instance. */
	UPROPERTY(Transient)
	TObjectPtr<ULevelStreamingDynamic> DungeonLevelStreaming;

	/** Resolved Entry marker from the loaded dungeon level. */
	UPROPERTY(Transient)
	TObjectPtr<ARpgPortalDungeonMarkerActor> DungeonEntryMarker;

	/** Resolved BossSpawn marker from the loaded dungeon level. */
	UPROPERTY(Transient)
	TObjectPtr<ARpgPortalDungeonMarkerActor> DungeonBossSpawnMarker;

	/** Resolved ExitPortal marker from the loaded dungeon level. */
	UPROPERTY(Transient)
	TObjectPtr<ARpgPortalDungeonMarkerActor> DungeonExitPortalMarker;

	/** Players/pawns currently considered inside this dungeon instance. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> DungeonOccupants;

	/** Players/pawns that interacted while the dungeon level was still loading. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> PendingDungeonEntrants;

	/** Travel components that loaded this dungeon locally and must be unloaded on exit/close/cancel. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<URpgPortalTravelComponent>> KnownTravelComponents;

	FGameplayMessageListenerHandle ActorKilledListenerHandle;
	int32 NextPortalTravelRequestId = 0;
};
