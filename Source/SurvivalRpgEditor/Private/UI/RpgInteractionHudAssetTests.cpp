#if WITH_DEV_AUTOMATION_TESTS

#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/GameFeatures/RpgGameFeatureAction_AddWidgets.h"
#include "SurvivalRpg/Interaction/Components/RpgInteractionPromptAnchorComponent.h"
#include "SurvivalRpg/UI/Interaction/RpgInteractionPromptWidget.h"
#include "SurvivalRpg/UI/Interaction/RpgInteractionReticleWidget.h"

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "CommonActionWidget.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Widget.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"
#include "Widgets/UIExtensionPointWidget.h"

namespace RpgInteractionHudAssetTests
{
	constexpr TCHAR FocusBlueprintPath[] =
		TEXT("/Game/SurvivalRpg/UI/Interaction/CUI_InteractionPrompt.CUI_InteractionPrompt");
	constexpr TCHAR NearbyBlueprintPath[] =
		TEXT("/Game/SurvivalRpg/UI/Interaction/CUI_InteractionNearbyIndicator.CUI_InteractionNearbyIndicator");
	constexpr TCHAR ReticleBlueprintPath[] =
		TEXT("/Game/SurvivalRpg/UI/Interaction/CUI_InteractionReticle.CUI_InteractionReticle");
	constexpr TCHAR HudLayoutClassPath[] =
		TEXT("/Game/SurvivalRpg/UI/Hud/CUI_RpgHudLayout.CUI_RpgHudLayout_C");
	constexpr TCHAR ExperiencePath[] =
		TEXT("/Game/SurvivalRpg/System/Experiences/RpgPrototypeExperience.RpgPrototypeExperience");

	bool HasBlueprintGraphLogic(const UBlueprint& Blueprint)
	{
		const auto HasNodes = [](const TArray<TObjectPtr<UEdGraph>>& Graphs)
		{
			return Graphs.ContainsByPredicate(
				[](const TObjectPtr<UEdGraph>& Graph)
				{
					return Graph && !Graph->Nodes.IsEmpty();
				});
		};
		return HasNodes(Blueprint.UbergraphPages) ||
			HasNodes(Blueprint.FunctionGraphs) ||
			HasNodes(Blueprint.MacroGraphs) ||
			HasNodes(Blueprint.DelegateSignatureGraphs);
	}

	const FGameplayTag* GetExtensionPointTag(
		const UUIExtensionPointWidget* ExtensionPoint)
	{
		const FStructProperty* Property = FindFProperty<FStructProperty>(
			UUIExtensionPointWidget::StaticClass(),
			TEXT("ExtensionPointTag"));
		return ExtensionPoint && Property
			? Property->ContainerPtrToValuePtr<FGameplayTag>(ExtensionPoint)
			: nullptr;
	}

