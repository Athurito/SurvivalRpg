#pragma once

#include "RpgItemizationProfile.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"

#include "RpgItemizationAutomationTestTypes.generated.h"

/** Editor-only deterministic Epic profile used by loot/itemization automation tests. */
UCLASS(NotBlueprintable, Transient)
class URpgItemizationAutomationTestProfile final : public URpgItemizationProfile
{
	GENERATED_BODY()

public:
	explicit URpgItemizationAutomationTestProfile(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};

/** Editor-only non-stackable definition with a production Itemization fragment. */
UCLASS(NotBlueprintable, Transient)
class URpgItemizationAutomationTestItemDefinition final
	: public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgItemizationAutomationTestItemDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};
