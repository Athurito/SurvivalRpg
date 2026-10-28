#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeDefinition.h"
#include "RpgInventoryFragment_Rune.generated.h"

class URpgAbilitySet;
class URpgCraftingRecipeDefinition;
class URpgInventoryItemInstance;
class URpgInventoryManagerComponent;
class URpgRecipeUnlockComponent;

/** Highest rank a rune item can reach: I, II and III. */
inline constexpr int32 RpgRuneMaxRank = 3;

/**
 * What one rank of a rune grants while the rune sits in a socket grid of an equipped item. Designer-authored static
 * data; the server's equipment manager applies it with the item's equipment instance as source.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgRuneRank
{
	GENERATED_BODY()

	/** Player-facing summary of this rank's power, for tooltips and the rune shrine. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rune", meta = (MultiLine = true))
	FText Description;

	/** Optional abilities, passive effects and attributes granted while socketed at this rank. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rune")
	TObjectPtr<const URpgAbilitySet> AbilitySet;

	/** Optional loose tags granted at this rank, in addition to the rune's RuneTag. Replicated with the ability system. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rune")
	FGameplayTagContainer GrantedTags;

	/**
	 * Numeric changes this rank makes to the abilities of the item the rune is socketed in, for example Brand's burn
	 * (Ability.Tuning.Harvest.BurnWork and .BurnDuration). They count only for abilities whose source equipment is
	 * that item, so a rune never tunes another tool, and they combine with the item's learned skill tree tunings.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rune", meta = (TitleProperty = "TuningTag"))
	TArray<FRpgSkillTreeAbilityTuning> AbilityTunings;

	/**
	 * Materials an upgrade to this rank costs, taken from the connected chests of a station that offers rune work, such
	 * as the Runenschrein. Ignored on rank I. Designer-tuned static data.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rune|Upgrade", meta = (TitleProperty = "ItemDefinition"))
	TArray<FRpgCraftingResourceCost> UpgradeCosts;

	/**
	 * Items salvaging an unsocketed rune of this rank returns into the station's connected chests, such as rune essence.
	 * Empty salvages for nothing. Designer-tuned static data.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rune|Salvage", meta = (TitleProperty = "ItemDefinition"))
	TArray<FRpgCraftingOutputItem> SalvageOutputs;
};

/**
 * Makes an item a physical rune: a non-stackable instance with its own rank that sits in an item's socket grid, such
 * as the three sockets of a harvesting tool. Designer-authored static data on the rune's item definition.
 *
 * The rank belongs to the concrete item instance; this fragment saves and restores it as versioned runtime state, so
 * moving, dropping, transferring and loading keep it. While the rune is socketed into an equipped item, the server's
 * equipment manager grants RuneTag as a loose tag plus the grants of the rune's current rank.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Rune"))
class SURVIVALRPG_API URpgInventoryFragment_Rune final : public URpgInventoryItemFragment
{
	GENERATED_BODY()

public:
	/** Highest rank of this rune, the number of authored Ranks clamped to 1..3. */
	UFUNCTION(BlueprintPure, Category = "Rune")
	int32 GetMaxRank() const;

	/** The authored rank Rank (1-based), or null outside 1..GetMaxRank(). */
	const FRpgRuneRank* FindRank(int32 Rank) const;

	/** The rune fragment of ItemInstance's definition, or null when the item is no rune. */
	static const URpgInventoryFragment_Rune* FindRuneOfItem(const UObject* ItemInstance);

	/** The RuneTags of the runes socketed in SourceItem, the rune context of its talents; empty for null. */
	static FGameplayTagContainer GetSocketedRuneTags(const URpgInventoryItemInstance* SourceItem);

	/**
	 * Folds the AbilityTunings of the current rank of every rune socketed in SourceItem into Accumulator. Valid on the
	 * server and on the owning client, which both receive the socket contents.
	 */
	static void AccumulateSocketedTunings(
		const URpgInventoryItemInstance& SourceItem,
		const FGameplayTagContainer& AbilityIds,
		FGameplayTag TuningTag,
		FRpgAbilityTuningAccumulator& Accumulator);

	/**
	 * Teaches the world the BaseRecipe of every rune in Inventory, including runes in item-owned grids such as a tool's
	 * sockets, without consuming them. Server only; returns how many recipes were newly learned. The player state calls
	 * this after each change of its inventory, so finding a rune is what teaches its recipe.
	 */
	static int32 LearnBaseRecipesOfRunesIn(
		const URpgInventoryManagerComponent& Inventory,
		URpgRecipeUnlockComponent& RecipeUnlocks);

	/**
	 * Identity of the rune kind, for example Rune.Brand. Talents and tools test for it; while socketed into an equipped
	 * item the wielder carries it as a loose tag.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rune", meta = (Categories = "Rune"))
	FGameplayTag RuneTag;

	/** Rank I, II and III in order; one to three entries. A new rune starts at rank I. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rune")
	TArray<FRpgRuneRank> Ranks;

	/**
	 * Recipe that crafts a new rank-I copy of this rune. Designer data. It is locked until a player of the world
	 * carries this rune: the server then unlocks its RecipeUnlockTag for the whole world and saves it with the world.
	 * The recipe should output this rune and not be unlocked by default. Empty means the rune cannot be crafted.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rune|Crafting")
	TObjectPtr<const URpgCraftingRecipeDefinition> BaseRecipe;

	//~URpgInventoryItemFragment interface
	virtual FName GetRuntimeStateIdentifier() const override;
	virtual int32 GetRuntimeStateVersion() const override;
	virtual bool ExportRuntimeState(
		const URpgInventoryItemInstance* Instance,
		FRpgInventoryFragmentStatePayload& OutPayload) const override;
	virtual bool ValidateRuntimeState(
		const URpgInventoryItemInstance* Instance,
		const FRpgInventoryFragmentStatePayload& Payload) const override;
	virtual bool ImportRuntimeState(
		URpgInventoryItemInstance* Instance,
		const FRpgInventoryFragmentStatePayload& Payload) const override;
	virtual void CopyRuntimeState(
		const URpgInventoryItemInstance* Source,
		URpgInventoryItemInstance* Target) const override;
	//~End of URpgInventoryItemFragment interface

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	bool ReadRank(const FRpgInventoryFragmentStatePayload& Payload, uint8& OutRank) const;
};
