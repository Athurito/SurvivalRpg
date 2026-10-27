#pragma once

#include "Components/Widget.h"
#include "GameplayTagContainer.h"
#include "SRpgSkillTreeMapPanel.h"
#include "Styling/SlateBrush.h"

#include "RpgSkillTreeMapWidget.generated.h"

class UUserWidget;
class URpgSkillTreeNodeViewModel;
class URpgSkillTreeViewModel;
class URpgUIScreenSubsystem;

/** How much a skill tree map's entries should show at the current zoom. Local presentation state; UI read-only. */
UENUM(BlueprintType)
enum class ERpgSkillTreeMapDetail : uint8
{
	/** Zoomed out: branches and links; entries usually show only their icon and state. */
	Overview,

	/** Zoomed in far enough for names and costs. */
	Detail
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgSkillTreeMapNodeSelected, URpgSkillTreeNodeViewModel*, Node);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgSkillTreeMapDetailChanged, ERpgSkillTreeMapDetail, Detail);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FRpgSkillTreeMapSelectedNodeMoved, bool, bHasSelection, FVector2D, NodePosition, FVector2D, NodeSize, FVector2D, MapSize);

/**
 * Zoomable, pannable mastery map of one or several skill trees. Each tree is a cluster at its definition's MapOrigin
 * and keeps its own points. Places one NodeEntryClass widget per node at its map position and one optional
 * TreeEntryClass widget below each tree, draws regions and prerequisite links behind them, and owns every pointer
 * gesture on the map.
 *
 * - The mouse wheel zooms around the cursor; a drag with any button pans once it passes DragThreshold. Zoom, fit
 *   and focus glide over CameraBlendTime; a drag follows the pointer directly.
 * - A left click without a drag selects the node under the cursor. Nothing on the map learns or refunds; those are
 *   explicit commands of the detail panel.
 * - Keyboard and gamepad navigation moves the selection spatially, also from one tree to the next, and brings the
 *   node into view; at the map's edge focus leaves for the neighboring widgets.
 * - The map has one selection: selecting a node clears the selection of every other tree shown.
 * - Camera and selection are remembered for the local player under the set of trees shown, so closing and reopening
 *   the screen restores them, also for pooled screens.
 *
 * A node is identified by its tree and its NodeTag, because node tags only need to be unique per tree; functions that
 * take only a NodeTag use the first shown tree that has it. Entries are presentation only and never receive presses.
 * They implement User Object List Entry and receive their URpgSkillTreeNodeViewModel, or tree entries their
 * URpgSkillTreeViewModel, again after every tree change; URpgMvvmListEntryWidget children get it as their entry item.
 * Pan and zoom never recreate entries or view models. The map owns no gameplay state.
 */
UCLASS(meta = (DisplayName = "Rpg Skill Tree Map"))
class SURVIVALRPG_API URpgSkillTreeMapWidget : public UWidget
{
	GENERATED_BODY()

public:
	explicit URpgSkillTreeMapWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Shows InSkillTrees as clusters of one map, in the given order, or clears the map with an empty array. Null and
	 * repeated trees are skipped. The map follows the view models' later changes on its own.
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|Map")
	void SetSkillTrees(const TArray<URpgSkillTreeViewModel*>& InSkillTrees);

