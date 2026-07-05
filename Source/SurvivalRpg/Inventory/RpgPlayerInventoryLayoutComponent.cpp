#include "RpgPlayerInventoryLayoutComponent.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "RpgInventoryFragment_EquippableItem.h"
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
const FName URpgPlayerInventoryLayoutComponent::ToolSlot1GroupId(TEXT("ToolSlot1"));
const FName URpgPlayerInventoryLayoutComponent::ToolSlot2GroupId(TEXT("ToolSlot2"));
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

		if (URpgPlayerInventoryLayoutComponent::IsSlotContainerEquipmentSlot(EquipmentSlot))
		{
			return Item->FindFragmentByClass<URpgInventoryFragment_SlotContainerProvider>() != nullptr;
		}

		const URpgInventoryFragment_EquippableItem* EquippableFragment = Item->FindFragmentByClass<URpgInventoryFragment_EquippableItem>();
		const TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition = EquippableFragment ? EquippableFragment->GetEquipmentDefinition() : nullptr;
		const URpgEquipmentDefinition* EquipmentCDO = EquipmentDefinition ? GetDefault<URpgEquipmentDefinition>(EquipmentDefinition) : nullptr;
		return EquipmentCDO && EquipmentCDO->CanEquipInSlot(EquipmentSlot);
	}
}

