#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "Blueprint/WidgetTree.h"
#include "Components/DynamicEntryBoxBase.h"
#include "GameFeatureData.h"
#include "GameplayTagContainer.h"
#include "Misc/AutomationTest.h"
#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewModelContext.h"
#include "MVVMEditorSubsystem.h"
#include "Editor.h"
#include "SurvivalRpg/GameFeatures/RpgGameFeatureAction_AddWidgets.h"
#include "SurvivalRpg/Mvvm/Combat/RpgRangedCombatViewModel.h"
#include "WidgetBlueprint.h"
#include "Widgets/UIExtensionPointWidget.h"

namespace RpgRangedHudAssetTests
{
	const TCHAR* const CrosshairPath = TEXT("/GF_Combat_Ranged/UI/CUI_RangedCrosshair.CUI_RangedCrosshair");
	const TCHAR* const CounterPath = TEXT("/GF_Combat_Ranged/UI/CUI_AmmunitionCounter.CUI_AmmunitionCounter");

	template <typename TValue>
	const TValue* ReadProperty(const UObject* Object, const UClass* Class, const TCHAR* Name)
	{
		const FProperty* Property = FindFProperty<FProperty>(Class, Name);
		return Object && Property ? Property->ContainerPtrToValuePtr<TValue>(Object) : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgRangedHudAssetContractTest,
	"SurvivalRpg.UI.RangedCombat.HudAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgRangedHudAssetContractTest::RunTest(const FString& Parameters)
{
	using namespace RpgRangedHudAssetTests;

	// GF_Combat_Ranged adds the bow crosshair to the crosshair slot and the ammunition counter beside Q/E/R.
	const UGameFeatureData* Feature = LoadObject<UGameFeatureData>(nullptr, TEXT("/GF_Combat_Ranged/GF_Combat_Ranged.GF_Combat_Ranged"));
	if (!TestNotNull(TEXT("GF_Combat_Ranged loads"), Feature)) return false;
	const FGameplayTag CrosshairSlot = FGameplayTag::RequestGameplayTag(TEXT("UI.HUD.Slot.Crosshair"));
	const FGameplayTag ResourceSlot = FGameplayTag::RequestGameplayTag(TEXT("UI.HUD.Slot.AbilityResource"));
	bool bCrosshairRegistered = false;
	bool bCounterRegistered = false;
	for (const UGameFeatureAction* Action : Feature->GetActions())
	{
		if (const URpgGameFeatureAction_AddWidgets* AddWidgets = Cast<URpgGameFeatureAction_AddWidgets>(Action))
		{
			for (const FRpgGameFeatureWidgetEntry& Entry : AddWidgets->Widgets)
			{
				const FString WidgetPath = Entry.WidgetClass.ToSoftObjectPath().ToString();
				bCrosshairRegistered |= WidgetPath == FString(CrosshairPath) + TEXT("_C") && Entry.SlotTag == CrosshairSlot;
				bCounterRegistered |= WidgetPath == FString(CounterPath) + TEXT("_C") && Entry.SlotTag == ResourceSlot;
			}
		}
	}
	TestTrue(TEXT("The bow crosshair joins the crosshair slot"), bCrosshairRegistered);
	TestTrue(TEXT("The ammunition counter joins the ability resource slot"), bCounterRegistered);

	// Both widgets read the local player's shared ranged view model.
	UMVVMEditorSubsystem* Mvvm = GEditor ? GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>() : nullptr;
	if (!TestNotNull(TEXT("The MVVM editor subsystem exists"), Mvvm)) return false;
	for (const TCHAR* Path : { CrosshairPath, CounterPath })
	{
		const UWidgetBlueprint* Widget = LoadObject<UWidgetBlueprint>(nullptr, Path);
		if (!TestNotNull(*FString::Printf(TEXT("%s loads"), Path), Widget)) continue;
		TestTrue(*FString::Printf(TEXT("%s compiles"), Path), Widget->Status == BS_UpToDate || Widget->Status == BS_UpToDateWithWarnings);
		const UMVVMBlueprintView* View = Mvvm->GetView(Widget);
		const TArrayView<const FMVVMBlueprintViewModelContext> Sources = View ? View->GetViewModels() : TArrayView<const FMVVMBlueprintViewModelContext>();
		TestTrue(*FString::Printf(TEXT("%s resolves the local player's ranged combat view model"), Path),
			Algo::AnyOf(Sources, [](const FMVVMBlueprintViewModelContext& Source)
			{
				return Source.GetViewModelClass() == URpgRangedCombatViewModel::StaticClass()
					&& Source.CreationType == EMVVMBlueprintViewModelContextCreationType::Resolver;
			}));
		TestTrue(*FString::Printf(TEXT("%s binds its view model"), Path), View && View->GetNumBindings() > 0);
	}

	// The crosshair slot stacks its entries at the screen centre, so the bow crosshair never shifts the interaction dot.
	const UWidgetBlueprint* Hud = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/SurvivalRpg/UI/Hud/CUI_RpgHudLayout.CUI_RpgHudLayout"));
	const UUIExtensionPointWidget* Point = Hud && Hud->WidgetTree
		? Cast<UUIExtensionPointWidget>(Hud->WidgetTree->FindWidget(TEXT("CrosshairExtensionPoint")))
		: nullptr;
	if (!TestNotNull(TEXT("The HUD has its crosshair slot"), Point)) return false;
	const UClass* EntryBox = UDynamicEntryBoxBase::StaticClass();
	const EDynamicBoxType* BoxType = ReadProperty<EDynamicBoxType>(Point, EntryBox, TEXT("EntryBoxType"));
	const TEnumAsByte<EHorizontalAlignment>* HAlign = ReadProperty<TEnumAsByte<EHorizontalAlignment>>(Point, EntryBox, TEXT("EntryHorizontalAlignment"));
	const TEnumAsByte<EVerticalAlignment>* VAlign = ReadProperty<TEnumAsByte<EVerticalAlignment>>(Point, EntryBox, TEXT("EntryVerticalAlignment"));
	const TArray<FVector2D>* Spacing = ReadProperty<TArray<FVector2D>>(Point, EntryBox, TEXT("SpacingPattern"));
	TestTrue(TEXT("Crosshair entries stack on each other"), BoxType && *BoxType == EDynamicBoxType::Overlay);
	TestTrue(TEXT("Crosshair entries centre horizontally"), HAlign && *HAlign == HAlign_Center);
	TestTrue(TEXT("Crosshair entries centre vertically"), VAlign && *VAlign == VAlign_Center);
	// Without a spacing pattern an overlay entry box ignores the entry alignment and pins entries to the top left.
	TestTrue(TEXT("Crosshair entries keep their alignment"), Spacing && !Spacing->IsEmpty()
		&& Algo::AllOf(*Spacing, [](const FVector2D& Step) { return Step.IsNearlyZero(); }));
	return true;
}

#endif
