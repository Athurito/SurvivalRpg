#include "RpgUIScreenSubsystem.h"

#include "CommonActivatableWidget.h"
#include "Engine/StreamableManager.h"
#include "PrimaryGameLayout.h"
#include "RpgUIScreenPayload.h"
#include "RpgUIScreenRegistry.h"
#include "RpgUISettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogRpgUIScreenSubsystem, Log, All);

void URpgUIScreenSubsystem::Deinitialize()
{
	TArray<TSharedPtr<FStreamableHandle>> StreamingHandles;
	PendingScreenLoads.GenerateValueArray(StreamingHandles);
	for (const TSharedPtr<FStreamableHandle>& StreamingHandle : StreamingHandles)
	{
		if (StreamingHandle.IsValid())
		{
			StreamingHandle->CancelHandle();
		}
	}

	ActiveScreens.Reset();
	PendingPayloads.Reset();
	PendingScreenTags.Reset();
	PendingScreenLoads.Reset();
	CanceledPendingScreenTags.Reset();

	Super::Deinitialize();
}

UCommonActivatableWidget* URpgUIScreenSubsystem::OpenScreen(FGameplayTag ScreenTag, UObject* Payload)
{
	if (!ScreenTag.IsValid())
	{
		UE_LOG(LogRpgUIScreenSubsystem, Warning, TEXT("OpenScreen called with an invalid ScreenTag."));
		return nullptr;
	}

	FRpgUIScreenRegistryEntry Entry;
	if (!ResolveScreenEntry(ScreenTag, Entry))
	{
		UE_LOG(LogRpgUIScreenSubsystem, Warning, TEXT("No UI screen registry entry found for [%s]."), *ScreenTag.ToString());
		return nullptr;
	}

	if (!Entry.LayerTag.IsValid() || Entry.WidgetClass.IsNull())
	{
		UE_LOG(LogRpgUIScreenSubsystem, Warning, TEXT("Invalid UI screen registry entry for [%s]."), *ScreenTag.ToString());
		return nullptr;
	}

	if (!Entry.bSingleInstance)
	{
		UE_LOG(LogRpgUIScreenSubsystem, Warning,
			TEXT("Screen [%s] has legacy bSingleInstance=false data. UI.Screen tags are always local-player singletons; enforcing single-instance routing."),
			*ScreenTag.ToString());
	}

	if (UCommonActivatableWidget* ExistingWidget = GetActiveScreen(ScreenTag))
	{
		ApplyPayloadToWidget(ExistingWidget, Payload);
		return ExistingWidget;
	}

	if (PendingScreenTags.Contains(ScreenTag))
	{
		return nullptr;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return nullptr;
	}

	UPrimaryGameLayout* RootLayout = GetPrimaryGameLayout();
	if (!RootLayout)
	{
		UE_LOG(LogRpgUIScreenSubsystem, Warning, TEXT("Cannot open [%s]: no PrimaryGameLayout exists for local player [%s]."),
			*ScreenTag.ToString(),
			*GetNameSafe(LocalPlayer));
		return nullptr;
	}

	if (!RootLayout->GetLayerWidget(Entry.LayerTag))
	{
		UE_LOG(LogRpgUIScreenSubsystem, Warning, TEXT("Cannot open [%s]: PrimaryGameLayout [%s] has no registered layer [%s]. Check the root layout's CommonActivatableWidgetStack bindings."),
			*ScreenTag.ToString(),
			*GetNameSafe(RootLayout),
			*Entry.LayerTag.ToString());
		return nullptr;
	}

	if (Payload)
	{
		PendingPayloads.Add(ScreenTag, Payload);
	}
	else
	{
		PendingPayloads.Remove(ScreenTag);
	}

	PendingScreenTags.Add(ScreenTag);

	UE_LOG(LogRpgUIScreenSubsystem, Log, TEXT("Opening screen [%s] on layer [%s] with widget class [%s]."),
		*ScreenTag.ToString(),
		*Entry.LayerTag.ToString(),
		*Entry.WidgetClass.ToString());

	const TWeakObjectPtr<URpgUIScreenSubsystem> WeakThis(this);
	TSharedPtr<FStreamableHandle> StreamingHandle =
		RootLayout->PushWidgetToLayerStackAsync<UCommonActivatableWidget>(
		Entry.LayerTag,
		Entry.bSuspendInputUntilLoaded,
		Entry.WidgetClass,
		[WeakThis, ScreenTag](EAsyncWidgetLayerState State, UCommonActivatableWidget* Widget)
		{
			if (URpgUIScreenSubsystem* ScreenSubsystem = WeakThis.Get())
			{
				ScreenSubsystem->HandleScreenPushState(ScreenTag, State, Widget);
			}
		});

	// RequestAsyncLoad can complete inline for an already-loaded class. Only retain
	// the handle when the completion callback has not already cleared this tag.
	if (PendingScreenTags.Contains(ScreenTag) && StreamingHandle.IsValid())
	{
		PendingScreenLoads.Add(ScreenTag, MoveTemp(StreamingHandle));
	}

	return nullptr;
}

