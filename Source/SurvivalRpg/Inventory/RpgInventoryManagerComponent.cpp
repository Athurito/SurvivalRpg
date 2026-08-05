// Copyright Epic Games, Inc. All Rights Reserved.
#include "RpgInventoryManagerComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Engine/ActorChannel.h"
#include "Engine/World.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerMessageTags.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UObjectGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryManagerComponent)

class FLifetimeProperty;
struct FReplicationFlags;

UE_DEFINE_GAMEPLAY_TAG(
	TAG_Rpg_Inventory_Message_StackChanged,
	"Rpg.Inventory.Message.StackChanged");

DEFINE_LOG_CATEGORY(LogRpgInventoryManager);


//////////////////////////////////////////////////////////////////////
// URpgInventoryManagerComponent

URpgInventoryManagerComponent::URpgInventoryManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, InventoryList(this)
{
	SetIsReplicatedByDefault(true);
	DefaultGridSize.Width = 10;
	DefaultGridSize.Height = 6;
}

void URpgInventoryManagerComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, CapacityMode);
	DOREPLIFETIME(ThisClass, FixedMaxEntries);
	DOREPLIFETIME(ThisClass, DefaultGridSize);
	DOREPLIFETIME(ThisClass, DefaultContainerId);
	FDoRepLifetimeParams InventoryStateParams;
	InventoryStateParams.Condition = COND_Dynamic;
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, InventoryList, InventoryStateParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, InventoryRevision, InventoryStateParams);
}

void URpgInventoryManagerComponent::GetReplicatedCustomConditionState(FCustomPropertyConditionState& OutActiveState) const
{
	Super::GetReplicatedCustomConditionState(OutActiveState);
	const ELifetimeCondition InventoryCondition = ReplicationPolicy == ERpgInventoryReplicationPolicy::OwnerOnly
		? COND_OwnerOnly
		: COND_None;
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(ThisClass, InventoryList, InventoryCondition);
	DOREPDYNAMICCONDITION_INITCONDITION_FAST(ThisClass, InventoryRevision, InventoryCondition);
}

void URpgInventoryManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	RefreshCapacityAttributeBinding();
}

void URpgInventoryManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearCapacityAttributeBinding();

	Super::EndPlay(EndPlayReason);
}

void URpgInventoryManagerComponent::SetCapacityMode(ERpgInventoryCapacityMode NewCapacityMode)
{
	if (IsInventoryMutationLocked())
	{
		return;
	}
	AActor* OwningActor = GetOwner();
	UWorld* World = OwningActor ? OwningActor->GetWorld() : nullptr;
	const bool bIsRuntimeGameWorld = World && World->IsGameWorld() && IsRegistered() && !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject);
	if (bIsRuntimeGameWorld && !OwningActor->HasAuthority())
	{
		return;
	}

	if (CapacityMode == NewCapacityMode)
	{
		return;
	}

	CapacityMode = NewCapacityMode;
	if (bIsRuntimeGameWorld)
	{
		RefreshCapacityAttributeBinding();
		BroadcastCapacityChanged();
	}
}

void URpgInventoryManagerComponent::SetFixedMaxEntries(int32 NewFixedMaxEntries)
{
	if (IsInventoryMutationLocked())
	{
		return;
	}
	AActor* OwningActor = GetOwner();
	UWorld* World = OwningActor ? OwningActor->GetWorld() : nullptr;
	const bool bIsRuntimeGameWorld = World && World->IsGameWorld() && IsRegistered() && !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject);
	if (bIsRuntimeGameWorld && !OwningActor->HasAuthority())
	{
		return;
	}

	const int32 ClampedValue = FMath::Max(0, NewFixedMaxEntries);
	if (FixedMaxEntries == ClampedValue)
	{
		return;
	}

	FixedMaxEntries = ClampedValue;
	if (bIsRuntimeGameWorld)
	{
		BroadcastCapacityChanged();
	}
}

