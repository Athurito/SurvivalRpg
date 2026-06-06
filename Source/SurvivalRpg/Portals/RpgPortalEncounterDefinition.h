#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "RpgPortalEncounterDefinition.generated.h"

class AActor;
class UWorld;

UENUM(BlueprintType)
enum class ERpgPortalEncounterMode : uint8
{
	Dungeon,
	BrokenOutbreak
};

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
	ERpgPortalEncounterMode EncounterMode = ERpgPortalEncounterMode::Dungeon;

	// Used by BrokenOutbreak portals that spill enemies into the overworld.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Encounter")
	TArray<FRpgPortalEnemySpawnEntry> EnemySpawnEntries;

	// Used by BrokenOutbreak portals that spill enemies into the overworld.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Encounter", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float SpawnRadius = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Dungeon")
	TSubclassOf<AActor> BossClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Dungeon", meta = (AssetBundles = "Server"))
	TSoftObjectPtr<UWorld> DungeonLevel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Dungeon")
	FTransform DungeonLevelInstanceTransform;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Dungeon")
	FTransform DungeonEntryTransform;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Dungeon")
	FTransform DungeonBossSpawnTransform;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Dungeon")
	FTransform DungeonExitSpawnTransform;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Dungeon")
	FTransform OverworldReturnTransformOffset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Encounter")
	FGameplayTagContainer CompletionTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Stability", meta = (ClampMin = "1.0"))
	float MaxStability = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Interaction")
	FText EnterInteractionText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Interaction")
	FText EnterInteractionSubText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Interaction")
	FText ExitInteractionText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Interaction")
	FText ExitInteractionSubText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Interaction")
	FText CloseInteractionText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Interaction")
	FText CloseInteractionSubText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Reward")
	bool bRewardsRequireBossDefeat = true;
};
