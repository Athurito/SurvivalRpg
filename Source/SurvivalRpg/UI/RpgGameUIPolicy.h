#pragma once

#include "GameUIPolicy.h"

#include "RpgGameUIPolicy.generated.h"

class UPrimaryGameLayout;

/**
 * Project CommonGame policy with a native root-layout fallback.
 *
 * Designers can still subclass UGameUIPolicy in Blueprint later; this class exists so the
 * CommonGame stack is usable before a dedicated policy asset has been created.
 */
UCLASS(Blueprintable, Config = Game)
class SURVIVALRPG_API URpgGameUIPolicy : public UGameUIPolicy
{
	GENERATED_BODY()

public:
	URpgGameUIPolicy();

	virtual void PostInitProperties() override;

protected:
	/** Root layout class created for each CommonLocalPlayer. Defaults to URpgPrimaryGameLayout. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "UI")
	TSoftClassPtr<UPrimaryGameLayout> RootLayoutClass;

private:
	void ApplyRootLayoutClass();
};
