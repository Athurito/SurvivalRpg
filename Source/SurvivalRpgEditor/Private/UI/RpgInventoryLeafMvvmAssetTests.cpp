#if WITH_DEV_AUTOMATION_TESTS

#include "SurvivalRpg/Mvvm/Base/RpgBaseStorageViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgActionBarViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgLoadoutViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"
#include "SurvivalRpg/UI/RpgActionBarSlotWidget.h"
#include "SurvivalRpg/UI/RpgBaseResourceEntryWidget.h"
#include "SurvivalRpg/UI/RpgInventoryAddressSlotWidget.h"
#include "SurvivalRpg/UI/RpgLoadoutSlotWidgets.h"

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "Editor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "Misc/AutomationTest.h"
#include "MVVMBlueprintViewConversionFunction.h"
#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewBinding.h"
#include "MVVMBlueprintViewModelContext.h"
#include "MVVMEditorSubsystem.h"
#include "Types/MVVMBindingMode.h"
#include "UObject/UnrealType.h"
#include "View/MVVMViewClass.h"
#include "WidgetBlueprint.h"

namespace RpgInventoryLeafMvvmAssetTests
{
	struct FBindingContract
	{
		FName SourceField;
		FName DestinationWidget;
		FName DestinationField;
		FName ForwardConversionFunction;
	};

	struct FLeafContract
	{
		const TCHAR* Label;
		const TCHAR* BlueprintPath;
		UClass* NativeParentClass;
		UClass* ViewModelClass;
		FName ViewModelSourceName;
		int32 BindingCount;
		TArray<FBindingContract> BindingContracts;
		TArray<FName> ForbiddenAuthoredGraphs;
		TArray<FName> ForbiddenBlueprintCalls;
	};

	const UClass* ResolveBlueprintContextClass(
		const UWidgetBlueprint& Blueprint)
	{
		return Blueprint.SkeletonGeneratedClass
			? Blueprint.SkeletonGeneratedClass
			: Blueprint.GeneratedClass
				? Blueprint.GeneratedClass
				: Blueprint.ParentClass;
	}

	bool HasExactFieldPath(
		const FMVVMBlueprintPropertyPath& Path,
		const UWidgetBlueprint& Blueprint,
		const FName ExpectedField)
	{
		const TArray<FName> FieldNames =
			Path.GetFieldNames(
				ResolveBlueprintContextClass(Blueprint));
		return FieldNames.Num() == 1 &&
			FieldNames[0] == ExpectedField;
	}

