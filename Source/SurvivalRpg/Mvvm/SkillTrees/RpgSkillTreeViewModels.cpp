#include "RpgSkillTreeViewModels.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "SurvivalRpg/Equipment/RpgEquipmentInstance.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemContainer.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Progression/Player/RpgPlayerProgressionComponent.h"
#include "SurvivalRpg/Progression/Rewards/RpgRewardClaimComponent.h"
#include "SurvivalRpg/Progression/Rewards/RpgRewardClaimDefinition.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeComponent.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeDefinition.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillProgressionComponent.h"
#include "SurvivalRpg/Runes/RpgInventoryFragment_Rune.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgSkillTreeViewModels)

namespace
{
	template <typename ViewModelType>
	bool AreViewModelArraysEqual(const TArray<TObjectPtr<ViewModelType>>& A, const TArray<TObjectPtr<ViewModelType>>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].Get() != B[Index].Get())
			{
				return false;
			}
		}
		return true;
	}

	template <typename ViewModelType>
	TArray<ViewModelType*> ToRawArray(const TArray<TObjectPtr<ViewModelType>>& Source)
	{
		TArray<ViewModelType*> Result;
		Result.Reserve(Source.Num());
		for (ViewModelType* ViewModel : Source)
		{
			Result.Add(ViewModel);
		}
		return Result;
	}

	ERpgSkillTreeNodeState ToNodeState(const ERpgSkillTreeUnlockResult Result)
	{
		switch (Result)
		{
		case ERpgSkillTreeUnlockResult::AlreadyUnlocked:
			return ERpgSkillTreeNodeState::Unlocked;
		case ERpgSkillTreeUnlockResult::Unlockable:
			return ERpgSkillTreeNodeState::Unlockable;
		case ERpgSkillTreeUnlockResult::NotEnoughPoints:
			return ERpgSkillTreeNodeState::Unaffordable;
		case ERpgSkillTreeUnlockResult::ExclusiveConflict:
			return ERpgSkillTreeNodeState::Excluded;
		default:
			return ERpgSkillTreeNodeState::Locked;
		}
	}

	float GetLevelProgress(const float XP, const float XPToNextLevel, const bool bIsMaxLevel)
	{
		if (bIsMaxLevel)
		{
			return 1.0f;
		}
		return XPToNextLevel > 0.0f ? FMath::Clamp(XP / XPToNextLevel, 0.0f, 1.0f) : 0.0f;
	}
}

bool FRpgSkillTreeRuneSlotView::operator==(const FRpgSkillTreeRuneSlotView& Other) const
{
	return bHasRune == Other.bHasRune && RuneTag == Other.RuneTag && Icon == Other.Icon &&
		bIsHighlighted == Other.bIsHighlighted && Label.IdenticalTo(Other.Label, ETextIdenticalModeFlags::LexicalCompareInvariants);
}

void URpgSkillTreeNodeViewModel::SetRuneHighlighted(const bool bHighlighted)
{
	UE_MVVM_SET_PROPERTY_VALUE(bIsRuneHighlighted, bHighlighted);
}

void URpgSkillTreeNodeViewModel::UpdateNode(
	URpgSkillTreeViewModel* InTree,
	const FRpgSkillTreeNode& Node,
	const FVector2D& InMapPosition,
	const ERpgSkillTreeUnlockResult InUnlockResult,
	const int32 InAssignedSlotIndex,
	const bool bInSelected,
	const bool bInRowUnlocked,
	const bool bInCanRefund,
	const FRpgSkillTreeNodeInteraction& Interaction,
	const bool bInRunesActive)
{
	Tree = InTree;

	TArray<FGameplayTag> AbilityIds;
	Node.GetGrantedAbilityIds(AbilityIds);
	const FGameplayTag NewAbilityId = AbilityIds.IsEmpty() ? FGameplayTag() : AbilityIds[0];
	const ERpgSkillTreeNodeState NewState = ToNodeState(InUnlockResult);

	UE_MVVM_SET_PROPERTY_VALUE(NodeTag, Node.NodeTag);
	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, Node.DisplayName);
	UE_MVVM_SET_PROPERTY_VALUE(Description, Node.Description);
	UE_MVVM_SET_PROPERTY_VALUE(Icon, Node.Icon);
	UE_MVVM_SET_PROPERTY_VALUE(KindTag, Node.KindTag);
	UE_MVVM_SET_PROPERTY_VALUE(Row, Node.Row);
	UE_MVVM_SET_PROPERTY_VALUE(Column, Node.Column);
	UE_MVVM_SET_PROPERTY_VALUE(MapPosition, InMapPosition);
	UE_MVVM_SET_PROPERTY_VALUE(RegionId, Node.RegionId);
	UE_MVVM_SET_PROPERTY_VALUE(Cost, Node.Cost);
	UE_MVVM_SET_PROPERTY_VALUE(RequiredPointsInTree, Node.RequiredPointsInTree);
	UE_MVVM_SET_PROPERTY_VALUE(State, NewState);
	UE_MVVM_SET_PROPERTY_VALUE(UnlockResult, InUnlockResult);
	UE_MVVM_SET_PROPERTY_VALUE(bIsUnlocked, NewState == ERpgSkillTreeNodeState::Unlocked);
	UE_MVVM_SET_PROPERTY_VALUE(bCanUnlock, NewState == ERpgSkillTreeNodeState::Unlockable);
	UE_MVVM_SET_PROPERTY_VALUE(bCanRefund, bInCanRefund);
	UE_MVVM_SET_PROPERTY_VALUE(bIsRowUnlocked, bInRowUnlocked);
	UE_MVVM_SET_PROPERTY_VALUE(AbilityIdTag, NewAbilityId);
	UE_MVVM_SET_PROPERTY_VALUE(bGrantsSlotAbility, NewAbilityId.IsValid());
	UE_MVVM_SET_PROPERTY_VALUE(AssignedSlotIndex, InAssignedSlotIndex);
	UE_MVVM_SET_PROPERTY_VALUE(bIsSelected, bInSelected);
	UE_MVVM_SET_PROPERTY_VALUE(bCanSwap, Interaction.bCanSwap);
	UE_MVVM_SET_PROPERTY_VALUE(SwapReplacesNodeTag, Interaction.SwapReplacesNodeTag);
	UE_MVVM_SET_PROPERTY_VALUE(SwapReplacesName, Interaction.SwapReplacesName);
	UE_MVVM_SET_PROPERTY_VALUE(RequestStatus, Interaction.RequestStatus);
	UE_MVVM_SET_PROPERTY_VALUE(bIsRequestPending, Interaction.RequestStatus == ERpgSkillTreeRequestStatus::Pending);
	UE_MVVM_SET_PROPERTY_VALUE(Rejection, Interaction.Rejection);

	const bool bNewNeedsRunes = !Node.RequiredRunes.IsEmpty();
	FText NewRuneRequirementText;
	if (bNewNeedsRunes)
	{
		// Rune tags end in the rune's name, for example Rune.Brand.
		TArray<FString> RuneNames;
		for (const FGameplayTag& Rune : Node.RequiredRunes)
		{
			FString Name = Rune.ToString();
			int32 DotIndex = INDEX_NONE;
			if (Name.FindLastChar(TEXT('.'), DotIndex))
			{
				Name.RightChopInline(DotIndex + 1);
			}
			RuneNames.Add(Name);
		}
		const FText Separator = Node.RuneMatch == ERpgSkillTreeRuneMatch::AllRunes
			? NSLOCTEXT("RpgSkillTreeViewModels", "RunesAnd", " and ")
			: NSLOCTEXT("RpgSkillTreeViewModels", "RunesOr", " or ");
		NewRuneRequirementText = FText::Format(
			NSLOCTEXT("RpgSkillTreeViewModels", "RuneRequirement", "Needs {0} in the tool's sockets"),
			FText::FromString(FString::Join(RuneNames, *Separator.ToString())));
	}
	UE_MVVM_SET_PROPERTY_VALUE(RequiredRunes, Node.RequiredRunes);
	UE_MVVM_SET_PROPERTY_VALUE(bNeedsRunes, bNewNeedsRunes);
	UE_MVVM_SET_PROPERTY_VALUE(bIsDormant, bNewNeedsRunes && NewState == ERpgSkillTreeNodeState::Unlocked && !bInRunesActive);
	UE_MVVM_SET_PROPERTY_VALUE(RuneRequirementText, NewRuneRequirementText);
}

