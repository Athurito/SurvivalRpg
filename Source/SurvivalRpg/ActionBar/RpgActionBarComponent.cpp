#include "RpgActionBarComponent.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Equipment/RpgAbilityBindingResolver.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryUiActionComponent.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgActionBarComponent)

URpgActionBarComponent::URpgActionBarComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void URpgActionBarComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, Slots, COND_OwnerOnly);
}

void URpgActionBarComponent::BeginPlay()
{
	EnsureSlotCount();
	Super::BeginPlay();
	RegisterStateListeners();
	RefreshBindingsInternal(true);
}

void URpgActionBarComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterStateListeners();
	Super::EndPlay(EndPlayReason);
}

FRpgActionBarSlot URpgActionBarComponent::GetSlot(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex] : FRpgActionBarSlot();
}

TArray<FRpgQuickAccessBinding> URpgActionBarComponent::GetQuickAccessBindings() const
{
	TArray<FRpgQuickAccessBinding> Result;
	Result.Reserve(8);
	for (int32 SlotIndex = 0; SlotIndex < 8; ++SlotIndex)
	{
		Result.Add(Slots.IsValidIndex(SlotIndex)
			? static_cast<const FRpgQuickAccessBinding&>(Slots[SlotIndex])
			: FRpgQuickAccessBinding());
	}
	return Result;
}

void URpgActionBarComponent::RequestBindInventorySlotToSlot_Implementation(int32 SlotIndex, FRpgInventorySlotAddress SlotAddress)
{
	TryBindInventorySlotToSlotAuthority(SlotIndex, SlotAddress);
}

bool URpgActionBarComponent::TryBindInventorySlotToSlotAuthority(
	int32 SlotIndex,
	const FRpgInventorySlotAddress& SlotAddress)
{
	EnsureSlotCount();
	if (!GetOwner() || !GetOwner()->HasAuthority() ||
		!IsValidSlotIndex(SlotIndex) || !SlotAddress.IsValid())
	{
		return false;
	}

	const ARpgPlayerController* RpgPC = GetRpgPlayerController();
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = RpgPC ? RpgPC->GetPlayerInventoryLayoutComponent() : nullptr;
	const URpgInventoryItemInstance* BoundItem = InventoryLayout ? InventoryLayout->GetItemInSlotAddress(SlotAddress) : nullptr;
	if (!InventoryLayout ||
		!InventoryLayout->CanBindSlotAddressToActionbar(SlotAddress, BoundItem) ||
		InventoryLayout->IsCarrySlotAddress(SlotAddress) ||
		!BoundItem ||
		BoundItem->FindFragmentByClass<URpgInventoryFragment_UsableItem>() == nullptr)
	{
		return false;
	}

	FRpgActionBarSlot Binding;
	Binding.SlotType = ERpgActionBarSlotType::Consumable;
	Binding.SlotAddress = SlotAddress;
	Binding.ConsumableDefinition = BoundItem->GetItemDef();
	Binding.PreferredItemId = BoundItem->GetItemId();
	ClearDuplicateBinding(SlotIndex, Binding);
	Slots[SlotIndex] = Binding;
	RefreshBindingsInternal(true);
	const FRpgActionBarSlot& AppliedSlot = Slots[SlotIndex];
	return AppliedSlot.SlotType == ERpgActionBarSlotType::Consumable &&
		AppliedSlot.SlotAddress == SlotAddress &&
		AppliedSlot.ConsumableDefinition == Binding.ConsumableDefinition &&
		AppliedSlot.PreferredItemId == Binding.PreferredItemId;
}

void URpgActionBarComponent::RequestBindCarrySlotToSlot_Implementation(int32 SlotIndex, FRpgInventorySlotAddress SlotAddress)
{
	TryBindCarrySlotToSlotAuthority(SlotIndex, SlotAddress);
}

