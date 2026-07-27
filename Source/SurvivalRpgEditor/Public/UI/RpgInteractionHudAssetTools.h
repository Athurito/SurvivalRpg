#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "RpgInteractionHudAssetTools.generated.h"

/**
 * Editor-only, idempotent authoring helpers for the canonical interaction HUD assets.
 *
 * Runtime widgets remain presentation-only. These helpers rebuild only the approved
 * interaction prompt trees and wire them through the existing Experience/UIExtension path.
 */
UCLASS()
class SURVIVALRPGEDITOR_API URpgInteractionHudAssetTools final
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Authors the focus prompt, nearby marker, reticle, HUD extension point, and Experience registration. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Interaction|Editor")
	static bool AuthorInteractionHudAssets();

	/** Adds one designer-movable Default prompt anchor to each supported reference Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Interaction|Editor")
	static bool AuthorInteractionReferencePromptAnchors();
};
