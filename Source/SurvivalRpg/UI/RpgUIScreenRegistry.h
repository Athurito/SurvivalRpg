#pragma once

#include "CommonActivatableWidget.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgUIScreenRegistry.generated.h"

/**
 * One project screen mapping from a semantic UI.Screen tag to a CommonGame layer and widget class.
 */
USTRUCT(BlueprintType)
struct FRpgUIScreenRegistryEntry
{
	GENERATED_BODY()

	/** Semantic screen id used by code, input, interaction, and old wrapper libraries. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (Categories = "UI.Screen"))
	FGameplayTag ScreenTag;

	/** CommonGame layer tag that receives the widget. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (Categories = "UI.Layer"))
	FGameplayTag LayerTag;

	/** Activatable widget pushed to the layer. Prefer widgets derived from URpgActivatableWidget. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowAbstract = "false"))
	TSoftClassPtr<UCommonActivatableWidget> WidgetClass;

	/** Temporarily suspends owning-player input while async widget loading is in flight. */
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
	/** Returns the exact registry entry for ScreenTag, if one exists. */
	bool FindScreen(FGameplayTag ScreenTag, FRpgUIScreenRegistryEntry& OutEntry) const;

	/** Designer-authored UI screen mappings. Later entries with duplicate tags are ignored by FindScreen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TArray<FRpgUIScreenRegistryEntry> Screens;
};
