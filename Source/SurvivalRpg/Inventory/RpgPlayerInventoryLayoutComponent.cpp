#include "RpgPlayerInventoryLayoutComponent.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "RpgInventoryEquipmentPlacementPolicy.h"
#include "RpgInventoryFragment_ItemContainer.h"
#include "RpgInventoryFragment_ItemTraits.h"
#include "RpgInventoryFragment_SlotContainerProvider.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"
#include "RpgPlayerInventoryLayoutDefinition.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Player/RpgBasePlayerState.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPlayerInventoryLayoutComponent)

const FName URpgPlayerInventoryLayoutComponent::WeaponSlot1GroupId(TEXT("WeaponSlot1"));
const FName URpgPlayerInventoryLayoutComponent::WeaponSlot2GroupId(TEXT("WeaponSlot2"));
const FName URpgPlayerInventoryLayoutComponent::ShieldSlotGroupId(TEXT("ShieldSlot"));
const FName URpgPlayerInventoryLayoutComponent::PocketsGroupId(TEXT("Pockets"));
const FName URpgPlayerInventoryLayoutComponent::GearHeadGroupId(TEXT("Gear.Head"));
const FName URpgPlayerInventoryLayoutComponent::GearChestGroupId(TEXT("Gear.Chest"));
const FName URpgPlayerInventoryLayoutComponent::GearHandsGroupId(TEXT("Gear.Hands"));
const FName URpgPlayerInventoryLayoutComponent::GearLegsGroupId(TEXT("Gear.Legs"));
const FName URpgPlayerInventoryLayoutComponent::GearFeetGroupId(TEXT("Gear.Feet"));
const FName URpgPlayerInventoryLayoutComponent::GearBackpackGroupId(TEXT("Gear.Backpack"));
const FName URpgPlayerInventoryLayoutComponent::GearBeltGroupId(TEXT("Gear.Belt"));
const FName URpgPlayerInventoryLayoutComponent::GearPouchGroupId(TEXT("Gear.Pouch"));
const FName URpgPlayerInventoryLayoutComponent::GearResourceBagGroupId(TEXT("Gear.ResourceBag"));

namespace
{
	bool DoesEquipmentSlotRoleMatchGroupKind(
		ERpgInventorySlotGroupKind GroupKind,
		ERpgEquipmentSlot EquipmentSlotRole)
	{
		switch (GroupKind)
		{
		case ERpgInventorySlotGroupKind::Content:
			return EquipmentSlotRole == ERpgEquipmentSlot::None;

		case ERpgInventorySlotGroupKind::Carry:
			return FRpgInventoryEquipmentPlacementPolicy::IsHandEquipmentSlot(
				EquipmentSlotRole);

		case ERpgInventorySlotGroupKind::Gear:
			return FRpgInventoryEquipmentPlacementPolicy::IsManagedEquipmentSlot(
					EquipmentSlotRole) &&
				!FRpgInventoryEquipmentPlacementPolicy::IsHandEquipmentSlot(
					EquipmentSlotRole);

		default:
			return false;
		}
	}

	bool IsValidGroupEquipmentContract(
		ERpgInventorySlotGroupKind GroupKind,
		ERpgEquipmentSlot EquipmentSlotRole,
		const FRpgInventoryGridSize& GridSize)
	{
		return DoesEquipmentSlotRoleMatchGroupKind(
				GroupKind,
				EquipmentSlotRole) &&
			(GroupKind == ERpgInventorySlotGroupKind::Content ||
				(GridSize.Width == 1 && GridSize.Height == 1));
	}

