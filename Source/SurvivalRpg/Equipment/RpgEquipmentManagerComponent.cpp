#include "RpgEquipmentManagerComponent.h"

#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/ActorChannel.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "RpgEquipmentDefinition.h"
#include "RpgEquipmentInstance.h"
#include "RpgWeaponInstance.h"
#include "RpgWeaponAbilityLoadoutComponent.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/AbilitySystem/Effects/RpgItemizationEquipmentEffect.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/Itemization/RpgItemizationGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_SkillTree.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeComponent.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgEquipmentManagerComponent)

namespace RpgEquipmentSkillTrees
{
	void GiveSkillNodeGrant(
		URpgAbilitySystemComponent& AbilitySystem,
		FRpgAppliedSkillNodeGrant& Grant,
		URpgEquipmentInstance* SourceEquipment)
	{
		if (Grant.AbilitySet)
		{
			Grant.AbilitySet->GiveToAbilitySystem(&AbilitySystem, &Grant.GrantedHandles, SourceEquipment);
		}
		if (!Grant.GrantedTags.IsEmpty())
		{
			AbilitySystem.AddLooseGameplayTags(Grant.GrantedTags, 1, EGameplayTagReplicationState::TagOnly);
		}
	}

	void TakeSkillNodeGrant(URpgAbilitySystemComponent& AbilitySystem, FRpgAppliedSkillNodeGrant& Grant)
	{
		Grant.GrantedHandles.TakeFromAbilitySystem(&AbilitySystem);
		if (!Grant.GrantedTags.IsEmpty())
		{
			AbilitySystem.RemoveLooseGameplayTags(Grant.GrantedTags, 1, EGameplayTagReplicationState::TagOnly);
			Grant.GrantedTags.Reset();
		}
	}
}

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

void FRpgEquipmentList::PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
	// Also called when an initially unmapped equipment subobject becomes available to a late joiner.
	if (URpgEquipmentManagerComponent* Manager = Cast<URpgEquipmentManagerComponent>(OwnerComponent))
	{
		Manager->RefreshBlockLocomotionLayer();
	}
}

URpgAbilitySystemComponent* FRpgEquipmentList::GetAbilitySystemComponent() const
{
	check(OwnerComponent);
	AActor* OwningActor = OwnerComponent->GetOwner();
	return Cast<URpgAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwningActor));
}

URpgEquipmentInstance* FRpgEquipmentList::AddEntry(
	TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition,
	ERpgEquipmentSlot EquippedSlot,
	UObject* SourceItemInstigator)
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
	Result->SetInstigator(SourceItemInstigator);
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
			if (Entry.ItemizationEffectHandle.IsValid())
			{
				AbilitySystemComponent->RemoveActiveGameplayEffect(Entry.ItemizationEffectHandle);
				Entry.ItemizationEffectHandle.Invalidate();
			}
			for (TPair<int32, FRpgAppliedEquipmentAbilityGrant>& GrantPair : Entry.AbilitySetGrants)
			{
				GrantPair.Value.GrantedHandles.TakeFromAbilitySystem(AbilitySystemComponent);
			}
			for (TPair<FGameplayTag, FRpgAppliedSkillNodeGrant>& NodeGrantPair : Entry.SkillNodeGrants)
			{
				RpgEquipmentSkillTrees::TakeSkillNodeGrant(*AbilitySystemComponent, NodeGrantPair.Value);
			}
		}
		Entry.AbilitySetGrants.Reset();
		Entry.SkillNodeGrants.Reset();

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
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void URpgEquipmentManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, EquipmentList);
}

URpgEquipmentInstance* URpgEquipmentManagerComponent::EquipItem(TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return nullptr;
	}

	const URpgEquipmentDefinition* EquipmentCDO = EquipmentDefinition ? GetDefault<URpgEquipmentDefinition>(EquipmentDefinition) : nullptr;
	return EquipItemInSlot(EquipmentDefinition, EquipmentCDO ? EquipmentCDO->GetDefaultEquipSlot() : ERpgEquipmentSlot::MainHand);
}

URpgEquipmentInstance* URpgEquipmentManagerComponent::EquipItemInSlot(TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition, ERpgEquipmentSlot Slot)
{
	return EquipItemInSlotWithInstigator(EquipmentDefinition, Slot, nullptr);
}

