#include "RpgEquipmentComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/ActorChannel.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffect.h"
#include "SurvivalRpg/GameplayTags/GameplayTags.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Items/RpgItemInstance.h"
#include "SurvivalRpg/Items/Fragments/RpgItemFragment_Equipment.h"
#include "SurvivalRpg/Items/Fragments/RpgItemFragment_Visual.h"
#include "SurvivalRpg/Items/Fragments/RpgItemFragment_Weapon.h"
#include "AnimNotify_RpgWeaponToolPresentation.h"
#include "RpgEquipmentRuleset.h"

URpgEquipmentComponent::URpgEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	PrimaryComponentTick.EndTickGroup = TG_PostUpdateWork;
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
	EquipmentRuleset = InRuleset;
	EnsureWeaponSetCount();
	RefreshActiveWeaponToolCharacterSettings();
	ApplyActiveWeaponToolCharacterSettings();
	RefreshActiveCameraSettings();
	ApplyVisibleWeaponToolPresentationSettings();
	QueueVisualRefresh();
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

	WeaponSets = MoveTemp(ProposedWeaponSets);
	HandleEquipmentStateChanged();
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
	const URpgEquipmentRuleset* Ruleset = EquipmentRuleset ? EquipmentRuleset.Get() : GetDefault<URpgEquipmentRuleset>();
	FRpgEquippedWeaponSet& WeaponSet = WeaponSets[WeaponSetIndex];
	if (HandSlot == ERpgEquipmentHandSlot::MainHand)
	{
		WeaponSet.MainHandItem = nullptr;
		if (Ruleset == nullptr || !Ruleset->AllowsOffHandWithoutMainHand())
		{
			WeaponSet.OffHandItem = nullptr;
		}
	}
	else
	{
		WeaponSet.OffHandItem = nullptr;
	}

	HandleEquipmentStateChanged();
	return true;
}

bool URpgEquipmentComponent::TryActivateWeaponSet(int32 WeaponSetIndex)
{
	if (!HasAuthorityForEquipment())
	{
		ServerTryActivateWeaponSet(WeaponSetIndex);
		return true;
	}

	if (WeaponSetIndex < 0 || WeaponSetIndex >= GetDesiredWeaponSetCount())
	{
		return false;
	}

	EnsureWeaponSetCount();
	ActiveWeaponSetIndex = (ActiveWeaponSetIndex == WeaponSetIndex) ? INDEX_NONE : WeaponSetIndex;
	HandleEquipmentStateChanged();
	return true;
}

FRpgEquippedWeaponSet URpgEquipmentComponent::GetActiveWeaponSet() const
{
	if (WeaponSets.IsValidIndex(ActiveWeaponSetIndex))
	{
		return WeaponSets[ActiveWeaponSetIndex];
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

void URpgEquipmentComponent::ApplyWeaponToolPresentationNotifyAction(ERpgWeaponToolPresentationNotifyAction Action)
{
	switch (Action)
	{
	case ERpgWeaponToolPresentationNotifyAction::ApplyCurrentState:
	case ERpgWeaponToolPresentationNotifyAction::DrawActiveSet:
		SetPresentationVisibleWeaponSetIndex(ActiveWeaponSetIndex);
		break;

	case ERpgWeaponToolPresentationNotifyAction::HolsterVisuals:
		SetPresentationVisibleWeaponSetIndex(INDEX_NONE);
		break;

	default:
		break;
	}
}

void URpgEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureWeaponSetCount();
	ObservedActiveWeaponSetIndex = ActiveWeaponSetIndex;
	PresentationVisibleWeaponSetIndex = ActiveWeaponSetIndex;
	RefreshActiveWeaponToolCharacterSettings();
	RefreshActiveCameraSettings();
	RefreshPresentationBindings();
	QueueVisualRefresh();
}

void URpgEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyAllVisualActors();
	RemoveAppliedGrants();
	Super::EndPlay(EndPlayReason);
}

void URpgEquipmentComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APawn* CurrentPawn = ResolveVisualPawn();
	if (CachedVisualPawn != CurrentPawn)
	{
		CachedVisualPawn = CurrentPawn;
		RefreshPresentationBindings();
		bVisualRefreshQueued = true;
	}

	UpdatePendingAnimClassSwitch();
	UpdateCameraBlend(DeltaTime);

	if (bVisualRefreshQueued)
	{
		RefreshVisuals();
	}
}

void URpgEquipmentComponent::OnRep_WeaponSets()
{
	QueueVisualRefresh();
	RefreshActiveWeaponToolCharacterSettings();
	ApplyActiveWeaponToolCharacterSettings();
	RefreshActiveCameraSettings();
	ApplyVisibleWeaponToolPresentationSettings();
	OnEquipmentChanged.Broadcast();
}