	bool TryFindUniqueGearGroup(
		const TArray<FRpgInventorySlotGroupView>& Groups,
		ERpgEquipmentSlot EquipmentSlot,
		FRpgInventorySlotGroupView& OutGroup)
	{
		OutGroup = FRpgInventorySlotGroupView();
		if (!FRpgInventoryEquipmentPlacementPolicy::IsManagedEquipmentSlot(
				EquipmentSlot) ||
			FRpgInventoryEquipmentPlacementPolicy::IsHandEquipmentSlot(
				EquipmentSlot))
		{
			return false;
		}

		bool bFound = false;
		for (const FRpgInventorySlotGroupView& Group : Groups)
		{
			if (Group.GroupKind != ERpgInventorySlotGroupKind::Gear ||
				Group.EquipmentSlotRole != EquipmentSlot ||
				!Group.ContainerHandle.IsRoot() ||
				!IsValidGroupEquipmentContract(
					Group.GroupKind,
					Group.EquipmentSlotRole,
					Group.GridSize))
			{
				continue;
			}

			if (bFound)
			{
				UE_LOG(
					LogTemp,
					Error,
					TEXT("Inventory Gear role '%s' is not unique in the active layout."),
					*UEnum::GetValueAsString(EquipmentSlot));
				OutGroup = FRpgInventorySlotGroupView();
				return false;
			}

			OutGroup = Group;
			bFound = true;
		}

		return bFound;
	}

	bool TryFindUniqueSemanticGroup(
		const TArray<FRpgInventorySlotGroupView>& Groups,
		FGameplayTag SemanticRole,
		FRpgInventorySlotGroupView& OutGroup,
		bool bLogAmbiguity)
	{
		OutGroup = FRpgInventorySlotGroupView();
		if (!SemanticRole.IsValid())
		{
			return false;
		}

		bool bFound = false;
		for (const FRpgInventorySlotGroupView& Group : Groups)
		{
			if (Group.SemanticRole != SemanticRole)
			{
				continue;
			}

			if (bFound || !Group.ContainerHandle.IsRoot())
			{
				if (bLogAmbiguity)
				{
					UE_LOG(
						LogTemp,
						Error,
						TEXT("Inventory layout semantic role '%s' is not a unique static root."),
						*SemanticRole.ToString());
				}
				OutGroup = FRpgInventorySlotGroupView();
				return false;
			}

			OutGroup = Group;
			bFound = true;
		}

		return bFound;
	}

	bool IsGroupActionbarBindable(
		const TArray<FRpgInventorySlotGroupView>& Groups,
		const FRpgInventorySlotGroupView& Group)
	{
		if (!Group.Rule.bActionbarBindable ||
			Group.ContainerHandle.Depth > 1 ||
			!IsValidGroupEquipmentContract(
				Group.GroupKind,
				Group.EquipmentSlotRole,
				Group.GridSize))
		{
			return false;
		}

		if (Group.GroupKind == ERpgInventorySlotGroupKind::Carry)
		{
			FRpgInventorySlotGroupView UniqueSemanticGroup;
			return TryFindUniqueSemanticGroup(
					Groups,
					Group.SemanticRole,
					UniqueSemanticGroup,
					false) &&
				UniqueSemanticGroup.ContainerHandle == Group.ContainerHandle;
		}

		if (Group.GroupKind != ERpgInventorySlotGroupKind::Content)
		{
			return false;
		}

		const bool bDisallowedProvider =
			Group.bProvidedByEquipment &&
			Group.SourceEquipmentSlot != ERpgEquipmentSlot::Belt &&
			Group.SourceEquipmentSlot != ERpgEquipmentSlot::Pouch;
		return !bDisallowedProvider;
	}
}

URpgPlayerInventoryLayoutComponent::URpgPlayerInventoryLayoutComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

const URpgPlayerInventoryLayoutDefinition* URpgPlayerInventoryLayoutComponent::GetLayoutDefinition() const
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	const ARpgBasePlayerState* PlayerState = OwnerController
		? OwnerController->GetPlayerState<ARpgBasePlayerState>()
		: nullptr;
	const URpgPawnData* PawnData = PlayerState
		? PlayerState->GetPawnData<URpgPawnData>()
		: nullptr;
	return PawnData ? PawnData->InventoryLayoutDefinition : nullptr;
}

void URpgPlayerInventoryLayoutComponent::RefreshLayoutFromPawnData()
{
	const AActor* OwnerActor = GetOwner();
	if (OwnerActor && OwnerActor->HasAuthority())
	{
		ApplyLayoutCapacityToInventory();
		return;
	}

	BroadcastLayoutChanged();
}

TArray<FRpgInventorySlotGroupView> URpgPlayerInventoryLayoutComponent::GetSlotGroups() const
{
	return BuildSlotGroups();
}

