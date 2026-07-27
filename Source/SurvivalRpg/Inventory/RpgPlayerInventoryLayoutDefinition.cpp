#include "RpgPlayerInventoryLayoutDefinition.h"

#include "RpgInventoryEquipmentPlacementPolicy.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPlayerInventoryLayoutDefinition)

#define LOCTEXT_NAMESPACE "RpgPlayerInventoryLayoutDefinition"

namespace
{
	struct FStaticGroupEquipmentContractStatus
	{
		bool bInvalidGridSize = false;
		bool bInvalidGroupKind = false;
		bool bInvalidEquipmentSlotRole = false;
		bool bEquipmentGroupIsNotSingleCell = false;
		ERpgInventoryStaticLayoutValidationIssue EquipmentSlotRoleIssue =
			ERpgInventoryStaticLayoutValidationIssue::ContentHasEquipmentSlotRole;

		bool IsValid() const
		{
			return !bInvalidGridSize &&
				!bInvalidGroupKind &&
				!bInvalidEquipmentSlotRole &&
				!bEquipmentGroupIsNotSingleCell;
		}
	};

	FStaticGroupEquipmentContractStatus EvaluateStaticGroupEquipmentContract(
		ERpgInventorySlotGroupKind GroupKind,
		ERpgEquipmentSlot EquipmentSlotRole,
		const FRpgInventoryGridSize& GridSize)
	{
		FStaticGroupEquipmentContractStatus Status;
		Status.bInvalidGridSize = !GridSize.IsValid();

		switch (GroupKind)
		{
		case ERpgInventorySlotGroupKind::Content:
			Status.bInvalidEquipmentSlotRole =
				EquipmentSlotRole != ERpgEquipmentSlot::None;
			Status.EquipmentSlotRoleIssue =
				ERpgInventoryStaticLayoutValidationIssue::
					ContentHasEquipmentSlotRole;
			break;

		case ERpgInventorySlotGroupKind::Carry:
			Status.bInvalidEquipmentSlotRole =
				!FRpgInventoryEquipmentPlacementPolicy::
					IsHandEquipmentSlot(EquipmentSlotRole);
			Status.EquipmentSlotRoleIssue =
				ERpgInventoryStaticLayoutValidationIssue::
					CarryHasInvalidEquipmentSlotRole;
			break;

		case ERpgInventorySlotGroupKind::Gear:
			Status.bInvalidEquipmentSlotRole =
				!FRpgInventoryEquipmentPlacementPolicy::
					IsManagedEquipmentSlot(EquipmentSlotRole) ||
				FRpgInventoryEquipmentPlacementPolicy::
					IsHandEquipmentSlot(EquipmentSlotRole);
			Status.EquipmentSlotRoleIssue =
				ERpgInventoryStaticLayoutValidationIssue::
					GearHasInvalidEquipmentSlotRole;
			break;

		default:
			Status.bInvalidGroupKind = true;
			break;
		}

		Status.bEquipmentGroupIsNotSingleCell =
			!Status.bInvalidGridSize &&
			!Status.bInvalidGroupKind &&
			GroupKind != ERpgInventorySlotGroupKind::Content &&
			(GridSize.Width != 1 || GridSize.Height != 1);
		return Status;
	}

	void AddValidationIssue(
		FRpgInventoryStaticLayoutValidationResult& Result,
		ERpgInventoryStaticLayoutValidationIssue Type,
		int32 GroupIndex,
		int32 ConflictingGroupIndex = INDEX_NONE,
		bool bPreventsMaterialization = false)
	{
		FRpgInventoryStaticLayoutValidationResult::FIssue& Issue =
			Result.Issues.AddDefaulted_GetRef();
		Issue.Type = Type;
		Issue.GroupIndex = GroupIndex;
		Issue.ConflictingGroupIndex = ConflictingGroupIndex;

		if (bPreventsMaterialization)
		{
			Result.NonMaterializableGroupIndices.Add(GroupIndex);
			if (ConflictingGroupIndex != INDEX_NONE)
			{
				Result.NonMaterializableGroupIndices.Add(
					ConflictingGroupIndex);
			}
		}
	}

#if WITH_EDITOR
	FText GetEquipmentSlotText(ERpgEquipmentSlot EquipmentSlot)
	{
		return FText::FromString(UEnum::GetValueAsString(EquipmentSlot));
	}

