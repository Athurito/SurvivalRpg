#pragma once

#include "CoreMinimal.h"

class UMVVMViewModelBase;
class UUserWidget;

/**
 * Small native helpers for the project's named, manually supplied MVVM sources.
 *
 * Screen and list-entry presenters own their view-model instances. Widget Blueprints only declare the exact
 * optional source and read-only bindings; they never create or discover gameplay-facing view models themselves.
 */
namespace RpgMvvmWidgetUtils
{
	/**
	 * Assigns or clears one exact optional manual source on an authored widget.
	 *
	 * Returns false when the widget has no compiled MVVM view, the source contract is missing or ambiguous, or the
	 * supplied object does not match ExpectedViewModelClass.
	 */
	SURVIVALRPG_API bool SetOptionalManualViewModel(
		UUserWidget* Widget,
		FName SourceName,
		UMVVMViewModelBase* ViewModel,
		UClass* ExpectedViewModelClass);
}