bool URpgPlayerInventoryLayoutComponent::TryGetSlotGroupBySemanticRole(
	FGameplayTag SemanticRole,
	FRpgInventorySlotGroupView& OutGroup) const
{
	return TryFindUniqueSemanticGroup(
		BuildSlotGroups(),
		SemanticRole,
		OutGroup,
		true);
}

bool URpgPlayerInventoryLayoutComponent::TryGetSemanticRoleForAddress(
	const FRpgInventorySlotAddress& Address,
	FGameplayTag& OutSemanticRole) const
{
	OutSemanticRole = FGameplayTag();
	if (!Address.IsValid())
	{
		return false;
	}

	const TArray<FRpgInventorySlotGroupView> Groups = BuildSlotGroups();
	for (const FRpgInventorySlotGroupView& Group : Groups)
	{
		if (Group.ContainerHandle != Address.GetContainerHandle() ||
			!Group.ContainsCell(Address.X, Address.Y) ||
			!Group.SemanticRole.IsValid())
		{
			continue;
		}

		FRpgInventorySlotGroupView UniqueGroup;
		if (!TryFindUniqueSemanticGroup(
				Groups,
				Group.SemanticRole,
				UniqueGroup,
				true) ||
			UniqueGroup.ContainerHandle != Group.ContainerHandle)
		{
			return false;
		}

		OutSemanticRole = Group.SemanticRole;
		return true;
	}

	return false;
}

bool URpgPlayerInventoryLayoutComponent::TryMakeSlotAddressForSemanticRole(
	FGameplayTag SemanticRole,
	FRpgInventorySlotAddress& OutAddress) const
{
	OutAddress = FRpgInventorySlotAddress();
	FRpgInventorySlotGroupView Group;
	if (!TryGetSlotGroupBySemanticRole(SemanticRole, Group) ||
		!Group.ContainsCell(0, 0))
	{
		return false;
	}

	OutAddress = Group.MakeAddress(0, 0);
	return true;
}

int32 URpgPlayerInventoryLayoutComponent::GetTotalCellCount() const
{
	int32 TotalCellCount = 0;
	for (const FRpgInventorySlotGroupView& Group : BuildSlotGroups())
	{
		TotalCellCount += FMath::Max(0, Group.GridSize.Width) * FMath::Max(0, Group.GridSize.Height);
	}
	return TotalCellCount;
}

bool URpgPlayerInventoryLayoutComponent::ResolveSlotAddress(const FRpgInventorySlotAddress& Address, FRpgInventoryGridPlacement& OutPlacement) const
{
	OutPlacement = FRpgInventoryGridPlacement();
	if (!Address.IsValid())
	{
		return false;
	}

	for (const FRpgInventorySlotGroupView& Group : BuildSlotGroups())
	{
		if (Group.ContainerHandle == Address.GetContainerHandle() && Group.ContainsCell(Address.X, Address.Y))
		{
			OutPlacement.SetContainerHandle(Address.GetContainerHandle());
			OutPlacement.X = Address.X;
			OutPlacement.Y = Address.Y;
			OutPlacement.Width = 1;
			OutPlacement.Height = 1;
			OutPlacement.bRotated = false;
			return true;
		}
	}

	return false;
}

bool URpgPlayerInventoryLayoutComponent::TryMakeSlotAddressFromPlacement(const FRpgInventoryGridPlacement& Placement, FRpgInventorySlotAddress& OutAddress) const
{
	OutAddress = FRpgInventorySlotAddress();
	if (!Placement.IsValid())
	{
		return false;
	}

	for (const FRpgInventorySlotGroupView& Group : BuildSlotGroups())
	{
		if (Group.ContainerHandle == Placement.GetContainerHandle() && Group.ContainsCell(Placement.X, Placement.Y))
		{
			OutAddress.SetContainerHandle(Placement.GetContainerHandle());
			OutAddress.X = Placement.X;
			OutAddress.Y = Placement.Y;
			return true;
		}
	}

	return false;
}

