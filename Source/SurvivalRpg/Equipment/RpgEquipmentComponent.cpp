#include "RpgEquipmentComponent.h"

#include "Engine/ActorChannel.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/GameplayTags/GameplayTags.h"
#include "SurvivalRpg/Items/Fragments/RpgItemFragment_Equipment.h"
#include "SurvivalRpg/Items/Fragments/RpgItemFragment_Weapon.h"
#include "SurvivalRpg/Items/RpgItemInstance.h"
#include "RpgEquipmentRuleset.h"

namespace
{
	struct FRpgDesiredAbilitySetGrant
	{
		TWeakObjectPtr<UObject> SourceObject = nullptr;
		int32 Count = 0;
	};

	struct FRpgDesiredGrantSnapshot
	{
		TMap<const URpgAbilitySet*, FRpgDesiredAbilitySetGrant> AbilitySets;
		TMap<FRpgItemGameplayEffectGrantKey, int32> GameplayEffects;
		TMap<FGameplayTag, int32> LooseTags;
	};

	void AddDesiredAbilitySets(FRpgDesiredGrantSnapshot& Snapshot, const TArray<TObjectPtr<const URpgAbilitySet>>& AbilitySets, UObject* SourceObject)
	{
		for (const URpgAbilitySet* AbilitySet : AbilitySets)
		{
			if (AbilitySet == nullptr)
			{
				continue;
			}

			FRpgDesiredAbilitySetGrant& DesiredGrant = Snapshot.AbilitySets.FindOrAdd(AbilitySet);
			if (!DesiredGrant.SourceObject.IsValid())
			{
				DesiredGrant.SourceObject = SourceObject;
			}

			++DesiredGrant.Count;
		}
	}

	void AddDesiredGameplayEffects(FRpgDesiredGrantSnapshot& Snapshot, const TArray<FRpgItemGameplayEffectGrant>& GameplayEffects)
	{
		for (const FRpgItemGameplayEffectGrant& GameplayEffectGrant : GameplayEffects)
		{
			if (GameplayEffectGrant.GameplayEffect == nullptr)
			{
				continue;
			}

			FRpgItemGameplayEffectGrantKey Key;
			Key.GameplayEffect = GameplayEffectGrant.GameplayEffect;
			Key.EffectLevel = GameplayEffectGrant.EffectLevel;
			Snapshot.GameplayEffects.FindOrAdd(Key) += 1;
		}
	}

	void AddDesiredLooseTags(FRpgDesiredGrantSnapshot& Snapshot, const FGameplayTagContainer& LooseTags)
	{
		for (const FGameplayTag& LooseTag : LooseTags)
		{
			Snapshot.LooseTags.FindOrAdd(LooseTag) += 1;
		}
	}
}

URpgEquipmentComponent::URpgEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	WeaponSets.SetNum(2);
}

void URpgEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, KnownItemInstances);
	DOREPLIFETIME(ThisClass, WeaponSets);
	DOREPLIFETIME(ThisClass, ActiveWeaponSetIndex);
}

bool URpgEquipmentComponent::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool bWroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);
	for (URpgItemInstance* ItemInstance : KnownItemInstances)
	{
		if (ItemInstance != nullptr)
		{
			bWroteSomething |= Channel->ReplicateSubobject(ItemInstance, *Bunch, *RepFlags);
		}
	}

	return bWroteSomething;
}

void URpgEquipmentComponent::SetEquipmentRuleset(const URpgEquipmentRuleset* InRuleset)
{
	const TArray<FRpgEquippedWeaponSet> PreviousWeaponSets = WeaponSets;
	const int32 PreviousActiveWeaponSetIndex = ActiveWeaponSetIndex;

	EquipmentRuleset = InRuleset;
	EnsureWeaponSetCount();

	if (HasAuthorityForEquipment() && (!AreWeaponSetsEqual(PreviousWeaponSets, WeaponSets) || PreviousActiveWeaponSetIndex != ActiveWeaponSetIndex))
	{
		HandleEquipmentStateChanged(PreviousWeaponSets, PreviousActiveWeaponSetIndex);
	}
}

URpgItemInstance* URpgEquipmentComponent::CreateItemInstance(URpgItemDefinition* ItemDefinition, const FRpgItemSourceHandle& SourceHandle)
{
	if (ItemDefinition == nullptr)
	{
		return nullptr;
	}

	UObject* DesiredOuter = GetOwner() ? static_cast<UObject*>(GetOwner()) : static_cast<UObject*>(this);
	URpgItemInstance* NewItemInstance = NewObject<URpgItemInstance>(DesiredOuter);
	NewItemInstance->InitializeItemInstance(ItemDefinition, SourceHandle);
	return RegisterExistingItemInstance(NewItemInstance);
}

