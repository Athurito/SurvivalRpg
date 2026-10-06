#pragma once

#include "Components/Widget.h"

#include "RpgSkillTreeGridWidget.generated.h"

class SRpgSkillTreeLinks;
class SUniformGridPanel;
class UUserWidget;
class URpgSkillTreeViewModel;

/**
 * Lays out any skill tree on a uniform grid from each node's Row and Column and draws the prerequisite links and the
 * rows whose point gate is not reached yet behind the nodes.
 *
 * One NodeEntryClass widget is created per node. The class must implement the User Object List Entry interface; its
 * On List Item Object Set event receives the URpgSkillTreeNodeViewModel again after every tree change, so the entry
 * restyles itself there. Get List Item Object is not available because the grid is not a list view. The grid owns no
 * gameplay state: new nodes or trees need no widget change.
 */
UCLASS(meta = (DisplayName = "Rpg Skill Tree Grid"))
class SURVIVALRPG_API URpgSkillTreeGridWidget : public UWidget
{
	GENERATED_BODY()

public:
	/** Shows InSkillTree, or clears the grid with null. The grid follows the view model's later changes on its own. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|Grid")
	void SetSkillTree(URpgSkillTreeViewModel* InSkillTree);

	/** Tree currently shown, or null. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|Grid")
	URpgSkillTreeViewModel* GetSkillTree() const { return SkillTree; }

	/** Entry widget created for each node, in authored node order. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|Grid")
	TArray<UUserWidget*> GetNodeEntries() const;

	//~ UWidget interface
	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif
	//~ End UWidget interface

protected:
	//~ UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	//~ End UWidget interface

	/** Widget created per node; must implement User Object List Entry. Presentation only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Grid", meta = (MustImplement = "/Script/UMG.UserObjectListEntry"))
	TSubclassOf<UUserWidget> NodeEntryClass;

	/** Smallest size of one grid cell in slate units; the grid grows with its entries and the space it is given. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Grid", meta = (ClampMin = "0"))
	FVector2D MinCellSize = FVector2D(112.0, 112.0);

	/** Space around each entry inside its cell. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Grid")
	FMargin CellPadding = FMargin(12.0f);

	/** Thickness of the prerequisite links in slate units. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Links", meta = (ClampMin = "0.5", UIMin = "0.5", UIMax = "12"))
	float LinkThickness = 3.0f;

	/** Link color when both ends are learned. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Links")
	FLinearColor LearnedLinkColor = FLinearColor(0.85f, 0.65f, 0.25f, 1.0f);

	/** Link color when only the prerequisite is learned, so the next node is reachable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Links")
	FLinearColor AvailableLinkColor = FLinearColor(0.75f, 0.75f, 0.75f, 0.9f);

	/** Link color when the prerequisite is not learned. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Links")
	FLinearColor LockedLinkColor = FLinearColor(0.3f, 0.3f, 0.3f, 0.6f);

	/** Band drawn behind rows whose point gate is not reached. Fully transparent hides it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|Links")
	FLinearColor LockedRowColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.35f);

private:
	UFUNCTION()
	void HandleSkillTreeChanged();

	/** Recreates the entries when the node set or the grid size changed, then refreshes everything. */
	void SyncEntries();
	void RebuildGridSlots();
	void PushLinks();

	TSharedPtr<SUniformGridPanel> MyGrid;
	TSharedPtr<SRpgSkillTreeLinks> MyLinks;

	UPROPERTY(Transient)
	TObjectPtr<URpgSkillTreeViewModel> SkillTree = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UUserWidget>> NodeEntries;

	/** Node tags and cells the entries were built for; a difference recreates them. */
	TArray<TPair<FName, FIntPoint>> BuiltLayout;
	FIntPoint BuiltGridSize = FIntPoint::ZeroValue;
};
