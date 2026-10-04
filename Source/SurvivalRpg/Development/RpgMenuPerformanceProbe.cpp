#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CommonActivatableWidget.h"
#include "CommonButtonBase.h"
#include "CommonAnimatedSwitcher.h"
#include "CommonLazyWidget.h"
#include "CommonTabListWidgetBase.h"
#include "Components/PanelWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "HAL/ThreadSafeCounter.h"
#include "Misc/CoreDelegates.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "String/LexFromString.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "SurvivalRpg/UI/RpgFrontendWidgets.h"
#include "SurvivalRpg/UI/RpgUIScreenSubsystem.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

DEFINE_LOG_CATEGORY_STATIC(LogRpgMenuPerformance, Log, All);

namespace RpgMenuPerformance
{
// This diagnostic deliberately discovers existing widget instances. Loading a
// Settings class here would invalidate the fresh-process first-open measurement.
constexpr TCHAR SettingsClassName[] = TEXT("CUI_SettingsMenu_C");
constexpr TCHAR ClickFunctionName[] = TEXT("HandleButtonClicked");

FString JsonString(FString Value)
{
	Value.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
	Value.ReplaceInline(TEXT("\""), TEXT("\\\""));
	Value.ReplaceInline(TEXT("\n"), TEXT("\\n"));
	Value.ReplaceInline(TEXT("\r"), TEXT("\\r"));
	Value.ReplaceInline(TEXT("\t"), TEXT("\\t"));
	return FString::Printf(TEXT("\"%s\""), *Value);
}

void Emit(const FString& Fields)
{
	UE_LOG(LogRpgMenuPerformance, Display, TEXT("RPG_MENU_PERF {%s}"), *Fields);
}

struct FOptions
{
	double TimeoutSeconds = 60.0;
	double OpenTimeoutSeconds = 20.0;
	double SettleSeconds = 1.0;
	double ObserveSeconds = 0.5;
	double MaxClickMs = 0.0;
	double MaxOpenMs = 0.0;
	double MaxFrameMs = 0.0;
	bool bQuit = false;
	bool bTabs = false;
};

bool ParseOptions(const TArray<FString>& Args, FOptions& Options)
{
	for (const FString& Arg : Args)
	{
		if (Arg.Equals(TEXT("tabs"), ESearchCase::IgnoreCase))
		{
			Options.bTabs = true;
			continue;
		}
		if (Arg.Equals(TEXT("quit"), ESearchCase::IgnoreCase))
		{
			Options.bQuit = true;
			continue;
		}
		FString Key;
		FString Value;
		double Number = 0.0;
		if (!Arg.Split(TEXT("="), &Key, &Value) ||
			!LexTryParseString(Number, *Value) || !FMath::IsFinite(Number))
		{
			return false;
		}
		if (Key == TEXT("timeout") && Number >= 5.0 && Number <= 600.0)
			Options.TimeoutSeconds = Number;
		else if (Key == TEXT("open_timeout") && Number >= 1.0 && Number <= 300.0)
			Options.OpenTimeoutSeconds = Number;
		else if (Key == TEXT("settle") && Number >= 0.0 && Number <= 30.0)
			Options.SettleSeconds = Number;
		else if (Key == TEXT("observe") && Number >= 0.1 && Number <= 30.0)
			Options.ObserveSeconds = Number;
		else if (Key == TEXT("max_click_ms") && Number >= 0.0)
			Options.MaxClickMs = Number;
		else if (Key == TEXT("max_open_ms") && Number >= 0.0)
			Options.MaxOpenMs = Number;
		else if (Key == TEXT("max_frame_ms") && Number >= 0.0)
			Options.MaxFrameMs = Number;
		else
			return false;
	}
	return true;
}

UCommonButtonBase* FindSettingsButton(UUserWidget* Root, TSet<UUserWidget*>& Visited)
{
	if (!Root || !Root->WidgetTree || Visited.Contains(Root))
	{
		return nullptr;
	}
	Visited.Add(Root);
	UCommonButtonBase* Result = nullptr;
	Root->WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		if (Result || !IsValid(Widget))
		{
			return;
		}
		if (Widget->GetFName() == TEXT("Button_Settings"))
		{
			UCommonButtonBase* Button = Cast<UCommonButtonBase>(Widget);
			if (Button && Button->IsInteractionEnabled() && Button->GetCachedWidget().IsValid())
			{
				Result = Button;
				return;
			}
		}
		// Stack pages are runtime pool entries, not nested WidgetTree children.
		if (UCommonActivatableWidgetContainerBase* Container = Cast<UCommonActivatableWidgetContainerBase>(Widget))
		{
			Result = FindSettingsButton(Container->GetActiveWidget(), Visited);
			if (Result) return;
		}
		if (UUserWidget* Child = Cast<UUserWidget>(Widget))
		{
			if (const UCommonActivatableWidget* Page = Cast<UCommonActivatableWidget>(Child);
				Page && !Page->IsActivated())
			{
				return;
			}
			Result = FindSettingsButton(Child, Visited);
		}
	});
	return Result;
}

