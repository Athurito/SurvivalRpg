#pragma once

#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTagContainer.h"
#include "MVVMViewModelBase.h"
#include "SurvivalRpg/Mvvm/RpgViewModelInvalidationQueue.h"
#include "SurvivalRpg/Progression/Skills/Data/RpgTradeSkillState.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeComponent.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeDefinition.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgSkillTreeViewModels.generated.h"

class APlayerController;
class APlayerState;
class UTexture2D;
class URpgPlayerProgressionComponent;
class URpgSkillTreeDefinition;
class URpgSkillTreeViewModel;
class URpgTradeSkillProgressionComponent;
struct FRpgEquipmentLoadoutSlotsChangedMessage;
struct FRpgInventoryChangeMessage;

/** One socket of the tool in hand, for the rune bar above the mastery map. Derived from the inventory; UI read-only. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgSkillTreeRuneSlotView
{
	GENERATED_BODY()

	/** True when a rune sits in this socket. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree|Runes")
	bool bHasRune = false;

	/** Identity of the socketed rune, for example Rune.Brand; empty for an empty socket. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree|Runes")
	FGameplayTag RuneTag;

	/** Player-facing rune name with its rank, for example "Brand II", or "Empty" for an empty socket. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree|Runes")
	FText Label;

	/** Rune icon from its item UI data, or null. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree|Runes")
	TSoftObjectPtr<UTexture2D> Icon;

	/** True while this rune's talents are highlighted on the map. Local UI state. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree|Runes")
	bool bIsHighlighted = false;

	bool operator==(const FRpgSkillTreeRuneSlotView& Other) const;
	bool operator!=(const FRpgSkillTreeRuneSlotView& Other) const { return !(*this == Other); }
};

/** How a skill tree node presents itself. Derived from ERpgSkillTreeUnlockResult; UI read-only. */
UENUM(BlueprintType)
enum class ERpgSkillTreeNodeState : uint8
{
	/** Learned; its grants apply while the tree's weapon is in use. */
	Unlocked,

	/** Every requirement is met and learning it is one click away. */
	Unlockable,

	/** Every requirement is met except free points. */
	Unaffordable,

	/** The row's point gate or a prerequisite is still missing. */
	Locked,

	/** Another node of the same exclusive group is learned. */
	Excluded
};

/** Whether a request for a node waits for the server or was rejected. Local UI state; UI read-only. */
UENUM(BlueprintType)
enum class ERpgSkillTreeRequestStatus : uint8
{
	/** No request for the node is open. */
	Idle,

	/** A request was sent and neither its result nor a state change has arrived yet. */
	Pending,

	/** The server rejected the last request; the node's Rejection says why. */
	Rejected
};

/** Swap option and request feedback a tree view model hands one node view model. Not reflected. */
struct FRpgSkillTreeNodeInteraction
{
	/** The node is excluded by a learned alternative that a swap can replace now. */
	bool bCanSwap = false;

	/** The learned alternative a swap would refund, or empty. */
	FGameplayTag SwapReplacesNodeTag;

	/** Player-facing name of SwapReplacesNodeTag, or empty. */
	FText SwapReplacesName;

	ERpgSkillTreeRequestStatus RequestStatus = ERpgSkillTreeRequestStatus::Idle;
	ERpgSkillTreeRequestRejection Rejection = ERpgSkillTreeRequestRejection::None;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRpgSkillTreeViewModelChanged);

/** One grid row of a skill tree with its point gate. UI read-only. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgSkillTreeRowView
{
	GENERATED_BODY()

	/** Grid row, zero at the top. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree")
	int32 Row = 0;

	/** Lowest RequiredPointsInTree among the row's nodes; zero for rows without nodes. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree")
	int32 RequiredPoints = 0;

	/** True when the player has spent at least RequiredPoints in the tree. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree")
	bool bIsUnlocked = false;

	bool operator==(const FRpgSkillTreeRowView& Other) const = default;
};

/** One prerequisite link the tree draws, from a prerequisite to the node that needs it. UI read-only. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgSkillTreeLinkView
{
	GENERATED_BODY()

	/** The prerequisite node. Links exist only for authored prerequisites, never for decoration. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree")
	FGameplayTag FromNodeTag;

	/** The node that needs the prerequisite. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree")
	FGameplayTag ToNodeTag;

	/** Map position of the prerequisite's center in map units. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree")
	FVector2D FromPosition = FVector2D::ZeroVector;

	/** Map position of the dependent node's center in map units. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree")
	FVector2D ToPosition = FVector2D::ZeroVector;

	/** Grid cell of the prerequisite (X = column, Y = row). */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree")
	FIntPoint FromCell = FIntPoint::ZeroValue;

	/** Grid cell of the node that needs the prerequisite (X = column, Y = row). */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree")
	FIntPoint ToCell = FIntPoint::ZeroValue;

	/** True when the prerequisite is learned. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree")
	bool bFromUnlocked = false;

	/** True when the node that needs the prerequisite is learned. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree")
	bool bToUnlocked = false;

	bool operator==(const FRpgSkillTreeLinkView& Other) const = default;
};

/** One region of the mastery map with its framed bounds. UI read-only. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgSkillTreeRegionView
{
	GENERATED_BODY()

	/** Stable region identity from the tree definition. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree")
	FName RegionId;

	/** Player-facing region name. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree")
	FText DisplayName;

	/** Map area the region frames in map units: its node centers grown by its padding. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree")
	FBox2D Bounds = FBox2D(ForceInit);

	bool operator==(const FRpgSkillTreeRegionView& Other) const
	{
		return RegionId == Other.RegionId && DisplayName.IdenticalTo(Other.DisplayName) && Bounds == Other.Bounds &&
			Bounds.bIsValid == Other.Bounds.bIsValid;
	}
};

/** One Q/E/R slot of a tree with the learned ability on it. UI read-only. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgSkillTreeSlotView
{
	GENERATED_BODY()

	/** Weapon ability slot, 0 = Q. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree")
	int32 SlotIndex = 0;

	/** Ability id on the slot, or empty. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree")
	FGameplayTag AbilityIdTag;

	/** Node that grants the ability, or empty. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree")
	FGameplayTag NodeTag;

	/** Name of the granting node, or empty. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree")
	FText DisplayName;

	/** Icon of the granting node, or null. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree")
	TSoftObjectPtr<UTexture2D> Icon;

	bool operator==(const FRpgSkillTreeSlotView& Other) const
	{
		return SlotIndex == Other.SlotIndex && AbilityIdTag == Other.AbilityIdTag && NodeTag == Other.NodeTag &&
			DisplayName.IdenticalTo(Other.DisplayName) && Icon == Other.Icon;
	}
};

/**
 * One node of a skill tree as the tree screen shows it. The owning URpgSkillTreeViewModel rebuilds it from replicated
 * owner-only progress; commands go back through the tree view model, which sends validated server requests.
 */
