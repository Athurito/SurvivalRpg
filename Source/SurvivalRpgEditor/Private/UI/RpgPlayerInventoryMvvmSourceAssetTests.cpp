#if WITH_DEV_AUTOMATION_TESTS

#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModel.h"
#include "SurvivalRpg/UI/RpgPlayerInventoryPaneWidget.h"
#include "SurvivalRpg/UI/RpgPlayerInventoryWidget.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Bindings/MVVMBindingHelper.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/World.h"
#include "Editor.h"
#include "Interfaces/IPluginManager.h"
#include "K2Node_CallFunction.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewModelContext.h"
#include "MVVMEditorSubsystem.h"
#include "UObject/UnrealType.h"
#include "View/MVVMViewClass.h"
#include "WidgetBlueprint.h"

namespace
{
	TArray<FName> GetProjectBlueprintContentRoots()
	{
		TArray<FName> ContentRoots = { FName(TEXT("/Game")) };
		for (const TSharedRef<IPlugin>& Plugin :
			IPluginManager::Get().GetEnabledPluginsWithContent())
		{
			if (!Plugin->IsMounted() ||
				Plugin->GetType() != EPluginType::Project)
			{
				continue;
			}

			FString MountedAssetPath = Plugin->GetMountedAssetPath();
			MountedAssetPath.RemoveFromEnd(TEXT("/"));
			if (!MountedAssetPath.IsEmpty())
			{
				ContentRoots.AddUnique(FName(*MountedAssetPath));
			}
		}
		return ContentRoots;
	}