bool URpgActionBarComponent::TryBindCarrySlotToSlotAuthority(
	int32 SlotIndex,
	const FRpgInventorySlotAddress& SlotAddress)
{
	EnsureSlotCount();
	if (!GetOwner() || !GetOwner()->HasAuthority() ||
		!IsValidSlotIndex(SlotIndex) || !SlotAddress.IsValid())
	{
		return false;
	}

	const ARpgPlayerController* RpgPC = GetRpgPlayerController();
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = RpgPC ? RpgPC->GetPlayerInventoryLayoutComponent() : nullptr;
	if (!InventoryLayout ||
		!InventoryLayout->IsCarrySlotAddress(SlotAddress))
	{
		return false;
	}

	FRpgActionBarSlot Binding;
	Binding.SlotType = ERpgActionBarSlotType::CarrySlot;
	Binding.SlotAddress = SlotAddress;
	Binding.CarryRole = SlotAddress.ContainerId;
	ClearDuplicateBinding(SlotIndex, Binding);
	Slots[SlotIndex] = Binding;
	RefreshBindingsInternal(true);
	const FRpgActionBarSlot& AppliedSlot = Slots[SlotIndex];
	return AppliedSlot.SlotType == ERpgActionBarSlotType::CarrySlot &&
		AppliedSlot.SlotAddress == SlotAddress &&
		AppliedSlot.CarryRole == SlotAddress.ContainerId;
}

void URpgActionBarComponent::RequestBindCarryRoleToSlot_Implementation(int32 SlotIndex, FName CarryRole)
{
	EnsureSlotCount();
	FRpgInventorySlotAddress CarryAddress;
	if (!IsValidSlotIndex(SlotIndex) || !IsValidCarryRole(CarryRole, CarryAddress))
	{
		return;
	}

	FRpgActionBarSlot Binding;
	Binding.SlotType = ERpgActionBarSlotType::CarrySlot;
	Binding.CarryRole = CarryRole;
	Binding.SlotAddress = CarryAddress;
	ClearDuplicateBinding(SlotIndex, Binding);
	Slots[SlotIndex] = Binding;
	RefreshBindingsInternal(true);
}

void URpgActionBarComponent::RequestBindConsumableToSlot_Implementation(
	int32 SlotIndex,
	TSubclassOf<URpgInventoryItemDefinition> ConsumableDefinition,
	FRpgInventoryItemId PreferredItemId)
{
	EnsureSlotCount();
	const URpgInventoryItemDefinition* ItemDefinition = ConsumableDefinition
		? GetDefault<URpgInventoryItemDefinition>(ConsumableDefinition)
		: nullptr;
	if (!IsValidSlotIndex(SlotIndex) ||
		!ItemDefinition ||
		!ItemDefinition->FindFragmentByClass(URpgInventoryFragment_UsableItem::StaticClass()))
	{
		return;
	}

	FRpgActionBarSlot Binding;
	Binding.SlotType = ERpgActionBarSlotType::Consumable;
	Binding.ConsumableDefinition = ConsumableDefinition;
	Binding.PreferredItemId = PreferredItemId;
	ClearDuplicateBinding(SlotIndex, Binding);
	Slots[SlotIndex] = Binding;
	RefreshBindingsInternal(true);
}

void URpgActionBarComponent::RequestBindAbilityToSlot_Implementation(int32 SlotIndex, FGameplayTag AbilityId)
{
	EnsureSlotCount();
	if (!IsValidSlotIndex(SlotIndex) || !AbilityId.IsValid())
	{
		return;
	}

	FRpgActionBarSlot Binding;
	Binding.SlotType = ERpgActionBarSlotType::Ability;
	Binding.AbilityId = AbilityId;
	ClearDuplicateBinding(SlotIndex, Binding);
	Slots[SlotIndex] = Binding;
	RefreshBindingsInternal(true);
}

void URpgActionBarComponent::RequestClearSlot_Implementation(int32 SlotIndex)
{
	TryClearSlotAuthority(SlotIndex);
}

bool URpgActionBarComponent::TryClearSlotAuthority(int32 SlotIndex)
{
	EnsureSlotCount();
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValidSlotIndex(SlotIndex))
	{
		return false;
	}

	Slots[SlotIndex] = FRpgActionBarSlot();
	RefreshBindingsInternal(true);
	return Slots[SlotIndex].IsEmpty();
}