URpgEquipmentInstance* URpgEquipmentManagerComponent::EquipItemInSlotWithInstigator(
	TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition,
	ERpgEquipmentSlot Slot,
	UObject* SourceItemInstigator)
{
	URpgEquipmentInstance* Result = nullptr;
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return Result;
	}

	if (CanEquipItemInSlot(EquipmentDefinition, Slot))
	{
		UnequipConflictingItems(EquipmentDefinition, Slot);

		Result = EquipmentList.AddEntry(EquipmentDefinition, Slot, SourceItemInstigator);
		if (Result != nullptr)
		{
			if (URpgInventoryItemInstance* SourceItem = Cast<URpgInventoryItemInstance>(SourceItemInstigator))
			{
				SourceItem->OnItemizationStateChanged.AddUniqueDynamic(
					this,
					&ThisClass::HandleEquippedItemizationStateChanged);
			}
			RebuildEquipmentAbilityGrants();
			Result->OnEquipped();
			RefreshBlockLocomotionLayer();

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
	if (!GetOwner() || !GetOwner()->HasAuthority() || ItemInstance == nullptr)
	{
		return;
	}

	if (IsUsingRegisteredSubObjectList())
	{
		RemoveReplicatedSubObject(ItemInstance);
	}

	if (URpgInventoryItemInstance* SourceItem = Cast<URpgInventoryItemInstance>(ItemInstance->GetInstigator()))
	{
		SourceItem->OnItemizationStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleEquippedItemizationStateChanged);
	}
	ItemInstance->OnUnequipped();
	EquipmentList.RemoveEntry(ItemInstance);
	RebuildEquipmentAbilityGrants();
	RefreshBlockLocomotionLayer();
}

void URpgEquipmentManagerComponent::UnequipItemInSlot(ERpgEquipmentSlot Slot)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

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
	bBlockLayerShuttingDown = true;
	ClearBlockLocomotionLayer();
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

void URpgEquipmentManagerComponent::BeginPlay()
{
	Super::BeginPlay();
	bBlockLayerShuttingDown = false;
	BlockPawnExtension = URpgPawnExtensionComponent::FindPawnExtensionComponent(GetOwner());
	if (URpgPawnExtensionComponent* Extension = BlockPawnExtension.Get())
	{
		Extension->OnAbilitySystemUninitialized_Register(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleBlockAvatarUninitialized));
		Extension->OnAbilitySystemInitialized_RegisterAndCall(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleBlockAvatarInitialized));
	}
	RefreshBlockLocomotionLayer();
}

void URpgEquipmentManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bBlockLayerShuttingDown = true;
	SetComponentTickEnabled(false);
	if (URpgPawnExtensionComponent* Extension = BlockPawnExtension.Get())
	{
		Extension->OnAbilitySystemInitialized.RemoveAll(this);
		Extension->OnAbilitySystemUninitialized.RemoveAll(this);
	}
	BlockPawnExtension.Reset();
	ClearBlockLocomotionLayer();
	if (USkeletalMeshComponent* Mesh = BlockGameplayMesh.Get())
	{
		Mesh->OnAnimInitialized.RemoveDynamic(this, &ThisClass::HandleBlockAnimationInitialized);
		Mesh->RemoveTickPrerequisiteComponent(this);
	}
	BlockGameplayMesh.Reset();
	Super::EndPlay(EndPlayReason);
}

void URpgEquipmentManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// Resolve mesh/subobject readiness and replacement before animation evaluation, never from a worker graph.
	RefreshBlockLocomotionLayer();
}

void URpgEquipmentManagerComponent::HandleBlockAvatarInitialized()
{
	RefreshBlockLocomotionLayer();
}

void URpgEquipmentManagerComponent::HandleBlockAvatarUninitialized()
{
	ClearBlockLocomotionLayer();
}

void URpgEquipmentManagerComponent::HandleBlockAnimationInitialized()
{
	// Reinitialization can reuse the same UAnimInstance pointer while recreating its linked nodes.
	ClearBlockLocomotionLayer();
	RefreshBlockLocomotionLayer();
}

UAnimInstance* URpgEquipmentManagerComponent::GetBlockLocomotionLayerInstance() const
{
	check(IsInGameThread());
	UAnimInstance* MainInstance = BlockMainAnimInstance.Get();
	const USkeletalMeshComponent* Mesh = BlockGameplayMesh.Get();
	return MainInstance && Mesh && Mesh->GetAnimInstance() == MainInstance && BlockLayerClass.IsValid()
		? MainInstance->GetLinkedAnimLayerInstanceByClass(BlockLayerClass.Get()) : nullptr;
}