// Follow displayed content, including lazy content outside the owning WidgetTree.
void VisitDisplayedWidgets(UWidget* Widget, TSet<UWidget*>& Visited, TFunctionRef<void(UWidget*)> Visit)
{
	if (!IsValid(Widget) || !Widget->IsVisible() || Visited.Contains(Widget)) return;
	Visited.Add(Widget);
	Visit(Widget);
	if (UCommonActivatableWidgetContainerBase* Container = Cast<UCommonActivatableWidgetContainerBase>(Widget))
		VisitDisplayedWidgets(Container->GetActiveWidget(), Visited, Visit);
	else if (UCommonLazyWidget* Lazy = Cast<UCommonLazyWidget>(Widget))
		VisitDisplayedWidgets(Lazy->GetContent(), Visited, Visit);
	else if (UUserWidget* User = Cast<UUserWidget>(Widget))
	{
		if (User->WidgetTree) VisitDisplayedWidgets(User->WidgetTree->RootWidget, Visited, Visit);
	}
	else if (UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(Widget))
		VisitDisplayedWidgets(Switcher->GetActiveWidget(), Visited, Visit);
	else if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
			VisitDisplayedWidgets(Panel->GetChildAt(Index), Visited, Visit);
	}
}

class FTabLogCounter : public FOutputDevice
{
public:
	virtual bool CanBeUsedOnAnyThread() const override { return true; }
	virtual void Serialize(const TCHAR*, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		if (Verbosity <= ELogVerbosity::Error) Errors.Increment();
		else if (Verbosity == ELogVerbosity::Warning && Category == TEXT("LogScript")) ScriptWarnings.Increment();
	}
	FThreadSafeCounter Errors;
	FThreadSafeCounter ScriptWarnings;
};

class FProbe : public TSharedFromThis<FProbe>
{
public:
	FProbe(UWorld* World, const FOptions& InOptions)
		: GameInstance(World ? World->GetGameInstance() : nullptr), Options(InOptions)
	{
	}

	~FProbe()
	{
		StopLogCapture();
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	}

	bool IsRunning() const { return !bFinished; }

	void Start()
	{
		StartedAt = LastTickAt = FPlatformTime::Seconds();
		Emit(TEXT("\"event\":\"start\",\"scope\":\"real_click_handler; close_by_DeactivateWidget; fresh_process_not_disk_cache\",\"timing\":\"first_active_is_ticker_observed_not_GPU_present\""));
		Emit(FString::Printf(TEXT("\"event\":\"options\",\"timeout_seconds\":%.3f,\"open_timeout_seconds\":%.3f,\"settle_seconds\":%.3f,\"observe_seconds\":%.3f,\"max_click_ms\":%.3f,\"max_open_ms\":%.3f,\"max_frame_ms\":%.3f,\"quit\":%s,\"tabs\":%s"),
			Options.TimeoutSeconds, Options.OpenTimeoutSeconds, Options.SettleSeconds, Options.ObserveSeconds,
			Options.MaxClickMs, Options.MaxOpenMs, Options.MaxFrameMs, Options.bQuit ? TEXT("true") : TEXT("false"), Options.bTabs ? TEXT("true") : TEXT("false")));
		WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddSP(AsShared(), &FProbe::OnWorldCleanup);
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateSP(AsShared(), &FProbe::Tick));
	}

	void Cancel()
	{
		Finish(false, TEXT("canceled"), false);
	}

