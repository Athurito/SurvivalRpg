#pragma once

#include "GameUIPolicy.h"

#include "RpgGameUIPolicy.generated.h"

class UPrimaryGameLayout;

/**
 * Project CommonGame policy that resolves the configured, Blueprint-authored root layout.
 *
 * Project config is the only composition authority. A missing authored class fails closed instead of constructing a
 * parallel native layout.
 */
UCLASS(Blueprintable, Config = Game)
class SURVIVALRPG_API URpgGameUIPolicy : public UGameUIPolicy
{
	GENERATED_BODY()

public:
	virtual void PostInitProperties() override;

protected:
	/** Required authored root layout class created for each CommonLocalPlayer; configured as CUI_RpgPrimaryGameLayout. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "UI")
	TSoftClassPtr<UPrimaryGameLayout> RootLayoutClass;

private:
	void ApplyRootLayoutClass();
};
