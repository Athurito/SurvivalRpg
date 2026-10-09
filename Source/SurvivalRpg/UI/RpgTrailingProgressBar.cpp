#include "RpgTrailingProgressBar.h"

#include "Widgets/Notifications/SProgressBar.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgTrailingProgressBar)

#define LOCTEXT_NAMESPACE "RpgTrailingProgressBar"

void FRpgProgressTrailState::SetTarget(const float InTarget)
{
	const float NewTarget = FMath::Clamp(InTarget, 0.0f, 1.0f);
	if (NewTarget >= Displayed)
	{
		Displayed = NewTarget;
	}
	else if (NewTarget < Target)
	{
		bRestartDelay = true;
	}
	Target = NewTarget;
}

bool FRpgProgressTrailState::Advance(const double Now, const float DeltaSeconds, const float DelaySeconds, const float DrainPerSecond)
{
	if (bRestartDelay)
	{
		DrainStart = Now + FMath::Max(0.0f, DelaySeconds);
		bRestartDelay = false;
	}
	if (Displayed <= Target)
	{
		Displayed = Target;
		return false;
	}
	if (Now >= DrainStart)
	{
		Displayed = FMath::Max(Target, Displayed - FMath::Max(0.0f, DeltaSeconds) * FMath::Max(0.01f, DrainPerSecond));
	}
	return Displayed > Target;
}

void URpgTrailingProgressBar::SetTargetPercent(const float InTargetPercent)
{
	TargetPercent = InTargetPercent;
	if (!bTrailInitialized || IsDesignTime())
	{
		// The first value is where the bar starts; nothing trails into it.
		TrailState.Displayed = TrailState.Target = FMath::Clamp(InTargetPercent, 0.0f, 1.0f);
		bTrailInitialized = !IsDesignTime();
	}
	else
	{
		TrailState.SetTarget(InTargetPercent);
	}
	SetPercent(TrailState.Displayed);
	EnsureTrailTimer();
}

void URpgTrailingProgressBar::SynchronizeProperties()
{
	if (!bTrailInitialized || IsDesignTime())
	{
		TrailState.Displayed = TrailState.Target = FMath::Clamp(TargetPercent, 0.0f, 1.0f);
	}
	SetPercent(TrailState.Displayed);
	Super::SynchronizeProperties();
	TrailTimerHandle.Reset();
	EnsureTrailTimer();
}

void URpgTrailingProgressBar::ReleaseSlateResources(const bool bReleaseChildren)
{
	if (const TSharedPtr<FActiveTimerHandle> Handle = TrailTimerHandle.Pin(); Handle && MyProgressBar)
	{
		MyProgressBar->UnRegisterActiveTimer(Handle.ToSharedRef());
	}
	TrailTimerHandle.Reset();
	Super::ReleaseSlateResources(bReleaseChildren);
}

#if WITH_EDITOR
const FText URpgTrailingProgressBar::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "RPG HUD");
}
#endif

void URpgTrailingProgressBar::EnsureTrailTimer()
{
	if (IsDesignTime() || !MyProgressBar || TrailTimerHandle.IsValid() || TrailState.Displayed <= TrailState.Target)
	{
		return;
	}
	TrailTimerHandle = MyProgressBar->RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateUObject(this, &ThisClass::HandleTrailTimer));
}

EActiveTimerReturnType URpgTrailingProgressBar::HandleTrailTimer(const double InCurrentTime, const float InDeltaTime)
{
	const bool bKeepGoing = TrailState.Advance(InCurrentTime, InDeltaTime, TrailDelaySeconds, TrailDrainPerSecond);
	SetPercent(TrailState.Displayed);
	if (!bKeepGoing)
	{
		TrailTimerHandle.Reset();
		return EActiveTimerReturnType::Stop;
	}
	return EActiveTimerReturnType::Continue;
}

#undef LOCTEXT_NAMESPACE
