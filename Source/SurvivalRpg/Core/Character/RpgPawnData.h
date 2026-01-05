// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RpgPawnData.generated.h"

class URpgInputConfig;
class URpgAbilitySet;
struct FRpgInputAction;
/**
 * 
 */
UCLASS()
class SURVIVALRPG_API URpgPawnData : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	
	// Class to instantiate for this pawn (should usually derive from ARpgPawn or ARpgCharacter).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Pawn")
	TSubclassOf<APawn> PawnClass;

	// Ability sets to grant to this pawn's ability system.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Abilities")
	TArray<TObjectPtr<URpgAbilitySet>> AbilitySets;

	// // What mapping of ability tags to use for actions taking by this pawn
	// UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Abilities")
	// TObjectPtr<URpgAbilityTagRelationshipMapping> TagRelationshipMapping;

	// Input configuration used by player controlled pawns to create input mappings and bind input actions.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Input")
	TObjectPtr<URpgInputConfig> InputConfig;

	// // Default camera mode used by player controlled pawns.
	// UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Camera")
	// TSubclassOf<URpgCameraMode> DefaultCameraMode;
};
