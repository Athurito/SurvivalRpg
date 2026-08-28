#include "AI/RpgAIAssetTools.h"

#include "Components/StateTreeAIComponentSchema.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagContainer.h"
#include "StateTree.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditorData.h"
#include "StateTreeEditorSchema.h"
#include "StateTreeState.h"
#include "SurvivalRpg/Combat/RpgEnemyCombatLoadout.h"
#include "SurvivalRpg/Core/AI/RpgAIController.h"
#include "SurvivalRpg/Core/AI/RpgAIPawnData.h"
#include "SurvivalRpg/Core/AI/RpgStateTreeFindHostileTargetTask.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgAIAssetTools)

static void EnsureRpgAIStateTreeEditorData(UStateTree& StateTree, UStateTreeEditorData*& OutEditorData)
{
	UClass* SchemaClass = UStateTreeAIComponentSchema::StaticClass();

	OutEditorData = Cast<UStateTreeEditorData>(StateTree.EditorData);
	if (!OutEditorData)
	{
		OutEditorData = NewObject<UStateTreeEditorData>(&StateTree, UStateTreeEditorData::StaticClass(), NAME_None, RF_Transactional);
		StateTree.EditorData = OutEditorData;
	}

	OutEditorData->Modify();

	if (!OutEditorData->Schema || !OutEditorData->Schema->IsA(SchemaClass))
	{
		OutEditorData->Schema = NewObject<UStateTreeSchema>(OutEditorData, SchemaClass, NAME_None, RF_Transactional);
	}

	if (!OutEditorData->EditorSchema)
	{
		OutEditorData->EditorSchema = NewObject<UStateTreeEditorSchema>(OutEditorData, UStateTreeEditorSchema::StaticClass(), NAME_None, RF_Transactional);
	}
}

bool URpgAIAssetTools::RebuildRiftGruntStateTree(UStateTree* StateTree)
{
	if (!StateTree)
	{
		UE_LOG(LogTemp, Warning, TEXT("RebuildRiftGruntStateTree called without a StateTree asset."));
		return false;
	}

	StateTree->Modify();

	UStateTreeEditorData* EditorData = nullptr;
	EnsureRpgAIStateTreeEditorData(*StateTree, EditorData);
	if (!EditorData)
	{
		return false;
	}

	EditorData->Evaluators.Reset();
	EditorData->GlobalTasks.Reset();
	EditorData->SubTrees.Reset();

	UStateTreeState& Root = EditorData->AddSubTree(TEXT("RiftGrunt"));
	Root.ID = FGuid::NewGuid();
	Root.Description = TEXT("Acquire the nearest hostile pawn, chase into sword range, and let the AI attack driver activate primary weapon attacks.");
	Root.SelectionBehavior = EStateTreeStateSelectionBehavior::TryEnterState;
	Root.TasksCompletion = EStateTreeTaskCompletionType::All;

	TStateTreeEditorNode<FRpgStateTreeFindHostileTargetTask>& FindTargetTask = Root.AddTask<FRpgStateTreeFindHostileTargetTask>();
	FRpgStateTreeFindHostileTargetTaskInstanceData& TaskData = FindTargetTask.GetInstanceData();
	TaskData.SearchRadius = 3000.0f;
	TaskData.AttackRange = 180.0f;
	TaskData.MoveAcceptableRadius = 140.0f;
	TaskData.RepathInterval = 0.25f;
	TaskData.bMoveTowardTarget = true;
	TaskData.bSetAttackDriverTarget = true;

	FStateTreeCompilerLog CompileLog;
	FStateTreeCompiler Compiler(CompileLog);
	const bool bCompiled = Compiler.Compile(StateTree);
	if (!bCompiled)
	{
		CompileLog.DumpToLog(LogTemp);
		return false;
	}

	StateTree->MarkPackageDirty();
	return true;
}

