#include "RpgSkillTreeGridWidget.h"

#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "SurvivalRpg/Mvvm/SkillTrees/RpgSkillTreeViewModels.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgSkillTreeGridWidget)

#define LOCTEXT_NAMESPACE "RpgSkillTreeGridWidget"

/** Paints locked row bands and prerequisite links between the cell centers of a uniform grid of the same size. */
class SRpgSkillTreeLinks : public SLeafWidget
{
public:
	struct FLink
	{
		FIntPoint FromCell;
		FIntPoint ToCell;
		FLinearColor Color;
	};

	SLATE_BEGIN_ARGS(SRpgSkillTreeLinks) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		SetVisibility(EVisibility::HitTestInvisible);
	}

	void SetLayout(const FIntPoint InGridSize, TArray<int32> InLockedRows, TArray<FLink> InLinks, const FLinearColor& InRowColor, const float InThickness)
	{
		GridSize = InGridSize;
		LockedRows = MoveTemp(InLockedRows);
		Links = MoveTemp(InLinks);
		LockedRowColor = InRowColor;
		Thickness = InThickness;
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D::ZeroVector;
	}

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		const int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		const bool bParentEnabled) const override
	{
		if (GridSize.X <= 0 || GridSize.Y <= 0)
		{
			return LayerId;
		}

		// SUniformGridPanel divides its whole area evenly, so cell centers follow from the grid size alone.
		const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());
		const FVector2f Cell(Size.X / GridSize.X, Size.Y / GridSize.Y);
		const auto CellCenter = [&Cell](const FIntPoint CellIndex)
		{
			return FVector2f((CellIndex.X + 0.5f) * Cell.X, (CellIndex.Y + 0.5f) * Cell.Y);
		};

		if (LockedRowColor.A > 0.0f)
		{
			const FSlateBrush* Brush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
			for (const int32 Row : LockedRows)
			{
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					LayerId,
					AllottedGeometry.ToPaintGeometry(FVector2f(Size.X, Cell.Y), FSlateLayoutTransform(1.0f, FVector2f(0.0f, Row * Cell.Y))),
					Brush,
					ESlateDrawEffect::None,
					LockedRowColor * InWidgetStyle.GetColorAndOpacityTint());
			}
		}

		for (const FLink& Link : Links)
		{
			TArray<FVector2f> Points;
			Points.Add(CellCenter(Link.FromCell));
			Points.Add(CellCenter(Link.ToCell));
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId + 1,
				AllottedGeometry.ToPaintGeometry(),
				Points,
				ESlateDrawEffect::None,
				Link.Color * InWidgetStyle.GetColorAndOpacityTint(),
				true,
				Thickness);
		}
		return LayerId + 1;
	}

private:
	FIntPoint GridSize = FIntPoint::ZeroValue;
	TArray<int32> LockedRows;
	TArray<FLink> Links;
	FLinearColor LockedRowColor = FLinearColor::Transparent;
	float Thickness = 2.0f;
};

void URpgSkillTreeGridWidget::SetSkillTree(URpgSkillTreeViewModel* InSkillTree)
{
	if (SkillTree != InSkillTree)
	{
		if (SkillTree)
		{
			SkillTree->OnTreeChanged.RemoveDynamic(this, &ThisClass::HandleSkillTreeChanged);
		}
		SkillTree = InSkillTree;
		if (SkillTree)
		{
			SkillTree->OnTreeChanged.AddUniqueDynamic(this, &ThisClass::HandleSkillTreeChanged);
		}
	}
	SyncEntries();
}

TArray<UUserWidget*> URpgSkillTreeGridWidget::GetNodeEntries() const
{
	TArray<UUserWidget*> Result;
	Result.Reserve(NodeEntries.Num());
	for (UUserWidget* Entry : NodeEntries)
	{
		Result.Add(Entry);
	}
	return Result;
}

void URpgSkillTreeGridWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (MyGrid)
	{
		MyGrid->SetSlotPadding(CellPadding);
		MyGrid->SetMinDesiredSlotWidth(static_cast<float>(MinCellSize.X));
		MyGrid->SetMinDesiredSlotHeight(static_cast<float>(MinCellSize.Y));
	}
	PushLinks();
}

void URpgSkillTreeGridWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	if (bReleaseChildren)
	{
		for (UUserWidget* Entry : NodeEntries)
		{
			if (Entry)
			{
				Entry->ReleaseSlateResources(true);
			}
		}
	}
	MyGrid.Reset();
	MyLinks.Reset();
}

#if WITH_EDITOR
const FText URpgSkillTreeGridWidget::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "Rpg");
}
#endif