UPrimaryGameLayout* URpgUIScreenSubsystem::GetPrimaryGameLayout() const
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	return LocalPlayer ? UPrimaryGameLayout::GetPrimaryGameLayout(LocalPlayer) : nullptr;
}

UCommonActivatableWidget* URpgUIScreenSubsystem::ToggleScreen(FGameplayTag ScreenTag, UObject* Payload)
{
	if (UCommonActivatableWidget* ActiveWidget = GetActiveScreen(ScreenTag))
	{
		CloseScreen(ScreenTag);
		return ActiveWidget;
	}

	if (PendingScreenTags.Contains(ScreenTag))
	{
		return nullptr;
	}

	return OpenScreen(ScreenTag, Payload);
}

void URpgUIScreenSubsystem::CloseScreen(FGameplayTag ScreenTag)
{
	if (UCommonActivatableWidget* ActiveWidget = GetActiveScreen(ScreenTag))
	{
		ActiveWidget->DeactivateWidget();
		return;
	}

	if (PendingScreenTags.Contains(ScreenTag))
	{
		CanceledPendingScreenTags.Add(ScreenTag);
		const TSharedPtr<FStreamableHandle> StreamingHandle =
			PendingScreenLoads.FindRef(ScreenTag);
		if (StreamingHandle.IsValid() &&
			!StreamingHandle->HasLoadCompleted())
		{
			// Hold a local shared reference because a zero-frame delegate delay
			// may remove this request from PendingScreenLoads synchronously.
			StreamingHandle->CancelHandle();
		}
	}
}

UCommonActivatableWidget* URpgUIScreenSubsystem::GetActiveScreen(FGameplayTag ScreenTag) const
{
	if (const TObjectPtr<UCommonActivatableWidget>* FoundWidget = ActiveScreens.Find(ScreenTag))
	{
		if (UCommonActivatableWidget* Widget = FoundWidget->Get())
		{
			if (Widget->IsActivated())
			{
				return Widget;
			}
		}
	}

	return nullptr;
}

bool URpgUIScreenSubsystem::IsScreenActiveOrPending(FGameplayTag ScreenTag) const
{
	return PendingScreenTags.Contains(ScreenTag) || GetActiveScreen(ScreenTag) != nullptr;
}

const URpgUIScreenRegistry* URpgUIScreenSubsystem::GetScreenRegistry() const
{
	const URpgUISettings* UISettings = GetDefault<URpgUISettings>();
	if (!UISettings || UISettings->ScreenRegistry.IsNull())
	{
		return nullptr;
	}

	return UISettings->ScreenRegistry.LoadSynchronous();
}

bool URpgUIScreenSubsystem::ResolveScreenEntry(FGameplayTag ScreenTag, FRpgUIScreenRegistryEntry& OutEntry) const
{
	if (const URpgUIScreenRegistry* Registry = GetScreenRegistry())
	{
		if (Registry->FindScreen(ScreenTag, OutEntry))
		{
			return true;
		}
	}

	const URpgUISettings* UISettings = GetDefault<URpgUISettings>();
	if (!UISettings)
	{
		return false;
	}

	for (const FRpgUIScreenRegistryEntry& Entry : UISettings->DefaultScreenMappings)
	{
		if (Entry.ScreenTag == ScreenTag)
		{
			OutEntry = Entry;
			return true;
		}
	}

	return false;
}