URpgItemInstance* URpgEquipmentComponent::RegisterExistingItemInstance(URpgItemInstance* ItemInstance)
{
	if (ItemInstance == nullptr)
	{
		return nullptr;
	}

	if (URpgItemInstance* ExistingItem = FindKnownItemById(ItemInstance->GetInstanceId()))
	{
		return ExistingItem;
	}

	UObject* DesiredOuter = GetOwner() ? static_cast<UObject*>(GetOwner()) : static_cast<UObject*>(this);
	URpgItemInstance* ManagedItem = (ItemInstance->GetOuter() == DesiredOuter)
		? ItemInstance
		: ItemInstance->DuplicateItemInstance(DesiredOuter);

	if (ManagedItem == nullptr)
	{
		return nullptr;
	}

	KnownItemInstances.Add(ManagedItem);
	ForceOwnerNetUpdate();
	return ManagedItem;
}

bool URpgEquipmentComponent::CanEquipItem(const URpgItemInstance* ItemInstance, FGameplayTag SlotTag) const
{
	if (ItemInstance == nullptr)
	{
		return false;
	}

	TArray<FRpgEquippedWeaponSet> ProposedWeaponSets = WeaponSets;
	EnsureWeaponSetCount(ProposedWeaponSets);
	return BuildProposedEquipState(const_cast<URpgItemInstance*>(ItemInstance), SlotTag, ProposedWeaponSets);
}

bool URpgEquipmentComponent::TryEquipItem(URpgItemInstance* ItemInstance, FGameplayTag SlotTag)
{
	if (ItemInstance == nullptr)
	{
		return false;
	}

	if (!HasAuthorityForEquipment())
	{
		ServerTryEquipItem(ItemInstance, SlotTag);
		return true;
	}

	URpgItemInstance* ManagedItem = RegisterExistingItemInstance(ItemInstance);
	if (ManagedItem == nullptr)
	{
		return false;
	}

	TArray<FRpgEquippedWeaponSet> ProposedWeaponSets = WeaponSets;
	EnsureWeaponSetCount(ProposedWeaponSets);
	if (!BuildProposedEquipState(ManagedItem, SlotTag, ProposedWeaponSets))
	{
		return false;
	}

	const TArray<FRpgEquippedWeaponSet> PreviousWeaponSets = WeaponSets;
	const int32 PreviousActiveWeaponSetIndex = ActiveWeaponSetIndex;
	WeaponSets = MoveTemp(ProposedWeaponSets);

	if (!AreWeaponSetsEqual(PreviousWeaponSets, WeaponSets))
	{
		HandleEquipmentStateChanged(PreviousWeaponSets, PreviousActiveWeaponSetIndex);
	}

	return true;
}

bool URpgEquipmentComponent::TryAutoEquipItem(URpgItemInstance* ItemInstance)
{
	if (ItemInstance == nullptr)
	{
		return false;
	}

	const int32 NumWeaponSets = GetDesiredWeaponSetCount();
	TArray<int32> CandidateWeaponSetIndices;
	if (ActiveWeaponSetIndex != INDEX_NONE && ActiveWeaponSetIndex >= 0 && ActiveWeaponSetIndex < NumWeaponSets)
	{
		CandidateWeaponSetIndices.Add(ActiveWeaponSetIndex);
	}

	for (int32 WeaponSetIndex = 0; WeaponSetIndex < NumWeaponSets; ++WeaponSetIndex)
	{
		if (!CandidateWeaponSetIndices.Contains(WeaponSetIndex))
		{
			CandidateWeaponSetIndices.Add(WeaponSetIndex);
		}
	}

	TArray<FGameplayTag> CandidateSlots;
	CandidateSlots.Reserve(CandidateWeaponSetIndices.Num() * 2);
	for (const int32 WeaponSetIndex : CandidateWeaponSetIndices)
	{
		CandidateSlots.Add(MakeSlotTag(WeaponSetIndex, ERpgEquipmentHandSlot::MainHand));
		CandidateSlots.Add(MakeSlotTag(WeaponSetIndex, ERpgEquipmentHandSlot::OffHand));
	}

	for (const FGameplayTag& SlotTag : CandidateSlots)
	{
		if (GetItemInSlot(SlotTag) != nullptr)
		{
			continue;
		}

		if (CanEquipItem(ItemInstance, SlotTag))
		{
			return TryEquipItem(ItemInstance, SlotTag);
		}
	}

	return false;
}

