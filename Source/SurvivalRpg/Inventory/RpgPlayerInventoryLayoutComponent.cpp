#include "RpgPlayerInventoryLayoutComponent.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "RpgInventoryFragment_EquippableItem.h"
#include "RpgInventoryFragment_ItemContainer.h"
#include "RpgInventoryFragment_ItemTraits.h"
#include "RpgInventoryFragment_SlotContainerProvider.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"
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
	bool CanItemEquipInLayoutGearSlot(const URpgInventoryItemInstance* Item, ERpgEquipmentSlot EquipmentSlot)
	{
		if (!Item || EquipmentSlot == ERpgEquipmentSlot::None)
		{
			return false;
		}

		const URpgInventoryFragment_EquippableItem* EquippableFragment = Item->FindFragmentByClass<URpgInventoryFragment_EquippableItem>();
		const TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition = EquippableFragment ? EquippableFragment->GetEquipmentDefinition() : nullptr;
		const URpgEquipmentDefinition* EquipmentCDO = EquipmentDefinition ? GetDefault<URpgEquipmentDefinition>(EquipmentDefinition) : nullptr;
		if (URpgPlayerInventoryLayoutComponent::IsSlotContainerEquipmentSlot(EquipmentSlot))
		{
			if (!Item->FindFragmentByClass<URpgInventoryFragment_ItemContainer>())
			{
				return false;
			}

			// Equipment data distinguishes backpacks, belts, pouches, and resource bags. Definition-less legacy
			// providers keep their migration compatibility until their item definitions are updated.
			return !EquipmentCDO || EquipmentCDO->CanEquipInSlot(EquipmentSlot);
		}

		return EquipmentCDO && EquipmentCDO->CanEquipInSlot(EquipmentSlot);
	}
}

URpgPlayerInventoryLayoutComponent::URpgPlayerInventoryLayoutComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);

	StaticSlotGroups =
	{
		MakeStaticGroup(GearHeadGroupId, NSLOCTEXT("RpgInventoryLayout", "GearHead", "Head"), 1, 1, { ERpgInventoryItemCategory::Armor }, false, false, ERpgInventorySlotGroupKind::Gear),
		MakeStaticGroup(GearChestGroupId, NSLOCTEXT("RpgInventoryLayout", "GearChest", "Chest"), 1, 1, { ERpgInventoryItemCategory::Armor }, false, false, ERpgInventorySlotGroupKind::Gear),
		MakeStaticGroup(GearHandsGroupId, NSLOCTEXT("RpgInventoryLayout", "GearHands", "Hands"), 1, 1, { ERpgInventoryItemCategory::Armor }, false, false, ERpgInventorySlotGroupKind::Gear),
		MakeStaticGroup(GearLegsGroupId, NSLOCTEXT("RpgInventoryLayout", "GearLegs", "Legs"), 1, 1, { ERpgInventoryItemCategory::Armor }, false, false, ERpgInventorySlotGroupKind::Gear),
		MakeStaticGroup(GearFeetGroupId, NSLOCTEXT("RpgInventoryLayout", "GearFeet", "Feet"), 1, 1, { ERpgInventoryItemCategory::Armor }, false, false, ERpgInventorySlotGroupKind::Gear),
		MakeStaticGroup(GearBackpackGroupId, NSLOCTEXT("RpgInventoryLayout", "GearBackpack", "Backpack"), 1, 1, {}, false, false, ERpgInventorySlotGroupKind::Gear),
		MakeStaticGroup(GearBeltGroupId, NSLOCTEXT("RpgInventoryLayout", "GearBelt", "Belt"), 1, 1, {}, false, false, ERpgInventorySlotGroupKind::Gear),
		MakeStaticGroup(GearPouchGroupId, NSLOCTEXT("RpgInventoryLayout", "GearPouch", "Pouch"), 1, 1, {}, false, false, ERpgInventorySlotGroupKind::Gear),
		MakeStaticGroup(GearResourceBagGroupId, NSLOCTEXT("RpgInventoryLayout", "GearResourceBag", "Resource Bag"), 1, 1, {}, false, false, ERpgInventorySlotGroupKind::Gear),
		MakeStaticGroup(WeaponSlot1GroupId, NSLOCTEXT("RpgInventoryLayout", "WeaponSlot1", "Weapon 1"), 1, 1, { ERpgInventoryItemCategory::Weapon }, true, true, ERpgInventorySlotGroupKind::Carry, RpgGameplayTags::Equipment_Slot_MainHand),
		MakeStaticGroup(WeaponSlot2GroupId, NSLOCTEXT("RpgInventoryLayout", "WeaponSlot2", "Weapon 2"), 1, 1, { ERpgInventoryItemCategory::Weapon }, true, true, ERpgInventorySlotGroupKind::Carry, RpgGameplayTags::Equipment_Slot_MainHand),
		MakeStaticGroup(ShieldSlotGroupId, NSLOCTEXT("RpgInventoryLayout", "ShieldSlot", "Shield"), 1, 1, { ERpgInventoryItemCategory::Shield }, true, true, ERpgInventorySlotGroupKind::Carry, RpgGameplayTags::Equipment_Slot_OffHand),
		MakeStaticGroup(PocketsGroupId, NSLOCTEXT("RpgInventoryLayout", "Pockets", "Pockets"), 4, 2, {}, true, false, ERpgInventorySlotGroupKind::Content)
	};
}

