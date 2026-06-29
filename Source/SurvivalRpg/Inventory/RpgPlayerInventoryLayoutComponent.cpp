#include "RpgPlayerInventoryLayoutComponent.h"

#include "GameFramework/GameplayMessageSubsystem.h"
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

URpgPlayerInventoryLayoutComponent::URpgPlayerInventoryLayoutComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);

	StaticSlotGroups =
	{
		MakeStaticGroup(WeaponSlot1GroupId, NSLOCTEXT("RpgInventoryLayout", "WeaponSlot1", "Weapon 1"), 1, { ERpgInventoryItemCategory::Weapon }, true, true),
		MakeStaticGroup(WeaponSlot2GroupId, NSLOCTEXT("RpgInventoryLayout", "WeaponSlot2", "Weapon 2"), 1, { ERpgInventoryItemCategory::Weapon }, true, true),
		MakeStaticGroup(ShieldSlotGroupId, NSLOCTEXT("RpgInventoryLayout", "ShieldSlot", "Shield"), 1, { ERpgInventoryItemCategory::Shield }, true, true),
		MakeStaticGroup(ToolSlot1GroupId, NSLOCTEXT("RpgInventoryLayout", "ToolSlot1", "Tool 1"), 1, { ERpgInventoryItemCategory::Tool }, true, true),
		MakeStaticGroup(ToolSlot2GroupId, NSLOCTEXT("RpgInventoryLayout", "ToolSlot2", "Tool 2"), 1, { ERpgInventoryItemCategory::Tool }, true, true),
		MakeStaticGroup(PocketsGroupId, NSLOCTEXT("RpgInventoryLayout", "Pockets", "Pockets"), 19, {}, true, false)
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
			return Group.Rule.AllowsItem(Item);
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
			return Group.Rule.bCarrySlot;
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

	for (const FRpgInventorySlotGroupView& Group : BuildSlotGroups())
	{
		if (!Group.bProvidedByEquipment || Group.SourceEquipmentSlotName != EquipmentSlotToSourceName(EquipmentSlot))
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

	const URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout();
	const ERpgEquipmentSlot ProviderSlots[] =
	{
		ERpgEquipmentSlot::Backpack,
		ERpgEquipmentSlot::Belt,
		ERpgEquipmentSlot::Pouch,
		ERpgEquipmentSlot::ResourceBag
	};

	for (const ERpgEquipmentSlot ProviderSlot : ProviderSlots)
	{
		URpgInventoryItemInstance* ProviderItem = EquipmentLoadout ? EquipmentLoadout->GetItemInEquipmentSlot(ProviderSlot) : nullptr;
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
	bool bCarrySlot)
{
	FRpgInventorySlotGroupDefinition Group;
	Group.GroupId = GroupId;
	Group.DisplayName = DisplayName;
	Group.SlotCount = SlotCount;
	Group.Rule.AllowedCategories = AllowedCategories;
	Group.Rule.bActionbarBindable = bActionbarBindable;
	Group.Rule.bCarrySlot = bCarrySlot;
	return Group;
}