	/** Shows only InSkillTree, or clears the map with null. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|Map")
	void SetSkillTree(URpgSkillTreeViewModel* InSkillTree);

	/** Trees currently shown, in map order. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|Map")
	TArray<URpgSkillTreeViewModel*> GetSkillTrees() const;

	/** First tree shown, or null. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|Map")
	URpgSkillTreeViewModel* GetSkillTree() const;

	/** Selected node of whichever tree, or null. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|Map")
	URpgSkillTreeNodeViewModel* GetSelectedNode() const;

	/** Entry widget created for each node, tree by tree in authored node order. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|Map")
	TArray<UUserWidget*> GetNodeEntries() const;

	/** Entry widget of NodeTag in the first shown tree that has it, or null. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|Map")
	UUserWidget* GetNodeEntry(FGameplayTag NodeTag) const;

	/** Entry widget of NodeTag in TreeTag, or null. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|Map")
	UUserWidget* GetTreeNodeEntry(FGameplayTag TreeTag, FGameplayTag NodeTag) const;

	/** Tree entry widget of TreeTag, or null without a TreeEntryClass or when the tree is not shown. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|Map")
	UUserWidget* GetTreeEntry(FGameplayTag TreeTag) const;

	/** Zooms in one step around the viewport center, for zoom buttons and actions without a mouse wheel. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|Map")
	void ZoomIn();

	/** Zooms out one step around the viewport center. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|Map")
	void ZoomOut();

	/** Sets the zoom around the viewport center, clamped to [MinZoom, MaxZoom]. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|Map")
	void SetZoom(float NewZoom);

	/** Frames every shown tree. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|Map")
	void FitTree();

	/** Frames the whole cluster of TreeTag, for tree shortcuts. Returns false when the tree is not shown. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|Map")
	bool FocusTree(FGameplayTag TreeTag);

	/** Frames RegionId of the first shown tree that has it. Returns false when no shown tree has it. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|Map")
	bool FocusRegion(FName RegionId);

	/** Frames RegionId of TreeTag, for region shortcuts. Returns false when the tree is not shown or lacks the region. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|Map")
	bool FocusTreeRegion(FGameplayTag TreeTag, FName RegionId);

	/** Centers the view on NodeTag of the first shown tree that has it, without selecting it. False when none has it. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|Map")
	bool FocusNode(FGameplayTag NodeTag);

	/** Centers the view on NodeTag of TreeTag without selecting it. Returns false when no shown tree has it. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|Map")
	bool FocusTreeNode(FGameplayTag TreeTag, FGameplayTag NodeTag);

	/** Moves the map by Delta slate units, as a drag of that length would. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|Map")
	void PanBy(FVector2D Delta);

	/** Ends a running press or drag without a click and releases the pointer, for example when the screen deactivates. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|Map")
	void CancelGesture();

	/** Current zoom in slate units per map unit; 1 before the map is constructed. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|Map")
	float GetZoom() const;

	/** Map point at the viewport center, in map units. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|Map")
	FVector2D GetViewCenter() const;

	/** How much entries should show at the current zoom. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|Map")
	ERpgSkillTreeMapDetail GetDetailLevel() const { return DetailLevel; }

	/** True while a drag pans the map. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|Map")
	bool IsPanning() const;

	/** Fired after a click or navigation selected a node. Local selection only; nothing is learned. */
	UPROPERTY(BlueprintAssignable, Category = "Skill Tree|Map")
	FRpgSkillTreeMapNodeSelected OnNodeSelected;

	/** Fired when zooming crosses DetailZoom, so the screen or its entries can show or hide names and costs. */
	UPROPERTY(BlueprintAssignable, Category = "Skill Tree|Map")
	FRpgSkillTreeMapDetailChanged OnDetailLevelChanged;