void URpgPlayerInventoryLayoutComponent::BeginPlay()
{
	Super::BeginPlay();

	ApplyLayoutCapacityToInventory();
}

TArray<FRpgInventorySlotGroupView> URpgPlayerInventoryLayoutComponent::GetSlotGroups() const
{
	return BuildSlotGroups();
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

bool URpgPlayerInventoryLayoutComponent::GetGridSizeForContainer(FName ContainerId, FRpgInventoryGridSize& OutGridSize) const
{
	OutGridSize = FRpgInventoryGridSize();
	if (ContainerId.IsNone())
	{
		return false;
	}

	for (const FRpgInventorySlotGroupView& Group : BuildSlotGroups())
	{
		if (Group.ContainerId == ContainerId && Group.GridSize.IsValid())
		{
			OutGridSize = Group.GridSize;
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

	for (const FRpgInventorySlotGroupView& Group : BuildSlotGroups())
	{
		if (Group.ContainerHandle == Address.GetContainerHandle() && Group.ContainsCell(Address.X, Address.Y))
		{
			if (!Group.Rule.AllowsItem(Item))
			{
				return false;
			}

			if (Group.GroupKind == ERpgInventorySlotGroupKind::Gear)
			{
				ERpgEquipmentSlot EquipmentSlot = ERpgEquipmentSlot::None;
				return TryGetEquipmentSlotForGearGroupId(Group.ContainerId, EquipmentSlot) &&
					CanItemEquipInLayoutGearSlot(Item, EquipmentSlot);
			}

			return true;
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

	for (const FRpgInventorySlotGroupView& Group : BuildSlotGroups())
	{
		if (Group.ContainerHandle == Address.GetContainerHandle() && Group.ContainsCell(Address.X, Address.Y))
		{
			const bool bDisallowedProvider = Group.bProvidedByEquipment &&
				Group.SourceEquipmentSlotName != FName(TEXT("Belt")) &&
				Group.SourceEquipmentSlotName != FName(TEXT("Pouch"));
			return Group.Rule.bActionbarBindable &&
				Group.ContainerHandle.Depth <= 1 &&
				(Group.GroupKind != ERpgInventorySlotGroupKind::Content || !bDisallowedProvider);
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

	for (const FRpgInventorySlotGroupView& Group : BuildSlotGroups())
	{
		if (Group.ContainerHandle != Address.GetContainerHandle() || !Group.ContainsCell(Address.X, Address.Y))
		{
			continue;
		}

		if (!Group.Rule.bActionbarBindable || !Group.Rule.AllowsItem(Item))
		{
			return false;
		}
		if (Group.ContainerHandle.Depth > 1)
		{
			return false;
		}

		if (Group.GroupKind == ERpgInventorySlotGroupKind::Carry && Group.Rule.bCarrySlot)
		{
			return true;
		}

		if (Group.GroupKind != ERpgInventorySlotGroupKind::Content)
		{
			return false;
		}
		if (Group.bProvidedByEquipment &&
			Group.SourceEquipmentSlotName != FName(TEXT("Belt")) &&
			Group.SourceEquipmentSlotName != FName(TEXT("Pouch")))
		{
			return false;
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
			return Group.GroupKind == ERpgInventorySlotGroupKind::Carry && Group.Rule.bCarrySlot;
		}
	}

	return false;
}

FGameplayTag URpgPlayerInventoryLayoutComponent::GetCarryActivationRole(const FRpgInventorySlotAddress& Address) const
{
	if (!Address.IsValid())
	{
		return FGameplayTag();
	}

	for (const FRpgInventorySlotGroupView& Group : BuildSlotGroups())
	{
		if (Group.ContainerHandle == Address.GetContainerHandle() && Group.ContainsCell(Address.X, Address.Y) &&
			Group.GroupKind == ERpgInventorySlotGroupKind::Carry && Group.Rule.bCarrySlot)
		{
			return Group.Rule.CarryActivationRole;
		}
	}

	return FGameplayTag();
}

bool URpgPlayerInventoryLayoutComponent::IsGearSlotAddress(const FRpgInventorySlotAddress& Address) const
{
	if (!Address.IsValid())
	{
		return false;
	}

	for (const FRpgInventorySlotGroupView& Group : BuildSlotGroups())
	{
		if (Group.ContainerHandle == Address.GetContainerHandle() && Group.ContainsCell(Address.X, Address.Y))
		{
			return Group.GroupKind == ERpgInventorySlotGroupKind::Gear;
		}
	}

	return false;
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
			return Group.GroupKind == ERpgInventorySlotGroupKind::Content;
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
	return EquipmentSlot == ERpgEquipmentSlot::Backpack ||
		EquipmentSlot == ERpgEquipmentSlot::Belt ||
		EquipmentSlot == ERpgEquipmentSlot::Pouch ||
		EquipmentSlot == ERpgEquipmentSlot::ResourceBag;
}

bool URpgPlayerInventoryLayoutComponent::CanUnequipSlotContainer(ERpgEquipmentSlot EquipmentSlot) const
{
	(void)EquipmentSlot;
	// Contents are addressed by the concrete provider item id and therefore travel with the item.
	// Emptying a backpack before unequipping was a legacy global-grid constraint, not a gameplay rule.
	return true;
}

bool URpgPlayerInventoryLayoutComponent::IsBuiltInCarryGroupId(FName GroupId)
{
	return GroupId == WeaponSlot1GroupId ||
		GroupId == WeaponSlot2GroupId ||
		GroupId == ShieldSlotGroupId;
}

bool URpgPlayerInventoryLayoutComponent::IsBuiltInGearGroupId(FName GroupId)
{
	return GroupId == GearHeadGroupId ||
		GroupId == GearChestGroupId ||
		GroupId == GearHandsGroupId ||
		GroupId == GearLegsGroupId ||
		GroupId == GearFeetGroupId ||
		GroupId == GearBackpackGroupId ||
		GroupId == GearBeltGroupId ||
		GroupId == GearPouchGroupId ||
		GroupId == GearResourceBagGroupId;
}

bool URpgPlayerInventoryLayoutComponent::TryMakeGearSlotAddress(ERpgEquipmentSlot EquipmentSlot, FRpgInventorySlotAddress& OutAddress)
{
	OutAddress = FRpgInventorySlotAddress();

	switch (EquipmentSlot)
	{
	case ERpgEquipmentSlot::Head:
		OutAddress.ContainerId = GearHeadGroupId;
		break;
	case ERpgEquipmentSlot::Chest:
		OutAddress.ContainerId = GearChestGroupId;
		break;
	case ERpgEquipmentSlot::Hands:
		OutAddress.ContainerId = GearHandsGroupId;
		break;
	case ERpgEquipmentSlot::Legs:
		OutAddress.ContainerId = GearLegsGroupId;
		break;
	case ERpgEquipmentSlot::Feet:
		OutAddress.ContainerId = GearFeetGroupId;
		break;
	case ERpgEquipmentSlot::Backpack:
		OutAddress.ContainerId = GearBackpackGroupId;
		break;
	case ERpgEquipmentSlot::Belt:
		OutAddress.ContainerId = GearBeltGroupId;
		break;
	case ERpgEquipmentSlot::Pouch:
		OutAddress.ContainerId = GearPouchGroupId;
		break;
	case ERpgEquipmentSlot::ResourceBag:
		OutAddress.ContainerId = GearResourceBagGroupId;
		break;
	default:
		return false;
	}

	OutAddress.X = 0;
	OutAddress.Y = 0;
	return true;
}

bool URpgPlayerInventoryLayoutComponent::TryGetEquipmentSlotForGearGroupId(FName GroupId, ERpgEquipmentSlot& OutEquipmentSlot)
{
	OutEquipmentSlot = ERpgEquipmentSlot::None;

	if (GroupId == GearHeadGroupId)
	{
		OutEquipmentSlot = ERpgEquipmentSlot::Head;
	}
	else if (GroupId == GearChestGroupId)
	{
		OutEquipmentSlot = ERpgEquipmentSlot::Chest;
	}
	else if (GroupId == GearHandsGroupId)
	{
		OutEquipmentSlot = ERpgEquipmentSlot::Hands;
	}
	else if (GroupId == GearLegsGroupId)
	{
		OutEquipmentSlot = ERpgEquipmentSlot::Legs;
	}
	else if (GroupId == GearFeetGroupId)
	{
		OutEquipmentSlot = ERpgEquipmentSlot::Feet;
	}
	else if (GroupId == GearBackpackGroupId)
	{
		OutEquipmentSlot = ERpgEquipmentSlot::Backpack;
	}
	else if (GroupId == GearBeltGroupId)
	{
		OutEquipmentSlot = ERpgEquipmentSlot::Belt;
	}
	else if (GroupId == GearPouchGroupId)
	{
		OutEquipmentSlot = ERpgEquipmentSlot::Pouch;
	}
	else if (GroupId == GearResourceBagGroupId)
	{
		OutEquipmentSlot = ERpgEquipmentSlot::ResourceBag;
	}

	return OutEquipmentSlot != ERpgEquipmentSlot::None;
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

	AppendGroupViews(StaticSlotGroups, false, ERpgEquipmentSlot::None, Groups);

	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	const ERpgEquipmentSlot ProviderSlots[] =
	{
		ERpgEquipmentSlot::Backpack,
		ERpgEquipmentSlot::Belt,
		ERpgEquipmentSlot::Pouch,
		ERpgEquipmentSlot::ResourceBag
	};

	for (const ERpgEquipmentSlot ProviderSlot : ProviderSlots)
	{
		URpgInventoryItemInstance* ProviderItem = nullptr;
		FRpgInventorySlotAddress GearAddress;
		if (PlayerInventory && TryMakeGearSlotAddress(ProviderSlot, GearAddress))
		{
			for (const FRpgInventorySlotGroupView& ExistingGroup : Groups)
			{
				if (ExistingGroup.ContainerHandle == GearAddress.GetContainerHandle() && ExistingGroup.ContainsCell(0, 0))
				{
					ProviderItem = PlayerInventory->GetItemAtContainerCell(ExistingGroup.ContainerHandle, 0, 0);
					break;
				}
			}
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
	for (const FRpgInventorySlotGroupDefinition& GroupDefinition : GroupDefinitions)
	{
		if (GroupDefinition.ContainerId.IsNone() || !GroupDefinition.GridSize.IsValid())
		{
			continue;
		}

		if (OutGroups.ContainsByPredicate([&GroupDefinition](const FRpgInventorySlotGroupView& ExistingGroup)
			{
				return ExistingGroup.ContainerId == GroupDefinition.ContainerId;
			}))
		{
			UE_LOG(LogTemp, Error, TEXT("Duplicate inventory container id '%s' was rejected while building the player layout."), *GroupDefinition.ContainerId.ToString());
			continue;
		}

		FRpgInventorySlotGroupView& GroupView = OutGroups.AddDefaulted_GetRef();
		GroupView.ContainerHandle = FRpgInventoryContainerHandle::MakeRoot(GroupDefinition.ContainerId);
		GroupView.ContainerId = GroupDefinition.ContainerId;
		GroupView.DisplayName = GroupDefinition.DisplayName;
		GroupView.Icon = GroupDefinition.Icon;
		GroupView.GroupKind = GroupDefinition.GroupKind;
		GroupView.GridSize = GroupDefinition.GridSize;
		GroupView.Rule = GroupDefinition.Rule;
		GroupView.bProvidedByEquipment = bProvidedByEquipment;
		GroupView.SourceEquipmentSlotName = EquipmentSlotToSourceName(SourceEquipmentSlot);
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
		GroupView.GridSize = Definition.GridSize;
		GroupView.Rule.AllowedCategories = Definition.AllowedCategories;
		GroupView.Rule.RequiredItemTags = Definition.RequiredItemTags;
		GroupView.Rule.BlockedItemTags = Definition.BlockedItemTags;
		GroupView.Rule.bActionbarBindable = Definition.bQuickAccessEligible;
		GroupView.bProvidedByEquipment = true;
		GroupView.SourceEquipmentSlotName = EquipmentSlotToSourceName(SourceEquipmentSlot);
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

FName URpgPlayerInventoryLayoutComponent::EquipmentSlotToSourceName(ERpgEquipmentSlot EquipmentSlot)
{
	switch (EquipmentSlot)
	{
	case ERpgEquipmentSlot::Backpack:
		return TEXT("Backpack");
	case ERpgEquipmentSlot::Belt:
		return TEXT("Belt");
	case ERpgEquipmentSlot::Pouch:
		return TEXT("Pouch");
	case ERpgEquipmentSlot::ResourceBag:
		return TEXT("ResourceBag");
	default:
		return NAME_None;
	}
}

FRpgInventorySlotGroupDefinition URpgPlayerInventoryLayoutComponent::MakeStaticGroup(
	FName ContainerId,
	const FText& DisplayName,
	int32 GridWidth,
	int32 GridHeight,
	const TArray<ERpgInventoryItemCategory>& AllowedCategories,
	bool bActionbarBindable,
	bool bCarrySlot,
	ERpgInventorySlotGroupKind GroupKind,
	FGameplayTag CarryActivationRole)
{
	FRpgInventorySlotGroupDefinition Group;
	Group.ContainerId = ContainerId;
	Group.DisplayName = DisplayName;
	Group.GroupKind = GroupKind;
	Group.GridSize.Width = FMath::Max(1, GridWidth);
	Group.GridSize.Height = FMath::Max(1, GridHeight);
	Group.Rule.AllowedCategories = AllowedCategories;
	Group.Rule.bActionbarBindable = bActionbarBindable;
	Group.Rule.bCarrySlot = bCarrySlot;
	Group.Rule.CarryActivationRole = CarryActivationRole;
	return Group;
}
