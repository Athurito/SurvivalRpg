#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "SurvivalRpg/Interaction/IInteractableTarget.h"
#include "RpgPortalActor.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class URpgGameplayAbility_ClosePortal;
class URpgPortalEncounterDefinition;
struct FRpgCombatActorKilledMessage;

UENUM(BlueprintType)
enum class ERpgPortalState : uint8
{
	Dormant,
	Active,
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

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Portal")
	bool TryClosePortal(AActor* ClosingActor);

	UFUNCTION(BlueprintPure, Category = "Portal")
	ERpgPortalState GetPortalState() const { return PortalState; }

	UFUNCTION(BlueprintPure, Category = "Portal")
	float GetCurrentStability() const { return CurrentStability; }

	UFUNCTION(BlueprintPure, Category = "Portal")
	float GetMaxStability() const;

	UFUNCTION(BlueprintPure, Category = "Portal")
	int32 GetDefeatedTrackedEnemyCount() const { return DefeatedTrackedEnemyCount; }

	UFUNCTION(BlueprintPure, Category = "Portal")
	int32 GetRemainingTrackedEnemyCount() const;

	UFUNCTION(BlueprintPure, Category = "Portal")
	bool IsSealable() const { return PortalState == ERpgPortalState::Sealable; }

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
	void HandleTrackedEnemyDestroyed(AActor* DestroyedActor);

	void HandleActorKilled(FGameplayTag Channel, const FRpgCombatActorKilledMessage& Message);
	void RegisterCombatMessageListener();
	void UnregisterCombatMessageListener();
	void SpawnEncounterEnemies();
	void MarkTrackedEnemyDefeated(AActor* DefeatedEnemy);
	void RefreshStabilityFromProgress();
	void SetPortalState(ERpgPortalState NewState);
	bool IsTrackedEnemy(AActor* Actor) const;
	bool ShouldRewardsBeEligible() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	TObjectPtr<USphereComponent> InteractionCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	TObjectPtr<UStaticMeshComponent> PortalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Encounter")
	TObjectPtr<const URpgPortalEncounterDefinition> EncounterDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Interaction")
	TSubclassOf<URpgGameplayAbility_ClosePortal> ClosePortalAbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Encounter")
	bool bAutoStartOnBeginPlay = true;

	UPROPERTY(ReplicatedUsing = OnRep_PortalState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Portal")
	ERpgPortalState PortalState = ERpgPortalState::Dormant;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentStability, VisibleInstanceOnly, BlueprintReadOnly, Category = "Portal")
	float CurrentStability = 0.0f;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Portal")
	int32 DefeatedTrackedEnemyCount = 0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> TrackedEnemies;

	FGameplayMessageListenerHandle ActorKilledListenerHandle;
};