bool URpgEquipmentComponent::TryUnequipItem(FGameplayTag SlotTag)
{
	if (!HasAuthorityForEquipment())
	{
		ServerTryUnequipItem(SlotTag);
		return true;
	}

	int32 WeaponSetIndex = INDEX_NONE;
	ERpgEquipmentHandSlot HandSlot = ERpgEquipmentHandSlot::MainHand;
	if (!ResolveSlotTag(SlotTag, WeaponSetIndex, HandSlot))
	{
		return false;
	}

	EnsureWeaponSetCount();
	if (!WeaponSets.IsValidIndex(WeaponSetIndex))
	{
		return false;
	}

	const TArray<FRpgEquippedWeaponSet> PreviousWeaponSets = WeaponSets;
	const int32 PreviousActiveWeaponSetIndex = ActiveWeaponSetIndex;

	const URpgEquipmentRuleset* Ruleset = EquipmentRuleset ? EquipmentRuleset.Get() : GetDefault<URpgEquipmentRuleset>();
	FRpgEquippedWeaponSet& WeaponSet = WeaponSets[WeaponSetIndex];
	if (HandSlot == ERpgEquipmentHandSlot::MainHand)
	{
		if (WeaponSet.MainHandItem == nullptr && (WeaponSet.OffHandItem == nullptr || (Ruleset != nullptr && Ruleset->AllowsOffHandWithoutMainHand())))
		{
			return false;
		}

		WeaponSet.MainHandItem = nullptr;
		if (Ruleset == nullptr || !Ruleset->AllowsOffHandWithoutMainHand())
		{
			WeaponSet.OffHandItem = nullptr;
		}
	}
	else
	{
		if (WeaponSet.OffHandItem == nullptr)
		{
			return false;
		}

		WeaponSet.OffHandItem = nullptr;
	}

	HandleEquipmentStateChanged(PreviousWeaponSets, PreviousActiveWeaponSetIndex);
	return true;
}

bool URpgEquipmentComponent::SetActiveWeaponSet(int32 WeaponSetIndex)
{
	if (!HasAuthorityForEquipment())
	{
		ServerSetActiveWeaponSet(WeaponSetIndex);
		return true;
	}

	if (WeaponSetIndex < 0 || WeaponSetIndex >= GetDesiredWeaponSetCount())
	{
		return false;
	}

	EnsureWeaponSetCount();
	if (ActiveWeaponSetIndex == WeaponSetIndex)
	{
		return false;
	}

	const TArray<FRpgEquippedWeaponSet> PreviousWeaponSets = WeaponSets;
	const int32 PreviousActiveWeaponSetIndex = ActiveWeaponSetIndex;
	ActiveWeaponSetIndex = WeaponSetIndex;
	HandleEquipmentStateChanged(PreviousWeaponSets, PreviousActiveWeaponSetIndex);
	return true;
}

bool URpgEquipmentComponent::ClearActiveWeaponSet()
{
	if (!HasAuthorityForEquipment())
	{
		ServerClearActiveWeaponSet();
		return true;
	}

	EnsureWeaponSetCount();
	if (ActiveWeaponSetIndex == INDEX_NONE)
	{
		return false;
	}

	const TArray<FRpgEquippedWeaponSet> PreviousWeaponSets = WeaponSets;
	const int32 PreviousActiveWeaponSetIndex = ActiveWeaponSetIndex;
	ActiveWeaponSetIndex = INDEX_NONE;
	HandleEquipmentStateChanged(PreviousWeaponSets, PreviousActiveWeaponSetIndex);
	return true;
}

FRpgEquippedWeaponSet URpgEquipmentComponent::GetActiveWeaponSet() const
{
	return GetWeaponSet(ActiveWeaponSetIndex);
}

FRpgEquippedWeaponSet URpgEquipmentComponent::GetWeaponSet(int32 WeaponSetIndex) const
{
	if (WeaponSets.IsValidIndex(WeaponSetIndex))
	{
		return WeaponSets[WeaponSetIndex];
	}

	return FRpgEquippedWeaponSet();
}

void URpgEquipmentComponent::GetEquippedItems(TArray<URpgItemInstance*>& OutItems) const
{
	OutItems.Reset();

	for (const FRpgEquippedWeaponSet& WeaponSet : WeaponSets)
	{
		if (WeaponSet.MainHandItem != nullptr)
		{
			OutItems.AddUnique(WeaponSet.MainHandItem);
		}

		if (WeaponSet.OffHandItem != nullptr)
		{
			OutItems.AddUnique(WeaponSet.OffHandItem);
		}
	}
}

URpgItemInstance* URpgEquipmentComponent::GetItemInSlot(FGameplayTag SlotTag) const
{
	int32 WeaponSetIndex = INDEX_NONE;
	ERpgEquipmentHandSlot HandSlot = ERpgEquipmentHandSlot::MainHand;
	if (!ResolveSlotTag(SlotTag, WeaponSetIndex, HandSlot) || !WeaponSets.IsValidIndex(WeaponSetIndex))
	{
		return nullptr;
	}

	return (HandSlot == ERpgEquipmentHandSlot::MainHand)
		? WeaponSets[WeaponSetIndex].MainHandItem
		: WeaponSets[WeaponSetIndex].OffHandItem;
}

void URpgEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureWeaponSetCount();
	LastNotifiedActiveWeaponSetIndex = ActiveWeaponSetIndex;
}

void URpgEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveAllAppliedGrants();
	Super::EndPlay(EndPlayReason);
}

void URpgEquipmentComponent::OnRep_WeaponSets()
{
	FRpgEquipmentStateChangedEvent Event;
	Event.PreviousActiveWeaponSetIndex = LastNotifiedActiveWeaponSetIndex;
	Event.NewActiveWeaponSetIndex = LastNotifiedActiveWeaponSetIndex;
	Event.bWeaponSlotsChanged = true;
	BroadcastStateChangedNative(Event);
	OnEquipmentChanged.Broadcast();
}

void URpgEquipmentComponent::OnRep_ActiveWeaponSetIndex()
{
	FRpgEquipmentStateChangedEvent Event;
	Event.PreviousActiveWeaponSetIndex = LastNotifiedActiveWeaponSetIndex;
	Event.NewActiveWeaponSetIndex = ActiveWeaponSetIndex;
	Event.bWeaponSlotsChanged = false;
	LastNotifiedActiveWeaponSetIndex = ActiveWeaponSetIndex;
	BroadcastStateChangedNative(Event);
	OnEquipmentChanged.Broadcast();
}

void URpgEquipmentComponent::ServerTryEquipItem_Implementation(URpgItemInstance* ItemInstance, FGameplayTag SlotTag)
{
	TryEquipItem(ItemInstance, SlotTag);
}

void URpgEquipmentComponent::ServerTryUnequipItem_Implementation(FGameplayTag SlotTag)
{
	TryUnequipItem(SlotTag);
}

void URpgEquipmentComponent::ServerSetActiveWeaponSet_Implementation(int32 WeaponSetIndex)
{
	SetActiveWeaponSet(WeaponSetIndex);
}

void URpgEquipmentComponent::ServerClearActiveWeaponSet_Implementation()
{
	ClearActiveWeaponSet();
}

bool URpgEquipmentComponent::HasAuthorityForEquipment() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor == nullptr || OwnerActor->HasAuthority();
}

int32 URpgEquipmentComponent::GetDesiredWeaponSetCount() const
{
	const URpgEquipmentRuleset* Ruleset = EquipmentRuleset ? EquipmentRuleset.Get() : GetDefault<URpgEquipmentRuleset>();
	return Ruleset ? Ruleset->GetNumWeaponSets() : 2;
}

void URpgEquipmentComponent::EnsureWeaponSetCount()
{
	EnsureWeaponSetCount(WeaponSets);
	if (ActiveWeaponSetIndex != INDEX_NONE && (ActiveWeaponSetIndex < 0 || ActiveWeaponSetIndex >= WeaponSets.Num()))
	{
		ActiveWeaponSetIndex = INDEX_NONE;
	}
}

void URpgEquipmentComponent::EnsureWeaponSetCount(TArray<FRpgEquippedWeaponSet>& InOutWeaponSets) const
{
	InOutWeaponSets.SetNum(GetDesiredWeaponSetCount());
}

bool URpgEquipmentComponent::ResolveSlotTag(const FGameplayTag& SlotTag, int32& OutWeaponSetIndex, ERpgEquipmentHandSlot& OutHandSlot) const
{
	OutWeaponSetIndex = INDEX_NONE;
	OutHandSlot = ERpgEquipmentHandSlot::MainHand;

	const URpgEquipmentRuleset* Ruleset = EquipmentRuleset ? EquipmentRuleset.Get() : GetDefault<URpgEquipmentRuleset>();
	if (Ruleset == nullptr || !Ruleset->IsWeaponSetSlot(SlotTag))
	{
		return false;
	}

	if (SlotTag == RpgGameplayTags::Equipment_Slot_WeaponSet_1_MainHand)
	{
		OutWeaponSetIndex = 0;
		OutHandSlot = ERpgEquipmentHandSlot::MainHand;
		return true;
	}

	if (SlotTag == RpgGameplayTags::Equipment_Slot_WeaponSet_1_OffHand)
	{
		OutWeaponSetIndex = 0;
		OutHandSlot = ERpgEquipmentHandSlot::OffHand;
		return true;
	}

	if (SlotTag == RpgGameplayTags::Equipment_Slot_WeaponSet_2_MainHand)
	{
		OutWeaponSetIndex = 1;
		OutHandSlot = ERpgEquipmentHandSlot::MainHand;
		return true;
	}

	if (SlotTag == RpgGameplayTags::Equipment_Slot_WeaponSet_2_OffHand)
	{
		OutWeaponSetIndex = 1;
		OutHandSlot = ERpgEquipmentHandSlot::OffHand;
		return true;
	}

	return false;
}