void URpgActionBarComponent::RestoreQuickAccessBindings(const TArray<FRpgQuickAccessBinding>& SavedBindings)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	EnsureSlotCount();
	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		FRpgQuickAccessBinding& TargetBinding = static_cast<FRpgQuickAccessBinding&>(Slots[SlotIndex]);
		TargetBinding = SavedBindings.IsValidIndex(SlotIndex)
			? SavedBindings[SlotIndex]
			: FRpgQuickAccessBinding();
		if (TargetBinding.SlotType == ERpgActionBarSlotType::InventorySlotBinding)
		{
			TargetBinding.SlotType = ERpgActionBarSlotType::Consumable;
		}
		else if (TargetBinding.SlotType == ERpgActionBarSlotType::CarrySlotBinding)
		{
			TargetBinding.SlotType = ERpgActionBarSlotType::CarrySlot;
		}

		const bool bKnownType =
			TargetBinding.SlotType == ERpgActionBarSlotType::Empty ||
			TargetBinding.SlotType == ERpgActionBarSlotType::Consumable ||
			TargetBinding.SlotType == ERpgActionBarSlotType::CarrySlot ||
			TargetBinding.SlotType == ERpgActionBarSlotType::Ability;
		if (!bKnownType)
		{
			TargetBinding.Reset();
		}
	}

	RefreshBindingsInternal(true);
}

void URpgActionBarComponent::RefreshBindings()
{
	RefreshBindingsInternal(false);
}

void URpgActionBarComponent::RefreshBindingsInternal(bool bForceBroadcast)
{
	EnsureSlotCount();
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	const TArray<FRpgActionBarSlot> PreviousSlots = Slots;

	URpgAbilitySystemComponent* AbilitySystemComponent = GetRpgPlayerController()
		? GetRpgPlayerController()->GetRpgAbilitySystemComponent()
		: nullptr;
	if (AbilitySystemComponent && AbilitySystemComponent->HasGrantAuthority())
	{
		for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
		{
			AbilitySystemComponent->ClearRuntimeAbilityInputTag(GetInputTagForSlotIndex(SlotIndex));
		}
	}

	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		RefreshBindingAvailability(SlotIndex, Slots[SlotIndex], AbilitySystemComponent);
	}

	bool bBindingsChanged = PreviousSlots.Num() != Slots.Num();
	for (int32 SlotIndex = 0; !bBindingsChanged && SlotIndex < Slots.Num(); ++SlotIndex)
	{
		bBindingsChanged = !AreBindingsEquivalent(PreviousSlots[SlotIndex], Slots[SlotIndex]);
	}

	if (bForceBroadcast || bBindingsChanged)
	{
		OnRep_Slots();
	}
}

void URpgActionBarComponent::ActivateSlot(int32 SlotIndex)
{
	EnsureSlotCount();
	if (!IsValidSlotIndex(SlotIndex))
	{
		return;
	}

	const FRpgActionBarSlot& Slot = Slots[SlotIndex];
	ARpgPlayerController* RpgPC = GetRpgPlayerController();
	if (!RpgPC)
	{
		return;
	}

	if (Slot.IsEmpty() || (Slot.SlotType == ERpgActionBarSlotType::Ability && !Slot.bAvailable))
	{
		return;
	}

	if (Slot.SlotType == ERpgActionBarSlotType::Ability)
	{
		if (URpgAbilitySystemComponent* AbilitySystemComponent = RpgPC->GetRpgAbilitySystemComponent())
		{
			AbilitySystemComponent->AbilityInputTagPressed(GetInputTagForSlotIndex(SlotIndex));
		}
		return;
	}

	URpgInventoryUiActionComponent* UiActions = RpgPC->GetInventoryUiActionComponent();
	URpgPlayerInventoryLayoutComponent* InventoryLayout = RpgPC->GetPlayerInventoryLayoutComponent();
	if (!UiActions || !InventoryLayout)
	{
		return;
	}

	if (Slot.SlotType == ERpgActionBarSlotType::CarrySlot)
	{
		FRpgInventorySlotAddress CarryAddress;
		if (IsValidCarryRole(Slot.CarryRole, CarryAddress))
		{
			UiActions->RequestActivateCarrySlot(CarryAddress);
		}
		return;
	}

	if (Slot.SlotType == ERpgActionBarSlotType::Consumable)
	{
		ARpgPlayerState* RpgPS = RpgPC->GetRpgPlayerState();
		URpgInventoryManagerComponent* PlayerInventory = RpgPS ? RpgPS->GetInventoryManagerComponent() : nullptr;
		URpgInventoryItemInstance* Item = ResolveConsumableItem(Slot);
		if (PlayerInventory && Item)
		{
			if (Item->FindFragmentByClass<URpgInventoryFragment_UsableItem>() != nullptr)
			{
				UiActions->RequestUseInventoryItem(PlayerInventory, Item, 1);
			}
		}
	}
}

