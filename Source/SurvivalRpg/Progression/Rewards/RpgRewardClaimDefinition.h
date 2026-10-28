#pragma once

#include "Engine/DataAsset.h"
#include "Templates/SubclassOf.h"

#include "RpgRewardClaimDefinition.generated.h"

class FDataValidationContext;
class URpgInventoryItemDefinition;
class URpgLootTable;
class UTexture2D;

/** One choice of a guaranteed reward, for example one rune of the player's choosing. Designer-authored static data. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgRewardClaimOption
{
	GENERATED_BODY()

	/** Item the player receives when picking this option. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reward")
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** How many of ItemDefinition the option gives; at least 1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reward", meta = (ClampMin = "1", UIMin = "1"))
	int32 Count = 1;
};

/**
 * A guaranteed reward a player claims later, for example at a mastery milestone or when a portal closes.
 * Designer-authored static data; the player's URpgRewardClaimComponent owns each granted claim.
 *
 * The reward is either a choice, where the player picks exactly one of Options, or a loot pool, where LootTable is
 * rolled once when the player claims it. The roll is seeded by the claim's id, so a claim that failed for lack of
 * room rolls the same items again. Configure exactly one of the two.
 */
UCLASS(BlueprintType, Const)
class SURVIVALRPG_API URpgRewardClaimDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Player-facing name of the reward, such as "Rune of your choice". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reward|Display")
	FText DisplayName;

	/** Optional player-facing description shown with the reward. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reward|Display", meta = (MultiLine = true))
	FText Description;

	/** Optional icon for reward lists. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reward|Display", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** The player picks exactly one of these. Leave empty to use LootTable instead. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reward", meta = (TitleProperty = "ItemDefinition"))
	TArray<FRpgRewardClaimOption> Options;

	/** Loot pool rolled once when the player claims the reward; used only while Options is empty. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reward")
	TObjectPtr<const URpgLootTable> LootTable;

	/** True when the player picks one of Options; false for a loot pool roll. */
	bool IsChoice() const { return !Options.IsEmpty(); }

	/** Number of buttons a claim of this reward offers: the options, or one for the loot pool. */
	int32 GetClaimOptionCount() const { return IsChoice() ? Options.Num() : (LootTable ? 1 : 0); }

	/** True with valid options or a loot table, not both. */
	bool HasValidConfiguration(FString* OutError = nullptr) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
