#include "RpgSkillTreeMapWidget.h"

#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/LocalPlayer.h"
#include "SRpgSkillTreeMapPanel.h"
#include "SurvivalRpg/Mvvm/SkillTrees/RpgSkillTreeViewModels.h"
#include "SurvivalRpg/UI/RpgMvvmListEntryWidgets.h"
#include "SurvivalRpg/UI/RpgUIScreenSubsystem.h"

#if WITH_EDITOR
#include "Editor/WidgetCompilerLog.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgSkillTreeMapWidget)

#define LOCTEXT_NAMESPACE "RpgSkillTreeMapWidget"

namespace
{
	/** Hands Node to Entry the way URpgViewModelEntryBox does, so MVVM entry rows and plain list entries both work. */
	void SetEntryNode(UUserWidget& Entry, URpgSkillTreeNodeViewModel* Node)
	{
		if (URpgMvvmListEntryWidget* MvvmEntry = Cast<URpgMvvmListEntryWidget>(&Entry))
		{
			MvvmEntry->SetEntryItem(Node);
		}
		else if (URpgMvvmListEntryButton* MvvmButton = Cast<URpgMvvmListEntryButton>(&Entry))
		{
			MvvmButton->SetEntryItem(Node);
		}
		else if (Entry.Implements<UUserObjectListEntry>())
		{
			IUserObjectListEntry::Execute_OnListItemObjectSet(&Entry, Node);
		}
	}
}

URpgSkillTreeMapWidget::URpgSkillTreeMapWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// UWidget pushes its Clipping onto the Slate panel, so the map must default to clipping or entries spill over
	// neighboring panels.
	SetClipping(EWidgetClipping::ClipToBounds);

	// Every focusable control needs a visible focus state; regions draw nothing until a screen styles them.
	FocusBrush = FSlateRoundedBoxBrush(FLinearColor::Transparent, 8.0f, FLinearColor(0.95f, 0.8f, 0.45f, 1.0f), 2.0f);
	RegionBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	BackgroundBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
}

void URpgSkillTreeMapWidget::SetSkillTree(URpgSkillTreeViewModel* InSkillTree)
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

TArray<UUserWidget*> URpgSkillTreeMapWidget::GetNodeEntries() const
{
	TArray<UUserWidget*> Result;
	Result.Reserve(NodeEntries.Num());
	for (UUserWidget* Entry : NodeEntries)
	{
		Result.Add(Entry);
	}
	return Result;
}

UUserWidget* URpgSkillTreeMapWidget::GetNodeEntry(const FGameplayTag NodeTag) const
{
	const int32 Index = EntryNodeTags.IndexOfByKey(NodeTag);
	return NodeEntries.IsValidIndex(Index) ? NodeEntries[Index].Get() : nullptr;
}

void URpgSkillTreeMapWidget::ZoomIn()
{
	if (MyPanel)
	{
		MyPanel->ZoomBySteps(1.0);
	}
}

void URpgSkillTreeMapWidget::ZoomOut()
{
	if (MyPanel)
	{
		MyPanel->ZoomBySteps(-1.0);
	}
}

void URpgSkillTreeMapWidget::SetZoom(const float NewZoom)
{
	if (MyPanel)
	{
		MyPanel->ZoomAtLocal(MyPanel->GetViewportSize() * 0.5, NewZoom);
	}
}

void URpgSkillTreeMapWidget::FitTree()
{
	if (MyPanel && SkillTree)
	{
		MyPanel->RequestFit(SkillTree->GetMapBounds(), true);
	}
}

bool URpgSkillTreeMapWidget::FocusRegion(const FName RegionId)
{
	const FRpgSkillTreeRegionView* Region = SkillTree ? SkillTree->FindRegion(RegionId) : nullptr;
	if (!Region || !Region->Bounds.bIsValid)
	{
		return false;
	}
	if (MyPanel)
	{
		MyPanel->RequestFit(Region->Bounds, true);
	}
	return true;
}

bool URpgSkillTreeMapWidget::FocusNode(const FGameplayTag NodeTag)
{
	const int32 Index = EntryNodeTags.IndexOfByKey(NodeTag);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	if (MyPanel)
	{
		MyPanel->CenterOnNode(Index);
	}
	return true;
}

void URpgSkillTreeMapWidget::PanBy(const FVector2D Delta)
{
	if (MyPanel)
	{
		MyPanel->PanByLocal(Delta);
	}
}