void URpgSkillTreeNodeViewModel::RequestUnlock()
{
	if (URpgSkillTreeViewModel* OwningTree = Tree.Get())
	{
		OwningTree->RequestUnlockNode(NodeTag);
	}
}

void URpgSkillTreeNodeViewModel::RequestRefund()
{
	if (URpgSkillTreeViewModel* OwningTree = Tree.Get())
	{
		OwningTree->RequestRefundNode(NodeTag);
	}
}

void URpgSkillTreeNodeViewModel::RequestSwap()
{
	if (URpgSkillTreeViewModel* OwningTree = Tree.Get())
	{
		OwningTree->RequestSwapNode(NodeTag);
	}
}

void URpgSkillTreeNodeViewModel::Select()
{
	if (URpgSkillTreeViewModel* OwningTree = Tree.Get())
	{
		OwningTree->SelectNode(NodeTag);
	}
}

void URpgSkillTreeViewModel::BeginDestroy()
{
	RefreshQueue.Cancel();
	Unsubscribe();
	Super::BeginDestroy();
}

void URpgSkillTreeViewModel::BindSkillTree(URpgSkillTreeComponent* InSkillTrees, const FGameplayTag InTreeTag)
{
	if (ObservedSkillTrees.Get() != InSkillTrees || TreeTag != InTreeTag)
	{
		Unsubscribe();
		ObservedSkillTrees = InSkillTrees;
		UE_MVVM_SET_PROPERTY_VALUE(TreeTag, InTreeTag);
		SelectedNodeTag = FGameplayTag();
		PendingNodeTags.Reset();
		RejectedNodes.Reset();
		Subscribe();
	}
	Refresh();
}

void URpgSkillTreeViewModel::UnbindSkillTree()
{
	Unsubscribe();
	ObservedSkillTrees.Reset();
	UE_MVVM_SET_PROPERTY_VALUE(TreeTag, FGameplayTag());
	SelectedNodeTag = FGameplayTag();
	PendingNodeTags.Reset();
	RejectedNodes.Reset();
	Refresh();
}

void URpgSkillTreeViewModel::Refresh()
{
	RefreshQueue.Cancel();
	Rebuild();
}

void URpgSkillTreeViewModel::RequestUnlockNode(const FGameplayTag NodeTag)
{
	URpgSkillTreeComponent* SkillTrees = ObservedSkillTrees.Get();
	if (!SkillTrees || !TreeTag.IsValid() || !NodeTag.IsValid())
	{
		return;
	}

	// Selecting first lets the player place a freshly learned active on Q/E/R right away.
	SelectNode(NodeTag);
	BeginRequest(NodeTag);
	SkillTrees->RequestUnlockNode(TreeTag, NodeTag);
	Refresh();
}

void URpgSkillTreeViewModel::RequestRefundNode(const FGameplayTag NodeTag)
{
	URpgSkillTreeComponent* SkillTrees = ObservedSkillTrees.Get();
	if (SkillTrees && TreeTag.IsValid() && NodeTag.IsValid())
	{
		BeginRequest(NodeTag);
		SkillTrees->RequestRefundNode(TreeTag, NodeTag);
		Refresh();
	}
}

void URpgSkillTreeViewModel::RequestSwapNode(const FGameplayTag NodeTag)
{
	URpgSkillTreeComponent* SkillTrees = ObservedSkillTrees.Get();
	if (!SkillTrees || !TreeTag.IsValid() || !NodeTag.IsValid())
	{
		return;
	}

	SelectNode(NodeTag);
	BeginRequest(NodeTag);
	SkillTrees->RequestSwapNode(TreeTag, NodeTag);
	Refresh();
}

void URpgSkillTreeViewModel::SetHighlightedRune(const FGameplayTag RuneTag)
{
	if (HighlightedRune == RuneTag)
	{
		return;
	}

	HighlightedRune = RuneTag;
	if (const URpgSkillTreeComponent* SkillTrees = ObservedSkillTrees.Get())
	{
		if (const URpgSkillTreeDefinition* Tree = SkillTrees->FindSkillTree(TreeTag))
		{
			for (const FRpgSkillTreeNode& Node : Tree->Nodes)
			{
				if (URpgSkillTreeNodeViewModel* NodeViewModel = FindNode(Node.NodeTag))
				{
					NodeViewModel->SetRuneHighlighted(RuneTag.IsValid() && Node.RequiredRunes.HasTagExact(RuneTag));
				}
			}
		}
	}
	// The map restyles its node entries on every tree change.
	OnTreeChanged.Broadcast();
}

void URpgSkillTreeViewModel::BeginRequest(const FGameplayTag NodeTag)
{
	// On the server the request runs right away, and its change clears the wait again before the next rebuild.
	RejectedNodes.Remove(NodeTag);
	PendingNodeTags.Add(NodeTag);
}

void URpgSkillTreeViewModel::RequestResetTree()
{
	if (URpgSkillTreeComponent* SkillTrees = ObservedSkillTrees.Get(); SkillTrees && TreeTag.IsValid())
	{
		SkillTrees->RequestResetTree(TreeTag);
	}
}

void URpgSkillTreeViewModel::SelectNode(const FGameplayTag NodeTag)
{
	if (SelectedNodeTag == NodeTag)
	{
		return;
	}
	SelectedNodeTag = NodeTag;
	// A rejection belongs to the moment it happened; a new selection starts clean.
	RejectedNodes.Reset();
	Refresh();
}

void URpgSkillTreeViewModel::AssignSelectedNodeToSlot(const int32 SlotIndex)
{
	URpgSkillTreeComponent* SkillTrees = ObservedSkillTrees.Get();
	if (!SkillTrees || !bCanAssignSelectedNode || !SelectedNode ||
		SlotIndex < 0 || SlotIndex >= URpgSkillTreeComponent::SlotCount)
	{
		return;
	}
	SkillTrees->RequestAssignSlot(TreeTag, SlotIndex, SelectedNode->GetAbilityIdTag());
}

void URpgSkillTreeViewModel::ClearSlot(const int32 SlotIndex)
{
	URpgSkillTreeComponent* SkillTrees = ObservedSkillTrees.Get();
	if (SkillTrees && TreeTag.IsValid() && SlotIndex >= 0 && SlotIndex < URpgSkillTreeComponent::SlotCount)
	{
		SkillTrees->RequestAssignSlot(TreeTag, SlotIndex, FGameplayTag());
	}
}

TArray<URpgSkillTreeNodeViewModel*> URpgSkillTreeViewModel::GetNodes() const
{
	return ToRawArray(Nodes);
}

URpgSkillTreeNodeViewModel* URpgSkillTreeViewModel::FindNode(const FGameplayTag NodeTag) const
{
	for (URpgSkillTreeNodeViewModel* Node : Nodes)
	{
		if (Node && Node->GetNodeTag() == NodeTag)
		{
			return Node;
		}
	}
	return nullptr;
}

