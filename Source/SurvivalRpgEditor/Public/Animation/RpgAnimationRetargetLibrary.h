#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RpgAnimationRetargetLibrary.generated.h"

class UAnimSequence;

/** Editor-only operations for updating curated animation poses without replacing gameplay metadata. */
UCLASS()
class SURVIVALRPGEDITOR_API URpgAnimationRetargetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Exports the sequence's preserved authoring metadata for before/after retarget validation.
	 * Includes full rich curves, typed animated attributes, notify events and owned notify settings,
	 * preview/retarget references, root-motion/compression settings, sync markers and asset metadata.
	 * Excludes authored bone tracks and derived caches. Read-only; does not load or save referenced assets.
	 * Returns false and clears OutMetadata for a missing sequence, skeleton, model or invalid attribute data.
	 */
	UFUNCTION(BlueprintCallable, Category = "RPG|Editor|Animation")
	static bool ExportPreservedMetadata(UAnimSequence* Sequence, FString& OutMetadata);

	/**
	 * Copies authored non-root bone tracks from a retarget candidate with the same skeleton and sample grid.
	 * Preserves the destination root track, curves, notifies, sequence settings and asset identity.
	 * Validates the complete track set before editing; participates in Undo and never saves either asset.
	 */
	UFUNCTION(BlueprintCallable, Category = "RPG|Editor|Animation")
	static bool CopyRetargetedBoneTracks(UAnimSequence* Candidate, UAnimSequence* Destination, FString& OutError);
};
