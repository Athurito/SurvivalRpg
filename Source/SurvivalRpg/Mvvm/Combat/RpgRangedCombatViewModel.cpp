#include "RpgRangedCombatViewModel.h"

#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/Equipment/RpgRangedWeaponInstance.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_EquippableItem.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerMessageTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgRangedCombatViewModel)

namespace RpgRangedCombatViewModel
{
	/** A draw this close to full counts as full, so float drift never hides the full-draw state. */
	constexpr float FullDrawTolerance = 0.001f;
}

void URpgRangedCombatViewModel::BeginDestroy()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
	if (LoadoutListener.IsValid())
	{
		LoadoutListener.Unregister();
	}
	if (InventoryListener.IsValid())
	{
		InventoryListener.Unregister();
	}
	Super::BeginDestroy();
}

void URpgRangedCombatViewModel::BindPlayerController(APlayerController* Controller)
{
	if (ObservedController.Get() != Controller)
	{
		if (LoadoutListener.IsValid())
		{
			LoadoutListener.Unregister();
		}
		if (InventoryListener.IsValid())
		{
			InventoryListener.Unregister();
		}
		ObservedController = Controller;
		if (UWorld* World = Controller ? Controller->GetWorld() : nullptr)
		{
			UGameplayMessageSubsystem& Messages = UGameplayMessageSubsystem::Get(World);
			LoadoutListener = Messages.RegisterListener<FRpgEquipmentLoadoutSlotsChangedMessage>(
				RpgGameplayTags::Rpg_EquipmentLoadout_Message_SlotsChanged, this, &ThisClass::HandleLoadoutChanged);
			InventoryListener = Messages.RegisterListener<FRpgInventoryChangeMessage>(
				TAG_Rpg_Inventory_Message_StackChanged, this, &ThisClass::HandleInventoryChanged);
		}
	}
	RefreshWeapon();
}

void URpgRangedCombatViewModel::BindAbilitySystem(URpgAbilitySystemComponent* AbilitySystem)
{
	ObservedAbilitySystem = AbilitySystem;
	UpdateTicker();
	RefreshRangedState();
}

void URpgRangedCombatViewModel::Unbind()
{
	BindAbilitySystem(nullptr);
	BindPlayerController(nullptr);
}

void URpgRangedCombatViewModel::RefreshWeapon()
{
	const ARpgPlayerController* Controller = Cast<ARpgPlayerController>(ObservedController.Get());
	const URpgEquipmentLoadoutComponent* Loadout = Controller ? Controller->GetEquipmentLoadoutComponent() : nullptr;
	const URpgInventoryItemInstance* MainHand = Loadout ? Loadout->GetItemInEquipmentSlot(ERpgEquipmentSlot::MainHand) : nullptr;
	ApplyWeapon(FindAmmunitionForWeapon(MainHand ? MainHand->GetItemDef() : nullptr));
}

void URpgRangedCombatViewModel::ApplyWeapon(const TSubclassOf<URpgInventoryItemDefinition> Ammunition)
{
	AmmunitionDefinition = Ammunition;
	UE_MVVM_SET_PROPERTY_VALUE(bHasRangedWeapon, AmmunitionDefinition != nullptr);

	const URpgInventoryItemDefinition* AmmunitionItem = AmmunitionDefinition ? GetDefault<URpgInventoryItemDefinition>(AmmunitionDefinition) : nullptr;
	const URpgInventoryFragment_UIData* UIData = AmmunitionItem
		? Cast<URpgInventoryFragment_UIData>(AmmunitionItem->FindFragmentByClass(URpgInventoryFragment_UIData::StaticClass()))
		: nullptr;
	const FText Name = AmmunitionItem ? AmmunitionItem->DisplayName : FText::GetEmpty();
	if (!AmmunitionName.IdenticalTo(Name))
	{
		AmmunitionName = Name;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(AmmunitionName);
	}
	UE_MVVM_SET_PROPERTY_VALUE(AmmunitionIcon, UIData ? UIData->Icon : TSoftObjectPtr<UTexture2D>());

	RefreshAmmunition();
	UpdateTicker();
	RefreshRangedState();
}