	int32 CountDefaultPromptAnchors(const UBlueprint& Blueprint)
	{
		int32 Count = 0;
		if (!Blueprint.SimpleConstructionScript)
		{
			return Count;
		}
		for (const USCS_Node* Node : Blueprint.SimpleConstructionScript->GetAllNodes())
		{
			const URpgInteractionPromptAnchorComponent* Anchor = Node
				? Cast<URpgInteractionPromptAnchorComponent>(Node->ComponentTemplate)
				: nullptr;
			Count += Anchor &&
				Anchor->AnchorId == FName(TEXT("Default"))
					? 1
					: 0;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInteractionHudPresentationAssetTest,
	"SurvivalRpg.Interaction.HudAssets.Presentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInteractionHudPresentationAssetTest::RunTest(const FString& Parameters)
{
	using namespace RpgInteractionHudAssetTests;

	const UWidgetBlueprint* FocusBlueprint =
		LoadObject<UWidgetBlueprint>(nullptr, FocusBlueprintPath);
	const UWidgetBlueprint* NearbyBlueprint =
		LoadObject<UWidgetBlueprint>(nullptr, NearbyBlueprintPath);
	const UWidgetBlueprint* ReticleBlueprint =
		LoadObject<UWidgetBlueprint>(nullptr, ReticleBlueprintPath);
	if (!TestNotNull(TEXT("CUI_InteractionPrompt loads"), FocusBlueprint) ||
		!TestNotNull(TEXT("CUI_InteractionNearbyIndicator loads"), NearbyBlueprint) ||
		!TestNotNull(TEXT("CUI_InteractionReticle loads"), ReticleBlueprint))
	{
		return false;
	}

	TestEqual(
		TEXT("Focus prompt has the exact native presenter parent"),
		FocusBlueprint->ParentClass.Get(),
		URpgInteractionPromptWidget::StaticClass());
	TestEqual(
		TEXT("Nearby marker has the exact native presenter parent"),
		NearbyBlueprint->ParentClass.Get(),
		URpgInteractionPromptWidget::StaticClass());
	TestEqual(
		TEXT("Reticle has the exact native presenter parent"),
		ReticleBlueprint->ParentClass.Get(),
		URpgInteractionReticleWidget::StaticClass());
	const FObjectPropertyBase* InputContainerProperty =
		FindFProperty<FObjectPropertyBase>(
			URpgInteractionPromptWidget::StaticClass(),
			TEXT("InputActionContainer"));
	if (TestNotNull(
		TEXT("Native presenter state-gates the 24 px glyph container"),
		InputContainerProperty))
	{
		TestEqual(
			TEXT("InputActionContainer accepts the authored SizeBox"),
			InputContainerProperty->PropertyClass.Get(),
			UWidget::StaticClass());
		TestTrue(
			TEXT("InputActionContainer is an optional authored binding"),
			InputContainerProperty->HasMetaData(TEXT("BindWidgetOptional")));
	}
	TestFalse(TEXT("Focus prompt owns no Blueprint graph logic"), HasBlueprintGraphLogic(*FocusBlueprint));
	TestFalse(TEXT("Nearby marker owns no Blueprint graph logic"), HasBlueprintGraphLogic(*NearbyBlueprint));
	TestFalse(TEXT("Reticle owns no Blueprint graph logic"), HasBlueprintGraphLogic(*ReticleBlueprint));
	TestTrue(TEXT("Focus prompt owns no authored animation"), FocusBlueprint->Animations.IsEmpty());
	TestTrue(TEXT("Nearby marker owns no authored animation"), NearbyBlueprint->Animations.IsEmpty());
	TestTrue(TEXT("Reticle owns no authored animation"), ReticleBlueprint->Animations.IsEmpty());

	const UWidgetBlueprintGeneratedClass* FocusClass =
		Cast<UWidgetBlueprintGeneratedClass>(FocusBlueprint->GeneratedClass);
	const UWidgetBlueprintGeneratedClass* NearbyClass =
		Cast<UWidgetBlueprintGeneratedClass>(NearbyBlueprint->GeneratedClass);
	const UWidgetBlueprintGeneratedClass* ReticleClass =
		Cast<UWidgetBlueprintGeneratedClass>(ReticleBlueprint->GeneratedClass);
	const UWidgetTree* FocusTree = FocusClass ? FocusClass->GetWidgetTreeArchetype() : nullptr;
	const UWidgetTree* NearbyTree = NearbyClass ? NearbyClass->GetWidgetTreeArchetype() : nullptr;
	const UWidgetTree* ReticleTree = ReticleClass ? ReticleClass->GetWidgetTreeArchetype() : nullptr;
	if (!TestNotNull(TEXT("Focus prompt has an authored WidgetTree"), FocusTree) ||
		!TestNotNull(TEXT("Nearby marker has an authored WidgetTree"), NearbyTree) ||
		!TestNotNull(TEXT("Reticle has an authored WidgetTree"), ReticleTree))
	{
		return false;
	}

	TestNotNull(
		TEXT("Focus prompt authors its CommonUI input glyph"),
		Cast<UCommonActionWidget>(FocusTree->FindWidget(TEXT("InputActionWidget"))));
	const USizeBox* InputActionContainer =
		Cast<USizeBox>(FocusTree->FindWidget(TEXT("InputActionContainer")));
	if (TestNotNull(
		TEXT("Focus prompt constrains and state-gates its input glyph container"),
		InputActionContainer))
	{
		TestEqual(TEXT("Input glyph container is 24 px wide"), InputActionContainer->GetWidthOverride(), 24.0f);
		TestEqual(TEXT("Input glyph container is 24 px high"), InputActionContainer->GetHeightOverride(), 24.0f);
	}
	TestNotNull(
		TEXT("Focus prompt authors its one-line action text"),
		Cast<UCommonTextBlock>(FocusTree->FindWidget(TEXT("ActionTextBlock"))));
	TestNotNull(
		TEXT("Focus prompt authors a geometric blocked icon"),
		Cast<USizeBox>(FocusTree->FindWidget(TEXT("BlockedIcon"))));
	TestNotNull(
		TEXT("Focus prompt authors its blocked reason"),
		Cast<UCommonTextBlock>(FocusTree->FindWidget(TEXT("BlockedReasonTextBlock"))));
	TestNull(
		TEXT("Default focus prompt deliberately omits TargetTextBlock"),
		FocusTree->FindWidget(TEXT("TargetTextBlock")));

	TArray<UWidget*> NearbyWidgets;
	NearbyTree->GetAllWidgets(NearbyWidgets);
	TestEqual(TEXT("Nearby asset contains only root and marker"), NearbyWidgets.Num(), 2);
	const USizeBox* NearbyRoot = Cast<USizeBox>(NearbyTree->RootWidget);
	const UImage* NearbyMarker =
		Cast<UImage>(NearbyTree->FindWidget(TEXT("NearbyMarker")));
	if (TestNotNull(TEXT("Nearby root is a SizeBox"), NearbyRoot))
	{
		TestEqual(TEXT("Nearby circle is 12 px wide"), NearbyRoot->GetWidthOverride(), 12.0f);
		TestEqual(TEXT("Nearby circle is 12 px high"), NearbyRoot->GetHeightOverride(), 12.0f);
	}
	if (TestNotNull(TEXT("Nearby marker is an authored Image"), NearbyMarker))
	{
		TestTrue(
			TEXT("Nearby marker uses a texture-free rounded-box ring"),
			NearbyMarker->GetBrush().DrawAs == ESlateBrushDrawType::RoundedBox &&
			NearbyMarker->GetBrush().GetResourceObject() == nullptr);
	}
	for (const UWidget* Widget : NearbyWidgets)
	{
		TestNull(
			TEXT("Nearby asset owns no input glyph"),
			Cast<UCommonActionWidget>(Widget));
		TestNull(
			TEXT("Nearby asset owns no interaction text"),
			Cast<UCommonTextBlock>(Widget));
	}

	const USizeBox* ReticleRoot = Cast<USizeBox>(ReticleTree->RootWidget);
	const UImage* ReticleDot = Cast<UImage>(ReticleTree->FindWidget(TEXT("ReticleDot")));
	if (TestNotNull(TEXT("Reticle root is a SizeBox"), ReticleRoot))
	{
		TestEqual(TEXT("Reticle dot is 4 px wide"), ReticleRoot->GetWidthOverride(), 4.0f);
		TestEqual(TEXT("Reticle dot is 4 px high"), ReticleRoot->GetHeightOverride(), 4.0f);
	}
	if (TestNotNull(TEXT("Reticle authors one geometric dot"), ReticleDot))
	{
		TestTrue(
			TEXT("Reticle dot has no texture hard reference"),
			ReticleDot->GetBrush().GetResourceObject() == nullptr);
	}
	const URpgInteractionReticleWidget* ReticleDefaults = ReticleClass
		? Cast<URpgInteractionReticleWidget>(ReticleClass->GetDefaultObject())
		: nullptr;
	if (TestNotNull(TEXT("Reticle generated defaults load"), ReticleDefaults))
	{
		TestFalse(TEXT("Reticle is never focusable"), ReticleDefaults->IsFocusable());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInteractionHudCompositionAssetTest,
	"SurvivalRpg.Interaction.HudAssets.CompositionAndAnchors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInteractionHudCompositionAssetTest::RunTest(const FString& Parameters)
{
	using namespace RpgInteractionHudAssetTests;

	UClass* HudClass = LoadClass<UUserWidget>(nullptr, HudLayoutClassPath);
	const UBlueprint* ExperienceBlueprint = LoadObject<UBlueprint>(nullptr, ExperiencePath);
	const UWidgetBlueprint* ReticleBlueprint =
		LoadObject<UWidgetBlueprint>(nullptr, ReticleBlueprintPath);
	const UWidgetBlueprintGeneratedClass* HudGeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(HudClass);
	const UWidgetTree* HudTree = HudGeneratedClass
		? HudGeneratedClass->GetWidgetTreeArchetype()
		: nullptr;
	const URpgExperienceDefinition* Experience = ExperienceBlueprint &&
		ExperienceBlueprint->GeneratedClass
			? Cast<URpgExperienceDefinition>(
				ExperienceBlueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
	if (!TestNotNull(TEXT("HUD layout loads"), HudClass) ||
		!TestNotNull(TEXT("HUD layout has an authored WidgetTree"), HudTree) ||
		!TestNotNull(TEXT("Prototype Experience loads"), Experience) ||
		!TestNotNull(TEXT("Reticle Blueprint loads"), ReticleBlueprint))
	{
		return false;
	}

	const FGameplayTag CrosshairTag = FGameplayTag::RequestGameplayTag(
		TEXT("UI.HUD.Slot.Crosshair"), false);
	TestTrue(TEXT("Crosshair UIExtension tag is registered"), CrosshairTag.IsValid());
	int32 CrosshairPointCount = 0;
	const UUIExtensionPointWidget* CrosshairPoint = nullptr;
	HudTree->ForEachWidget(
		[&CrosshairPointCount, &CrosshairPoint, &CrosshairTag](UWidget* Widget)
		{
			const UUIExtensionPointWidget* Candidate = Cast<UUIExtensionPointWidget>(Widget);
			const FGameplayTag* Tag = GetExtensionPointTag(Candidate);
			if (Tag && *Tag == CrosshairTag)
			{
				++CrosshairPointCount;
				CrosshairPoint = Candidate;
			}
		});
	TestEqual(TEXT("HUD owns exactly one Crosshair extension point"), CrosshairPointCount, 1);
	if (TestNotNull(TEXT("Crosshair extension point resolves"), CrosshairPoint))
	{
		TestEqual(
			TEXT("Crosshair extension point has its canonical name"),
			CrosshairPoint->GetFName(),
			FName(TEXT("CrosshairExtensionPoint")));
		const UOverlaySlot* Slot = Cast<UOverlaySlot>(CrosshairPoint->Slot);
		if (TestNotNull(TEXT("Crosshair extension point is an Overlay child"), Slot))
		{
			TestTrue(TEXT("Crosshair slot is horizontally centered"),
				Slot->GetHorizontalAlignment() == HAlign_Center);
			TestTrue(TEXT("Crosshair slot is vertically centered"),
				Slot->GetVerticalAlignment() == VAlign_Center);
		}
	}

	const FSoftObjectPath ReticleClassPath(ReticleBlueprint->GeneratedClass);
	int32 RelatedRegistrationCount = 0;
	for (const UGameFeatureAction* Action : Experience->Actions)
	{
		const URpgGameFeatureAction_AddWidgets* AddWidgets =
			Cast<URpgGameFeatureAction_AddWidgets>(Action);
		if (!AddWidgets)
		{
			continue;
		}
		for (const FRpgGameFeatureWidgetEntry& Entry : AddWidgets->Widgets)
		{
			const bool bUsesReticle =
				Entry.WidgetClass.ToSoftObjectPath() == ReticleClassPath;
			const bool bUsesCrosshairSlot = Entry.SlotTag == CrosshairTag;
			if (!bUsesReticle && !bUsesCrosshairSlot)
			{
				continue;
			}
			++RelatedRegistrationCount;
			TestTrue(TEXT("Crosshair registration uses the reticle class"), bUsesReticle);
			TestTrue(TEXT("Reticle registration uses the Crosshair slot"), bUsesCrosshairSlot);
			TestEqual(TEXT("Reticle keeps default extension priority"), Entry.Priority, -1);
		}
	}
	TestEqual(TEXT("Prototype Experience registers the reticle exactly once"), RelatedRegistrationCount, 1);

	struct FAnchorAsset
	{
		const TCHAR* Label;
		const TCHAR* ObjectPath;
	};
	const FAnchorAsset RequiredAnchorAssets[] = {
		{TEXT("Rock"), TEXT("/Game/SurvivalRpg/Interaction/Items/BP_InteractableRock.BP_InteractableRock")},
		{TEXT("Storage"), TEXT("/Game/SurvivalRpg/Storage/BP_StorageUnit_Wood.BP_StorageUnit_Wood")},
		{TEXT("Door"), TEXT("/Game/SurvivalRpg/Interaction/Reference/BP_InteractableDoor_Reference.BP_InteractableDoor_Reference")},
	};
	for (const FAnchorAsset& AnchorAsset : RequiredAnchorAssets)
	{
		const UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, AnchorAsset.ObjectPath);
		if (TestNotNull(
			*FString::Printf(TEXT("%s reference Blueprint loads"), AnchorAsset.Label),
			Blueprint))
		{
			TestEqual(
				*FString::Printf(TEXT("%s owns exactly one Default prompt anchor"), AnchorAsset.Label),
				CountDefaultPromptAnchors(*Blueprint),
				1);
		}
	}

	const FAnchorAsset OptionalPortalAssets[] = {
		{TEXT("Portal"), TEXT("/GF_Portals_Core/Portals/BP_Portal_RiftGruntTrial.BP_Portal_RiftGruntTrial")},
		{TEXT("Portal Exit"), TEXT("/GF_Portals_Core/Portals/BP_PortalExit_Prototype.BP_PortalExit_Prototype")},
	};
	for (const FAnchorAsset& AnchorAsset : OptionalPortalAssets)
	{
		const UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, AnchorAsset.ObjectPath);
		if (!Blueprint)
		{
			AddInfo(FString::Printf(
				TEXT("%s GameFeature asset is not mounted; runtime Actor-Bounds placement remains active."),
				AnchorAsset.Label));
			continue;
		}
		const int32 AnchorCount = CountDefaultPromptAnchors(*Blueprint);
		TestTrue(
			*FString::Printf(TEXT("%s has no duplicate Default prompt anchors"), AnchorAsset.Label),
			AnchorCount <= 1);
	}
	return true;
}

#endif
