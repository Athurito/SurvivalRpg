#pragma once

#include "Components/Widget.h"
#include "GameplayTagContainer.h"
#include "Styling/SlateBrush.h"

#include "RpgSkillTreeMapWidget.generated.h"

class SRpgSkillTreeMapPanel;
class UUserWidget;
class URpgSkillTreeNodeViewModel;
class URpgSkillTreeViewModel;
class URpgUIScreenSubsystem;
struct FRpgSkillTreeMapSettings;

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

/**
 * Zoomable, pannable mastery map of one skill tree. Places one NodeEntryClass widget per node at its map position,
 * draws regions and prerequisite links behind them, and owns every pointer gesture on the map.
 *
 * - The mouse wheel zooms around the cursor; a drag with any button pans once it passes DragThreshold.
 * - A left click without a drag selects the node under the cursor. Nothing on the map learns or refunds; those are
 *   explicit commands of the detail panel.
 * - Keyboard and gamepad navigation moves the selection spatially and brings the node into view; at the map's edge
 *   focus leaves for the neighboring widgets.
 * - Camera and selection are remembered per tree for the local player, so closing and reopening the screen restores
 *   them, also for pooled screens.
 *
 * Entries are presentation only and never receive presses. They implement User Object List Entry and receive their
 * URpgSkillTreeNodeViewModel again after every tree change; URpgMvvmListEntryWidget children get it as their entry
 * item. Pan and zoom never recreate entries or view models. The map owns no gameplay state.
 */
UCLASS(meta = (DisplayName = "Rpg Skill Tree Map"))
class SURVIVALRPG_API URpgSkillTreeMapWidget : public UWidget
{
	GENERATED_BODY()

public:
	explicit URpgSkillTreeMapWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Shows InSkillTree, or clears the map with null. The map follows the view model's later changes on its own. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|Map")
	void SetSkillTree(URpgSkillTreeViewModel* InSkillTree);

	/** Tree currently shown, or null. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|Map")
	URpgSkillTreeViewModel* GetSkillTree() const { return SkillTree; }

	/** Entry widget created for each node, in authored node order. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|Map")
	TArray<UUserWidget*> GetNodeEntries() const;

	/** Entry widget of NodeTag, or null. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|Map")
	UUserWidget* GetNodeEntry(FGameplayTag NodeTag) const;

	/** Zooms in one step around the viewport center, for zoom buttons and actions without a mouse wheel. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|Map")
	void ZoomIn();

	/** Zooms out one step around the viewport center. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|Map")
	void ZoomOut();

	/** Sets the zoom around the viewport center, clamped to [MinZoom, MaxZoom]. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|Map")
	void SetZoom(float NewZoom);

	/** Frames the whole tree. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|Map")
	void FitTree();

	/** Frames RegionId, for region shortcuts. Returns false when the tree has no such region. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|Map")
	bool FocusRegion(FName RegionId);

	/** Centers the view on NodeTag without selecting it. Returns false when the tree has no such node. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|Map")
	bool FocusNode(FGameplayTag NodeTag);

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

	/** Remembers camera and selection per tree for the local player across screen close and reopen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Map")
	bool bRememberView = true;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FRpgSkillTreeMapWidgetLifecycleTest;
#endif

	UFUNCTION()
	void HandleSkillTreeChanged();

	void HandlePanelNodeClicked(int32 NodeIndex);
	void HandlePanelNodeNavigated(int32 NodeIndex);
	void HandlePanelViewChanged();

	/** Reuses entries by node tag, creates missing ones, and refreshes positions, links and selection. */
	void SyncEntries();
	void PushNodes();
	void PushLinksAndRegions();

	/** Shows the selection on the panel; a changed selection is brought into view and remembered. */
	void PushSelection();

	/** Restores the remembered view of a newly shown tree, or frames its default region. */
	void InitializeView();
	void SaveViewState() const;
	void UpdateDetailLevel();
	void SelectNodeAt(int32 NodeIndex);
	FRpgSkillTreeMapSettings MakeSettings() const;
	URpgUIScreenSubsystem* GetViewStateCache() const;
	FGameplayTag GetSelectedNodeTag() const;

	TSharedPtr<SRpgSkillTreeMapPanel> MyPanel;

	UPROPERTY(Transient)
	TObjectPtr<URpgSkillTreeViewModel> SkillTree = nullptr;

	/** Entry per node in authored order, parallel to EntryNodeTags and EntryPositions. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UUserWidget>> NodeEntries;

	TArray<FGameplayTag> EntryNodeTags;
	TArray<FVector2D> EntryPositions;

	/** Tree whose view the panel shows; another tree restores or frames its own view. */
	FGameplayTag ViewTreeTag;
	FGameplayTag LastSelectedTag;
	ERpgSkillTreeMapDetail DetailLevel = ERpgSkillTreeMapDetail::Overview;
	bool bViewInitialized = false;

#if WITH_DEV_AUTOMATION_TESTS
	/** View cache used instead of the owning local player's, for tests without a local player. */
	TWeakObjectPtr<URpgUIScreenSubsystem> ViewStateCacheForTests;
#endif
};
