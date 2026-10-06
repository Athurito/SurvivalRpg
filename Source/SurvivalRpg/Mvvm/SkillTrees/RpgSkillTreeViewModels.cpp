#include "RpgSkillTreeViewModels.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "SurvivalRpg/Progression/Player/RpgPlayerProgressionComponent.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeDefinition.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillProgressionComponent.h"

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

void URpgSkillTreeNodeViewModel::UpdateNode(
	URpgSkillTreeViewModel* InTree,
	const FRpgSkillTreeNode& Node,
	const ERpgSkillTreeUnlockResult InUnlockResult,
	const int32 InAssignedSlotIndex,
	const bool bInSelected,
	const bool bInRowUnlocked,
	const bool bInCanRefund)
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
	SkillTrees->RequestUnlockNode(TreeTag, NodeTag);
}

void URpgSkillTreeViewModel::RequestRefundNode(const FGameplayTag NodeTag)
{
	URpgSkillTreeComponent* SkillTrees = ObservedSkillTrees.Get();
	if (SkillTrees && TreeTag.IsValid() && NodeTag.IsValid())
	{
		SkillTrees->RequestRefundNode(TreeTag, NodeTag);
	}
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

void URpgSkillTreeViewModel::HandleSkillTreeChanged(const FGameplayTag ChangedTreeTag)
{
	if (ChangedTreeTag == TreeTag)
	{
		QueueRefresh();
	}
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
			NodeViewModel->UpdateNode(
				this,
				Node,
				SkillTrees->EvaluateUnlock(TreeTag, Node.NodeTag),
				AssignedSlot,
				SelectedNodeTag.IsValid() && Node.NodeTag == SelectedNodeTag,
				NewSpentPoints >= Node.RequiredPointsInTree,
				SkillTrees->EvaluateRefund(TreeTag, Node.NodeTag) == ERpgSkillTreeRefundResult::Refundable);
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
	const float InXPToNextLevel,
	const int32 InMaxLevel)
{
	const bool bNewIsMaxLevel = SkillState.Level >= InMaxLevel;
	UE_MVVM_SET_PROPERTY_VALUE(SkillTag, SkillState.SkillTag);
	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, InDisplayName);
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
	ObserveSelectedTree(nullptr);
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

void URpgSkillProgressionViewModel::HandleSelectedTreeChanged()
{
	OnProgressionChanged.Broadcast();
}

void URpgSkillProgressionViewModel::ObserveSelectedTree(URpgSkillTreeViewModel* TreeViewModel)
{
	if (ObservedSelectedTree.Get() == TreeViewModel)
	{
		return;
	}
	if (URpgSkillTreeViewModel* PreviousTree = ObservedSelectedTree.Get())
	{
		PreviousTree->OnTreeChanged.RemoveDynamic(this, &ThisClass::HandleSelectedTreeChanged);
	}
	ObservedSelectedTree = TreeViewModel;
	if (TreeViewModel)
	{
		TreeViewModel->OnTreeChanged.AddUniqueDynamic(this, &ThisClass::HandleSelectedTreeChanged);
	}
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
}

void URpgSkillProgressionViewModel::Unsubscribe()
{
	RefreshQueue.Cancel();
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

	// Selection: the player's pick, else the current one, else the main-hand weapon's tree, else the first.
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
	URpgSkillTreeViewModel* NewSelectedTree = FindTree(RequestedTreeTag);
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
	ObserveSelectedTree(NewSelectedTree);

	OnProgressionChanged.Broadcast();
}