bool URpgPlayerInventoryLayoutComponent::GetGridSizeForContainerHandle(FRpgInventoryContainerHandle ContainerHandle, FRpgInventoryGridSize& OutGridSize) const
{
	OutGridSize = FRpgInventoryGridSize();
	if (!ContainerHandle.IsValid())
	{
		return false;
	}

	for (const FRpgInventorySlotGroupView& Group : BuildSlotGroups())
	{
		if (Group.ContainerHandle == ContainerHandle && Group.GridSize.IsValid())
		{
			OutGridSize = Group.GridSize;
			return true;
		}
	}

	return false;
}

URpgInventoryItemInstance* URpgPlayerInventoryLayoutComponent::GetItemInSlotAddress(const FRpgInventorySlotAddress& Address) const
{
	URpgInventoryManagerComponent* Inventory = FindPlayerInventory();
	FRpgInventoryGridPlacement Placement;
	return Inventory && ResolveSlotAddress(Address, Placement) ? Inventory->GetItemAtContainerCell(Placement.GetContainerHandle(), Placement.X, Placement.Y) : nullptr;
}

bool URpgPlayerInventoryLayoutComponent::CanItemUseSlotAddress(URpgInventoryItemInstance* Item, const FRpgInventorySlotAddress& Address) const
{
	if (!Item || !Address.IsValid())
	{
		return false;
	}

	const TArray<FRpgInventorySlotGroupView> Groups = BuildSlotGroups();
	for (const FRpgInventorySlotGroupView& Group : Groups)
	{
		if (Group.ContainerHandle == Address.GetContainerHandle() && Group.ContainsCell(Address.X, Address.Y))
		{
			if (!Group.Rule.AllowsItem(Item) ||
				!IsValidGroupEquipmentContract(
					Group.GroupKind,
					Group.EquipmentSlotRole,
					Group.GridSize))
			{
				return false;
			}

			if (Group.GroupKind == ERpgInventorySlotGroupKind::Content)
			{
				return true;
			}

			ERpgEquipmentSlot EquipmentSlot = Group.EquipmentSlotRole;
			if (Group.GroupKind == ERpgInventorySlotGroupKind::Gear)
			{
				FRpgInventorySlotGroupView UniqueGearGroup;
				if (!TryFindUniqueGearGroup(
						Groups,
						EquipmentSlot,
						UniqueGearGroup) ||
					UniqueGearGroup.ContainerHandle != Group.ContainerHandle)
				{
					return false;
				}
			}

			return FRpgInventoryEquipmentPlacementPolicy::CanItemUseEquipmentSlot(Item, EquipmentSlot);
		}
	}

	return false;
}

bool URpgPlayerInventoryLayoutComponent::IsSlotAddressActionbarBindable(const FRpgInventorySlotAddress& Address) const
{
	if (!Address.IsValid())
	{
		return false;
	}

	const TArray<FRpgInventorySlotGroupView> Groups = BuildSlotGroups();
	for (const FRpgInventorySlotGroupView& Group : Groups)
	{
		if (Group.ContainerHandle == Address.GetContainerHandle() && Group.ContainsCell(Address.X, Address.Y))
		{
			return IsGroupActionbarBindable(Groups, Group);
		}
	}

	return false;
}

bool URpgPlayerInventoryLayoutComponent::CanBindSlotAddressToActionbar(const FRpgInventorySlotAddress& Address, const URpgInventoryItemInstance* Item) const
{
	if (!Address.IsValid() || !Item)
	{
		return false;
	}

	const TArray<FRpgInventorySlotGroupView> Groups = BuildSlotGroups();
	for (const FRpgInventorySlotGroupView& Group : Groups)
	{
		if (Group.ContainerHandle != Address.GetContainerHandle() || !Group.ContainsCell(Address.X, Address.Y))
		{
			continue;
		}

		if (!IsGroupActionbarBindable(Groups, Group) ||
			!Group.Rule.AllowsItem(Item))
		{
			return false;
		}

		if (Group.GroupKind == ERpgInventorySlotGroupKind::Carry)
		{
			return true;
		}

		const URpgInventoryFragment_UsableItem* UsableFragment = Item->FindFragmentByClass<URpgInventoryFragment_UsableItem>();
		const URpgInventoryFragment_ItemTraits* Traits = Item->FindFragmentByClass<URpgInventoryFragment_ItemTraits>();
		if (!UsableFragment || !Traits)
		{
			return false;
		}

		switch (Traits->ItemCategory)
		{
		case ERpgInventoryItemCategory::Weapon:
		case ERpgInventoryItemCategory::Shield:
		case ERpgInventoryItemCategory::Armor:
		case ERpgInventoryItemCategory::Tool:
			return false;

		default:
			return true;
		}
	}

	return false;
}