void URpgEquipmentManagerComponent::ClearBlockLocomotionLayer()
{
	check(IsInGameThread());
	UAnimInstance* PreviousInstance = BlockMainAnimInstance.Get();
	UClass* PreviousClass = BlockLayerClass.Get();
	BlockMainAnimInstance.Reset();
	BlockLayerSource.Reset();
	BlockLayerClass.Reset();
	// Retire ownership before callbacks. A replacement main instance already owns its own linked graph lifecycle.
	if (PreviousInstance && PreviousClass && BlockGameplayMesh.IsValid()
		&& BlockGameplayMesh->GetAnimInstance() == PreviousInstance)
	{
		PreviousInstance->UnlinkAnimClassLayers(PreviousClass);
	}
}

void URpgEquipmentManagerComponent::RefreshBlockLocomotionLayer()
{
	check(IsInGameThread());
	if (bBlockLayerShuttingDown || bRefreshingBlockLayer) { return; }
	TGuardValue<bool> RefreshGuard(bRefreshingBlockLayer, true);
	SetComponentTickEnabled(!EquipmentList.Entries.IsEmpty());
	if (!HasBegunPlay()) { return; }

	USkeletalMeshComponent* Mesh = URpgPawnExtensionComponent::FindGameplayMesh(GetOwner());
	if (BlockGameplayMesh.Get() != Mesh)
	{
		ClearBlockLocomotionLayer();
		if (USkeletalMeshComponent* PreviousMesh = BlockGameplayMesh.Get())
		{
			PreviousMesh->OnAnimInitialized.RemoveDynamic(this, &ThisClass::HandleBlockAnimationInitialized);
			PreviousMesh->RemoveTickPrerequisiteComponent(this);
		}
		BlockGameplayMesh = Mesh;
		if (Mesh)
		{
			Mesh->OnAnimInitialized.AddUniqueDynamic(this, &ThisClass::HandleBlockAnimationInitialized);
			Mesh->AddTickPrerequisiteComponent(this);
		}
	}
	URpgWeaponInstance* Source = Cast<URpgWeaponInstance>(GetActiveBlockSource());
	UClass* DesiredClass = Source ? Source->GetBlockDefinition().BlockLocomotionLayer.Get() : nullptr;
	UAnimInstance* MainInstance = Mesh && Mesh->IsRegistered() ? Mesh->GetAnimInstance() : nullptr;
	if (const URpgPawnExtensionComponent* Extension = BlockPawnExtension.Get())
	{
		const URpgAbilitySystemComponent* ASC = Extension->GetRpgAbilitySystemComponent();
		if (!ASC || ASC->GetAvatarActor() != GetOwner()) { DesiredClass = nullptr; }
	}
	if (!MainInstance || !DesiredClass)
	{
		ClearBlockLocomotionLayer();
		return;
	}
	if (BlockMainAnimInstance.Get() == MainInstance && BlockLayerClass.Get() == DesiredClass && BlockLayerSource.Get() == Source)
	{
		return;
	}
	ClearBlockLocomotionLayer();
	BlockMainAnimInstance = MainInstance;
	BlockLayerClass = DesiredClass;
	BlockLayerSource = Source;
	MainInstance->LinkAnimClassLayers(DesiredClass);
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
	// The offhand class may still be unmapped after the list arrives. Wait for that decision rather
	// than briefly binding a mainhand presentation which authority never selected.
	for (const FRpgAppliedEquipmentEntry& Entry : EquipmentList.Entries)
	{
		if (Entry.EquippedSlot == ERpgEquipmentSlot::OffHand && !Entry.Instance) { return nullptr; }
	}
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

void URpgEquipmentManagerComponent::RebuildEquipmentItemizationEffects()
{
	for (FRpgAppliedEquipmentEntry& Entry : EquipmentList.Entries)
	{
		RefreshEquipmentItemizationEffect(Entry);
	}
}

void URpgEquipmentManagerComponent::RefreshEquipmentItemizationEffect(
	FRpgAppliedEquipmentEntry& Entry)
{
	URpgAbilitySystemComponent* AbilitySystemComponent = EquipmentList.GetAbilitySystemComponent();
	if (!AbilitySystemComponent || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	using namespace RpgItemizationGameplayTags;
	const TArray<FGameplayTag> GlobalStatTags = {
		Item_Stat_Armor,
		Item_Stat_Strength,
		Item_Stat_Intelligence,
		Item_Stat_Resilience,
		Item_Stat_Vitality,
		Item_Stat_ArmorPenetration,
		Item_Stat_CriticalHitChance,
		Item_Stat_CriticalHitDamage,
		Item_Stat_CriticalHitResistance,
		Item_Stat_MaxStamina,
	};

	const URpgInventoryItemInstance* SourceItem = Entry.Instance
		? Cast<URpgInventoryItemInstance>(Entry.Instance->GetInstigator())
		: nullptr;
	const FRpgItemizationState EmptyState;
	const FRpgItemizationState& Itemization = SourceItem
		? SourceItem->GetItemizationStateRef()
		: EmptyState;

	bool bHasGlobalValue = false;
	for (const FGameplayTag& StatTag : GlobalStatTags)
	{
		bHasGlobalValue |= !FMath::IsNearlyZero(Itemization.GetTotalValueForStat(StatTag));
	}

	const bool bStateUnchanged = Entry.bHasAppliedItemizationState &&
		Entry.AppliedItemizationState == Itemization;
	const bool bExistingEffectIsActive = Entry.ItemizationEffectHandle.IsValid() &&
		AbilitySystemComponent->GetActiveGameplayEffect(Entry.ItemizationEffectHandle) != nullptr;
	if (bStateUnchanged &&
		((bHasGlobalValue && bExistingEffectIsActive) ||
			(!bHasGlobalValue && !Entry.ItemizationEffectHandle.IsValid())))
	{
		return;
	}

	if (Entry.ItemizationEffectHandle.IsValid())
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(Entry.ItemizationEffectHandle);
		Entry.ItemizationEffectHandle.Invalidate();
	}
	Entry.AppliedItemizationState = Itemization;
	Entry.bHasAppliedItemizationState = true;

	if (!Itemization.bGenerated || !bHasGlobalValue)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(Entry.Instance);
	FGameplayEffectSpec EffectSpec(
		GetDefault<URpgItemizationEquipmentEffect>(),
		EffectContext,
		1.0f);
	for (const FGameplayTag& StatTag : GlobalStatTags)
	{
		EffectSpec.SetSetByCallerMagnitude(
			StatTag,
			Itemization.GetTotalValueForStat(StatTag));
	}

	Entry.ItemizationEffectHandle =
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(EffectSpec);
}

void URpgEquipmentManagerComponent::HandleEquippedItemizationStateChanged(
	const FRpgItemizationState& NewState)
{
	(void)NewState;
	RebuildEquipmentItemizationEffects();
}

void URpgEquipmentManagerComponent::RebuildEquipmentAbilityGrants()
{
	RebuildEquipmentItemizationEffects();
	URpgAbilitySystemComponent* AbilitySystemComponent = EquipmentList.GetAbilitySystemComponent();
	if (!AbilitySystemComponent)
	{
		return;
	}

	const URpgEquipmentInstance* ActiveBlockSource = GetActiveBlockSource();
	bool bGrantsChanged = false;
	URpgSkillTreeComponent* SkillTrees = URpgSkillTreeComponent::FindForActor(GetOwner());
	const bool bHasAuthority = GetOwner() && GetOwner()->HasAuthority();

	for (FRpgAppliedEquipmentEntry& Entry : EquipmentList.Entries)
	{
		TMap<int32, const URpgAbilitySet*> DesiredAbilitySets;
		const URpgEquipmentDefinition* EquipmentCDO = Entry.EquipmentDefinition ? GetDefault<URpgEquipmentDefinition>(Entry.EquipmentDefinition) : nullptr;
		if (EquipmentCDO && Entry.Instance)
		{
			for (int32 AbilitySetIndex = 0; AbilitySetIndex < EquipmentCDO->AbilitySetsToGrant.Num(); ++AbilitySetIndex)
			{
				if (const URpgAbilitySet* AbilitySet = EquipmentCDO->AbilitySetsToGrant[AbilitySetIndex])
				{
					// Even keys belong to unconditional grants from AbilitySetsToGrant.
					DesiredAbilitySets.Add(AbilitySetIndex * 2, AbilitySet);
				}
			}

			for (int32 SlotAbilitySetIndex = 0; SlotAbilitySetIndex < EquipmentCDO->SlotAbilitySetsToGrant.Num(); ++SlotAbilitySetIndex)
			{
				const FRpgEquipmentSlotAbilitySet& SlotAbilitySet = EquipmentCDO->SlotAbilitySetsToGrant[SlotAbilitySetIndex];
				if (ShouldGrantSlotAbilitySet(Entry, SlotAbilitySet, ActiveBlockSource))
				{
					// Odd keys keep conditional slot grants separate from unconditional grants.
					DesiredAbilitySets.Add(SlotAbilitySetIndex * 2 + 1, SlotAbilitySet.AbilitySet);
				}
			}
		}

		for (auto GrantIt = Entry.AbilitySetGrants.CreateIterator(); GrantIt; ++GrantIt)
		{
			const URpgAbilitySet* const* DesiredAbilitySet = DesiredAbilitySets.Find(GrantIt.Key());
			if (!DesiredAbilitySet || GrantIt.Value().AbilitySet != *DesiredAbilitySet)
			{
				GrantIt.Value().GrantedHandles.TakeFromAbilitySystem(AbilitySystemComponent);
				GrantIt.RemoveCurrent();
				bGrantsChanged = true;
			}
		}

		for (const TPair<int32, const URpgAbilitySet*>& DesiredGrant : DesiredAbilitySets)
		{
			if (!Entry.AbilitySetGrants.Contains(DesiredGrant.Key))
			{
				FRpgAppliedEquipmentAbilityGrant& NewGrant = Entry.AbilitySetGrants.Add(DesiredGrant.Key);
				NewGrant.AbilitySet = DesiredGrant.Value;
				DesiredGrant.Value->GiveToAbilitySystem(AbilitySystemComponent, &NewGrant.GrantedHandles, Entry.Instance);
				bGrantsChanged = true;
			}
		}

		// Learned nodes of the item's skill tree are granted while the item is in use, sourced from its equipment.
		TMap<FGameplayTag, const FRpgSkillTreeNode*> DesiredNodes;
		const URpgSkillTreeDefinition* SkillTree = Entry.Instance && SkillTrees && bHasAuthority
			? URpgInventoryFragment_SkillTree::FindActiveSkillTreeOfItem(Entry.Instance->GetInstigator(), Entry.EquippedSlot)
			: nullptr;
		if (SkillTree)
		{
			SkillTrees->RegisterSkillTree(SkillTree);
			for (const FRpgSkillTreeNode& Node : SkillTree->Nodes)
			{
				if ((Node.AbilitySet || !Node.GrantedTags.IsEmpty()) &&
					SkillTrees->IsNodeUnlocked(SkillTree->TreeTag, Node.NodeTag))
				{
					DesiredNodes.Add(Node.NodeTag, &Node);
				}
			}
		}

		for (auto NodeGrantIt = Entry.SkillNodeGrants.CreateIterator(); NodeGrantIt; ++NodeGrantIt)
		{
			const FRpgSkillTreeNode* const* DesiredNode = DesiredNodes.Find(NodeGrantIt.Key());
			if (!DesiredNode ||
				NodeGrantIt.Value().AbilitySet != (*DesiredNode)->AbilitySet ||
				NodeGrantIt.Value().GrantedTags != (*DesiredNode)->GrantedTags)
			{
				RpgEquipmentSkillTrees::TakeSkillNodeGrant(*AbilitySystemComponent, NodeGrantIt.Value());
				NodeGrantIt.RemoveCurrent();
				bGrantsChanged = true;
			}
		}

		for (const TPair<FGameplayTag, const FRpgSkillTreeNode*>& DesiredNode : DesiredNodes)
		{
			if (!Entry.SkillNodeGrants.Contains(DesiredNode.Key))
			{
				FRpgAppliedSkillNodeGrant& NewGrant = Entry.SkillNodeGrants.Add(DesiredNode.Key);
				NewGrant.AbilitySet = DesiredNode.Value->AbilitySet;
				NewGrant.GrantedTags = DesiredNode.Value->GrantedTags;
				RpgEquipmentSkillTrees::GiveSkillNodeGrant(*AbilitySystemComponent, NewGrant, Entry.Instance);
				bGrantsChanged = true;
			}
		}
	}

	// Q/E/R and quick-access bindings resolve against the granted specs, whichever equip path changed them.
	if (bGrantsChanged)
	{
		RefreshOwnerAbilityBindings();
	}
}

void URpgEquipmentManagerComponent::RefreshSkillTreeGrants()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	RebuildEquipmentAbilityGrants();
	// A changed slot assignment moves Q/E/R even when no grant changed.
	RefreshOwnerAbilityBindings();
}

void URpgEquipmentManagerComponent::RefreshOwnerAbilityBindings() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Grants made inside an executing ability (for example a collect interaction) stay pending until the ability
	// list unlocks, so the controller re-resolves its bindings on the next tick.
	World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(
			this,
			[this]()
			{
				const APawn* Pawn = GetPawn<APawn>();
				const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
				if (!Controller || !Controller->HasAuthority())
				{
					return;
				}

				if (URpgWeaponAbilityLoadoutComponent* WeaponAbilityLoadout =
						Controller->FindComponentByClass<URpgWeaponAbilityLoadoutComponent>())
				{
					WeaponAbilityLoadout->RefreshAbilityBindings();
				}
				if (URpgActionBarComponent* ActionBar = Controller->FindComponentByClass<URpgActionBarComponent>())
				{
					ActionBar->RefreshBindings();
				}
			}));
}
