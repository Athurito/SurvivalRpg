#include "RpgBaseResourceEntryWidget.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Blueprint/IUserListEntry.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "MVVMSubsystem.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Mvvm/Base/RpgBaseStorageViewModels.h"
#include "View/MVVMView.h"
#include "View/MVVMViewClass.h"
#include "Widgets/SWidget.h"

namespace RpgBaseResourceEntryWidgetTests
{
	class FScopedWidgetWorld
	{
	public:
		FScopedWidgetWorld()
		{
			GameInstance =
				NewObject<UGameInstance>(
					GEngine,
					NAME_None,
					RF_Transient);
			if (!GameInstance)
			{
				return;
			}

			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}

		~FScopedWidgetWorld()
		{
			UWorld* WorldToDestroy = World;
			if (GameInstance)
			{
				GameInstance->Shutdown();
			}
			if (WorldToDestroy)
			{
				GEngine->DestroyWorldContext(WorldToDestroy);
				WorldToDestroy->DestroyWorld(false);
			}
			if (GameInstance)
			{
				GameInstance->RemoveFromRoot();
			}
		}

		bool IsValid() const
		{
			return World != nullptr;
		}

		UWorld* GetWorld() const
		{
			return World;
		}

	private:
		TObjectPtr<UGameInstance> GameInstance;
		TObjectPtr<UWorld> World;
	};

