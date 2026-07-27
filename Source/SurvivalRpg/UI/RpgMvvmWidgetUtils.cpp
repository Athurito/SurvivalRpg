#include "RpgMvvmWidgetUtils.h"

#include "Blueprint/UserWidget.h"
#include "MVVMSubsystem.h"
#include "MVVMViewModelBase.h"
#include "View/MVVMView.h"
#include "View/MVVMViewClass.h"

DEFINE_LOG_CATEGORY_STATIC(LogRpgMvvmWidgetUtils, Log, All);

bool RpgMvvmWidgetUtils::SetOptionalManualViewModel(
	UUserWidget* Widget,
	FName SourceName,
	UMVVMViewModelBase* ViewModel,
	UClass* ExpectedViewModelClass)
{
	if (!Widget || SourceName.IsNone() || !ExpectedViewModelClass ||
		(ViewModel && !ViewModel->IsA(ExpectedViewModelClass)))
	{
		return false;
	}

	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(Widget);
	const UMVVMViewClass* ViewClass = View ? View->GetViewClass() : nullptr;
	if (!View || !ViewClass)
	{
		UE_LOG(
			LogRpgMvvmWidgetUtils,
			Error,
			TEXT("%s has no compiled MVVM view for required manual source %s."),
			*GetNameSafe(Widget),
			*SourceName.ToString());
		return false;
	}

	const FMVVMViewClass_Source* CompiledSource =
		ViewClass->GetSources().FindByPredicate(
			[SourceName](const FMVVMViewClass_Source& Candidate)
			{
				return Candidate.IsViewModel() &&
					Candidate.GetName() == SourceName;
			});
	if (!CompiledSource ||
		!CompiledSource->CanBeSet() ||
		!CompiledSource->IsOptional() ||
		CompiledSource->GetSourceClass() != ExpectedViewModelClass)
	{
		UE_LOG(
			LogRpgMvvmWidgetUtils,
			Error,
			TEXT("%s requires one settable optional manual MVVM source named %s with exact type %s."),
			*GetNameSafe(Widget),
			*SourceName.ToString(),
			*GetNameSafe(ExpectedViewModelClass));
		return false;
	}

	if (View->GetViewModel(SourceName).GetObject() == ViewModel)
	{
		return true;
	}

	TScriptInterface<INotifyFieldValueChanged> ViewModelInterface;
	if (ViewModel)
	{
		ViewModelInterface.SetObject(ViewModel);
		ViewModelInterface.SetInterface(ViewModel);
	}

	if (!View->SetViewModel(SourceName, ViewModelInterface))
	{
		UE_LOG(
			LogRpgMvvmWidgetUtils,
			Error,
			TEXT("%s failed to assign manual MVVM source %s."),
			*GetNameSafe(Widget),
			*SourceName.ToString());
		return false;
	}

	return View->GetViewModel(SourceName).GetObject() == ViewModel;
}