void URpgSkillTreeMapWidget::CancelGesture()
{
	if (MyPanel)
	{
		MyPanel->CancelGesture();
	}
}

float URpgSkillTreeMapWidget::GetZoom() const
{
	return MyPanel ? static_cast<float>(MyPanel->GetCamera().Zoom) : 1.0f;
}

FVector2D URpgSkillTreeMapWidget::GetViewCenter() const
{
	return MyPanel ? MyPanel->GetCamera().Center : FVector2D::ZeroVector;
}

bool URpgSkillTreeMapWidget::IsPanning() const
{
	return MyPanel && MyPanel->IsDragging();
}

void URpgSkillTreeMapWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (MyPanel)
	{
		MyPanel->SetSettings(MakeSettings());
		PushLinksAndRegions();
		UpdateDetailLevel();
	}
}

void URpgSkillTreeMapWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	if (MyPanel)
	{
		SaveViewState();
		MyPanel->CancelGesture();
	}
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
	MyPanel.Reset();
	bViewInitialized = false;
}

#if WITH_EDITOR
const FText URpgSkillTreeMapWidget::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "Rpg");
}

void URpgSkillTreeMapWidget::ValidateCompiledDefaults(IWidgetCompilerLog& CompileLog) const
{
	Super::ValidateCompiledDefaults(CompileLog);
	if (!NodeEntryClass)
	{
		CompileLog.Error(FText::Format(
			LOCTEXT("MissingNodeEntryClass", "{0} has no NodeEntryClass, so the map shows no nodes."),
			FText::FromName(GetFName())));
	}
}
#endif

TSharedRef<SWidget> URpgSkillTreeMapWidget::RebuildWidget()
{
	MyPanel = SNew(SRpgSkillTreeMapPanel)
		.OnNodeClicked_UObject(this, &ThisClass::HandlePanelNodeClicked)
		.OnNodeNavigated_UObject(this, &ThisClass::HandlePanelNodeNavigated)
		.OnViewChanged_UObject(this, &ThisClass::HandlePanelViewChanged);
	MyPanel->SetSettings(MakeSettings());

	// A new panel starts with a default camera: restore the remembered view or frame the tree again.
	bViewInitialized = false;
	PushNodes();
	PushLinksAndRegions();
	InitializeView();
	PushSelection();
	return MyPanel.ToSharedRef();
}

void URpgSkillTreeMapWidget::HandleSkillTreeChanged()
{
	SyncEntries();
}

void URpgSkillTreeMapWidget::HandlePanelNodeClicked(const int32 NodeIndex)
{
	SelectNodeAt(NodeIndex);
}

void URpgSkillTreeMapWidget::HandlePanelNodeNavigated(const int32 NodeIndex)
{
	SelectNodeAt(NodeIndex);
}

void URpgSkillTreeMapWidget::HandlePanelViewChanged()
{
	UpdateDetailLevel();
	SaveViewState();
}

void URpgSkillTreeMapWidget::SyncEntries()
{
	const TArray<URpgSkillTreeNodeViewModel*> Nodes = SkillTree ? SkillTree->GetNodes() : TArray<URpgSkillTreeNodeViewModel*>();

	// The old tree's view is already remembered. Pushing the new tree's bounds clamps the camera, which must not be
	// saved under the old tree; InitializeView then restores or frames the new one.
	if (bViewInitialized && (SkillTree ? SkillTree->GetTreeTag() : FGameplayTag()) != ViewTreeTag)
	{
		bViewInitialized = false;
	}

	// Entries stay the same widgets per node tag, so their presentation state survives every refresh.
	TMap<FGameplayTag, UUserWidget*> PreviousEntries;
	for (int32 Index = 0; Index < NodeEntries.Num() && Index < EntryNodeTags.Num(); ++Index)
	{
		PreviousEntries.Add(EntryNodeTags[Index], NodeEntries[Index]);
	}

	TArray<TObjectPtr<UUserWidget>> NewEntries;
	TArray<FGameplayTag> NewTags;
	TArray<FVector2D> NewPositions;
	if (NodeEntryClass)
	{
		for (URpgSkillTreeNodeViewModel* Node : Nodes)
		{
			UUserWidget* Entry = PreviousEntries.FindRef(Node->GetNodeTag());
			if (!Entry || Entry->GetClass() != NodeEntryClass)
			{
				Entry = CreateWidget<UUserWidget>(this, NodeEntryClass);
			}
			if (!Entry)
			{
				continue;
			}
			NewEntries.Add(Entry);
			NewTags.Add(Node->GetNodeTag());
			NewPositions.Add(Node->GetMapPosition());
		}
	}

	bool bEntriesChanged = NewEntries.Num() != NodeEntries.Num();
	for (int32 Index = 0; !bEntriesChanged && Index < NewEntries.Num(); ++Index)
	{
		bEntriesChanged = NewEntries[Index] != NodeEntries[Index];
	}
	const bool bPositionsChanged = NewPositions != EntryPositions;
	NodeEntries = MoveTemp(NewEntries);
	EntryNodeTags = MoveTemp(NewTags);
	EntryPositions = MoveTemp(NewPositions);
	if (MyPanel)
	{
		if (bEntriesChanged)
		{
			PushNodes();
		}
		else if (bPositionsChanged)
		{
			MyPanel->SetNodePositions(EntryPositions);
		}
	}

	// Entries restyle themselves from their node view model on every change.
	for (int32 Index = 0; Index < NodeEntries.Num(); ++Index)
	{
		if (UUserWidget* Entry = NodeEntries[Index])
		{
			SetEntryNode(*Entry, SkillTree ? SkillTree->FindNode(EntryNodeTags[Index]) : nullptr);
		}
	}

	PushLinksAndRegions();
	InitializeView();
	PushSelection();
}

