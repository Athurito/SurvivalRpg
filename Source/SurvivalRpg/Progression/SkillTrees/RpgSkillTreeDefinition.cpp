#include "RpgSkillTreeDefinition.h"

#include "Curves/CurveFloat.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgSkillTreeDefinition)

#define LOCTEXT_NAMESPACE "RpgSkillTreeDefinition"

void FRpgSkillTreeNode::GetGrantedAbilityIds(TArray<FGameplayTag>& OutAbilityIds) const
{
	if (!AbilitySet)
	{
		return;
	}

	for (const FRpgAbilitySet_GameplayAbility& GrantedAbility : AbilitySet->GetGrantedGameplayAbilities())
	{
		if (GrantedAbility.Ability && GrantedAbility.AbilityIdTag.IsValid())
		{
			OutAbilityIds.AddUnique(GrantedAbility.AbilityIdTag);
		}
	}
}

const FRpgSkillTreeNode* URpgSkillTreeDefinition::FindNode(const FGameplayTag NodeTag) const
{
	if (!NodeTag.IsValid())
	{
		return nullptr;
	}

	return Nodes.FindByPredicate([NodeTag](const FRpgSkillTreeNode& Node)
	{
		return Node.NodeTag == NodeTag;
	});
}

bool URpgSkillTreeDefinition::K2_FindNode(const FGameplayTag NodeTag, FRpgSkillTreeNode& OutNode) const
{
	if (const FRpgSkillTreeNode* Node = FindNode(NodeTag))
	{
		OutNode = *Node;
		return true;
	}

	OutNode = FRpgSkillTreeNode();
	return false;
}

int32 URpgSkillTreeDefinition::GetEarnedPointsForLevel(const int32 SkillLevel) const
{
	const int32 Level = FMath::Max(1, SkillLevel);
	int32 Points = Level - 1;
	if (PointsByLevel)
	{
		const float AuthoredPoints = PointsByLevel->GetFloatValue(static_cast<float>(Level));
		Points = FMath::IsFinite(AuthoredPoints) ? FMath::FloorToInt32(AuthoredPoints) : 0;
	}

	return FMath::Clamp(Points, 0, FMath::Max(0, MaxPoints));
}