	bool ValidateNoParallelBlueprintDataPath(
		FAutomationTestBase& Test,
		const UWidgetBlueprint& Blueprint,
		const FLeafContract& Contract)
	{
		bool bValid = true;
		TSet<const UEdGraph*> AuthoredGraphs;
		TFunction<void(const UEdGraph*)> AppendAuthoredGraph;
		AppendAuthoredGraph =
			[&Blueprint, &AuthoredGraphs, &AppendAuthoredGraph](
				const UEdGraph* Graph)
			{
				if (!Graph ||
					Graph->HasAnyFlags(RF_Transient) ||
					Blueprint.IntermediateGeneratedGraphs.Contains(
						Graph) ||
					Blueprint.EventGraphs.Contains(Graph) ||
					AuthoredGraphs.Contains(Graph))
				{
					return;
				}

				AuthoredGraphs.Add(Graph);
				for (const UEdGraph* SubGraph : Graph->SubGraphs)
				{
					AppendAuthoredGraph(SubGraph);
				}
			};
		const auto AppendAuthoredGraphs =
			[&AppendAuthoredGraph](
				const TArray<TObjectPtr<UEdGraph>>& Graphs)
			{
				for (const UEdGraph* Graph : Graphs)
				{
					AppendAuthoredGraph(Graph);
				}
			};

		AppendAuthoredGraphs(Blueprint.UbergraphPages);
		AppendAuthoredGraphs(Blueprint.FunctionGraphs);
		AppendAuthoredGraphs(Blueprint.MacroGraphs);
		AppendAuthoredGraphs(Blueprint.DelegateSignatureGraphs);

		for (const UEdGraph* Graph : AuthoredGraphs)
		{
			if (Contract.ForbiddenAuthoredGraphs.Contains(
				Graph->GetFName()))
			{
				Test.AddError(
					FString::Printf(
						TEXT(
							"%s still authors the parallel data graph %s"),
						Contract.Label,
						*Graph->GetName()));
				bValid = false;
			}

			for (const UEdGraphNode* Node : Graph->Nodes)
			{
				const UK2Node_Event* EventNode =
					Cast<UK2Node_Event>(Node);
				if (EventNode &&
					Contract.ForbiddenAuthoredGraphs.Contains(
						EventNode->GetFunctionName()))
				{
					Test.AddError(
						FString::Printf(
							TEXT(
								"%s still implements the parallel data event %s"),
							Contract.Label,
							*EventNode->GetFunctionName().ToString()));
					bValid = false;
				}

				const UK2Node_CallFunction* CallNode =
					Cast<UK2Node_CallFunction>(Node);
				const UFunction* TargetFunction =
					CallNode
						? CallNode->GetTargetFunction()
						: nullptr;
				const UClass* TargetOwnerClass =
					TargetFunction
						? TargetFunction->GetOwnerClass()
						: nullptr;
				const bool bTargetsThisBlueprint =
					CallNode &&
					(
						CallNode->FunctionReference.IsSelfContext() ||
						TargetOwnerClass ==
							Blueprint.SkeletonGeneratedClass ||
						TargetOwnerClass ==
							Blueprint.GeneratedClass);
				if (CallNode &&
					bTargetsThisBlueprint &&
					Contract.ForbiddenBlueprintCalls.Contains(
						CallNode->GetFunctionName()))
				{
					Test.AddError(
						FString::Printf(
							TEXT(
								"%s graph %s still calls the data destination %s outside MVVM"),
							Contract.Label,
							*Graph->GetName(),
							*CallNode->GetFunctionName().ToString()));
					bValid = false;
				}
			}
		}

		return bValid;
	}

	bool DoesBindingMatchContract(
		const FMVVMBlueprintViewBinding& Binding,
		const UWidgetBlueprint& Blueprint,
		const FMVVMBlueprintViewModelContext& Source,
		const FBindingContract& Contract)
	{
		if (Binding.BindingType !=
				EMVVMBindingMode::OneWayToDestination ||
			!Binding.bEnabled ||
			!Binding.bCompile)
		{
			return false;
		}

		const bool bDestinationOwnerMatches =
			Contract.DestinationWidget.IsNone()
				? Binding.DestinationPath.GetSource(&Blueprint) ==
					EMVVMBlueprintFieldPathSource::SelfContext
				: Binding.DestinationPath.GetSource(&Blueprint) ==
						EMVVMBlueprintFieldPathSource::Widget &&
					Binding.DestinationPath.GetWidgetName() ==
						Contract.DestinationWidget;
		if (!bDestinationOwnerMatches ||
			!HasExactFieldPath(
				Binding.DestinationPath,
				Blueprint,
				Contract.DestinationField) ||
			Binding.Conversion.GetConversionFunction(
				/*bSourceToDestination=*/ false))
		{
			return false;
		}

		const UMVVMBlueprintViewConversionFunction*
			ForwardConversion =
				Binding.Conversion.GetConversionFunction(
					/*bSourceToDestination=*/ true);
		if (Contract.ForwardConversionFunction.IsNone())
		{
			return !ForwardConversion &&
				Binding.SourcePath.GetSource(&Blueprint) ==
					EMVVMBlueprintFieldPathSource::ViewModel &&
				Binding.SourcePath.GetViewModelId() ==
					Source.GetViewModelId() &&
				HasExactFieldPath(
					Binding.SourcePath,
					Blueprint,
					Contract.SourceField);
		}

		if (!ForwardConversion ||
			ForwardConversion->GetConversionFunction().GetName() !=
				Contract.ForwardConversionFunction)
		{
			return false;
		}

		int32 BoundSourcePinCount = 0;
		for (const FMVVMBlueprintPin& Pin :
			ForwardConversion->GetPins())
		{
			if (!Pin.UsedPathAsValue())
			{
				continue;
			}

			++BoundSourcePinCount;
			if (Pin.GetStatus() !=
					EMVVMBlueprintPinStatus::Valid ||
				Pin.GetPath().GetSource(&Blueprint) !=
					EMVVMBlueprintFieldPathSource::ViewModel ||
				Pin.GetPath().GetViewModelId() !=
					Source.GetViewModelId() ||
				!HasExactFieldPath(
					Pin.GetPath(),
					Blueprint,
					Contract.SourceField))
			{
				return false;
			}
		}

		return BoundSourcePinCount == 1;
	}