FGameplayTag URpgEquipmentComponent::MakeSlotTag(int32 WeaponSetIndex, ERpgEquipmentHandSlot HandSlot) const
{
	if (WeaponSetIndex <= 0)
	{
		return HandSlot == ERpgEquipmentHandSlot::MainHand
			? RpgGameplayTags::Equipment_Slot_WeaponSet_1_MainHand
			: RpgGameplayTags::Equipment_Slot_WeaponSet_1_OffHand;
	}

	return HandSlot == ERpgEquipmentHandSlot::MainHand
		? RpgGameplayTags::Equipment_Slot_WeaponSet_2_MainHand
		: RpgGameplayTags::Equipment_Slot_WeaponSet_2_OffHand;
}

bool URpgEquipmentComponent::BuildProposedEquipState(URpgItemInstance* ItemInstance, const FGameplayTag& SlotTag, TArray<FRpgEquippedWeaponSet>& InOutWeaponSets) const
{
	const URpgEquipmentRuleset* Ruleset = EquipmentRuleset ? EquipmentRuleset.Get() : GetDefault<URpgEquipmentRuleset>();
	if (ItemInstance == nullptr || Ruleset == nullptr)
	{
		return false;
	}

	const URpgItemFragment_Equipment* EquipmentFragment = ItemInstance->FindFragmentByClass<URpgItemFragment_Equipment>();
	if (EquipmentFragment == nullptr || ItemInstance->FindFragmentByClass<URpgItemFragment_Weapon>() == nullptr)
	{
		return false;
	}

	int32 WeaponSetIndex = INDEX_NONE;
	ERpgEquipmentHandSlot HandSlot = ERpgEquipmentHandSlot::MainHand;
	if (!ResolveSlotTag(SlotTag, WeaponSetIndex, HandSlot) || !InOutWeaponSets.IsValidIndex(WeaponSetIndex))
	{
		return false;
	}

	if (!Ruleset->DoesItemFitSlot(ItemInstance, SlotTag))
	{
		return false;
	}

	if (!EquipmentFragment->GetSupportedSlotTags().IsEmpty() && !EquipmentFragment->GetSupportedSlotTags().HasTagExact(SlotTag))
	{
		return false;
	}

	StripItemFromWeaponSets(ItemInstance, InOutWeaponSets);

	FRpgEquippedWeaponSet& TargetWeaponSet = InOutWeaponSets[WeaponSetIndex];
	if (HandSlot == ERpgEquipmentHandSlot::MainHand)
	{
		TargetWeaponSet.MainHandItem = ItemInstance;

		if (Ruleset->IsTwoHanded(ItemInstance))
		{
			TargetWeaponSet.OffHandItem = nullptr;
		}
		else if (TargetWeaponSet.OffHandItem != nullptr && !Ruleset->AreItemsCompatible(ItemInstance, TargetWeaponSet.OffHandItem))
		{
			TargetWeaponSet.OffHandItem = nullptr;
		}
	}
	else
	{
		if (TargetWeaponSet.MainHandItem == nullptr && !Ruleset->AllowsOffHandWithoutMainHand())
		{
			return false;
		}

		TargetWeaponSet.OffHandItem = ItemInstance;
	}

	return ValidateWeaponSets(InOutWeaponSets);
}

bool URpgEquipmentComponent::ValidateWeaponSets(const TArray<FRpgEquippedWeaponSet>& WeaponSetStates) const
{
	const URpgEquipmentRuleset* Ruleset = EquipmentRuleset ? EquipmentRuleset.Get() : GetDefault<URpgEquipmentRuleset>();
	if (Ruleset == nullptr)
	{
		return false;
	}

	if (Ruleset->IsTwoHandedCarryLimitEnabled() && CountTwoHandedItems(WeaponSetStates) > 1)
	{
		return false;
	}

	for (int32 WeaponSetIndex = 0; WeaponSetIndex < WeaponSetStates.Num(); ++WeaponSetIndex)
	{
		const FRpgEquippedWeaponSet& WeaponSet = WeaponSetStates[WeaponSetIndex];

		if (WeaponSet.MainHandItem != nullptr && !Ruleset->DoesItemFitSlot(WeaponSet.MainHandItem, MakeSlotTag(WeaponSetIndex, ERpgEquipmentHandSlot::MainHand)))
		{
			return false;
		}

		if (WeaponSet.OffHandItem != nullptr && !Ruleset->DoesItemFitSlot(WeaponSet.OffHandItem, MakeSlotTag(WeaponSetIndex, ERpgEquipmentHandSlot::OffHand)))
		{
			return false;
		}

		if (WeaponSet.MainHandItem != nullptr && Ruleset->IsTwoHanded(WeaponSet.MainHandItem) && WeaponSet.OffHandItem != nullptr)
		{
			return false;
		}

		if (!Ruleset->AreItemsCompatible(WeaponSet.MainHandItem, WeaponSet.OffHandItem))
		{
			return false;
		}
	}

	return true;
}