void URpgActionBarComponent::ReleaseSlot(int32 SlotIndex)
{
	EnsureSlotCount();
	if (!IsValidSlotIndex(SlotIndex))
	{
		return;
	}

	const FRpgActionBarSlot& Slot = Slots[SlotIndex];
	if (Slot.SlotType == ERpgActionBarSlotType::Ability && Slot.bAvailable)
	{
		if (ARpgPlayerController* RpgPC = GetRpgPlayerController())
		{
			if (URpgAbilitySystemComponent* AbilitySystemComponent = RpgPC->GetRpgAbilitySystemComponent())
			{
				AbilitySystemComponent->AbilityInputTagReleased(GetInputTagForSlotIndex(SlotIndex));
			}
		}
	}
}

void URpgActionBarComponent::TriggerSlot(int32 SlotIndex)
{
	ActivateSlot(SlotIndex);
	ReleaseSlot(SlotIndex);
}

FGameplayTag URpgActionBarComponent::GetInputTagForSlotIndex(int32 SlotIndex)
{
	switch (SlotIndex)
	{
	case 0:
		return RpgGameplayTags::InputTag_ActionBar_Slot_1;
	case 1:
		return RpgGameplayTags::InputTag_ActionBar_Slot_2;
	case 2:
		return RpgGameplayTags::InputTag_ActionBar_Slot_3;
	case 3:
		return RpgGameplayTags::InputTag_ActionBar_Slot_4;
	case 4:
		return RpgGameplayTags::InputTag_ActionBar_Slot_5;
	case 5:
		return RpgGameplayTags::InputTag_ActionBar_Slot_6;
	case 6:
		return RpgGameplayTags::InputTag_ActionBar_Slot_7;
	case 7:
		return RpgGameplayTags::InputTag_ActionBar_Slot_8;
	default:
		return FGameplayTag();
	}
}

void URpgActionBarComponent::OnRep_Slots()
{
	EnsureSlotCount();
	BroadcastSlotsChanged();
}

void URpgActionBarComponent::EnsureSlotCount()
{
	constexpr int32 RequiredSlotCount = 8;
	SlotCount = RequiredSlotCount;
	if (Slots.Num() != RequiredSlotCount)
	{
		Slots.SetNum(RequiredSlotCount);
	}
}

void URpgActionBarComponent::BroadcastSlotsChanged() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FRpgActionBarSlotsChangedMessage Message;
	Message.Owner = GetTypedOuter<APlayerController>();
	Message.ActionBarComponent = const_cast<URpgActionBarComponent*>(this);

	UGameplayMessageSubsystem& MessageSystem = UGameplayMessageSubsystem::Get(World);
	MessageSystem.BroadcastMessage(RpgGameplayTags::Rpg_ActionBar_Message_SlotsChanged, Message);
}

bool URpgActionBarComponent::IsValidSlotIndex(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex);
}

ARpgPlayerController* URpgActionBarComponent::GetRpgPlayerController() const
{
	return Cast<ARpgPlayerController>(GetOwner());
}

