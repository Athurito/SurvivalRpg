// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RpgPawnData.generated.h"

class URpgCameraMode;
class URpgInputConfig;
class URpgAbilitySet;
class URpgAbilityTagRelationshipMapping;
class URpgPlayerInventoryLayoutDefinition;
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

	// Data-driven GenericTeamAgent team. 255/negative values are treated as NoTeam.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Team", meta = (ClampMin = "-1", ClampMax = "254"))
	int32 TeamId = 1;
	
	// What mapping of ability tags to use for actions taking by this pawn
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Abilities")
	TObjectPtr<URpgAbilityTagRelationshipMapping> TagRelationshipMapping;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<URpgInputConfig> InputConfig;

	// Startup-only ability sets that should be granted whenever a pawn using this data is initialized.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TArray<TObjectPtr<const URpgAbilitySet>> AbilitySets;
	
	// Default camera mode used by player controlled pawns.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Camera")
	TSubclassOf<URpgCameraMode> DefaultCameraMode;

	/**
	 * Static player-inventory layout selected by this PawnData.
	 * This hard reference is designer-authored, cooked transitively with the PawnData, and read-only at runtime.
	 * Non-player PawnData may leave it unset.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Inventory")
	TObjectPtr<const URpgPlayerInventoryLayoutDefinition> InventoryLayoutDefinition;
};
