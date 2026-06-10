#include "RpgUIScreenSubsystem.h"

#include "CommonActivatableWidget.h"
#include "PrimaryGameLayout.h"
#include "RpgUIScreenPayload.h"
#include "RpgUIScreenRegistry.h"
#include "RpgUISettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogRpgUIScreenSubsystem, Log, All);

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

	if (Entry.bSingleInstance)
	{
		if (UCommonActivatableWidget* ExistingWidget = GetActiveScreen(ScreenTag))
		{
			ApplyPayloadToWidget(ExistingWidget, Payload);
			return ExistingWidget;
		}

		if (PendingScreenTags.Contains(ScreenTag))
		{
			return nullptr;
		}
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return nullptr;
	}

	UPrimaryGameLayout* RootLayout = UPrimaryGameLayout::GetPrimaryGameLayout(LocalPlayer);
	if (!RootLayout)
	{
		UE_LOG(LogRpgUIScreenSubsystem, Warning, TEXT("Cannot open [%s]: no PrimaryGameLayout exists for the local player."), *ScreenTag.ToString());
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

	RootLayout->PushWidgetToLayerStackAsync<UCommonActivatableWidget>(
		Entry.LayerTag,
		Entry.bSuspendInputUntilLoaded,
		Entry.WidgetClass,
		[this, ScreenTag](EAsyncWidgetLayerState State, UCommonActivatableWidget* Widget)
		{
			if (State == EAsyncWidgetLayerState::Canceled)
			{
				PendingPayloads.Remove(ScreenTag);
				PendingScreenTags.Remove(ScreenTag);
				return;
			}

			if (State == EAsyncWidgetLayerState::Initialize && Widget)
			{
				ActiveScreens.Add(ScreenTag, Widget);

				UObject* PayloadToApply = nullptr;
				if (TObjectPtr<UObject>* PendingPayload = PendingPayloads.Find(ScreenTag))
				{
					PayloadToApply = PendingPayload->Get();
				}

				ApplyPayloadToWidget(Widget, PayloadToApply);
				Widget->OnDeactivated().AddUObject(this, &ThisClass::HandleScreenDeactivated, ScreenTag, Widget);
				return;
			}

			if (State == EAsyncWidgetLayerState::AfterPush)
			{
				PendingPayloads.Remove(ScreenTag);
				PendingScreenTags.Remove(ScreenTag);
			}
		});

	return nullptr;
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

void URpgUIScreenSubsystem::HandleScreenDeactivated(FGameplayTag ScreenTag, UCommonActivatableWidget* Widget)
{
	if (const TObjectPtr<UCommonActivatableWidget>* FoundWidget = ActiveScreens.Find(ScreenTag))
	{
		if (FoundWidget->Get() == Widget)
		{
			ActiveScreens.Remove(ScreenTag);
		}
	}

	PendingPayloads.Remove(ScreenTag);
	PendingScreenTags.Remove(ScreenTag);
}