bool URpgPlayerInventoryLayoutComponent::IsCarrySlotAddress(const FRpgInventorySlotAddress& Address) const
{
	if (!Address.IsValid())
	{
		return false;
	}

	for (const FRpgInventorySlotGroupView& Group : BuildSlotGroups())
	{
		if (Group.ContainerHandle == Address.GetContainerHandle() && Group.ContainsCell(Address.X, Address.Y))
		{
			return Group.GroupKind == ERpgInventorySlotGroupKind::Carry &&
				IsValidGroupEquipmentContract(
					Group.GroupKind,
					Group.EquipmentSlotRole,
					Group.GridSize);
		}
	}

	return false;
}

bool URpgPlayerInventoryLayoutComponent::TryGetEquipmentSlotRoleForAddress(
	const FRpgInventorySlotAddress& Address,
	ERpgEquipmentSlot& OutEquipmentSlot) const
{
	OutEquipmentSlot = ERpgEquipmentSlot::None;
	if (!Address.IsValid())
	{
		return false;
	}

	const TArray<FRpgInventorySlotGroupView> Groups = BuildSlotGroups();
	for (const FRpgInventorySlotGroupView& Group : Groups)
	{
		if (Group.ContainerHandle != Address.GetContainerHandle() ||
			!Group.ContainsCell(Address.X, Address.Y) ||
			!IsValidGroupEquipmentContract(
				Group.GroupKind,
				Group.EquipmentSlotRole,
				Group.GridSize) ||
			Group.GroupKind == ERpgInventorySlotGroupKind::Content)
		{
			continue;
		}

		if (Group.GroupKind == ERpgInventorySlotGroupKind::Gear)
		{
			FRpgInventorySlotGroupView UniqueGearGroup;
			if (!TryFindUniqueGearGroup(
					Groups,
					Group.EquipmentSlotRole,
					UniqueGearGroup) ||
				UniqueGearGroup.ContainerHandle != Group.ContainerHandle)
			{
				return false;
			}
		}

		OutEquipmentSlot = Group.EquipmentSlotRole;
		return true;
	}

	return false;
}

bool URpgPlayerInventoryLayoutComponent::IsGearSlotAddress(const FRpgInventorySlotAddress& Address) const
{
	ERpgEquipmentSlot EquipmentSlot = ERpgEquipmentSlot::None;
	return TryGetEquipmentSlotRoleForAddress(Address, EquipmentSlot) &&
		!FRpgInventoryEquipmentPlacementPolicy::IsHandEquipmentSlot(
			EquipmentSlot);
}

bool URpgPlayerInventoryLayoutComponent::IsContentSlotAddress(const FRpgInventorySlotAddress& Address) const
{
	if (!Address.IsValid())
	{
		return false;
	}

	for (const FRpgInventorySlotGroupView& Group : BuildSlotGroups())
	{
		if (Group.ContainerHandle == Address.GetContainerHandle() && Group.ContainsCell(Address.X, Address.Y))
		{
			return Group.GroupKind == ERpgInventorySlotGroupKind::Content &&
				Group.EquipmentSlotRole == ERpgEquipmentSlot::None;
		}
	}

	return false;
}

void URpgPlayerInventoryLayoutComponent::ApplyLayoutCapacityToInventory()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	if (!PlayerInventory)
	{
		return;
	}

	// Spatial player capacity is defined only by active grids and per-item stack limits.
	PlayerInventory->SetCapacityMode(ERpgInventoryCapacityMode::Unlimited);
	BroadcastLayoutChanged();
}

bool URpgPlayerInventoryLayoutComponent::IsSlotContainerEquipmentSlot(ERpgEquipmentSlot EquipmentSlot)
{
	return FRpgInventoryEquipmentPlacementPolicy::IsSlotContainerEquipmentSlot(EquipmentSlot);
}

