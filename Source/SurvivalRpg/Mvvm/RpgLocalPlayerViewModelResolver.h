#pragma once

#include "CoreMinimal.h"
#include "View/MVVMViewModelContextResolver.h"

#include "RpgLocalPlayerViewModelResolver.generated.h"

/**
 * Viewmodel resolver for the view models URpgUiSubsystem keeps per local player, such as
 * URpgCharacterStatsViewModel. A widget that adds one of them with the Resolver creation type gets the owning
 * player's shared instance, already bound to its gameplay state.
 *
 * Config/DefaultModelViewViewModel.ini makes this the default resolver, so the editor and the MVVM toolset select it
 * when such a view model is added to a widget.
 */
UCLASS(DisplayName = "Local Player Viewmodel")
class SURVIVALRPG_API URpgLocalPlayerViewModelResolver : public UMVVMViewModelContextResolver
{
	GENERATED_BODY()

public:
	virtual UObject* CreateInstance(const UClass* ExpectedType, const UUserWidget* UserWidget, const UMVVMView* View) const override;

	/** The subsystem owns the instance, so a widget going away does not destroy it. */
	virtual void DestroyInstance(UObject* ViewModel, const UMVVMView* View) const override {}

#if WITH_EDITOR
	virtual bool DoesSupportViewModelClass(const UClass* Class) const override;
#endif
};