private:
	enum class EPhase { WaitMenu, SettleMenu, WaitSettings, ObserveSettings, WaitClosed, WaitTabContent, ObserveTab };

	bool Tick(float)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RpgMenuPerformanceProbe);
		if (bFinished)
		{
			return false;
		}
		const double Now = FPlatformTime::Seconds();
		if (Phase == EPhase::WaitSettings || Phase == EPhase::ObserveSettings)
		{
			MaxTickGapMs = FMath::Max(MaxTickGapMs, (Now - LastTickAt) * 1000.0);
		}
		if (Phase == EPhase::WaitTabContent || Phase == EPhase::ObserveTab)
			TabMaxTickGapMs = FMath::Max(TabMaxTickGapMs, (Now - LastTickAt) * 1000.0);
		LastTickAt = Now;
		if (!GameInstance.IsValid() || (bBound && (!World.IsValid() || !LocalPlayer.IsValid())))
		{
			Finish(false, TEXT("world_or_local_player_lost"));
			return false;
		}
		if (Now - StartedAt > Options.TimeoutSeconds)
		{
			Finish(false, TEXT("run_timeout"));
			return false;
		}

		switch (Phase)
		{
		case EPhase::WaitMenu:
		case EPhase::SettleMenu:
			if (UCommonButtonBase* Button = ResolveMenuButton())
			{
				if (FindActiveSettings())
				{
					Finish(false, TEXT("settings_already_open"));
					break;
				}
				if (Phase == EPhase::WaitMenu)
				{
					Phase = EPhase::SettleMenu;
					PhaseStartedAt = Now;
				}
				else if (Now - PhaseStartedAt >= Options.SettleSeconds)
				{
					BeginOpen(Button);
				}
			}
			else
			{
				Phase = EPhase::WaitMenu;
			}
			break;
		case EPhase::WaitSettings:
			if (UCommonActivatableWidget* Settings = FindActiveSettings())
			{
				OpenedSettings = Settings;
				FirstActiveMs = (Now - ClickStartedAt) * 1000.0;
				Phase = EPhase::ObserveSettings;
				PhaseStartedAt = Now;
				TRACE_BOOKMARK(TEXT("RpgMenuPerformance %s active"), PassName());
				Emit(FString::Printf(TEXT("\"event\":\"active\",\"pass\":\"%s\",\"first_active_ms\":%.3f,\"widget\":%s"),
					PassName(), FirstActiveMs, *JsonString(Settings->GetPathName())));
				LogPageCounts(TEXT("active"));
			}
			else if (Now - ClickStartedAt > Options.OpenTimeoutSeconds)
			{
				Finish(false, TEXT("settings_open_timeout"));
			}
			break;
		case EPhase::ObserveSettings:
			if (!OpenedSettings.IsValid() || !OpenedSettings->IsActivated())
			{
				Finish(false, TEXT("settings_closed_during_observation"));
			}
			else if (Now - PhaseStartedAt >= Options.ObserveSeconds)
			{
				const bool bWithinBudget =
					(Options.MaxClickMs <= 0.0 || ClickMs <= Options.MaxClickMs) &&
					(Options.MaxOpenMs <= 0.0 || FirstActiveMs <= Options.MaxOpenMs) &&
					(Options.MaxFrameMs <= 0.0 || MaxTickGapMs <= Options.MaxFrameMs);
				bAllWithinBudget &= bWithinBudget;
				Emit(FString::Printf(TEXT("\"event\":\"measurement\",\"pass\":\"%s\",\"click_call_ms\":%.3f,\"first_active_ms\":%.3f,\"max_tick_gap_ms\":%.3f,\"within_budget\":%s"),
					PassName(), ClickMs, FirstActiveMs, MaxTickGapMs, bWithinBudget ? TEXT("true") : TEXT("false")));
				if (PassIndex == 1 && Options.bTabs) BeginTabs();
				else BeginClose();
			}
			break;
		case EPhase::WaitClosed:
			if (!FindActiveSettings() && ResolveMenuButton() && Now - PhaseStartedAt >= Options.SettleSeconds)
			{
				OpenedSettings.Reset();
				if (PassIndex == 0)
				{
					PassIndex = 1;
					Phase = EPhase::WaitMenu;
				}
				else
				{
					Finish(bAllWithinBudget && bNoTabRuntimeErrors, !bNoTabRuntimeErrors ? TEXT("tab_runtime_errors") :
						(bAllWithinBudget ? TEXT("completed") : TEXT("budget_exceeded")));
				}
			}
			else if (Now - PhaseStartedAt > Options.OpenTimeoutSeconds)
			{
				Finish(false, TEXT("menu_restore_timeout"));
			}
			break;
		case EPhase::WaitTabContent:
		case EPhase::ObserveTab:
			TickTab(Now);
			break;
		}
		return !bFinished;
	}

	void BeginClose()
	{
		StopLogCapture();
		Phase = EPhase::WaitClosed;
		PhaseStartedAt = FPlatformTime::Seconds();
		Emit(FString::Printf(TEXT("\"event\":\"close\",\"pass\":\"%s\",\"method\":\"DeactivateWidget\""), PassName()));
		bOpenRequested = false;
		if (OpenedSettings.IsValid()) OpenedSettings->DeactivateWidget();
	}

	void BeginTabs()
	{
		TSet<UWidget*> Visited;
		TArray<UCommonTabListWidgetBase*> Lists;
		VisitDisplayedWidgets(OpenedSettings.Get(), Visited, [&](UWidget* Widget)
		{
			if (UCommonTabListWidgetBase* List = Cast<UCommonTabListWidgetBase>(Widget);
				List && List->GetTabCount() > 0 && List->GetLinkedSwitcher()) Lists.Add(List);
		});
		if (Lists.Num() != 1)
		{
			Finish(false, TEXT("settings_tab_list_missing_or_ambiguous"));
			return;
		}
		TabList = Lists[0];
		for (int32 Index = 0; Index < TabList->GetTabCount(); ++Index)
		{
			const FName Id = TabList->GetTabIdAtIndex(Index);
			if (const UCommonButtonBase* Button = TabList->GetTabButtonBaseByID(Id);
				Button && Button->IsInteractionEnabled()) AvailableTabIds.Add(Id);
			else Emit(FString::Printf(TEXT("\"event\":\"tab_skipped\",\"tab\":%s,\"reason\":\"hidden_or_disabled\""), *JsonString(Id.ToString())));
		}
		if (AvailableTabIds.IsEmpty())
		{
			Finish(false, TEXT("no_available_settings_tabs"));
			return;
		}
		Emit(FString::Printf(TEXT("\"event\":\"tabs_start\",\"available_count\":%d,\"selection\":\"SelectTabByID\",\"readiness\":\"switcher_stable_and_lazy_content_loaded\",\"error_scope\":\"process_logs_during_each_tab_not_causal_attribution\""), AvailableTabIds.Num()));
		if (GLog)
		{
			GLog->AddOutputDevice(&TabLogs);
			bCapturingLogs = true;
		}
		BeginNextTab();
	}

	void BeginNextTab()
	{
		if (!TabList.IsValid())
		{
			Finish(false, TEXT("settings_tab_list_lost"));
			return;
		}
		if (TabIndex >= AvailableTabIds.Num())
		{
			BeginClose();
			return;
		}
		TabLogs.Errors.Reset();
		TabLogs.ScriptWarnings.Reset();
		TabMaxTickGapMs = 0.0;
		Phase = EPhase::WaitTabContent;
		TabStartedAt = LastTickAt = FPlatformTime::Seconds();
		const FName Id = AvailableTabIds[TabIndex];
		Emit(FString::Printf(TEXT("\"event\":\"tab_select\",\"tab\":%s"), *JsonString(Id.ToString())));
		TRACE_BOOKMARK(TEXT("RpgMenuPerformance tab %s select"), *Id.ToString());
		const bool bSelected = TabList->SelectTabByID(Id);
		TabSelectMs = (FPlatformTime::Seconds() - TabStartedAt) * 1000.0;
		if (!bSelected) Finish(false, TEXT("tab_selection_failed"));
	}

	bool IsTabContentReady(int32& LazyCount, int32& LoadedCount) const
	{
		if (!TabList.IsValid() || !AvailableTabIds.IsValidIndex(TabIndex) ||
			TabList->GetSelectedTabId() != AvailableTabIds[TabIndex] ||
			!TabList->HasTabContentWidget(AvailableTabIds[TabIndex])) return false;
		UCommonAnimatedSwitcher* Switcher = TabList->GetLinkedSwitcher();
		if (!Switcher || Switcher->IsCurrentlySwitching() || Switcher->IsTransitionPlaying()) return false;
		UWidget* Content = Switcher->GetActiveWidget();
		if (!IsValid(Content) || !Content->IsVisible() || !Content->GetCachedWidget().IsValid()) return false;
		if (const UCommonActivatableWidget* Page = Cast<UCommonActivatableWidget>(Content);
			Page && !Page->IsActivated()) return false;
		TSet<UWidget*> Visited;
		VisitDisplayedWidgets(Content, Visited, [&](UWidget* Widget)
		{
			if (UCommonLazyWidget* Lazy = Cast<UCommonLazyWidget>(Widget))
			{
				++LazyCount;
				UUserWidget* Loaded = Lazy->GetContent();
				const UCommonActivatableWidget* Page = Cast<UCommonActivatableWidget>(Loaded);
				if (!Lazy->IsLoading() && IsValid(Loaded) && Loaded->GetCachedWidget().IsValid() &&
					(!Page || Page->IsActivated())) ++LoadedCount;
			}
		});
		return LazyCount == LoadedCount;
	}

	void TickTab(double Now)
	{
		if (!OpenedSettings.IsValid() || !OpenedSettings->IsActivated() || !TabList.IsValid())
		{
			Finish(false, TEXT("settings_or_tabs_closed_during_observation"));
			return;
		}
		int32 LazyCount = 0;
		int32 LoadedCount = 0;
		const bool bReady = IsTabContentReady(LazyCount, LoadedCount);
		if (Now - TabStartedAt > Options.OpenTimeoutSeconds + Options.ObserveSeconds)
		{
			Emit(FString::Printf(TEXT("\"event\":\"tab_timeout\",\"tab\":%s,\"lazy_widgets\":%d,\"loaded_lazy_widgets\":%d,\"runtime_errors\":%d,\"script_warnings\":%d"),
				*JsonString(AvailableTabIds[TabIndex].ToString()), LazyCount, LoadedCount, TabLogs.Errors.GetValue(), TabLogs.ScriptWarnings.GetValue()));
			Finish(false, TEXT("tab_content_timeout"));
			return;
		}
		if (!bReady)
		{
			Phase = EPhase::WaitTabContent;
			return;
		}
		if (Phase == EPhase::WaitTabContent)
		{
			TabReadyMs = (Now - TabStartedAt) * 1000.0;
			PhaseStartedAt = Now;
			Phase = EPhase::ObserveTab;
			return;
		}
		if (Now - PhaseStartedAt < Options.ObserveSeconds) return;
		const bool bWithinBudget = (Options.MaxClickMs <= 0.0 || TabSelectMs <= Options.MaxClickMs) &&
			(Options.MaxOpenMs <= 0.0 || TabReadyMs <= Options.MaxOpenMs) &&
			(Options.MaxFrameMs <= 0.0 || TabMaxTickGapMs <= Options.MaxFrameMs);
		bAllWithinBudget &= bWithinBudget;
		bNoTabRuntimeErrors &= TabLogs.Errors.GetValue() == 0 && TabLogs.ScriptWarnings.GetValue() == 0;
		Emit(FString::Printf(TEXT("\"event\":\"tab_measurement\",\"tab\":%s,\"select_call_ms\":%.3f,\"content_ready_ms\":%.3f,\"max_tick_gap_ms\":%.3f,\"lazy_widgets\":%d,\"loaded_lazy_widgets\":%d,\"runtime_errors\":%d,\"script_warnings\":%d,\"within_budget\":%s"),
			*JsonString(AvailableTabIds[TabIndex].ToString()), TabSelectMs, TabReadyMs, TabMaxTickGapMs,
			LazyCount, LoadedCount, TabLogs.Errors.GetValue(), TabLogs.ScriptWarnings.GetValue(), bWithinBudget ? TEXT("true") : TEXT("false")));
		LogPageCounts(TEXT("tab_ready"));
		++TabIndex;
		BeginNextTab();
	}

	void StopLogCapture()
	{
		if (bCapturingLogs && GLog) GLog->RemoveOutputDevice(&TabLogs);
		bCapturingLogs = false;
	}

	UCommonButtonBase* ResolveMenuButton()
	{
		UGameInstance* Instance = GameInstance.Get();
		if (!Instance)
		{
			return nullptr;
		}
		for (TObjectIterator<URpgMainMenuStackWidget> It; It; ++It)
		{
			URpgMainMenuStackWidget* Root = *It;
			if (!IsValid(Root) || Root->IsTemplate() || !Root->IsActivated() ||
				Root->GetGameInstance() != Instance || !Root->GetOwningLocalPlayer() ||
				(bBound && (Root->GetWorld() != World.Get() || Root->GetOwningLocalPlayer() != LocalPlayer.Get())))
			{
				continue;
			}
			TSet<UUserWidget*> Visited;
			if (UCommonButtonBase* Button = FindSettingsButton(Root, Visited))
			{
				World = Root->GetWorld();
				LocalPlayer = Root->GetOwningLocalPlayer();
				bBound = true;
				return Button;
			}
		}
		return nullptr;
	}

	UCommonActivatableWidget* FindActiveSettings() const
	{
		for (TObjectIterator<UCommonActivatableWidget> It; It; ++It)
		{
			UCommonActivatableWidget* Widget = *It;
			if (IsValid(Widget) && !Widget->IsTemplate() && Widget->GetWorld() == World.Get() &&
				Widget->GetOwningLocalPlayer() == LocalPlayer.Get() && Widget->IsActivated() &&
				Widget->GetClass()->GetFName() == SettingsClassName)
			{
				return Widget;
			}
		}
		return nullptr;
	}

	void BeginOpen(UCommonButtonBase* Button)
	{
		UFunction* ClickFunction = Button->FindFunction(ClickFunctionName);
		if (!ClickFunction || ClickFunction->ParmsSize != 0)
		{
			Finish(false, TEXT("click_handler_unavailable"));
			return;
		}
		LogPageCounts(TEXT("before_click"));
		Emit(FString::Printf(TEXT("\"event\":\"click\",\"pass\":\"%s\",\"button\":%s"),
			PassName(), *JsonString(Button->GetPathName())));
		Phase = EPhase::WaitSettings;
		bOpenRequested = true;
		MaxTickGapMs = 0.0;
		ClickStartedAt = LastTickAt = FPlatformTime::Seconds();
		TRACE_BOOKMARK(TEXT("RpgMenuPerformance %s click"), PassName());
		Button->ProcessEvent(ClickFunction, nullptr);
		ClickMs = (FPlatformTime::Seconds() - ClickStartedAt) * 1000.0;
		// The synchronous path may create the screen before ProcessEvent returns.
		OpenedSettings = FindActiveSettings();
		Emit(FString::Printf(TEXT("\"event\":\"click_returned\",\"pass\":\"%s\",\"click_call_ms\":%.3f"), PassName(), ClickMs));
	}

	void LogPageCounts(const TCHAR* Moment) const
	{
		TMap<FString, FIntPoint> Counts;
		for (TObjectIterator<UUserWidget> It; It; ++It)
		{
			UUserWidget* Widget = *It;
			if (!IsValid(Widget) || Widget->IsTemplate() || Widget->GetWorld() != World.Get() ||
				Widget->GetOwningLocalPlayer() != LocalPlayer.Get())
			{
				continue;
			}
			const FString ClassPath = Widget->GetClass()->GetPathName();
			if (ClassPath.Contains(TEXT("/UI/SettingsMenu/ChildSettings/")))
			{
				FIntPoint& Count = Counts.FindOrAdd(ClassPath, FIntPoint::ZeroValue);
				++Count.X;
				Count.Y += Widget->GetCachedWidget().IsValid() ? 1 : 0;
			}
		}
		TArray<FString> Classes;
		Counts.GetKeys(Classes);
		Classes.Sort();
		FString Entries;
		for (const FString& ClassPath : Classes)
		{
			const FIntPoint Count = Counts.FindChecked(ClassPath);
			if (!Entries.IsEmpty()) Entries += TEXT(",");
			Entries += FString::Printf(TEXT("{\"class\":%s,\"live_instances\":%d,\"with_slate\":%d}"),
				*JsonString(ClassPath), Count.X, Count.Y);
		}
		Emit(FString::Printf(TEXT("\"event\":\"page_counts\",\"pass\":\"%s\",\"moment\":\"%s\",\"diagnostic_only\":true,\"pages\":[%s]"),
			PassName(), Moment, *Entries));
	}

	void OnWorldCleanup(UWorld* CleanupWorld, bool, bool)
	{
		if (bBound && CleanupWorld == World.Get())
		{
			bOpenRequested = false;
			OpenedSettings.Reset();
			Finish(false, TEXT("bound_world_cleanup"));
		}
	}

	void Finish(bool bSuccess, const TCHAR* Reason, bool bAllowQuit = true)
	{
		if (bFinished) return;
		bFinished = true;
		StopLogCapture();
		if (bOpenRequested && World.IsValid() && LocalPlayer.IsValid())
		{
			bOpenRequested = false;
			if (UCommonActivatableWidget* Settings = OpenedSettings.Get(); Settings &&
				Settings->GetWorld() == World.Get() && Settings->GetOwningLocalPlayer() == LocalPlayer.Get())
			{
				// Remove this exact probe-owned instance, including covered stack
				// entries. A same-tag page opened later is outside this probe's scope.
				bool bRemovedFromContainer = false;
				for (TObjectIterator<UCommonActivatableWidgetContainerBase> It; It; ++It)
				{
					if (IsValid(*It) && !It->IsTemplate() && It->GetWorld() == World.Get() &&
						It->GetWidgetList().Contains(Settings))
					{
						bRemovedFromContainer = true;
						It->RemoveWidget(*Settings);
						break;
					}
				}
				if (!bRemovedFromContainer && Settings->IsActivated()) Settings->DeactivateWidget();
			}
			else
			{
				// No instance has been observed yet: cancel only our outstanding
				// Settings route. The tag may be absent on the baseline build.
				const FGameplayTag SettingsTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Screen.Settings"), false);
				if (SettingsTag.IsValid())
				{
					if (URpgUIScreenSubsystem* Router = LocalPlayer->GetSubsystem<URpgUIScreenSubsystem>();
						Router && Router->IsScreenActiveOrPending(SettingsTag)) Router->CloseScreen(SettingsTag);
				}
			}
		}
		OpenedSettings.Reset();
		Emit(FString::Printf(TEXT("\"event\":\"result\",\"success\":%s,\"reason\":\"%s\",\"elapsed_seconds\":%.3f"),
			bSuccess ? TEXT("true") : TEXT("false"), Reason, FPlatformTime::Seconds() - StartedAt));
		TRACE_BOOKMARK(TEXT("RpgMenuPerformance finished %s"), Reason);
		if (Options.bQuit && bAllowQuit)
		{
			FPlatformMisc::RequestExitWithStatus(false, bSuccess ? 0 : 1);
		}
	}

	const TCHAR* PassName() const { return PassIndex == 0 ? TEXT("first") : TEXT("warm"); }

	TWeakObjectPtr<UGameInstance> GameInstance;
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ULocalPlayer> LocalPlayer;
	TWeakObjectPtr<UCommonActivatableWidget> OpenedSettings;
	TWeakObjectPtr<UCommonTabListWidgetBase> TabList;
	TArray<FName> AvailableTabIds;
	FTabLogCounter TabLogs;
	FOptions Options;
	FTSTicker::FDelegateHandle TickerHandle;
	FDelegateHandle WorldCleanupHandle;
	EPhase Phase = EPhase::WaitMenu;
	double StartedAt = 0.0;
	double LastTickAt = 0.0;
	double PhaseStartedAt = 0.0;
	double ClickStartedAt = 0.0;
	double ClickMs = 0.0;
	double FirstActiveMs = 0.0;
	double MaxTickGapMs = 0.0;
	double TabStartedAt = 0.0;
	double TabSelectMs = 0.0;
	double TabReadyMs = 0.0;
	double TabMaxTickGapMs = 0.0;
	int32 TabIndex = 0;
	int32 PassIndex = 0;
	bool bBound = false;
	bool bOpenRequested = false;
	bool bFinished = false;
	bool bAllWithinBudget = true;
	bool bCapturingLogs = false;
	bool bNoTabRuntimeErrors = true;
};