bool URpgPlayerInventoryLayoutComponent::CanUnequipSlotContainer(ERpgEquipmentSlot EquipmentSlot) const
{
	(void)EquipmentSlot;
	// Contents are addressed by the concrete provider item id and therefore travel with the item.
	// Emptying a backpack before unequipping was a legacy global-grid constraint, not a gameplay rule.
	return true;
}

bool URpgPlayerInventoryLayoutComponent::IsGearContainer(
	FRpgInventoryContainerHandle ContainerHandle) const
{
	ERpgEquipmentSlot EquipmentSlot = ERpgEquipmentSlot::None;
	return TryGetEquipmentSlotForGearContainer(
		ContainerHandle,
		EquipmentSlot);
}

bool URpgPlayerInventoryLayoutComponent::
	HasValidStaticEquipmentRoleContract() const
{
	const URpgPlayerInventoryLayoutDefinition* LayoutDefinition =
		GetLayoutDefinition();
	if (!LayoutDefinition)
	{
		return false;
	}

	TSet<FName> ContainerIds;
	TSet<ERpgEquipmentSlot> GearRoles;
	for (const FRpgInventorySlotGroupDefinition& Group :
		LayoutDefinition->StaticSlotGroups)
	{
		if (Group.ContainerId.IsNone() ||
			!Group.GridSize.IsValid() ||
			!IsValidGroupEquipmentContract(
				Group.GroupKind,
				Group.EquipmentSlotRole,
				Group.GridSize) ||
			ContainerIds.Contains(Group.ContainerId))
		{
			return false;
		}
		ContainerIds.Add(Group.ContainerId);

		if (Group.GroupKind == ERpgInventorySlotGroupKind::Gear)
		{
			if (GearRoles.Contains(Group.EquipmentSlotRole))
			{
				return false;
			}
			GearRoles.Add(Group.EquipmentSlotRole);
		}
	}

	return true;
}

bool URpgPlayerInventoryLayoutComponent::TryMakeGearSlotAddress(
	ERpgEquipmentSlot EquipmentSlot,
	FRpgInventorySlotAddress& OutAddress) const
{
	OutAddress = FRpgInventorySlotAddress();
	FRpgInventorySlotGroupView GearGroup;
	if (!TryFindUniqueGearGroup(
			BuildSlotGroups(),
			EquipmentSlot,
			GearGroup) ||
		!GearGroup.ContainsCell(0, 0))
	{
		return false;
	}

	OutAddress = GearGroup.MakeAddress(0, 0);
	return true;
}

bool URpgPlayerInventoryLayoutComponent::TryGetEquipmentSlotForGearContainer(
	FRpgInventoryContainerHandle ContainerHandle,
	ERpgEquipmentSlot& OutEquipmentSlot) const
{
	OutEquipmentSlot = ERpgEquipmentSlot::None;
	if (!ContainerHandle.IsRoot())
	{
		return false;
	}

	const TArray<FRpgInventorySlotGroupView> Groups = BuildSlotGroups();
	const FRpgInventorySlotGroupView* MatchingGroup =
		Groups.FindByPredicate(
			[ContainerHandle](const FRpgInventorySlotGroupView& Group)
			{
				return Group.ContainerHandle == ContainerHandle &&
					Group.GroupKind == ERpgInventorySlotGroupKind::Gear;
			});
	if (!MatchingGroup)
	{
		return false;
	}

	FRpgInventorySlotGroupView UniqueGearGroup;
	if (!TryFindUniqueGearGroup(
			Groups,
			MatchingGroup->EquipmentSlotRole,
			UniqueGearGroup) ||
		UniqueGearGroup.ContainerHandle != ContainerHandle)
	{
		return false;
	}

	OutEquipmentSlot = MatchingGroup->EquipmentSlotRole;
	return true;
}

URpgInventoryManagerComponent* URpgPlayerInventoryLayoutComponent::FindPlayerInventory() const
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	const ARpgPlayerState* PlayerState = OwnerController ? OwnerController->GetPlayerState<ARpgPlayerState>() : nullptr;
	return PlayerState ? PlayerState->GetInventoryManagerComponent() : nullptr;
}

