#include "RpgEquipmentManagerComponent.h"

#include "AbilitySystemGlobals.h"
#include "Engine/ActorChannel.h"
#include "Net/UnrealNetwork.h"
#include "RpgEquipmentDefinition.h"
#include "RpgEquipmentInstance.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgEquipmentManagerComponent)

FString FRpgAppliedEquipmentEntry::GetDebugString() const
{
	return FString::Printf(TEXT("%s of %s"), *GetNameSafe(Instance), *GetNameSafe(EquipmentDefinition.Get()));
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

URpgEquipmentInstance* FRpgEquipmentList::AddEntry(TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition)
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
	NewEntry.Instance = NewObject<URpgEquipmentInstance>(OwnerComponent->GetOwner(), InstanceType);

	URpgEquipmentInstance* Result = NewEntry.Instance;

	if (URpgAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent())
	{
		for (const TObjectPtr<const URpgAbilitySet>& AbilitySet : EquipmentCDO->AbilitySetsToGrant)
		{
			if (AbilitySet != nullptr)
			{
				AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, &NewEntry.GrantedHandles, Result);
			}
		}
	}

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
	URpgEquipmentInstance* Result = nullptr;
	if (EquipmentDefinition != nullptr)
	{
		Result = EquipmentList.AddEntry(EquipmentDefinition);
		if (Result != nullptr)
		{
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