void URpgEquipmentComponent::StripItemFromWeaponSets(URpgItemInstance* ItemInstance, TArray<FRpgEquippedWeaponSet>& InOutWeaponSets) const
{
	if (ItemInstance == nullptr)
	{
		return;
	}

	for (FRpgEquippedWeaponSet& WeaponSet : InOutWeaponSets)
	{
		if (WeaponSet.MainHandItem == ItemInstance)
		{
			WeaponSet.MainHandItem = nullptr;
		}

		if (WeaponSet.OffHandItem == ItemInstance)
		{
			WeaponSet.OffHandItem = nullptr;
		}
	}
}

bool URpgEquipmentComponent::IsItemInActiveWeaponSet(const URpgItemInstance* ItemInstance) const
{
	if (ItemInstance == nullptr || !WeaponSets.IsValidIndex(ActiveWeaponSetIndex))
	{
		return false;
	}

	const FRpgEquippedWeaponSet& ActiveWeaponSet = WeaponSets[ActiveWeaponSetIndex];
	return ActiveWeaponSet.MainHandItem == ItemInstance || ActiveWeaponSet.OffHandItem == ItemInstance;
}

int32 URpgEquipmentComponent::CountTwoHandedItems(const TArray<FRpgEquippedWeaponSet>& WeaponSetStates) const
{
	const URpgEquipmentRuleset* Ruleset = EquipmentRuleset ? EquipmentRuleset.Get() : GetDefault<URpgEquipmentRuleset>();
	if (Ruleset == nullptr)
	{
		return 0;
	}

	int32 Result = 0;
	for (const FRpgEquippedWeaponSet& WeaponSet : WeaponSetStates)
	{
		if (WeaponSet.MainHandItem != nullptr && Ruleset->IsTwoHanded(WeaponSet.MainHandItem))
		{
			++Result;
		}
	}

	return Result;
}

URpgItemInstance* URpgEquipmentComponent::FindKnownItemById(const FGuid& InstanceId) const
{
	if (!InstanceId.IsValid())
	{
		return nullptr;
	}

	for (URpgItemInstance* KnownItem : KnownItemInstances)
	{
		if (KnownItem != nullptr && KnownItem->GetInstanceId() == InstanceId)
		{
			return KnownItem;
		}
	}

	return nullptr;
}

void URpgEquipmentComponent::HandleEquipmentStateChanged(const TArray<FRpgEquippedWeaponSet>& PreviousWeaponSets, int32 PreviousActiveWeaponSetIndex)
{
	EnsureWeaponSetCount();
	ReconcileAppliedGrants();
	CompactKnownItemInstances();
	ForceOwnerNetUpdate();

	FRpgEquipmentStateChangedEvent Event;
	Event.PreviousActiveWeaponSetIndex = PreviousActiveWeaponSetIndex;
	Event.NewActiveWeaponSetIndex = ActiveWeaponSetIndex;
	Event.bWeaponSlotsChanged = !AreWeaponSetsEqual(PreviousWeaponSets, WeaponSets);
	LastNotifiedActiveWeaponSetIndex = ActiveWeaponSetIndex;
	BroadcastStateChangedNative(Event);
	OnEquipmentChanged.Broadcast();
}

void URpgEquipmentComponent::RemoveAllAppliedGrants()
{
	if (URpgAbilitySystemComponent* AbilitySystemComponent = ResolveAbilitySystemComponent())
	{
		for (TPair<const URpgAbilitySet*, FRpgAbilitySet_GrantedHandles>& AppliedAbilitySetPair : AppliedAbilitySetHandles)
		{
			AppliedAbilitySetPair.Value.TakeFromAbilitySystem(AbilitySystemComponent);
		}

		for (TPair<FRpgItemGameplayEffectGrantKey, TArray<FActiveGameplayEffectHandle>>& AppliedGameplayEffectPair : AppliedGameplayEffectHandles)
		{
			for (const FActiveGameplayEffectHandle& EffectHandle : AppliedGameplayEffectPair.Value)
			{
				if (EffectHandle.IsValid())
				{
					AbilitySystemComponent->RemoveActiveGameplayEffect(EffectHandle);
				}
			}
		}

		for (const TPair<FGameplayTag, int32>& TagCountPair : AppliedLooseTagCounts)
		{
			for (int32 CountIndex = 0; CountIndex < TagCountPair.Value; ++CountIndex)
			{
				AbilitySystemComponent->RemoveLooseGameplayTag(TagCountPair.Key);
			}
		}
	}

	AppliedAbilitySetHandles.Reset();
	AppliedAbilitySetSourceObjects.Reset();
	AppliedGameplayEffectHandles.Reset();
	AppliedLooseTagCounts.Reset();
}

