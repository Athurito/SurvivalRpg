#include "RpgSkillTreeComponent.h"

#include "Abilities/GameplayAbility.h"
#include "Engine/AssetManager.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "RpgSkillTreeDefinition.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Equipment/RpgEquipmentInstance.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_SkillTree.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillProgressionComponent.h"
#include "SurvivalRpg/SurvivalRpg.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgSkillTreeComponent)

namespace RpgSkillTree
{
	/** Primary asset type of URpgSkillTreeDefinition; native data assets use their class name. */
	static const FPrimaryAssetType SkillTreeAssetType(TEXT("RpgSkillTreeDefinition"));

	bool HasUniqueValidTags(const TArray<FGameplayTag>& Tags, const bool bAllowEmpty)
	{
		TSet<FGameplayTag> Seen;
		for (const FGameplayTag& Tag : Tags)
		{
			if (!Tag.IsValid())
			{
				if (bAllowEmpty)
				{
					continue;
				}
				return false;
			}
			if (Seen.Contains(Tag))
			{
				return false;
			}
			Seen.Add(Tag);
		}
		return true;
	}
}

bool FRpgSkillTreeState::IsValid() const
{
	return TreeTag.IsValid() &&
		SlotAbilityIds.Num() <= URpgSkillTreeComponent::SlotCount &&
		RpgSkillTree::HasUniqueValidTags(UnlockedNodes, false) &&
		RpgSkillTree::HasUniqueValidTags(SlotAbilityIds, true);
}

URpgSkillTreeComponent::URpgSkillTreeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void URpgSkillTreeComponent::BeginPlay()
{
	Super::BeginPlay();
	RegisterSkillTreesFromAssetManager();
}

void URpgSkillTreeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(URpgSkillTreeComponent, TreeStates, COND_OwnerOnly, REPNOTIFY_Always);
}

void URpgSkillTreeComponent::RegisterSkillTree(const URpgSkillTreeDefinition* Tree)
{
	if (!Tree || !Tree->TreeTag.IsValid() || KnownTrees.Contains(Tree))
	{
		return;
	}

	if (const URpgSkillTreeDefinition* Existing = FindSkillTree(Tree->TreeTag))
	{
		UE_LOG(LogRpg, Error, TEXT("Skill tree [%s] and [%s] share tree tag [%s]; keeping the first."),
			*GetNameSafe(Existing), *GetNameSafe(Tree), *Tree->TreeTag.ToString());
		return;
	}

	KnownTrees.Add(Tree);
}

URpgSkillTreeDefinition* URpgSkillTreeComponent::FindSkillTree(const FGameplayTag TreeTag) const
{
	for (const TObjectPtr<const URpgSkillTreeDefinition>& Tree : KnownTrees)
	{
		if (Tree && Tree->TreeTag == TreeTag)
		{
			return const_cast<URpgSkillTreeDefinition*>(Tree.Get());
		}
	}
	return nullptr;
}

TArray<URpgSkillTreeDefinition*> URpgSkillTreeComponent::GetKnownSkillTrees() const
{
	TArray<URpgSkillTreeDefinition*> Trees;
	for (const TObjectPtr<const URpgSkillTreeDefinition>& Tree : KnownTrees)
	{
		if (Tree)
		{
			Trees.Add(const_cast<URpgSkillTreeDefinition*>(Tree.Get()));
		}
	}
	return Trees;
}

bool URpgSkillTreeComponent::IsNodeUnlocked(const FGameplayTag TreeTag, const FGameplayTag NodeTag) const
{
	const FRpgSkillTreeState* State = FindState(TreeTag);
	return State && NodeTag.IsValid() && State->UnlockedNodes.Contains(NodeTag);
}

TArray<FGameplayTag> URpgSkillTreeComponent::GetUnlockedNodes(const FGameplayTag TreeTag) const
{
	const FRpgSkillTreeState* State = FindState(TreeTag);
	return State ? State->UnlockedNodes : TArray<FGameplayTag>();
}