void URpgInventoryManagerComponent::SetCapacityAttribute(FGameplayAttribute NewCapacityAttribute)
{
	if (IsInventoryMutationLocked())
	{
		return;
	}
	AActor* OwningActor = GetOwner();
	UWorld* World = OwningActor ? OwningActor->GetWorld() : nullptr;
	const bool bIsRuntimeGameWorld = World && World->IsGameWorld() && IsRegistered() && !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject);
	if (bIsRuntimeGameWorld && !OwningActor->HasAuthority())
	{
		return;
	}

	if (CapacityAttribute == NewCapacityAttribute)
	{
		return;
	}

	CapacityAttribute = NewCapacityAttribute;
	if (bIsRuntimeGameWorld)
	{
		RefreshCapacityAttributeBinding();
		BroadcastCapacityChanged();
	}
}

bool URpgInventoryManagerComponent::CanSetDefaultGridSize(
	FRpgInventoryGridSize NewGridSize) const
{
	if (!NewGridSize.IsValid() || FindOwningPlayerInventoryLayout())
	{
		return false;
	}

	const FRpgInventoryContainerHandle DefaultHandle =
		FRpgInventoryContainerHandle::MakeRoot(DefaultContainerId);
	for (const FRpgInventoryEntryView& Entry : GetAllEntries())
	{
		if (Entry.Placement.GetContainerHandle() != DefaultHandle)
		{
			continue;
		}

		const FRpgInventoryGridSize Occupied =
			Entry.Placement.GetOccupiedSize();
		const int64 Right = static_cast<int64>(Entry.Placement.X) + Occupied.Width;
		const int64 Bottom = static_cast<int64>(Entry.Placement.Y) + Occupied.Height;
		if (!Entry.Placement.IsValid() || Right > NewGridSize.Width ||
			Bottom > NewGridSize.Height)
		{
			return false;
		}
	}
	return true;
}

bool URpgInventoryManagerComponent::SetDefaultGridSize(
	FRpgInventoryGridSize NewGridSize)
{
	if (IsInventoryMutationLocked() || !CanSetDefaultGridSize(NewGridSize))
	{
		return false;
	}

	AActor* OwningActor = GetOwner();
	UWorld* World = OwningActor ? OwningActor->GetWorld() : nullptr;
	const bool bIsRuntimeGameWorld =
		World && World->IsGameWorld() && IsRegistered() &&
		!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject);
	if (bIsRuntimeGameWorld && (!OwningActor || !OwningActor->HasAuthority()))
	{
		return false;
	}
	if (DefaultGridSize == NewGridSize)
	{
		return true;
	}

	DefaultGridSize = NewGridSize;
	if (bIsRuntimeGameWorld)
	{
		BroadcastCapacityChanged();
		OwningActor->ForceNetUpdate();
	}
	return true;
}

bool URpgInventoryManagerComponent::ExpandDefaultGridToMinimum(
	FRpgInventoryGridSize MinimumSize)
{
	if (IsInventoryMutationLocked())
	{
		return false;
	}
	AActor* OwningActor = GetOwner();
	UWorld* World = OwningActor ? OwningActor->GetWorld() : nullptr;
	const bool bIsRuntimeGameWorld =
		World && World->IsGameWorld() && IsRegistered() &&
		!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject);
	if (!MinimumSize.IsValid() ||
		(bIsRuntimeGameWorld &&
			(!OwningActor || !OwningActor->HasAuthority())))
	{
		return false;
	}

	FRpgInventoryGridSize ExpandedSize;
	ExpandedSize.Width =
		FMath::Max(DefaultGridSize.Width, MinimumSize.Width);
	ExpandedSize.Height =
		FMath::Max(DefaultGridSize.Height, MinimumSize.Height);
	if (ExpandedSize.Width == DefaultGridSize.Width &&
		ExpandedSize.Height == DefaultGridSize.Height)
	{
		return true;
	}

	DefaultGridSize = ExpandedSize;
	if (bIsRuntimeGameWorld)
	{
		BroadcastCapacityChanged();
		OwningActor->ForceNetUpdate();
	}
	return true;
}