void URpgActionBarComponent::ClearDuplicateBinding(int32 TargetSlotIndex, const FRpgActionBarSlot& Binding)
{
	if (Binding.IsEmpty())
	{
		return;
	}

	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (Index == TargetSlotIndex || Slots[Index].SlotType != Binding.SlotType)
		{
			continue;
		}

		const bool bSameSource =
			(Binding.SlotType == ERpgActionBarSlotType::CarrySlot && Slots[Index].CarryRole == Binding.CarryRole) ||
			(Binding.SlotType == ERpgActionBarSlotType::Consumable && Slots[Index].ConsumableDefinition == Binding.ConsumableDefinition) ||
			(Binding.SlotType == ERpgActionBarSlotType::Ability && Slots[Index].AbilityId == Binding.AbilityId);
		if (bSameSource)
		{
			Slots[Index] = FRpgActionBarSlot();
		}
	}
}

URpgInventoryItemInstance* URpgActionBarComponent::ResolveConsumableItem(
	const FRpgActionBarSlot& Slot,
	FRpgInventorySlotAddress* OutAddress) const
{
	if (OutAddress)
	{
		*OutAddress = FRpgInventorySlotAddress();
	}

	if (Slot.SlotType != ERpgActionBarSlotType::Consumable || !Slot.ConsumableDefinition)
	{
		return nullptr;
	}

	const ARpgPlayerController* RpgPC = GetRpgPlayerController();
	const ARpgPlayerState* RpgPS = RpgPC ? RpgPC->GetRpgPlayerState() : nullptr;
	const URpgInventoryManagerComponent* PlayerInventory = RpgPS ? RpgPS->GetInventoryManagerComponent() : nullptr;
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = RpgPC ? RpgPC->GetPlayerInventoryLayoutComponent() : nullptr;
	if (!PlayerInventory || !InventoryLayout)
	{
		return nullptr;
	}

	URpgInventoryItemInstance* FallbackItem = nullptr;
	FRpgInventorySlotAddress FallbackAddress;
	for (const FRpgInventoryEntryView& Entry : PlayerInventory->GetAllEntries())
	{
		URpgInventoryItemInstance* Candidate = Entry.Instance;
		if (!Candidate || Candidate->GetItemDef() != Slot.ConsumableDefinition)
		{
			continue;
		}

		FRpgInventorySlotAddress CandidateAddress;
		if (!InventoryLayout->TryMakeSlotAddressFromPlacement(Entry.Placement, CandidateAddress) ||
			CandidateAddress.GetContainerHandle().Depth > 1 ||
			InventoryLayout->IsCarrySlotAddress(CandidateAddress) ||
			!InventoryLayout->CanBindSlotAddressToActionbar(CandidateAddress, Candidate) ||
			Candidate->FindFragmentByClass<URpgInventoryFragment_UsableItem>() == nullptr)
		{
			continue;
		}

		if (Entry.ItemId == Slot.PreferredItemId)
		{
			if (OutAddress)
			{
				*OutAddress = CandidateAddress;
			}
			return Candidate;
		}

		if (!FallbackItem)
		{
			FallbackItem = Candidate;
			FallbackAddress = CandidateAddress;
		}
	}

	if (FallbackItem && OutAddress)
	{
		*OutAddress = FallbackAddress;
	}
	return FallbackItem;
}

bool URpgActionBarComponent::IsValidCarryRole(FName CarryRole, FRpgInventorySlotAddress& OutAddress) const
{
	OutAddress = FRpgInventorySlotAddress();
	const ARpgPlayerController* RpgPC = GetRpgPlayerController();
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = RpgPC ? RpgPC->GetPlayerInventoryLayoutComponent() : nullptr;
	if (!InventoryLayout || CarryRole.IsNone())
	{
		return false;
	}

	for (const FRpgInventorySlotGroupView& Group : InventoryLayout->GetSlotGroups())
	{
		if (Group.ContainerId == CarryRole &&
			Group.GroupKind == ERpgInventorySlotGroupKind::Carry &&
			Group.Rule.bCarrySlot &&
			Group.ContainsCell(0, 0))
		{
			OutAddress = Group.MakeAddress(0, 0);
			return true;
		}
	}

	return false;
}

