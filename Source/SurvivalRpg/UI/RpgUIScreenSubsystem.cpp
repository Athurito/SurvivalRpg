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
	CloseAllScreens();
	Super::Deinitialize();
}

void URpgUIScreenSubsystem::CloseAllScreens()
{
	if (bIsClosingAllScreens)
	{
		return;
	}
	TGuardValue<bool> DrainGuard(bIsClosingAllScreens, true);
	++ScreenGeneration;

	TArray<TSharedPtr<FStreamableHandle>> StreamingHandles;
	PendingScreenLoads.GenerateValueArray(StreamingHandles);
	for (const TSharedPtr<FStreamableHandle>& StreamingHandle : StreamingHandles)
	{
		if (StreamingHandle.IsValid() && !StreamingHandle->HasLoadCompleted())
		{
			StreamingHandle->CancelHandle();
		}
	}
	// Completed loads can lack a cancel delegate. Keep their queued completion
	// so CommonGame resumes input; the generation check discards the stale push.

	TArray<FScreenDeactivationBinding> ScreensToRelease;
	ScreenDeactivationBindings.GenerateValueArray(ScreensToRelease);
	TArray<uint64> CheckoutIds;
	ScreenDeactivationBindings.GenerateKeyArray(CheckoutIds);
	for (const uint64 CheckoutId : CheckoutIds)
	{
		ReleaseScreenDeactivationBinding(CheckoutId);
	}
	TMap<UCommonActivatableWidgetContainerBase*, float> TransitionDurations;
	for (const FScreenDeactivationBinding& Screen : ScreensToRelease)
	{
		if (UCommonActivatableWidgetContainerBase* Layer = Screen.Layer.Get())
		{
			if (!TransitionDurations.Contains(Layer))
			{
				TransitionDurations.Add(Layer, Layer->GetTransitionDuration());
				Layer->SetTransitionDuration(0.0f);
			}
		}
	}
	// Remove covered entries first so removing the visible page cannot reactivate
	// a screen whose owning frontend or local-player lifetime has ended.
	for (const bool bRemoveVisible : {false, true})
	{
		for (const FScreenDeactivationBinding& Screen : ScreensToRelease)
		{
			UCommonActivatableWidget* Widget = Screen.Widget.Get();
			if (!Widget)
			{
				continue;
			}
			if (UCommonActivatableWidgetContainerBase* Layer = Screen.Layer.Get())
			{
				if ((Layer->GetActiveWidget() == Widget) == bRemoveVisible)
				{
					if (bRemoveVisible && Layer->GetWidgetList().Num() == 1 &&
						Layer->GetWidgetList().Contains(Widget))
					{
						// Also completes removal when the last owned page already
						// deactivated into an unfinished transition. Never clear
						// a layer that still contains an unrelated direct push.
						Layer->ClearWidgets();
					}
					else
					{
						Layer->RemoveWidget(*Widget);
					}
				}
			}
			else if (bRemoveVisible)
			{
				Widget->DeactivateWidget();
			}
		}
	}
	for (const TPair<UCommonActivatableWidgetContainerBase*, float>& Layer : TransitionDurations)
	{
		Layer.Key->SetTransitionDuration(Layer.Value);
	}

	ActiveScreens.Reset();
	ActiveScreenCheckoutIds.Reset();
	ScreenDeactivationBindings.Reset();
	PendingPayloads.Reset();
	PendingScreenTags.Reset();
	PendingScreenLoads.Reset();
	CanceledPendingScreenTags.Reset();
}