TArray<URpgInventoryItemInstance*> URpgInventoryManagerComponent::GetAllItems() const
{
	return InventoryList.GetAllItems();
}

TArray<FRpgInventoryEntryView> URpgInventoryManagerComponent::GetAllEntries() const
{
	return InventoryList.GetAllEntries();
}

URpgInventoryItemInstance* URpgInventoryManagerComponent::GetItemAtContainerCell(FRpgInventoryContainerHandle ContainerHandle, int32 X, int32 Y) const
{
	return InventoryList.GetItemAtCell(ContainerHandle, X, Y);
}

bool URpgInventoryManagerComponent::GetItemPlacement(URpgInventoryItemInstance* ItemInstance, FRpgInventoryGridPlacement& OutPlacement) const
{
	return InventoryList.GetPlacementForItem(ItemInstance, OutPlacement);
}

int32 URpgInventoryManagerComponent::GetFreeStackCapacity(URpgInventoryItemInstance* ItemInstance) const
{
	return InventoryList.GetFreeStackCapacity(ItemInstance);
}

bool URpgInventoryManagerComponent::ContainsItemInstance(URpgInventoryItemInstance* ItemInstance) const
{
	return InventoryList.ContainsItemInstance(ItemInstance);
}

bool URpgInventoryManagerComponent::ContainsEntry(FGuid EntryId) const
{
	return InventoryList.ContainsEntry(EntryId);
}

URpgInventoryItemInstance* URpgInventoryManagerComponent::FindItemById(FRpgInventoryItemId ItemId) const
{
	const FRpgInventoryEntry* Entry = InventoryList.FindEntryByItemId(ItemId);
	return Entry ? Entry->Instance.Get() : nullptr;
}

int32 URpgInventoryManagerComponent::GetItemStackCount(URpgInventoryItemInstance* ItemInstance) const
{
	return InventoryList.GetStackCount(ItemInstance);
}

URpgInventoryItemInstance* URpgInventoryManagerComponent::FindFirstItemStackByDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef) const
{
	for (const FRpgInventoryEntry& Entry : InventoryList.Entries)
	{
		URpgInventoryItemInstance* Instance = Entry.Instance;

		if (IsValid(Instance))
		{
			if (Instance->GetItemDef() == ItemDef)
			{
				return Instance;
			}
		}
	}

	return nullptr;
}

int32 URpgInventoryManagerComponent::GetTotalItemCountByDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef) const
{
	int32 TotalCount = 0;
	for (const FRpgInventoryEntry& Entry : InventoryList.Entries)
	{
		URpgInventoryItemInstance* Instance = Entry.Instance;

		if (IsValid(Instance))
		{
			if (Instance->GetItemDef() == ItemDef)
			{
				TotalCount += Entry.StackCount;
			}
		}
	}

	return TotalCount;
}

void URpgInventoryManagerComponent::ReadyForReplication()
{
	Super::ReadyForReplication();

	// Register existing URpgInventoryItemInstance
	if (IsUsingRegisteredSubObjectList())
	{
		for (const FRpgInventoryEntry& Entry : InventoryList.Entries)
		{
			URpgInventoryItemInstance* Instance = Entry.Instance;

			if (IsValid(Instance))
			{
				AddReplicatedSubObject(Instance, ReplicationPolicy == ERpgInventoryReplicationPolicy::OwnerOnly ? COND_OwnerOnly : COND_None);
			}
		}
	}
}

bool URpgInventoryManagerComponent::ReplicateSubobjects(UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool WroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);
	if (ReplicationPolicy == ERpgInventoryReplicationPolicy::OwnerOnly && (!RepFlags || !RepFlags->bNetOwner))
	{
		return WroteSomething;
	}

	for (FRpgInventoryEntry& Entry : InventoryList.Entries)
	{
		URpgInventoryItemInstance* Instance = Entry.Instance;

		if (Instance && IsValid(Instance))
		{
			WroteSomething |= Channel->ReplicateSubobject(Instance, *Bunch, *RepFlags);
		}
	}

	return WroteSomething;
}