UCLASS(BlueprintType, meta = (MVVMAllowedContextCreationType = "Manual"))
class SURVIVALRPG_API URpgSkillTreeNodeViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Projects Node at InMapPosition for InTree. Called by the tree view model. */
	void UpdateNode(
		URpgSkillTreeViewModel* InTree,
		const FRpgSkillTreeNode& Node,
		const FVector2D& InMapPosition,
		ERpgSkillTreeUnlockResult InUnlockResult,
		int32 InAssignedSlotIndex,
		bool bInSelected,
		bool bInRowUnlocked,
		bool bInCanRefund,
		const FRpgSkillTreeNodeInteraction& Interaction,
		bool bInRunesActive = true);

	/** Tree view model that owns this node. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|ViewModel")
	URpgSkillTreeViewModel* GetTree() const { return Tree.Get(); }

	/** Asks the server to learn this node. The server validates it; the UI updates when the result replicates. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|ViewModel")
	void RequestUnlock();

	/**
	 * Asks the server to refund this learned node's point, for example on a right-click. The server validates it; the
	 * UI updates when the result replicates.
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|ViewModel")
	void RequestRefund();

	/**
	 * Asks the server to learn this excluded node and refund its learned alternative in one step. The server validates
	 * it; the UI updates when the result replicates.
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|ViewModel")
	void RequestSwap();

	/** Selects this node in its tree, for example before placing its ability on Q/E/R. Local UI state only. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|ViewModel")
	void Select();

	FGameplayTag GetNodeTag() const { return NodeTag; }
	FIntPoint GetCell() const { return FIntPoint(Column, Row); }
	FVector2D GetMapPosition() const { return MapPosition; }
	FName GetRegionId() const { return RegionId; }
	ERpgSkillTreeNodeState GetState() const { return State; }
	ERpgSkillTreeUnlockResult GetUnlockResult() const { return UnlockResult; }
	FGameplayTag GetAbilityIdTag() const { return AbilityIdTag; }
	int32 GetAssignedSlotIndex() const { return AssignedSlotIndex; }
	bool IsSelected() const { return bIsSelected; }
	bool CanRefund() const { return bCanRefund; }
	bool CanSwap() const { return bCanSwap; }
	FGameplayTag GetSwapReplacesNodeTag() const { return SwapReplacesNodeTag; }
	ERpgSkillTreeRequestStatus GetRequestStatus() const { return RequestStatus; }
	ERpgSkillTreeRequestRejection GetRejection() const { return Rejection; }
	bool NeedsRunes() const { return bNeedsRunes; }
	bool IsDormant() const { return bIsDormant; }
	bool IsRuneHighlighted() const { return bIsRuneHighlighted; }

	/** Marks the node as a talent of the rune the rune bar highlights. Called by the tree view model. */
	void SetRuneHighlighted(bool bHighlighted);

protected:
	/** Stable node identity. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	FGameplayTag NodeTag;

	/** Player-facing node name. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	/** Player-facing description for the tooltip. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	FText Description;

	/** Optional node icon. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** Presentation style, SkillTree.NodeKind.*. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	FGameplayTag KindTag;

	/** Grid row, zero at the top. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	int32 Row = 0;

	/** Grid column, zero at the left. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	int32 Column = 0;

	/** Center of the node on the map in map units, from the tree's layout mode. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	FVector2D MapPosition = FVector2D::ZeroVector;

	/** Map region (branch) of the node, or None. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	FName RegionId;

	/** Points the node costs. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	int32 Cost = 1;

	/** Points that must be spent in the tree before the node can be learned. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	int32 RequiredPointsInTree = 0;

	/** How the node presents itself right now. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	ERpgSkillTreeNodeState State = ERpgSkillTreeNodeState::Locked;

	/** First reason the node cannot be learned, or Unlockable / AlreadyUnlocked. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	ERpgSkillTreeUnlockResult UnlockResult = ERpgSkillTreeUnlockResult::UnknownNode;

	/** True when the node is learned. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	bool bIsUnlocked = false;

	/** True when RequestUnlock would succeed now. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	bool bCanUnlock = false;

	/** True when RequestRefund would succeed now: the node is learned and no other learned node depends on it. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	bool bCanRefund = false;

	/** True when the row's point gate is reached. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	bool bIsRowUnlocked = false;

	/** First ability id the node grants, which the Q/E/R bar places; empty for passive nodes. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	FGameplayTag AbilityIdTag;

	/** True when the node grants an ability that can go on Q/E/R. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	bool bGrantsSlotAbility = false;

	/** Q/E/R slot that holds the node's ability (0 = Q), or -1. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	int32 AssignedSlotIndex = INDEX_NONE;

	/** True when the node is selected in its tree. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	bool bIsSelected = false;

	/**
	 * True when RequestSwap would succeed now: a learned alternative of the node's exclusive group excludes it, and
	 * replacing that alternative keeps every other learned node valid. Works without free points.
	 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	bool bCanSwap = false;

	/** The learned alternative a swap would refund, or empty. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	FGameplayTag SwapReplacesNodeTag;

	/** Player-facing name of the alternative a swap would refund, or empty. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	FText SwapReplacesName;

	/** Whether a learn, refund or swap request for this node waits for the server or was rejected. Local UI state. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	ERpgSkillTreeRequestStatus RequestStatus = ERpgSkillTreeRequestStatus::Idle;

	/** True while RequestStatus is Pending, so a card can hold its commands without comparing enums. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	bool bIsRequestPending = false;

	/** Why the server rejected the last request, while RequestStatus is Rejected; None otherwise. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	ERpgSkillTreeRequestRejection Rejection = ERpgSkillTreeRequestRejection::None;

	/** Rune tags the node needs in its tool's sockets to work, copied from the node; empty for most nodes. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	FGameplayTagContainer RequiredRunes;

	/** True when the node only works with runes in its tool's sockets. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	bool bNeedsRunes = false;

	/**
	 * True when the node is learned but sleeps: the tool of this tree in hand lacks its runes, or no such tool is in
	 * hand. Derived from the same rule the server's grants use; UI read-only.
	 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	bool bIsDormant = false;

	/** Player-facing rune requirement, for example "Needs Brand in the tool's sockets"; empty without one. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	FText RuneRequirementText;

	/** True while the rune bar highlights a rune this node needs. Local UI state. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Node", meta = (AllowPrivateAccess = "true"))
	bool bIsRuneHighlighted = false;

private:
	TWeakObjectPtr<URpgSkillTreeViewModel> Tree;
};

/**
 * One skill tree with the player's progress: points, nodes, row gates, links and the Q/E/R assignment.
 *
 * Observes URpgSkillTreeComponent and the trade skill that earns the tree's points, coalescing changes to one rebuild
 * per frame. Commands send the component's server requests; the server validates them and the view updates from the
 * replicated result. Never authoritative.
 */