const FRpgSkillTreeRegionView* URpgSkillTreeViewModel::FindRegion(const FName RegionId) const
{
	return RegionId.IsNone() ? nullptr : Regions.FindByPredicate([RegionId](const FRpgSkillTreeRegionView& Region)
	{
		return Region.RegionId == RegionId;
	});
}

void URpgSkillTreeViewModel::HandleSkillTreeChanged(const FGameplayTag ChangedTreeTag)
{
	if (ChangedTreeTag == TreeTag)
	{
		// The server's answer to an accepted request arrives as replicated state; the requests are no longer open.
		PendingNodeTags.Reset();
		QueueRefresh();
	}
}

void URpgSkillTreeViewModel::HandleRequestRejected(
	const FGameplayTag RejectedTreeTag,
	const FGameplayTag NodeTag,
	const ERpgSkillTreeRequestRejection Reason)
{
	if (RejectedTreeTag != TreeTag || !NodeTag.IsValid())
	{
		return;
	}
	PendingNodeTags.Remove(NodeTag);
	RejectedNodes.Add(NodeTag, Reason);
	QueueRefresh();
}

void URpgSkillTreeViewModel::HandleTradeSkillChanged(const FGameplayTag SkillTag, const FTradeSkillState& NewState)
{
	if (SkillTag == MasterySkillTag)
	{
		QueueRefresh();
	}
}

void URpgSkillTreeViewModel::Subscribe()
{
	URpgSkillTreeComponent* SkillTrees = ObservedSkillTrees.Get();
	if (!SkillTrees)
	{
		return;
	}

	SkillTrees->OnSkillTreeChanged.AddUniqueDynamic(this, &ThisClass::HandleSkillTreeChanged);
	SkillTrees->OnRequestRejected.AddUniqueDynamic(this, &ThisClass::HandleRequestRejected);
	const AActor* Owner = SkillTrees->GetOwner();
	URpgTradeSkillProgressionComponent* TradeSkills =
		Owner ? Owner->FindComponentByClass<URpgTradeSkillProgressionComponent>() : nullptr;
	ObservedTradeSkills = TradeSkills;
	if (TradeSkills)
	{
		TradeSkills->OnTradeSkillTagChanged.AddUniqueDynamic(this, &ThisClass::HandleTradeSkillChanged);
	}
}

void URpgSkillTreeViewModel::Unsubscribe()
{
	RefreshQueue.Cancel();
	if (URpgSkillTreeComponent* SkillTrees = ObservedSkillTrees.Get())
	{
		SkillTrees->OnSkillTreeChanged.RemoveDynamic(this, &ThisClass::HandleSkillTreeChanged);
		SkillTrees->OnRequestRejected.RemoveDynamic(this, &ThisClass::HandleRequestRejected);
	}
	if (URpgTradeSkillProgressionComponent* TradeSkills = ObservedTradeSkills.Get())
	{
		TradeSkills->OnTradeSkillTagChanged.RemoveDynamic(this, &ThisClass::HandleTradeSkillChanged);
	}
	ObservedTradeSkills.Reset();
}

void URpgSkillTreeViewModel::QueueRefresh()
{
	if (const URpgSkillTreeComponent* SkillTrees = ObservedSkillTrees.Get())
	{
		RefreshQueue.Queue(SkillTrees->GetWorld(), this, &ThisClass::ExecuteQueuedRefresh);
	}
}

void URpgSkillTreeViewModel::ExecuteQueuedRefresh()
{
	if (RefreshQueue.Consume())
	{
		Rebuild();
	}
}

