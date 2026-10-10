#include "RpgPickupFeedViewModels.h"

#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerMessageTags.h"
#include "SurvivalRpg/UI/RpgInventoryScreenMessages.h"
#include "SurvivalRpg/UI/RpgUISettings.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPickupFeedViewModels)

#define LOCTEXT_NAMESPACE "RpgPickupFeed"

namespace RpgPickupFeed
{
	/** Stack changes of one interaction arrive over a few frames; they are netted before a notification appears. */
	constexpr float SettleSeconds = 0.15f;

	/** Replication of a screen move can arrive shortly after the screen closed. */
	constexpr double ScreenCloseGraceSeconds = 1.0;

	constexpr float ExpiryTickSeconds = 0.1f;
}

void URpgPickupFeedEntryViewModel::InitializeEntry(
	TSubclassOf<URpgInventoryItemDefinition> InItemDefinition,
	int32 InCount,
	double Now)
{
	ItemDefinition = InItemDefinition;
	LastGainTime = Now;

	const URpgInventoryItemDefinition* ItemCDO = InItemDefinition
		? GetDefault<URpgInventoryItemDefinition>(InItemDefinition)
		: nullptr;
	const URpgInventoryFragment_UIData* UIData = ItemCDO
		? Cast<URpgInventoryFragment_UIData>(ItemCDO->FindFragmentByClass(URpgInventoryFragment_UIData::StaticClass()))
		: nullptr;
	DisplayName = ItemCDO ? ItemCDO->DisplayName : FText::GetEmpty();
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(DisplayName);
	UE_MVVM_SET_PROPERTY_VALUE(Icon, UIData ? UIData->Icon : TSoftObjectPtr<UTexture2D>());
	UE_MVVM_SET_PROPERTY_VALUE(Count, FMath::Max(1, InCount));
	UE_MVVM_SET_PROPERTY_VALUE(bExpiring, false);
	RefreshLabel();
}

void URpgPickupFeedEntryViewModel::AddCount(int32 Extra, double Now)
{
	LastGainTime = Now;
	UE_MVVM_SET_PROPERTY_VALUE(Count, Count + FMath::Max(0, Extra));
	UE_MVVM_SET_PROPERTY_VALUE(bExpiring, false);
	RefreshLabel();
}

void URpgPickupFeedEntryViewModel::SetExpiring(bool bInExpiring)
{
	UE_MVVM_SET_PROPERTY_VALUE(bExpiring, bInExpiring);
}

void URpgPickupFeedEntryViewModel::RefreshLabel()
{
	const FText NewLabel = FText::Format(
		LOCTEXT("PickupLabel", "{0} × {1}"),
		FText::AsNumber(Count),
		DisplayName);
	if (!LabelText.IdenticalTo(NewLabel, ETextIdenticalModeFlags::DeepCompare | ETextIdenticalModeFlags::LexicalCompareInvariants))
	{
		LabelText = NewLabel;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(LabelText);
	}
}

void URpgPickupFeedViewModel::BeginDestroy()
{
	Unbind();
	Super::BeginDestroy();
}

void URpgPickupFeedViewModel::BindPlayerController(APlayerController* InPlayerController)
{
	if (PlayerController.Get() == InPlayerController)
	{
		return;
	}

	Unbind();
	UWorld* World = InPlayerController ? InPlayerController->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	PlayerController = InPlayerController;
	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
	InventoryListenerHandle = MessageSubsystem.RegisterListener<FRpgInventoryChangeMessage>(
		TAG_Rpg_Inventory_Message_StackChanged,
		this,
		&ThisClass::HandleInventoryChanged);
	ScreenListenerHandle = MessageSubsystem.RegisterListener<FRpgInventoryScreenActivationMessage>(
		RpgGameplayTags::Rpg_Inventory_Message_ScreenActivation,
		this,
		&ThisClass::HandleScreenActivation);
}

void URpgPickupFeedViewModel::Unbind()
{
	if (UWorld* World = GetFeedWorld())
	{
		World->GetTimerManager().ClearTimer(FlushTimerHandle);
		World->GetTimerManager().ClearTimer(ExpiryTimerHandle);
	}
	if (InventoryListenerHandle.IsValid())
	{
		InventoryListenerHandle.Unregister();
	}
	if (ScreenListenerHandle.IsValid())
	{
		ScreenListenerHandle.Unregister();
	}

	PlayerController.Reset();
	ObservedInventory.Reset();
	OpenScreenCount = 0;
	LastScreenCloseTime = -1.0e9;
	PendingDeltas.Reset();
	SetEntries({});
}

void URpgPickupFeedViewModel::AddGain(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 GainedCount)
{
	if (!ItemDefinition || GainedCount <= 0)
	{
		return;
	}

	const double Now = GetNow();
	for (URpgPickupFeedEntryViewModel* Entry : Entries)
	{
		// A fading notification of the same item comes back instead of stacking a second one.
		if (Entry && Entry->GetItemDefinition() == ItemDefinition)
		{
			Entry->AddCount(GainedCount, Now);
			UpdateExpiryTimer();
			return;
		}
	}

	TArray<TObjectPtr<URpgPickupFeedEntryViewModel>> NewEntries = Entries;
	URpgPickupFeedEntryViewModel* NewEntry = NewObject<URpgPickupFeedEntryViewModel>(this);
	NewEntry->InitializeEntry(ItemDefinition, GainedCount, Now);
	NewEntries.Add(NewEntry);

	const int32 MaxEntries = FMath::Max(1, GetDefault<URpgUISettings>()->HudPickupMaxEntries);
	while (NewEntries.Num() > MaxEntries)
	{
		NewEntries.RemoveAt(0);
	}
	SetEntries(MoveTemp(NewEntries));
	UpdateExpiryTimer();
}