UCLASS(BlueprintType, meta = (MVVMAllowedContextCreationType = "Manual"))
class SURVIVALRPG_API URpgSkillTreeViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

	/** Starts observing TreeTag's progress on InSkillTrees, usually the local player's component. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|ViewModel")
	void BindSkillTree(URpgSkillTreeComponent* InSkillTrees, FGameplayTag InTreeTag);

	/** Stops observing and clears every field. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|ViewModel")
	void UnbindSkillTree();

	/** Rebuilds every field from current state at once. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|ViewModel")
	void Refresh();

	/** Asks the server to learn NodeTag. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|ViewModel")
	void RequestUnlockNode(FGameplayTag NodeTag);

	/** Asks the server to refund the point of the learned NodeTag. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|ViewModel")
	void RequestRefundNode(FGameplayTag NodeTag);

	/** Asks the server to learn NodeTag and refund the learned alternative of its exclusive group in one step. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|ViewModel")
	void RequestSwapNode(FGameplayTag NodeTag);

	/** Asks the server to refund every point of the tree for free. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|ViewModel")
	void RequestResetTree();

	/** Selects NodeTag, or clears the selection with an empty tag. Local UI state only. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|ViewModel")
	void SelectNode(FGameplayTag NodeTag);

	/** Asks the server to place the selected node's ability on SlotIndex (0 = Q). Does nothing without a learned active selection. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|ViewModel")
	void AssignSelectedNodeToSlot(int32 SlotIndex);

	/** Asks the server to clear SlotIndex (0 = Q). */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|ViewModel")
	void ClearSlot(int32 SlotIndex);

	/** Node view models in authored order; instances stay stable while the tree's node set is unchanged. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|ViewModel")
	TArray<URpgSkillTreeNodeViewModel*> GetNodes() const;

	/** Returns the node view model of NodeTag, or null. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|ViewModel")
	URpgSkillTreeNodeViewModel* FindNode(FGameplayTag NodeTag) const;

	/** Observed component, or null. */
	URpgSkillTreeComponent* GetSkillTreeComponent() const { return ObservedSkillTrees.Get(); }
	FGameplayTag GetTreeTag() const { return TreeTag; }
	int32 GetEarnedPoints() const { return EarnedPoints; }
	int32 GetSpentPoints() const { return SpentPoints; }
	int32 GetAvailablePoints() const { return AvailablePoints; }
	int32 GetRowCount() const { return RowCount; }
	int32 GetColumnCount() const { return ColumnCount; }
	const TArray<FRpgSkillTreeRowView>& GetRows() const { return Rows; }
	const TArray<FRpgSkillTreeLinkView>& GetLinks() const { return Links; }
	const TArray<FRpgSkillTreeSlotView>& GetSlots() const { return Slots; }
	bool CanResetTree() const { return bCanResetTree; }
	URpgSkillTreeNodeViewModel* GetSelectedNode() const { return SelectedNode; }
	ERpgSkillTreeLayoutMode GetLayoutMode() const { return LayoutMode; }
	const FBox2D& GetMapBounds() const { return MapBounds; }
	const TArray<FRpgSkillTreeRegionView>& GetRegions() const { return Regions; }
	FName GetDefaultRegionId() const { return DefaultRegionId; }

	/** Returns the region view of RegionId, or null. */
	const FRpgSkillTreeRegionView* FindRegion(FName RegionId) const;

	/** Fired after every rebuild, for widgets that redraw the whole tree such as URpgSkillTreeGridWidget. */
	UPROPERTY(BlueprintAssignable, Category = "Skill Tree|ViewModel")
	FRpgSkillTreeViewModelChanged OnTreeChanged;