	FText MakeValidationIssueText(
		const URpgPlayerInventoryLayoutDefinition& Layout,
		const FRpgInventoryStaticLayoutValidationResult::FIssue& Issue)
	{
		if (!Layout.StaticSlotGroups.IsValidIndex(Issue.GroupIndex))
		{
			return LOCTEXT(
				"InvalidInternalGroupIndex",
				"StaticSlotGroups contains an internal validation error with no valid group index.");
		}

		const FRpgInventorySlotGroupDefinition& Group =
			Layout.StaticSlotGroups[Issue.GroupIndex];
		const FText GroupIndexText = FText::AsNumber(Issue.GroupIndex);
		switch (Issue.Type)
		{
		case ERpgInventoryStaticLayoutValidationIssue::MissingContainerId:
			return FText::Format(
				LOCTEXT(
					"MissingContainerId",
					"StaticSlotGroups[{0}] has no ContainerId. Assign a stable, layout-unique root id."),
				GroupIndexText);

		case ERpgInventoryStaticLayoutValidationIssue::DuplicateContainerId:
			return FText::Format(
				LOCTEXT(
					"DuplicateContainerId",
					"StaticSlotGroups[{0}] reuses ContainerId '{1}' from StaticSlotGroups[{2}]. Root ContainerIds must be unique because they form persistent inventory handles."),
				GroupIndexText,
				FText::FromName(Group.ContainerId),
				FText::AsNumber(Issue.ConflictingGroupIndex));

		case ERpgInventoryStaticLayoutValidationIssue::InvalidGridSize:
			return FText::Format(
				LOCTEXT(
					"InvalidGridSize",
					"StaticSlotGroups[{0}] ('{1}') has invalid GridSize {2} x {3}. Width and height must both be at least one cell."),
				GroupIndexText,
				FText::FromName(Group.ContainerId),
				FText::AsNumber(Group.GridSize.Width),
				FText::AsNumber(Group.GridSize.Height));

		case ERpgInventoryStaticLayoutValidationIssue::InvalidGroupKind:
			return FText::Format(
				LOCTEXT(
					"InvalidGroupKind",
					"StaticSlotGroups[{0}] ('{1}') has an unknown GroupKind value ({2}). Select Content, Carry, or Gear."),
				GroupIndexText,
				FText::FromName(Group.ContainerId),
				FText::AsNumber(static_cast<uint8>(Group.GroupKind)));

		case ERpgInventoryStaticLayoutValidationIssue::
			ContentHasEquipmentSlotRole:
			return FText::Format(
				LOCTEXT(
					"ContentHasEquipmentSlotRole",
					"StaticSlotGroups[{0}] ('{1}') is Content but uses EquipmentSlotRole '{2}'. Content groups must use None."),
				GroupIndexText,
				FText::FromName(Group.ContainerId),
				GetEquipmentSlotText(Group.EquipmentSlotRole));

		case ERpgInventoryStaticLayoutValidationIssue::
			CarryHasInvalidEquipmentSlotRole:
			return FText::Format(
				LOCTEXT(
					"CarryHasInvalidEquipmentSlotRole",
					"StaticSlotGroups[{0}] ('{1}') is Carry but uses EquipmentSlotRole '{2}'. Carry groups must use MainHand or OffHand."),
				GroupIndexText,
				FText::FromName(Group.ContainerId),
				GetEquipmentSlotText(Group.EquipmentSlotRole));

		case ERpgInventoryStaticLayoutValidationIssue::
			GearHasInvalidEquipmentSlotRole:
			return FText::Format(
				LOCTEXT(
					"GearHasInvalidEquipmentSlotRole",
					"StaticSlotGroups[{0}] ('{1}') is Gear but uses EquipmentSlotRole '{2}'. Gear groups require one managed non-hand slot."),
				GroupIndexText,
				FText::FromName(Group.ContainerId),
				GetEquipmentSlotText(Group.EquipmentSlotRole));

		case ERpgInventoryStaticLayoutValidationIssue::
			EquipmentGroupIsNotSingleCell:
			return FText::Format(
				LOCTEXT(
					"EquipmentGroupIsNotSingleCell",
					"StaticSlotGroups[{0}] ('{1}') is a Gear or Carry group with GridSize {2} x {3}. Equipment groups must be exactly 1 x 1."),
				GroupIndexText,
				FText::FromName(Group.ContainerId),
				FText::AsNumber(Group.GridSize.Width),
				FText::AsNumber(Group.GridSize.Height));

		case ERpgInventoryStaticLayoutValidationIssue::
			MissingActionbarCarrySemanticRole:
			return FText::Format(
				LOCTEXT(
					"MissingActionbarCarrySemanticRole",
					"StaticSlotGroups[{0}] ('{1}') is an actionbar-bindable Carry group without a SemanticRole. Assign a unique Rpg.Inventory.Layout.Role.Carry tag."),
				GroupIndexText,
				FText::FromName(Group.ContainerId));

		case ERpgInventoryStaticLayoutValidationIssue::
			SemanticRoleOutsideLayoutNamespace:
			return FText::Format(
				LOCTEXT(
					"SemanticRoleOutsideLayoutNamespace",
					"StaticSlotGroups[{0}] ('{1}') uses SemanticRole '{2}' outside the Rpg.Inventory.Layout.Role namespace. Assign a concrete descendant of Rpg.Inventory.Layout.Role."),
				GroupIndexText,
				FText::FromName(Group.ContainerId),
				FText::FromString(Group.SemanticRole.ToString()));

		case ERpgInventoryStaticLayoutValidationIssue::DuplicateSemanticRole:
			return FText::Format(
				LOCTEXT(
					"DuplicateSemanticRole",
					"StaticSlotGroups[{0}] ('{1}') reuses SemanticRole '{2}' from StaticSlotGroups[{3}]. Non-empty semantic roles are layout-wide singleton keys."),
				GroupIndexText,
				FText::FromName(Group.ContainerId),
				FText::FromString(Group.SemanticRole.ToString()),
				FText::AsNumber(Issue.ConflictingGroupIndex));

		case ERpgInventoryStaticLayoutValidationIssue::
			DuplicateGearEquipmentSlotRole:
			return FText::Format(
				LOCTEXT(
					"DuplicateGearEquipmentSlotRole",
					"StaticSlotGroups[{0}] ('{1}') reuses Gear EquipmentSlotRole '{2}' from StaticSlotGroups[{3}]. Each non-hand Gear role must resolve to exactly one static root."),
				GroupIndexText,
				FText::FromName(Group.ContainerId),
				GetEquipmentSlotText(Group.EquipmentSlotRole),
				FText::AsNumber(Issue.ConflictingGroupIndex));
		}

		return FText::Format(
			LOCTEXT(
				"UnknownValidationIssue",
				"StaticSlotGroups[{0}] contains an unknown layout validation failure."),
			GroupIndexText);
	}
#endif
}

