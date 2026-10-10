#pragma once

#include "CoreMinimal.h"
#include "SurvivalRpg/UI/RpgMvvmListEntryWidgets.h"

#include "RpgHudAutomationTestTypes.generated.h"

/** Concrete row for automation tests of URpgViewModelEntryBox. Not used by content. */
UCLASS(NotBlueprintable, HideDropdown)
class URpgHudAutomationTestEntry final : public URpgMvvmListEntryWidget
{
	GENERATED_BODY()
};
