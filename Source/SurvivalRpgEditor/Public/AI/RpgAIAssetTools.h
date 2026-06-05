#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RpgAIAssetTools.generated.h"

class ARpgAIController;
class URpgAbilitySet;
class URpgAIPawnData;
class URpgEnemyCombatLoadoutDefinition;
class UStateTree;

UCLASS()
class SURVIVALRPGEDITOR_API URpgAIAssetTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Rpg|AI|Editor")
	static bool RebuildRiftGruntStateTree(UStateTree* StateTree);

	UFUNCTION(BlueprintCallable, Category = "Rpg|AI|Editor")
	static bool ConfigureRiftGruntPawnDataAndController(
		URpgAIPawnData* PawnData,
		TSubclassOf<APawn> PawnClass,
		UStateTree* StateTree,
		const URpgAbilitySet* CoreAbilitySet,
		TSubclassOf<ARpgAIController> ControllerClass,
		const URpgEnemyCombatLoadoutDefinition* BasicSwordLoadout);

	UFUNCTION(BlueprintCallable, Category = "Rpg|AI|Editor")
	static bool ValidateRiftGruntCombatDefaults(
		TSubclassOf<APawn> PawnClass,
		const URpgEnemyCombatLoadoutDefinition* BasicSwordLoadout);
};
