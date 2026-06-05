#include "RpgEquipmentManagerComponent.h"

#include "AbilitySystemGlobals.h"
#include "Engine/ActorChannel.h"
#include "Net/UnrealNetwork.h"
#include "RpgEquipmentDefinition.h"
#include "RpgEquipmentInstance.h"
#include "RpgWeaponInstance.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgEquipmentManagerComponent)

FString FRpgAppliedEquipmentEntry::GetDebugString() const
{
	return FString::Printf(TEXT("%s of %s in slot %d"), *GetNameSafe(Instance), *GetNameSafe(EquipmentDefinition.Get()), static_cast<int32>(EquippedSlot));
}

void FRpgEquipmentList::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	for (const int32 Index : RemovedIndices)
	{
		const FRpgAppliedEquipmentEntry& Entry = Entries[Index];
		if (Entry.Instance != nullptr)
		{
			Entry.Instance->OnUnequipped();
		}
	}
}

void FRpgEquipmentList::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	for (const int32 Index : AddedIndices)
	{
		const FRpgAppliedEquipmentEntry& Entry = Entries[Index];
		if (Entry.Instance != nullptr)
		{
			Entry.Instance->OnEquipped();
		}
	}
}

void FRpgEquipmentList::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
}

URpgAbilitySystemComponent* FRpgEquipmentList::GetAbilitySystemComponent() const
{
	check(OwnerComponent);
	AActor* OwningActor = OwnerComponent->GetOwner();
	return Cast<URpgAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwningActor));
}

URpgEquipmentInstance* FRpgEquipmentList::AddEntry(TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition, ERpgEquipmentSlot EquippedSlot)
{
	check(EquipmentDefinition != nullptr);
	check(OwnerComponent);
	check(OwnerComponent->GetOwner()->HasAuthority());

	const URpgEquipmentDefinition* EquipmentCDO = GetDefault<URpgEquipmentDefinition>(EquipmentDefinition);
	TSubclassOf<URpgEquipmentInstance> InstanceType = EquipmentCDO->InstanceType;
	if (InstanceType == nullptr)
	{
		InstanceType = URpgEquipmentInstance::StaticClass();
	}

	FRpgAppliedEquipmentEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.EquipmentDefinition = EquipmentDefinition;
	NewEntry.EquippedSlot = EquippedSlot;
	NewEntry.Instance = NewObject<URpgEquipmentInstance>(OwnerComponent->GetOwner(), InstanceType);

	URpgEquipmentInstance* Result = NewEntry.Instance;
	Result->SetEquippedSlot(EquippedSlot);

	Result->SpawnEquipmentActors(EquipmentCDO->ActorsToSpawn);
	MarkItemDirty(NewEntry);
	return Result;
}

void FRpgEquipmentList::RemoveEntry(URpgEquipmentInstance* Instance)
{
	if (Instance == nullptr)
	{
		return;
	}

	for (auto EntryIt = Entries.CreateIterator(); EntryIt; ++EntryIt)
	{
		FRpgAppliedEquipmentEntry& Entry = *EntryIt;
		if (Entry.Instance != Instance)
		{
			continue;
		}

		if (URpgAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent())
		{
			Entry.GrantedHandles.TakeFromAbilitySystem(AbilitySystemComponent);
		}

		Instance->DestroyEquipmentActors();
		EntryIt.RemoveCurrent();
		MarkArrayDirty();
		return;
	}
}

URpgEquipmentManagerComponent::URpgEquipmentManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, EquipmentList(this)
{
	SetIsReplicatedByDefault(true);
	bWantsInitializeComponent = true;
}

void URpgEquipmentManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, EquipmentList);
}

URpgEquipmentInstance* URpgEquipmentManagerComponent::EquipItem(TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition)
{
	const URpgEquipmentDefinition* EquipmentCDO = EquipmentDefinition ? GetDefault<URpgEquipmentDefinition>(EquipmentDefinition) : nullptr;
	return EquipItemInSlot(EquipmentDefinition, EquipmentCDO ? EquipmentCDO->GetDefaultEquipSlot() : ERpgEquipmentSlot::MainHand);
}

