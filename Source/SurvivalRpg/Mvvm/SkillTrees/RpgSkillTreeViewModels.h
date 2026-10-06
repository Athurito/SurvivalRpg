#pragma once

#include "GameplayTagContainer.h"
#include "MVVMViewModelBase.h"
#include "SurvivalRpg/Mvvm/RpgViewModelInvalidationQueue.h"
#include "SurvivalRpg/Progression/Skills/Data/RpgTradeSkillState.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeComponent.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgSkillTreeViewModels.generated.h"

class APlayerController;
class APlayerState;
class UTexture2D;
class URpgPlayerProgressionComponent;
class URpgSkillTreeDefinition;
class URpgSkillTreeViewModel;
class URpgTradeSkillProgressionComponent;
struct FRpgSkillTreeNode;

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
	/** Projects Node for InTree. Called by the tree view model. */
	void UpdateNode(
		URpgSkillTreeViewModel* InTree,
		const FRpgSkillTreeNode& Node,
		ERpgSkillTreeUnlockResult InUnlockResult,
		int32 InAssignedSlotIndex,
		bool bInSelected,
		bool bInRowUnlocked,
		bool bInCanRefund);

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

	/** Selects this node in its tree, for example before placing its ability on Q/E/R. Local UI state only. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|ViewModel")
	void Select();

	FGameplayTag GetNodeTag() const { return NodeTag; }
	FIntPoint GetCell() const { return FIntPoint(Column, Row); }
	ERpgSkillTreeNodeState GetState() const { return State; }
	ERpgSkillTreeUnlockResult GetUnlockResult() const { return UnlockResult; }
	FGameplayTag GetAbilityIdTag() const { return AbilityIdTag; }
	int32 GetAssignedSlotIndex() const { return AssignedSlotIndex; }
	bool IsSelected() const { return bIsSelected; }
	bool CanRefund() const { return bCanRefund; }

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

	void Subscribe();
	void Unsubscribe();
	void QueueRefresh();
	void ExecuteQueuedRefresh();
	void Rebuild();

	TWeakObjectPtr<URpgSkillTreeComponent> ObservedSkillTrees;
	TWeakObjectPtr<URpgTradeSkillProgressionComponent> ObservedTradeSkills;
	FGameplayTag SelectedNodeTag;
	FRpgViewModelInvalidationQueue RefreshQueue;
};

/** One trade skill in the progression overview. UI read-only. */
UCLASS(BlueprintType, meta = (MVVMAllowedContextCreationType = "Manual"))
class SURVIVALRPG_API URpgTradeSkillViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Projects one replicated skill state. */
	void UpdateSkill(const FTradeSkillState& SkillState, const FText& InDisplayName, float InXPToNextLevel, int32 InMaxLevel);

	FGameplayTag GetSkillTag() const { return SkillTag; }
	int32 GetLevel() const { return Level; }

protected:
	/** Stable skill identity, Skill.*. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Skill", meta = (AllowPrivateAccess = "true"))
	FGameplayTag SkillTag;

	/** Player-facing skill name. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Skill", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

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
 * with its free points. Owns one URpgSkillTreeViewModel per known tree and the screen's tree selection.
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

	/** Shows TreeTag in the tree view. Local UI state only. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|ViewModel")
	void SelectSkillTree(FGameplayTag TreeTag);

	/** Skill view models in replicated order. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|ViewModel")
	TArray<URpgTradeSkillViewModel*> GetSkills() const;

	/** One tree view model per known tree, sorted by name. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|ViewModel")
	TArray<URpgSkillTreeViewModel*> GetSkillTrees() const;

	URpgSkillTreeViewModel* GetSelectedSkillTree() const { return SelectedSkillTree; }
	int32 GetCharacterLevel() const { return CharacterLevel; }
	int32 GetTotalAvailablePoints() const { return TotalAvailablePoints; }

	/**
	 * Fired after every rebuild and whenever the selected tree view model changes, for example when a node is selected,
	 * so a screen can refresh everything from one event.
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

	/** Tree shown in the tree view: the one picked by SelectSkillTree, else the main-hand weapon's, else the first. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Trees", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgSkillTreeViewModel> SelectedSkillTree = nullptr;

	/** Free points over every tree, for a badge on the menu tab. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Skill Tree|Trees", meta = (AllowPrivateAccess = "true"))
	int32 TotalAvailablePoints = 0;

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
	void HandleSelectedTreeChanged();

	void ObserveSelectedTree(URpgSkillTreeViewModel* TreeViewModel);

	void Subscribe();
	void Unsubscribe();
	void QueueRefresh();
	void ExecuteQueuedRefresh();
	void Rebuild();

	TWeakObjectPtr<APlayerState> ObservedPlayerState;
	TWeakObjectPtr<URpgSkillTreeComponent> ObservedSkillTrees;
	TWeakObjectPtr<URpgTradeSkillProgressionComponent> ObservedTradeSkills;
	TWeakObjectPtr<URpgPlayerProgressionComponent> ObservedCharacter;
	TWeakObjectPtr<URpgSkillTreeViewModel> ObservedSelectedTree;
	FGameplayTag RequestedTreeTag;
	FRpgViewModelInvalidationQueue RefreshQueue;
};