void URpgSkillTreeViewModel::Rebuild()
{
	const URpgSkillTreeComponent* SkillTrees = ObservedSkillTrees.Get();
	const URpgSkillTreeDefinition* Tree = SkillTrees ? SkillTrees->FindSkillTree(TreeTag) : nullptr;
	const URpgTradeSkillProgressionComponent* TradeSkills = ObservedTradeSkills.Get();

	const FGameplayTag NewMasterySkillTag = Tree ? Tree->MasterySkillTag : FGameplayTag();
	const int32 NewSpentPoints = Tree ? SkillTrees->GetSpentPoints(TreeTag) : 0;
	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, Tree ? Tree->DisplayName : FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(Description, Tree ? Tree->Description : FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(Icon, Tree ? Tree->Icon : TSoftObjectPtr<UTexture2D>());
	UE_MVVM_SET_PROPERTY_VALUE(BranchNames, Tree ? Tree->BranchNames : TArray<FText>());
	UE_MVVM_SET_PROPERTY_VALUE(MasterySkillTag, NewMasterySkillTag);
	UE_MVVM_SET_PROPERTY_VALUE(
		MasterySkillName,
		TradeSkills && NewMasterySkillTag.IsValid() ? TradeSkills->GetSkillDisplayName(NewMasterySkillTag) : FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(
		MasteryLevel,
		TradeSkills && NewMasterySkillTag.IsValid() ? TradeSkills->GetSkillLevelByTag(NewMasterySkillTag) : 1);
	UE_MVVM_SET_PROPERTY_VALUE(EarnedPoints, Tree ? SkillTrees->GetEarnedPoints(TreeTag) : 0);
	UE_MVVM_SET_PROPERTY_VALUE(SpentPoints, NewSpentPoints);
	UE_MVVM_SET_PROPERTY_VALUE(AvailablePoints, Tree ? SkillTrees->GetAvailablePoints(TreeTag) : 0);
	UE_MVVM_SET_PROPERTY_VALUE(MaxPoints, Tree ? Tree->MaxPoints : 0);
	UE_MVVM_SET_PROPERTY_VALUE(bCanResetTree, NewSpentPoints > 0);

	// Q/E/R: each slot names the node whose learned ability it holds.
	TArray<FRpgSkillTreeSlotView> NewSlots;
	TArray<FGameplayTag, TInlineAllocator<URpgSkillTreeComponent::SlotCount>> SlotAbilityIds;
	if (Tree)
	{
		for (int32 SlotIndex = 0; SlotIndex < URpgSkillTreeComponent::SlotCount; ++SlotIndex)
		{
			FRpgSkillTreeSlotView& Slot = NewSlots.AddDefaulted_GetRef();
			Slot.SlotIndex = SlotIndex;
			Slot.AbilityIdTag = SkillTrees->GetSlotAbilityId(TreeTag, SlotIndex);
			SlotAbilityIds.Add(Slot.AbilityIdTag);
		}
	}

	// Node view models stay stable per node tag, so list entries keep their objects across rebuilds.
	TMap<FGameplayTag, URpgSkillTreeNodeViewModel*> PreviousNodes;
	for (URpgSkillTreeNodeViewModel* Node : Nodes)
	{
		if (Node)
		{
			PreviousNodes.Add(Node->GetNodeTag(), Node);
		}
	}

	// Rune-bound nodes work only with their runes in the sockets of this tree's tool in hand.
	const APlayerState* OwnerPlayerState = SkillTrees ? Cast<APlayerState>(SkillTrees->GetOwner()) : nullptr;
	const FGameplayTagContainer SocketedRunes = URpgInventoryFragment_Rune::GetSocketedRuneTags(Cast<URpgInventoryItemInstance>(
		URpgSkillTreeComponent::FindActiveItemOfTree(OwnerPlayerState ? OwnerPlayerState->GetPawn() : nullptr, TreeTag)));

	TArray<TObjectPtr<URpgSkillTreeNodeViewModel>> NewNodes;
	int32 NewRowCount = 0;
	int32 NewColumnCount = 0;
	TMap<int32, int32> RowGates;
	if (Tree)
	{
		NewNodes.Reserve(Tree->Nodes.Num());
		for (const FRpgSkillTreeNode& Node : Tree->Nodes)
		{
			if (!Node.NodeTag.IsValid())
			{
				continue;
			}

			URpgSkillTreeNodeViewModel* NodeViewModel = PreviousNodes.FindRef(Node.NodeTag);
			if (!NodeViewModel)
			{
				NodeViewModel = NewObject<URpgSkillTreeNodeViewModel>(this);
			}

			TArray<FGameplayTag> AbilityIds;
			Node.GetGrantedAbilityIds(AbilityIds);
			const int32 AssignedSlot = AbilityIds.IsEmpty() ? INDEX_NONE : SlotAbilityIds.IndexOfByKey(AbilityIds[0]);
			const ERpgSkillTreeUnlockResult UnlockResult = SkillTrees->EvaluateUnlock(TreeTag, Node.NodeTag);

			// An excluded choice can still be reached by swapping out the learned alternative.
			FRpgSkillTreeNodeInteraction Interaction;
			if (UnlockResult == ERpgSkillTreeUnlockResult::ExclusiveConflict)
			{
				FGameplayTag ReplacedTag;
				Interaction.bCanSwap =
					SkillTrees->EvaluateSwap(TreeTag, Node.NodeTag, ReplacedTag) == ERpgSkillTreeUnlockResult::Unlockable;
				Interaction.SwapReplacesNodeTag = ReplacedTag;
				if (const FRpgSkillTreeNode* ReplacedNode = Tree->FindNode(ReplacedTag))
				{
					Interaction.SwapReplacesName = ReplacedNode->DisplayName;
				}
			}
			if (PendingNodeTags.Contains(Node.NodeTag))
			{
				Interaction.RequestStatus = ERpgSkillTreeRequestStatus::Pending;
			}
			else if (const ERpgSkillTreeRequestRejection* Rejection = RejectedNodes.Find(Node.NodeTag))
			{
				Interaction.RequestStatus = ERpgSkillTreeRequestStatus::Rejected;
				Interaction.Rejection = *Rejection;
			}

			NodeViewModel->UpdateNode(
				this,
				Node,
				Tree->GetNodeMapPosition(Node),
				UnlockResult,
				AssignedSlot,
				SelectedNodeTag.IsValid() && Node.NodeTag == SelectedNodeTag,
				NewSpentPoints >= Node.RequiredPointsInTree,
				SkillTrees->EvaluateRefund(TreeTag, Node.NodeTag) == ERpgSkillTreeRefundResult::Refundable,
				Interaction,
				Node.IsActiveWithRunes(SocketedRunes));
			NodeViewModel->SetRuneHighlighted(HighlightedRune.IsValid() && Node.RequiredRunes.HasTagExact(HighlightedRune));
			NewNodes.Add(NodeViewModel);

			NewRowCount = FMath::Max(NewRowCount, Node.Row + 1);
			NewColumnCount = FMath::Max(NewColumnCount, Node.Column + 1);
			int32& RowGate = RowGates.FindOrAdd(Node.Row, Node.RequiredPointsInTree);
			RowGate = FMath::Min(RowGate, Node.RequiredPointsInTree);

			if (AssignedSlot != INDEX_NONE)
			{
				FRpgSkillTreeSlotView& Slot = NewSlots[AssignedSlot];
				Slot.NodeTag = Node.NodeTag;
				Slot.DisplayName = Node.DisplayName;
				Slot.Icon = Node.Icon;
			}
		}
	}

	TArray<FRpgSkillTreeRowView> NewRows;
	for (int32 RowIndex = 0; RowIndex < NewRowCount; ++RowIndex)
	{
		FRpgSkillTreeRowView& RowView = NewRows.AddDefaulted_GetRef();
		RowView.Row = RowIndex;
		RowView.RequiredPoints = RowGates.FindRef(RowIndex);
		RowView.bIsUnlocked = NewSpentPoints >= RowView.RequiredPoints;
	}

	TArray<FRpgSkillTreeLinkView> NewLinks;
	if (Tree)
	{
		for (const FRpgSkillTreeNode& Node : Tree->Nodes)
		{
			for (const FGameplayTag& PrerequisiteTag : Node.Prerequisites)
			{
				const FRpgSkillTreeNode* Prerequisite = Tree->FindNode(PrerequisiteTag);
				if (!Prerequisite)
				{
					continue;
				}
				FRpgSkillTreeLinkView& Link = NewLinks.AddDefaulted_GetRef();
				Link.FromNodeTag = PrerequisiteTag;
				Link.ToNodeTag = Node.NodeTag;
				Link.FromPosition = Tree->GetNodeMapPosition(*Prerequisite);
				Link.ToPosition = Tree->GetNodeMapPosition(Node);
				Link.FromCell = FIntPoint(Prerequisite->Column, Prerequisite->Row);
				Link.ToCell = FIntPoint(Node.Column, Node.Row);
				Link.bFromUnlocked = SkillTrees->IsNodeUnlocked(TreeTag, PrerequisiteTag);
				Link.bToUnlocked = SkillTrees->IsNodeUnlocked(TreeTag, Node.NodeTag);
			}
		}
	}

	const bool bNodesChanged = !AreViewModelArraysEqual(Nodes, NewNodes);
	Nodes = MoveTemp(NewNodes);
	if (bNodesChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Nodes);
	}
	UE_MVVM_SET_PROPERTY_VALUE(RowCount, NewRowCount);
	UE_MVVM_SET_PROPERTY_VALUE(ColumnCount, NewColumnCount);
	UE_MVVM_SET_PROPERTY_VALUE(Rows, NewRows);
	UE_MVVM_SET_PROPERTY_VALUE(Links, NewLinks);
	UE_MVVM_SET_PROPERTY_VALUE(Slots, NewSlots);

	TArray<FRpgSkillTreeRegionView> NewRegions;
	if (Tree)
	{
		for (const FRpgSkillTreeMapRegion& Region : Tree->Regions)
		{
			if (!Region.RegionId.IsNone())
			{
				FRpgSkillTreeRegionView& RegionView = NewRegions.AddDefaulted_GetRef();
				RegionView.RegionId = Region.RegionId;
				RegionView.DisplayName = Region.DisplayName;
				RegionView.Bounds = Tree->GetRegionBounds(Region.RegionId);
			}
		}
	}
	const FBox2D NewMapBounds = Tree ? Tree->GetMapBounds() : FBox2D(ForceInit);
	UE_MVVM_SET_PROPERTY_VALUE(LayoutMode, Tree ? Tree->LayoutMode : ERpgSkillTreeLayoutMode::Grid);
	if (MapBounds != NewMapBounds || MapBounds.bIsValid != NewMapBounds.bIsValid)
	{
		MapBounds = NewMapBounds;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MapBounds);
	}
	UE_MVVM_SET_PROPERTY_VALUE(Regions, NewRegions);
	UE_MVVM_SET_PROPERTY_VALUE(DefaultRegionId, Tree ? Tree->DefaultRegionId : NAME_None);

	// A selection that left the tree, for example after a content change, is dropped.
	URpgSkillTreeNodeViewModel* NewSelectedNode = SelectedNodeTag.IsValid() ? FindNode(SelectedNodeTag) : nullptr;
	if (!NewSelectedNode)
	{
		SelectedNodeTag = FGameplayTag();
	}
	UE_MVVM_SET_PROPERTY_VALUE(SelectedNode, NewSelectedNode);
	UE_MVVM_SET_PROPERTY_VALUE(
		bCanAssignSelectedNode,
		NewSelectedNode && NewSelectedNode->GetState() == ERpgSkillTreeNodeState::Unlocked &&
			NewSelectedNode->GetAbilityIdTag().IsValid());

	OnTreeChanged.Broadcast();
}

