#if WITH_DEV_AUTOMATION_TESTS

#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"
#include "SurvivalRpg/UI/RpgPlayerInventoryWidget.h"

#include "Bindings/MVVMBindingHelper.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Editor.h"
#include "Misc/AutomationTest.h"
#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewModelContext.h"
#include "MVVMEditorSubsystem.h"
#include "UObject/UnrealType.h"
#include "View/MVVMViewClass.h"
#include "WidgetBlueprint.h"

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
			"CUI_PlayerInventory.CUI_PlayerInventory"));
	if (!TestNotNull(
			TEXT("Canonical Player Inventory Widget Blueprint loads"),
			Blueprint))
	{
		return false;
	}

	bool bValid = true;
	bValid &= TestEqual(
		TEXT("Player Inventory uses its exact native presenter"),
		Blueprint->ParentClass.Get(),
		URpgPlayerInventoryWidget::StaticClass());
	bValid &= TestTrue(
		TEXT("Player Inventory Blueprint is compiled"),
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
			TEXT("Player Inventory owns an authored MVVM view"),
			BlueprintView))
	{
		return false;
	}

	const TArrayView<const FMVVMBlueprintViewModelContext> Sources =
		BlueprintView->GetViewModels();
	bValid &= TestEqual(
		TEXT("Player Inventory authors exactly one ViewModel source"),
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
		URpgPlayerInventoryWidget::PlayerInventoryViewModelSourceName);
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
		TEXT("Root Player Inventory owns no declarative leaf bindings"),
		BlueprintView->GetNumBindings(),
		0);
	bValid &= TestEqual(
		TEXT("Root Player Inventory owns no MVVM events"),
		BlueprintView->GetEvents().Num(),
		0);
	bValid &= TestEqual(
		TEXT("Root Player Inventory owns no MVVM conditions"),
		BlueprintView->GetConditions().Num(),
		0);
	const UMVVMBlueprintViewSettings* ViewSettings =
		BlueprintView->GetSettings();
	if (TestNotNull(
			TEXT("Player Inventory MVVM view settings exist"),
			ViewSettings))
	{
		bValid &= TestTrue(
			TEXT("PropertyPath source initializes automatically during widget construction"),
			ViewSettings->bInitializeSourcesOnConstruct);
		bValid &= TestTrue(
			TEXT("Root view is compiled even without declarative leaf bindings"),
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
			TEXT("Player Inventory generated class is valid"),
			GeneratedClass))
	{
		return false;
	}

	const UFunction* NativeGetter =
		GeneratedClass->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				URpgPlayerInventoryWidget,
				GetPlayerInventoryViewModel));
	bValid &= TestNotNull(
		TEXT("Generated class retains the native ownership getter"),
		NativeGetter);
	if (NativeGetter)
	{
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
		AddError(TEXT("Player Inventory Blueprint has no skeleton class."));
		bValid = false;
	}

	const FObjectPropertyBase* GeneratedSourceProperty =
		FindFProperty<FObjectPropertyBase>(
			GeneratedClass,
			URpgPlayerInventoryWidget::
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
		TEXT("Player Inventory owns exactly one compiled MVVM view"),
		MvvmExtensions.Num(),
		1);
	const UMVVMViewClass* CompiledView =
		MvvmExtensions.Num() == 1
			? Cast<UMVVMViewClass>(MvvmExtensions[0])
			: nullptr;
	if (!TestNotNull(
			TEXT("Compiled Player Inventory MVVM view is valid"),
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
				URpgPlayerInventoryWidget::
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
			TEXT("Root source owns no declarative bindings"),
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

	return bValid;
}

#endif