	bool ValidateLeafContract(
		FAutomationTestBase& Test,
		const UMVVMEditorSubsystem& MvvmEditor,
		const FLeafContract& Contract)
	{
		UWidgetBlueprint* Blueprint =
			LoadObject<UWidgetBlueprint>(
				nullptr,
				Contract.BlueprintPath);
		if (!Test.TestNotNull(
				*FString::Printf(
					TEXT("%s Widget Blueprint loads"),
					Contract.Label),
				Blueprint))
		{
			return false;
		}

		bool bValid = true;
		bValid &= Test.TestEqual(
			*FString::Printf(
				TEXT("%s uses its exact native presenter"),
				Contract.Label),
			Blueprint->ParentClass.Get(),
			Contract.NativeParentClass);
		bValid &= Test.TestTrue(
			*FString::Printf(
				TEXT("%s is compiled"),
				Contract.Label),
			Blueprint->Status == BS_UpToDate ||
				Blueprint->Status == BS_UpToDateWithWarnings);
		bValid &= ValidateNoParallelBlueprintDataPath(
			Test,
			*Blueprint,
			Contract);

		const UMVVMBlueprintView* BlueprintView =
			MvvmEditor.GetView(Blueprint);
		if (!Test.TestNotNull(
				*FString::Printf(
					TEXT("%s owns an authored MVVM view"),
					Contract.Label),
				BlueprintView))
		{
			return false;
		}

		const TArrayView<const FMVVMBlueprintViewModelContext>
			ViewModelSources = BlueprintView->GetViewModels();
		bValid &= Test.TestEqual(
			*FString::Printf(
				TEXT("%s owns exactly one authored ViewModel source"),
				Contract.Label),
			ViewModelSources.Num(),
			1);
		if (ViewModelSources.Num() != 1)
		{
			return false;
		}

		const FMVVMBlueprintViewModelContext& Source =
			ViewModelSources[0];
		bValid &= Test.TestEqual(
			*FString::Printf(
				TEXT("%s source has its canonical name"),
				Contract.Label),
			Source.GetViewModelName(),
			Contract.ViewModelSourceName);
		bValid &= Test.TestEqual(
			*FString::Printf(
				TEXT("%s source has its exact ViewModel type"),
				Contract.Label),
			Source.GetViewModelClass(),
			Contract.ViewModelClass);
		bValid &= Test.TestTrue(
			*FString::Printf(
				TEXT("%s source is manually supplied"),
				Contract.Label),
			Source.CreationType ==
				EMVVMBlueprintViewModelContextCreationType::Manual);
		bValid &= Test.TestTrue(
			*FString::Printf(
				TEXT("%s source is optional while pooled"),
				Contract.Label),
			Source.bOptional);
		bValid &= Test.TestTrue(
			*FString::Printf(
				TEXT("%s currently exposes MVVM's generated manual setter"),
				Contract.Label),
			Source.bCreateSetterFunction);
		bValid &= Test.TestEqual(
			*FString::Printf(
				TEXT("%s owns no MVVM events"),
				Contract.Label),
			BlueprintView->GetEvents().Num(),
			0);
		bValid &= Test.TestEqual(
			*FString::Printf(
				TEXT("%s owns no MVVM conditions"),
				Contract.Label),
			BlueprintView->GetConditions().Num(),
			0);
		bValid &= Test.TestEqual(
			*FString::Printf(
				TEXT("%s owns its exact declarative binding count"),
				Contract.Label),
			BlueprintView->GetNumBindings(),
			Contract.BindingCount);

		for (const FMVVMBlueprintViewBinding& Binding :
			BlueprintView->GetBindings())
		{
			bValid &= Test.TestTrue(
				*FString::Printf(
					TEXT("%s binding is OneWayToDestination"),
					Contract.Label),
				Binding.BindingType ==
					EMVVMBindingMode::OneWayToDestination);
			bValid &= Test.TestTrue(
				*FString::Printf(
					TEXT("%s binding is enabled"),
					Contract.Label),
				Binding.bEnabled);
			bValid &= Test.TestTrue(
				*FString::Printf(
					TEXT("%s binding is compiled"),
					Contract.Label),
				Binding.bCompile);
			const UMVVMBlueprintViewConversionFunction*
				ForwardConversion =
					Binding.Conversion.GetConversionFunction(
						/*bSourceToDestination=*/ true);
			if (!ForwardConversion)
			{
				bValid &= Test.TestEqual(
					*FString::Printf(
						TEXT(
							"%s direct binding reads only the canonical source"),
						Contract.Label),
					Binding.SourcePath.GetViewModelId(),
					Source.GetViewModelId());
				continue;
			}

			int32 BoundConversionPinCount = 0;
			for (const FMVVMBlueprintPin& Pin :
				ForwardConversion->GetPins())
			{
				if (!Pin.UsedPathAsValue())
				{
					continue;
				}

				++BoundConversionPinCount;
				bValid &= Test.TestTrue(
					*FString::Printf(
						TEXT(
							"%s conversion pin is valid"),
						Contract.Label),
					Pin.GetStatus() ==
						EMVVMBlueprintPinStatus::Valid);
				bValid &= Test.TestEqual(
					*FString::Printf(
						TEXT(
							"%s conversion pin reads only the canonical source"),
						Contract.Label),
					Pin.GetPath().GetViewModelId(),
					Source.GetViewModelId());
			}
			bValid &= Test.TestTrue(
				*FString::Printf(
					TEXT(
						"%s conversion binds at least one canonical source pin"),
					Contract.Label),
				BoundConversionPinCount > 0);
		}

		bValid &= Test.TestEqual(
			*FString::Printf(
				TEXT(
					"%s declares one exact contract per binding"),
				Contract.Label),
			Contract.BindingContracts.Num(),
			Contract.BindingCount);
		for (const FBindingContract& BindingContract :
			Contract.BindingContracts)
		{
			const FString DestinationOwner =
				BindingContract.DestinationWidget.IsNone()
					? FString(TEXT("Self"))
					: BindingContract.DestinationWidget.ToString();
			bValid &= Test.TestNotNull(
				*FString::Printf(
					TEXT(
						"%s source field %s exists on its ViewModel"),
					Contract.Label,
					*BindingContract.SourceField.ToString()),
				FindFProperty<FProperty>(
					Contract.ViewModelClass,
					BindingContract.SourceField));

			int32 MatchingBindingCount = 0;
			for (const FMVVMBlueprintViewBinding& Binding :
				BlueprintView->GetBindings())
			{
				MatchingBindingCount +=
					DoesBindingMatchContract(
						Binding,
						*Blueprint,
						Source,
						BindingContract)
						? 1
						: 0;
			}

			bValid &= Test.TestEqual(
				*FString::Printf(
					TEXT(
						"%s owns exactly one binding %s -> %s.%s"),
					Contract.Label,
					*BindingContract.SourceField.ToString(),
					*DestinationOwner,
					*BindingContract.DestinationField.ToString()),
				MatchingBindingCount,
				1);
		}

		UWidgetBlueprintGeneratedClass* GeneratedClass =
			Cast<UWidgetBlueprintGeneratedClass>(
				Blueprint->GeneratedClass);
		if (!Test.TestNotNull(
				*FString::Printf(
					TEXT("%s generated class is valid"),
					Contract.Label),
				GeneratedClass))
		{
			return false;
		}

		const TArray<UWidgetBlueprintGeneratedClassExtension*>
			MvvmExtensions =
				GeneratedClass->GetExtensions(
					UMVVMViewClass::StaticClass(),
					/*bIncludeSuper=*/ false);
		bValid &= Test.TestEqual(
			*FString::Printf(
				TEXT("%s owns exactly one compiled MVVM view"),
				Contract.Label),
			MvvmExtensions.Num(),
			1);
		const UMVVMViewClass* CompiledView =
			MvvmExtensions.Num() == 1
				? Cast<UMVVMViewClass>(MvvmExtensions[0])
				: nullptr;
		if (!Test.TestNotNull(
				*FString::Printf(
					TEXT("%s compiled MVVM view is valid"),
					Contract.Label),
				CompiledView))
		{
			return false;
		}

		bValid &= Test.TestEqual(
			*FString::Printf(
				TEXT("%s compiled binding count matches authored MVVM"),
				Contract.Label),
			CompiledView->GetBindings().Num(),
			Contract.BindingCount);
		for (const FMVVMViewClass_Binding& Binding :
			CompiledView->GetBindings())
		{
			bValid &= Test.TestTrue(
				*FString::Printf(
					TEXT("%s compiled binding is one-way"),
					Contract.Label),
				Binding.IsOneWay());
		}

		int32 ViewModelSourceCount = 0;
		int32 CanonicalSourceCount = 0;
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
					Contract.ViewModelSourceName &&
				CompiledSource.GetSourceClass() ==
					Contract.ViewModelClass)
			{
				++CanonicalSourceCount;
				CanonicalSource = &CompiledSource;
			}
		}
		bValid &= Test.TestEqual(
			*FString::Printf(
				TEXT("%s compiled view owns one ViewModel source"),
				Contract.Label),
			ViewModelSourceCount,
			1);
		bValid &= Test.TestEqual(
			*FString::Printf(
				TEXT("%s compiled view owns one canonical source"),
				Contract.Label),
			CanonicalSourceCount,
			1);
		if (CanonicalSource)
		{
			bValid &= Test.TestTrue(
				*FString::Printf(
					TEXT("%s compiled source is manually settable"),
					Contract.Label),
				CanonicalSource->CanBeSet());
			bValid &= Test.TestTrue(
				*FString::Printf(
					TEXT("%s compiled source is optional"),
					Contract.Label),
				CanonicalSource->IsOptional());
			bValid &= Test.TestEqual(
				*FString::Printf(
					TEXT("%s canonical source owns every binding"),
					Contract.Label),
				CanonicalSource->GetBindings().Num(),
				Contract.BindingCount);
		}

		bValid &= Test.TestEqual(
			*FString::Printf(
				TEXT("%s ViewModel permits manual composition only"),
				Contract.Label),
			Contract.ViewModelClass->GetMetaData(
				TEXT("MVVMAllowedContextCreationType")),
			FString(TEXT("Manual")));
		return bValid;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryLeafMvvmAssetContractsTest,
	"SurvivalRpg.Inventory.UI.LeafMvvmAssetContracts",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryLeafMvvmAssetContractsTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryLeafMvvmAssetTests;

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

	const FLeafContract Contracts[] = {
		{
			TEXT("ActionBar slot"),
			TEXT(
				"/Game/SurvivalRpg/UI/Hud/ActionBar/"
				"CUI_ActionBarSlotEntry.CUI_ActionBarSlotEntry"),
			URpgActionBarSlotWidget::StaticClass(),
			URpgActionBarSlotViewModel::StaticClass(),
			URpgActionBarSlotWidget::
				ActionBarSlotViewModelSourceName,
			3,
			{
				{
					TEXT("Icon"),
					NAME_None,
					TEXT("SetSlotIcon"),
					NAME_None
				},
				{
					TEXT("StackCount"),
					NAME_None,
					TEXT("SetStackSize"),
					NAME_None
				},
				{
					TEXT("HotkeyActionRowName"),
					NAME_None,
					TEXT("SetInputAction"),
					NAME_None
				}
			},
			{ TEXT("BP_OnActionBarSlotViewModelSet") },
			{
				TEXT("BP_OnActionBarSlotViewModelSet"),
				TEXT("SetInputAction"),
				TEXT("SetSlotIcon"),
				TEXT("SetStackSize")
			}
		},
		{
			TEXT("Address slot"),
			TEXT(
				"/Game/SurvivalRpg/Inventory/UI/"
				"CUI_AddressSlotEntry.CUI_AddressSlotEntry"),
			URpgInventoryAddressSlotWidget::StaticClass(),
			URpgInventoryAddressSlotViewModel::StaticClass(),
			URpgInventoryAddressSlotWidget::
				AddressSlotViewModelSourceName,
			2,
			{
				{
					TEXT("Icon"),
					NAME_None,
					TEXT("SetSlotIcon"),
					NAME_None
				},
				{
					TEXT("StackCount"),
					NAME_None,
					TEXT("SetStackSize"),
					NAME_None
				}
			},
			{ TEXT("BP_OnAddressSlotViewModelSet") },
			{
				TEXT("BP_OnAddressSlotViewModelSet"),
				TEXT("SetSlotIcon"),
				TEXT("SetStackSize")
			}
		},
		{
			TEXT("Equipment slot"),
			TEXT(
				"/Game/SurvivalRpg/Inventory/UI/"
				"CUI_GearSlot.CUI_GearSlot"),
			URpgEquipmentSlotWidget::StaticClass(),
			URpgEquipmentSlotViewModel::StaticClass(),
			URpgEquipmentSlotWidget::
				EquipmentSlotViewModelSourceName,
			1,
			{
				{
					TEXT("Icon"),
					NAME_None,
					TEXT("SetSlotIcon"),
					NAME_None
				}
			},
			{ TEXT("BP_OnEquipmentSlotUpdated") },
			{
				TEXT("BP_OnEquipmentSlotUpdated"),
				TEXT("SetSlotIcon")
			}
		},
		{
			TEXT("Base resource entry"),
			TEXT(
				"/Game/SurvivalRpg/UI/"
				"CUI_BaseResourceEntry.CUI_BaseResourceEntry"),
			URpgBaseResourceEntryWidget::StaticClass(),
			URpgBaseResourceEntryViewModel::StaticClass(),
			URpgBaseResourceEntryWidget::
				BaseResourceEntryViewModelSourceName,
			3,
			{
				{
					TEXT("Count"),
					TEXT("CommontText_Count"),
					TEXT("Text"),
					TEXT("Conv_IntToText")
				},
				{
					TEXT("Capacity"),
					TEXT("CommontText_Capacity"),
					TEXT("Text"),
					TEXT("Conv_IntToText")
				},
				{
					TEXT("ItemDefinition"),
					NAME_None,
					TEXT("SetResourceName"),
					NAME_None
				}
			},
			{ TEXT("OnListItemObjectSet") },
			{
				TEXT("SetRpgBaseResourceEntryViewModel"),
				TEXT("SetResourceName")
			}
		}
	};

	bool bAllContractsValid = true;
	for (const FLeafContract& Contract : Contracts)
	{
		bAllContractsValid &=
			ValidateLeafContract(
				*this,
				*MvvmEditor,
				Contract);
	}

	const struct
	{
		const TCHAR* Label;
		UClass* NativeClass;
		FName ForbiddenFunction;
	} RemovedNativeDataEvents[] = {
		{
			TEXT("ActionBar"),
			URpgActionBarSlotWidget::StaticClass(),
			TEXT("BP_OnActionBarSlotViewModelSet")
		},
		{
			TEXT("Address"),
			URpgInventoryAddressSlotWidget::StaticClass(),
			TEXT("BP_OnAddressSlotViewModelSet")
		},
		{
			TEXT("Equipment"),
			URpgEquipmentSlotWidget::StaticClass(),
			TEXT("BP_OnEquipmentSlotUpdated")
		}
	};
	for (const auto& RemovedEvent : RemovedNativeDataEvents)
	{
		bAllContractsValid &= TestNull(
			*FString::Printf(
				TEXT("%s exposes no parallel Blueprint data event"),
				RemovedEvent.Label),
			RemovedEvent.NativeClass->FindFunctionByName(
				RemovedEvent.ForbiddenFunction));
	}

	return bAllContractsValid;
}

#endif