void URpgSkillTreeMapWidget::PushNodes()
{
	if (!MyPanel)
	{
		return;
	}

	TArray<TSharedRef<SWidget>> Widgets;
	Widgets.Reserve(NodeEntries.Num());
	for (UUserWidget* Entry : NodeEntries)
	{
		Widgets.Add(Entry->TakeWidget());
	}
	MyPanel->SetNodes(Widgets, EntryPositions);
}

void URpgSkillTreeMapWidget::PushLinksAndRegions()
{
	if (!MyPanel)
	{
		return;
	}

	TArray<SRpgSkillTreeMapPanel::FLink> Links;
	TArray<FBox2D> Regions;
	if (SkillTree)
	{
		for (const FRpgSkillTreeLinkView& LinkView : SkillTree->GetLinks())
		{
			SRpgSkillTreeMapPanel::FLink& Link = Links.AddDefaulted_GetRef();
			Link.From = LinkView.FromPosition;
			Link.To = LinkView.ToPosition;
			Link.Color = LinkView.bFromUnlocked && LinkView.bToUnlocked
				? LearnedLinkColor
				: LinkView.bFromUnlocked ? AvailableLinkColor : LockedLinkColor;
		}
		for (const FRpgSkillTreeRegionView& Region : SkillTree->GetRegions())
		{
			Regions.Add(Region.Bounds);
		}
	}
	MyPanel->SetLinks(MoveTemp(Links));
	MyPanel->SetRegions(MoveTemp(Regions));
	MyPanel->SetContentBounds(SkillTree ? SkillTree->GetMapBounds() : FBox2D(ForceInit));
}

void URpgSkillTreeMapWidget::PushSelection()
{
	if (!MyPanel)
	{
		return;
	}

	const FGameplayTag SelectedTag = GetSelectedNodeTag();
	const int32 SelectedIndex = EntryNodeTags.IndexOfByKey(SelectedTag);
	MyPanel->SetSelectedIndex(SelectedIndex);
	if (SelectedTag != LastSelectedTag)
	{
		LastSelectedTag = SelectedTag;
		if (!bSelectingFromMap)
		{
			MyPanel->BringNodeIntoView(SelectedIndex);
		}
		SaveViewState();
	}
}

void URpgSkillTreeMapWidget::InitializeView()
{
	const FGameplayTag TreeTag = SkillTree ? SkillTree->GetTreeTag() : FGameplayTag();
	if (!MyPanel || !TreeTag.IsValid() || (bViewInitialized && TreeTag == ViewTreeTag))
	{
		return;
	}

	ViewTreeTag = TreeTag;
	bViewInitialized = true;
	LastSelectedTag = GetSelectedNodeTag();

	const URpgUIScreenSubsystem* Cache = bRememberView ? GetViewStateCache() : nullptr;
	if (const FRpgUIMapViewState* RememberedState = Cache ? Cache->FindMapViewState(TreeTag) : nullptr)
	{
		// Copy first: applying the camera saves the view again and overwrites the cached entry.
		const FRpgUIMapViewState State = *RememberedState;

		// A pooled screen may still hold its selection; otherwise the remembered one comes back.
		const bool bRestoreSelection =
			!LastSelectedTag.IsValid() && State.SelectedTag.IsValid() && SkillTree->FindNode(State.SelectedTag);
		if (bRestoreSelection)
		{
			LastSelectedTag = State.SelectedTag;
		}

		FRpgSkillTreeMapCamera Camera;
		Camera.Center = State.Center;
		Camera.Zoom = State.Zoom;
		MyPanel->SetCamera(Camera);
		UpdateDetailLevel();
		if (bRestoreSelection)
		{
			SkillTree->SelectNode(State.SelectedTag);
		}
		return;
	}

	const FRpgSkillTreeRegionView* DefaultRegion = SkillTree->FindRegion(SkillTree->GetDefaultRegionId());
	MyPanel->RequestFit(DefaultRegion && DefaultRegion->Bounds.bIsValid ? DefaultRegion->Bounds : SkillTree->GetMapBounds());
	UpdateDetailLevel();
}