UCommonActivatableWidget* URpgUIScreenSubsystem::OpenScreen(FGameplayTag ScreenTag, UObject* Payload)
{
	if (bIsDeinitializing || bIsClosingAllScreens)
	{
		return nullptr;
	}

	if (!ScreenTag.IsValid())
	{
		UE_LOG(LogRpgUIScreenSubsystem, Warning, TEXT("OpenScreen called with an invalid ScreenTag."));
		return nullptr;
	}

	if (PendingScreenTags.Contains(ScreenTag))
	{
		return nullptr;
	}
	if (UCommonActivatableWidget* ExistingWidget = GetOpenScreen(ScreenTag))
	{
		const FScreenDeactivationBinding* Binding =
			ScreenDeactivationBindings.Find(ActiveScreenCheckoutIds.FindRef(ScreenTag));
		if (Binding && Binding->bClosing)
		{
			return nullptr;
		}
		ApplyPayloadToWidget(ExistingWidget, Payload);
		return ExistingWidget;
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

	UCommonActivatableWidgetContainerBase* Layer = RootLayout->GetLayerWidget(Entry.LayerTag);
	if (!Layer)
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
	const TWeakObjectPtr<UPrimaryGameLayout> WeakRootLayout(RootLayout);
	const FGameplayTag RequestedLayerTag = Entry.LayerTag;
	const uint64 RequestGeneration = ScreenGeneration;
	TSharedPtr<FStreamableHandle> StreamingHandle =
		RootLayout->PushWidgetToLayerStackAsync<UCommonActivatableWidget>(
		Entry.LayerTag,
		Entry.bSuspendInputUntilLoaded,
		Entry.WidgetClass,
		[WeakThis, WeakRootLayout, RequestedLayerTag, RequestGeneration, ScreenTag](EAsyncWidgetLayerState State, UCommonActivatableWidget* Widget)
		{
			if (URpgUIScreenSubsystem* ScreenSubsystem = WeakThis.Get())
			{
				UPrimaryGameLayout* CurrentLayout = WeakRootLayout.Get();
				UCommonActivatableWidgetContainerBase* CurrentLayer = CurrentLayout
					? CurrentLayout->GetLayerWidget(RequestedLayerTag) : nullptr;
				ScreenSubsystem->HandleScreenPushState(RequestGeneration, ScreenTag, State, Widget, CurrentLayer);
			}
		});

	// RequestAsyncLoad can complete inline for an already-loaded class. Only retain
	// the handle when the completion callback has not already cleared this tag.
	if (RequestGeneration == ScreenGeneration && PendingScreenTags.Contains(ScreenTag) && StreamingHandle.IsValid())
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
	if (bIsDeinitializing || bIsClosingAllScreens)
	{
		return nullptr;
	}

	if (UCommonActivatableWidget* ActiveWidget = GetOpenScreen(ScreenTag))
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
	if (bIsDeinitializing || bIsClosingAllScreens)
	{
		return;
	}

	if (UCommonActivatableWidget* ActiveWidget = GetOpenScreen(ScreenTag))
	{
		const uint64 CheckoutId = ActiveScreenCheckoutIds.FindRef(ScreenTag);
		FScreenDeactivationBinding* Binding = ScreenDeactivationBindings.Find(CheckoutId);
		if (Binding)
		{
			Binding->bClosing = true;
			if (UCommonActivatableWidgetContainerBase* Layer = Binding->Layer.Get())
			{
				Layer->RemoveWidget(*ActiveWidget);
				// Removing a covered entry does not change the displayed widget.
				HandleScreenLayerChanged(Layer->GetActiveWidget(), CheckoutId);
				return;
			}
		}
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
	UCommonActivatableWidget* Widget = GetOpenScreen(ScreenTag);
	return Widget && Widget->IsActivated() ? Widget : nullptr;
}

bool URpgUIScreenSubsystem::IsScreenActiveOrPending(FGameplayTag ScreenTag) const
{
	return PendingScreenTags.Contains(ScreenTag) || GetActiveScreen(ScreenTag) != nullptr;
}

UCommonActivatableWidget* URpgUIScreenSubsystem::GetOpenScreen(FGameplayTag ScreenTag) const
{
	const FScreenDeactivationBinding* Binding =
		ScreenDeactivationBindings.Find(ActiveScreenCheckoutIds.FindRef(ScreenTag));
	if (Binding)
	{
		UCommonActivatableWidget* Widget = Binding->Widget.Get();
		if (const UCommonActivatableWidgetContainerBase* Layer = Binding->Layer.Get())
		{
			return Layer->GetWidgetList().Contains(Widget) ? Widget : nullptr;
		}
		return Widget && Widget->IsActivated() ? Widget : nullptr;
	}
	return nullptr;
}

bool URpgUIScreenSubsystem::IsScreenOpenOrPending(FGameplayTag ScreenTag) const
{
	return PendingScreenTags.Contains(ScreenTag) || GetOpenScreen(ScreenTag) != nullptr;
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
	uint64 RequestGeneration,
	FGameplayTag ScreenTag,
	EAsyncWidgetLayerState State,
	UCommonActivatableWidget* Widget,
	UCommonActivatableWidgetContainerBase* Layer)
{
	if (bIsDeinitializing || bIsClosingAllScreens || RequestGeneration != ScreenGeneration)
	{
		if (State == EAsyncWidgetLayerState::AfterPush && Widget)
		{
			// A pooled UObject may already belong to a newer checkout. A late
			// callback owns only its old request, never that replacement screen.
			for (const TPair<uint64, FScreenDeactivationBinding>& Binding : ScreenDeactivationBindings)
			{
				if (Binding.Value.Widget.Get() == Widget)
				{
					return;
				}
			}
			TGuardValue<bool> DiscardGuard(bIsClosingAllScreens, true);
			if (Layer)
			{
				const float TransitionDuration = Layer->GetTransitionDuration();
				Layer->SetTransitionDuration(0.0f);
				if (Layer->GetWidgetList().Num() == 1 && Layer->GetWidgetList().Contains(Widget))
				{
					Layer->ClearWidgets();
				}
				else
				{
					Layer->RemoveWidget(*Widget);
				}
				Layer->SetTransitionDuration(TransitionDuration);
			}
			else if (UPrimaryGameLayout* RootLayout = GetPrimaryGameLayout())
			{
				RootLayout->FindAndRemoveWidgetFromLayer(Widget);
			}
			else
			{
				Widget->DeactivateWidget();
			}
		}
		// The drain already released this generation. In particular, a delayed
		// cancel must not clear a same-tag request opened by the next map.
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
		// Designer payload handlers can synchronously travel or close the UI.
		// Do not attach a new checkout after such a drain has invalidated us.
		if (bIsDeinitializing || bIsClosingAllScreens || RequestGeneration != ScreenGeneration ||
			CanceledPendingScreenTags.Contains(ScreenTag))
		{
			return;
		}
		const uint64 CheckoutId =
			RegisterScreenDeactivationBinding(ScreenTag, Widget, Layer);
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
	FScreenDeactivationBinding* Binding =
		ScreenDeactivationBindings.Find(CheckoutId);
	if (!Binding ||
		Binding->ScreenTag != ScreenTag ||
		Binding->Widget.Get() != Widget)
	{
		return;
	}
	if (const UCommonActivatableWidgetContainerBase* Layer = Binding->Layer.Get())
	{
		// Deactivation also happens when another page covers this one. The
		// container's displayed-change event runs after its membership update,
		// so only that event can distinguish a pop from a retained history entry.
		const TArray<UCommonActivatableWidget*>& Widgets = Layer->GetWidgetList();
		if (Widgets.Num() > 0 && Widgets.Last() == Widget)
		{
			Binding->bClosing = true;
		}
		return;
	}
	CompleteScreenCheckout(CheckoutId);
}

void URpgUIScreenSubsystem::HandleScreenLayerChanged(
	UCommonActivatableWidget* DisplayedWidget,
	uint64 CheckoutId)
{
	const FScreenDeactivationBinding* Binding = ScreenDeactivationBindings.Find(CheckoutId);
	if (!Binding)
	{
		return;
	}
	const UCommonActivatableWidgetContainerBase* Layer = Binding->Layer.Get();
	if (!Layer || !Layer->GetWidgetList().Contains(Binding->Widget.Get()))
	{
		CompleteScreenCheckout(CheckoutId);
	}
}

void URpgUIScreenSubsystem::HandleScreenDestructed(UUserWidget* Widget, uint64 CheckoutId)
{
	// Slate rebuilding is not a close while the container still retains this
	// checkout. A removed pooled widget must no longer retain router callbacks.
	HandleScreenLayerChanged(nullptr, CheckoutId);
}

void URpgUIScreenSubsystem::CompleteScreenCheckout(uint64 CheckoutId)
{
	const FScreenDeactivationBinding* Binding = ScreenDeactivationBindings.Find(CheckoutId);
	if (!Binding)
	{
		return;
	}
	const FGameplayTag ScreenTag = Binding->ScreenTag;
	UCommonActivatableWidget* Widget = Binding->Widget.Get();

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
	UCommonActivatableWidget* Widget,
	UCommonActivatableWidgetContainerBase* Layer)
{
	check(Widget);

	// A removed entry can return from the pool inside another screen's
	// activation before the container emits its displayed-change notification.
	// Retire that checkout before attaching callbacks to the reused UObject.
	TArray<uint64> ReusedCheckoutIds;
	for (const TPair<uint64, FScreenDeactivationBinding>& Existing : ScreenDeactivationBindings)
	{
		if (Existing.Value.Widget.Get() == Widget && Existing.Value.Layer.IsValid())
		{
			ReusedCheckoutIds.Add(Existing.Key);
		}
	}
	for (const uint64 CheckoutId : ReusedCheckoutIds)
	{
		CompleteScreenCheckout(CheckoutId);
	}

	++NextScreenCheckoutId;
	if (NextScreenCheckoutId == 0)
	{
		++NextScreenCheckoutId;
	}

	FScreenDeactivationBinding Binding;
	Binding.ScreenTag = ScreenTag;
	Binding.Widget = Widget;
	Binding.Layer = Layer;
	Binding.DelegateHandle = Widget->OnDeactivated().AddUObject(
		this,
		&ThisClass::HandleScreenDeactivated,
		ScreenTag,
		Widget,
		NextScreenCheckoutId);
	Binding.DestructDelegateHandle = Widget->OnNativeDestruct.AddUObject(
		this, &ThisClass::HandleScreenDestructed, NextScreenCheckoutId);
	if (Layer)
	{
		Binding.LayerDelegateHandle = Layer->OnDisplayedWidgetChanged().AddUObject(
			this, &ThisClass::HandleScreenLayerChanged, NextScreenCheckoutId);
	}
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
		Widget->OnNativeDestruct.Remove(Binding.DestructDelegateHandle);
	}
	if (UCommonActivatableWidgetContainerBase* Layer = Binding.Layer.Get())
	{
		Layer->OnDisplayedWidgetChanged().Remove(Binding.LayerDelegateHandle);
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
