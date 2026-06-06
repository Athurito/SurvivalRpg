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
class URpgGameplayAbility_ClosePortal;
class URpgGameplayAbility_EnterPortal;
class URpgPortalEncounterDefinition;
struct FRpgCombatActorKilledMessage;

UENUM(BlueprintType)
enum class ERpgPortalState : uint8
{
	Dormant,
	Active,
	DungeonInProgress,
	ExitOpen,
	Sealable,
	Closed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgPortalStateChanged, ERpgPortalState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRpgPortalStabilityChanged, float, CurrentStability, float, MaxStability);

UCLASS(Blueprintable)
class SURVIVALRPG_API ARpgPortalActor : public AActor, public IInteractableTarget
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

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Portal|Encounter")
	void ConfigureEncounterDefinition(const URpgPortalEncounterDefinition* InEncounterDefinition);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Portal")
	bool TryClosePortal(AActor* ClosingActor);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Portal")
	bool TryEnterPortal(AActor* EnteringActor);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Portal")
	bool TryExitPortal(AActor* ExitingActor);

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

	UPROPERTY(BlueprintAssignable, Category = "Portal")
	FRpgPortalStateChanged OnPortalStateChanged;

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
	void HandleTrackedEnemyDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void HandleDungeonOccupantDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void HandleTrackedBossDestroyed(AActor* DestroyedActor);

	void HandleActorKilled(FGameplayTag Channel, const FRpgCombatActorKilledMessage& Message);
	void RegisterCombatMessageListener();
	void UnregisterCombatMessageListener();
	void SpawnEncounterEnemies();
	void StartDungeonEncounter();
	bool LoadDungeonLevelInstance();
	void UnloadDungeonLevelInstance();
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
	FTransform GetDungeonEntryTransform() const;
	FTransform GetDungeonBossSpawnTransform() const;
	FTransform GetDungeonExitSpawnTransform() const;
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

	UPROPERTY(ReplicatedUsing = OnRep_EncounterDefinition, EditAnywhere, BlueprintReadOnly, Category = "Portal|Encounter")
	TObjectPtr<const URpgPortalEncounterDefinition> EncounterDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Interaction")
	TSubclassOf<URpgGameplayAbility_ClosePortal> ClosePortalAbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Interaction")
	TSubclassOf<URpgGameplayAbility_EnterPortal> EnterPortalAbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Dungeon")
	TSubclassOf<ARpgPortalExitActor> ExitPortalActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Encounter")
	bool bAutoStartOnBeginPlay = true;

	UPROPERTY(ReplicatedUsing = OnRep_PortalState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Portal")
	ERpgPortalState PortalState = ERpgPortalState::Dormant;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentStability, VisibleInstanceOnly, BlueprintReadOnly, Category = "Portal")
	float CurrentStability = 0.0f;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Portal")
	int32 DefeatedTrackedEnemyCount = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Portal")
	int32 TotalTrackedEnemyCount = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Portal|Dungeon")
	bool bDungeonBossDefeated = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Portal|Dungeon")
	int32 DungeonOccupantCount = 0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> TrackedEnemies;

	UPROPERTY(Transient)
	TObjectPtr<AActor> TrackedBoss;

	UPROPERTY(Transient)
	TObjectPtr<ARpgPortalExitActor> ExitPortalActor;

	UPROPERTY(Transient)
	TObjectPtr<ULevelStreamingDynamic> DungeonLevelStreaming;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> DungeonOccupants;

	FGameplayMessageListenerHandle ActorKilledListenerHandle;
};
