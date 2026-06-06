#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "RpgPortalEncounterDefinition.generated.h"

class AActor;

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgPortalEnemySpawnEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Encounter")
	TSubclassOf<AActor> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Encounter", meta = (ClampMin = "0"))
	int32 Count = 1;
};

UCLASS(BlueprintType)
class SURVIVALRPG_API URpgPortalEncounterDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	URpgPortalEncounterDefinition();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Encounter")
	TArray<FRpgPortalEnemySpawnEntry> EnemySpawnEntries;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Encounter", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float SpawnRadius = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Encounter")
	FGameplayTagContainer CompletionTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Stability", meta = (ClampMin = "1.0"))
	float MaxStability = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Interaction")
	FText CloseInteractionText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Interaction")
	FText CloseInteractionSubText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Reward")
	bool bRewardsRequireBossDefeat = true;
};