int32 URpgSkillTreeComponent::GetEarnedPoints(const FGameplayTag TreeTag) const
{
	const URpgSkillTreeDefinition* Tree = FindSkillTree(TreeTag);
	return Tree ? Tree->GetEarnedPointsForLevel(GetMasterySkillLevel(*Tree)) : 0;
}

int32 URpgSkillTreeComponent::GetSpentPoints(const FGameplayTag TreeTag) const
{
	const URpgSkillTreeDefinition* Tree = FindSkillTree(TreeTag);
	return Tree ? CalculateSpentPoints(*Tree, FindState(TreeTag)) : 0;
}

int32 URpgSkillTreeComponent::GetAvailablePoints(const FGameplayTag TreeTag) const
{
	return FMath::Max(0, GetEarnedPoints(TreeTag) - GetSpentPoints(TreeTag));
}

ERpgSkillTreeUnlockResult URpgSkillTreeComponent::EvaluateUnlock(const FGameplayTag TreeTag, const FGameplayTag NodeTag) const
{
	const URpgSkillTreeDefinition* Tree = FindSkillTree(TreeTag);
	if (!Tree)
	{
		return ERpgSkillTreeUnlockResult::UnknownTree;
	}

	return EvaluateUnlockForState(
		*Tree,
		FindState(TreeTag),
		NodeTag,
		Tree->GetEarnedPointsForLevel(GetMasterySkillLevel(*Tree)));
}

FGameplayTag URpgSkillTreeComponent::GetSlotAbilityId(const FGameplayTag TreeTag, const int32 SlotIndex) const
{
	const FRpgSkillTreeState* State = FindState(TreeTag);
	return State && State->SlotAbilityIds.IsValidIndex(SlotIndex) ? State->SlotAbilityIds[SlotIndex] : FGameplayTag();
}

void URpgSkillTreeComponent::RequestUnlockNode_Implementation(const FGameplayTag TreeTag, const FGameplayTag NodeTag)
{
	const ERpgSkillTreeUnlockResult Result = UnlockNode(TreeTag, NodeTag);
	if (Result != ERpgSkillTreeUnlockResult::Unlockable)
	{
		UE_LOG(LogRpg, Verbose, TEXT("Skill tree [%s] rejected node [%s] for [%s]: %s."),
			*TreeTag.ToString(), *NodeTag.ToString(), *GetNameSafe(GetOwner()),
			*UEnum::GetValueAsString(Result));
	}
}

void URpgSkillTreeComponent::RequestResetTree_Implementation(const FGameplayTag TreeTag)
{
	ResetTree(TreeTag);
}

void URpgSkillTreeComponent::RequestAssignSlot_Implementation(
	const FGameplayTag TreeTag,
	const int32 SlotIndex,
	const FGameplayTag AbilityIdTag)
{
	AssignSlot(TreeTag, SlotIndex, AbilityIdTag);
}

ERpgSkillTreeUnlockResult URpgSkillTreeComponent::UnlockNode(const FGameplayTag TreeTag, const FGameplayTag NodeTag)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return ERpgSkillTreeUnlockResult::UnknownTree;
	}

	const ERpgSkillTreeUnlockResult Result = EvaluateUnlock(TreeTag, NodeTag);
	if (Result != ERpgSkillTreeUnlockResult::Unlockable)
	{
		return Result;
	}

	const URpgSkillTreeDefinition* Tree = FindSkillTree(TreeTag);
	const FRpgSkillTreeNode* Node = Tree ? Tree->FindNode(NodeTag) : nullptr;
	check(Node);

	FRpgSkillTreeState& State = FindOrAddState(TreeTag);
	State.UnlockedNodes.Add(NodeTag);
	AssignFreeSlots(*Node, State);
	HandleSkillTreeChanged(TreeTag);
	return ERpgSkillTreeUnlockResult::Unlockable;
}

