#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "RpgSkillTreeDefinition.generated.h"

class UCurveFloat;
class UTexture2D;
class URpgAbilitySet;
class FDataValidationContext;

/** How one skill tree tuning entry combines with the ability's base value. */
UENUM(BlueprintType)
enum class ERpgSkillTreeTuningOperation : uint8
{
	/** Replaces the base value. When several learned entries set the same value, the last one in node order wins. */
	Set,

	/** Adds Value after every Set entry was applied. */
	Add,

	/** Multiplies the result by Value after every Add entry was applied. */
	Multiply
};

/** Where the nodes of a skill tree sit on the mastery map. Presentation only; no gameplay rule reads it. */
UENUM(BlueprintType)
enum class ERpgSkillTreeLayoutMode : uint8
{
	/** Nodes sit on a grid from Row and Column; GridSpacing turns cells into map positions. Existing trees use it. */
	Grid,

	/** Nodes sit at their authored MapPosition. Row and Column are ignored. */
	FreePosition
};

/**
 * One region of the mastery map, usually a branch such as Wucht. Its bounds follow the nodes that name it, so moving
 * a node never needs a second edit. Designer-tuned static presentation data; no gameplay rule reads it.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgSkillTreeMapRegion
{
	GENERATED_BODY()

	/** Stable region identity nodes and region shortcuts refer to, for example Wucht. Must be unique in the tree. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Map")
	FName RegionId;

	/** Player-facing region name, for example the branch title. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Map")
	FText DisplayName;

	/** Space around the region's node centers in map units; the map frames the region with it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Map", meta = (ClampMin = "0", UIMin = "0", UIMax = "400"))
	FVector2D Padding = FVector2D(96.0, 96.0);
};

/**
 * One numeric change a learned node makes to the abilities granted by its weapon, for example a larger area radius.
 * Abilities read the result through URpgGameplayAbility::GetTunedValue, so new upgrades of existing values need no code.
 * Designer-tuned static data.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgSkillTreeAbilityTuning
{
	GENERATED_BODY()

	/**
	 * Stable ability id the entry applies to, for example Ability.Harvesting.GraveSwarm. Empty applies it to every
	 * ability the weapon of this tree grants.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Tuning", meta = (Categories = "Ability"))
	FGameplayTag AbilityIdTag;

	/** Value the ability reads, for example Ability.Tuning.Harvest.AreaRadius. Abilities define which tags they read. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Tuning", meta = (Categories = "Ability.Tuning"))
	FGameplayTag TuningTag;

	/** How Value combines with the ability's base value: all Set entries first, then Add, then Multiply. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Tuning")
	ERpgSkillTreeTuningOperation Operation = ERpgSkillTreeTuningOperation::Add;

	/** Operand in the unit of the tuned value, for example centimeters for a radius. Must be finite. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Tuning")
	float Value = 0.0f;
};

/**
 * One purchasable node of a skill tree. Every field except NodeTag is optional, so a node can grant an active ability,
 * a passive effect, gameplay tags, numeric upgrades or any combination of them. Designer-tuned static data.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgSkillTreeNode
{
	GENERATED_BODY()

	/** Stable node identity used by saves, prerequisites and UI, for example SkillTree.Node.Axe.DeathWave. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Node", meta = (Categories = "SkillTree.Node"))
	FGameplayTag NodeTag;

	/** Player-facing node name. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Display")
	FText DisplayName;

	/** Player-facing description shown in the node tooltip. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Display", meta = (MultiLine = true))
	FText Description;

	/** Optional node icon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Display", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** Presentation style such as SkillTree.NodeKind.Active or SkillTree.NodeKind.Ultimate. UI only; no gameplay rule reads it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Display", meta = (Categories = "SkillTree.NodeKind"))
	FGameplayTag KindTag;

	/** Grid row of the node, zero at the top, on a Grid tree. Rows usually rise with RequiredPointsInTree. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Layout", meta = (ClampMin = "0", UIMin = "0", UIMax = "12"))
	int32 Row = 0;

	/** Grid column of the node, zero at the left, on a Grid tree. Branches occupy neighboring columns. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Layout", meta = (ClampMin = "0", UIMin = "0", UIMax = "8"))
	int32 Column = 0;

	/**
	 * Center of the node on a FreePosition tree, in map units: slate units at zoom 1, X to the right, Y down. Ignored
	 * on a Grid tree. Presentation only.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Layout")
	FVector2D MapPosition = FVector2D::ZeroVector;

	/** Map region (branch) the node belongs to: one of the tree's Regions, or None. Presentation only. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Layout")
	FName RegionId;

	/** Points the node costs. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Requirements", meta = (ClampMin = "1", UIMin = "1", UIMax = "5"))
	int32 Cost = 1;

	/** Points the player must already have spent in this tree before the node can be learned. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Requirements", meta = (ClampMin = "0", UIMin = "0", UIMax = "30"))
	int32 RequiredPointsInTree = 0;

	/** Nodes of the same tree that must all be learned first. The UI draws a link from each of them. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Requirements", meta = (Categories = "SkillTree.Node"))
	TArray<FGameplayTag> Prerequisites;

	/** Nodes sharing a non-empty group exclude each other: only one of them can be learned at a time. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Requirements")
	FName ExclusiveGroup;

	/**
	 * Abilities, passive effects and attribute changes granted while the node is learned and the weapon of this tree is
	 * in use. Abilities with an AbilityIdTag can be placed on Q/E/R; the first free slot takes a newly learned one.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Grants")
	TObjectPtr<const URpgAbilitySet> AbilitySet = nullptr;

	/** Loose gameplay tags owned while the node is learned and the weapon of this tree is in use. Replicated through GAS. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Grants")
	FGameplayTagContainer GrantedTags;

	/** Numeric changes to the abilities of this tree's weapon while the node is learned. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Grants", meta = (TitleProperty = "TuningTag"))
	TArray<FRpgSkillTreeAbilityTuning> AbilityTunings;

	/** Returns the stable ability ids of the abilities this node grants. */
	void GetGrantedAbilityIds(TArray<FGameplayTag>& OutAbilityIds) const;
};