void URpgEquipmentComponent::ReconcileAppliedGrants()
{
	if (!HasAuthorityForEquipment())
	{
		return;
	}

	URpgAbilitySystemComponent* AbilitySystemComponent = ResolveAbilitySystemComponent();
	if (AbilitySystemComponent == nullptr)
	{
		return;
	}

	FRpgDesiredGrantSnapshot DesiredSnapshot;
	TArray<URpgItemInstance*> EquippedItems;
	GetEquippedItems(EquippedItems);

	for (URpgItemInstance* ItemInstance : EquippedItems)
	{
		if (const URpgItemFragment_Equipment* EquipmentFragment = ItemInstance ? ItemInstance->FindFragmentByClass<URpgItemFragment_Equipment>() : nullptr)
		{
			AddDesiredAbilitySets(DesiredSnapshot, EquipmentFragment->GetEquippedAbilitySets(), ItemInstance);
			AddDesiredGameplayEffects(DesiredSnapshot, EquipmentFragment->GetEquippedGameplayEffects());
			AddDesiredLooseTags(DesiredSnapshot, EquipmentFragment->GetEquippedLooseTags());
		}

		if (IsItemInActiveWeaponSet(ItemInstance))
		{
			if (const URpgItemFragment_Weapon* WeaponFragment = ItemInstance->FindFragmentByClass<URpgItemFragment_Weapon>())
			{
				AddDesiredAbilitySets(DesiredSnapshot, WeaponFragment->GetActiveAbilitySets(), ItemInstance);
				AddDesiredGameplayEffects(DesiredSnapshot, WeaponFragment->GetActiveGameplayEffects());
				AddDesiredLooseTags(DesiredSnapshot, WeaponFragment->GetActiveLooseTags());
			}
		}
	}

	for (auto AppliedAbilitySetIt = AppliedAbilitySetHandles.CreateIterator(); AppliedAbilitySetIt; ++AppliedAbilitySetIt)
	{
		const URpgAbilitySet* AbilitySet = AppliedAbilitySetIt.Key();
		const FRpgDesiredAbilitySetGrant* DesiredGrant = DesiredSnapshot.AbilitySets.Find(AbilitySet);
		const UObject* AppliedSourceObject = AppliedAbilitySetSourceObjects.FindRef(AbilitySet).Get();
		const UObject* DesiredSourceObject = DesiredGrant ? DesiredGrant->SourceObject.Get() : nullptr;
		const bool bSourceChanged = DesiredGrant != nullptr && AppliedSourceObject != DesiredSourceObject;
		if (DesiredGrant == nullptr || DesiredGrant->Count <= 0 || bSourceChanged)
		{
			AppliedAbilitySetIt.Value().TakeFromAbilitySystem(AbilitySystemComponent);
			AppliedAbilitySetSourceObjects.Remove(AbilitySet);
			AppliedAbilitySetIt.RemoveCurrent();
		}
	}

	for (const TPair<const URpgAbilitySet*, FRpgDesiredAbilitySetGrant>& DesiredAbilitySetPair : DesiredSnapshot.AbilitySets)
	{
		if (DesiredAbilitySetPair.Key == nullptr || AppliedAbilitySetHandles.Contains(DesiredAbilitySetPair.Key))
		{
			continue;
		}

		FRpgAbilitySet_GrantedHandles Handles;
		DesiredAbilitySetPair.Key->GiveToAbilitySystem(AbilitySystemComponent, &Handles, DesiredAbilitySetPair.Value.SourceObject.Get());
		AppliedAbilitySetHandles.Add(DesiredAbilitySetPair.Key, Handles);
		AppliedAbilitySetSourceObjects.Add(DesiredAbilitySetPair.Key, DesiredAbilitySetPair.Value.SourceObject);
	}

	for (auto AppliedGameplayEffectIt = AppliedGameplayEffectHandles.CreateIterator(); AppliedGameplayEffectIt; ++AppliedGameplayEffectIt)
	{
		const int32 DesiredCount = DesiredSnapshot.GameplayEffects.FindRef(AppliedGameplayEffectIt.Key());
		TArray<FActiveGameplayEffectHandle>& EffectHandles = AppliedGameplayEffectIt.Value();
		while (EffectHandles.Num() > DesiredCount)
		{
			const FActiveGameplayEffectHandle EffectHandle = EffectHandles.Pop(EAllowShrinking::No);
			if (EffectHandle.IsValid())
			{
				AbilitySystemComponent->RemoveActiveGameplayEffect(EffectHandle);
			}
		}

		if (DesiredCount <= 0)
		{
			AppliedGameplayEffectIt.RemoveCurrent();
		}
	}

	for (const TPair<FRpgItemGameplayEffectGrantKey, int32>& DesiredGameplayEffectPair : DesiredSnapshot.GameplayEffects)
	{
		TArray<FActiveGameplayEffectHandle>& EffectHandles = AppliedGameplayEffectHandles.FindOrAdd(DesiredGameplayEffectPair.Key);
		while (EffectHandles.Num() < DesiredGameplayEffectPair.Value)
		{
			const UGameplayEffect* GameplayEffect = DesiredGameplayEffectPair.Key.GameplayEffect->GetDefaultObject<UGameplayEffect>();
			const FActiveGameplayEffectHandle EffectHandle = AbilitySystemComponent->ApplyGameplayEffectToSelf(
				GameplayEffect,
				DesiredGameplayEffectPair.Key.EffectLevel,
				AbilitySystemComponent->MakeEffectContext());

			if (!EffectHandle.IsValid())
			{
				break;
			}

			EffectHandles.Add(EffectHandle);
		}
	}

	for (auto AppliedLooseTagIt = AppliedLooseTagCounts.CreateIterator(); AppliedLooseTagIt; ++AppliedLooseTagIt)
	{
		const int32 DesiredCount = DesiredSnapshot.LooseTags.FindRef(AppliedLooseTagIt.Key());
		while (AppliedLooseTagIt.Value() > DesiredCount)
		{
			AbilitySystemComponent->RemoveLooseGameplayTag(AppliedLooseTagIt.Key());
			AppliedLooseTagIt.Value() -= 1;
		}

		if (AppliedLooseTagIt.Value() <= 0)
		{
			AppliedLooseTagIt.RemoveCurrent();
		}
	}

	for (const TPair<FGameplayTag, int32>& DesiredLooseTagPair : DesiredSnapshot.LooseTags)
	{
		int32& AppliedCount = AppliedLooseTagCounts.FindOrAdd(DesiredLooseTagPair.Key);
		while (AppliedCount < DesiredLooseTagPair.Value)
		{
			AbilitySystemComponent->AddLooseGameplayTag(DesiredLooseTagPair.Key);
			AppliedCount += 1;
		}
	}
}