protected:
	/** Stable tree identity. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Tree", meta = (AllowPrivateAccess = "true"))
	FGameplayTag TreeTag;

	/** Player-facing tree name. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Tree", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	/** Short tree description for the overview. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Tree", meta = (AllowPrivateAccess = "true"))
	FText Description;

	/** Optional tree icon. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Tree", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** Branch titles above the columns. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Tree", meta = (AllowPrivateAccess = "true"))
	TArray<FText> BranchNames;

	/** Skill whose level earns the points. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Points", meta = (AllowPrivateAccess = "true"))
	FGameplayTag MasterySkillTag;

	/** Player-facing name of the mastery skill. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Points", meta = (AllowPrivateAccess = "true"))
	FText MasterySkillName;

	/** Current level of the mastery skill. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Points", meta = (AllowPrivateAccess = "true"))
	int32 MasteryLevel = 1;

	/** Points earned from the mastery level. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Points", meta = (AllowPrivateAccess = "true"))
	int32 EarnedPoints = 0;

	/** Points spent on learned nodes. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Points", meta = (AllowPrivateAccess = "true"))
	int32 SpentPoints = 0;

	/** Earned points not spent yet. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Points", meta = (AllowPrivateAccess = "true"))
	int32 AvailablePoints = 0;

	/** Upper limit of the tree's points. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Points", meta = (AllowPrivateAccess = "true"))
	int32 MaxPoints = 0;

	/** Number of grid rows: the highest node row plus one. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Layout", meta = (AllowPrivateAccess = "true"))
	int32 RowCount = 0;

	/** Number of grid columns: the highest node column plus one. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Layout", meta = (AllowPrivateAccess = "true"))
	int32 ColumnCount = 0;

	/** Node view models in authored order. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Layout", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgSkillTreeNodeViewModel>> Nodes;

	/** Every row with its point gate. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Layout", meta = (AllowPrivateAccess = "true"))
	TArray<FRpgSkillTreeRowView> Rows;

	/** Prerequisite links to draw. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Layout", meta = (AllowPrivateAccess = "true"))
	TArray<FRpgSkillTreeLinkView> Links;

	/** Whether the tree places nodes on its grid or at authored map positions. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Map", meta = (AllowPrivateAccess = "true"))
	ERpgSkillTreeLayoutMode LayoutMode = ERpgSkillTreeLayoutMode::Grid;

	/** Map area around every node and region in map units, at the tree's place on the shared map; invalid without nodes. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Map", meta = (AllowPrivateAccess = "true"))
	FBox2D MapBounds = FBox2D(ForceInit);

	/** Map regions in authored order. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Map", meta = (AllowPrivateAccess = "true"))
	TArray<FRpgSkillTreeRegionView> Regions;

	/** Region the map frames when it opens without a remembered view, or None for the whole tree. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Map", meta = (AllowPrivateAccess = "true"))
	FName DefaultRegionId;

	/** The three Q/E/R slots of the tree. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Slots", meta = (AllowPrivateAccess = "true"))
	TArray<FRpgSkillTreeSlotView> Slots;

	/** Selected node, or null. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Slots", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgSkillTreeNodeViewModel> SelectedNode = nullptr;

	/** True when the selection is a learned node whose ability can go on Q/E/R. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Slots", meta = (AllowPrivateAccess = "true"))
	bool bCanAssignSelectedNode = false;

	/** True when any point is spent, so a reset would refund something. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Points", meta = (AllowPrivateAccess = "true"))
	bool bCanResetTree = false;

private:
	UFUNCTION()
	void HandleSkillTreeChanged(FGameplayTag ChangedTreeTag);

	UFUNCTION()
	void HandleTradeSkillChanged(FGameplayTag SkillTag, const FTradeSkillState& NewState);

	UFUNCTION()
	void HandleRequestRejected(FGameplayTag RejectedTreeTag, FGameplayTag NodeTag, ERpgSkillTreeRequestRejection Reason);

	/** Marks NodeTag as waiting for the server before a request goes out, and forgets its last rejection. */
	void BeginRequest(FGameplayTag NodeTag);

