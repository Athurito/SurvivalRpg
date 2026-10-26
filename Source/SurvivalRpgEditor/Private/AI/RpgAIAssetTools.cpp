#include "AI/RpgAIAssetTools.h"

#include "Components/StateTreeAIComponentSchema.h"
#include "Dom/JsonObject.h"
#include "Logging/TokenizedMessage.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagContainer.h"
#include "StateTree.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeConditionBase.h"
#include "StateTreeEditorData.h"
#include "StateTreeEditorSchema.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeState.h"
#include "StateTreeTaskBase.h"
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

namespace RpgStateTreeBuilder
{
	struct FBuildContext
	{
		UStateTreeEditorData* EditorData = nullptr;
		TMap<FName, UStateTreeState*> StatesByName;
		TArray<TPair<UStateTreeState*, TSharedPtr<FJsonObject>>> StatesWithJson;
		TArray<FString> Errors;
	};

	template <typename TEnum>
	bool ParseEnum(const FString& Text, TEnum& OutValue, const FString& Where, FBuildContext& Context)
	{
		const int64 Value = StaticEnum<TEnum>()->GetValueByNameString(Text);
		if (Value == INDEX_NONE)
		{
			Context.Errors.Add(FString::Printf(TEXT("%s: unknown %s '%s'"), *Where, *StaticEnum<TEnum>()->GetName(), *Text));
			return false;
		}
		OutValue = static_cast<TEnum>(Value);
		return true;
	}

	bool JsonToText(const TSharedPtr<FJsonValue>& Value, FString& OutText)
	{
		switch (Value.IsValid() ? Value->Type : EJson::None)
		{
		case EJson::String:
			OutText = Value->AsString();
			return true;
		case EJson::Number:
			OutText = FString::SanitizeFloat(Value->AsNumber());
			return true;
		case EJson::Boolean:
			OutText = Value->AsBool() ? TEXT("True") : TEXT("False");
			return true;
		default:
			return false;
		}
	}