bool URpgSkillTreeComponent::ResetTree(const FGameplayTag TreeTag)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	FRpgSkillTreeState* State = FindMutableState(TreeTag);
	if (!State)
	{
		return false;
	}

	const bool bHasSlots = State->SlotAbilityIds.ContainsByPredicate([](const FGameplayTag& AbilityId)
	{
		return AbilityId.IsValid();
	});
	if (State->UnlockedNodes.IsEmpty() && !bHasSlots)
	{
		return false;
	}

	State->UnlockedNodes.Reset();
	State->SlotAbilityIds.Init(FGameplayTag(), SlotCount);
	HandleSkillTreeChanged(TreeTag);
	return true;
}

bool URpgSkillTreeComponent::AssignSlot(const FGameplayTag TreeTag, const int32 SlotIndex, const FGameplayTag AbilityIdTag)
{
	const URpgSkillTreeDefinition* Tree = FindSkillTree(TreeTag);
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Tree || SlotIndex < 0 || SlotIndex >= SlotCount)
	{
		return false;
	}

	FRpgSkillTreeState& State = FindOrAddState(TreeTag);
	State.SlotAbilityIds.SetNum(SlotCount);
	if (AbilityIdTag.IsValid())
	{
		bool bLearned = false;
		for (const FGameplayTag& NodeTag : State.UnlockedNodes)
		{
			TArray<FGameplayTag> AbilityIds;
			if (const FRpgSkillTreeNode* Node = Tree->FindNode(NodeTag))
			{
				Node->GetGrantedAbilityIds(AbilityIds);
			}
			if (AbilityIds.Contains(AbilityIdTag))
			{
				bLearned = true;
				break;
			}
		}
		if (!bLearned)
		{
			return false;
		}
	}

	if (State.SlotAbilityIds[SlotIndex] == AbilityIdTag)
	{
		return false;
	}

	// Moving an ability that already sits on another slot swaps it with this slot's occupant.
	const int32 PreviousSlot = AbilityIdTag.IsValid() ? State.SlotAbilityIds.IndexOfByKey(AbilityIdTag) : INDEX_NONE;
	if (PreviousSlot != INDEX_NONE)
	{
		State.SlotAbilityIds[PreviousSlot] = State.SlotAbilityIds[SlotIndex];
	}
	State.SlotAbilityIds[SlotIndex] = AbilityIdTag;
	HandleSkillTreeChanged(TreeTag);
	return true;
}

float URpgSkillTreeComponent::ResolveAbilityTuning(
	const URpgSkillTreeDefinition& Tree,
	const FGameplayTagContainer& AbilityIds,
	const FGameplayTag TuningTag,
	const float BaseValue) const
{
	const FRpgSkillTreeState* State = FindState(Tree.TreeTag);
	if (!State || !TuningTag.IsValid())
	{
		return BaseValue;
	}

	bool bHasSet = false;
	float SetValue = BaseValue;
	float AddSum = 0.0f;
	float MultiplyProduct = 1.0f;
	for (const FRpgSkillTreeNode& Node : Tree.Nodes)
	{
		if (!State->UnlockedNodes.Contains(Node.NodeTag))
		{
			continue;
		}

		for (const FRpgSkillTreeAbilityTuning& Tuning : Node.AbilityTunings)
		{
			if (Tuning.TuningTag != TuningTag || !FMath::IsFinite(Tuning.Value) ||
				(Tuning.AbilityIdTag.IsValid() && !AbilityIds.HasTagExact(Tuning.AbilityIdTag)))
			{
				continue;
			}

			switch (Tuning.Operation)
			{
			case ERpgSkillTreeTuningOperation::Set:
				bHasSet = true;
				SetValue = Tuning.Value;
				break;
			case ERpgSkillTreeTuningOperation::Add:
				AddSum += Tuning.Value;
				break;
			case ERpgSkillTreeTuningOperation::Multiply:
				MultiplyProduct *= Tuning.Value;
				break;
			}
		}
	}

	return ((bHasSet ? SetValue : BaseValue) + AddSum) * MultiplyProduct;
}