public:
	/**
	 * Highlights the nodes that need RuneTag, or none for an empty tag, and announces the change to the map. Local UI
	 * state; called by the progression view model's rune bar.
	 */
	void SetHighlightedRune(FGameplayTag RuneTag);

private:
	FGameplayTag HighlightedRune;

	void Subscribe();
	void Unsubscribe();
	void QueueRefresh();
	void ExecuteQueuedRefresh();
	void Rebuild();

	TWeakObjectPtr<URpgSkillTreeComponent> ObservedSkillTrees;
	TWeakObjectPtr<URpgTradeSkillProgressionComponent> ObservedTradeSkills;
	FGameplayTag SelectedNodeTag;

	/** Nodes with a request in flight; any replicated change of the tree or a rejection ends the wait. */
	TSet<FGameplayTag> PendingNodeTags;

	/** Last rejection per node, until the selection moves or the node gets a new request. */
	TMap<FGameplayTag, ERpgSkillTreeRequestRejection> RejectedNodes;
	FRpgViewModelInvalidationQueue RefreshQueue;
};

/** One trade skill in the progression overview. UI read-only. */
UCLASS(BlueprintType, meta = (MVVMAllowedContextCreationType = "Manual"))
class SURVIVALRPG_API URpgTradeSkillViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Projects one replicated skill state. */
	void UpdateSkill(
		const FTradeSkillState& SkillState,
		const FText& InDisplayName,
		const TSoftObjectPtr<UTexture2D>& InIcon,
		float InXPToNextLevel,
		int32 InMaxLevel);

	FGameplayTag GetSkillTag() const { return SkillTag; }
	int32 GetLevel() const { return Level; }