void URpgUIScreenSubsystem::ApplyPayloadToWidget(UCommonActivatableWidget* Widget, UObject* Payload) const
{
	if (Widget && Widget->GetClass()->ImplementsInterface(URpgUIScreenPayloadReceiver::StaticClass()))
	{
		IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(Widget, Payload);
	}
}

void URpgUIScreenSubsystem::HandleScreenPushState(
	FGameplayTag ScreenTag,
	EAsyncWidgetLayerState State,
	UCommonActivatableWidget* Widget)
{
	if (State == EAsyncWidgetLayerState::Canceled)
	{
		CanceledPendingScreenTags.Remove(ScreenTag);
		ClearPendingScreenState(ScreenTag);
		return;
	}

	if (State == EAsyncWidgetLayerState::Initialize)
	{
		if (!Widget || CanceledPendingScreenTags.Contains(ScreenTag))
		{
			return;
		}

		ActiveScreens.Add(ScreenTag, Widget);

		UObject* PayloadToApply = nullptr;
		if (TObjectPtr<UObject>* PendingPayload = PendingPayloads.Find(ScreenTag))
		{
			PayloadToApply = PendingPayload->Get();
		}

		ApplyPayloadToWidget(Widget, PayloadToApply);
		Widget->OnDeactivated().AddUObject(this, &ThisClass::HandleScreenDeactivated, ScreenTag, Widget);
		UE_LOG(LogRpgUIScreenSubsystem, Log, TEXT("Initialized screen [%s] as widget [%s]."),
			*ScreenTag.ToString(),
			*GetNameSafe(Widget));
		return;
	}

	if (State != EAsyncWidgetLayerState::AfterPush)
	{
		return;
	}

	const bool bWasCanceled = CanceledPendingScreenTags.Remove(ScreenTag) > 0;
	if (bWasCanceled && Widget)
	{
		if (const TObjectPtr<UCommonActivatableWidget>* ActiveWidget = ActiveScreens.Find(ScreenTag))
		{
			if (ActiveWidget->Get() == Widget)
			{
				ActiveScreens.Remove(ScreenTag);
			}
		}

		if (UPrimaryGameLayout* RootLayout = GetPrimaryGameLayout())
		{
			RootLayout->FindAndRemoveWidgetFromLayer(Widget);
		}
		else
		{
			Widget->DeactivateWidget();
		}

		UE_LOG(LogRpgUIScreenSubsystem, Verbose,
			TEXT("Discarded canceled screen [%s] after its async push completed."),
			*ScreenTag.ToString());
	}
	else if (!Widget)
	{
		UE_LOG(LogRpgUIScreenSubsystem, Warning,
			TEXT("Screen [%s] finished pushing but no widget was created. Check that the mapped class is a CommonActivatableWidget and can load."),
			*ScreenTag.ToString());
	}

	ClearPendingScreenState(ScreenTag);
}

void URpgUIScreenSubsystem::HandleScreenDeactivated(FGameplayTag ScreenTag, UCommonActivatableWidget* Widget)
{
	if (const TObjectPtr<UCommonActivatableWidget>* FoundWidget = ActiveScreens.Find(ScreenTag))
	{
		if (FoundWidget->Get() == Widget)
		{
			ActiveScreens.Remove(ScreenTag);
		}
	}

	// A widget can deactivate from its activation callback before CommonGame
	// emits AfterPush. Keep the tag pending until that terminal callback so a
	// same-tag reopen cannot race the still-completing async push.
	if (!PendingScreenTags.Contains(ScreenTag))
	{
		PendingPayloads.Remove(ScreenTag);
	}
}

void URpgUIScreenSubsystem::ClearPendingScreenState(FGameplayTag ScreenTag)
{
	PendingPayloads.Remove(ScreenTag);
	PendingScreenTags.Remove(ScreenTag);
	PendingScreenLoads.Remove(ScreenTag);
}
