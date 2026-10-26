#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RpgAIAssetTools.generated.h"

class ARpgAIController;
class URpgAIPawnData;
class URpgEnemyCombatLoadoutDefinition;
class UStateTree;

/** Editor-only authoring helpers for project-owned AI definitions and fixtures. */
UCLASS()
class SURVIVALRPGEDITOR_API URpgAIAssetTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Rebuilds the project-owned Rift Grunt StateTree with the standard chase-and-attack task. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|AI|Editor")
	static bool RebuildRiftGruntStateTree(UStateTree* StateTree);

	/**
	 * Replaces a StateTree's evaluators, global tasks and states with a JSON description and compiles it, because the
	 * engine's StateTree toolset only inspects. Nodes are named by script struct path and their parameters set as Unreal
	 * text values; the format is documented on the Python tool build_state_tree. Returns an empty string on success,
	 * otherwise the errors, and leaves the asset unsaved.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|AI|Editor")
	static FString BuildStateTreeFromJson(UStateTree* StateTree, const FString& DescriptionJson);

	/**
	 * Configures Rift Grunt PawnData, controller, and loadout ownership without changing startup AbilitySets.
	 * Shared combat grants are composed by GF_Combat_Core and must not be copied into project-owned PawnData.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|AI|Editor")
	static bool ConfigureRiftGruntPawnDataAndController(
		URpgAIPawnData* PawnData,
		TSubclassOf<APawn> PawnClass,
		UStateTree* StateTree,
		TSubclassOf<ARpgAIController> ControllerClass,
		const URpgEnemyCombatLoadoutDefinition* BasicSwordLoadout);

	/** Validates the Rift Grunt pawn class and its designer-authored basic-sword loadout defaults. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|AI|Editor")
	static bool ValidateRiftGruntCombatDefaults(
		TSubclassOf<APawn> PawnClass,
		const URpgEnemyCombatLoadoutDefinition* BasicSwordLoadout);
};