void URpgEquipmentComponent::CompactKnownItemInstances()
{
	TSet<const URpgItemInstance*> ReferencedItems;
	for (const FRpgEquippedWeaponSet& WeaponSet : WeaponSets)
	{
		if (WeaponSet.MainHandItem != nullptr)
		{
			ReferencedItems.Add(WeaponSet.MainHandItem);
		}

		if (WeaponSet.OffHandItem != nullptr)
		{
			ReferencedItems.Add(WeaponSet.OffHandItem);
		}
	}

	KnownItemInstances.RemoveAll([&ReferencedItems](const TObjectPtr<URpgItemInstance>& KnownItem)
	{
		return KnownItem == nullptr || !ReferencedItems.Contains(KnownItem.Get());
	});
}

URpgAbilitySystemComponent* URpgEquipmentComponent::ResolveAbilitySystemComponent() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (AbilitySystemOverrideForTests != nullptr)
	{
		return AbilitySystemOverrideForTests;
	}
#endif

	if (const ARpgPlayerState* PlayerState = Cast<ARpgPlayerState>(GetOwner()))
	{
		return PlayerState->GetRpgAbilitySystemComponent();
	}

	return nullptr;
}

void URpgEquipmentComponent::ForceOwnerNetUpdate() const
{
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
}

void URpgEquipmentComponent::BroadcastStateChangedNative(const FRpgEquipmentStateChangedEvent& Event)
{
	EquipmentStateChangedNative.Broadcast(Event);
}

bool URpgEquipmentComponent::AreWeaponSetsEqual(const TArray<FRpgEquippedWeaponSet>& Left, const TArray<FRpgEquippedWeaponSet>& Right) const
{
	if (Left.Num() != Right.Num())
	{
		return false;
	}

	for (int32 Index = 0; Index < Left.Num(); ++Index)
	{
		if (Left[Index] != Right[Index])
		{
			return false;
		}
	}

	return true;
}