URpgEquipmentInstance* URpgEquipmentManagerComponent::EquipItemInSlot(TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition, ERpgEquipmentSlot Slot)
{
	URpgEquipmentInstance* Result = nullptr;
	if (CanEquipItemInSlot(EquipmentDefinition, Slot))
	{
		UnequipConflictingItems(EquipmentDefinition, Slot);

		Result = EquipmentList.AddEntry(EquipmentDefinition, Slot);
		if (Result != nullptr)
		{
			RebuildEquipmentAbilityGrants();
			Result->OnEquipped();

			if (IsUsingRegisteredSubObjectList() && IsReadyForReplication())
			{
				AddReplicatedSubObject(Result);
			}
		}
	}

	return Result;
}

void URpgEquipmentManagerComponent::UnequipItem(URpgEquipmentInstance* ItemInstance)
{
	if (ItemInstance == nullptr)
	{
		return;
	}

	if (IsUsingRegisteredSubObjectList())
	{
		RemoveReplicatedSubObject(ItemInstance);
	}

	ItemInstance->OnUnequipped();
	EquipmentList.RemoveEntry(ItemInstance);
	RebuildEquipmentAbilityGrants();
}

void URpgEquipmentManagerComponent::UnequipItemInSlot(ERpgEquipmentSlot Slot)
{
	TArray<URpgEquipmentInstance*> InstancesToUnequip;
	for (const FRpgAppliedEquipmentEntry& Entry : EquipmentList.Entries)
	{
		if (DoesEquipmentOccupySlot(Entry, Slot))
		{
			InstancesToUnequip.Add(Entry.Instance);
		}
	}

	for (URpgEquipmentInstance* Instance : InstancesToUnequip)
	{
		UnequipItem(Instance);
	}
}

URpgEquipmentInstance* URpgEquipmentManagerComponent::GetFirstInstanceOfType(TSubclassOf<URpgEquipmentInstance> InstanceType) const
{
	for (const FRpgAppliedEquipmentEntry& Entry : EquipmentList.Entries)
	{
		if (Entry.Instance != nullptr && Entry.Instance->IsA(InstanceType))
		{
			return Entry.Instance;
		}
	}

	return nullptr;
}

TArray<URpgEquipmentInstance*> URpgEquipmentManagerComponent::GetEquipmentInstancesOfType(TSubclassOf<URpgEquipmentInstance> InstanceType) const
{
	TArray<URpgEquipmentInstance*> Results;
	for (const FRpgAppliedEquipmentEntry& Entry : EquipmentList.Entries)
	{
		if (Entry.Instance != nullptr && Entry.Instance->IsA(InstanceType))
		{
			Results.Add(Entry.Instance);
		}
	}

	return Results;
}

URpgEquipmentInstance* URpgEquipmentManagerComponent::GetEquipmentInstanceInSlot(ERpgEquipmentSlot Slot) const
{
	for (const FRpgAppliedEquipmentEntry& Entry : EquipmentList.Entries)
	{
		if (Entry.EquippedSlot == Slot && Entry.Instance != nullptr)
		{
			return Entry.Instance;
		}
	}

	return nullptr;
}

bool URpgEquipmentManagerComponent::IsEquipmentSlotBlocked(ERpgEquipmentSlot Slot) const
{
	if (Slot == ERpgEquipmentSlot::None)
	{
		return true;
	}

	for (const FRpgAppliedEquipmentEntry& Entry : EquipmentList.Entries)
	{
		if (Entry.EquippedSlot != Slot && DoesEquipmentOccupySlot(Entry, Slot))
		{
			return true;
		}
	}

	return false;
}