bool FRpgInventoryStaticLayoutValidationResult::HasIssue(
	ERpgInventoryStaticLayoutValidationIssue Type,
	int32 GroupIndex) const
{
	return Issues.ContainsByPredicate(
		[Type, GroupIndex](const FIssue& Issue)
		{
			return Issue.Type == Type &&
				(Issue.GroupIndex == GroupIndex ||
					Issue.ConflictingGroupIndex == GroupIndex);
		});
}

bool FRpgInventoryStaticLayoutValidationResult::
	PassesPhysicalEquipmentPreflight() const
{
	for (const FIssue& Issue : Issues)
	{
		switch (Issue.Type)
		{
		case ERpgInventoryStaticLayoutValidationIssue::
			MissingActionbarCarrySemanticRole:
		case ERpgInventoryStaticLayoutValidationIssue::
			SemanticRoleOutsideLayoutNamespace:
		case ERpgInventoryStaticLayoutValidationIssue::
			DuplicateSemanticRole:
			continue;

		default:
			return false;
		}
	}
	return true;
}

FRpgInventoryStaticLayoutValidationResult
URpgPlayerInventoryLayoutDefinition::ValidateStaticSlotGroups() const
{
	FRpgInventoryStaticLayoutValidationResult Result;
	TMap<FName, int32> FirstContainerIndex;
	TMap<FGameplayTag, int32> FirstSemanticRoleIndex;
	TMap<ERpgEquipmentSlot, int32> FirstGearRoleIndex;

	for (int32 GroupIndex = 0;
		GroupIndex < StaticSlotGroups.Num();
		++GroupIndex)
	{
		const FRpgInventorySlotGroupDefinition& Group =
			StaticSlotGroups[GroupIndex];

		if (Group.ContainerId.IsNone())
		{
			AddValidationIssue(
				Result,
				ERpgInventoryStaticLayoutValidationIssue::MissingContainerId,
				GroupIndex,
				INDEX_NONE,
				true);
		}
		else if (const int32* ExistingIndex =
			FirstContainerIndex.Find(Group.ContainerId))
		{
			AddValidationIssue(
				Result,
				ERpgInventoryStaticLayoutValidationIssue::DuplicateContainerId,
				GroupIndex,
				*ExistingIndex,
				true);
		}
		else
		{
			FirstContainerIndex.Add(Group.ContainerId, GroupIndex);
		}

		const FStaticGroupEquipmentContractStatus ContractStatus =
			EvaluateStaticGroupEquipmentContract(
				Group.GroupKind,
				Group.EquipmentSlotRole,
				Group.GridSize);
		if (ContractStatus.bInvalidGridSize)
		{
			AddValidationIssue(
				Result,
				ERpgInventoryStaticLayoutValidationIssue::InvalidGridSize,
				GroupIndex,
				INDEX_NONE,
				true);
		}
		if (ContractStatus.bInvalidGroupKind)
		{
			AddValidationIssue(
				Result,
				ERpgInventoryStaticLayoutValidationIssue::InvalidGroupKind,
				GroupIndex,
				INDEX_NONE,
				true);
		}
		if (ContractStatus.bInvalidEquipmentSlotRole)
		{
			AddValidationIssue(
				Result,
				ContractStatus.EquipmentSlotRoleIssue,
				GroupIndex,
				INDEX_NONE,
				true);
		}
		if (ContractStatus.bEquipmentGroupIsNotSingleCell)
		{
			AddValidationIssue(
				Result,
				ERpgInventoryStaticLayoutValidationIssue::
					EquipmentGroupIsNotSingleCell,
				GroupIndex,
				INDEX_NONE,
				true);
		}

		if (Group.GroupKind == ERpgInventorySlotGroupKind::Carry &&
			Group.Rule.bActionbarBindable &&
			!Group.SemanticRole.IsValid())
		{
			AddValidationIssue(
				Result,
				ERpgInventoryStaticLayoutValidationIssue::
					MissingActionbarCarrySemanticRole,
				GroupIndex);
		}

		if (Group.SemanticRole.IsValid())
		{
			if (!IsConcreteLayoutSemanticRole(Group.SemanticRole))
			{
				AddValidationIssue(
					Result,
					ERpgInventoryStaticLayoutValidationIssue::
						SemanticRoleOutsideLayoutNamespace,
					GroupIndex);
			}

			if (const int32* ExistingIndex =
				FirstSemanticRoleIndex.Find(Group.SemanticRole))
			{
				AddValidationIssue(
					Result,
					ERpgInventoryStaticLayoutValidationIssue::
						DuplicateSemanticRole,
					GroupIndex,
					*ExistingIndex);
			}
			else
			{
				FirstSemanticRoleIndex.Add(
					Group.SemanticRole,
					GroupIndex);
			}
		}

		const bool bHasValidGearRole =
			Group.GroupKind == ERpgInventorySlotGroupKind::Gear &&
			!ContractStatus.bInvalidGroupKind &&
			!ContractStatus.bInvalidEquipmentSlotRole;
		if (bHasValidGearRole)
		{
			if (const int32* ExistingIndex =
				FirstGearRoleIndex.Find(Group.EquipmentSlotRole))
			{
				AddValidationIssue(
					Result,
					ERpgInventoryStaticLayoutValidationIssue::
						DuplicateGearEquipmentSlotRole,
					GroupIndex,
					*ExistingIndex);
			}
			else
			{
				FirstGearRoleIndex.Add(
					Group.EquipmentSlotRole,
					GroupIndex);
			}
		}
	}

	return Result;
}

