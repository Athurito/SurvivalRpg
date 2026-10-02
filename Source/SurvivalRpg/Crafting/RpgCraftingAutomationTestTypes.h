#pragma once

#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "RpgCraftingAutomationTestTypes.generated.h"

/** Editor-only hook that exercises synchronous station commands during inventory grant staging. */
UCLASS(NotBlueprintable, Transient)
class URpgCraftingAutomationReentrantFragment final : public URpgInventoryItemFragment
{
	GENERATED_BODY()
public:
	virtual bool IsEditorOnly() const override { return true; }
	virtual void OnInstanceCreated(URpgInventoryItemInstance* Instance) const override;
	static TFunction<void()> OnCreate;
};

/** Editor-only one-cell product carrying the crafting reentrancy test hook. */
UCLASS(NotBlueprintable, Transient)
class URpgCraftingAutomationReentrantProduct final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()
public:
	URpgCraftingAutomationReentrantProduct(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual bool IsEditorOnly() const override { return true; }
};