void URpgTradeSkillViewModel::UpdateSkill(
	const FTradeSkillState& SkillState,
	const FText& InDisplayName,
	const TSoftObjectPtr<UTexture2D>& InIcon,
	const float InXPToNextLevel,
	const int32 InMaxLevel)
{
	const bool bNewIsMaxLevel = SkillState.Level >= InMaxLevel;
	UE_MVVM_SET_PROPERTY_VALUE(SkillTag, SkillState.SkillTag);
	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, InDisplayName);
	UE_MVVM_SET_PROPERTY_VALUE(Icon, InIcon);
	UE_MVVM_SET_PROPERTY_VALUE(Level, SkillState.Level);
	UE_MVVM_SET_PROPERTY_VALUE(MaxLevel, InMaxLevel);
	UE_MVVM_SET_PROPERTY_VALUE(XP, SkillState.XP);
	UE_MVVM_SET_PROPERTY_VALUE(XPToNextLevel, bNewIsMaxLevel ? 0.0f : InXPToNextLevel);
	UE_MVVM_SET_PROPERTY_VALUE(LevelProgress, GetLevelProgress(SkillState.XP, InXPToNextLevel, bNewIsMaxLevel));
	UE_MVVM_SET_PROPERTY_VALUE(bIsMaxLevel, bNewIsMaxLevel);
}

void URpgSkillProgressionViewModel::BeginDestroy()
{
	RefreshQueue.Cancel();
	Unsubscribe();
	ObserveTrees(TConstArrayView<TObjectPtr<URpgSkillTreeViewModel>>());
	Super::BeginDestroy();
}

void URpgSkillProgressionViewModel::BindPlayerController(APlayerController* InPlayerController)
{
	BindPlayerState(InPlayerController ? InPlayerController->PlayerState.Get() : nullptr);
}

void URpgSkillProgressionViewModel::BindPlayerState(APlayerState* InPlayerState)
{
	if (ObservedPlayerState.Get() != InPlayerState)
	{
		Unsubscribe();
		ObservedPlayerState = InPlayerState;
		ObservedSkillTrees = InPlayerState ? InPlayerState->FindComponentByClass<URpgSkillTreeComponent>() : nullptr;
		ObservedTradeSkills = InPlayerState ? InPlayerState->FindComponentByClass<URpgTradeSkillProgressionComponent>() : nullptr;
		ObservedCharacter = InPlayerState ? InPlayerState->FindComponentByClass<URpgPlayerProgressionComponent>() : nullptr;
		ObservedRewardClaims = InPlayerState ? InPlayerState->FindComponentByClass<URpgRewardClaimComponent>() : nullptr;
		Subscribe();
	}

	// Game Features registered after the player state began play contribute their trees here.
	if (URpgSkillTreeComponent* SkillTreeComponent = ObservedSkillTrees.Get())
	{
		SkillTreeComponent->RegisterSkillTreesFromAssetManager();
	}
	Refresh();
}

void URpgSkillProgressionViewModel::Unbind()
{
	BindPlayerState(nullptr);
}

void URpgSkillProgressionViewModel::Refresh()
{
	RefreshQueue.Cancel();
	bRefreshTreesOnRebuild = true;
	Rebuild();
}

void URpgSkillProgressionViewModel::SelectSkillTree(const FGameplayTag TreeTag)
{
	RequestedTreeTag = TreeTag;
	Refresh();
}

TArray<URpgTradeSkillViewModel*> URpgSkillProgressionViewModel::GetSkills() const
{
	return ToRawArray(Skills);
}

TArray<URpgSkillTreeViewModel*> URpgSkillProgressionViewModel::GetSkillTrees() const
{
	return ToRawArray(SkillTrees);
}

void URpgSkillProgressionViewModel::HandleSkillTreeChanged(FGameplayTag TreeTag)
{
	QueueRefresh();
}

void URpgSkillProgressionViewModel::HandleTradeSkillChanged(FGameplayTag SkillTag, const FTradeSkillState& NewState)
{
	QueueRefresh();
}

void URpgSkillProgressionViewModel::HandleCharacterLevelChanged(int32 NewLevel)
{
	QueueRefresh();
}

void URpgSkillProgressionViewModel::HandleCharacterXPChanged(float CurrentXP, float XPToNextLevel)
{
	QueueRefresh();
}

int32 URpgSkillProgressionViewModel::GetRewardChoiceCount() const
{
	int32 Count = 0;
	for (const FRpgRewardClaimView& Claim : RewardClaims)
	{
		Count += Claim.OptionLabels.Num();
	}
	return Count;
}

bool URpgSkillProgressionViewModel::ResolveRewardChoice(
	const int32 ChoiceIndex,
	int32& OutClaimIndex,
	int32& OutOptionIndex) const
{
	int32 Remaining = ChoiceIndex;
	for (int32 ClaimIndex = 0; ClaimIndex < RewardClaims.Num() && Remaining >= 0; ++ClaimIndex)
	{
		const int32 OptionCount = RewardClaims[ClaimIndex].OptionLabels.Num();
		if (Remaining < OptionCount)
		{
			OutClaimIndex = ClaimIndex;
			OutOptionIndex = Remaining;
			return true;
		}
		Remaining -= OptionCount;
	}
	return false;
}

FText URpgSkillProgressionViewModel::GetRewardChoiceLabel(const int32 ChoiceIndex) const
{
	int32 ClaimIndex = INDEX_NONE;
	int32 OptionIndex = INDEX_NONE;
	if (!ResolveRewardChoice(ChoiceIndex, ClaimIndex, OptionIndex))
	{
		return FText::GetEmpty();
	}
	const FRpgRewardClaimView& Claim = RewardClaims[ClaimIndex];
	return Claim.SourceText.IsEmpty()
		? Claim.OptionLabels[OptionIndex]
		: FText::Format(
			NSLOCTEXT("RpgSkillTreeViewModels", "RewardChoiceWithSource", "{0} ({1})"),
			Claim.OptionLabels[OptionIndex],
			Claim.SourceText);
}

void URpgSkillProgressionViewModel::ClaimRewardChoice(const int32 ChoiceIndex)
{
	int32 ClaimIndex = INDEX_NONE;
	int32 OptionIndex = INDEX_NONE;
	URpgRewardClaimComponent* Claims = ObservedRewardClaims.Get();
	if (Claims && ResolveRewardChoice(ChoiceIndex, ClaimIndex, OptionIndex))
	{
		Claims->RequestClaimReward(RewardClaims[ClaimIndex].ClaimId, OptionIndex);
	}
}

void URpgSkillProgressionViewModel::HandleRewardClaimsChanged()
{
	QueueRefresh();
}

void URpgSkillProgressionViewModel::HandleRewardClaimResult(const FName ClaimId, const ERpgRewardClaimResult Result)
{
	FText Feedback;
	switch (Result)
	{
	case ERpgRewardClaimResult::Claimed:
		break;
	case ERpgRewardClaimResult::NoRoom:
		Feedback = NSLOCTEXT("RpgSkillTreeViewModels", "RewardNoRoom", "No room in your inventory. The reward stays open.");
		break;
	case ERpgRewardClaimResult::UnknownClaim:
		Feedback = NSLOCTEXT("RpgSkillTreeViewModels", "RewardUnknown", "This reward was already claimed.");
		break;
	case ERpgRewardClaimResult::InvalidOption:
		Feedback = NSLOCTEXT("RpgSkillTreeViewModels", "RewardInvalidOption", "This choice is not available.");
		break;
	default:
		Feedback = NSLOCTEXT("RpgSkillTreeViewModels", "RewardRejected", "The reward could not be claimed.");
		break;
	}
	UE_MVVM_SET_PROPERTY_VALUE(RewardFeedbackText, Feedback);
	OnProgressionChanged.Broadcast();
}

