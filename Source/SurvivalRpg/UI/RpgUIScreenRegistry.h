#pragma once

#include "CommonActivatableWidget.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgUIScreenRegistry.generated.h"

/**
 * Static mapping from one semantic UI.Screen tag to a CommonGame layer and
 * activatable widget class. CommonUI retains lifecycle, focus, input, and
 * stacking authority for the resolved screen.
 */
USTRUCT(BlueprintType)
struct FRpgUIScreenRegistryEntry
{
	GENERATED_BODY()

	/** Unique semantic screen id. Must be a descendant of UI.Screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (Categories = "UI.Screen"))
	FGameplayTag ScreenTag;

	/** Exact CommonGame layer registered by URpgPrimaryGameLayout that receives this widget. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (Categories = "UI.Layer"))
	FGameplayTag LayerTag;

	/**
	 * Non-abstract activatable widget class streamed and pushed by CommonUI.
	 * This is immutable presentation configuration and never owns gameplay state.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowAbstract = "false"))
	TSoftClassPtr<UCommonActivatableWidget> WidgetClass;

	/** Whether CommonUI temporarily suspends owning-player input while this class streams. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	bool bSuspendInputUntilLoaded = true;

	/**
	 * Semantic UI.Screen tags are always single-instance per local player.
	 * Retained as visible serialized compatibility data for existing registries.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	bool bSingleInstance = true;
};

/**
 * DataAsset registry for opening UI by gameplay tag instead of by hard-coded widget class.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgUIScreenRegistry : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Returns the exact first registry entry for ScreenTag, if one exists. */
	bool FindScreen(FGameplayTag ScreenTag, FRpgUIScreenRegistryEntry& OutEntry) const;

	//~ UObject interface
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
	//~ End UObject interface

	/**
	 * Designer-authored, immutable screen composition. Screen tags must be
	 * unique; duplicate entries are invalid and FindScreen remains first-match.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TArray<FRpgUIScreenRegistryEntry> Screens;
};