bool URpgEquipmentManagerComponent::IsEquipmentInstanceActiveForInputTag(const URpgEquipmentInstance* EquipmentInstance, FGameplayTag InputTag) const
{
	if (!EquipmentInstance)
	{
		return false;
	}

	if (!InputTag.IsValid())
	{
		return true;
	}

	if (InputTag == RpgGameplayTags::InputTag_Weapon_Primary)
	{
		return EquipmentInstance == GetEquipmentInstanceInSlot(ERpgEquipmentSlot::MainHand);
	}

	if (InputTag == RpgGameplayTags::InputTag_Weapon_Secondary)
	{
		return !IsEquipmentSlotBlocked(ERpgEquipmentSlot::OffHand) &&
			EquipmentInstance == GetEquipmentInstanceInSlot(ERpgEquipmentSlot::OffHand);
	}

	if (InputTag == RpgGameplayTags::InputTag_Weapon_Block)
	{
		if (URpgEquipmentInstance* OffHandInstance = GetEquipmentInstanceInSlot(ERpgEquipmentSlot::OffHand))
		{
			if (CanEquipmentBlock(OffHandInstance))
			{
				return EquipmentInstance == OffHandInstance;
			}
		}

		URpgEquipmentInstance* MainHandInstance = GetEquipmentInstanceInSlot(ERpgEquipmentSlot::MainHand);
		return EquipmentInstance == MainHandInstance && CanEquipmentBlock(MainHandInstance);
	}

	return true;
}

bool URpgEquipmentManagerComponent::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool bWroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	for (FRpgAppliedEquipmentEntry& Entry : EquipmentList.Entries)
	{
		if (Entry.Instance != nullptr && IsValid(Entry.Instance))
		{
			bWroteSomething |= Channel->ReplicateSubobject(Entry.Instance, *Bunch, *RepFlags);
		}
	}

	return bWroteSomething;
}

void URpgEquipmentManagerComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

void URpgEquipmentManagerComponent::UninitializeComponent()
{
	TArray<URpgEquipmentInstance*> EquipmentInstances;
	for (const FRpgAppliedEquipmentEntry& Entry : EquipmentList.Entries)
	{
		EquipmentInstances.Add(Entry.Instance);
	}

	for (URpgEquipmentInstance* EquipmentInstance : EquipmentInstances)
	{
		UnequipItem(EquipmentInstance);
	}

	Super::UninitializeComponent();
}

void URpgEquipmentManagerComponent::ReadyForReplication()
{
	Super::ReadyForReplication();

	if (IsUsingRegisteredSubObjectList())
	{
		for (const FRpgAppliedEquipmentEntry& Entry : EquipmentList.Entries)
		{
			if (Entry.Instance != nullptr && IsValid(Entry.Instance))
			{
				AddReplicatedSubObject(Entry.Instance);
			}
		}
	}
}

bool URpgEquipmentManagerComponent::CanEquipItemInSlot(TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition, ERpgEquipmentSlot Slot) const
{
	const URpgEquipmentDefinition* EquipmentCDO = EquipmentDefinition ? GetDefault<URpgEquipmentDefinition>(EquipmentDefinition) : nullptr;
	return EquipmentCDO && EquipmentCDO->CanEquipInSlot(Slot);
}

void URpgEquipmentManagerComponent::UnequipConflictingItems(TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition, ERpgEquipmentSlot Slot)
{
	const URpgEquipmentDefinition* NewEquipmentCDO = EquipmentDefinition ? GetDefault<URpgEquipmentDefinition>(EquipmentDefinition) : nullptr;
	if (!NewEquipmentCDO)
	{
		return;
	}

	TArray<URpgEquipmentInstance*> InstancesToUnequip;
	for (const FRpgAppliedEquipmentEntry& Entry : EquipmentList.Entries)
	{
		const URpgEquipmentDefinition* ExistingEquipmentCDO = Entry.EquipmentDefinition ? GetDefault<URpgEquipmentDefinition>(Entry.EquipmentDefinition) : nullptr;
		if (!ExistingEquipmentCDO || !Entry.Instance)
		{
			continue;
		}

		const bool bNewConflictsWithExistingSlot = NewEquipmentCDO->OccupiesSlot(Slot, Entry.EquippedSlot);
		const bool bExistingConflictsWithNewMainHand = NewEquipmentCDO->OccupiesSlot(Slot, ERpgEquipmentSlot::MainHand) && ExistingEquipmentCDO->OccupiesSlot(Entry.EquippedSlot, ERpgEquipmentSlot::MainHand);
		const bool bExistingConflictsWithNewOffHand = NewEquipmentCDO->OccupiesSlot(Slot, ERpgEquipmentSlot::OffHand) && ExistingEquipmentCDO->OccupiesSlot(Entry.EquippedSlot, ERpgEquipmentSlot::OffHand);

		if (bNewConflictsWithExistingSlot || bExistingConflictsWithNewMainHand || bExistingConflictsWithNewOffHand)
		{
			InstancesToUnequip.Add(Entry.Instance);
		}
	}

	for (URpgEquipmentInstance* Instance : InstancesToUnequip)
	{
		UnequipItem(Instance);
	}
}