void URpgSkillProgressionViewModel::RebuildRewardClaims()
{
	TArray<FRpgRewardClaimView> NewClaims;
	if (const URpgRewardClaimComponent* Claims = ObservedRewardClaims.Get())
	{
		const URpgTradeSkillProgressionComponent* TradeSkills = ObservedTradeSkills.Get();
		for (const FRpgRewardClaim& Claim : Claims->GetPendingClaims())
		{
			const URpgRewardClaimDefinition* Reward = Claim.Reward;
			if (!Reward)
			{
				continue;
			}

			FRpgRewardClaimView& View = NewClaims.AddDefaulted_GetRef();
			View.ClaimId = Claim.ClaimId;
			View.Title = Reward->DisplayName;
			if (Claim.SourceTag.IsValid())
			{
				const FText SourceName = TradeSkills
					? TradeSkills->GetSkillDisplayName(Claim.SourceTag)
					: FText::FromName(Claim.SourceTag.GetTagLeafName());
				View.SourceText = Claim.SourceLevel > 0
					? FText::Format(NSLOCTEXT("RpgSkillTreeViewModels", "RewardSourceLevel", "{0} level {1}"), SourceName, Claim.SourceLevel)
					: SourceName;
			}
			if (Reward->IsChoice())
			{
				for (const FRpgRewardClaimOption& Option : Reward->Options)
				{
					const URpgInventoryItemDefinition* Item = Option.ItemDefinition
						? GetDefault<URpgInventoryItemDefinition>(Option.ItemDefinition)
						: nullptr;
					const FText ItemName = Item ? Item->DisplayName : FText::GetEmpty();
					View.OptionLabels.Add(Option.Count > 1
						? FText::Format(NSLOCTEXT("RpgSkillTreeViewModels", "RewardOptionCount", "{0} {1}"), Option.Count, ItemName)
						: ItemName);
				}
			}
			else
			{
				// A loot pool has one button, named after the reward.
				View.OptionLabels.Add(Reward->DisplayName);
			}
		}
	}
	UE_MVVM_SET_PROPERTY_VALUE(RewardClaims, NewClaims);
}

void URpgSkillProgressionViewModel::ToggleRuneHighlight(const int32 SlotIndex)
{
	const FGameplayTag RuneTag = RuneSlots.IsValidIndex(SlotIndex) ? RuneSlots[SlotIndex].RuneTag : FGameplayTag();
	UE_MVVM_SET_PROPERTY_VALUE(HighlightedRuneTag, RuneTag.IsValid() && RuneTag != HighlightedRuneTag ? RuneTag : FGameplayTag());
	ApplyRuneHighlight();
}

FText URpgSkillProgressionViewModel::GetRuneSlotLabel(const int32 SlotIndex) const
{
	return RuneSlots.IsValidIndex(SlotIndex) ? RuneSlots[SlotIndex].Label : FText::GetEmpty();
}

bool URpgSkillProgressionViewModel::IsRuneSlotFilled(const int32 SlotIndex) const
{
	return RuneSlots.IsValidIndex(SlotIndex) && RuneSlots[SlotIndex].bHasRune;
}

bool URpgSkillProgressionViewModel::IsRuneSlotHighlighted(const int32 SlotIndex) const
{
	return RuneSlots.IsValidIndex(SlotIndex) && RuneSlots[SlotIndex].bIsHighlighted;
}

void URpgSkillProgressionViewModel::RebuildRuneContext()
{
	// The socketed item in the main hand is the rune context of its tree's talents.
	const APlayerState* PlayerState = ObservedPlayerState.Get();
	const APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;
	const URpgEquipmentManagerComponent* Equipment = Pawn ? Pawn->FindComponentByClass<URpgEquipmentManagerComponent>() : nullptr;
	const URpgEquipmentInstance* MainHand = Equipment ? Equipment->GetEquipmentInstanceInSlot(ERpgEquipmentSlot::MainHand) : nullptr;
	const URpgInventoryItemInstance* Tool = MainHand ? Cast<URpgInventoryItemInstance>(MainHand->GetInstigator()) : nullptr;
	const URpgInventoryFragment_ItemContainer* Containers =
		Tool ? Tool->FindFragmentByClass<URpgInventoryFragment_ItemContainer>() : nullptr;
	const URpgInventoryManagerComponent* Inventory = URpgInventoryManagerComponent::FindInventoryOfItem(Tool);

	FRpgInventoryItemContainerDefinition SocketGrid;
	bool bHasSocketGrid = false;
	if (Containers)
	{
		TArray<FRpgInventoryItemContainerDefinition> Grids;
		Containers->GetProvidedContainers(Grids);
		for (const FRpgInventoryItemContainerDefinition& Grid : Grids)
		{
			if (Grid.bSocketGrid && Grid.IsValid())
			{
				SocketGrid = Grid;
				bHasSocketGrid = true;
				break;
			}
		}
	}

	TArray<FRpgSkillTreeRuneSlotView> NewSlots;
	if (bHasSocketGrid && Inventory)
	{
		NewSlots.SetNum(SocketGrid.GridSize.Width * SocketGrid.GridSize.Height);
		for (FRpgSkillTreeRuneSlotView& Slot : NewSlots)
		{
			Slot.Label = NSLOCTEXT("RpgSkillTreeViewModels", "EmptySocket", "Empty");
		}
		for (URpgInventoryItemInstance* Socketed : Inventory->GetSocketedItems(Tool))
		{
			FRpgInventoryGridPlacement Placement;
			const URpgInventoryFragment_Rune* Rune = URpgInventoryFragment_Rune::FindRuneOfItem(Socketed);
			if (!Rune || !Inventory->GetItemPlacement(Socketed, Placement) ||
				Placement.GetContainerHandle().ContainerId != SocketGrid.ContainerId)
			{
				continue;
			}
			const int32 SlotIndex = Placement.Y * SocketGrid.GridSize.Width + Placement.X;
			if (!NewSlots.IsValidIndex(SlotIndex))
			{
				continue;
			}

			static const TCHAR* RankNumerals[] = {TEXT("I"), TEXT("II"), TEXT("III")};
			const int32 Rank = FMath::Clamp(Socketed->GetRuneRank(), 1, UE_ARRAY_COUNT(RankNumerals));
			const URpgInventoryFragment_UIData* UIData = Socketed->FindFragmentByClass<URpgInventoryFragment_UIData>();
			const URpgInventoryItemDefinition* Definition = Socketed->GetItemDef()
				? GetDefault<URpgInventoryItemDefinition>(Socketed->GetItemDef())
				: nullptr;
			FRpgSkillTreeRuneSlotView& Slot = NewSlots[SlotIndex];
			Slot.bHasRune = true;
			Slot.RuneTag = Rune->RuneTag;
			Slot.Icon = UIData ? UIData->Icon : TSoftObjectPtr<UTexture2D>();
			Slot.Label = FText::Format(
				NSLOCTEXT("RpgSkillTreeViewModels", "SocketedRune", "{0} {1}"),
				Definition ? Definition->DisplayName : FText::FromString(Rune->RuneTag.ToString()),
				FText::FromString(RankNumerals[Rank - 1]));
		}
	}

	const URpgInventoryItemDefinition* ToolDefinition =
		Tool && Tool->GetItemDef() ? GetDefault<URpgInventoryItemDefinition>(Tool->GetItemDef()) : nullptr;
	UE_MVVM_SET_PROPERTY_VALUE(bHasRuneTool, !NewSlots.IsEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(RuneToolName, !NewSlots.IsEmpty() && ToolDefinition ? ToolDefinition->DisplayName : FText::GetEmpty());

	// The highlight ends with its rune leaving the tool in hand.
	const bool bHighlightStillSocketed = NewSlots.ContainsByPredicate([this](const FRpgSkillTreeRuneSlotView& Slot)
	{
		return Slot.bHasRune && Slot.RuneTag == HighlightedRuneTag;
	});
	if (!bHighlightStillSocketed)
	{
		UE_MVVM_SET_PROPERTY_VALUE(HighlightedRuneTag, FGameplayTag());
	}
	for (FRpgSkillTreeRuneSlotView& Slot : NewSlots)
	{
		Slot.bIsHighlighted = Slot.bHasRune && HighlightedRuneTag.IsValid() && Slot.RuneTag == HighlightedRuneTag;
	}
	UE_MVVM_SET_PROPERTY_VALUE(RuneSlots, NewSlots);
}

void URpgSkillProgressionViewModel::ApplyRuneHighlight()
{
	TArray<FRpgSkillTreeRuneSlotView> NewSlots = RuneSlots;
	for (FRpgSkillTreeRuneSlotView& Slot : NewSlots)
	{
		Slot.bIsHighlighted = Slot.bHasRune && HighlightedRuneTag.IsValid() && Slot.RuneTag == HighlightedRuneTag;
	}
	UE_MVVM_SET_PROPERTY_VALUE(RuneSlots, NewSlots);
	for (URpgSkillTreeViewModel* TreeViewModel : SkillTrees)
	{
		if (TreeViewModel)
		{
			TreeViewModel->SetHighlightedRune(HighlightedRuneTag);
		}
	}
	OnProgressionChanged.Broadcast();
}

void URpgSkillProgressionViewModel::HandleInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message)
{
	const APlayerState* PlayerState = ObservedPlayerState.Get();
	if (PlayerState && Message.InventoryOwner &&
		Message.InventoryOwner == PlayerState->FindComponentByClass<URpgInventoryManagerComponent>())
	{
		bRefreshTreesOnRebuild = true;
		QueueRefresh();
	}
}