	bool IsPlayerAggregateLifecycleCall(
		const UK2Node_CallFunction& CallNode,
		const UBlueprint& Blueprint,
		const TSet<FName>& LifecycleFunctionNames)
	{
		if (!LifecycleFunctionNames.Contains(
				CallNode.FunctionReference.GetMemberName()))
		{
			return false;
		}

		UClass* BlueprintScope =
			Blueprint.SkeletonGeneratedClass
				? Blueprint.SkeletonGeneratedClass
				: Blueprint.GeneratedClass;
		const UFunction* TargetFunction =
			CallNode.GetTargetFunction();
		const UClass* TargetOwner =
			TargetFunction
				? TargetFunction->GetOwnerClass()
				: CallNode.FunctionReference.GetMemberParentClass(
					BlueprintScope);
		return (
			TargetOwner &&
				TargetOwner->IsChildOf(
					URpgPlayerInventoryViewModel::StaticClass())) ||
			CallNode.FunctionReference.IsSelfContext() &&
				BlueprintScope &&
				BlueprintScope->IsChildOf(
					URpgPlayerInventoryViewModel::StaticClass());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgPlayerInventoryMvvmSourceAssetContractTest,
	"SurvivalRpg.Inventory.UI.PlayerMvvmSourceAssetContract",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgPlayerInventoryMvvmSourceAssetContractTest::RunTest(
	const FString& Parameters)
{
	UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(
		nullptr,
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/"
			"CUI_PlayerInventoryPane.CUI_PlayerInventoryPane"));
	if (!TestNotNull(
			TEXT("Canonical Player Inventory Pane Widget Blueprint loads"),
			Blueprint))
	{
		return false;
	}

	bool bValid = true;
	bValid &= TestEqual(
		TEXT("Player Inventory Pane uses its exact passive native presenter"),
		Blueprint->ParentClass.Get(),
		URpgPlayerInventoryPaneWidget::StaticClass());
	bValid &= TestTrue(
		TEXT("Player Inventory Pane Blueprint is compiled"),
		Blueprint->Status == BS_UpToDate ||
			Blueprint->Status == BS_UpToDateWithWarnings);

	const UMVVMEditorSubsystem* MvvmEditor =
		GEditor
			? GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>()
			: nullptr;
	if (!TestNotNull(
			TEXT("MVVM Editor subsystem is available"),
			MvvmEditor))
	{
		return false;
	}

	UMVVMBlueprintView* BlueprintView =
		MvvmEditor->GetView(Blueprint);
	if (!TestNotNull(
			TEXT("Player Inventory Pane owns an authored MVVM view"),
			BlueprintView))
	{
		return false;
	}

	const TArrayView<const FMVVMBlueprintViewModelContext> Sources =
		BlueprintView->GetViewModels();
	bValid &= TestEqual(
		TEXT("Player Inventory Pane authors exactly one ViewModel source"),
		Sources.Num(),
		1);
	if (Sources.Num() != 1)
	{
		return false;
	}

	const FMVVMBlueprintViewModelContext& Source = Sources[0];
	bValid &= TestEqual(
		TEXT("Authored source keeps its canonical name"),
		Source.GetViewModelName(),
		URpgPlayerInventoryPaneWidget::PlayerInventoryViewModelSourceName);
	bValid &= TestEqual(
		TEXT("Authored source expects the aggregate Player ViewModel"),
		Source.GetViewModelClass(),
		URpgPlayerInventoryViewModel::StaticClass());
	bValid &= TestTrue(
		TEXT("Authored source is resolved through a native PropertyPath"),
		Source.CreationType ==
			EMVVMBlueprintViewModelContextCreationType::PropertyPath);
	bValid &= TestEqual(
		TEXT("Authored source resolves through the one native ownership getter"),
		Source.ViewModelPropertyPath,
		FString(TEXT("GetPlayerInventoryViewModel")));
	bValid &= TestFalse(
		TEXT("Authored source requests no generated public setter"),
		Source.bCreateSetterFunction);
	bValid &= TestFalse(
		TEXT("Authored source requests no competing generated getter"),
		Source.bCreateGetterFunction);
	bValid &= TestFalse(
		TEXT("Authored source is non-optional after native pre-initialization"),
		Source.bOptional);
	bValid &= TestEqual(
		TEXT("Player Inventory Pane owns no declarative leaf bindings"),
		BlueprintView->GetNumBindings(),
		0);
	bValid &= TestEqual(
		TEXT("Player Inventory Pane owns no MVVM events"),
		BlueprintView->GetEvents().Num(),
		0);
	bValid &= TestEqual(
		TEXT("Player Inventory Pane owns no MVVM conditions"),
		BlueprintView->GetConditions().Num(),
		0);
	const UMVVMBlueprintViewSettings* ViewSettings =
		BlueprintView->GetSettings();
	if (TestNotNull(
			TEXT("Player Inventory Pane MVVM view settings exist"),
			ViewSettings))
	{
		bValid &= TestTrue(
			TEXT("PropertyPath source initializes automatically during widget construction"),
			ViewSettings->bInitializeSourcesOnConstruct);
		bValid &= TestTrue(
			TEXT("Pane view is compiled even without declarative leaf bindings"),
			ViewSettings->bCreateViewWithoutBindings);
	}
	else
	{
		bValid = false;
	}

	UWidgetBlueprintGeneratedClass* GeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(
			Blueprint->GeneratedClass);
	if (!TestNotNull(
			TEXT("Player Inventory Pane generated class is valid"),
			GeneratedClass))
	{
		return false;
	}

	const UFunction* NativeGetter =
		GeneratedClass->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				URpgPlayerInventoryPaneWidget,
				GetPlayerInventoryViewModel));
	bValid &= TestNotNull(
		TEXT("Generated class retains the native ownership getter"),
		NativeGetter);
	if (NativeGetter)
	{
		bValid &= TestEqual(
			TEXT("Ownership getter is declared by the passive native pane"),
			NativeGetter->GetOwnerClass(),
			static_cast<UClass*>(
				URpgPlayerInventoryPaneWidget::StaticClass()));
		bValid &= TestTrue(
			TEXT("Native ownership getter is a valid MVVM source path"),
			UE::MVVM::BindingHelper::
				IsValidForSourceBinding(NativeGetter));
		const FObjectPropertyBase* GetterReturnProperty =
			CastField<FObjectPropertyBase>(
				UE::MVVM::BindingHelper::
					GetReturnProperty(NativeGetter));
		if (TestNotNull(
				TEXT("Native ownership getter returns an object"),
				GetterReturnProperty))
		{
			bValid &= TestEqual(
				TEXT("Native ownership getter returns the exact aggregate VM type"),
				GetterReturnProperty->PropertyClass.Get(),
				URpgPlayerInventoryViewModel::StaticClass());
		}
		else
		{
			bValid = false;
		}
	}

	int32 NativeAggregateViewModelPropertyCount = 0;
	const FObjectPropertyBase* NativeAggregateViewModelProperty = nullptr;
	for (TFieldIterator<FObjectPropertyBase> PropertyIt(
			URpgPlayerInventoryPaneWidget::StaticClass(),
			EFieldIteratorFlags::ExcludeSuper);
		PropertyIt;
		++PropertyIt)
	{
		const FObjectPropertyBase* Property = *PropertyIt;
		if (Property &&
			Property->PropertyClass ==
				URpgPlayerInventoryViewModel::StaticClass())
		{
			++NativeAggregateViewModelPropertyCount;
			NativeAggregateViewModelProperty = Property;
		}
	}
	bValid &= TestEqual(
		TEXT("Passive pane declares exactly one native aggregate VM property"),
		NativeAggregateViewModelPropertyCount,
		1);
	if (TestNotNull(
			TEXT("Passive pane native aggregate VM property exists"),
			NativeAggregateViewModelProperty))
	{
		bValid &= TestEqual(
			TEXT("Stable aggregate VM is owned directly by the native pane class"),
			NativeAggregateViewModelProperty->GetOwnerClass(),
			static_cast<UClass*>(
				URpgPlayerInventoryPaneWidget::StaticClass()));
		bValid &= TestTrue(
			TEXT("Stable aggregate VM property is transient presentation state"),
			NativeAggregateViewModelProperty->HasAnyPropertyFlags(
				CPF_Transient));
		bValid &= TestFalse(
			TEXT("Stable aggregate VM property is not directly writable from Blueprint"),
			NativeAggregateViewModelProperty->HasAnyPropertyFlags(
				CPF_BlueprintVisible));
	}
	else
	{
		bValid = false;
	}

	bValid &= TestNull(
		TEXT("Generated class exposes no MVVM source setter"),
		GeneratedClass->FindFunctionByName(
			TEXT("SetRpgPlayerInventoryViewModel")));
	if (Blueprint->SkeletonGeneratedClass)
	{
		bValid &= TestNull(
			TEXT("Blueprint skeleton exposes no MVVM source setter"),
			Blueprint->SkeletonGeneratedClass->FindFunctionByName(
				TEXT("SetRpgPlayerInventoryViewModel")));
	}
	else
	{
		AddError(TEXT("Player Inventory Pane Blueprint has no skeleton class."));
		bValid = false;
	}

	const FObjectPropertyBase* GeneratedSourceProperty =
		FindFProperty<FObjectPropertyBase>(
			GeneratedClass,
			URpgPlayerInventoryPaneWidget::
				PlayerInventoryViewModelSourceName);
	if (TestNotNull(
			TEXT("Compiled view owns its internal source property"),
			GeneratedSourceProperty))
	{
		bValid &= TestEqual(
			TEXT("Internal source property has the exact ViewModel type"),
			GeneratedSourceProperty->PropertyClass.Get(),
			URpgPlayerInventoryViewModel::StaticClass());
		bValid &= TestEqual(
			TEXT("Internal source property is authored only on this generated class"),
			GeneratedSourceProperty->GetOwnerClass(),
			static_cast<UClass*>(GeneratedClass));
		bValid &= TestTrue(
			TEXT("Internal source property is Blueprint read-only"),
			GeneratedSourceProperty->HasAnyPropertyFlags(
				CPF_BlueprintReadOnly));
		bValid &= TestFalse(
			TEXT("Internal source property is not ExposeOnSpawn"),
			GeneratedSourceProperty->HasAnyPropertyFlags(
				CPF_ExposeOnSpawn));
		bValid &= TestTrue(
			TEXT("Internal source property is hidden from Blueprint callers"),
			GeneratedSourceProperty->GetBoolMetaData(
				TEXT("BlueprintPrivate")));
		bValid &= TestFalse(
			TEXT("Internal source property names no Blueprint setter"),
			GeneratedSourceProperty->HasMetaData(
				TEXT("BlueprintSetter")));
	}
	else
	{
		bValid = false;
	}

	const TArray<UWidgetBlueprintGeneratedClassExtension*>
		MvvmExtensions = GeneratedClass->GetExtensions(
			UMVVMViewClass::StaticClass(),
			/*bIncludeSuper=*/ false);
	bValid &= TestEqual(
		TEXT("Player Inventory Pane owns exactly one compiled MVVM view"),
		MvvmExtensions.Num(),
		1);
	const UMVVMViewClass* CompiledView =
		MvvmExtensions.Num() == 1
			? Cast<UMVVMViewClass>(MvvmExtensions[0])
			: nullptr;
	if (!TestNotNull(
			TEXT("Compiled Player Inventory Pane MVVM view is valid"),
			CompiledView))
	{
		return false;
	}

	int32 ViewModelSourceCount = 0;
	const FMVVMViewClass_Source* CanonicalSource = nullptr;
	for (const FMVVMViewClass_Source& CompiledSource :
		CompiledView->GetSources())
	{
		if (!CompiledSource.IsViewModel())
		{
			continue;
		}

		++ViewModelSourceCount;
		if (CompiledSource.GetName() ==
				URpgPlayerInventoryPaneWidget::
					PlayerInventoryViewModelSourceName &&
			CompiledSource.GetSourceClass() ==
				URpgPlayerInventoryViewModel::StaticClass())
		{
			CanonicalSource = &CompiledSource;
		}
	}

	bValid &= TestEqual(
		TEXT("Compiled view owns exactly one ViewModel source"),
		ViewModelSourceCount,
		1);
	if (CanonicalSource)
	{
		bValid &= TestFalse(
			TEXT("Compiled PropertyPath source cannot be set at runtime"),
			CanonicalSource->CanBeSet());
		bValid &= TestFalse(
			TEXT("Compiled PropertyPath source is non-optional"),
			CanonicalSource->IsOptional());
		bValid &= TestTrue(
			TEXT("Compiled source reads from the UserWidget ownership path"),
			CanonicalSource->IsUserWidgetProperty());
		bValid &= TestTrue(
			TEXT("Compiled source mirrors the resolved VM into its internal property"),
			CanonicalSource->RequireSettingUserWidgetProperty());
		bValid &= TestEqual(
			TEXT("Pane source owns no declarative bindings"),
			CanonicalSource->GetBindings().Num(),
			0);
	}
	else
	{
		AddError(
			TEXT(
				"Compiled view does not contain the canonical Player Inventory source."));
		bValid = false;
	}

	bValid &= TestEqual(
		TEXT("Aggregate ViewModel permits only PropertyPath composition"),
		URpgPlayerInventoryViewModel::StaticClass()->GetMetaData(
			TEXT("MVVMAllowedContextCreationType")),
		FString(TEXT("PropertyPath")));

	UWidgetBlueprint* RootBlueprint = LoadObject<UWidgetBlueprint>(
		nullptr,
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/"
			"CUI_PlayerInventory.CUI_PlayerInventory"));
	if (!TestNotNull(
			TEXT("Canonical Player Inventory Root Widget Blueprint loads"),
			RootBlueprint))
	{
		return false;
	}

	bValid &= TestEqual(
		TEXT("Player Inventory Root keeps its exact activatable native shell"),
		RootBlueprint->ParentClass.Get(),
		URpgPlayerInventoryWidget::StaticClass());
	bValid &= TestTrue(
		TEXT("Player Inventory Root Blueprint is compiled"),
		RootBlueprint->Status == BS_UpToDate ||
			RootBlueprint->Status == BS_UpToDateWithWarnings);

	if (const UMVVMBlueprintView* RootBlueprintView =
			MvvmEditor->GetView(RootBlueprint))
	{
		bValid &= TestEqual(
			TEXT("Player Inventory Root authors no ViewModel sources"),
			RootBlueprintView->GetViewModels().Num(),
			0);
	}

	UWidgetBlueprintGeneratedClass* RootGeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(
			RootBlueprint->GeneratedClass);
	if (!TestNotNull(
			TEXT("Player Inventory Root generated class is valid"),
			RootGeneratedClass))
	{
		return false;
	}

	bValid &= TestNull(
		TEXT("Player Inventory Root owns no compiled Player VM source property"),
		FindFProperty<FObjectPropertyBase>(
			RootGeneratedClass,
			URpgPlayerInventoryPaneWidget::
				PlayerInventoryViewModelSourceName));
	bValid &= TestNull(
		TEXT("Player Inventory Root exposes no generated Player VM setter"),
		RootGeneratedClass->FindFunctionByName(
			TEXT("SetRpgPlayerInventoryViewModel")));

	int32 RootCompiledViewModelSourceCount = 0;
	const TArray<UWidgetBlueprintGeneratedClassExtension*>
		RootMvvmExtensions = RootGeneratedClass->GetExtensions(
			UMVVMViewClass::StaticClass(),
			/*bIncludeSuper=*/ false);
	for (const UWidgetBlueprintGeneratedClassExtension* Extension :
		RootMvvmExtensions)
	{
		const UMVVMViewClass* RootCompiledView =
			Cast<UMVVMViewClass>(Extension);
		if (!RootCompiledView)
		{
			continue;
		}

		for (const FMVVMViewClass_Source& RootCompiledSource :
			RootCompiledView->GetSources())
		{
			RootCompiledViewModelSourceCount +=
				RootCompiledSource.IsViewModel() ? 1 : 0;
		}
	}
	bValid &= TestEqual(
		TEXT("Player Inventory Root compiles no ViewModel sources"),
		RootCompiledViewModelSourceCount,
		0);

	return bValid;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgPlayerInventoryAggregateLifecycleAssetReferenceTest,
	"SurvivalRpg.Inventory.UI.PlayerAggregateVmLifecycleAssetReferences",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgPlayerInventoryAggregateLifecycleAssetReferenceTest::RunTest(
	const FString& Parameters)
{
	const TSet<FName> LifecycleFunctionNames =
	{
		FName(TEXT("BindPlayerController")),
		FName(TEXT("UnbindPlayerInventory")),
		FName(TEXT("RefreshAll")),
	};

	bool bValid = true;
	for (const FName LifecycleFunctionName :
		LifecycleFunctionNames)
	{
		bValid &= TestNull(
			*FString::Printf(
				TEXT(
					"Player aggregate lifecycle method %s is not reflected to Blueprint"),
				*LifecycleFunctionName.ToString()),
			URpgPlayerInventoryViewModel::StaticClass()->
				FindFunctionByName(LifecycleFunctionName));
	}

	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
			TEXT("AssetRegistry")).Get();
	AssetRegistry.WaitForCompletion();

	const TArray<FName> ContentRoots =
		GetProjectBlueprintContentRoots();
	FARFilter BlueprintFilter;
	BlueprintFilter.ClassPaths.Add(
		UBlueprint::StaticClass()->GetClassPathName());
	BlueprintFilter.bRecursiveClasses = true;
	BlueprintFilter.bRecursivePaths = true;
	for (const FName ContentRoot : ContentRoots)
	{
		BlueprintFilter.PackagePaths.Add(ContentRoot);
	}

	TArray<FAssetData> BlueprintAssets;
	AssetRegistry.GetAssets(BlueprintFilter, BlueprintAssets);
	BlueprintAssets.Sort(
		[](const FAssetData& Left, const FAssetData& Right)
		{
			return Left.PackageName.LexicalLess(
				Right.PackageName);
		});

	int32 ScannedBlueprintCount = 0;
	int32 ScannedMapCount = 0;
	int32 ScannedLevelScriptCount = 0;
	int32 ScannedCallNodeCount = 0;
	int32 LifecycleCallCount = 0;
	const auto ScanBlueprint =
		[this,
		 &LifecycleFunctionNames,
		 &ScannedBlueprintCount,
		 &ScannedCallNodeCount,
		 &LifecycleCallCount](
			const UBlueprint& Blueprint,
			const FString& AssetLabel)
		{
			++ScannedBlueprintCount;
			TArray<UEdGraph*> Graphs;
			Blueprint.GetAllGraphs(Graphs);
			for (const UEdGraph* Graph : Graphs)
			{
				if (!Graph)
				{
					continue;
				}

				for (const UEdGraphNode* Node : Graph->Nodes)
				{
					const UK2Node_CallFunction* CallNode =
						Cast<UK2Node_CallFunction>(Node);
					if (!CallNode)
					{
						continue;
					}

					++ScannedCallNodeCount;
					if (!IsPlayerAggregateLifecycleCall(
							*CallNode,
							Blueprint,
							LifecycleFunctionNames))
					{
						continue;
					}

					++LifecycleCallCount;
					AddError(
						FString::Printf(
							TEXT(
								"%s graph %s still calls Player aggregate lifecycle mutator %s"),
							*AssetLabel,
							*Graph->GetName(),
							*CallNode->FunctionReference.
								GetMemberName().ToString()));
				}
			}
		};

	for (const FAssetData& BlueprintAsset : BlueprintAssets)
	{
		const UBlueprint* Blueprint =
			Cast<UBlueprint>(BlueprintAsset.GetAsset());
		if (!Blueprint)
		{
			AddError(
				FString::Printf(
					TEXT("Project Blueprint failed to load: %s"),
					*BlueprintAsset.GetObjectPathString()));
			bValid = false;
			continue;
		}

		ScanBlueprint(
			*Blueprint,
			BlueprintAsset.GetObjectPathString());
	}

	FARFilter WorldFilter;
	WorldFilter.ClassPaths.Add(
		UWorld::StaticClass()->GetClassPathName());
	WorldFilter.bRecursiveClasses = true;
	WorldFilter.bRecursivePaths = true;
	for (const FName ContentRoot : ContentRoots)
	{
		WorldFilter.PackagePaths.Add(ContentRoot);
	}

	TArray<FAssetData> WorldAssets;
	AssetRegistry.GetAssets(WorldFilter, WorldAssets);
	WorldAssets.Sort(
		[](const FAssetData& Left, const FAssetData& Right)
		{
			return Left.PackageName.LexicalLess(
				Right.PackageName);
		});
	for (const FAssetData& WorldAsset : WorldAssets)
	{
		UWorld* World = Cast<UWorld>(WorldAsset.GetAsset());
		if (!World)
		{
			AddError(
				FString::Printf(
					TEXT("Project map failed to load: %s"),
					*WorldAsset.GetObjectPathString()));
			bValid = false;
			continue;
		}

		++ScannedMapCount;
		for (ULevel* Level : World->GetLevels())
		{
			ULevelScriptBlueprint* LevelScript =
				Level
					? Level->GetLevelScriptBlueprint(
						/*bDontCreate=*/ true)
					: nullptr;
			if (!LevelScript)
			{
				continue;
			}

			++ScannedLevelScriptCount;
			ScanBlueprint(
				*LevelScript,
				FString::Printf(
					TEXT("%s [%s]"),
					*WorldAsset.GetObjectPathString(),
					*Level->GetName()));
		}
	}

	AddInfo(
		FString::Printf(
			TEXT(
				"Scanned %d project Blueprints/level scripts, %d maps, %d level scripts, and %d call nodes for Player aggregate lifecycle references."),
			ScannedBlueprintCount,
			ScannedMapCount,
			ScannedLevelScriptCount,
			ScannedCallNodeCount));
	bValid &= TestEqual(
		TEXT(
			"Project Blueprint assets and maps own no Player aggregate lifecycle mutator calls"),
		LifecycleCallCount,
		0);
	return bValid;
}

#endif