TSharedPtr<FProbe> ActiveProbe;

void Run(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() == 1 && Args[0].Equals(TEXT("cancel"), ESearchCase::IgnoreCase))
	{
		if (ActiveProbe) ActiveProbe->Cancel();
		ActiveProbe.Reset();
		return;
	}
	if (ActiveProbe && ActiveProbe->IsRunning())
	{
		Emit(TEXT("\"event\":\"rejected\",\"reason\":\"probe_already_running\""));
		return;
	}
	FOptions Options;
	if (!World || !World->IsGameWorld() || !World->GetGameInstance() || !ParseOptions(Args, Options))
	{
		Emit(TEXT("\"event\":\"rejected\",\"reason\":\"requires_game_world_and_valid_arguments\""));
		return;
	}
	ActiveProbe = MakeShared<FProbe>(World, Options);
	ActiveProbe->Start();
}

FAutoConsoleCommandWithWorldAndArgs MeasureSettingsCommand(
	TEXT("rpg.UI.MeasureSettings"),
	TEXT("Measures first and warm Settings clicks without preloading pages. Development only; no desktop input/final GPU-present claim. "
		"Args: quit, tabs, timeout=60, open_timeout=20, settle=1, observe=0.5, max_click_ms=0, max_open_ms=0, max_frame_ms=0. "
		"Optional tabs traverses available registered tabs after the warm open and waits for lazy content readiness. "
		"Zero budgets only report. Close uses DeactivateWidget. 'cancel' restores a probe-opened page and stops."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Run));

const FDelegateHandle PreExitHandle = FCoreDelegates::OnPreExit.AddLambda([]
{
	if (ActiveProbe) ActiveProbe->Cancel();
	ActiveProbe.Reset();
});
}

#endif // !UE_BUILD_SHIPPING && !UE_BUILD_TEST
