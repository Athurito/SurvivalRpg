#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RpgBlueprintAssetTools.generated.h"

class UBlueprint;

/** Reusable editor operations for authoring Blueprint contracts through Unreal MCP/Python. */
UCLASS()
class SURVIVALRPGEDITOR_API URpgBlueprintAssetTools final : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Adds an interface with Unreal's native graph setup; idempotent for inherited or existing interfaces. Compile and save the Blueprint afterwards. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Blueprint|Editor")
	static bool ImplementInterface(UBlueprint* Blueprint, TSubclassOf<UInterface> InterfaceClass);

	/**
	 * Remaps serialized object references only in this asset and its nested Outer-owned objects.
	 * Uses Unreal's replacement archive, including protected properties and instanced struct data.
	 * Hard references and resolvable weak/soft references are supported; unresolved soft paths and
	 * arbitrary string paths require separate validation. Outer/archetype references are preserved.
	 * The caller supplies non-null, type-compatible replacements without mapping chains or cycles.
	 * Package roots and package replacements are rejected. The archive does not traverse external objects
	 * or package-sibling Blueprint generated classes/CDOs. Normal PostEditChange notifications still run
	 * and may refresh loaded referencers; callers must restrict those referencers to their authorized assets.
	 * Makes owned objects transactional, records undo
	 * when the editor transactor is available, and marks changed content dirty. Does not compile, save,
	 * rename, or consolidate assets. Compile Blueprints afterwards.
	 * Returns the archive's replacement count, zero for no changes, or -1 for invalid arguments.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Blueprint|Editor")
	static int32 RemapOwnedObjectReferences(UObject* Asset, const TMap<UObject*, UObject*>& Replacements);
};