float URpgSkillTreeComponent::ResolveAbilityTuningForSpec(
	const FGameplayAbilitySpec& Spec,
	const FGameplayAbilityActorInfo& ActorInfo,
	const FGameplayTag TuningTag,
	const float BaseValue)
{
	const URpgEquipmentInstance* Equipment = Cast<URpgEquipmentInstance>(Spec.SourceObject.Get());
	const URpgSkillTreeDefinition* Tree = Equipment
		? URpgInventoryFragment_SkillTree::FindSkillTreeOfItem(Equipment->GetInstigator())
		: nullptr;
	URpgSkillTreeComponent* SkillTrees = FindForActor(ActorInfo.OwnerActor.Get());
	if (!SkillTrees)
	{
		SkillTrees = FindForActor(ActorInfo.AvatarActor.Get());
	}
	if (!Tree || !SkillTrees)
	{
		return BaseValue;
	}

	// The same ids the Q/E/R resolver uses: Ability.* spec-source tags and the ability's asset tags.
	FGameplayTagContainer AbilityIds;
	const FGameplayTag AbilityRoot = FGameplayTag::RequestGameplayTag(TEXT("Ability"), false);
	for (const FGameplayTag& SourceTag : Spec.GetDynamicSpecSourceTags())
	{
		if (AbilityRoot.IsValid() && SourceTag.MatchesTag(AbilityRoot))
		{
			AbilityIds.AddTag(SourceTag);
		}
	}
	if (Spec.Ability)
	{
		AbilityIds.AppendTags(Spec.Ability->GetAssetTags());
	}

	return SkillTrees->ResolveAbilityTuning(*Tree, AbilityIds, TuningTag, BaseValue);
}

URpgSkillTreeComponent* URpgSkillTreeComponent::FindForActor(const AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	if (URpgSkillTreeComponent* Direct = Actor->FindComponentByClass<URpgSkillTreeComponent>())
	{
		return Direct;
	}

	const APlayerState* PlayerState = nullptr;
	if (const APawn* Pawn = Cast<APawn>(Actor))
	{
		PlayerState = Pawn->GetPlayerState();
	}
	else if (const AController* Controller = Cast<AController>(Actor))
	{
		PlayerState = Controller->PlayerState;
	}

	return PlayerState ? PlayerState->FindComponentByClass<URpgSkillTreeComponent>() : nullptr;
}

const URpgSkillTreeDefinition* URpgSkillTreeComponent::FindActiveWeaponSkillTree(const APawn* Pawn)
{
	const URpgEquipmentManagerComponent* Equipment = Pawn ? Pawn->FindComponentByClass<URpgEquipmentManagerComponent>() : nullptr;
	if (!Equipment)
	{
		return nullptr;
	}

	for (const ERpgEquipmentSlot Slot : {ERpgEquipmentSlot::MainHand, ERpgEquipmentSlot::OffHand})
	{
		const URpgEquipmentInstance* Instance = Equipment->GetEquipmentInstanceInSlot(Slot);
		if (const URpgSkillTreeDefinition* Tree = Instance
			? URpgInventoryFragment_SkillTree::FindActiveSkillTreeOfItem(Instance->GetInstigator(), Slot)
			: nullptr)
		{
			return Tree;
		}
	}
	return nullptr;
}

bool URpgSkillTreeComponent::RestoreSkillTreeStates(const TArray<FRpgSkillTreeState>& InStates)
{
	FString Error;
	if (!GetOwner() || !GetOwner()->HasAuthority() || !ValidateSkillTreeStates(InStates, &Error))
	{
		UE_LOG(LogRpg, Error, TEXT("Skill tree restore rejected for [%s]: %s"), *GetNameSafe(GetOwner()), *Error);
		return false;
	}

	TArray<FGameplayTag> ChangedTrees;
	for (const FRpgSkillTreeState& State : TreeStates)
	{
		ChangedTrees.AddUnique(State.TreeTag);
	}

	TreeStates = InStates;
	for (FRpgSkillTreeState& State : TreeStates)
	{
		State.SlotAbilityIds.SetNum(SlotCount);
		ChangedTrees.AddUnique(State.TreeTag);
		if (const URpgSkillTreeDefinition* Tree = FindSkillTree(State.TreeTag))
		{
			ReplayRestoredState(*Tree, State);
		}
	}

	for (const FGameplayTag& TreeTag : ChangedTrees)
	{
		OnSkillTreeChanged.Broadcast(TreeTag);
	}
	RefreshOwnerGrants();
	GetOwner()->ForceNetUpdate();
	return true;
}