URpgEquipmentLoadoutComponent* URpgPlayerInventoryLayoutComponent::FindEquipmentLoadout() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<URpgEquipmentLoadoutComponent>() : nullptr;
}

TArray<FRpgInventorySlotGroupView> URpgPlayerInventoryLayoutComponent::BuildSlotGroups() const
{
	TArray<FRpgInventorySlotGroupView> Groups;

	const URpgPlayerInventoryLayoutDefinition* LayoutDefinition = GetLayoutDefinition();
	if (!LayoutDefinition)
	{
		return Groups;
	}

	AppendGroupViews(LayoutDefinition->StaticSlotGroups, false, ERpgEquipmentSlot::None, Groups);

	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	const TArray<FRpgInventorySlotGroupView> StaticGroups = Groups;
	for (const FRpgInventorySlotGroupView& StaticGroup : StaticGroups)
	{
		const ERpgEquipmentSlot ProviderSlot =
			StaticGroup.EquipmentSlotRole;
		if (StaticGroup.GroupKind != ERpgInventorySlotGroupKind::Gear ||
			!FRpgInventoryEquipmentPlacementPolicy::
				IsSlotContainerEquipmentSlot(ProviderSlot))
		{
			continue;
		}

		URpgInventoryItemInstance* ProviderItem = nullptr;
		FRpgInventorySlotGroupView GearGroup;
		if (PlayerInventory &&
			TryFindUniqueGearGroup(
				StaticGroups,
				ProviderSlot,
				GearGroup) &&
			GearGroup.ContainerHandle ==
				StaticGroup.ContainerHandle &&
			GearGroup.ContainsCell(0, 0))
		{
			ProviderItem = PlayerInventory->GetItemAtContainerCell(
				GearGroup.ContainerHandle,
				0,
				0);
		}

		if (!ProviderItem || !ProviderItem->FindFragmentByClass<URpgInventoryFragment_ItemContainer>())
		{
			continue;
		}

		AppendItemContainerViews(ProviderItem, ProviderSlot, Groups);
	}

	return Groups;
}

void URpgPlayerInventoryLayoutComponent::AppendGroupViews(
	const TArray<FRpgInventorySlotGroupDefinition>& GroupDefinitions,
	bool bProvidedByEquipment,
	ERpgEquipmentSlot SourceEquipmentSlot,
	TArray<FRpgInventorySlotGroupView>& OutGroups) const
{
	TSet<FName> SeenContainerIds;
	TSet<FName> DuplicateContainerIds;
	for (const FRpgInventorySlotGroupDefinition& GroupDefinition :
		GroupDefinitions)
	{
		if (GroupDefinition.ContainerId.IsNone())
		{
			continue;
		}

		if (SeenContainerIds.Contains(GroupDefinition.ContainerId))
		{
			DuplicateContainerIds.Add(GroupDefinition.ContainerId);
		}
		else
		{
			SeenContainerIds.Add(GroupDefinition.ContainerId);
		}
	}

	TSet<FName> ReportedDuplicateIds;
	for (const FRpgInventorySlotGroupDefinition& GroupDefinition : GroupDefinitions)
	{
		if (GroupDefinition.ContainerId.IsNone() || !GroupDefinition.GridSize.IsValid())
		{
			continue;
		}

		if (DuplicateContainerIds.Contains(GroupDefinition.ContainerId))
		{
			if (!ReportedDuplicateIds.Contains(GroupDefinition.ContainerId))
			{
				UE_LOG(
					LogTemp,
					Error,
					TEXT("Duplicate inventory container id '%s' was rejected while building the player layout."),
					*GroupDefinition.ContainerId.ToString());
				ReportedDuplicateIds.Add(GroupDefinition.ContainerId);
			}
			continue;
		}

		if (!IsValidGroupEquipmentContract(
				GroupDefinition.GroupKind,
				GroupDefinition.EquipmentSlotRole,
				GroupDefinition.GridSize))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("Inventory group '%s' has an invalid GroupKind, EquipmentSlotRole, or grid-size contract."),
				*GroupDefinition.ContainerId.ToString());
			continue;
		}

		FRpgInventorySlotGroupView& GroupView = OutGroups.AddDefaulted_GetRef();
		GroupView.ContainerHandle = FRpgInventoryContainerHandle::MakeRoot(GroupDefinition.ContainerId);
		GroupView.ContainerId = GroupDefinition.ContainerId;
		GroupView.SemanticRole = GroupDefinition.SemanticRole;
		GroupView.DisplayName = GroupDefinition.DisplayName;
		GroupView.Icon = GroupDefinition.Icon;
		GroupView.GroupKind = GroupDefinition.GroupKind;
		GroupView.EquipmentSlotRole =
			GroupDefinition.EquipmentSlotRole;
		GroupView.GridSize = GroupDefinition.GridSize;
		GroupView.Rule = GroupDefinition.Rule;
		GroupView.bProvidedByEquipment = bProvidedByEquipment;
		GroupView.SourceEquipmentSlot = SourceEquipmentSlot;
	}
}