void URpgInventoryManagerComponent::OnRep_InventoryRevision()
{
	BroadcastInventoryStateChanged();
}

void URpgInventoryManagerComponent::OnRep_CapacitySettings()
{
	RefreshCapacityAttributeBinding();
	BroadcastCapacityChanged();
}

void URpgInventoryManagerComponent::MarkInventoryStateDirty()
{
	AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority())
	{
		return;
	}

	++InventoryRevision;
	OwningActor->ForceNetUpdate();
	OnInventoryPostCommit.Broadcast(this);
}

void URpgInventoryManagerComponent::BroadcastInventoryStateChanged() const
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || !IsRegistered() || HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return;
	}

	FRpgInventoryChangeMessage Message;
	Message.InventoryOwner = const_cast<URpgInventoryManagerComponent*>(this);
	Message.bOrderChanged = true;

	UGameplayMessageSubsystem& MessageSystem = UGameplayMessageSubsystem::Get(World);
	MessageSystem.BroadcastMessage(TAG_Rpg_Inventory_Message_StackChanged, Message);
}


bool URpgInventoryManagerComponent::IsInventoryMutationLocked() const
{
	return bIsApplyingPickupBatch || bIsPlanningPickupBatch ||
		bIsApplyingCollectBatch || bIsApplyingCrossInventoryTransfer ||
		bIsRestoringInventoryGraph;
}


UAbilitySystemComponent* URpgInventoryManagerComponent::FindCapacityAbilitySystem() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	if (const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(OwnerActor))
	{
		return AbilitySystemInterface->GetAbilitySystemComponent();
	}

	return OwnerActor->FindComponentByClass<UAbilitySystemComponent>();
}

void URpgInventoryManagerComponent::RefreshCapacityAttributeBinding()
{
	ClearCapacityAttributeBinding();

	if (CapacityMode != ERpgInventoryCapacityMode::AbilitySystemAttribute || !CapacityAttribute.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = FindCapacityAbilitySystem())
	{
		BoundCapacityAbilitySystem = ASC;
		CapacityAttributeChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(CapacityAttribute)
			.AddUObject(this, &ThisClass::HandleCapacityAttributeChanged);
	}
}

void URpgInventoryManagerComponent::ClearCapacityAttributeBinding()
{
	if (UAbilitySystemComponent* ASC = BoundCapacityAbilitySystem.Get())
	{
		if (CapacityAttributeChangedHandle.IsValid() && CapacityAttribute.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(CapacityAttribute).Remove(CapacityAttributeChangedHandle);
		}
	}

	CapacityAttributeChangedHandle.Reset();
	BoundCapacityAbilitySystem.Reset();
}

void URpgInventoryManagerComponent::HandleCapacityAttributeChanged(const FOnAttributeChangeData& Data)
{
	BroadcastCapacityChanged();
}

void URpgInventoryManagerComponent::BroadcastCapacityChanged() const
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || !IsRegistered() || HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return;
	}

	FRpgInventoryChangeMessage Message;
	Message.InventoryOwner = const_cast<URpgInventoryManagerComponent*>(this);
	Message.bCapacityChanged = true;
	Message.bOrderChanged = true;

	UGameplayMessageSubsystem& MessageSystem = UGameplayMessageSubsystem::Get(World);
	MessageSystem.BroadcastMessage(TAG_Rpg_Inventory_Message_StackChanged, Message);
}

//////////////////////////////////////////////////////////////////////
//

// UCLASS(Abstract)
// class URpgInventoryFilter : public UObject
// {
// public:
// 	virtual bool PassesFilter(URpgInventoryItemInstance* Instance) const { return true; }
// };

// UCLASS()
// class URpgInventoryFilter_HasTag : public URpgInventoryFilter
// {
// public:
// 	virtual bool PassesFilter(URpgInventoryItemInstance* Instance) const { return true; }
// };