void URpgRangedCombatViewModel::RefreshRangedState()
{
	const URpgAbilitySystemComponent* AbilitySystem = ObservedAbilitySystem.Get();
	if (!AbilitySystem || !bHasRangedWeapon)
	{
		ResetRangedState();
		return;
	}

	float Draw = 0.0f;
	float Nock = 0.0f;
	const bool bHasDraw = AbilitySystem->GetLocalRangedDrawProgress(Draw, Nock);
	UE_MVVM_SET_PROPERTY_VALUE(bAiming, AbilitySystem->IsRangedAimActive());
	UE_MVVM_SET_PROPERTY_VALUE(bDrawing, bHasDraw);
	UE_MVVM_SET_PROPERTY_VALUE(DrawFraction, Draw);
	UE_MVVM_SET_PROPERTY_VALUE(NockFraction, Nock);
	UE_MVVM_SET_PROPERTY_VALUE(bNocked, bHasDraw && Draw >= Nock);
	UE_MVVM_SET_PROPERTY_VALUE(bFullyDrawn, bHasDraw && Draw >= 1.0f - RpgRangedCombatViewModel::FullDrawTolerance);
}

TSubclassOf<URpgInventoryItemDefinition> URpgRangedCombatViewModel::FindAmmunitionForWeapon(const TSubclassOf<URpgInventoryItemDefinition> WeaponItem)
{
	const URpgInventoryItemDefinition* Item = WeaponItem ? GetDefault<URpgInventoryItemDefinition>(WeaponItem) : nullptr;
	const URpgInventoryFragment_EquippableItem* Equippable = Item
		? Cast<URpgInventoryFragment_EquippableItem>(Item->FindFragmentByClass(URpgInventoryFragment_EquippableItem::StaticClass()))
		: nullptr;
	const TSubclassOf<URpgEquipmentDefinition> Equipment = Equippable ? Equippable->GetEquipmentDefinition() : nullptr;
	const TSubclassOf<URpgEquipmentInstance> InstanceType = Equipment ? GetDefault<URpgEquipmentDefinition>(Equipment)->InstanceType : nullptr;
	const URpgRangedWeaponInstance* Weapon = InstanceType ? Cast<URpgRangedWeaponInstance>(InstanceType->GetDefaultObject()) : nullptr;
	return Weapon ? Weapon->GetShotDefinition().AmmunitionItemDefinition : nullptr;
}

void URpgRangedCombatViewModel::HandleLoadoutChanged(FGameplayTag, const FRpgEquipmentLoadoutSlotsChangedMessage&)
{
	RefreshWeapon();
}

void URpgRangedCombatViewModel::HandleInventoryChanged(FGameplayTag, const FRpgInventoryChangeMessage& Message)
{
	if (AmmunitionDefinition && Message.InventoryOwner && Message.InventoryOwner == FindInventory())
	{
		RefreshAmmunition();
	}
}

bool URpgRangedCombatViewModel::HandleTick(float)
{
	RefreshRangedState();
	return true;
}

URpgInventoryManagerComponent* URpgRangedCombatViewModel::FindInventory() const
{
	// The shot spends arrows from the player state's inventory, carried bags included; the HUD counts the same items.
	const APlayerController* Controller = ObservedController.Get();
	const ARpgPlayerState* PlayerState = Controller ? Controller->GetPlayerState<ARpgPlayerState>() : nullptr;
	return PlayerState ? PlayerState->GetInventoryManagerComponent() : nullptr;
}

void URpgRangedCombatViewModel::RefreshAmmunition()
{
	const URpgInventoryManagerComponent* Inventory = FindInventory();
	const int32 Count = AmmunitionDefinition && Inventory ? Inventory->GetTotalItemCountByDefinition(AmmunitionDefinition) : 0;
	if (UE_MVVM_SET_PROPERTY_VALUE(AmmunitionCount, Count) || AmmunitionCountText.IsEmpty())
	{
		AmmunitionCountText = FText::AsNumber(Count);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(AmmunitionCountText);
	}
	UE_MVVM_SET_PROPERTY_VALUE(bOutOfAmmunition, bHasRangedWeapon && Count <= 0);
	UE_MVVM_SET_PROPERTY_VALUE(bShotAvailable, bHasRangedWeapon && Count > 0);
}

void URpgRangedCombatViewModel::UpdateTicker()
{
	// Aim and draw change every frame while a ranged weapon is out; without one nothing ticks.
	const bool bShouldTick = bHasRangedWeapon && ObservedAbilitySystem.IsValid();
	if (bShouldTick && !TickerHandle.IsValid())
	{
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::HandleTick));
	}
	else if (!bShouldTick && TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
}

void URpgRangedCombatViewModel::ResetRangedState()
{
	UE_MVVM_SET_PROPERTY_VALUE(bAiming, false);
	UE_MVVM_SET_PROPERTY_VALUE(bDrawing, false);
	UE_MVVM_SET_PROPERTY_VALUE(DrawFraction, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(NockFraction, 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(bNocked, false);
	UE_MVVM_SET_PROPERTY_VALUE(bFullyDrawn, false);
}