bool URpgPlayerInventoryLayoutDefinition::IsConcreteLayoutSemanticRole(
	const FGameplayTag SemanticRole)
{
	static const FGameplayTag RootTag =
		FGameplayTag::RequestGameplayTag(
			TEXT("Rpg.Inventory.Layout.Role"),
			/*ErrorIfNotFound=*/ false);
	return SemanticRole.IsValid() &&
		RootTag.IsValid() &&
		SemanticRole != RootTag &&
		SemanticRole.MatchesTag(RootTag);
}

bool URpgPlayerInventoryLayoutDefinition::
	IsStaticGroupEquipmentContractValid(
		ERpgInventorySlotGroupKind GroupKind,
		ERpgEquipmentSlot EquipmentSlotRole,
		const FRpgInventoryGridSize& GridSize)
{
	return EvaluateStaticGroupEquipmentContract(
		GroupKind,
		EquipmentSlotRole,
		GridSize).IsValid();
}

#if WITH_EDITOR
EDataValidationResult URpgPlayerInventoryLayoutDefinition::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(
		Super::IsDataValid(Context),
		EDataValidationResult::Valid);
	const FRpgInventoryStaticLayoutValidationResult Validation =
		ValidateStaticSlotGroups();
	for (const FRpgInventoryStaticLayoutValidationResult::FIssue& Issue :
		Validation.Issues)
	{
		Context.AddError(MakeValidationIssueText(*this, Issue));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