	/**
	 * Fired when the selected node's drawn box moves in the map's local space: on every frame of a pan, zoom or glide,
	 * when the map is resized, and when the selection changes or clears. NodePosition is the box's top-left corner and
	 * NodeSize its size, in slate units; MapSize is the map's own size. Without a selection, bHasSelection is false and
	 * the box is zero. Sent at most once per frame and only on a change, so a detail card can follow its node without a
	 * tick. Local presentation only.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Skill Tree|Map")
	FRpgSkillTreeMapSelectedNodeMoved OnSelectedNodeMoved;

	//~ UWidget interface
	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
	virtual void ValidateCompiledDefaults(class IWidgetCompilerLog& CompileLog) const override;
#endif
	//~ End UWidget interface

protected:
	//~ UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	//~ End UWidget interface

	/** Widget created per node; must implement User Object List Entry. Presentation only; never receives presses. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Map", meta = (MustImplement = "/Script/UMG.UserObjectListEntry"))
	TSubclassOf<UUserWidget> NodeEntryClass;

	/**
	 * Optional widget created per shown tree, centered below its lowest node and scaled like the node entries, for example
	 * the tree's name and free points. Receives the URpgSkillTreeViewModel; must implement User Object List Entry.
	 * Presentation only; never receives presses. None shows no tree entries.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Map", meta = (MustImplement = "/Script/UMG.UserObjectListEntry"))
	TSubclassOf<UUserWidget> TreeEntryClass;

	/** Space in slate units between a tree's lowest node and its tree entry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Map|Entries", meta = (ClampMin = "0", UIMin = "0", UIMax = "64"))
	float TreeEntryGap = 8.0f;

	/** Smallest zoom in slate units per map unit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Map|Camera", meta = (ClampMin = "0.05", UIMin = "0.05", UIMax = "1"))
	float MinZoom = 0.25f;

	/** Largest zoom in slate units per map unit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Map|Camera", meta = (ClampMin = "0.1", UIMin = "1", UIMax = "4"))
	float MaxZoom = 2.0f;

	/** Zoom factor of one mouse wheel notch or one ZoomIn step. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Map|Camera", meta = (ClampMin = "1.01", UIMin = "1.05", UIMax = "2"))
	float ZoomStep = 1.15f;

	/** Zoom from which the detail level is Detail instead of Overview. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Map|Camera", meta = (ClampMin = "0.05", UIMin = "0.25", UIMax = "2"))
	float DetailZoom = 0.75f;

	/** Pointer travel in slate units that turns a press into a pan; shorter presses are clicks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Map|Input", meta = (ClampMin = "0", UIMin = "2", UIMax = "24"))
	float DragThreshold = 6.0f;

	/** Time constant in seconds with which the view glides to a new zoom, fit or focus; zero jumps. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Map|Camera", meta = (ClampMin = "0", UIMin = "0", UIMax = "0.5"))
	float CameraBlendTime = 0.12f;

	/** Free space in slate units around a framed tree or region. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Map|Camera", meta = (ClampMin = "0", UIMin = "0", UIMax = "200"))
	float FitPadding = 64.0f;

	/** Smallest scale entries are drawn with when zoomed out, so their art and hit targets stay readable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Map|Entries", meta = (ClampMin = "0.1", UIMin = "0.25", UIMax = "1"))
	float MinEntryScale = 0.5f;

	/** Largest scale entries are drawn with when zoomed in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Map|Entries", meta = (ClampMin = "0.1", UIMin = "0.5", UIMax = "2"))
	float MaxEntryScale = 1.0f;

	/** Smallest square in slate units a node's click target covers, whatever its entry's drawn size. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Map|Input", meta = (ClampMin = "0", UIMin = "32", UIMax = "96"))
	float MinHitTargetSize = 48.0f;

	/** Space in slate units kept between a navigated node and the map's edge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Map|Input", meta = (ClampMin = "0", UIMin = "0", UIMax = "200"))
	float FocusMargin = 48.0f;

	/** Drawn around the selected node while the map has keyboard or gamepad focus. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Map|Style")
	FSlateBrush FocusBrush;

	/** Space in slate units between the selected entry and FocusBrush. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Map|Style", meta = (ClampMin = "0", UIMax = "24"))
	float FocusPadding = 6.0f;

	/** Drawn behind each region of the tree. No Draw Type hides regions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Map|Style")
	FSlateBrush RegionBrush;

	/**
	 * Map art drawn behind the whole tree, centered on it, that pans and zooms with the nodes. No Draw Type hides it.
	 * Presentation only.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Map|Style")
	FSlateBrush BackgroundBrush;

	/** Side of the square BackgroundBrush as a multiple of the tree's larger side. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Map|Style", meta = (ClampMin = "0.1", UIMin = "0.5", UIMax = "3"))
	float BackgroundScale = 1.2f;

	/** Thickness of the prerequisite links in slate units. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Map|Style", meta = (ClampMin = "0.5", UIMin = "0.5", UIMax = "12"))
	float LinkThickness = 3.0f;

	/** Link color when both ends are learned. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Map|Style")
	FLinearColor LearnedLinkColor = FLinearColor(0.85f, 0.65f, 0.25f, 1.0f);

	/** Link color when only the prerequisite is learned, so the next node is reachable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Map|Style")
	FLinearColor AvailableLinkColor = FLinearColor(0.75f, 0.75f, 0.75f, 0.9f);

	/** Link color when the prerequisite is not learned. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Map|Style")
	FLinearColor LockedLinkColor = FLinearColor(0.3f, 0.3f, 0.3f, 0.6f);

	/** Remembers camera and selection per set of shown trees for the local player across screen close and reopen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Map")
	bool bRememberView = true;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FRpgSkillTreeMapWidgetLifecycleTest;
	friend class FRpgSkillTreeMapSeveralTreesTest;
	friend class FRpgSkillTreeMapSyntheticPerformanceTest;
	friend class FRpgSkillTreeMapSkillScreenContractTest;
#endif

	UFUNCTION()
	void HandleSkillTreeChanged();

	void HandlePanelNodeClicked(int32 NodeIndex);
	void HandlePanelNodeNavigated(int32 NodeIndex);
	void HandlePanelViewChanged();
	void HandlePanelSelectedNodeMoved(bool bHasSelection, const FSlateRect& LocalBox, const FVector2D& ViewportSize);

	/** A node on the map: node tags only need to be unique per tree. */
	struct FNodeKey
	{
		FGameplayTag TreeTag;
		FGameplayTag NodeTag;