void URpgEquipmentComponent::OnRep_ActiveWeaponSetIndex()
{
	RefreshPresentationState(true);
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

void URpgEquipmentComponent::ServerTryActivateWeaponSet_Implementation(int32 WeaponSetIndex)
{
	TryActivateWeaponSet(WeaponSetIndex);
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

	if (PresentationVisibleWeaponSetIndex != INDEX_NONE && (PresentationVisibleWeaponSetIndex < 0 || PresentationVisibleWeaponSetIndex >= WeaponSets.Num()))
	{
		PresentationVisibleWeaponSetIndex = INDEX_NONE;
		QueueVisualRefresh();
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

bool URpgEquipmentComponent::IsItemInPresentationVisibleWeaponSet(const URpgItemInstance* ItemInstance) const
{
	if (ItemInstance == nullptr || !WeaponSets.IsValidIndex(PresentationVisibleWeaponSetIndex))
	{
		return false;
	}

	const FRpgEquippedWeaponSet& VisibleWeaponSet = WeaponSets[PresentationVisibleWeaponSetIndex];
	return VisibleWeaponSet.MainHandItem == ItemInstance || VisibleWeaponSet.OffHandItem == ItemInstance;
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

void URpgEquipmentComponent::HandleEquipmentStateChanged()
{
	EnsureWeaponSetCount();
	RemoveAppliedGrants();
	ApplyCurrentGrants();
	RefreshPresentationState(true);
	ForceOwnerNetUpdate();
	OnEquipmentChanged.Broadcast();
}

void URpgEquipmentComponent::RefreshPresentationState(bool bAllowMontage)
{
	const int32 PreviousActiveWeaponSetIndex = ObservedActiveWeaponSetIndex;

	RefreshActiveWeaponToolCharacterSettings();
	ApplyActiveWeaponToolCharacterSettings();
	QueueVisualRefresh();
	RefreshActiveCameraSettings();
	ApplyVisibleWeaponToolPresentationSettings();

	if (bAllowMontage && PreviousActiveWeaponSetIndex != ActiveWeaponSetIndex)
	{
		APawn* VisualPawn = ResolveVisualPawn();
		if (VisualPawn != nullptr && VisualPawn->GetNetMode() != NM_DedicatedServer)
		{
			const bool bUseEquipMontage = ActiveWeaponSetIndex != INDEX_NONE;
			const int32 MontageWeaponSetIndex = bUseEquipMontage ? ActiveWeaponSetIndex : PreviousActiveWeaponSetIndex;
			const bool bUsesNotifyDrivenPresentation = MontageUsesPresentationNotify(MontageWeaponSetIndex, bUseEquipMontage);
			const bool bPlayedMontage = PlayPresentationMontageForWeaponSet(MontageWeaponSetIndex, bUseEquipMontage);

			if (!bUsesNotifyDrivenPresentation || !bPlayedMontage)
			{
				SetPresentationVisibleWeaponSetIndex(ActiveWeaponSetIndex);
			}
		}
		else
		{
			SetPresentationVisibleWeaponSetIndex(ActiveWeaponSetIndex);
		}
	}

	ObservedActiveWeaponSetIndex = ActiveWeaponSetIndex;
}

void URpgEquipmentComponent::SetPresentationVisibleWeaponSetIndex(int32 InPresentationVisibleWeaponSetIndex)
{
	const int32 NewVisibleWeaponSetIndex = WeaponSets.IsValidIndex(InPresentationVisibleWeaponSetIndex)
		? InPresentationVisibleWeaponSetIndex
		: INDEX_NONE;

	if (PresentationVisibleWeaponSetIndex == NewVisibleWeaponSetIndex)
	{
		return;
	}

	PresentationVisibleWeaponSetIndex = NewVisibleWeaponSetIndex;
	RefreshActiveCameraSettings();
	ApplyVisibleWeaponToolPresentationSettings();
	QueueVisualRefresh();
}

void URpgEquipmentComponent::RemoveAppliedGrants()
{
	if (URpgAbilitySystemComponent* AbilitySystemComponent = ResolveAbilitySystemComponent())
	{
		for (FRpgAbilitySet_GrantedHandles& GrantedHandles : AppliedAbilitySetHandles)
		{
			GrantedHandles.TakeFromAbilitySystem(AbilitySystemComponent);
		}

		for (const FActiveGameplayEffectHandle& EffectHandle : AppliedGameplayEffectHandles)
		{
			if (EffectHandle.IsValid())
			{
				AbilitySystemComponent->RemoveActiveGameplayEffect(EffectHandle);
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
	AppliedGameplayEffectHandles.Reset();
	AppliedLooseTagCounts.Reset();
}

void URpgEquipmentComponent::ApplyCurrentGrants()
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

	TArray<URpgItemInstance*> EquippedItems;
	GetEquippedItems(EquippedItems);

	for (URpgItemInstance* ItemInstance : EquippedItems)
	{
		if (const URpgItemFragment_Equipment* EquipmentFragment = ItemInstance ? ItemInstance->FindFragmentByClass<URpgItemFragment_Equipment>() : nullptr)
		{
			ApplyAbilitySets(AbilitySystemComponent, EquipmentFragment->GetEquippedAbilitySets(), ItemInstance);
			ApplyGameplayEffects(AbilitySystemComponent, EquipmentFragment->GetEquippedGameplayEffects());
			ApplyLooseTags(AbilitySystemComponent, EquipmentFragment->GetEquippedLooseTags());
		}

		if (IsItemInActiveWeaponSet(ItemInstance))
		{
			if (const URpgItemFragment_Weapon* WeaponFragment = ItemInstance->FindFragmentByClass<URpgItemFragment_Weapon>())
			{
				ApplyAbilitySets(AbilitySystemComponent, WeaponFragment->GetActiveAbilitySets(), ItemInstance);
				ApplyGameplayEffects(AbilitySystemComponent, WeaponFragment->GetActiveGameplayEffects());
				ApplyLooseTags(AbilitySystemComponent, WeaponFragment->GetActiveLooseTags());
			}
		}
	}
}

void URpgEquipmentComponent::ApplyAbilitySets(URpgAbilitySystemComponent* AbilitySystemComponent, const TArray<TObjectPtr<const URpgAbilitySet>>& AbilitySets, UObject* SourceObject)
{
	if (AbilitySystemComponent == nullptr)
	{
		return;
	}

	for (const URpgAbilitySet* AbilitySet : AbilitySets)
	{
		if (AbilitySet == nullptr)
		{
			continue;
		}

		FRpgAbilitySet_GrantedHandles& Handles = AppliedAbilitySetHandles.AddDefaulted_GetRef();
		AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, &Handles, SourceObject);
	}
}

void URpgEquipmentComponent::ApplyGameplayEffects(URpgAbilitySystemComponent* AbilitySystemComponent, const TArray<FRpgItemGameplayEffectGrant>& GameplayEffects)
{
	if (AbilitySystemComponent == nullptr)
	{
		return;
	}

	for (const FRpgItemGameplayEffectGrant& EffectGrant : GameplayEffects)
	{
		if (EffectGrant.GameplayEffect == nullptr)
		{
			continue;
		}

		const UGameplayEffect* GameplayEffect = EffectGrant.GameplayEffect->GetDefaultObject<UGameplayEffect>();
		const FActiveGameplayEffectHandle EffectHandle = AbilitySystemComponent->ApplyGameplayEffectToSelf(
			GameplayEffect,
			EffectGrant.EffectLevel,
			AbilitySystemComponent->MakeEffectContext());

		if (EffectHandle.IsValid())
		{
			AppliedGameplayEffectHandles.Add(EffectHandle);
		}
	}
}

void URpgEquipmentComponent::ApplyLooseTags(URpgAbilitySystemComponent* AbilitySystemComponent, const FGameplayTagContainer& LooseTags)
{
	if (AbilitySystemComponent == nullptr)
	{
		return;
	}

	for (const FGameplayTag& LooseTag : LooseTags)
	{
		AbilitySystemComponent->AddLooseGameplayTag(LooseTag);
		AppliedLooseTagCounts.FindOrAdd(LooseTag) += 1;
	}
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

void URpgEquipmentComponent::QueueVisualRefresh()
{
	bVisualRefreshQueued = true;
}

void URpgEquipmentComponent::RefreshActiveWeaponToolCharacterSettings()
{
	FRpgWeaponToolCharacterSettings NewCharacterSettings;
	if (const URpgItemFragment_Visual* VisualFragment = GetPrimaryPresentationVisualFragmentForWeaponSet(ActiveWeaponSetIndex))
	{
		NewCharacterSettings = VisualFragment->GetWeaponToolCharacterSettings();
	}

	ActiveWeaponToolCharacterSettings = NewCharacterSettings;
}

void URpgEquipmentComponent::RefreshActiveCameraSettings()
{
	FRpgWeaponToolCameraSettings NewCameraSettings;
	if (const URpgItemFragment_Visual* VisualFragment = GetPrimaryPresentationVisualFragmentForWeaponSet(PresentationVisibleWeaponSetIndex))
	{
		NewCameraSettings = VisualFragment->GetWeaponToolCameraSettings();
	}

	if (ActiveCameraSettings == NewCameraSettings)
	{
		return;
	}

	ActiveCameraSettings = NewCameraSettings;
	OnActiveCameraSettingsChanged.Broadcast(ActiveCameraSettings);
}

void URpgEquipmentComponent::RefreshPresentationBindings()
{
	ResetPresentationBindings();

	APawn* VisualPawn = CachedVisualPawn ? CachedVisualPawn.Get() : ResolveVisualPawn();
	if (VisualPawn == nullptr || VisualPawn->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	CachedPresentationMesh = ResolvePresentationMesh(VisualPawn);
	CachedPresentationMovementComponent = ResolvePresentationMovementComponent(VisualPawn);
	CachedPresentationCameraComponent = ResolvePresentationCameraComponent(VisualPawn);
	CachedPresentationSpringArmComponent = ResolvePresentationSpringArmComponent(VisualPawn);

	if (CachedPresentationMesh != nullptr)
	{
		DefaultPresentationAnimClass = CachedPresentationMesh->GetAnimClass();
	}

	if (CachedPresentationMovementComponent != nullptr)
	{
		DefaultPresentationMaxWalkSpeed = CachedPresentationMovementComponent->MaxWalkSpeed;
		bDefaultPresentationOrientRotationToMovement = CachedPresentationMovementComponent->bOrientRotationToMovement;
		bDefaultPresentationUseControllerDesiredRotation = CachedPresentationMovementComponent->bUseControllerDesiredRotation;
	}

	if (CachedPresentationCameraComponent != nullptr)
	{
		DefaultPresentationCameraFOV = CachedPresentationCameraComponent->FieldOfView;
		AppliedPresentationCameraFOV = DefaultPresentationCameraFOV;
	}
	else
	{
		AppliedPresentationCameraFOV = DefaultPresentationCameraFOV;
	}

	if (CachedPresentationSpringArmComponent != nullptr)
	{
		DefaultPresentationSpringArmSocketOffset = CachedPresentationSpringArmComponent->SocketOffset;
		AppliedPresentationSpringArmSocketOffset = DefaultPresentationSpringArmSocketOffset;
	}
	else
	{
		AppliedPresentationSpringArmSocketOffset = DefaultPresentationSpringArmSocketOffset;
	}

	ApplyActiveWeaponToolCharacterSettings();
	ApplyVisibleWeaponToolPresentationSettings();
}

void URpgEquipmentComponent::ResetPresentationBindings()
{
	CachedPresentationMesh = nullptr;
	CachedPresentationMovementComponent = nullptr;
	CachedPresentationCameraComponent = nullptr;
	CachedPresentationSpringArmComponent = nullptr;
	DefaultPresentationAnimClass = nullptr;
	DefaultPresentationMaxWalkSpeed = 600.0f;
	bDefaultPresentationOrientRotationToMovement = true;
	bDefaultPresentationUseControllerDesiredRotation = false;
	DefaultPresentationCameraFOV = 90.0f;
	DefaultPresentationSpringArmSocketOffset = FVector::ZeroVector;
	PendingPresentationAnimClass = nullptr;
	AppliedPresentationCameraFOV = DefaultPresentationCameraFOV;
	AppliedPresentationSpringArmSocketOffset = DefaultPresentationSpringArmSocketOffset;
	PresentationCameraBlendStartFOV = DefaultPresentationCameraFOV;
	PresentationCameraBlendStartSpringArmSocketOffset = DefaultPresentationSpringArmSocketOffset;
	PresentationCameraBlendTargetFOV = DefaultPresentationCameraFOV;
	PresentationCameraBlendTargetSpringArmSocketOffset = DefaultPresentationSpringArmSocketOffset;
	PresentationCameraBlendDuration = 0.0f;
	PresentationCameraBlendElapsedTime = 0.0f;
	bHasPendingPresentationAnimClassSwitch = false;
	bPresentationCameraBlendActive = false;
}

void URpgEquipmentComponent::ApplyActiveWeaponToolCharacterSettings()
{
	APawn* VisualPawn = CachedVisualPawn ? CachedVisualPawn.Get() : ResolveVisualPawn();
	if (!ShouldApplyActiveWeaponToolCharacterSettingsToPawn(VisualPawn) || CachedPresentationMovementComponent == nullptr)
	{
		return;
	}

	const bool bUseOverride = ActiveWeaponToolCharacterSettings.bEnabled;
	CachedPresentationMovementComponent->MaxWalkSpeed = bUseOverride ? ActiveWeaponToolCharacterSettings.MaxWalkSpeed : DefaultPresentationMaxWalkSpeed;
	CachedPresentationMovementComponent->bOrientRotationToMovement = bUseOverride ? ActiveWeaponToolCharacterSettings.bOrientRotationToMovement : bDefaultPresentationOrientRotationToMovement;
	CachedPresentationMovementComponent->bUseControllerDesiredRotation = bUseOverride ? ActiveWeaponToolCharacterSettings.bUseControllerDesiredRotation : bDefaultPresentationUseControllerDesiredRotation;
}

void URpgEquipmentComponent::ApplyVisibleWeaponToolPresentationSettings()
{
	RefreshTargetVisiblePresentationState();
	StartOrUpdateCameraBlend();
}

void URpgEquipmentComponent::RefreshTargetVisiblePresentationState()
{
	FRpgWeaponToolCharacterSettings VisibleCharacterSettings;
	if (const URpgItemFragment_Visual* VisualFragment = GetPrimaryPresentationVisualFragmentForWeaponSet(PresentationVisibleWeaponSetIndex))
	{
		VisibleCharacterSettings = VisualFragment->GetWeaponToolCharacterSettings();
	}

	const TSubclassOf<UAnimInstance> DesiredAnimClass = (VisibleCharacterSettings.bEnabled && VisibleCharacterSettings.AnimClass != nullptr)
		? VisibleCharacterSettings.AnimClass
		: DefaultPresentationAnimClass;

	QueuePendingAnimClassSwitch(DesiredAnimClass);
}

void URpgEquipmentComponent::QueuePendingAnimClassSwitch(TSubclassOf<UAnimInstance> DesiredAnimClass)
{
	PendingPresentationAnimClass = DesiredAnimClass;
	bHasPendingPresentationAnimClassSwitch = CachedPresentationMesh != nullptr && CachedPresentationMesh->GetAnimClass() != DesiredAnimClass;
}

void URpgEquipmentComponent::StartOrUpdateCameraBlend()
{
	const float DesiredFOV = ActiveCameraSettings.bEnabled ? ActiveCameraSettings.FOV : DefaultPresentationCameraFOV;
	const FVector DesiredSocketOffset = ActiveCameraSettings.bEnabled ? ActiveCameraSettings.SpringArmSocketOffset : DefaultPresentationSpringArmSocketOffset;

	float DesiredBlendTime = ActiveCameraSettings.BlendTime;
	if (!ActiveCameraSettings.bEnabled && FMath::IsNearlyZero(DesiredBlendTime))
	{
		DesiredBlendTime = LastPresentationCameraBlendTime;
	}
	LastPresentationCameraBlendTime = DesiredBlendTime;

	PresentationCameraBlendTargetFOV = DesiredFOV;
	PresentationCameraBlendTargetSpringArmSocketOffset = DesiredSocketOffset;

	APawn* VisualPawn = CachedVisualPawn ? CachedVisualPawn.Get() : ResolveVisualPawn();
	if (!ShouldApplyVisibleWeaponToolCameraSettingsToPawn(VisualPawn))
	{
		bPresentationCameraBlendActive = false;
		AppliedPresentationCameraFOV = DesiredFOV;
		AppliedPresentationSpringArmSocketOffset = DesiredSocketOffset;
		return;
	}

	const float CurrentFOV = CachedPresentationCameraComponent != nullptr
		? CachedPresentationCameraComponent->FieldOfView
		: AppliedPresentationCameraFOV;
	const FVector CurrentSocketOffset = CachedPresentationSpringArmComponent != nullptr
		? CachedPresentationSpringArmComponent->SocketOffset
		: AppliedPresentationSpringArmSocketOffset;

	AppliedPresentationCameraFOV = CurrentFOV;
	AppliedPresentationSpringArmSocketOffset = CurrentSocketOffset;

	if (DesiredBlendTime <= 0.0f
		|| (FMath::IsNearlyEqual(CurrentFOV, DesiredFOV) && CurrentSocketOffset.Equals(DesiredSocketOffset)))
	{
		PresentationCameraBlendDuration = 0.0f;
		PresentationCameraBlendElapsedTime = 0.0f;
		bPresentationCameraBlendActive = false;
		PresentationCameraBlendStartFOV = DesiredFOV;
		PresentationCameraBlendStartSpringArmSocketOffset = DesiredSocketOffset;
		ApplyCameraBlendAlpha(1.0f);
		return;
	}

	PresentationCameraBlendStartFOV = CurrentFOV;
	PresentationCameraBlendStartSpringArmSocketOffset = CurrentSocketOffset;
	PresentationCameraBlendDuration = DesiredBlendTime;
	PresentationCameraBlendElapsedTime = 0.0f;
	bPresentationCameraBlendActive = true;
}

void URpgEquipmentComponent::UpdatePendingAnimClassSwitch()
{
	if (!bHasPendingPresentationAnimClassSwitch)
	{
		return;
	}

	APawn* VisualPawn = CachedVisualPawn ? CachedVisualPawn.Get() : ResolveVisualPawn();
	if (!ShouldApplyVisibleWeaponToolAnimClassToPawn(VisualPawn) || CachedPresentationMesh == nullptr)
	{
		return;
	}

	if (CachedPresentationMesh->GetAnimClass() == PendingPresentationAnimClass)
	{
		bHasPendingPresentationAnimClassSwitch = false;
		return;
	}

	UAnimInstance* CurrentAnimInstance = CachedPresentationMesh->GetAnimInstance();
	if (CachedPresentationMesh->IsRunningParallelEvaluation()
		|| (CurrentAnimInstance != nullptr
			&& (CurrentAnimInstance->IsRunningParallelEvaluation()
				|| CurrentAnimInstance->IsUpdatingAnimation()
				|| CurrentAnimInstance->IsPostUpdatingAnimation())))
	{
		return;
	}

	CachedPresentationMesh->SetAnimInstanceClass(PendingPresentationAnimClass);
	bHasPendingPresentationAnimClassSwitch = false;
}

void URpgEquipmentComponent::UpdateCameraBlend(float DeltaTime)
{
	if (!bPresentationCameraBlendActive)
	{
		return;
	}

	APawn* VisualPawn = CachedVisualPawn ? CachedVisualPawn.Get() : ResolveVisualPawn();
	if (!ShouldApplyVisibleWeaponToolCameraSettingsToPawn(VisualPawn))
	{
		bPresentationCameraBlendActive = false;
		return;
	}

	if (PresentationCameraBlendDuration <= 0.0f)
	{
		ApplyCameraBlendAlpha(1.0f);
		bPresentationCameraBlendActive = false;
		return;
	}

	PresentationCameraBlendElapsedTime = FMath::Min(PresentationCameraBlendElapsedTime + DeltaTime, PresentationCameraBlendDuration);
	const float LinearAlpha = FMath::Clamp(PresentationCameraBlendElapsedTime / PresentationCameraBlendDuration, 0.0f, 1.0f);
	const float EasedAlpha = LinearAlpha * LinearAlpha * (3.0f - (2.0f * LinearAlpha));
	ApplyCameraBlendAlpha(EasedAlpha);

	if (LinearAlpha >= 1.0f)
	{
		bPresentationCameraBlendActive = false;
	}
}

void URpgEquipmentComponent::ApplyCameraBlendAlpha(float BlendAlpha)
{
	const float NewFOV = FMath::Lerp(PresentationCameraBlendStartFOV, PresentationCameraBlendTargetFOV, BlendAlpha);
	const FVector NewSocketOffset = FMath::Lerp(PresentationCameraBlendStartSpringArmSocketOffset, PresentationCameraBlendTargetSpringArmSocketOffset, BlendAlpha);

	AppliedPresentationCameraFOV = NewFOV;
	AppliedPresentationSpringArmSocketOffset = NewSocketOffset;

	if (CachedPresentationCameraComponent != nullptr && !FMath::IsNearlyEqual(CachedPresentationCameraComponent->FieldOfView, NewFOV))
	{
		CachedPresentationCameraComponent->SetFieldOfView(NewFOV);
	}

	if (CachedPresentationSpringArmComponent != nullptr && !CachedPresentationSpringArmComponent->SocketOffset.Equals(NewSocketOffset))
	{
		CachedPresentationSpringArmComponent->SocketOffset = NewSocketOffset;
	}
}

bool URpgEquipmentComponent::ShouldApplyActiveWeaponToolCharacterSettingsToPawn(const APawn* VisualPawn) const
{
	if (VisualPawn == nullptr || VisualPawn->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	return HasAuthorityForEquipment() || VisualPawn->IsLocallyControlled();
}

bool URpgEquipmentComponent::ShouldApplyVisibleWeaponToolAnimClassToPawn(const APawn* VisualPawn) const
{
	return VisualPawn != nullptr && VisualPawn->GetNetMode() != NM_DedicatedServer;
}

bool URpgEquipmentComponent::ShouldApplyVisibleWeaponToolCameraSettingsToPawn(const APawn* VisualPawn) const
{
	return VisualPawn != nullptr && VisualPawn->GetNetMode() != NM_DedicatedServer && VisualPawn->IsLocallyControlled();
}

void URpgEquipmentComponent::RefreshVisuals()
{
	bVisualRefreshQueued = false;

	APawn* VisualPawn = ResolveVisualPawn();
	if (VisualPawn == nullptr || VisualPawn->GetNetMode() == NM_DedicatedServer)
	{
		DestroyAllVisualActors();
		return;
	}

	TArray<URpgItemInstance*> EquippedItems;
	GetEquippedItems(EquippedItems);

	for (int32 EntryIndex = VisualEntries.Num() - 1; EntryIndex >= 0; --EntryIndex)
	{
		const FRpgEquipmentVisualEntry& Entry = VisualEntries[EntryIndex];
		if (Entry.ItemInstance == nullptr || !EquippedItems.Contains(Entry.ItemInstance))
		{
			if (Entry.VisualActor != nullptr)
			{
				Entry.VisualActor->Destroy();
			}
			VisualEntries.RemoveAtSwap(EntryIndex);
		}
	}

	USkeletalMeshComponent* MeshComponent = VisualPawn->FindComponentByClass<USkeletalMeshComponent>();
	if (MeshComponent == nullptr)
	{
		DestroyAllVisualActors();
		return;
	}

	for (URpgItemInstance* ItemInstance : EquippedItems)
	{
		const URpgItemFragment_Visual* VisualFragment = ItemInstance ? ItemInstance->FindFragmentByClass<URpgItemFragment_Visual>() : nullptr;
		if (VisualFragment == nullptr || VisualFragment->GetEquippedActorClass() == nullptr)
		{
			DestroyVisualActorForItem(ItemInstance);
			continue;
		}

		AActor* VisualActor = FindOrSpawnVisualActor(ItemInstance, VisualPawn);
		if (VisualActor == nullptr)
		{
			continue;
		}

		const bool bIsVisibleEquippedItem = IsItemInPresentationVisibleWeaponSet(ItemInstance);
		const FName DesiredSocket = bIsVisibleEquippedItem ? VisualFragment->GetEquippedSocketName() : VisualFragment->GetStowedSocketName();
		const bool bShouldHide = !bIsVisibleEquippedItem && DesiredSocket.IsNone() && VisualFragment->ShouldHideWhenInactiveWithoutStowedSocket();

		VisualActor->SetActorHiddenInGame(bShouldHide);
		if (bShouldHide)
		{
			continue;
		}

		VisualActor->AttachToComponent(MeshComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, DesiredSocket);
		VisualActor->SetActorRelativeTransform(bIsVisibleEquippedItem ? VisualFragment->GetEquippedRelativeTransform() : VisualFragment->GetStowedRelativeTransform());
	}
}

APawn* URpgEquipmentComponent::ResolveVisualPawn() const
{
	if (const APlayerState* PlayerState = Cast<APlayerState>(GetOwner()))
	{
		return PlayerState->GetPawn();
	}

	return nullptr;
}

USkeletalMeshComponent* URpgEquipmentComponent::ResolvePresentationMesh(APawn* VisualPawn) const
{
	return VisualPawn ? VisualPawn->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

UCharacterMovementComponent* URpgEquipmentComponent::ResolvePresentationMovementComponent(APawn* VisualPawn) const
{
	return VisualPawn ? VisualPawn->FindComponentByClass<UCharacterMovementComponent>() : nullptr;
}

UCameraComponent* URpgEquipmentComponent::ResolvePresentationCameraComponent(APawn* VisualPawn) const
{
	return VisualPawn ? VisualPawn->FindComponentByClass<UCameraComponent>() : nullptr;
}

USpringArmComponent* URpgEquipmentComponent::ResolvePresentationSpringArmComponent(APawn* VisualPawn) const
{
	return VisualPawn ? VisualPawn->FindComponentByClass<USpringArmComponent>() : nullptr;
}

URpgItemInstance* URpgEquipmentComponent::GetPrimaryPresentationItemForWeaponSet(int32 WeaponSetIndex) const
{
	if (!WeaponSets.IsValidIndex(WeaponSetIndex))
	{
		return nullptr;
	}

	const FRpgEquippedWeaponSet& WeaponSet = WeaponSets[WeaponSetIndex];
	return WeaponSet.MainHandItem != nullptr ? WeaponSet.MainHandItem : WeaponSet.OffHandItem;
}

const URpgItemFragment_Visual* URpgEquipmentComponent::GetPrimaryPresentationVisualFragmentForWeaponSet(int32 WeaponSetIndex) const
{
	if (URpgItemInstance* PresentationItem = GetPrimaryPresentationItemForWeaponSet(WeaponSetIndex))
	{
		return PresentationItem->FindFragmentByClass<URpgItemFragment_Visual>();
	}

	return nullptr;
}

bool URpgEquipmentComponent::MontageUsesPresentationNotify(int32 WeaponSetIndex, bool bUseEquipMontage) const
{
	const URpgItemFragment_Visual* VisualFragment = GetPrimaryPresentationVisualFragmentForWeaponSet(WeaponSetIndex);
	if (VisualFragment == nullptr)
	{
		return false;
	}

	const UAnimMontage* MontageToInspect = bUseEquipMontage ? VisualFragment->GetEquipMontage() : VisualFragment->GetUnequipMontage();
	if (MontageToInspect == nullptr)
	{
		return false;
	}

	for (const FAnimNotifyEvent& NotifyEvent : MontageToInspect->Notifies)
	{
		if (NotifyEvent.Notify != nullptr && NotifyEvent.Notify->IsA<UAnimNotify_RpgWeaponToolPresentation>())
		{
			return true;
		}
	}

	return false;
}

bool URpgEquipmentComponent::PlayPresentationMontageForWeaponSet(int32 WeaponSetIndex, bool bUseEquipMontage) const
{
	const URpgItemFragment_Visual* VisualFragment = GetPrimaryPresentationVisualFragmentForWeaponSet(WeaponSetIndex);
	APawn* VisualPawn = ResolveVisualPawn();
	if (VisualFragment == nullptr || VisualPawn == nullptr)
	{
		return false;
	}

	UAnimMontage* MontageToPlay = bUseEquipMontage ? VisualFragment->GetEquipMontage() : VisualFragment->GetUnequipMontage();
	if (MontageToPlay == nullptr)
	{
		return false;
	}

	USkeletalMeshComponent* MeshComponent = VisualPawn->FindComponentByClass<USkeletalMeshComponent>();
	UAnimInstance* AnimInstance = MeshComponent ? MeshComponent->GetAnimInstance() : nullptr;
	return AnimInstance != nullptr && AnimInstance->Montage_Play(MontageToPlay) > 0.0f;
}

AActor* URpgEquipmentComponent::FindVisualActorForItem(const URpgItemInstance* ItemInstance) const
{
	for (const FRpgEquipmentVisualEntry& Entry : VisualEntries)
	{
		if (Entry.ItemInstance == ItemInstance)
		{
			return Entry.VisualActor;
		}
	}

	return nullptr;
}

AActor* URpgEquipmentComponent::FindOrSpawnVisualActor(URpgItemInstance* ItemInstance, APawn* VisualPawn)
{
	if (ItemInstance == nullptr || VisualPawn == nullptr)
	{
		return nullptr;
	}

	if (AActor* ExistingActor = FindVisualActorForItem(ItemInstance))
	{
		return ExistingActor;
	}

	const URpgItemFragment_Visual* VisualFragment = ItemInstance->FindFragmentByClass<URpgItemFragment_Visual>();
	if (VisualFragment == nullptr || VisualFragment->GetEquippedActorClass() == nullptr)
	{
		return nullptr;
	}

	UWorld* World = VisualPawn->GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = VisualPawn;
	SpawnParameters.Instigator = VisualPawn;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* VisualActor = World->SpawnActor<AActor>(VisualFragment->GetEquippedActorClass(), VisualPawn->GetActorLocation(), VisualPawn->GetActorRotation(), SpawnParameters);
	if (VisualActor == nullptr)
	{
		return nullptr;
	}

	VisualActor->SetReplicates(false);
	VisualActor->SetActorEnableCollision(false);

	FRpgEquipmentVisualEntry& NewEntry = VisualEntries.AddDefaulted_GetRef();
	NewEntry.ItemInstance = ItemInstance;
	NewEntry.VisualActor = VisualActor;
	return VisualActor;
}

void URpgEquipmentComponent::DestroyVisualActorForItem(const URpgItemInstance* ItemInstance)
{
	for (int32 EntryIndex = 0; EntryIndex < VisualEntries.Num(); ++EntryIndex)
	{
		if (VisualEntries[EntryIndex].ItemInstance != ItemInstance)
		{
			continue;
		}

		if (VisualEntries[EntryIndex].VisualActor != nullptr)
		{
			VisualEntries[EntryIndex].VisualActor->Destroy();
		}

		VisualEntries.RemoveAtSwap(EntryIndex);
		return;
	}
}

void URpgEquipmentComponent::DestroyAllVisualActors()
{
	for (FRpgEquipmentVisualEntry& Entry : VisualEntries)
	{
		if (Entry.VisualActor != nullptr)
		{
			Entry.VisualActor->Destroy();
		}
	}

	VisualEntries.Reset();
}

void URpgEquipmentComponent::ForceOwnerNetUpdate() const
{
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
}
