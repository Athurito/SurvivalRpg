#pragma once

#include "GameUIPolicy.h"

#include "RpgGameUIPolicy.generated.h"

class UPrimaryGameLayout;

/**
 * Project CommonGame policy that resolves the configured, Blueprint-authored root layout.
 *
 * The native layout class remains a deliberate emergency fallback when project config cannot load the authored asset.
 */
UCLASS(Blueprintable, Config = Game)
class SURVIVALRPG_API URpgGameUIPolicy : public UGameUIPolicy
{
	GENERATED_BODY()

public:
	URpgGameUIPolicy();

	virtual void PostInitProperties() override;

protected:
	/** Root layout class created for each CommonLocalPlayer; project config points to CUI_RpgPrimaryGameLayout. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "UI")
	TSoftClassPtr<UPrimaryGameLayout> RootLayoutClass;

private:
	void ApplyRootLayoutClass();
};