		bool IsValid() const { return NodeTag.IsValid(); }
		bool operator==(const FNodeKey& Other) const = default;
	};

	/**
	 * Keeps one selection on the map, reuses node and tree entries by identity, creates missing ones, and refreshes
	 * positions, links and selection.
	 */
	void SyncEntries();
	void PushNodes();
	void PushTreeEntries();
	void PushLinksAndRegions();

	/** Shows the selection on the panel; a changed selection is brought into view and remembered. */
	void PushSelection();

	/** Clears the selection of every shown tree except KeepTree. Their refreshes sync the map again. */
	void ClearSelectionExcept(const URpgSkillTreeViewModel* KeepTree);

	/** When several trees hold a selection, keeps the newest one: one the map did not show as selected yet. */
	void KeepSingleSelection();

	/** Restores the remembered view of a newly shown set of trees, or frames its default region or every tree. */
	void InitializeView();
	void SaveViewState() const;
	void UpdateDetailLevel();
	void SelectNodeAt(int32 NodeIndex);
	FRpgSkillTreeMapSettings MakeSettings() const;
	URpgUIScreenSubsystem* GetViewStateCache() const;

	/** Identity under which the view is remembered: the shown tree tags in map order. None without a tree. */
	FName MakeViewMapId() const;
	FBox2D GetCombinedBounds() const;
	FNodeKey GetSelectedKey() const;
	URpgSkillTreeViewModel* FindTree(FGameplayTag TreeTag) const;
	URpgSkillTreeNodeViewModel* FindNodeViewModel(const FNodeKey& Key) const;

	/** Index of Key's node entry; without a tree tag, the first one with Key's node tag. INDEX_NONE when absent. */
	int32 FindEntryIndex(const FNodeKey& Key) const;

	TSharedPtr<SRpgSkillTreeMapPanel> MyPanel;

	/** Shown trees in map order. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<URpgSkillTreeViewModel>> SkillTrees;

	/** Entry per node, tree by tree in authored order, parallel to EntryKeys and EntryPositions. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UUserWidget>> NodeEntries;

	TArray<FNodeKey> EntryKeys;
	TArray<FVector2D> EntryPositions;

	/** Entry per shown tree, parallel to TreeEntryTags and TreeEntryPlacements; empty without a TreeEntryClass. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UUserWidget>> TreeEntries;

	TArray<FGameplayTag> TreeEntryTags;
	TArray<SRpgSkillTreeMapPanel::FTreeLabelPlacement> TreeEntryPlacements;

	/** Set of trees whose view the panel shows; another set restores or frames its own view. */
	FName ViewMapId;
	FNodeKey LastSelected;
	ERpgSkillTreeMapDetail DetailLevel = ERpgSkillTreeMapDetail::Overview;
	bool bViewInitialized = false;

	/** True while the map itself selects a node; the clicked or navigated node is already where the player looks. */
	bool bSelectingFromMap = false;

#if WITH_DEV_AUTOMATION_TESTS
	/** View cache used instead of the owning local player's, for tests without a local player. */
	TWeakObjectPtr<URpgUIScreenSubsystem> ViewStateCacheForTests;
#endif
};
