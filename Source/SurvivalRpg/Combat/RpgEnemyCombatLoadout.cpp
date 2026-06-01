#include "RpgEnemyCombatLoadout.h"

#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentInstance.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_EquippableItem.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgEnemyCombatLoadout)

URpgEnemyCombatArchetypeComponent::URpgEnemyCombatArchetypeComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

URpgEnemyCombatArchetypeComponent* URpgEnemyCombatArchetypeComponent::FindEnemyCombatArchetypeComponent(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<URpgEnemyCombatArchetypeComponent>() : nullptr;
}

void URpgEnemyCombatArchetypeComponent::SetEnemyCombatArchetypeTag(FGameplayTag NewArchetypeTag)
{
	if (AActor* Owner = GetOwner(); Owner && Owner->HasAuthority())
	{
		EnemyCombatArchetypeTag = NewArchetypeTag;
		Owner->ForceNetUpdate();
	}
}

void URpgEnemyCombatArchetypeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, EnemyCombatArchetypeTag);
}

URpgEnemyCombatLoadoutComponent::URpgEnemyCombatLoadoutComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void URpgEnemyCombatLoadoutComponent::OnRegister()
{
	Super::OnRegister();
	BindToPawnExtension();
}

void URpgEnemyCombatLoadoutComponent::BeginPlay()
{
	Super::BeginPlay();
	BindToPawnExtension();
}

void URpgEnemyCombatLoadoutComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ApplyRetryTimerHandle);
	}

	ClearAppliedCombatLoadout();
	UnbindFromPawnExtension();

	Super::EndPlay(EndPlayReason);
}

void URpgEnemyCombatLoadoutComponent::ApplyCombatLoadout()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	URpgEquipmentManagerComponent* EquipmentManager = FindEquipmentManager();
	if (!EquipmentManager)
	{
		ScheduleApplyRetry();
		return;
	}

	const FGameplayTag ArchetypeTag = ResolveArchetypeTag();
	const URpgEnemyCombatLoadoutDefinition* LoadoutDefinition = ResolveLoadoutDefinition(ArchetypeTag);
	if (!LoadoutDefinition)
	{
		return;
	}

	ClearAppliedCombatLoadout();

	for (const FRpgEnemyCombatLoadoutItem& LoadoutItem : LoadoutDefinition->GetEquipmentItems())
	{
		if (TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition = ResolveEquipmentDefinition(LoadoutItem))
		{
			if (URpgEquipmentInstance* EquipmentInstance = EquipmentManager->EquipItemInSlot(EquipmentDefinition, LoadoutItem.EquipmentSlot))
			{
				AppliedEquipmentInstances.Add(EquipmentInstance);
			}
		}
	}

	AppliedArchetypeTag = LoadoutDefinition->GetArchetypeTag();
	Owner->ForceNetUpdate();
}

void URpgEnemyCombatLoadoutComponent::ClearAppliedCombatLoadout()
{
	URpgEquipmentManagerComponent* EquipmentManager = FindEquipmentManager();
	if (!EquipmentManager)
	{
		AppliedEquipmentInstances.Reset();
		AppliedArchetypeTag = FGameplayTag();
		return;
	}

	for (TWeakObjectPtr<URpgEquipmentInstance>& EquipmentInstancePtr : AppliedEquipmentInstances)
	{
		if (URpgEquipmentInstance* EquipmentInstance = EquipmentInstancePtr.Get())
		{
			EquipmentManager->UnequipItem(EquipmentInstance);
		}
	}

	AppliedEquipmentInstances.Reset();
	AppliedArchetypeTag = FGameplayTag();
}

void URpgEnemyCombatLoadoutComponent::BindToPawnExtension()
{
	if (BoundPawnExtension)
	{
		return;
	}

	URpgPawnExtensionComponent* PawnExtension = URpgPawnExtensionComponent::FindPawnExtensionComponent(GetOwner());
	if (!PawnExtension)
	{
		return;
	}

	BoundPawnExtension = PawnExtension;
	PawnExtension->OnAbilitySystemInitialized_RegisterAndCall(
		FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemInitialized));
	PawnExtension->OnAbilitySystemUninitialized_Register(
		FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemUninitialized));
}

void URpgEnemyCombatLoadoutComponent::UnbindFromPawnExtension()
{
	if (!BoundPawnExtension)
	{
		return;
	}

	BoundPawnExtension->OnAbilitySystemInitialized.RemoveAll(this);
	BoundPawnExtension->OnAbilitySystemUninitialized.RemoveAll(this);
	BoundPawnExtension = nullptr;
}

void URpgEnemyCombatLoadoutComponent::HandleAbilitySystemInitialized()
{
	if (bApplyOnAbilitySystemInitialized)
	{
		ApplyCombatLoadout();
	}
}

void URpgEnemyCombatLoadoutComponent::HandleAbilitySystemUninitialized()
{
	ClearAppliedCombatLoadout();
}

void URpgEnemyCombatLoadoutComponent::ScheduleApplyRetry()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ApplyRetryTimerHandle,
			this,
			&ThisClass::ApplyCombatLoadout,
			0.1f,
			false);
	}
}

FGameplayTag URpgEnemyCombatLoadoutComponent::ResolveArchetypeTag() const
{
	if (const URpgEnemyCombatArchetypeComponent* ArchetypeComponent = URpgEnemyCombatArchetypeComponent::FindEnemyCombatArchetypeComponent(GetOwner()))
	{
		if (ArchetypeComponent->GetEnemyCombatArchetypeTag().IsValid())
		{
			return ArchetypeComponent->GetEnemyCombatArchetypeTag();
		}
	}

	return DefaultArchetypeTag;
}

const URpgEnemyCombatLoadoutDefinition* URpgEnemyCombatLoadoutComponent::ResolveLoadoutDefinition(FGameplayTag ArchetypeTag) const
{
	if (!ArchetypeTag.IsValid())
	{
		return nullptr;
	}

	for (const TSoftObjectPtr<const URpgEnemyCombatLoadoutDefinition>& LoadoutDefinitionPtr : LoadoutDefinitions)
	{
		const URpgEnemyCombatLoadoutDefinition* LoadoutDefinition = LoadoutDefinitionPtr.LoadSynchronous();
		if (LoadoutDefinition && LoadoutDefinition->GetArchetypeTag() == ArchetypeTag)
		{
			return LoadoutDefinition;
		}
	}

	return nullptr;
}

TSubclassOf<URpgEquipmentDefinition> URpgEnemyCombatLoadoutComponent::ResolveEquipmentDefinition(const FRpgEnemyCombatLoadoutItem& LoadoutItem) const
{
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinitionClass = LoadoutItem.ItemDefinition.LoadSynchronous();
	if (!ItemDefinitionClass)
	{
		return nullptr;
	}

	const URpgInventoryItemDefinition* ItemDefinitionCDO = GetDefault<URpgInventoryItemDefinition>(ItemDefinitionClass);
	const URpgInventoryFragment_EquippableItem* EquippableFragment = ItemDefinitionCDO
		? Cast<URpgInventoryFragment_EquippableItem>(ItemDefinitionCDO->FindFragmentByClass(URpgInventoryFragment_EquippableItem::StaticClass()))
		: nullptr;

	return EquippableFragment ? EquippableFragment->GetEquipmentDefinition() : nullptr;
}

URpgEquipmentManagerComponent* URpgEnemyCombatLoadoutComponent::FindEquipmentManager() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<URpgEquipmentManagerComponent>() : nullptr;
}