protected:
	/** Stable skill identity, Skill.*. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Skill", meta = (AllowPrivateAccess = "true"))
	FGameplayTag SkillTag;

	/** Player-facing skill name. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Skill", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	/** Profession icon from the trade skill config, or null when none is authored. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Skill", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** Current level. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Skill", meta = (AllowPrivateAccess = "true"))
	int32 Level = 1;

	/** Highest attainable level. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Skill", meta = (AllowPrivateAccess = "true"))
	int32 MaxLevel = 100;

	/** Experience toward the next level. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Skill", meta = (AllowPrivateAccess = "true"))
	float XP = 0.0f;

	/** Experience the next level costs; zero at the maximum level. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Skill", meta = (AllowPrivateAccess = "true"))
	float XPToNextLevel = 0.0f;

	/** XP / XPToNextLevel in [0, 1]; one at the maximum level. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Skill", meta = (AllowPrivateAccess = "true"))
	float LevelProgress = 0.0f;

	/** True at the maximum level. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Skill", meta = (AllowPrivateAccess = "true"))
	bool bIsMaxLevel = false;
};

/**
 * Progression overview of the local player: character level, every trade skill with its XP bar and every skill tree
 * with its free points. Owns one URpgSkillTreeViewModel per known tree, the node selected on the mastery map across
 * all trees, and the tree that selection belongs to.
 *
 * Bind it to the owning player controller when the screen opens. It observes the player state's progression
 * components and rebuilds at most once per frame. UI read-only apart from the tree commands it forwards.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgSkillProgressionViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

	/** Observes the RPG player state of InPlayerController. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|ViewModel")
	void BindPlayerController(APlayerController* InPlayerController);

	/** Observes the progression components of InPlayerState. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|ViewModel")
	void BindPlayerState(APlayerState* InPlayerState);

	/** Stops observing and clears every field. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|ViewModel")
	void Unbind();

	/** Rebuilds every field from current state at once. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|ViewModel")
	void Refresh();

	/** Makes TreeTag the selected tree while no node is selected. Local UI state only. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|ViewModel")
	void SelectSkillTree(FGameplayTag TreeTag);

	/** Skill view models in replicated order. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|ViewModel")
	TArray<URpgTradeSkillViewModel*> GetSkills() const;

	/** One tree view model per known tree, sorted by name. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|ViewModel")
	TArray<URpgSkillTreeViewModel*> GetSkillTrees() const;

	/**
	 * Highlights on the map the talents of the rune in socket SlotIndex of the tool in hand, or turns the highlight off
	 * when that rune is already highlighted or the socket is empty. Local UI state only.
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|Runes")
	void ToggleRuneHighlight(int32 SlotIndex);

	/** Label of socket SlotIndex of the tool in hand, for example "Brand II" or "Empty"; empty outside the sockets. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|Runes")
	FText GetRuneSlotLabel(int32 SlotIndex) const;

	/** True when a rune sits in socket SlotIndex of the tool in hand. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|Runes")
	bool IsRuneSlotFilled(int32 SlotIndex) const;

	/** True while the rune in socket SlotIndex is highlighted. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|Runes")
	bool IsRuneSlotHighlighted(int32 SlotIndex) const;

	bool HasRuneTool() const { return bHasRuneTool; }
	const TArray<FRpgSkillTreeRuneSlotView>& GetRuneSlots() const { return RuneSlots; }
	FGameplayTag GetHighlightedRuneTag() const { return HighlightedRuneTag; }

	URpgSkillTreeViewModel* GetSelectedSkillTree() const { return SelectedSkillTree; }
	URpgSkillTreeNodeViewModel* GetSelectedNode() const { return SelectedNode; }
	int32 GetCharacterLevel() const { return CharacterLevel; }
	int32 GetTotalAvailablePoints() const { return TotalAvailablePoints; }

	/**
	 * Fired after every rebuild and whenever any tree view model changes, for example when a node is selected or its
	 * points change, so a screen can refresh everything from one event.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Skill Tree|ViewModel")
	FRpgSkillTreeViewModelChanged OnProgressionChanged;

protected:
	/** General character level. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Character", meta = (AllowPrivateAccess = "true"))
	int32 CharacterLevel = 1;

	/** Character experience toward the next level. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Character", meta = (AllowPrivateAccess = "true"))
	float CharacterXP = 0.0f;

	/** Character experience the next level costs. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Character", meta = (AllowPrivateAccess = "true"))
	float CharacterXPToNextLevel = 0.0f;

	/** CharacterXP / CharacterXPToNextLevel in [0, 1]; zero when the character progression has no XP curve. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Character", meta = (AllowPrivateAccess = "true"))
	float CharacterLevelProgress = 0.0f;

	/** Trade skills in replicated order. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Skills", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgTradeSkillViewModel>> Skills;

	/** One view model per known tree, sorted by name. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Trees", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgSkillTreeViewModel>> SkillTrees;

	/**
	 * Tree of SelectedNode; without a selection the one picked by SelectSkillTree, else the main-hand weapon's, else
	 * the first.
	 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Trees", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgSkillTreeViewModel> SelectedSkillTree = nullptr;

	/**
	 * Node selected in any tree, for the detail card of a map that shows every tree, or null. A map keeps one selection;
	 * if several trees hold one anyway, the most recently selected wins.
	 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Trees", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgSkillTreeNodeViewModel> SelectedNode = nullptr;

	/** Free points over every tree, for a badge on the menu tab. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Trees", meta = (AllowPrivateAccess = "true"))
	int32 TotalAvailablePoints = 0;

	/** True while the main hand holds an item with rune sockets, for the rune bar above the map. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Runes", meta = (AllowPrivateAccess = "true"))
	bool bHasRuneTool = false;

	/** Name of the socketed item in the main hand, or empty. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Runes", meta = (AllowPrivateAccess = "true"))
	FText RuneToolName;

	/** Sockets of the item in the main hand in socket order; empty without one. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Runes", meta = (AllowPrivateAccess = "true"))
	TArray<FRpgSkillTreeRuneSlotView> RuneSlots;

	/** Rune whose talents the map highlights, or empty. Local UI state. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Runes", meta = (AllowPrivateAccess = "true"))
	FGameplayTag HighlightedRuneTag;

private:
	UFUNCTION()
	void HandleSkillTreeChanged(FGameplayTag TreeTag);

	UFUNCTION()
	void HandleTradeSkillChanged(FGameplayTag SkillTag, const FTradeSkillState& NewState);

	UFUNCTION()
	void HandleCharacterLevelChanged(int32 NewLevel);

	UFUNCTION()
	void HandleCharacterXPChanged(float CurrentXP, float XPToNextLevel);

	UFUNCTION()
	void HandleTreeViewChanged();

	/** Follows the OnTreeChanged of exactly Trees. */
	void ObserveTrees(TConstArrayView<TObjectPtr<URpgSkillTreeViewModel>> Trees);

	/** Updates SelectedNode and SelectedSkillTree from the trees' selections. */
	void UpdateSelection();

	/** Rebuilds the rune bar from the item in the main hand and keeps the highlight only while its rune is socketed. */
	void RebuildRuneContext();

	/** Pushes HighlightedRuneTag to every tree and marks the highlighted socket. */
	void ApplyRuneHighlight();

	void HandleInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message);
	void HandleEquipmentSlotsChanged(FGameplayTag Channel, const FRpgEquipmentLoadoutSlotsChangedMessage& Message);

	void Subscribe();
	void Unsubscribe();
	void QueueRefresh();
	void ExecuteQueuedRefresh();
	void Rebuild();

	TWeakObjectPtr<APlayerState> ObservedPlayerState;
	TWeakObjectPtr<URpgSkillTreeComponent> ObservedSkillTrees;
	TWeakObjectPtr<URpgTradeSkillProgressionComponent> ObservedTradeSkills;
	TWeakObjectPtr<URpgPlayerProgressionComponent> ObservedCharacter;
	TArray<TWeakObjectPtr<URpgSkillTreeViewModel>> ObservedTrees;
	FGameplayTag RequestedTreeTag;
	FRpgViewModelInvalidationQueue RefreshQueue;
	FGameplayMessageListenerHandle InventoryChangedHandle;
	FGameplayMessageListenerHandle EquipmentChangedHandle;

	/** Set when sockets or the tool in hand may have changed, so the next rebuild re-derives the trees' sleeping talents. */
	bool bRefreshTreesOnRebuild = false;
};