void URpgSkillTreeComponent::ResetSkillTreesToDefaults()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	TArray<FGameplayTag> ChangedTrees;
	for (const FRpgSkillTreeState& State : TreeStates)
	{
		ChangedTrees.Add(State.TreeTag);
	}
	TreeStates.Reset();

	for (const FGameplayTag& TreeTag : ChangedTrees)
	{
		OnSkillTreeChanged.Broadcast(TreeTag);
	}
	RefreshOwnerGrants();
	GetOwner()->ForceNetUpdate();
}

bool URpgSkillTreeComponent::ValidateSkillTreeStates(const TArray<FRpgSkillTreeState>& InStates, FString* OutError)
{
	TSet<FGameplayTag> SeenTrees;
	for (const FRpgSkillTreeState& State : InStates)
	{
		if (!State.IsValid())
		{
			if (OutError)
			{
				*OutError = FString::Printf(TEXT("Skill tree state [%s] has invalid or duplicate tags."), *State.TreeTag.ToString());
			}
			return false;
		}
		if (SeenTrees.Contains(State.TreeTag))
		{
			if (OutError)
			{
				*OutError = FString::Printf(TEXT("Skill tree [%s] is saved more than once."), *State.TreeTag.ToString());
			}
			return false;
		}
		SeenTrees.Add(State.TreeTag);
	}
	return true;
}

void URpgSkillTreeComponent::OnRep_TreeStates()
{
	// Known trees without a state are included, so a cleared state still refreshes their views.
	TArray<FGameplayTag, TInlineAllocator<8>> ChangedTreeTags;
	for (const FRpgSkillTreeState& State : TreeStates)
	{
		ChangedTreeTags.AddUnique(State.TreeTag);
	}
	for (const TObjectPtr<const URpgSkillTreeDefinition>& Tree : KnownTrees)
	{
		if (Tree)
		{
			ChangedTreeTags.AddUnique(Tree->TreeTag);
		}
	}
	for (const FGameplayTag& TreeTag : ChangedTreeTags)
	{
		OnSkillTreeChanged.Broadcast(TreeTag);
	}
}

const FRpgSkillTreeState* URpgSkillTreeComponent::FindState(const FGameplayTag TreeTag) const
{
	return TreeStates.FindByPredicate([TreeTag](const FRpgSkillTreeState& State)
	{
		return State.TreeTag == TreeTag;
	});
}

FRpgSkillTreeState* URpgSkillTreeComponent::FindMutableState(const FGameplayTag TreeTag)
{
	return TreeStates.FindByPredicate([TreeTag](const FRpgSkillTreeState& State)
	{
		return State.TreeTag == TreeTag;
	});
}

FRpgSkillTreeState& URpgSkillTreeComponent::FindOrAddState(const FGameplayTag TreeTag)
{
	if (FRpgSkillTreeState* Existing = FindMutableState(TreeTag))
	{
		return *Existing;
	}

	FRpgSkillTreeState& State = TreeStates.AddDefaulted_GetRef();
	State.TreeTag = TreeTag;
	State.SlotAbilityIds.Init(FGameplayTag(), SlotCount);
	return State;
}