URpgPlayerInventoryLayoutComponent::URpgPlayerInventoryLayoutComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);

	StaticSlotGroups =
	{
		MakeStaticGroup(GearHeadGroupId, NSLOCTEXT("RpgInventoryLayout", "GearHead", "Head"), 1, { ERpgInventoryItemCategory::Armor }, false, false, ERpgInventorySlotGroupKind::Gear),
		MakeStaticGroup(GearChestGroupId, NSLOCTEXT("RpgInventoryLayout", "GearChest", "Chest"), 1, { ERpgInventoryItemCategory::Armor }, false, false, ERpgInventorySlotGroupKind::Gear),
		MakeStaticGroup(GearHandsGroupId, NSLOCTEXT("RpgInventoryLayout", "GearHands", "Hands"), 1, { ERpgInventoryItemCategory::Armor }, false, false, ERpgInventorySlotGroupKind::Gear),
		MakeStaticGroup(GearLegsGroupId, NSLOCTEXT("RpgInventoryLayout", "GearLegs", "Legs"), 1, { ERpgInventoryItemCategory::Armor }, false, false, ERpgInventorySlotGroupKind::Gear),
		MakeStaticGroup(GearFeetGroupId, NSLOCTEXT("RpgInventoryLayout", "GearFeet", "Feet"), 1, { ERpgInventoryItemCategory::Armor }, false, false, ERpgInventorySlotGroupKind::Gear),
		MakeStaticGroup(GearBackpackGroupId, NSLOCTEXT("RpgInventoryLayout", "GearBackpack", "Backpack"), 1, {}, false, false, ERpgInventorySlotGroupKind::Gear),
		MakeStaticGroup(GearBeltGroupId, NSLOCTEXT("RpgInventoryLayout", "GearBelt", "Belt"), 1, {}, false, false, ERpgInventorySlotGroupKind::Gear),
		MakeStaticGroup(GearPouchGroupId, NSLOCTEXT("RpgInventoryLayout", "GearPouch", "Pouch"), 1, {}, false, false, ERpgInventorySlotGroupKind::Gear),
		MakeStaticGroup(GearResourceBagGroupId, NSLOCTEXT("RpgInventoryLayout", "GearResourceBag", "Resource Bag"), 1, {}, false, false, ERpgInventorySlotGroupKind::Gear),
		MakeStaticGroup(WeaponSlot1GroupId, NSLOCTEXT("RpgInventoryLayout", "WeaponSlot1", "Weapon 1"), 1, { ERpgInventoryItemCategory::Weapon }, true, true, ERpgInventorySlotGroupKind::Carry),
		MakeStaticGroup(WeaponSlot2GroupId, NSLOCTEXT("RpgInventoryLayout", "WeaponSlot2", "Weapon 2"), 1, { ERpgInventoryItemCategory::Weapon }, true, true, ERpgInventorySlotGroupKind::Carry),
		MakeStaticGroup(ShieldSlotGroupId, NSLOCTEXT("RpgInventoryLayout", "ShieldSlot", "Shield"), 1, { ERpgInventoryItemCategory::Shield }, true, true, ERpgInventorySlotGroupKind::Carry),
		MakeStaticGroup(ToolSlot1GroupId, NSLOCTEXT("RpgInventoryLayout", "ToolSlot1", "Tool 1"), 1, { ERpgInventoryItemCategory::Tool }, true, true, ERpgInventorySlotGroupKind::Carry),
		MakeStaticGroup(ToolSlot2GroupId, NSLOCTEXT("RpgInventoryLayout", "ToolSlot2", "Tool 2"), 1, { ERpgInventoryItemCategory::Tool }, true, true, ERpgInventorySlotGroupKind::Carry),
		MakeStaticGroup(PocketsGroupId, NSLOCTEXT("RpgInventoryLayout", "Pockets", "Pockets"), 8, {}, false, false, ERpgInventorySlotGroupKind::Content)
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

int32 URpgPlayerInventoryLayoutComponent::GetTotalSlotCount() const
{
	int32 TotalSlotCount = 0;
	for (const FRpgInventorySlotGroupView& Group : BuildSlotGroups())
	{
		TotalSlotCount += FMath::Max(0, Group.SlotCount);
	}
	return TotalSlotCount;
}

bool URpgPlayerInventoryLayoutComponent::ResolveSlotAddress(const FRpgInventorySlotAddress& Address, int32& OutGlobalSlotIndex) const
{
	OutGlobalSlotIndex = INDEX_NONE;
	if (!Address.IsValid())
	{
		return false;
	}

	for (const FRpgInventorySlotGroupView& Group : BuildSlotGroups())
	{
		if (Group.GroupId == Address.GroupId && Address.LocalSlotIndex >= 0 && Address.LocalSlotIndex < Group.SlotCount)
		{
			OutGlobalSlotIndex = Group.FirstGlobalSlotIndex + Address.LocalSlotIndex;
			return true;
		}
	}

	return false;
}

bool URpgPlayerInventoryLayoutComponent::TryMakeSlotAddressFromGlobalSlotIndex(int32 GlobalSlotIndex, FRpgInventorySlotAddress& OutAddress) const
{
	OutAddress = FRpgInventorySlotAddress();
	if (GlobalSlotIndex < 0)
	{
		return false;
	}

	for (const FRpgInventorySlotGroupView& Group : BuildSlotGroups())
	{
		if (Group.ContainsGlobalSlotIndex(GlobalSlotIndex))
		{
			OutAddress.GroupId = Group.GroupId;
			OutAddress.LocalSlotIndex = GlobalSlotIndex - Group.FirstGlobalSlotIndex;
			return true;
		}
	}

	return false;
}

URpgInventoryItemInstance* URpgPlayerInventoryLayoutComponent::GetItemInSlotAddress(const FRpgInventorySlotAddress& Address) const
{
	int32 GlobalSlotIndex = INDEX_NONE;
	URpgInventoryManagerComponent* Inventory = FindPlayerInventory();
	return Inventory && ResolveSlotAddress(Address, GlobalSlotIndex) ? Inventory->GetItemInSlot(GlobalSlotIndex) : nullptr;
}

bool URpgPlayerInventoryLayoutComponent::CanItemUseSlotAddress(URpgInventoryItemInstance* Item, const FRpgInventorySlotAddress& Address) const
{
	if (!Item || !Address.IsValid())
	{
		return false;
	}

	for (const FRpgInventorySlotGroupView& Group : BuildSlotGroups())
	{
		if (Group.GroupId == Address.GroupId && Address.LocalSlotIndex >= 0 && Address.LocalSlotIndex < Group.SlotCount)
		{
			if (!Group.Rule.AllowsItem(Item))
			{
				return false;
			}

			if (Group.GroupKind == ERpgInventorySlotGroupKind::Gear)
			{
				ERpgEquipmentSlot EquipmentSlot = ERpgEquipmentSlot::None;
				return TryGetEquipmentSlotForGearGroupId(Group.GroupId, EquipmentSlot) &&
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
		if (Group.GroupId == Address.GroupId && Address.LocalSlotIndex >= 0 && Address.LocalSlotIndex < Group.SlotCount)
		{
			return Group.Rule.bActionbarBindable;
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
		if (Group.GroupId != Address.GroupId || Address.LocalSlotIndex < 0 || Address.LocalSlotIndex >= Group.SlotCount)
		{
			continue;
		}

		if (!Group.Rule.bActionbarBindable || !Group.Rule.AllowsItem(Item))
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
		if (Group.GroupId == Address.GroupId && Address.LocalSlotIndex >= 0 && Address.LocalSlotIndex < Group.SlotCount)
		{
			return Group.GroupKind == ERpgInventorySlotGroupKind::Carry && Group.Rule.bCarrySlot;
		}
	}

	return false;
}

bool URpgPlayerInventoryLayoutComponent::IsGearSlotAddress(const FRpgInventorySlotAddress& Address) const
{
	if (!Address.IsValid())
	{
		return false;
	}

	for (const FRpgInventorySlotGroupView& Group : BuildSlotGroups())
	{
		if (Group.GroupId == Address.GroupId && Address.LocalSlotIndex >= 0 && Address.LocalSlotIndex < Group.SlotCount)
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
		if (Group.GroupId == Address.GroupId && Address.LocalSlotIndex >= 0 && Address.LocalSlotIndex < Group.SlotCount)
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

	PlayerInventory->SetCapacityMode(ERpgInventoryCapacityMode::FixedEntries);
	PlayerInventory->SetFixedMaxEntries(GetTotalSlotCount());
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
	if (!IsSlotContainerEquipmentSlot(EquipmentSlot))
	{
		return true;
	}

	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	if (!PlayerInventory)
	{
		return true;
	}

	const FName SourceName = EquipmentSlotToSourceName(EquipmentSlot);
	bool bCheckThisAndFollowingProviderGroups = false;
	for (const FRpgInventorySlotGroupView& Group : BuildSlotGroups())
	{
		if (!Group.bProvidedByEquipment)
		{
			continue;
		}

		if (Group.SourceEquipmentSlotName == SourceName)
		{
			bCheckThisAndFollowingProviderGroups = true;
		}

		if (!bCheckThisAndFollowingProviderGroups)
		{
			continue;
		}

		for (int32 LocalSlotIndex = 0; LocalSlotIndex < Group.SlotCount; ++LocalSlotIndex)
		{
			if (PlayerInventory->GetItemInSlot(Group.FirstGlobalSlotIndex + LocalSlotIndex) != nullptr)
			{
				return false;
			}
		}
	}

	return true;
}

bool URpgPlayerInventoryLayoutComponent::IsBuiltInCarryGroupId(FName GroupId)
{
	return GroupId == WeaponSlot1GroupId ||
		GroupId == WeaponSlot2GroupId ||
		GroupId == ShieldSlotGroupId ||
		GroupId == ToolSlot1GroupId ||
		GroupId == ToolSlot2GroupId;
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
		OutAddress.GroupId = GearHeadGroupId;
		break;
	case ERpgEquipmentSlot::Chest:
		OutAddress.GroupId = GearChestGroupId;
		break;
	case ERpgEquipmentSlot::Hands:
		OutAddress.GroupId = GearHandsGroupId;
		break;
	case ERpgEquipmentSlot::Legs:
		OutAddress.GroupId = GearLegsGroupId;
		break;
	case ERpgEquipmentSlot::Feet:
		OutAddress.GroupId = GearFeetGroupId;
		break;
	case ERpgEquipmentSlot::Backpack:
		OutAddress.GroupId = GearBackpackGroupId;
		break;
	case ERpgEquipmentSlot::Belt:
		OutAddress.GroupId = GearBeltGroupId;
		break;
	case ERpgEquipmentSlot::Pouch:
		OutAddress.GroupId = GearPouchGroupId;
		break;
	case ERpgEquipmentSlot::ResourceBag:
		OutAddress.GroupId = GearResourceBagGroupId;
		break;
	default:
		return false;
	}

	OutAddress.LocalSlotIndex = 0;
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
	int32 NextGlobalSlotIndex = 0;

	AppendGroupViews(StaticSlotGroups, false, ERpgEquipmentSlot::None, Groups, NextGlobalSlotIndex);

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
				if (ExistingGroup.GroupId == GearAddress.GroupId && ExistingGroup.SlotCount > 0)
				{
					ProviderItem = PlayerInventory->GetItemInSlot(ExistingGroup.FirstGlobalSlotIndex);
					break;
				}
			}
		}

		const URpgInventoryFragment_SlotContainerProvider* ProviderFragment = ProviderItem ? ProviderItem->FindFragmentByClass<URpgInventoryFragment_SlotContainerProvider>() : nullptr;
		if (!ProviderFragment)
		{
			continue;
		}

		AppendGroupViews(ProviderFragment->ProvidedSlotGroups, true, ProviderSlot, Groups, NextGlobalSlotIndex);
	}

	return Groups;
}

void URpgPlayerInventoryLayoutComponent::AppendGroupViews(
	const TArray<FRpgInventorySlotGroupDefinition>& GroupDefinitions,
	bool bProvidedByEquipment,
	ERpgEquipmentSlot SourceEquipmentSlot,
	TArray<FRpgInventorySlotGroupView>& OutGroups,
	int32& InOutFirstGlobalSlotIndex) const
{
	for (const FRpgInventorySlotGroupDefinition& GroupDefinition : GroupDefinitions)
	{
		if (GroupDefinition.GroupId.IsNone() || GroupDefinition.SlotCount <= 0)
		{
			continue;
		}

		FRpgInventorySlotGroupView& GroupView = OutGroups.AddDefaulted_GetRef();
		GroupView.GroupId = GroupDefinition.GroupId;
		GroupView.DisplayName = GroupDefinition.DisplayName;
		GroupView.Icon = GroupDefinition.Icon;
		GroupView.GroupKind = GroupDefinition.GroupKind;
		GroupView.FirstGlobalSlotIndex = InOutFirstGlobalSlotIndex;
		GroupView.SlotCount = GroupDefinition.SlotCount;
		GroupView.Rule = GroupDefinition.Rule;
		GroupView.bProvidedByEquipment = bProvidedByEquipment;
		GroupView.SourceEquipmentSlotName = EquipmentSlotToSourceName(SourceEquipmentSlot);
		InOutFirstGlobalSlotIndex += GroupDefinition.SlotCount;
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
	Message.TotalSlotCount = GetTotalSlotCount();

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
	FName GroupId,
	const FText& DisplayName,
	int32 SlotCount,
	const TArray<ERpgInventoryItemCategory>& AllowedCategories,
	bool bActionbarBindable,
	bool bCarrySlot,
	ERpgInventorySlotGroupKind GroupKind)
{
	FRpgInventorySlotGroupDefinition Group;
	Group.GroupId = GroupId;
	Group.DisplayName = DisplayName;
	Group.GroupKind = GroupKind;
	Group.SlotCount = SlotCount;
	Group.Rule.AllowedCategories = AllowedCategories;
	Group.Rule.bActionbarBindable = bActionbarBindable;
	Group.Rule.bCarrySlot = bCarrySlot;
	return Group;
}
