#include "RpgSkillTreeDefinition.h"

#include "Curves/CurveFloat.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/Progression/Rewards/RpgRewardClaimDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgSkillTreeDefinition)

#define LOCTEXT_NAMESPACE "RpgSkillTreeDefinition"

void FRpgAbilityTuningAccumulator::Apply(
	const FRpgSkillTreeAbilityTuning& Tuning,
	const FGameplayTagContainer& AbilityIds,
	const FGameplayTag TuningTag)
{
	if (!TuningTag.IsValid() || Tuning.TuningTag != TuningTag || !FMath::IsFinite(Tuning.Value) ||
		(Tuning.AbilityIdTag.IsValid() && !AbilityIds.HasTagExact(Tuning.AbilityIdTag)))
	{
		return;
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

float FRpgAbilityTuningAccumulator::Resolve(const float BaseValue) const
{
	return ((bHasSet ? SetValue : BaseValue) + AddSum) * MultiplyProduct;
}

bool FRpgSkillTreeNode::IsActiveWithRunes(const FGameplayTagContainer& SocketedRunes) const
{
	if (RequiredRunes.IsEmpty())
	{
		return true;
	}
	return RuneMatch == ERpgSkillTreeRuneMatch::AllRunes
		? SocketedRunes.HasAllExact(RequiredRunes)
		: SocketedRunes.HasAnyExact(RequiredRunes);
}

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

FName URpgSkillTreeDefinition::GetMilestoneClaimId(const int32 MasteryLevel) const
{
	return FName(*FString::Printf(TEXT("%s/Mastery/%d"), *TreeTag.ToString(), MasteryLevel));
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

const FRpgSkillTreeMapRegion* URpgSkillTreeDefinition::FindRegion(const FName RegionId) const
{
	if (RegionId.IsNone())
	{
		return nullptr;
	}

	return Regions.FindByPredicate([RegionId](const FRpgSkillTreeMapRegion& Region)
	{
		return Region.RegionId == RegionId;
	});
}

FVector2D URpgSkillTreeDefinition::GetNodeMapPosition(const FRpgSkillTreeNode& Node) const
{
	if (LayoutMode == ERpgSkillTreeLayoutMode::FreePosition)
	{
		return MapOrigin + Node.MapPosition;
	}
	return MapOrigin + FVector2D(Node.Column * GridSpacing.X, Node.Row * GridSpacing.Y);
}

FBox2D URpgSkillTreeDefinition::GetRegionBounds(const FName RegionId) const
{
	const FRpgSkillTreeMapRegion* Region = FindRegion(RegionId);
	FBox2D Bounds(ForceInit);
	if (!Region)
	{
		return Bounds;
	}

	for (const FRpgSkillTreeNode& Node : Nodes)
	{
		if (Node.NodeTag.IsValid() && Node.RegionId == RegionId)
		{
			Bounds += GetNodeMapPosition(Node);
		}
	}
	return Bounds.bIsValid ? Bounds.ExpandBy(Region->Padding) : Bounds;
}

FBox2D URpgSkillTreeDefinition::GetMapBounds() const
{
	FBox2D Bounds(ForceInit);
	for (const FRpgSkillTreeNode& Node : Nodes)
	{
		if (Node.NodeTag.IsValid())
		{
			Bounds += GetNodeMapPosition(Node);
		}
	}
	for (const FRpgSkillTreeMapRegion& Region : Regions)
	{
		if (const FBox2D RegionBounds = GetRegionBounds(Region.RegionId); RegionBounds.bIsValid)
		{
			Bounds += RegionBounds;
		}
	}
	return Bounds;
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

	const bool bFreePosition = LayoutMode == ERpgSkillTreeLayoutMode::FreePosition;
	if (!bFreePosition && (!(GridSpacing.X > 0.0) || !(GridSpacing.Y > 0.0)))
	{
		OutErrors.Add(LOCTEXT("InvalidGridSpacing", "GridSpacing must be positive in both directions."));
	}
	if (!FMath::IsFinite(MapOrigin.X) || !FMath::IsFinite(MapOrigin.Y))
	{
		OutErrors.Add(LOCTEXT("InvalidMapOrigin", "MapOrigin must be finite."));
	}

	TSet<int32> MilestoneLevels;
	for (const FRpgSkillTreeMasteryMilestone& Milestone : MasteryMilestones)
	{
		if (Milestone.MasteryLevel < 2 || MilestoneLevels.Contains(Milestone.MasteryLevel))
		{
			OutErrors.Add(FText::Format(
				LOCTEXT("InvalidMilestoneLevel", "Mastery milestone level {0} must be 2 or higher and unique."),
				Milestone.MasteryLevel));
		}
		MilestoneLevels.Add(Milestone.MasteryLevel);
		if (!Milestone.Reward || !Milestone.Reward->HasValidConfiguration())
		{
			OutErrors.Add(FText::Format(
				LOCTEXT("InvalidMilestoneReward", "Mastery milestone level {0} needs a valid reward."),
				Milestone.MasteryLevel));
		}
	}

	TSet<FName> RegionIds;
	for (int32 RegionIndex = 0; RegionIndex < Regions.Num(); ++RegionIndex)
	{
		const FRpgSkillTreeMapRegion& Region = Regions[RegionIndex];
		if (Region.RegionId.IsNone())
		{
			OutErrors.Add(FText::Format(LOCTEXT("MissingRegionId", "Regions[{0}] has no RegionId."), RegionIndex));
			continue;
		}
		if (RegionIds.Contains(Region.RegionId))
		{
			OutErrors.Add(FText::Format(
				LOCTEXT("DuplicateRegionId", "Region {0} appears more than once."),
				FText::FromName(Region.RegionId)));
			continue;
		}
		RegionIds.Add(Region.RegionId);

		if (!FMath::IsFinite(Region.Padding.X) || !FMath::IsFinite(Region.Padding.Y) ||
			Region.Padding.X < 0.0 || Region.Padding.Y < 0.0)
		{
			OutErrors.Add(FText::Format(
				LOCTEXT("InvalidRegionPadding", "Region {0} needs a finite, non-negative Padding."),
				FText::FromName(Region.RegionId)));
		}
		if (!Nodes.ContainsByPredicate([&Region](const FRpgSkillTreeNode& Node) { return Node.RegionId == Region.RegionId; }))
		{
			OutErrors.Add(FText::Format(
				LOCTEXT("EmptyRegion", "Region {0} has no node, so the map cannot frame it."),
				FText::FromName(Region.RegionId)));
		}
	}
	if (!DefaultRegionId.IsNone() && !RegionIds.Contains(DefaultRegionId))
	{
		OutErrors.Add(FText::Format(
			LOCTEXT("UnknownDefaultRegion", "DefaultRegionId {0} is not one of the tree's regions."),
			FText::FromName(DefaultRegionId)));
	}

	TMap<FGameplayTag, int32> NodeIndices;
	TSet<FIntPoint> Positions;
	TSet<FVector2D> MapPositions;
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

		if (bFreePosition)
		{
			if (!FMath::IsFinite(Node.MapPosition.X) || !FMath::IsFinite(Node.MapPosition.Y))
			{
				OutErrors.Add(FText::Format(
					LOCTEXT("InvalidMapPosition", "Node {0} needs a finite MapPosition."),
					FText::FromName(Node.NodeTag.GetTagName())));
			}
			else if (MapPositions.Contains(Node.MapPosition))
			{
				OutErrors.Add(FText::Format(
					LOCTEXT("DuplicateMapPosition", "Node {0} shares its MapPosition with another node."),
					FText::FromName(Node.NodeTag.GetTagName())));
			}
			MapPositions.Add(Node.MapPosition);
		}
		else
		{
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
		}

		if (!Node.RegionId.IsNone() && !RegionIds.Contains(Node.RegionId))
		{
			OutErrors.Add(FText::Format(
				LOCTEXT("UnknownNodeRegion", "Node {0} names region {1}, which is not one of the tree's regions."),
				FText::FromName(Node.NodeTag.GetTagName()),
				FText::FromName(Node.RegionId)));
		}

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