int32 URpgSkillTreeComponent::GetMasterySkillLevel(const URpgSkillTreeDefinition& Tree) const
{
	const URpgTradeSkillProgressionComponent* TradeSkills =
		GetOwner() ? GetOwner()->FindComponentByClass<URpgTradeSkillProgressionComponent>() : nullptr;
	return TradeSkills ? TradeSkills->GetSkillLevelByTag(Tree.MasterySkillTag) : 1;
}

int32 URpgSkillTreeComponent::CalculateSpentPoints(const URpgSkillTreeDefinition& Tree, const FRpgSkillTreeState* State) const
{
	int32 Spent = 0;
	if (State)
	{
		for (const FGameplayTag& NodeTag : State->UnlockedNodes)
		{
			if (const FRpgSkillTreeNode* Node = Tree.FindNode(NodeTag))
			{
				Spent += FMath::Max(1, Node->Cost);
			}
		}
	}
	return Spent;
}

ERpgSkillTreeUnlockResult URpgSkillTreeComponent::EvaluateUnlockForState(
	const URpgSkillTreeDefinition& Tree,
	const FRpgSkillTreeState* State,
	const FGameplayTag NodeTag,
	const int32 EarnedPoints) const
{
	const FRpgSkillTreeNode* Node = Tree.FindNode(NodeTag);
	if (!Node)
	{
		return ERpgSkillTreeUnlockResult::UnknownNode;
	}

	const TArray<FGameplayTag> NoNodes;
	const TArray<FGameplayTag>& Unlocked = State ? State->UnlockedNodes : NoNodes;
	if (Unlocked.Contains(NodeTag))
	{
		return ERpgSkillTreeUnlockResult::AlreadyUnlocked;
	}

	if (!Node->ExclusiveGroup.IsNone())
	{
		for (const FGameplayTag& UnlockedTag : Unlocked)
		{
			const FRpgSkillTreeNode* UnlockedNode = Tree.FindNode(UnlockedTag);
			if (UnlockedNode && UnlockedNode->ExclusiveGroup == Node->ExclusiveGroup)
			{
				return ERpgSkillTreeUnlockResult::ExclusiveConflict;
			}
		}
	}

	for (const FGameplayTag& Prerequisite : Node->Prerequisites)
	{
		if (!Unlocked.Contains(Prerequisite))
		{
			return ERpgSkillTreeUnlockResult::MissingPrerequisite;
		}
	}

	const int32 Spent = CalculateSpentPoints(Tree, State);
	if (Spent < Node->RequiredPointsInTree)
	{
		return ERpgSkillTreeUnlockResult::TreePointsRequired;
	}

	if (EarnedPoints - Spent < FMath::Max(1, Node->Cost))
	{
		return ERpgSkillTreeUnlockResult::NotEnoughPoints;
	}

	return ERpgSkillTreeUnlockResult::Unlockable;
}

void URpgSkillTreeComponent::AssignFreeSlots(const FRpgSkillTreeNode& Node, FRpgSkillTreeState& State) const
{
	State.SlotAbilityIds.SetNum(SlotCount);

	TArray<FGameplayTag> AbilityIds;
	Node.GetGrantedAbilityIds(AbilityIds);
	for (const FGameplayTag& AbilityId : AbilityIds)
	{
		if (State.SlotAbilityIds.Contains(AbilityId))
		{
			continue;
		}

		const int32 FreeSlot = State.SlotAbilityIds.IndexOfByPredicate([](const FGameplayTag& Occupant)
		{
			return !Occupant.IsValid();
		});
		if (FreeSlot == INDEX_NONE)
		{
			return;
		}
		State.SlotAbilityIds[FreeSlot] = AbilityId;
	}
}

void URpgSkillTreeComponent::SanitizeSlots(const URpgSkillTreeDefinition& Tree, FRpgSkillTreeState& State) const
{
	TArray<FGameplayTag> LearnedAbilityIds;
	for (const FGameplayTag& NodeTag : State.UnlockedNodes)
	{
		if (const FRpgSkillTreeNode* Node = Tree.FindNode(NodeTag))
		{
			Node->GetGrantedAbilityIds(LearnedAbilityIds);
		}
	}

	State.SlotAbilityIds.SetNum(SlotCount);
	for (FGameplayTag& AbilityId : State.SlotAbilityIds)
	{
		if (AbilityId.IsValid() && !LearnedAbilityIds.Contains(AbilityId))
		{
			AbilityId = FGameplayTag();
		}
	}
}

