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

UCLASS()
class SURVIVALRPG_API ARpgAICharacter : public ARpgCharacter
{
	GENERATED_BODY()

public:
	explicit ARpgAICharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void OnRep_PlayerState() override;

private:
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
};