bool URpgSkillTreeDefinition::ValidateTree(TArray<FText>& OutErrors) const
{
	const int32 InitialErrorCount = OutErrors.Num();

	if (!TreeTag.IsValid())
	{
		OutErrors.Add(LOCTEXT("MissingTreeTag", "TreeTag is required; saves and requests address the tree by it."));
	}
	if (!MasterySkillTag.IsValid())
	{
		OutErrors.Add(LOCTEXT("MissingMasterySkill", "MasterySkillTag is required; its level earns the tree's points."));
	}

	TMap<FGameplayTag, int32> NodeIndices;
	TSet<FIntPoint> Positions;
	for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); ++NodeIndex)
	{
		const FRpgSkillTreeNode& Node = Nodes[NodeIndex];
		if (!Node.NodeTag.IsValid())
		{
			OutErrors.Add(FText::Format(LOCTEXT("MissingNodeTag", "Nodes[{0}] has no NodeTag."), NodeIndex));
			continue;
		}
		if (NodeIndices.Contains(Node.NodeTag))
		{
			OutErrors.Add(FText::Format(
				LOCTEXT("DuplicateNodeTag", "Node {0} appears more than once."),
				FText::FromName(Node.NodeTag.GetTagName())));
			continue;
		}
		NodeIndices.Add(Node.NodeTag, NodeIndex);

		const FIntPoint Position(Node.Column, Node.Row);
		if (Positions.Contains(Position))
		{
			OutErrors.Add(FText::Format(
				LOCTEXT("DuplicatePosition", "Node {0} shares row {1}, column {2} with another node."),
				FText::FromName(Node.NodeTag.GetTagName()),
				Node.Row,
				Node.Column));
		}
		Positions.Add(Position);

		if (Node.Cost < 1)
		{
			OutErrors.Add(FText::Format(
				LOCTEXT("InvalidCost", "Node {0} must cost at least one point."),
				FText::FromName(Node.NodeTag.GetTagName())));
		}
		if (Node.RequiredPointsInTree + FMath::Max(1, Node.Cost) > MaxPoints)
		{
			OutErrors.Add(FText::Format(
				LOCTEXT("UnaffordableNode", "Node {0} needs {1} points spent plus its cost, but the tree earns at most {2}."),
				FText::FromName(Node.NodeTag.GetTagName()),
				Node.RequiredPointsInTree,
				MaxPoints));
		}

		for (const FRpgSkillTreeAbilityTuning& Tuning : Node.AbilityTunings)
		{
			if (!Tuning.TuningTag.IsValid() || !FMath::IsFinite(Tuning.Value))
			{
				OutErrors.Add(FText::Format(
					LOCTEXT("InvalidTuning", "Node {0} has a tuning entry without TuningTag or with a non-finite value."),
					FText::FromName(Node.NodeTag.GetTagName())));
			}
		}
	}

	// Prerequisites must exist, may not exclude their dependent, and may not form cycles.
	for (const FRpgSkillTreeNode& Node : Nodes)
	{
		if (!Node.NodeTag.IsValid())
		{
			continue;
		}

		for (const FGameplayTag& Prerequisite : Node.Prerequisites)
		{
			const int32* PrerequisiteIndex = NodeIndices.Find(Prerequisite);
			if (!PrerequisiteIndex || Prerequisite == Node.NodeTag)
			{
				OutErrors.Add(FText::Format(
					LOCTEXT("InvalidPrerequisite", "Node {0} requires {1}, which is not another node of this tree."),
					FText::FromName(Node.NodeTag.GetTagName()),
					FText::FromName(Prerequisite.GetTagName())));
				continue;
			}

			const FRpgSkillTreeNode& PrerequisiteNode = Nodes[*PrerequisiteIndex];
			if (!Node.ExclusiveGroup.IsNone() && PrerequisiteNode.ExclusiveGroup == Node.ExclusiveGroup)
			{
				OutErrors.Add(FText::Format(
					LOCTEXT("ExclusivePrerequisite", "Node {0} requires {1} from its own exclusive group and can never be learned."),
					FText::FromName(Node.NodeTag.GetTagName()),
					FText::FromName(Prerequisite.GetTagName())));
			}
		}
	}

	// Depth-first search with colors: 0 unvisited, 1 on the stack, 2 done.
	TMap<FGameplayTag, uint8> VisitState;
	TFunction<bool(const FGameplayTag&)> HasCycleFrom = [&](const FGameplayTag& NodeTag) -> bool
	{
		uint8& State = VisitState.FindOrAdd(NodeTag);
		if (State == 1)
		{
			return true;
		}
		if (State == 2)
		{
			return false;
		}

		State = 1;
		const int32* NodeIndex = NodeIndices.Find(NodeTag);
		if (NodeIndex)
		{
			for (const FGameplayTag& Prerequisite : Nodes[*NodeIndex].Prerequisites)
			{
				if (NodeIndices.Contains(Prerequisite) && Prerequisite != NodeTag && HasCycleFrom(Prerequisite))
				{
					return true;
				}
			}
		}
		VisitState.FindOrAdd(NodeTag) = 2;
		return false;
	};
	for (const TPair<FGameplayTag, int32>& Pair : NodeIndices)
	{
		if (HasCycleFrom(Pair.Key))
		{
			OutErrors.Add(FText::Format(
				LOCTEXT("PrerequisiteCycle", "The prerequisites of node {0} form a cycle."),
				FText::FromName(Pair.Key.GetTagName())));
			break;
		}
	}

	// A point gate is reachable when the other nodes, at most one per exclusive group, can absorb the required points.
	for (const FRpgSkillTreeNode& Node : Nodes)
	{
		if (!Node.NodeTag.IsValid() || Node.RequiredPointsInTree <= 0)
		{
			continue;
		}

		int32 SpendablePoints = 0;
		TMap<FName, int32> MostExpensiveByGroup;
		for (const FRpgSkillTreeNode& Other : Nodes)
		{
			if (&Other == &Node || !Other.NodeTag.IsValid() ||
				(!Node.ExclusiveGroup.IsNone() && Other.ExclusiveGroup == Node.ExclusiveGroup))
			{
				continue;
			}

			const int32 OtherCost = FMath::Max(1, Other.Cost);
			if (Other.ExclusiveGroup.IsNone())
			{
				SpendablePoints += OtherCost;
			}
			else
			{
				int32& GroupCost = MostExpensiveByGroup.FindOrAdd(Other.ExclusiveGroup);
				GroupCost = FMath::Max(GroupCost, OtherCost);
			}
		}
		for (const TPair<FName, int32>& Group : MostExpensiveByGroup)
		{
			SpendablePoints += Group.Value;
		}

		if (SpendablePoints < Node.RequiredPointsInTree)
		{
			OutErrors.Add(FText::Format(
				LOCTEXT("UnreachableGate", "Node {0} needs {1} points spent in the tree, but the other nodes absorb at most {2}."),
				FText::FromName(Node.NodeTag.GetTagName()),
				Node.RequiredPointsInTree,
				SpendablePoints));
		}
	}

	return OutErrors.Num() == InitialErrorCount;
}

#if WITH_EDITOR
EDataValidationResult URpgSkillTreeDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	TArray<FText> Errors;
	if (!ValidateTree(Errors))
	{
		for (const FText& Error : Errors)
		{
			Context.AddError(Error);
		}
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