void URpgActionBarComponent::RefreshBindingAvailability(
	int32 SlotIndex,
	FRpgActionBarSlot& Slot,
	URpgAbilitySystemComponent* AbilitySystemComponent)
{
	Slot.bAvailable = false;
	Slot.BlockedReason = ERpgQuickAccessBlockedReason::Empty;
	if (Slot.SlotType == ERpgActionBarSlotType::InventorySlotBinding)
	{
		Slot.SlotType = ERpgActionBarSlotType::Consumable;
	}
	else if (Slot.SlotType == ERpgActionBarSlotType::CarrySlotBinding)
	{
		Slot.SlotType = ERpgActionBarSlotType::CarrySlot;
	}

	if (Slot.SlotType == ERpgActionBarSlotType::Empty)
	{
		return;
	}

	if (Slot.SlotType == ERpgActionBarSlotType::CarrySlot)
	{
		// Migrate pre-typed actionbar data whose numeric enum value already maps to CarrySlot.
		if (Slot.CarryRole.IsNone() && Slot.SlotAddress.IsValid())
		{
			Slot.CarryRole = Slot.SlotAddress.ContainerId;
		}

		FRpgInventorySlotAddress CarryAddress;
		if (!IsValidCarryRole(Slot.CarryRole, CarryAddress))
		{
			Slot.BlockedReason = ERpgQuickAccessBlockedReason::InvalidCarryRole;
			return;
		}

		Slot.SlotAddress = CarryAddress;
		const URpgPlayerInventoryLayoutComponent* InventoryLayout = GetRpgPlayerController()
			? GetRpgPlayerController()->GetPlayerInventoryLayoutComponent()
			: nullptr;
		URpgInventoryItemInstance* CarryItem = InventoryLayout ? InventoryLayout->GetItemInSlotAddress(CarryAddress) : nullptr;
		if (!InventoryLayout || !CarryItem || !InventoryLayout->CanBindSlotAddressToActionbar(CarryAddress, CarryItem))
		{
			Slot.BlockedReason = ERpgQuickAccessBlockedReason::MissingItem;
			return;
		}

		Slot.bAvailable = true;
		Slot.BlockedReason = ERpgQuickAccessBlockedReason::None;
		return;
	}

	if (Slot.SlotType == ERpgActionBarSlotType::Consumable)
	{
		// Migrate old cell-address bindings once, then resolve by definition and persistent item identity.
		if (!Slot.ConsumableDefinition && Slot.SlotAddress.IsValid())
		{
			const URpgPlayerInventoryLayoutComponent* InventoryLayout = GetRpgPlayerController()
				? GetRpgPlayerController()->GetPlayerInventoryLayoutComponent()
				: nullptr;
			if (URpgInventoryItemInstance* LegacyItem = InventoryLayout ? InventoryLayout->GetItemInSlotAddress(Slot.SlotAddress) : nullptr)
			{
				Slot.ConsumableDefinition = LegacyItem->GetItemDef();
				Slot.PreferredItemId = LegacyItem->GetItemId();
			}
		}

		const URpgInventoryItemDefinition* ItemDefinition = Slot.ConsumableDefinition
			? GetDefault<URpgInventoryItemDefinition>(Slot.ConsumableDefinition)
			: nullptr;
		if (!ItemDefinition || !ItemDefinition->FindFragmentByClass(URpgInventoryFragment_UsableItem::StaticClass()))
		{
			Slot.BlockedReason = ERpgQuickAccessBlockedReason::NotConsumable;
			return;
		}

		FRpgInventorySlotAddress ResolvedAddress;
		if (URpgInventoryItemInstance* Item = ResolveConsumableItem(Slot, &ResolvedAddress))
		{
			Slot.PreferredItemId = Item->GetItemId();
			Slot.SlotAddress = ResolvedAddress;
			Slot.bAvailable = true;
			Slot.BlockedReason = ERpgQuickAccessBlockedReason::None;
			return;
		}

		Slot.BlockedReason = ERpgQuickAccessBlockedReason::MissingItem;
		return;
	}

	if (Slot.SlotType == ERpgActionBarSlotType::Ability)
	{
		const FRpgUniqueAbilityBindingResolution Resolution = FRpgAbilityBindingResolver::ResolveUniqueAbilityId(
			AbilitySystemComponent,
			Slot.AbilityId,
			this);
		if (Resolution.Result == ERpgAbilityBindingResolveResult::Ambiguous)
		{
			Slot.BlockedReason = ERpgQuickAccessBlockedReason::AmbiguousAbility;
			return;
		}

		if (!Resolution.IsUnique() || !AbilitySystemComponent || !AbilitySystemComponent->HasGrantAuthority())
		{
			Slot.BlockedReason = ERpgQuickAccessBlockedReason::MissingAbility;
			return;
		}

		Slot.bAvailable = AbilitySystemComponent->BindInputTagToAbilityId(
			Slot.AbilityId,
			GetInputTagForSlotIndex(SlotIndex));
		Slot.BlockedReason = Slot.bAvailable
			? ERpgQuickAccessBlockedReason::None
			: ERpgQuickAccessBlockedReason::MissingAbility;
	}
}

