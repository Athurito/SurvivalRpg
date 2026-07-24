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
	bIsDeinitializing = true;

	TArray<TSharedPtr<FStreamableHandle>> StreamingHandles;
	PendingScreenLoads.GenerateValueArray(StreamingHandles);
	for (const TSharedPtr<FStreamableHandle>& StreamingHandle : StreamingHandles)
	{
		if (StreamingHandle.IsValid())
		{
			StreamingHandle->CancelHandle();
		}
	}

	TSet<TObjectPtr<UCommonActivatableWidget>> WidgetsToRelease;
	for (const TPair<FGameplayTag, TObjectPtr<UCommonActivatableWidget>>& ActiveScreen :
		ActiveScreens)
	{
		if (ActiveScreen.Value)
		{
			WidgetsToRelease.Add(ActiveScreen.Value);
		}
	}
	TArray<uint64> CheckoutIds;
	ScreenDeactivationBindings.GenerateKeyArray(CheckoutIds);
	for (const uint64 CheckoutId : CheckoutIds)
	{
		ReleaseScreenDeactivationBinding(CheckoutId);
	}
	for (UCommonActivatableWidget* Widget : WidgetsToRelease)
	{
		// The layout can retain this UObject for pooling after the subsystem is
		// gone. Router callbacks were removed by exact handle above, so widget
		// cleanup cannot re-enter this subsystem with stale checkout identity.
		if (Widget->IsActivated())
		{
			Widget->DeactivateWidget();
		}
	}

	ActiveScreens.Reset();
	ActiveScreenCheckoutIds.Reset();
	ScreenDeactivationBindings.Reset();
	PendingPayloads.Reset();
	PendingScreenTags.Reset();
	PendingScreenLoads.Reset();
	CanceledPendingScreenTags.Reset();

	Super::Deinitialize();
}

UCommonActivatableWidget* URpgUIScreenSubsystem::OpenScreen(FGameplayTag ScreenTag, UObject* Payload)
{
	if (bIsDeinitializing)
	{
		return nullptr;
	}

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
	if (bIsDeinitializing)
	{
		return nullptr;
	}

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
	if (bIsDeinitializing)
	{
		return;
	}

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
	if (bIsDeinitializing)
	{
		ReleaseScreenDeactivationBindings(ScreenTag, Widget);
		if (State == EAsyncWidgetLayerState::AfterPush && Widget)
		{
			// An already-dispatched async push can finish while LocalPlayer
			// teardown is in progress. Do not leave its now-untracked widget
			// active in the CommonUI layer.
			if (UPrimaryGameLayout* RootLayout = GetPrimaryGameLayout())
			{
				RootLayout->FindAndRemoveWidgetFromLayer(Widget);
			}
			else
			{
				Widget->DeactivateWidget();
			}
		}
		CanceledPendingScreenTags.Remove(ScreenTag);
		ClearPendingScreenState(ScreenTag);
		return;
	}

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

		UObject* PayloadToApply = nullptr;
		if (TObjectPtr<UObject>* PendingPayload = PendingPayloads.Find(ScreenTag))
		{
			PayloadToApply = PendingPayload->Get();
		}

		ApplyPayloadToWidget(Widget, PayloadToApply);
		const uint64 CheckoutId =
			RegisterScreenDeactivationBinding(ScreenTag, Widget);
		ActiveScreens.Add(ScreenTag, Widget);
		ActiveScreenCheckoutIds.Add(ScreenTag, CheckoutId);
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
		// A canceled widget may never have activated, so it would never emit
		// OnDeactivated. Release the checkout callback explicitly.
		ReleaseScreenDeactivationBindings(ScreenTag, Widget);

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

void URpgUIScreenSubsystem::HandleScreenDeactivated(
	FGameplayTag ScreenTag,
	UCommonActivatableWidget* Widget,
	uint64 CheckoutId)
{
	const FScreenDeactivationBinding* Binding =
		ScreenDeactivationBindings.Find(CheckoutId);
	if (!Binding ||
		Binding->ScreenTag != ScreenTag ||
		Binding->Widget.Get() != Widget)
	{
		return;
	}

	ReleaseScreenDeactivationBinding(CheckoutId);

	const uint64* ActiveCheckoutId =
		ActiveScreenCheckoutIds.Find(ScreenTag);
	if (ActiveCheckoutId && *ActiveCheckoutId == CheckoutId)
	{
		ActiveScreenCheckoutIds.Remove(ScreenTag);
		if (const TObjectPtr<UCommonActivatableWidget>* FoundWidget =
			ActiveScreens.Find(ScreenTag))
		{
			if (FoundWidget->Get() == Widget)
			{
				ActiveScreens.Remove(ScreenTag);
			}
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

uint64 URpgUIScreenSubsystem::RegisterScreenDeactivationBinding(
	FGameplayTag ScreenTag,
	UCommonActivatableWidget* Widget)
{
	check(Widget);

	++NextScreenCheckoutId;
	if (NextScreenCheckoutId == 0)
	{
		++NextScreenCheckoutId;
	}

	FScreenDeactivationBinding Binding;
	Binding.ScreenTag = ScreenTag;
	Binding.Widget = Widget;
	Binding.DelegateHandle = Widget->OnDeactivated().AddUObject(
		this,
		&ThisClass::HandleScreenDeactivated,
		ScreenTag,
		Widget,
		NextScreenCheckoutId);
	ScreenDeactivationBindings.Add(
		NextScreenCheckoutId,
		MoveTemp(Binding));
	return NextScreenCheckoutId;
}

void URpgUIScreenSubsystem::ReleaseScreenDeactivationBinding(
	uint64 CheckoutId)
{
	FScreenDeactivationBinding Binding;
	if (!ScreenDeactivationBindings.RemoveAndCopyValue(
		CheckoutId,
		Binding))
	{
		return;
	}

	if (UCommonActivatableWidget* Widget = Binding.Widget.Get())
	{
		Widget->OnDeactivated().Remove(Binding.DelegateHandle);
	}
}

void URpgUIScreenSubsystem::ReleaseScreenDeactivationBindings(
	FGameplayTag ScreenTag,
	UCommonActivatableWidget* Widget)
{
	TArray<uint64> CheckoutIds;
	for (const TPair<uint64, FScreenDeactivationBinding>& Candidate :
		ScreenDeactivationBindings)
	{
		if (Candidate.Value.ScreenTag == ScreenTag &&
			Candidate.Value.Widget.Get() == Widget)
		{
			CheckoutIds.Add(Candidate.Key);
		}
	}

	for (const uint64 CheckoutId : CheckoutIds)
	{
		ReleaseScreenDeactivationBinding(CheckoutId);
		if (ActiveScreenCheckoutIds.FindRef(ScreenTag) == CheckoutId)
		{
			ActiveScreenCheckoutIds.Remove(ScreenTag);
			if (const TObjectPtr<UCommonActivatableWidget>* ActiveWidget =
				ActiveScreens.Find(ScreenTag))
			{
				if (ActiveWidget->Get() == Widget)
				{
					ActiveScreens.Remove(ScreenTag);
				}
			}
		}
	}
}

void URpgUIScreenSubsystem::ClearPendingScreenState(FGameplayTag ScreenTag)
{
	PendingPayloads.Remove(ScreenTag);
	PendingScreenTags.Remove(ScreenTag);
	PendingScreenLoads.Remove(ScreenTag);
}