TSharedRef<SWidget> URpgSkillTreeGridWidget::RebuildWidget()
{
	MyGrid = SNew(SUniformGridPanel)
		.SlotPadding(CellPadding)
		.MinDesiredSlotWidth(static_cast<float>(MinCellSize.X))
		.MinDesiredSlotHeight(static_cast<float>(MinCellSize.Y));
	MyLinks = SNew(SRpgSkillTreeLinks);
	RebuildGridSlots();
	PushLinks();

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			MyLinks.ToSharedRef()
		]
		+ SOverlay::Slot()
		[
			MyGrid.ToSharedRef()
		];
}

void URpgSkillTreeGridWidget::HandleSkillTreeChanged()
{
	SyncEntries();
}

void URpgSkillTreeGridWidget::SyncEntries()
{
	const TArray<URpgSkillTreeNodeViewModel*> Nodes = SkillTree ? SkillTree->GetNodes() : TArray<URpgSkillTreeNodeViewModel*>();
	TArray<TPair<FName, FIntPoint>> Layout;
	Layout.Reserve(Nodes.Num());
	for (const URpgSkillTreeNodeViewModel* Node : Nodes)
	{
		Layout.Emplace(Node->GetNodeTag().GetTagName(), Node->GetCell());
	}
	const FIntPoint GridSize = SkillTree ? FIntPoint(SkillTree->GetColumnCount(), SkillTree->GetRowCount()) : FIntPoint::ZeroValue;

	const bool bEntriesMatchClass = NodeEntries.Num() == (NodeEntryClass ? Nodes.Num() : 0);
	if (Layout != BuiltLayout || GridSize != BuiltGridSize || !bEntriesMatchClass)
	{
		NodeEntries.Reset();
		if (NodeEntryClass)
		{
			for (int32 Index = 0; Index < Nodes.Num(); ++Index)
			{
				NodeEntries.Add(CreateWidget<UUserWidget>(this, NodeEntryClass));
			}
		}
		BuiltLayout = MoveTemp(Layout);
		BuiltGridSize = GridSize;
		RebuildGridSlots();
	}

	// Entries restyle themselves from the node view model on every change.
	for (int32 Index = 0; Index < NodeEntries.Num(); ++Index)
	{
		UUserWidget* Entry = NodeEntries[Index];
		if (Entry && Nodes.IsValidIndex(Index) && Entry->GetClass()->ImplementsInterface(UUserObjectListEntry::StaticClass()))
		{
			IUserObjectListEntry::Execute_OnListItemObjectSet(Entry, Nodes[Index]);
		}
	}
	PushLinks();
}

void URpgSkillTreeGridWidget::RebuildGridSlots()
{
	if (!MyGrid)
	{
		return;
	}

	MyGrid->ClearChildren();
	for (int32 Index = 0; Index < NodeEntries.Num(); ++Index)
	{
		if (UUserWidget* Entry = NodeEntries[Index]; Entry && BuiltLayout.IsValidIndex(Index))
		{
			const FIntPoint Cell = BuiltLayout[Index].Value;
			MyGrid->AddSlot(Cell.X, Cell.Y)
			[
				Entry->TakeWidget()
			];
		}
	}
}

void URpgSkillTreeGridWidget::PushLinks()
{
	if (!MyLinks)
	{
		return;
	}

	TArray<int32> LockedRows;
	TArray<SRpgSkillTreeLinks::FLink> Links;
	FIntPoint GridSize = FIntPoint::ZeroValue;
	if (SkillTree)
	{
		GridSize = FIntPoint(SkillTree->GetColumnCount(), SkillTree->GetRowCount());
		for (const FRpgSkillTreeRowView& Row : SkillTree->GetRows())
		{
			if (!Row.bIsUnlocked)
			{
				LockedRows.Add(Row.Row);
			}
		}
		for (const FRpgSkillTreeLinkView& LinkView : SkillTree->GetLinks())
		{
			SRpgSkillTreeLinks::FLink& Link = Links.AddDefaulted_GetRef();
			Link.FromCell = LinkView.FromCell;
			Link.ToCell = LinkView.ToCell;
			Link.Color = LinkView.bFromUnlocked && LinkView.bToUnlocked
				? LearnedLinkColor
				: LinkView.bFromUnlocked ? AvailableLinkColor : LockedLinkColor;
		}
	}
	MyLinks->SetLayout(GridSize, MoveTemp(LockedRows), MoveTemp(Links), LockedRowColor, LinkThickness);
}

#undef LOCTEXT_NAMESPACE