bool URpgActionBarComponent::AreBindingsEquivalent(
	const FRpgQuickAccessBinding& A,
	const FRpgQuickAccessBinding& B)
{
	return A.SlotType == B.SlotType &&
		A.SlotAddress == B.SlotAddress &&
		A.CarryRole == B.CarryRole &&
		A.ConsumableDefinition == B.ConsumableDefinition &&
		A.PreferredItemId == B.PreferredItemId &&
		A.AbilityId == B.AbilityId &&
		A.bAvailable == B.bAvailable &&
		A.BlockedReason == B.BlockedReason;
}

void URpgActionBarComponent::RegisterStateListeners()
{
	UnregisterStateListeners();
	if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld())
	{
		return;
	}

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
	InventoryChangedHandle = MessageSubsystem.RegisterListener<FRpgInventoryChangeMessage>(
		FGameplayTag::RequestGameplayTag(TEXT("Rpg.Inventory.Message.StackChanged")),
		this,
		&ThisClass::HandleInventoryChanged);

	InventoryLayoutChangedHandle = MessageSubsystem.RegisterListener<FRpgPlayerInventoryLayoutChangedMessage>(
		RpgGameplayTags::Rpg_InventoryLayout_Message_Changed,
		this,
		&ThisClass::HandleInventoryLayoutChanged);
}

void URpgActionBarComponent::UnregisterStateListeners()
{
	if (InventoryChangedHandle.IsValid())
	{
		InventoryChangedHandle.Unregister();
	}

	if (InventoryLayoutChangedHandle.IsValid())
	{
		InventoryLayoutChangedHandle.Unregister();
	}
}

void URpgActionBarComponent::HandleInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message)
{
	const ARpgPlayerController* RpgPC = GetRpgPlayerController();
	const ARpgPlayerState* RpgPS = RpgPC ? RpgPC->GetRpgPlayerState() : nullptr;
	const URpgInventoryManagerComponent* PlayerInventory = RpgPS ? RpgPS->GetInventoryManagerComponent() : nullptr;
	if (PlayerInventory && Message.InventoryOwner == PlayerInventory)
	{
		RefreshBindings();
	}
}

void URpgActionBarComponent::HandleInventoryLayoutChanged(
	FGameplayTag Channel,
	const FRpgPlayerInventoryLayoutChangedMessage& Message)
{
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = GetRpgPlayerController()
		? GetRpgPlayerController()->GetPlayerInventoryLayoutComponent()
		: nullptr;
	if (InventoryLayout && Message.LayoutComponent == InventoryLayout)
	{
		RefreshBindings();
	}
}
