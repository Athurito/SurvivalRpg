#pragma once

#include "GameplayTagContainer.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryItemTypes.h"
#include "RpgInventorySpatialTypes.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgInventoryFragment_ItemTraits.generated.h"

class UTexture2D;
class UGameplayEffect;
class UAnimMontage;
class URpgGameplayAbility;

/**
 * Static spatial footprint used by the server when placing an item in a grid inventory.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryFragment_SpatialItem : public URpgInventoryItemFragment
{
	GENERATED_BODY()

public:
	/** Unrotated item size in inventory grid cells. Static definition data used by server placement validation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Spatial")
	FRpgInventoryGridSize Footprint;

	/** If true, the item may be rotated by UI/controller input before server placement validation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Spatial")
	bool bAllowRotation = true;

	/** Returns the configured footprint after applying optional rotation. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial")
	FRpgInventoryGridSize GetFootprint(bool bRotated) const { return Footprint.GetRotated(bRotated && bAllowRotation); }
};

/**
 * One SetByCaller value written onto an outgoing item-use GameplayEffect spec.
 */
USTRUCT(BlueprintType)
struct FRpgInventoryUsableItemSetByCallerMagnitude
{
	GENERATED_BODY()

public:
	/** GameplayEffect SetByCaller data tag, for example SetByCaller.Heal. Must match the effect's expected tag. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Use")
	FGameplayTag DataTag;

	/** Magnitude written for this tag. Designers tune this per item definition; the server applies it during item use. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Use")
	float Magnitude = 0.0f;

	/** If true, using multiple stack units in one request multiplies this value by the requested use count. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Use")
	bool bScaleByUseCount = true;
};

/**
 * GameplayEffect applied by a usable item when its use ability runs.
 */
USTRUCT(BlueprintType)
struct FRpgInventoryUsableItemEffect
{
	GENERATED_BODY()

public:
	/** Effect applied to the owning player ASC. Use SetByCaller rows below for item-specific values. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Use")
	TSubclassOf<UGameplayEffect> GameplayEffect;

	/** Level used for this effect. Values <= 0 use the activated ability level instead. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Use", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float EffectLevel = 1.0f;

	/** Optional item-specific SetByCaller values written before the effect is applied. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Use")
	TArray<FRpgInventoryUsableItemSetByCallerMagnitude> SetByCallerMagnitudes;
};

/**
 * Point in an item-use sequence at which a configured use step should execute.
 */
UENUM(BlueprintType)
enum class ERpgInventoryUseStepTrigger : uint8
{
	/** Executes immediately after the item-use ability commits. */
	OnActivate,

	/** Executes after the step's Delay value on the server. */
	AfterDelay,

	/** Executes when the active montage emits the step's GameplayEvent tag. */
	OnMontageEvent,

	/** Executes when the optional use montage completes or blends out normally. */
	OnMontageCompleted,

	/** Executes when the optional use montage is interrupted or cancelled. */
	OnMontageInterrupted
};

/**
 * Built-in requirement checks used by the generic item-use ability before it accepts an item use.
 */
UENUM(BlueprintType)
enum class ERpgInventoryUseRequirementType : uint8
{
	/** No requirement. Useful as a temporarily disabled row in data assets. */
	None,

	/** The user must have Health below MaxHealth. Typical for healing potions. */
	HealthBelowMax
};

/**
 * One server-authoritative requirement for using an item.
 */
USTRUCT(BlueprintType)
struct FRpgInventoryUsableItemRequirement
{
	GENERATED_BODY()

public:
	/** Requirement evaluated by URpgGameplayAbility_ApplyItemEffects before activation is accepted. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Use")
	ERpgInventoryUseRequirementType RequirementType = ERpgInventoryUseRequirementType::None;
};

/**
 * One-shot gameplay cue emitted by an item-use sequence step.
 */
USTRUCT(BlueprintType)
struct FRpgInventoryUsableItemGameplayCue
{
	GENERATED_BODY()

public:
	/** GameplayCue tag executed on the user's ASC when the step runs. Effect-owned cues should still live on GameplayEffects. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Use", meta = (Categories = "GameplayCue"))
	FGameplayTag GameplayCueTag;

	/** Optional raw magnitude passed to the cue parameters. Cosmetic cues may ignore this value. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Use")
	float RawMagnitude = 0.0f;
};

/**
 * One step in a data-driven item-use sequence. Steps are executed only on the server.
 */
USTRUCT(BlueprintType)
struct FRpgInventoryUsableItemUseStep
{
	GENERATED_BODY()

public:
	/** When this step should run during the item-use ability. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Use")
	ERpgInventoryUseStepTrigger Trigger = ERpgInventoryUseStepTrigger::OnActivate;

	/** Delay in seconds for AfterDelay steps. Ignored by other triggers. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Use", meta = (EditCondition = "Trigger == ERpgInventoryUseStepTrigger::AfterDelay", ClampMin = "0.0", UIMin = "0.0"))
	float Delay = 0.0f;

	/** GameplayEvent tag required by OnMontageEvent steps. The generic item AnimNotify can send this tag. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Use", meta = (EditCondition = "Trigger == ERpgInventoryUseStepTrigger::OnMontageEvent", Categories = "GameplayEvent"))
	FGameplayTag MontageEventTag;

	/** If true, the item stack is consumed before this step's effects and cues are executed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Use")
	bool bConsumeItem = false;

	/** Effects applied to the owning player ASC when this step runs. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Use")
	TArray<FRpgInventoryUsableItemEffect> EffectsToApply;

	/** One-shot cues executed when this step runs. Prefer effect-owned cues for persistent effect visuals. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Use")
	TArray<FRpgInventoryUsableItemGameplayCue> GameplayCues;
};

/**
 * Presentation data read by inventory, equipment, loot, and storage widgets.
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

	/** Montage played by the generic item-use ability. Leave null for instant or timer-only consumables. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Use|Animation")
	TObjectPtr<UAnimMontage> UseMontage;

	/** Server-authoritative play rate for UseMontage. Values <= 0 are clamped to a tiny positive rate. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Use|Animation", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float MontagePlayRate = 1.0f;

	/** Optional montage section to start from. NAME_None starts at the montage default section. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Use|Animation")
	FName MontageStartSection = NAME_None;

	/** Data-driven requirements that must pass before the item-use ability is accepted. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Use|Requirements")
	TArray<FRpgInventoryUsableItemRequirement> UseRequirements;

	/** Ordered server-side steps for animated, delayed, or event-timed item effects, cues, and consumption. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Use")
	TArray<FRpgInventoryUsableItemUseStep> UseSequence;

	/** Legacy instant effects. Used as a single OnActivate step when UseSequence is empty. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Use")
	TArray<FRpgInventoryUsableItemEffect> EffectsToApply;
};