	int32 FindCanonicalSource(
		const UMVVMViewClass& ViewClass,
		int32& OutViewModelSourceCount)
	{
		OutViewModelSourceCount = 0;
		int32 CanonicalSourceIndex = INDEX_NONE;
		const TArrayView<const FMVVMViewClass_Source> Sources =
			ViewClass.GetSources();
		for (int32 SourceIndex = 0;
			SourceIndex < Sources.Num();
			++SourceIndex)
		{
			const FMVVMViewClass_Source& Source =
				Sources[SourceIndex];
			if (!Source.IsViewModel())
			{
				continue;
			}

			++OutViewModelSourceCount;
			if (Source.GetName() ==
				URpgBaseResourceEntryWidget::
					BaseResourceEntryViewModelSourceName)
			{
				CanonicalSourceIndex = SourceIndex;
			}
		}
		return CanonicalSourceIndex;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgBaseResourceEntryPoolingTest,
	"SurvivalRpg.Inventory.UI.BaseResourceEntryPooling",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgBaseResourceEntryPoolingTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgBaseResourceEntryWidgetTests;

	FScopedWidgetWorld TestWorld;
	if (!TestTrue(
			TEXT("Standalone widget world is valid"),
			TestWorld.IsValid()))
	{
		return false;
	}

	UClass* EntryClass =
		LoadClass<URpgBaseResourceEntryWidget>(
			nullptr,
			TEXT(
				"/Game/SurvivalRpg/UI/"
				"CUI_BaseResourceEntry.CUI_BaseResourceEntry_C"));
	UWidgetBlueprintGeneratedClass* GeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(EntryClass);
	if (!TestNotNull(
			TEXT("Canonical base-resource entry class loads"),
			EntryClass) ||
		!TestNotNull(
			TEXT("Canonical base-resource entry is a Widget Blueprint"),
			GeneratedClass))
	{
		return false;
	}
	TestTrue(
		TEXT("Base-resource entry uses its typed native presenter"),
		EntryClass->IsChildOf(
			URpgBaseResourceEntryWidget::StaticClass()));
	TestTrue(
		TEXT("Base-resource entry initializes without a player context"),
		GeneratedClass->bCanCallInitializedWithoutPlayerContext);

	const TArray<UWidgetBlueprintGeneratedClassExtension*>
		CompiledViewExtensions =
			GeneratedClass->GetExtensions(
				UMVVMViewClass::StaticClass(),
				/*bIncludeSuper=*/ false);
	TestEqual(
		TEXT("Base-resource entry owns exactly one compiled MVVM view"),
		CompiledViewExtensions.Num(),
		1);
	const UMVVMViewClass* CompiledView =
		CompiledViewExtensions.Num() == 1
			? Cast<UMVVMViewClass>(CompiledViewExtensions[0])
			: nullptr;
	if (!TestNotNull(
			TEXT("Base-resource entry compiled MVVM view is valid"),
			CompiledView))
	{
		return false;
	}

	int32 ViewModelSourceCount = 0;
	const int32 CanonicalSourceIndex =
		FindCanonicalSource(
			*CompiledView,
			ViewModelSourceCount);
	TestEqual(
		TEXT("Base-resource entry owns exactly one ViewModel source"),
		ViewModelSourceCount,
		1);
	const TArrayView<const FMVVMViewClass_Source> Sources =
		CompiledView->GetSources();
	if (!Sources.IsValidIndex(CanonicalSourceIndex))
	{
		AddError(
			TEXT(
				"Base-resource entry has no canonical "
				"RpgBaseResourceEntryViewModel source"));
		return false;
	}

	const FMVVMViewClass_Source& CanonicalSource =
		Sources[CanonicalSourceIndex];
	TestEqual(
		TEXT("Base-resource source expects the exact VM type"),
		CanonicalSource.GetSourceClass(),
		URpgBaseResourceEntryViewModel::StaticClass());
	TestTrue(
		TEXT("Base-resource source is manually settable"),
		CanonicalSource.CanBeSet());
	TestTrue(
		TEXT("Base-resource source is optional while pooled"),
		CanonicalSource.IsOptional());
	TestEqual(
		TEXT("Base-resource source owns all three leaf bindings"),
		CanonicalSource.GetBindings().Num(),
		3);
	TestEqual(
		TEXT("Base-resource VM permits manual composition only"),
		URpgBaseResourceEntryViewModel::StaticClass()->GetMetaData(
			TEXT("MVVMAllowedContextCreationType")),
		FString(TEXT("Manual")));

	URpgBaseResourceEntryWidget* Widget =
		CreateWidget<URpgBaseResourceEntryWidget>(
			TestWorld.GetWorld(),
			EntryClass);
	if (!TestNotNull(
			TEXT("Canonical base-resource entry initializes"),
			Widget))
	{
		return false;
	}

	UMVVMView* RuntimeView =
		UMVVMSubsystem::GetViewFromUserWidget(Widget);
	if (!TestNotNull(
			TEXT("Base-resource entry runtime MVVM view exists"),
			RuntimeView))
	{
		return false;
	}
	TestNull(
		TEXT("Fresh base-resource entry has no represented VM"),
		Widget->GetBaseResourceEntryViewModel());
	TestNull(
		TEXT("Fresh base-resource MVVM source starts empty"),
		RuntimeView->GetViewModel(
			URpgBaseResourceEntryWidget::
				BaseResourceEntryViewModelSourceName).GetObject());

	TSharedPtr<SWidget> SlateWidget = Widget->TakeWidget();
	if (!TestTrue(
			TEXT("Base-resource entry constructs its authored Slate leaf"),
			SlateWidget.IsValid()))
	{
		return false;
	}

	URpgBaseResourceEntryViewModel* FirstViewModel =
		NewObject<URpgBaseResourceEntryViewModel>(Widget);
	URpgBaseResourceEntryViewModel* SecondViewModel =
		NewObject<URpgBaseResourceEntryViewModel>(Widget);
	FRpgBaseResourceEntryView FirstResource;
	FirstResource.ItemDefinition =
		URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	FirstResource.Count = 2;
	FirstResource.Capacity = 10;
	FirstResource.SortIndex = 0;
	FirstViewModel->InitializeFromResourceEntry(FirstResource);

	FRpgBaseResourceEntryView SecondResource = FirstResource;
	SecondResource.Count = 7;
	SecondResource.SortIndex = 1;
	SecondViewModel->InitializeFromResourceEntry(SecondResource);

	Widget->NativeOnListItemObjectSet(FirstViewModel);
	TestEqual(
		TEXT("Base-resource entry represents VM A"),
		Widget->GetBaseResourceEntryViewModel(),
		FirstViewModel);
	TestEqual(
		TEXT("Base-resource entry injects VM A into its exact source"),
		RuntimeView->GetViewModel(
			URpgBaseResourceEntryWidget::
				BaseResourceEntryViewModelSourceName).GetObject(),
		static_cast<UObject*>(FirstViewModel));

	Widget->NativeOnListItemObjectSet(SecondViewModel);
	TestEqual(
		TEXT("Base-resource entry replaces VM A with VM B"),
		Widget->GetBaseResourceEntryViewModel(),
		SecondViewModel);
	TestEqual(
		TEXT("Base-resource entry replaces the exact source with VM B"),
		RuntimeView->GetViewModel(
			URpgBaseResourceEntryWidget::
				BaseResourceEntryViewModelSourceName).GetObject(),
		static_cast<UObject*>(SecondViewModel));

	IUserListEntry::ReleaseEntry(*Widget);
	TestNull(
		TEXT("Released base-resource entry clears its represented VM"),
		Widget->GetBaseResourceEntryViewModel());
	TestNull(
		TEXT("Released base-resource entry clears its optional source"),
		RuntimeView->GetViewModel(
			URpgBaseResourceEntryWidget::
				BaseResourceEntryViewModelSourceName).GetObject());

	Widget->NativeOnListItemObjectSet(FirstViewModel);
	TestEqual(
		TEXT("Pooled base-resource entry cleanly rebinds VM A"),
		RuntimeView->GetViewModel(
			URpgBaseResourceEntryWidget::
				BaseResourceEntryViewModelSourceName).GetObject(),
		static_cast<UObject*>(FirstViewModel));

	IUserListEntry::ReleaseEntry(*Widget);
	SlateWidget.Reset();
	return true;
}

#endif