	/** Imports one Unreal text value into the property of that name, looking at the instance data before the node. */
	void SetNodeValues(FStateTreeEditorNode& Node, const TSharedPtr<FJsonObject>& Values, const FString& Where, FBuildContext& Context)
	{
		if (!Values.IsValid())
		{
			return;
		}

		const FStateTreeDataView InstanceView = Node.GetInstance();
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Values->Values)
		{
			FString Text;
			if (!JsonToText(Pair.Value, Text))
			{
				Context.Errors.Add(FString::Printf(TEXT("%s.%s: values must be strings, numbers or booleans"), *Where, *Pair.Key));
				continue;
			}

			const UStruct* Struct = InstanceView.GetStruct();
			void* Memory = InstanceView.GetMutableMemory();
			FProperty* Property = Struct ? FindFProperty<FProperty>(Struct, *Pair.Key) : nullptr;
			if (!Property)
			{
				Struct = Node.Node.GetScriptStruct();
				Memory = Node.Node.GetMutableMemory();
				Property = Struct ? FindFProperty<FProperty>(Struct, *Pair.Key) : nullptr;
			}
			if (!Property || !Memory)
			{
				Context.Errors.Add(FString::Printf(TEXT("%s: no property '%s'"), *Where, *Pair.Key));
				continue;
			}
			if (!Property->ImportText_Direct(*Text, Property->ContainerPtrToValuePtr<void>(Memory), nullptr, PPF_None))
			{
				Context.Errors.Add(FString::Printf(TEXT("%s.%s: cannot import '%s'"), *Where, *Pair.Key, *Text));
			}
		}
	}

	void AddNodes(
		TArray<FStateTreeEditorNode>& Nodes,
		UObject* Outer,
		const TArray<TSharedPtr<FJsonValue>>* NodeJsons,
		const UScriptStruct* RequiredBase,
		const FString& Where,
		FBuildContext& Context)
	{
		if (!NodeJsons)
		{
			return;
		}

		for (int32 Index = 0; Index < NodeJsons->Num(); ++Index)
		{
			const FString NodeWhere = FString::Printf(TEXT("%s[%d]"), *Where, Index);
			const TSharedPtr<FJsonObject> NodeJson = (*NodeJsons)[Index]->AsObject();
			FString StructPath;
			if (!NodeJson.IsValid() || !NodeJson->TryGetStringField(TEXT("struct"), StructPath))
			{
				Context.Errors.Add(FString::Printf(TEXT("%s: needs a struct path"), *NodeWhere));
				continue;
			}

			const UScriptStruct* NodeStruct = FindObject<UScriptStruct>(nullptr, *StructPath);
			if (!NodeStruct || !NodeStruct->IsChildOf(RequiredBase))
			{
				Context.Errors.Add(FString::Printf(TEXT("%s: '%s' is not a %s"), *NodeWhere, *StructPath, *RequiredBase->GetName()));
				continue;
			}

			FStateTreeEditorNode& Node = Nodes.AddDefaulted_GetRef();
			Node.InitializeAs(Outer, NodeStruct);
			FString Operand;
			if (NodeJson->TryGetStringField(TEXT("operand"), Operand))
			{
				ParseEnum(Operand, Node.ExpressionOperand, NodeWhere, Context);
			}
			int32 Indent = 0;
			if (NodeJson->TryGetNumberField(TEXT("indent"), Indent))
			{
				Node.ExpressionIndent = static_cast<uint8>(FMath::Clamp(Indent, 0, 4));
			}
			const TSharedPtr<FJsonObject>* Values = nullptr;
			if (NodeJson->TryGetObjectField(TEXT("values"), Values))
			{
				SetNodeValues(Node, *Values, NodeWhere, Context);
			}
		}
	}

	void BuildState(UStateTreeState* Parent, const TSharedPtr<FJsonObject>& StateJson, FBuildContext& Context)
	{
		FString Name;
		if (!StateJson.IsValid() || !StateJson->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
		{
			Context.Errors.Add(TEXT("Every state needs a name"));
			return;
		}
		if (Context.StatesByName.Contains(FName(Name)))
		{
			Context.Errors.Add(FString::Printf(TEXT("State name '%s' is used twice; transitions address states by name"), *Name));
			return;
		}

		EStateTreeStateType Type = EStateTreeStateType::State;
		FString TypeText;
		if (StateJson->TryGetStringField(TEXT("type"), TypeText))
		{
			ParseEnum(TypeText, Type, Name, Context);
		}

		UStateTreeState& State = Parent ? Parent->AddChildState(FName(Name), Type) : Context.EditorData->AddSubTree(FName(Name), Type);
		State.ID = FGuid::NewGuid();
		Context.StatesByName.Add(FName(Name), &State);
		Context.StatesWithJson.Emplace(&State, StateJson);

		FString Text;
		if (StateJson->TryGetStringField(TEXT("description"), Text))
		{
			State.Description = Text;
		}
		if (StateJson->TryGetStringField(TEXT("selection"), Text))
		{
			ParseEnum(Text, State.SelectionBehavior, Name, Context);
		}
		if (StateJson->TryGetStringField(TEXT("tasks_completion"), Text))
		{
			ParseEnum(Text, State.TasksCompletion, Name, Context);
		}

		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		AddNodes(State.EnterConditions, &State, StateJson->TryGetArrayField(TEXT("enter_conditions"), Array) ? Array : nullptr,
			FStateTreeConditionBase::StaticStruct(), Name + TEXT(".enter_conditions"), Context);
		AddNodes(State.Tasks, &State, StateJson->TryGetArrayField(TEXT("tasks"), Array) ? Array : nullptr,
			FStateTreeTaskBase::StaticStruct(), Name + TEXT(".tasks"), Context);

		if (StateJson->TryGetArrayField(TEXT("children"), Array))
		{
			for (const TSharedPtr<FJsonValue>& Child : *Array)
			{
				BuildState(&State, Child->AsObject(), Context);
			}
		}
	}

	void BuildTransitions(UStateTreeState& State, const TSharedPtr<FJsonObject>& StateJson, FBuildContext& Context)
	{
		const TArray<TSharedPtr<FJsonValue>>* Transitions = nullptr;
		if (!StateJson->TryGetArrayField(TEXT("transitions"), Transitions))
		{
			return;
		}

		for (int32 Index = 0; Index < Transitions->Num(); ++Index)
		{
			const FString Where = FString::Printf(TEXT("%s.transitions[%d]"), *State.Name.ToString(), Index);
			const TSharedPtr<FJsonObject> Json = (*Transitions)[Index]->AsObject();
			FString Text;
			EStateTreeTransitionTrigger Trigger = EStateTreeTransitionTrigger::OnStateCompleted;
			if (!Json.IsValid() || !Json->TryGetStringField(TEXT("trigger"), Text) || !ParseEnum(Text, Trigger, Where, Context))
			{
				Context.Errors.Add(FString::Printf(TEXT("%s: needs a trigger"), *Where));
				continue;
			}

			// "#Succeeded", "#Failed", "#NextState" or "#NextSelectableState" end or advance; any other text names a state.
			EStateTreeTransitionType Type = EStateTreeTransitionType::GotoState;
			UStateTreeState* Target = nullptr;
			if (!Json->TryGetStringField(TEXT("to"), Text))
			{
				Context.Errors.Add(FString::Printf(TEXT("%s: needs a target in 'to'"), *Where));
				continue;
			}
			if (Text.StartsWith(TEXT("#")))
			{
				if (!ParseEnum(Text.RightChop(1), Type, Where, Context))
				{
					continue;
				}
			}
			else if (UStateTreeState* const* Found = Context.StatesByName.Find(FName(Text)))
			{
				Target = *Found;
			}
			else
			{
				Context.Errors.Add(FString::Printf(TEXT("%s: no state named '%s'"), *Where, *Text));
				continue;
			}

			FGameplayTag EventTag;
			if (Json->TryGetStringField(TEXT("event"), Text))
			{
				EventTag = FGameplayTag::RequestGameplayTag(FName(Text), false);
				if (!EventTag.IsValid())
				{
					Context.Errors.Add(FString::Printf(TEXT("%s: unknown event tag '%s'"), *Where, *Text));
					continue;
				}
			}

			FStateTreeTransition& Transition = EventTag.IsValid()
				? State.AddTransition(Trigger, EventTag, Type, Target)
				: State.AddTransition(Trigger, Type, Target);
			if (Json->TryGetStringField(TEXT("priority"), Text))
			{
				ParseEnum(Text, Transition.Priority, Where, Context);
			}
			double Delay = 0.0;
			if (Json->TryGetNumberField(TEXT("delay"), Delay) && Delay > 0.0)
			{
				Transition.bDelayTransition = true;
				Transition.DelayDuration = static_cast<float>(Delay);
				double Variance = 0.0;
				Json->TryGetNumberField(TEXT("delay_variance"), Variance);
				Transition.DelayRandomVariance = static_cast<float>(FMath::Max(0.0, Variance));
			}

			const TArray<TSharedPtr<FJsonValue>>* Conditions = nullptr;
			AddNodes(Transition.Conditions, &State, Json->TryGetArrayField(TEXT("conditions"), Conditions) ? Conditions : nullptr,
				FStateTreeConditionBase::StaticStruct(), Where + TEXT(".conditions"), Context);
		}
	}
}