bool URpgAIAssetTools::ConfigureRiftGruntPawnDataAndController(
	URpgAIPawnData* PawnData,
	TSubclassOf<APawn> PawnClass,
	UStateTree* StateTree,
	TSubclassOf<ARpgAIController> ControllerClass,
	const URpgEnemyCombatLoadoutDefinition* BasicSwordLoadout)
{
	if (!PawnData || !PawnClass || !StateTree || !BasicSwordLoadout)
	{
		UE_LOG(LogTemp, Warning, TEXT("ConfigureRiftGruntPawnDataAndController requires PawnData, PawnClass, StateTree, and BasicSwordLoadout."));
		return false;
	}

	PawnData->Modify();
	PawnData->PawnClass = PawnClass;
	PawnData->StateTree = StateTree;
	PawnData->TeamId = 2;

	PawnData->RoleTags.Reset();
	if (const FGameplayTag RoleTag = FGameplayTag::RequestGameplayTag(TEXT("AI.Role.Grunt"), false); RoleTag.IsValid())
	{
		PawnData->RoleTags.AddTag(RoleTag);
	}

	PawnData->FactionTags.Reset();
	if (const FGameplayTag FactionTag = FGameplayTag::RequestGameplayTag(TEXT("Faction.Enemy"), false); FactionTag.IsValid())
	{
		PawnData->FactionTags.AddTag(FactionTag);
	}

	PawnData->MarkPackageDirty();

	if (ControllerClass)
	{
		if (ARpgAIController* ControllerCDO = Cast<ARpgAIController>(ControllerClass->GetDefaultObject()))
		{
			ControllerCDO->SetDefaultPawnDataForEditor(PawnData);
		}
	}

	const FGameplayTag BasicSwordTag = FGameplayTag::RequestGameplayTag(TEXT("Enemy.Archetype.BasicSword"), false);
	if (!BasicSwordTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("ConfigureRiftGruntPawnDataAndController could not find Enemy.Archetype.BasicSword."));
		return false;
	}

	APawn* PawnCDO = PawnClass->GetDefaultObject<APawn>();
	if (!PawnCDO)
	{
		UE_LOG(LogTemp, Warning, TEXT("ConfigureRiftGruntPawnDataAndController could not read PawnClass CDO."));
		return false;
	}

	URpgEnemyCombatArchetypeComponent* ArchetypeComponent = URpgEnemyCombatArchetypeComponent::FindEnemyCombatArchetypeComponent(PawnCDO);
	if (!ArchetypeComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("ConfigureRiftGruntPawnDataAndController could not find an enemy combat archetype component on %s."), *GetNameSafe(PawnClass));
		return false;
	}
	ArchetypeComponent->SetEnemyCombatArchetypeTagForEditor(BasicSwordTag);

	URpgEnemyCombatLoadoutComponent* LoadoutComponent = PawnCDO->FindComponentByClass<URpgEnemyCombatLoadoutComponent>();
	if (!LoadoutComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("ConfigureRiftGruntPawnDataAndController could not find an enemy combat loadout component on %s."), *GetNameSafe(PawnClass));
		return false;
	}

	TArray<TSoftObjectPtr<const URpgEnemyCombatLoadoutDefinition>> LoadoutDefinitions;
	LoadoutDefinitions.Add(BasicSwordLoadout);
	LoadoutComponent->ConfigureEnemyCombatLoadoutForEditor(BasicSwordTag, LoadoutDefinitions);

	return true;
}

bool URpgAIAssetTools::ValidateRiftGruntCombatDefaults(
	TSubclassOf<APawn> PawnClass,
	const URpgEnemyCombatLoadoutDefinition* BasicSwordLoadout)
{
	if (!PawnClass || !BasicSwordLoadout)
	{
		UE_LOG(LogTemp, Warning, TEXT("ValidateRiftGruntCombatDefaults requires PawnClass and BasicSwordLoadout."));
		return false;
	}

	const FGameplayTag BasicSwordTag = FGameplayTag::RequestGameplayTag(TEXT("Enemy.Archetype.BasicSword"), false);
	if (!BasicSwordTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("ValidateRiftGruntCombatDefaults could not find Enemy.Archetype.BasicSword."));
		return false;
	}

	APawn* PawnCDO = PawnClass->GetDefaultObject<APawn>();
	if (!PawnCDO)
	{
		UE_LOG(LogTemp, Warning, TEXT("ValidateRiftGruntCombatDefaults could not read PawnClass CDO."));
		return false;
	}

	if (!URpgEnemyCombatArchetypeComponent::FindEnemyCombatArchetypeComponent(PawnCDO))
	{
		UE_LOG(LogTemp, Warning, TEXT("ValidateRiftGruntCombatDefaults could not find an enemy combat archetype component on %s."), *GetNameSafe(PawnClass));
		return false;
	}

	const URpgEnemyCombatLoadoutComponent* LoadoutComponent = PawnCDO->FindComponentByClass<URpgEnemyCombatLoadoutComponent>();
	if (!LoadoutComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("ValidateRiftGruntCombatDefaults could not find an enemy combat loadout component on %s."), *GetNameSafe(PawnClass));
		return false;
	}

	if (LoadoutComponent->GetDefaultArchetypeTagForEditor() != BasicSwordTag)
	{
		UE_LOG(LogTemp, Warning, TEXT("ValidateRiftGruntCombatDefaults found DefaultArchetypeTag '%s' instead of '%s'."),
			*LoadoutComponent->GetDefaultArchetypeTagForEditor().ToString(),
			*BasicSwordTag.ToString());
		return false;
	}

	const TArray<TSoftObjectPtr<const URpgEnemyCombatLoadoutDefinition>>& LoadoutDefinitions = LoadoutComponent->GetLoadoutDefinitionsForEditor();
	for (const TSoftObjectPtr<const URpgEnemyCombatLoadoutDefinition>& LoadoutDefinition : LoadoutDefinitions)
	{
		if (LoadoutDefinition.Get() == BasicSwordLoadout || LoadoutDefinition.ToSoftObjectPath() == FSoftObjectPath(BasicSwordLoadout))
		{
			return true;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("ValidateRiftGruntCombatDefaults did not find DA_EnemyLoadout_BasicSword on %s."), *GetNameSafe(PawnClass));
	return false;
}