void URpgPickupFeedViewModel::FlushPendingGains()
{
	if (UWorld* World = GetFeedWorld())
	{
		World->GetTimerManager().ClearTimer(FlushTimerHandle);
	}

	TMap<TSubclassOf<URpgInventoryItemDefinition>, int32> Deltas = MoveTemp(PendingDeltas);
	PendingDeltas.Reset();
	for (const TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>& Delta : Deltas)
	{
		if (Delta.Value > 0)
		{
			AddGain(Delta.Key, Delta.Value);
		}
	}
}

void URpgPickupFeedViewModel::UpdateExpiry(double Now)
{
	const URpgUISettings* Settings = GetDefault<URpgUISettings>();
	const double HoldSeconds = FMath::Max(0.5f, Settings->HudPickupHoldSeconds);
	const double FadeSeconds = FMath::Max(0.0f, Settings->HudPickupFadeSeconds);

	TArray<TObjectPtr<URpgPickupFeedEntryViewModel>> NewEntries;
	NewEntries.Reserve(Entries.Num());
	for (URpgPickupFeedEntryViewModel* Entry : Entries)
	{
		if (!Entry)
		{
			continue;
		}
		const double Age = Now - Entry->GetLastGainTime();
		if (Age >= HoldSeconds + FadeSeconds)
		{
			continue;
		}
		Entry->SetExpiring(Age >= HoldSeconds);
		NewEntries.Add(Entry);
	}
	if (NewEntries.Num() != Entries.Num())
	{
		SetEntries(MoveTemp(NewEntries));
	}
	UpdateExpiryTimer();
}

void URpgPickupFeedViewModel::HandleInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	if (!PlayerInventory || Message.InventoryOwner.Get() != PlayerInventory ||
		Message.bCapacityChanged || Message.Delta == 0)
	{
		return;
	}

	const double Now = GetNow();
	if (ObservedInventory.Get() != PlayerInventory)
	{
		// A new player inventory (first spawn or travel) replicates in a burst that is not a pickup.
		ObservedInventory = PlayerInventory;
		ObserveStartTime = Now;
		PendingDeltas.Reset();
	}

	const URpgInventoryItemInstance* Instance = Message.Instance.Get();
	const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition = Instance ? Instance->GetItemDef() : nullptr;
	if (!ItemDefinition || Message.bFromRestore || IsSuppressed(Now))
	{
		return;
	}

	PendingDeltas.FindOrAdd(ItemDefinition) += Message.Delta;
	UWorld* World = GetFeedWorld();
	if (World && !World->GetTimerManager().IsTimerActive(FlushTimerHandle))
	{
		World->GetTimerManager().SetTimer(
			FlushTimerHandle,
			FTimerDelegate::CreateUObject(this, &ThisClass::FlushPendingGains),
			RpgPickupFeed::SettleSeconds,
			false);
	}
}

void URpgPickupFeedViewModel::HandleScreenActivation(
	FGameplayTag Channel,
	const FRpgInventoryScreenActivationMessage& Message)
{
	if (Message.OwningPlayer.Get() != PlayerController.Get())
	{
		return;
	}

	if (Message.bActive)
	{
		++OpenScreenCount;
		// Gains netted before the screen opened still count; anything after is a screen move.
		FlushPendingGains();
	}
	else
	{
		OpenScreenCount = FMath::Max(0, OpenScreenCount - 1);
		LastScreenCloseTime = GetNow();
	}
}

void URpgPickupFeedViewModel::HandleExpiryTimer()
{
	UpdateExpiry(GetNow());
}

URpgInventoryManagerComponent* URpgPickupFeedViewModel::FindPlayerInventory() const
{
	const APlayerController* Controller = PlayerController.Get();
	const ARpgPlayerState* PlayerState = Controller ? Controller->GetPlayerState<ARpgPlayerState>() : nullptr;
	return PlayerState ? PlayerState->GetInventoryManagerComponent() : nullptr;
}

bool URpgPickupFeedViewModel::IsSuppressed(double Now) const
{
	const double WarmupSeconds = FMath::Max(0.0f, GetDefault<URpgUISettings>()->HudPickupWarmupSeconds);
	return OpenScreenCount > 0 ||
		Now - LastScreenCloseTime < RpgPickupFeed::ScreenCloseGraceSeconds ||
		Now - ObserveStartTime < WarmupSeconds;
}

double URpgPickupFeedViewModel::GetNow() const
{
	const UWorld* World = GetFeedWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

UWorld* URpgPickupFeedViewModel::GetFeedWorld() const
{
	const APlayerController* Controller = PlayerController.Get();
	return Controller ? Controller->GetWorld() : nullptr;
}

void URpgPickupFeedViewModel::SetEntries(TArray<TObjectPtr<URpgPickupFeedEntryViewModel>>&& NewEntries)
{
	if (Entries != NewEntries)
	{
		Entries = MoveTemp(NewEntries);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Entries);
	}
	UE_MVVM_SET_PROPERTY_VALUE(bHasEntries, !Entries.IsEmpty());
}

void URpgPickupFeedViewModel::UpdateExpiryTimer()
{
	UWorld* World = GetFeedWorld();
	if (!World)
	{
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();
	if (Entries.IsEmpty())
	{
		TimerManager.ClearTimer(ExpiryTimerHandle);
	}
	else if (!TimerManager.IsTimerActive(ExpiryTimerHandle))
	{
		TimerManager.SetTimer(
			ExpiryTimerHandle,
			FTimerDelegate::CreateUObject(this, &ThisClass::HandleExpiryTimer),
			RpgPickupFeed::ExpiryTickSeconds,
			true);
	}
}

#undef LOCTEXT_NAMESPACE