void URpgPlayerInventoryLayoutComponent::AppendItemContainerViews(
	const URpgInventoryItemInstance* ProviderItem,
	ERpgEquipmentSlot SourceEquipmentSlot,
	TArray<FRpgInventorySlotGroupView>& OutGroups) const
{
	const URpgInventoryFragment_ItemContainer* ProviderFragment = ProviderItem
		? ProviderItem->FindFragmentByClass<URpgInventoryFragment_ItemContainer>()
		: nullptr;
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	if (!ProviderFragment || !PlayerInventory || !ProviderItem->GetItemId().IsValid())
	{
		return;
	}

	FRpgInventoryGridPlacement ProviderPlacement;
	if (!PlayerInventory->GetItemPlacement(const_cast<URpgInventoryItemInstance*>(ProviderItem), ProviderPlacement))
	{
		return;
	}

	const uint8 ChildDepth = ProviderPlacement.GetContainerHandle().GetDirectChildDepth();
	if (ChildDepth == 0)
	{
		return;
	}

	TArray<FRpgInventoryItemContainerDefinition> ContainerDefinitions;
	ProviderFragment->GetProvidedContainers(ContainerDefinitions);
	for (const FRpgInventoryItemContainerDefinition& Definition : ContainerDefinitions)
	{
		if (!Definition.IsValid())
		{
			continue;
		}

		const FRpgInventoryContainerHandle Handle = FRpgInventoryContainerHandle::MakeItemOwned(
			ProviderItem->GetItemId(), Definition.ContainerId, ChildDepth);
		if (OutGroups.ContainsByPredicate([&Handle](const FRpgInventorySlotGroupView& ExistingGroup)
			{
				return ExistingGroup.ContainerHandle == Handle;
			}))
		{
			continue;
		}

		FRpgInventorySlotGroupView& GroupView = OutGroups.AddDefaulted_GetRef();
		GroupView.ContainerHandle = Handle;
		GroupView.ContainerId = Definition.ContainerId;
		GroupView.DisplayName = Definition.DisplayName;
		GroupView.Icon = Definition.Icon;
		GroupView.GroupKind = ERpgInventorySlotGroupKind::Content;
		GroupView.EquipmentSlotRole = ERpgEquipmentSlot::None;
		GroupView.GridSize = Definition.GridSize;
		GroupView.Rule.AllowedCategories = Definition.AllowedCategories;
		GroupView.Rule.RequiredItemTags = Definition.RequiredItemTags;
		GroupView.Rule.BlockedItemTags = Definition.BlockedItemTags;
		GroupView.Rule.bActionbarBindable = Definition.bQuickAccessEligible;
		GroupView.bProvidedByEquipment = true;
		GroupView.SourceEquipmentSlot = SourceEquipmentSlot;
	}
}

void URpgPlayerInventoryLayoutComponent::BroadcastLayoutChanged() const
{
	if (!GetWorld())
	{
		return;
	}

	FRpgPlayerInventoryLayoutChangedMessage Message;
	Message.Owner = GetOwner();
	Message.LayoutComponent = const_cast<URpgPlayerInventoryLayoutComponent*>(this);
	Message.TotalCellCount = GetTotalCellCount();

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(GetWorld());
	MessageSubsystem.BroadcastMessage(RpgGameplayTags::Rpg_InventoryLayout_Message_Changed, Message);
}
