// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "RpgAICharacter.generated.h"

class URpgExperienceRewardComponent;
class URpgEnemyCombatArchetypeComponent;
class URpgEnemyCombatLoadoutComponent;
class URpgInventoryContainerComponent;
class URpgInventoryManagerComponent;
class URpgLootSourceComponent;
class URpgCorpseLifecycleComponent;

UCLASS()
class SURVIVALRPG_API ARpgAICharacter : public ARpgCharacter
{
	GENERATED_BODY()

public:
	explicit ARpgAICharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void OnRep_PlayerState() override;

	/** Replicated corpse anchor and lifecycle used by loot and harvesting features after death. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Corpse")
	URpgCorpseLifecycleComponent* GetCorpseLifecycleComponent() const
	{
		return CorpseLifecycleComponent;
	}

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnDeathStarted(AActor* OwningActor) override;
	virtual void OnDeathFinished(AActor* OwningActor) override;

private:
	void HandleLootPopulationCompleted(URpgLootSourceComponent* Source, bool bHasLoot);
	void HandleInventoryPostCommit(URpgInventoryManagerComponent* Inventory);
	void HandleCorpseAvailabilityChanged(URpgCorpseLifecycleComponent* Corpse, bool bIsAvailable);
	void RefreshCorpseContainerState();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Combat", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgEnemyCombatArchetypeComponent> CombatArchetypeComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Combat", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgEnemyCombatLoadoutComponent> CombatLoadoutComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Progression", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgExperienceRewardComponent> ExperienceRewardComponent;

	/** Replicated inventory used as the enemy corpse/container item source after death. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Inventory", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgInventoryManagerComponent> LootInventoryComponent;

	/** Interaction-facing container gate for opening the enemy's death loot. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Inventory", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgInventoryContainerComponent> LootContainerComponent;

	/** Populates and unlocks the enemy loot inventory when death finishes. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Inventory", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgLootSourceComponent> LootSourceComponent;

	/** Server-owned corpse timers and locally simulated bone-following ragdoll presentation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Corpse", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgCorpseLifecycleComponent> CorpseLifecycleComponent;
};