bool URpgSkillTreeComponent::ReplayRestoredState(const URpgSkillTreeDefinition& Tree, FRpgSkillTreeState& State) const
{
	const int32 EarnedPoints = Tree.GetEarnedPointsForLevel(GetMasterySkillLevel(Tree));
	const TArray<FGameplayTag> SavedNodes = State.UnlockedNodes;

	FRpgSkillTreeState Replayed;
	Replayed.TreeTag = State.TreeTag;
	bool bResetTree = false;
	for (const FGameplayTag& NodeTag : SavedNodes)
	{
		const ERpgSkillTreeUnlockResult Result = EvaluateUnlockForState(Tree, &Replayed, NodeTag, EarnedPoints);
		if (Result == ERpgSkillTreeUnlockResult::Unlockable)
		{
			Replayed.UnlockedNodes.Add(NodeTag);
		}
		else if (Result == ERpgSkillTreeUnlockResult::UnknownNode)
		{
			UE_LOG(LogRpg, Warning, TEXT("Skill tree [%s] no longer has saved node [%s]; its points are refunded."),
				*State.TreeTag.ToString(), *NodeTag.ToString());
		}
		else
		{
			UE_LOG(LogRpg, Warning, TEXT("Saved node [%s] of skill tree [%s] no longer fits (%s); the tree is reset and its points refunded."),
				*NodeTag.ToString(), *State.TreeTag.ToString(), *UEnum::GetValueAsString(Result));
			bResetTree = true;
			break;
		}
	}

	State.UnlockedNodes = bResetTree ? TArray<FGameplayTag>() : Replayed.UnlockedNodes;
	SanitizeSlots(Tree, State);
	return !bResetTree && State.UnlockedNodes.Num() == SavedNodes.Num();
}

void URpgSkillTreeComponent::RegisterSkillTreesFromAssetManager()
{
	if (!UAssetManager::IsInitialized())
	{
		return;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	TArray<FPrimaryAssetId> TreeIds;
	AssetManager.GetPrimaryAssetIdList(RpgSkillTree::SkillTreeAssetType, TreeIds);
	for (const FPrimaryAssetId& TreeId : TreeIds)
	{
		const FSoftObjectPath TreePath = AssetManager.GetPrimaryAssetPath(TreeId);
		RegisterSkillTree(Cast<URpgSkillTreeDefinition>(TreePath.TryLoad()));
	}
}

void URpgSkillTreeComponent::HandleSkillTreeChanged(const FGameplayTag TreeTag)
{
	OnSkillTreeChanged.Broadcast(TreeTag);
	RefreshOwnerGrants();
	MarkOwnerSaveDirty();
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
}

void URpgSkillTreeComponent::RefreshOwnerGrants() const
{
	const AActor* Owner = GetOwner();
	const APlayerState* PlayerState = Cast<APlayerState>(Owner);
	const APawn* Pawn = PlayerState ? PlayerState->GetPawn() : Cast<APawn>(Owner);
	if (URpgEquipmentManagerComponent* Equipment = Pawn ? Pawn->FindComponentByClass<URpgEquipmentManagerComponent>() : nullptr)
	{
		Equipment->RefreshSkillTreeGrants();
	}
}

void URpgSkillTreeComponent::MarkOwnerSaveDirty() const
{
	const AActor* Owner = GetOwner();
	APlayerController* PlayerController = Owner ? Cast<APlayerController>(Owner->GetOwner()) : nullptr;
	if (ARpgGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARpgGameModeBase>() : nullptr)
	{
		GameMode->MarkPlayerSaveDirty(PlayerController);
	}
}