void URpgSkillProgressionViewModel::HandleEquipmentSlotsChanged(
	FGameplayTag Channel,
	const FRpgEquipmentLoadoutSlotsChangedMessage& Message)
{
	const APlayerState* PlayerState = ObservedPlayerState.Get();
	if (PlayerState && Message.Owner && Message.Owner == PlayerState->GetOwner())
	{
		bRefreshTreesOnRebuild = true;
		QueueRefresh();
	}
}

void URpgSkillProgressionViewModel::HandleTreeViewChanged()
{
	UpdateSelection();
	OnProgressionChanged.Broadcast();
}

void URpgSkillProgressionViewModel::ObserveTrees(const TConstArrayView<TObjectPtr<URpgSkillTreeViewModel>> Trees)
{
	for (const TWeakObjectPtr<URpgSkillTreeViewModel>& ObservedTree : ObservedTrees)
	{
		if (URpgSkillTreeViewModel* Tree = ObservedTree.Get(); Tree && !Trees.Contains(Tree))
		{
			Tree->OnTreeChanged.RemoveDynamic(this, &ThisClass::HandleTreeViewChanged);
		}
	}
	ObservedTrees.Reset(Trees.Num());
	for (URpgSkillTreeViewModel* Tree : Trees)
	{
		if (Tree)
		{
			Tree->OnTreeChanged.AddUniqueDynamic(this, &ThisClass::HandleTreeViewChanged);
			ObservedTrees.Add(Tree);
		}
	}
}

void URpgSkillProgressionViewModel::UpdateSelection()
{
	// A selection in another tree than the current one is the newer one.
	URpgSkillTreeNodeViewModel* NewSelectedNode = nullptr;
	for (URpgSkillTreeViewModel* TreeViewModel : SkillTrees)
	{
		URpgSkillTreeNodeViewModel* Node = TreeViewModel ? TreeViewModel->GetSelectedNode() : nullptr;
		if (Node && (!NewSelectedNode || !SelectedNode || Node->GetTree() != SelectedNode->GetTree()))
		{
			NewSelectedNode = Node;
		}
	}
	UE_MVVM_SET_PROPERTY_VALUE(SelectedNode, NewSelectedNode);

	// Selection: the selected node's tree, else the player's pick, else the current one, else the main-hand weapon's
	// tree, else the first.
	const auto FindTree = [this](const FGameplayTag TreeTag) -> URpgSkillTreeViewModel*
	{
		for (URpgSkillTreeViewModel* TreeViewModel : SkillTrees)
		{
			if (TreeViewModel && TreeTag.IsValid() && TreeViewModel->GetTreeTag() == TreeTag)
			{
				return TreeViewModel;
			}
		}
		return nullptr;
	};
	URpgSkillTreeViewModel* NewSelectedTree = NewSelectedNode ? NewSelectedNode->GetTree() : nullptr;
	if (!NewSelectedTree)
	{
		NewSelectedTree = FindTree(RequestedTreeTag);
	}
	if (!NewSelectedTree && SelectedSkillTree)
	{
		NewSelectedTree = FindTree(SelectedSkillTree->GetTreeTag());
	}
	if (!NewSelectedTree)
	{
		const APlayerState* PlayerState = ObservedPlayerState.Get();
		const URpgSkillTreeDefinition* WeaponTree =
			PlayerState ? URpgSkillTreeComponent::FindActiveWeaponSkillTree(PlayerState->GetPawn()) : nullptr;
		NewSelectedTree = WeaponTree ? FindTree(WeaponTree->TreeTag) : nullptr;
	}
	if (!NewSelectedTree && !SkillTrees.IsEmpty())
	{
		NewSelectedTree = SkillTrees[0];
	}
	UE_MVVM_SET_PROPERTY_VALUE(SelectedSkillTree, NewSelectedTree);
}

void URpgSkillProgressionViewModel::Subscribe()
{
	if (URpgSkillTreeComponent* SkillTreeComponent = ObservedSkillTrees.Get())
	{
		SkillTreeComponent->OnSkillTreeChanged.AddUniqueDynamic(this, &ThisClass::HandleSkillTreeChanged);
	}
	if (URpgTradeSkillProgressionComponent* TradeSkills = ObservedTradeSkills.Get())
	{
		TradeSkills->OnTradeSkillTagChanged.AddUniqueDynamic(this, &ThisClass::HandleTradeSkillChanged);
	}
	if (URpgPlayerProgressionComponent* Character = ObservedCharacter.Get())
	{
		Character->OnLevelChanged.AddUniqueDynamic(this, &ThisClass::HandleCharacterLevelChanged);
		Character->OnXPChanged.AddUniqueDynamic(this, &ThisClass::HandleCharacterXPChanged);
	}
	if (URpgRewardClaimComponent* Claims = ObservedRewardClaims.Get())
	{
		Claims->OnClaimsChanged.AddUniqueDynamic(this, &ThisClass::HandleRewardClaimsChanged);
		Claims->OnClaimResult.AddUniqueDynamic(this, &ThisClass::HandleRewardClaimResult);
	}

	// Sockets and the tool in hand decide which learned talents sleep and what the rune bar shows.
	if (UWorld* World = ObservedPlayerState.IsValid() ? ObservedPlayerState->GetWorld() : nullptr)
	{
		UGameplayMessageSubsystem& Messages = UGameplayMessageSubsystem::Get(World);
		InventoryChangedHandle = Messages.RegisterListener<FRpgInventoryChangeMessage>(
			FGameplayTag::RequestGameplayTag(TEXT("Rpg.Inventory.Message.StackChanged")),
			this,
			&ThisClass::HandleInventoryChanged);
		EquipmentChangedHandle = Messages.RegisterListener<FRpgEquipmentLoadoutSlotsChangedMessage>(
			RpgGameplayTags::Rpg_EquipmentLoadout_Message_SlotsChanged,
			this,
			&ThisClass::HandleEquipmentSlotsChanged);
	}
}