FString URpgAIAssetTools::BuildStateTreeFromJson(UStateTree* StateTree, const FString& DescriptionJson)
{
	using namespace RpgStateTreeBuilder;
	if (!StateTree)
	{
		return TEXT("No StateTree asset");
	}

	TSharedPtr<FJsonObject> Description;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(DescriptionJson), Description) || !Description.IsValid())
	{
		return TEXT("The description is not a JSON object");
	}
	const TSharedPtr<FJsonObject>* RootJson = nullptr;
	if (!Description->TryGetObjectField(TEXT("root"), RootJson))
	{
		return TEXT("The description needs a root state");
	}

	StateTree->Modify();
	FBuildContext Context;
	EnsureRpgAIStateTreeEditorData(*StateTree, Context.EditorData);
	if (!Context.EditorData)
	{
		return TEXT("The StateTree has no editor data");
	}

	Context.EditorData->Evaluators.Reset();
	Context.EditorData->GlobalTasks.Reset();
	Context.EditorData->SubTrees.Reset();

	const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
	AddNodes(Context.EditorData->Evaluators, Context.EditorData, Description->TryGetArrayField(TEXT("evaluators"), Array) ? Array : nullptr,
		FStateTreeEvaluatorBase::StaticStruct(), TEXT("evaluators"), Context);
	AddNodes(Context.EditorData->GlobalTasks, Context.EditorData, Description->TryGetArrayField(TEXT("global_tasks"), Array) ? Array : nullptr,
		FStateTreeTaskBase::StaticStruct(), TEXT("global_tasks"), Context);
	BuildState(nullptr, *RootJson, Context);
	for (const TPair<UStateTreeState*, TSharedPtr<FJsonObject>>& Entry : Context.StatesWithJson)
	{
		BuildTransitions(*Entry.Key, Entry.Value, Context);
	}
	if (!Context.Errors.IsEmpty())
	{
		return FString::Join(Context.Errors, TEXT("\n"));
	}

	FStateTreeCompilerLog CompileLog;
	FStateTreeCompiler Compiler(CompileLog);
	if (!Compiler.Compile(StateTree))
	{
		TArray<FString> Messages;
		for (const TSharedRef<FTokenizedMessage>& Message : CompileLog.ToTokenizedMessages())
		{
			Messages.Add(Message->ToText().ToString());
		}
		return Messages.IsEmpty() ? FString(TEXT("The StateTree did not compile")) : FString::Join(Messages, TEXT("\n"));
	}

	StateTree->MarkPackageDirty();
	return FString();
}
