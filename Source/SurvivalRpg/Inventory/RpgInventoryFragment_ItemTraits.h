#pragma once

#include "GameplayTagContainer.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryItemTypes.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgInventoryFragment_ItemTraits.generated.h"

class UTexture2D;
class URpgGameplayAbility;

/**
 * Presentation data read by inventory, quickbar, loot, and storage widgets.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryFragment_UIData : public URpgInventoryItemFragment
{
	GENERATED_BODY()

public:
	/** Optional item icon used by UI only. It is static definition data and is safe to load lazily in widgets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|UI", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** Optional designer-authored display name override for compact slot UI. Empty means use the item definition display name. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|UI")
	FText ShortDisplayName;

	/** Optional tooltip text shown in inventory-style screens. Static definition data, never runtime-mutated. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|UI", meta = (MultiLine = true))
	FText Description;

	/** Optional UI tags such as rarity or item family; UI may read these but gameplay should use explicit rules. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|UI")
	FGameplayTagContainer PresentationTags;
};

/**
 * Gameplay-facing item traits used by server-side inventory validation and V1 drop/crafting rules.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryFragment_ItemTraits : public URpgInventoryItemFragment
{
	GENERATED_BODY()

public:
	/** Broad item category used by UI grouping and simple gameplay validation. Static definition data. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Traits")
	ERpgInventoryItemCategory ItemCategory = ERpgInventoryItemCategory::Misc;

	/** Additional gameplay tags for designer filtering, recipes, loot tables, or future item queries. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Traits")
	FGameplayTagContainer ItemTags;

	/** Whether items of this definition may combine into one inventory entry. Instance-specific gear should leave this false. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Stacking")
	bool bCanStack = false;

	/** Maximum count in one stack when stacking is enabled. Values below 1 are treated as 1 at runtime. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Stacking", meta = (EditCondition = "bCanStack", ClampMin = "1", UIMin = "1"))
	int32 MaxStackSize = 1;

	/** Explicit designer permission for quickbar assignment. Weapons and shields may enable this; armor and materials should not. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|QuickBar")
	bool bCanAssignToQuickBar = false;

	/** Treat this item as a material for death drops and crafting-source scans even if its category is more specific. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Drops")
	bool bTreatAsMaterial = false;

	/** Death-drop behavior for this item definition. Equipment should keep Never. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Drops")
	ERpgInventoryDeathDropRule DeathDropRule = ERpgInventoryDeathDropRule::Never;

	/** Manual UI drop behavior. Leave Default for category-based V1 rules, or override for quest/special items. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Drops")
	ERpgInventoryManualDropPolicy ManualDropPolicy = ERpgInventoryManualDropPolicy::Default;

	UFUNCTION(BlueprintPure, Category = "Inventory|Traits")
	bool IsMaterial() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Traits")
	bool CanDropForMode(ERpgPlayerDeathDropMode DropMode) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Traits")
	ERpgInventoryManualDropPolicy GetResolvedManualDropPolicy() const;

	int32 GetMaxStackSize() const;
};

/**
 * Ability-driven active item behavior for consumables and other one-shot usable inventory items.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryFragment_UsableItem : public URpgInventoryItemFragment
{
	GENERATED_BODY()

public:
	/** Ability granted and activated once on the owning player's ASC when the item is used. Must execute on the server. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Use")
	TSubclassOf<URpgGameplayAbility> UseAbility;

	/** Ability level passed to GAS for this one-shot activation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Use", meta = (ClampMin = "1", UIMin = "1"))
	int32 AbilityLevel = 1;

	/** Number of item units consumed per accepted activation. Zero keeps the item stack unchanged. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Use", meta = (ClampMin = "0", UIMin = "0"))
	int32 ConsumeCount = 1;

	/** If true, ConsumeCount is removed after GAS accepts the one-shot ability activation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Use")
	bool bConsumeOnActivationAccepted = true;

	/** If true, the item must be in the player's own backpack, not storage, loot, or crafting output. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Use")
	bool bOnlyFromPlayerInventory = true;
};