void URpgSkillTreeMapWidget::SaveViewState() const
{
	// A pending fit still shows the default camera; the fit itself reports the view worth remembering.
	if (!bRememberView || !bViewInitialized || !ViewTreeTag.IsValid() || !MyPanel || MyPanel->HasPendingFit())
	{
		return;
	}

	if (URpgUIScreenSubsystem* Cache = GetViewStateCache())
	{
		FRpgUIMapViewState State;
		State.Center = MyPanel->GetCamera().Center;
		State.Zoom = MyPanel->GetCamera().Zoom;
		State.SelectedTag = LastSelectedTag;
		Cache->SetMapViewState(ViewTreeTag, State);
	}
}

void URpgSkillTreeMapWidget::UpdateDetailLevel()
{
	const ERpgSkillTreeMapDetail NewDetail = GetZoom() >= DetailZoom ? ERpgSkillTreeMapDetail::Detail : ERpgSkillTreeMapDetail::Overview;
	if (DetailLevel != NewDetail)
	{
		DetailLevel = NewDetail;
		OnDetailLevelChanged.Broadcast(DetailLevel);
	}
}

void URpgSkillTreeMapWidget::SelectNodeAt(const int32 NodeIndex)
{
	URpgSkillTreeNodeViewModel* Node = SkillTree && EntryNodeTags.IsValidIndex(NodeIndex)
		? SkillTree->FindNode(EntryNodeTags[NodeIndex])
		: nullptr;
	if (!Node)
	{
		return;
	}

	// Selecting refreshes the tree view model, which calls SyncEntries; push again for an unchanged selection. A click
	// never moves the camera, and navigation already brought the node into view.
	TGuardValue<bool> SelectingGuard(bSelectingFromMap, true);
	Node->Select();
	PushSelection();
	OnNodeSelected.Broadcast(Node);
}

FRpgSkillTreeMapSettings URpgSkillTreeMapWidget::MakeSettings() const
{
	FRpgSkillTreeMapSettings Settings;
	Settings.MinZoom = MinZoom;
	Settings.MaxZoom = MaxZoom;
	Settings.ZoomStep = ZoomStep;
	Settings.DragThreshold = DragThreshold;
	Settings.FitPadding = FitPadding;
	Settings.MinEntryScale = MinEntryScale;
	Settings.MaxEntryScale = MaxEntryScale;
	Settings.MinHitTargetSize = MinHitTargetSize;
	Settings.FocusMargin = FocusMargin;
	Settings.LinkThickness = LinkThickness;
	Settings.RegionBrush = &RegionBrush;
	Settings.FocusBrush = &FocusBrush;
	Settings.FocusPadding = FocusPadding;
	Settings.CameraBlendTime = CameraBlendTime;
	Settings.BackgroundBrush = &BackgroundBrush;
	Settings.BackgroundScale = BackgroundScale;
	return Settings;
}

URpgUIScreenSubsystem* URpgSkillTreeMapWidget::GetViewStateCache() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (URpgUIScreenSubsystem* TestCache = ViewStateCacheForTests.Get())
	{
		return TestCache;
	}
#endif
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetSubsystem<URpgUIScreenSubsystem>() : nullptr;
}

FGameplayTag URpgSkillTreeMapWidget::GetSelectedNodeTag() const
{
	if (SkillTree)
	{
		for (const URpgSkillTreeNodeViewModel* Node : SkillTree->GetNodes())
		{
			if (Node && Node->IsSelected())
			{
				return Node->GetNodeTag();
			}
		}
	}
	return FGameplayTag();
}

#undef LOCTEXT_NAMESPACE
