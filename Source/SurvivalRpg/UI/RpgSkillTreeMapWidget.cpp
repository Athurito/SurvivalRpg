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
	/** Hands Item to Entry the way URpgViewModelEntryBox does, so MVVM entry rows and plain list entries both work. */
	void AssignEntryItem(UUserWidget& Entry, UObject* Item)
	{
		if (URpgMvvmListEntryWidget* MvvmEntry = Cast<URpgMvvmListEntryWidget>(&Entry))
		{
			MvvmEntry->SetEntryItem(Item);
		}
		else if (URpgMvvmListEntryButton* MvvmButton = Cast<URpgMvvmListEntryButton>(&Entry))
		{
			MvvmButton->SetEntryItem(Item);
		}
		else if (Entry.Implements<UUserObjectListEntry>())
		{
			IUserObjectListEntry::Execute_OnListItemObjectSet(&Entry, Item);
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

void URpgSkillTreeMapWidget::SetSkillTrees(const TArray<URpgSkillTreeViewModel*>& InSkillTrees)
{
	TArray<TObjectPtr<URpgSkillTreeViewModel>> NewTrees;
	for (URpgSkillTreeViewModel* Tree : InSkillTrees)
	{
		if (Tree && !NewTrees.Contains(Tree))
		{
			NewTrees.Add(Tree);
		}
	}

	for (URpgSkillTreeViewModel* OldTree : SkillTrees)
	{
		if (OldTree && !NewTrees.Contains(OldTree))
		{
			OldTree->OnTreeChanged.RemoveDynamic(this, &ThisClass::HandleSkillTreeChanged);
		}
	}
	for (URpgSkillTreeViewModel* NewTree : NewTrees)
	{
		NewTree->OnTreeChanged.AddUniqueDynamic(this, &ThisClass::HandleSkillTreeChanged);
	}
	SkillTrees = MoveTemp(NewTrees);
	SyncEntries();
}

void URpgSkillTreeMapWidget::SetSkillTree(URpgSkillTreeViewModel* InSkillTree)
{
	TArray<URpgSkillTreeViewModel*> Trees;
	if (InSkillTree)
	{
		Trees.Add(InSkillTree);
	}
	SetSkillTrees(Trees);
}

TArray<URpgSkillTreeViewModel*> URpgSkillTreeMapWidget::GetSkillTrees() const
{
	TArray<URpgSkillTreeViewModel*> Result;
	Result.Reserve(SkillTrees.Num());
	for (URpgSkillTreeViewModel* Tree : SkillTrees)
	{
		Result.Add(Tree);
	}
	return Result;
}

URpgSkillTreeViewModel* URpgSkillTreeMapWidget::GetSkillTree() const
{
	return SkillTrees.IsEmpty() ? nullptr : SkillTrees[0].Get();
}

URpgSkillTreeNodeViewModel* URpgSkillTreeMapWidget::GetSelectedNode() const
{
	return FindNodeViewModel(GetSelectedKey());
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
	return GetTreeNodeEntry(FGameplayTag(), NodeTag);
}

UUserWidget* URpgSkillTreeMapWidget::GetTreeNodeEntry(const FGameplayTag TreeTag, const FGameplayTag NodeTag) const
{
	const int32 Index = FindEntryIndex({TreeTag, NodeTag});
	return NodeEntries.IsValidIndex(Index) ? NodeEntries[Index].Get() : nullptr;
}

UUserWidget* URpgSkillTreeMapWidget::GetTreeEntry(const FGameplayTag TreeTag) const
{
	const int32 Index = TreeTag.IsValid() ? TreeEntryTags.IndexOfByKey(TreeTag) : INDEX_NONE;
	return TreeEntries.IsValidIndex(Index) ? TreeEntries[Index].Get() : nullptr;
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
	if (const FBox2D Bounds = GetCombinedBounds(); MyPanel && Bounds.bIsValid)
	{
		MyPanel->RequestFit(Bounds, true);
	}
}

bool URpgSkillTreeMapWidget::FocusTree(const FGameplayTag TreeTag)
{
	const URpgSkillTreeViewModel* Tree = FindTree(TreeTag);
	if (!Tree || !Tree->GetMapBounds().bIsValid)
	{
		return false;
	}
	if (MyPanel)
	{
		MyPanel->RequestFit(Tree->GetMapBounds(), true);
	}
	return true;
}

bool URpgSkillTreeMapWidget::FocusRegion(const FName RegionId)
{
	for (const URpgSkillTreeViewModel* Tree : SkillTrees)
	{
		if (Tree && FocusTreeRegion(Tree->GetTreeTag(), RegionId))
		{
			return true;
		}
	}
	return false;
}

bool URpgSkillTreeMapWidget::FocusTreeRegion(const FGameplayTag TreeTag, const FName RegionId)
{
	const URpgSkillTreeViewModel* Tree = FindTree(TreeTag);
	const FRpgSkillTreeRegionView* Region = Tree ? Tree->FindRegion(RegionId) : nullptr;
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
	return FocusTreeNode(FGameplayTag(), NodeTag);
}

bool URpgSkillTreeMapWidget::FocusTreeNode(const FGameplayTag TreeTag, const FGameplayTag NodeTag)
{
	const int32 Index = FindEntryIndex({TreeTag, NodeTag});
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
		for (UUserWidget* Entry : TreeEntries)
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
		.OnViewChanged_UObject(this, &ThisClass::HandlePanelViewChanged)
		.OnSelectedNodeMoved_UObject(this, &ThisClass::HandlePanelSelectedNodeMoved);
	MyPanel->SetSettings(MakeSettings());

	// A new panel starts with a default camera: restore the remembered view or frame the trees again.
	bViewInitialized = false;
	PushNodes();
	PushTreeEntries();
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

void URpgSkillTreeMapWidget::HandlePanelSelectedNodeMoved(
	const bool bHasSelection,
	const FSlateRect& LocalBox,
	const FVector2D& ViewportSize)
{
	OnSelectedNodeMoved.Broadcast(bHasSelection, LocalBox.GetTopLeft(), LocalBox.GetSize(), ViewportSize);
}

void URpgSkillTreeMapWidget::SyncEntries()
{
	KeepSingleSelection();

	// The old trees' view is already remembered. Pushing the new trees' bounds clamps the camera, which must not be
	// saved under the old set; InitializeView then restores or frames the new one.
	if (bViewInitialized && MakeViewMapId() != ViewMapId)
	{
		bViewInitialized = false;
	}

	// Entries stay the same widgets per node, so their presentation state survives every refresh.
	TMap<FGameplayTag, TMap<FGameplayTag, UUserWidget*>> PreviousEntries;
	for (int32 Index = 0; Index < NodeEntries.Num() && Index < EntryKeys.Num(); ++Index)
	{
		PreviousEntries.FindOrAdd(EntryKeys[Index].TreeTag).Add(EntryKeys[Index].NodeTag, NodeEntries[Index]);
	}
	TMap<FGameplayTag, UUserWidget*> PreviousTreeEntries;
	for (int32 Index = 0; Index < TreeEntries.Num() && Index < TreeEntryTags.Num(); ++Index)
	{
		PreviousTreeEntries.Add(TreeEntryTags[Index], TreeEntries[Index]);
	}

	TArray<TObjectPtr<UUserWidget>> NewEntries;
	TArray<FNodeKey> NewKeys;
	TArray<FVector2D> NewPositions;
	TArray<TObjectPtr<UUserWidget>> NewTreeEntries;
	TArray<FGameplayTag> NewTreeTags;
	TArray<SRpgSkillTreeMapPanel::FTreeLabelPlacement> NewPlacements;
	for (URpgSkillTreeViewModel* Tree : SkillTrees)
	{
		const FGameplayTag TreeTag = Tree ? Tree->GetTreeTag() : FGameplayTag();
		if (!TreeTag.IsValid())
		{
			continue;
		}

		const int32 FirstNode = NewEntries.Num();
		const TMap<FGameplayTag, UUserWidget*>* PreviousTreeNodes = PreviousEntries.Find(TreeTag);
		if (NodeEntryClass)
		{
			for (URpgSkillTreeNodeViewModel* Node : Tree->GetNodes())
			{
				UUserWidget* Entry = PreviousTreeNodes ? PreviousTreeNodes->FindRef(Node->GetNodeTag()) : nullptr;
				if (!Entry || Entry->GetClass() != NodeEntryClass)
				{
					Entry = CreateWidget<UUserWidget>(this, NodeEntryClass);
				}
				if (!Entry)
				{
					continue;
				}
				NewEntries.Add(Entry);
				NewKeys.Add({TreeTag, Node->GetNodeTag()});
				NewPositions.Add(Node->GetMapPosition());
			}
		}

		if (TreeEntryClass)
		{
			UUserWidget* TreeEntry = PreviousTreeEntries.FindRef(TreeTag);
			if (!TreeEntry || TreeEntry->GetClass() != TreeEntryClass)
			{
				TreeEntry = CreateWidget<UUserWidget>(this, TreeEntryClass);
			}
			if (TreeEntry)
			{
				const FBox2D& Bounds = Tree->GetMapBounds();
				SRpgSkillTreeMapPanel::FTreeLabelPlacement& Placement = NewPlacements.AddDefaulted_GetRef();
				Placement.Anchor = Bounds.bIsValid ? FVector2D(Bounds.GetCenter().X, Bounds.Max.Y) : FVector2D::ZeroVector;
				Placement.FirstNode = FirstNode;
				Placement.NodeCount = NewEntries.Num() - FirstNode;
				NewTreeEntries.Add(TreeEntry);
				NewTreeTags.Add(TreeTag);
			}
		}
	}

	bool bEntriesChanged = NewEntries.Num() != NodeEntries.Num();
	for (int32 Index = 0; !bEntriesChanged && Index < NewEntries.Num(); ++Index)
	{
		bEntriesChanged = NewEntries[Index] != NodeEntries[Index];
	}
	bool bTreeEntriesChanged = NewTreeEntries.Num() != TreeEntries.Num();
	for (int32 Index = 0; !bTreeEntriesChanged && Index < NewTreeEntries.Num(); ++Index)
	{
		bTreeEntriesChanged = NewTreeEntries[Index] != TreeEntries[Index];
	}
	const bool bPositionsChanged = NewPositions != EntryPositions;
	const bool bPlacementsChanged = NewPlacements != TreeEntryPlacements;
	NodeEntries = MoveTemp(NewEntries);
	EntryKeys = MoveTemp(NewKeys);
	EntryPositions = MoveTemp(NewPositions);
	TreeEntries = MoveTemp(NewTreeEntries);
	TreeEntryTags = MoveTemp(NewTreeTags);
	TreeEntryPlacements = MoveTemp(NewPlacements);
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
		if (bTreeEntriesChanged)
		{
			PushTreeEntries();
		}
		else if (bPlacementsChanged)
		{
			MyPanel->SetTreeLabelPlacements(TreeEntryPlacements);
		}
	}

	// Entries restyle themselves from their view model on every change.
	for (int32 Index = 0; Index < NodeEntries.Num(); ++Index)
	{
		if (UUserWidget* Entry = NodeEntries[Index])
		{
			AssignEntryItem(*Entry, FindNodeViewModel(EntryKeys[Index]));
		}
	}
	for (int32 Index = 0; Index < TreeEntries.Num(); ++Index)
	{
		if (UUserWidget* Entry = TreeEntries[Index])
		{
			AssignEntryItem(*Entry, FindTree(TreeEntryTags[Index]));
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

void URpgSkillTreeMapWidget::PushTreeEntries()
{
	if (!MyPanel)
	{
		return;
	}

	TArray<TSharedRef<SWidget>> Widgets;
	Widgets.Reserve(TreeEntries.Num());
	for (UUserWidget* Entry : TreeEntries)
	{
		Widgets.Add(Entry->TakeWidget());
	}
	MyPanel->SetTreeLabels(Widgets, TreeEntryPlacements);
}

void URpgSkillTreeMapWidget::PushLinksAndRegions()
{
	if (!MyPanel)
	{
		return;
	}

	TArray<SRpgSkillTreeMapPanel::FLink> Links;
	TArray<FBox2D> Regions;
	for (const URpgSkillTreeViewModel* Tree : SkillTrees)
	{
		if (!Tree)
		{
			continue;
		}
		for (const FRpgSkillTreeLinkView& LinkView : Tree->GetLinks())
		{
			SRpgSkillTreeMapPanel::FLink& Link = Links.AddDefaulted_GetRef();
			Link.From = LinkView.FromPosition;
			Link.To = LinkView.ToPosition;
			Link.Color = LinkView.bFromUnlocked && LinkView.bToUnlocked
				? LearnedLinkColor
				: LinkView.bFromUnlocked ? AvailableLinkColor : LockedLinkColor;
		}
		for (const FRpgSkillTreeRegionView& Region : Tree->GetRegions())
		{
			Regions.Add(Region.Bounds);
		}
	}
	MyPanel->SetLinks(MoveTemp(Links));
	MyPanel->SetRegions(MoveTemp(Regions));
	MyPanel->SetContentBounds(GetCombinedBounds());
}

void URpgSkillTreeMapWidget::PushSelection()
{
	if (!MyPanel)
	{
		return;
	}

	const FNodeKey SelectedKey = GetSelectedKey();
	const int32 SelectedIndex = SelectedKey.IsValid() ? FindEntryIndex(SelectedKey) : INDEX_NONE;
	MyPanel->SetSelectedIndex(SelectedIndex);
	if (!(SelectedKey == LastSelected))
	{
		LastSelected = SelectedKey;
		if (!bSelectingFromMap)
		{
			MyPanel->BringNodeIntoView(SelectedIndex);
		}
		SaveViewState();
	}
}

void URpgSkillTreeMapWidget::ClearSelectionExcept(const URpgSkillTreeViewModel* KeepTree)
{
	// Copy first: a cleared tree refreshes and syncs the map, which may change the list.
	const TArray<TObjectPtr<URpgSkillTreeViewModel>> Trees = SkillTrees;
	for (URpgSkillTreeViewModel* Tree : Trees)
	{
		if (Tree && Tree != KeepTree && Tree->GetSelectedNode())
		{
			Tree->SelectNode(FGameplayTag());
		}
	}
}

void URpgSkillTreeMapWidget::KeepSingleSelection()
{
	const URpgSkillTreeViewModel* NewestTree = nullptr;
	int32 SelectedTrees = 0;
	for (const URpgSkillTreeViewModel* Tree : SkillTrees)
	{
		const URpgSkillTreeNodeViewModel* Node = Tree ? Tree->GetSelectedNode() : nullptr;
		if (!Node)
		{
			continue;
		}
		++SelectedTrees;
		const FNodeKey Key{Tree->GetTreeTag(), Node->GetNodeTag()};
		if (!NewestTree || !(Key == LastSelected))
		{
			NewestTree = Tree;
		}
	}
	if (SelectedTrees > 1)
	{
		ClearSelectionExcept(NewestTree);
	}
}

void URpgSkillTreeMapWidget::InitializeView()
{
	const FName MapId = MakeViewMapId();
	if (!MyPanel || MapId.IsNone() || (bViewInitialized && MapId == ViewMapId))
	{
		return;
	}

	ViewMapId = MapId;
	bViewInitialized = true;
	LastSelected = GetSelectedKey();

	const URpgUIScreenSubsystem* Cache = bRememberView ? GetViewStateCache() : nullptr;
	if (const FRpgUIMapViewState* RememberedState = Cache ? Cache->FindMapViewState(MapId) : nullptr)
	{
		// Copy first: applying the camera saves the view again and overwrites the cached entry.
		const FRpgUIMapViewState State = *RememberedState;

		// A pooled screen may still hold its selection; otherwise the remembered one comes back.
		const int32 RememberedIndex = FindEntryIndex({State.SelectedGroupTag, State.SelectedTag});
		URpgSkillTreeViewModel* RememberedTree = EntryKeys.IsValidIndex(RememberedIndex) ? FindTree(EntryKeys[RememberedIndex].TreeTag) : nullptr;
		const bool bRestoreSelection = !LastSelected.IsValid() && State.SelectedTag.IsValid() && RememberedTree;
		if (bRestoreSelection)
		{
			LastSelected = EntryKeys[RememberedIndex];
		}

		FRpgSkillTreeMapCamera Camera;
		Camera.Center = State.Center;
		Camera.Zoom = State.Zoom;
		MyPanel->SetCamera(Camera);
		UpdateDetailLevel();
		if (bRestoreSelection)
		{
			RememberedTree->SelectNode(State.SelectedTag);
		}
		return;
	}

	// One tree opens on its default region; several open on the whole map, so every cluster is in sight.
	const URpgSkillTreeViewModel* OnlyTree = SkillTrees.Num() == 1 ? SkillTrees[0].Get() : nullptr;
	const FRpgSkillTreeRegionView* DefaultRegion = OnlyTree ? OnlyTree->FindRegion(OnlyTree->GetDefaultRegionId()) : nullptr;
	MyPanel->RequestFit(DefaultRegion && DefaultRegion->Bounds.bIsValid ? DefaultRegion->Bounds : GetCombinedBounds());
	UpdateDetailLevel();
}

void URpgSkillTreeMapWidget::SaveViewState() const
{
	// A pending fit still shows the default camera; the fit itself reports the view worth remembering.
	if (!bRememberView || !bViewInitialized || ViewMapId.IsNone() || !MyPanel || MyPanel->HasPendingFit())
	{
		return;
	}

	if (URpgUIScreenSubsystem* Cache = GetViewStateCache())
	{
		FRpgUIMapViewState State;
		State.Center = MyPanel->GetCamera().Center;
		State.Zoom = MyPanel->GetCamera().Zoom;
		State.SelectedTag = LastSelected.NodeTag;
		State.SelectedGroupTag = LastSelected.TreeTag;
		Cache->SetMapViewState(ViewMapId, State);
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
	URpgSkillTreeNodeViewModel* Node = EntryKeys.IsValidIndex(NodeIndex) ? FindNodeViewModel(EntryKeys[NodeIndex]) : nullptr;
	if (!Node)
	{
		return;
	}

	// Selecting refreshes the tree view models, which call SyncEntries; push again for an unchanged selection. A click
	// never moves the camera, and navigation already brought the node into view. Clearing the other trees first keeps
	// the map from showing two selections in between.
	TGuardValue<bool> SelectingGuard(bSelectingFromMap, true);
	ClearSelectionExcept(Node->GetTree());
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
	Settings.TreeLabelGap = TreeEntryGap;
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

FName URpgSkillTreeMapWidget::MakeViewMapId() const
{
	TStringBuilder<256> MapId;
	for (const URpgSkillTreeViewModel* Tree : SkillTrees)
	{
		if (Tree && Tree->GetTreeTag().IsValid())
		{
			if (MapId.Len() > 0)
			{
				MapId << TEXT('+');
			}
			MapId << Tree->GetTreeTag().GetTagName();
		}
	}
	return MapId.Len() > 0 ? FName(MapId.ToView()) : NAME_None;
}

FBox2D URpgSkillTreeMapWidget::GetCombinedBounds() const
{
	FBox2D Bounds(ForceInit);
	for (const URpgSkillTreeViewModel* Tree : SkillTrees)
	{
		if (Tree && Tree->GetMapBounds().bIsValid)
		{
			Bounds += Tree->GetMapBounds();
		}
	}
	return Bounds;
}

URpgSkillTreeMapWidget::FNodeKey URpgSkillTreeMapWidget::GetSelectedKey() const
{
	// KeepSingleSelection leaves at most one tree with a selection; the first one wins in between.
	for (const URpgSkillTreeViewModel* Tree : SkillTrees)
	{
		if (const URpgSkillTreeNodeViewModel* Node = Tree ? Tree->GetSelectedNode() : nullptr)
		{
			return {Tree->GetTreeTag(), Node->GetNodeTag()};
		}
	}
	return FNodeKey();
}

URpgSkillTreeViewModel* URpgSkillTreeMapWidget::FindTree(const FGameplayTag TreeTag) const
{
	if (!TreeTag.IsValid())
	{
		return nullptr;
	}
	for (URpgSkillTreeViewModel* Tree : SkillTrees)
	{
		if (Tree && Tree->GetTreeTag() == TreeTag)
		{
			return Tree;
		}
	}
	return nullptr;
}

URpgSkillTreeNodeViewModel* URpgSkillTreeMapWidget::FindNodeViewModel(const FNodeKey& Key) const
{
	const URpgSkillTreeViewModel* Tree = Key.IsValid() ? FindTree(Key.TreeTag) : nullptr;
	return Tree ? Tree->FindNode(Key.NodeTag) : nullptr;
}

int32 URpgSkillTreeMapWidget::FindEntryIndex(const FNodeKey& Key) const
{
	if (!Key.IsValid())
	{
		return INDEX_NONE;
	}
	return EntryKeys.IndexOfByPredicate([&Key](const FNodeKey& EntryKey)
	{
		return EntryKey.NodeTag == Key.NodeTag && (!Key.TreeTag.IsValid() || EntryKey.TreeTag == Key.TreeTag);
	});
}

#undef LOCTEXT_NAMESPACE
