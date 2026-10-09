#include "RpgHudFadeBox.h"

#include "Widgets/Layout/SBox.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgHudFadeBox)

#define LOCTEXT_NAMESPACE "RpgHudFadeBox"

void FRpgHudFadeState::SetPinned(const bool bInPinned)
{
	if (bPinned && !bInPinned)
	{
		bRestartHold = true;
	}
	bPinned = bInPinned;
}

void FRpgHudFadeState::Pulse()
{
	bRestartHold = true;
}

float FRpgHudFadeState::GetTargetOpacity(const double Now, const FRpgHudFadeSettings& Settings) const
{
	return bPinned || bRestartHold || Now < HoldUntil ? 1.0f : FMath::Clamp(Settings.HiddenOpacity, 0.0f, 1.0f);
}

bool FRpgHudFadeState::Advance(const double Now, const float DeltaSeconds, const FRpgHudFadeSettings& Settings)
{
	if (bRestartHold)
	{
		HoldUntil = Now + FMath::Max(0.0f, Settings.HoldSeconds);
		bRestartHold = false;
	}

	const float Target = GetTargetOpacity(Now, Settings);
	const float FadeSeconds = Target > Opacity ? Settings.FadeInSeconds : Settings.FadeOutSeconds;
	const float Step = FadeSeconds > UE_KINDA_SMALL_NUMBER ? FMath::Max(0.0f, DeltaSeconds) / FadeSeconds : 1.0f;
	Opacity = Target > Opacity ? FMath::Min(Target, Opacity + Step) : FMath::Max(Target, Opacity - Step);

	const bool bHolding = !bPinned && Now < HoldUntil;
	return Opacity != Target || bHolding;
}

void URpgHudFadeBox::SetPinned(const bool bInPinned)
{
	FadeState.SetPinned(bInPinned);
	EnsureFadeTimer();
}

void URpgHudFadeBox::Pulse()
{
	FadeState.Pulse();
	EnsureFadeTimer();
}

void URpgHudFadeBox::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (!IsDesignTime())
	{
		SetRenderOpacity(FadeState.Opacity);
	}
}

void URpgHudFadeBox::ReleaseSlateResources(const bool bReleaseChildren)
{
	if (const TSharedPtr<FActiveTimerHandle> Handle = FadeTimerHandle.Pin(); Handle && MySizeBox)
	{
		MySizeBox->UnRegisterActiveTimer(Handle.ToSharedRef());
	}
	FadeTimerHandle.Reset();
	Super::ReleaseSlateResources(bReleaseChildren);
}

#if WITH_EDITOR
const FText URpgHudFadeBox::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "RPG HUD");
}
#endif

TSharedRef<SWidget> URpgHudFadeBox::RebuildWidget()
{
	TSharedRef<SWidget> Widget = Super::RebuildWidget();
	FadeTimerHandle.Reset();
	EnsureFadeTimer();
	return Widget;
}

FRpgHudFadeSettings URpgHudFadeBox::MakeSettings() const
{
	FRpgHudFadeSettings Settings;
	Settings.HiddenOpacity = HiddenOpacity;
	Settings.HoldSeconds = HoldSeconds;
	Settings.FadeInSeconds = FadeInSeconds;
	Settings.FadeOutSeconds = FadeOutSeconds;
	return Settings;
}

void URpgHudFadeBox::EnsureFadeTimer()
{
	if (IsDesignTime() || !MySizeBox || FadeTimerHandle.IsValid())
	{
		return;
	}
	FadeTimerHandle = MySizeBox->RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateUObject(this, &ThisClass::HandleFadeTimer));
}

EActiveTimerReturnType URpgHudFadeBox::HandleFadeTimer(const double InCurrentTime, const float InDeltaTime)
{
	const bool bKeepGoing = FadeState.Advance(InCurrentTime, InDeltaTime, MakeSettings());
	SetRenderOpacity(FadeState.Opacity);
	if (!bKeepGoing)
	{
		FadeTimerHandle.Reset();
		return EActiveTimerReturnType::Stop;
	}
	return EActiveTimerReturnType::Continue;
}

#undef LOCTEXT_NAMESPACE