/**
 * Data-only skill tree carried by a weapon or tool type through URpgInventoryFragment_SkillTree.
 *
 * Every item that references the tree shares its progress, which belongs to the character. Points come from the level
 * of MasterySkillTag. The asset is registered as primary asset type RpgSkillTreeDefinition, so new trees are found
 * without code; the UI places nodes by LayoutMode, from Row and Column or from MapPosition. Static designer data;
 * runtime state lives in URpgSkillTreeComponent.
 */
UCLASS(BlueprintType, Const)
class SURVIVALRPG_API URpgSkillTreeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Stable tree identity used by saves and requests, for example SkillTree.Tree.Axe. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree", meta = (Categories = "SkillTree.Tree"))
	FGameplayTag TreeTag;

	/** Player-facing tree name, usually the weapon or tool type. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Display")
	FText DisplayName;

	/** Optional short description shown in the progression overview. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Display", meta = (MultiLine = true))
	FText Description;

	/** Optional tree icon for the progression overview. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Display", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** Optional branch titles shown above the columns, for example Death and Grave. Display only. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Display")
	TArray<FText> BranchNames;

	/** Skill whose level earns this tree's points, for example Skill.Gathering.Logging. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Points", meta = (Categories = "Skill"))
	FGameplayTag MasterySkillTag;

	/** Optional total points by skill level (X = level). Empty earns one point per level above level 1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Points")
	TObjectPtr<UCurveFloat> PointsByLevel = nullptr;

	/** Upper limit of the points this tree can earn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Points", meta = (ClampMin = "0", UIMin = "0", UIMax = "50"))
	int32 MaxPoints = 20;

	/** Purchasable nodes. Order is the order in which Set tunings apply. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree", meta = (TitleProperty = "NodeTag"))
	TArray<FRpgSkillTreeNode> Nodes;

	/** Whether nodes sit on the Row/Column grid or at their MapPosition. Presentation only. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Map")
	ERpgSkillTreeLayoutMode LayoutMode = ERpgSkillTreeLayoutMode::Grid;

	/** Distance between neighboring grid cells in map units on a Grid tree. Both components must be positive. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Map", meta = (ClampMin = "1", UIMin = "40", UIMax = "400"))
	FVector2D GridSpacing = FVector2D(160.0, 160.0);

	/** Map regions, usually one per branch, that region shortcuts can focus. Optional. Presentation only. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Map", meta = (TitleProperty = "RegionId"))
	TArray<FRpgSkillTreeMapRegion> Regions;

	/** Region the map frames when it opens without a remembered view; None frames the whole tree. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Map")
	FName DefaultRegionId;

	/** Returns the node with NodeTag, or null. */
	const FRpgSkillTreeNode* FindNode(FGameplayTag NodeTag) const;

	/** Returns the region with RegionId, or null. */
	const FRpgSkillTreeMapRegion* FindRegion(FName RegionId) const;

	/** Center of Node on the map in map units, from its cell on a Grid tree or its MapPosition otherwise. */
	FVector2D GetNodeMapPosition(const FRpgSkillTreeNode& Node) const;

	/** Box around the centers of RegionId's nodes, grown by its Padding; an invalid box when the region has no node. */
	FBox2D GetRegionBounds(FName RegionId) const;

	/** Box around every node center and every region; an invalid box when the tree has no node. */
	FBox2D GetMapBounds() const;

	/** Returns the points earned at SkillLevel, clamped to [0, MaxPoints]. */
	int32 GetEarnedPointsForLevel(int32 SkillLevel) const;

	/** Returns the node with NodeTag, or false when the tree has none. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Skill Tree", meta = (DisplayName = "Find Node"))
	bool K2_FindNode(FGameplayTag NodeTag, FRpgSkillTreeNode& OutNode) const;

#if WITH_EDITOR
	/** Rejects duplicate or missing tags, cycles, unreachable point gates and inconsistent exclusive groups. */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	/** Same checks as IsDataValid, usable at runtime and in tests. Returns true when the tree has no errors. */
	bool ValidateTree(TArray<FText>& OutErrors) const;
};