bool URpgEquipmentManagerComponent::DoesEquipmentOccupySlot(const FRpgAppliedEquipmentEntry& Entry, ERpgEquipmentSlot Slot) const
{
	const URpgEquipmentDefinition* EquipmentCDO = Entry.EquipmentDefinition ? GetDefault<URpgEquipmentDefinition>(Entry.EquipmentDefinition) : nullptr;
	return EquipmentCDO && EquipmentCDO->OccupiesSlot(Entry.EquippedSlot, Slot);
}

bool URpgEquipmentManagerComponent::CanEquipmentBlock(const URpgEquipmentInstance* EquipmentInstance) const
{
	const URpgWeaponInstance* WeaponInstance = Cast<URpgWeaponInstance>(EquipmentInstance);
	return WeaponInstance && WeaponInstance->CanBlock();
}

URpgEquipmentInstance* URpgEquipmentManagerComponent::GetActiveBlockSource() const
{
	if (URpgEquipmentInstance* OffHandInstance = GetEquipmentInstanceInSlot(ERpgEquipmentSlot::OffHand))
	{
		if (CanEquipmentBlock(OffHandInstance))
		{
			return OffHandInstance;
		}
	}

	URpgEquipmentInstance* MainHandInstance = GetEquipmentInstanceInSlot(ERpgEquipmentSlot::MainHand);
	return CanEquipmentBlock(MainHandInstance) ? MainHandInstance : nullptr;
}

bool URpgEquipmentManagerComponent::ShouldGrantSlotAbilitySet(const FRpgAppliedEquipmentEntry& Entry, const FRpgEquipmentSlotAbilitySet& SlotAbilitySet, const URpgEquipmentInstance* ActiveBlockSource) const
{
	if (!Entry.Instance || !SlotAbilitySet.AbilitySet)
	{
		return false;
	}

	if (Entry.EquippedSlot != SlotAbilitySet.EquippedSlot)
	{
		return false;
	}

	if (SlotAbilitySet.GrantPolicy == ERpgEquipmentAbilityGrantPolicy::ActiveBlockSourceOnly)
	{
		return Entry.Instance == ActiveBlockSource;
	}

	return true;
}

void URpgEquipmentManagerComponent::RebuildEquipmentAbilityGrants()
{
	URpgAbilitySystemComponent* AbilitySystemComponent = EquipmentList.GetAbilitySystemComponent();
	if (!AbilitySystemComponent)
	{
		return;
	}

	for (FRpgAppliedEquipmentEntry& Entry : EquipmentList.Entries)
	{
		Entry.GrantedHandles.TakeFromAbilitySystem(AbilitySystemComponent);
	}

	const URpgEquipmentInstance* ActiveBlockSource = GetActiveBlockSource();

	for (FRpgAppliedEquipmentEntry& Entry : EquipmentList.Entries)
	{
		const URpgEquipmentDefinition* EquipmentCDO = Entry.EquipmentDefinition ? GetDefault<URpgEquipmentDefinition>(Entry.EquipmentDefinition) : nullptr;
		if (!EquipmentCDO || !Entry.Instance)
		{
			continue;
		}

		for (const TObjectPtr<const URpgAbilitySet>& AbilitySet : EquipmentCDO->AbilitySetsToGrant)
		{
			if (AbilitySet != nullptr)
			{
				AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, &Entry.GrantedHandles, Entry.Instance);
			}
		}

		for (const FRpgEquipmentSlotAbilitySet& SlotAbilitySet : EquipmentCDO->SlotAbilitySetsToGrant)
		{
			if (ShouldGrantSlotAbilitySet(Entry, SlotAbilitySet, ActiveBlockSource))
			{
				SlotAbilitySet.AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, &Entry.GrantedHandles, Entry.Instance);
			}
		}
	}
}