void URpgSkillProgressionViewModel::Unsubscribe()
{
	RefreshQueue.Cancel();
	InventoryChangedHandle.Unregister();
	EquipmentChangedHandle.Unregister();
	if (URpgSkillTreeComponent* SkillTreeComponent = ObservedSkillTrees.Get())
	{
		SkillTreeComponent->OnSkillTreeChanged.RemoveDynamic(this, &ThisClass::HandleSkillTreeChanged);
	}
	if (URpgTradeSkillProgressionComponent* TradeSkills = ObservedTradeSkills.Get())
	{
		TradeSkills->OnTradeSkillTagChanged.RemoveDynamic(this, &ThisClass::HandleTradeSkillChanged);
	}
	if (URpgPlayerProgressionComponent* Character = ObservedCharacter.Get())
	{
		Character->OnLevelChanged.RemoveDynamic(this, &ThisClass::HandleCharacterLevelChanged);
		Character->OnXPChanged.RemoveDynamic(this, &ThisClass::HandleCharacterXPChanged);
	}
	if (URpgRewardClaimComponent* Claims = ObservedRewardClaims.Get())
	{
		Claims->OnClaimsChanged.RemoveDynamic(this, &ThisClass::HandleRewardClaimsChanged);
		Claims->OnClaimResult.RemoveDynamic(this, &ThisClass::HandleRewardClaimResult);
	}
}

void URpgSkillProgressionViewModel::QueueRefresh()
{
	if (const APlayerState* PlayerState = ObservedPlayerState.Get())
	{
		RefreshQueue.Queue(PlayerState->GetWorld(), this, &ThisClass::ExecuteQueuedRefresh);
	}
}

void URpgSkillProgressionViewModel::ExecuteQueuedRefresh()
{
	if (RefreshQueue.Consume())
	{
		Rebuild();
	}
}

void URpgSkillProgressionViewModel::Rebuild()
{
	const URpgPlayerProgressionComponent* Character = ObservedCharacter.Get();
	const float NewCharacterXP = Character ? Character->GetXP() : 0.0f;
	const float NewCharacterXPToNextLevel = Character ? Character->GetXPToNextLevelForCurrentLevel() : 0.0f;
	UE_MVVM_SET_PROPERTY_VALUE(CharacterLevel, Character ? Character->GetLevel() : 1);
	UE_MVVM_SET_PROPERTY_VALUE(CharacterXP, NewCharacterXP);
	UE_MVVM_SET_PROPERTY_VALUE(CharacterXPToNextLevel, NewCharacterXPToNextLevel);
	// Character progression without an XP curve reports zero to the next level; that shows an empty bar, not a full one.
	UE_MVVM_SET_PROPERTY_VALUE(CharacterLevelProgress, GetLevelProgress(NewCharacterXP, NewCharacterXPToNextLevel, false));

	const URpgTradeSkillProgressionComponent* TradeSkills = ObservedTradeSkills.Get();
	TMap<FGameplayTag, URpgTradeSkillViewModel*> PreviousSkills;
	for (URpgTradeSkillViewModel* Skill : Skills)
	{
		if (Skill)
		{
			PreviousSkills.Add(Skill->GetSkillTag(), Skill);
		}
	}
	TArray<TObjectPtr<URpgTradeSkillViewModel>> NewSkills;
	if (TradeSkills)
	{
		for (const FTradeSkillState& SkillState : TradeSkills->SkillStates)
		{
			URpgTradeSkillViewModel* Skill = PreviousSkills.FindRef(SkillState.SkillTag);
			if (!Skill)
			{
				Skill = NewObject<URpgTradeSkillViewModel>(this);
			}
			Skill->UpdateSkill(
				SkillState,
				TradeSkills->GetSkillDisplayName(SkillState.SkillTag),
				TradeSkills->GetSkillIcon(SkillState.SkillTag),
				TradeSkills->GetXPToNextLevelByTag(SkillState.SkillTag),
				TradeSkills->GetMaxSkillLevelByTag(SkillState.SkillTag));
			NewSkills.Add(Skill);
		}
	}
	const bool bSkillsChanged = !AreViewModelArraysEqual(Skills, NewSkills);
	Skills = MoveTemp(NewSkills);
	if (bSkillsChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Skills);
	}

	URpgSkillTreeComponent* SkillTreeComponent = ObservedSkillTrees.Get();
	TArray<URpgSkillTreeDefinition*> KnownTrees =
		SkillTreeComponent ? SkillTreeComponent->GetKnownSkillTrees() : TArray<URpgSkillTreeDefinition*>();
	KnownTrees.RemoveAll([](const URpgSkillTreeDefinition* Tree)
	{
		return !Tree || !Tree->TreeTag.IsValid();
	});
	KnownTrees.Sort([](const URpgSkillTreeDefinition& A, const URpgSkillTreeDefinition& B)
	{
		const int32 NameOrder = A.DisplayName.CompareTo(B.DisplayName);
		return NameOrder != 0 ? NameOrder < 0 : A.TreeTag.ToString() < B.TreeTag.ToString();
	});

	TMap<FGameplayTag, URpgSkillTreeViewModel*> PreviousTrees;
	for (URpgSkillTreeViewModel* TreeViewModel : SkillTrees)
	{
		if (TreeViewModel)
		{
			PreviousTrees.Add(TreeViewModel->GetTreeTag(), TreeViewModel);
		}
	}
	TArray<TObjectPtr<URpgSkillTreeViewModel>> NewTrees;
	int32 NewTotalAvailablePoints = 0;
	for (const URpgSkillTreeDefinition* Tree : KnownTrees)
	{
		URpgSkillTreeViewModel* TreeViewModel = nullptr;
		PreviousTrees.RemoveAndCopyValue(Tree->TreeTag, TreeViewModel);
		if (!TreeViewModel)
		{
			TreeViewModel = NewObject<URpgSkillTreeViewModel>(this);
			TreeViewModel->BindSkillTree(SkillTreeComponent, Tree->TreeTag);
		}
		NewTrees.Add(TreeViewModel);
		NewTotalAvailablePoints += SkillTreeComponent->GetAvailablePoints(Tree->TreeTag);
	}
	for (const TPair<FGameplayTag, URpgSkillTreeViewModel*>& RemovedTree : PreviousTrees)
	{
		RemovedTree.Value->UnbindSkillTree();
	}
	const bool bTreesChanged = !AreViewModelArraysEqual(SkillTrees, NewTrees);
	SkillTrees = MoveTemp(NewTrees);
	if (bTreesChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SkillTrees);
	}
	UE_MVVM_SET_PROPERTY_VALUE(TotalAvailablePoints, NewTotalAvailablePoints);

	ObserveTrees(SkillTrees);

	// Sockets or the tool in hand changed: the trees re-derive which learned talents sleep.
	RebuildRuneContext();
	const bool bRefreshTrees = bRefreshTreesOnRebuild;
	bRefreshTreesOnRebuild = false;
	for (URpgSkillTreeViewModel* TreeViewModel : SkillTrees)
	{
		if (TreeViewModel)
		{
			TreeViewModel->SetHighlightedRune(HighlightedRuneTag);
			if (bRefreshTrees)
			{
				TreeViewModel->Refresh();
			}
		}
	}
	UpdateSelection();
	RebuildRewardClaims();

	OnProgressionChanged.Broadcast();
}
