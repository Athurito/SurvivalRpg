#include "UI/RpgInteractionHudAssetTools.h"

#include "AssetToolsModule.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "BlueprintEditorLibrary.h"
#include "CommonActionWidget.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SceneComponent.h"
#include "Components/SizeBox.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/GameFeatures/RpgGameFeatureAction_AddWidgets.h"
#include "SurvivalRpg/Interaction/Components/RpgInteractionPromptAnchorComponent.h"
#include "SurvivalRpg/UI/Interaction/RpgInteractionPromptWidget.h"
#include "SurvivalRpg/UI/Interaction/RpgInteractionReticleWidget.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"
#include "Widgets/UIExtensionPointWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInteractionHudAssetTools)

namespace RpgInteractionHudAuthoring
{
	constexpr TCHAR FocusWidgetPackage[] =
		TEXT("/Game/SurvivalRpg/UI/Interaction/CUI_InteractionPrompt");
	constexpr TCHAR NearbyWidgetPackage[] =
		TEXT("/Game/SurvivalRpg/UI/Interaction/CUI_InteractionNearbyIndicator");
	constexpr TCHAR ReticleWidgetPackage[] =
		TEXT("/Game/SurvivalRpg/UI/Interaction/CUI_InteractionReticle");
	constexpr TCHAR HudLayoutPackage[] =
		TEXT("/Game/SurvivalRpg/UI/Hud/CUI_RpgHudLayout");
	constexpr TCHAR ExperiencePackage[] =
		TEXT("/Game/SurvivalRpg/System/Experiences/RpgPrototypeExperience");

	FString MakeObjectPath(const FString& PackagePath)
	{
		return FString::Printf(
			TEXT("%s.%s"),
			*PackagePath,
			*FPackageName::GetLongPackageAssetName(PackagePath));
	}

	template <typename AssetType>
	AssetType* LoadAsset(const TCHAR* PackagePath)
	{
		return LoadObject<AssetType>(
			nullptr,
			*MakeObjectPath(PackagePath));
	}

	FSlateBrush MakeRoundedBrush(
		const FVector2D Size,
		const FLinearColor Fill,
		const float Radius,
		const FLinearColor Outline = FLinearColor::Transparent,
		const float OutlineWidth = 0.0f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.SetImageSize(Size);
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(
			Radius,
			FSlateColor(Outline),
			OutlineWidth);
		return Brush;
	}

	void ConfigurePromptText(UCommonTextBlock& TextBlock, const int32 FontSize)
	{
		FSlateFontInfo Font = TextBlock.GetFont();
		Font.Size = FontSize;
		Font.OutlineSettings.OutlineSize = 1;
		Font.OutlineSettings.OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.65f);
		TextBlock.SetFont(Font);
		TextBlock.SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.97f, 1.0f, 0.96f)));
		TextBlock.SetShadowOffset(FVector2D(0.0f, 1.0f));
		TextBlock.SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));
		TextBlock.SetJustification(ETextJustify::Center);
		TextBlock.SetAutoWrapText(false);
		TextBlock.SetVisibility(ESlateVisibility::Collapsed);
	}

	bool HasExactWidgetSet(
		const UWidgetBlueprint& Blueprint,
		const TArray<TPair<FName, UClass*>>& ExpectedWidgets)
	{
		if (!Blueprint.WidgetTree || !Blueprint.WidgetTree->RootWidget)
		{
			return false;
		}

		TArray<UWidget*> Widgets;
		Blueprint.WidgetTree->GetAllWidgets(Widgets);
		if (Widgets.Num() != ExpectedWidgets.Num())
		{
			return false;
		}

		for (const TPair<FName, UClass*>& Expected : ExpectedWidgets)
		{
			const UWidget* Widget = Blueprint.WidgetTree->FindWidget(Expected.Key);
			if (!Widget || Widget->GetClass() != Expected.Value)
			{
				return false;
			}
		}
		return true;
	}

	bool IsFocusTreeCanonical(const UWidgetBlueprint& Blueprint)
	{
		const TArray<TPair<FName, UClass*>> Expected = {
			{TEXT("InteractionPromptRoot"), UBorder::StaticClass()},
			{TEXT("InteractionPromptRow"), UHorizontalBox::StaticClass()},
			{TEXT("InputActionContainer"), USizeBox::StaticClass()},
			{TEXT("InputActionWidget"), UCommonActionWidget::StaticClass()},
			{TEXT("ActionTextBlock"), UCommonTextBlock::StaticClass()},
			{TEXT("BlockedIcon"), USizeBox::StaticClass()},
			{TEXT("BlockedIconCanvas"), UCanvasPanel::StaticClass()},
			{TEXT("BlockedIconShackle"), UBorder::StaticClass()},
			{TEXT("BlockedIconBody"), UBorder::StaticClass()},
			{TEXT("BlockedReasonTextBlock"), UCommonTextBlock::StaticClass()},
		};
		return HasExactWidgetSet(Blueprint, Expected);
	}

	bool IsNearbyTreeCanonical(const UWidgetBlueprint& Blueprint)
	{
		const TArray<TPair<FName, UClass*>> Expected = {
			{TEXT("NearbyIndicatorRoot"), USizeBox::StaticClass()},
			{TEXT("NearbyMarker"), UImage::StaticClass()},
		};
		return HasExactWidgetSet(Blueprint, Expected);
	}

	bool IsReticleTreeCanonical(const UWidgetBlueprint& Blueprint)
	{
		const TArray<TPair<FName, UClass*>> Expected = {
			{TEXT("InteractionReticleRoot"), USizeBox::StaticClass()},
			{TEXT("ReticleDot"), UImage::StaticClass()},
		};
		return HasExactWidgetSet(Blueprint, Expected);
	}

	UWidgetTree* ResetWidgetTree(UWidgetBlueprint& Blueprint)
	{
		Blueprint.Modify();
		if (Blueprint.WidgetTree)
		{
			Blueprint.WidgetTree->Rename(
				nullptr,
				GetTransientPackage(),
				REN_DontCreateRedirectors | REN_NonTransactional);
		}
		Blueprint.WidgetTree = NewObject<UWidgetTree>(
			&Blueprint,
			TEXT("WidgetTree"),
			RF_Transactional);
		Blueprint.WidgetTree->Modify();
		return Blueprint.WidgetTree;
	}

	bool RemoveBlueprintGraphLogic(UWidgetBlueprint& Blueprint)
	{
		bool bChanged = false;
		for (UEdGraph* Graph : Blueprint.UbergraphPages)
		{
			if (!Graph || Graph->Nodes.IsEmpty())
			{
				continue;
			}

			const TArray<TObjectPtr<UEdGraphNode>> Nodes = Graph->Nodes;
			for (UEdGraphNode* Node : Nodes)
			{
				if (Node)
				{
					FBlueprintEditorUtils::RemoveNode(&Blueprint, Node, true);
					bChanged = true;
				}
			}
		}

		TArray<UEdGraph*> GraphsToRemove;
		for (UEdGraph* Graph : Blueprint.FunctionGraphs)
		{
			GraphsToRemove.Add(Graph);
		}
		for (UEdGraph* Graph : Blueprint.MacroGraphs)
		{
			GraphsToRemove.Add(Graph);
		}
		if (!GraphsToRemove.IsEmpty())
		{
			FBlueprintEditorUtils::RemoveGraphs(&Blueprint, GraphsToRemove);
			bChanged = true;
		}
		return bChanged;
	}

	bool RemoveWidgetAnimations(UWidgetBlueprint& Blueprint)
	{
		if (Blueprint.Animations.IsEmpty())
		{
			return false;
		}

		for (UWidgetAnimation* Animation : Blueprint.Animations)
		{
			if (!Animation)
			{
				continue;
			}
			Blueprint.OnVariableRemoved(Animation->GetFName());
			Animation->Rename(
				nullptr,
				GetTransientPackage(),
				REN_DontCreateRedirectors | REN_NonTransactional);
		}
		Blueprint.Animations.Reset();
		return true;
	}

	void BuildFocusTree(UWidgetBlueprint& Blueprint)
	{
		UWidgetTree* Tree = ResetWidgetTree(Blueprint);
		UBorder* Root = Tree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("InteractionPromptRoot"));
		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("InteractionPromptRow"));
		USizeBox* GlyphSize = Tree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), TEXT("InputActionContainer"));
		UCommonActionWidget* InputAction = Tree->ConstructWidget<UCommonActionWidget>(
			UCommonActionWidget::StaticClass(), TEXT("InputActionWidget"));
		UCommonTextBlock* ActionText = Tree->ConstructWidget<UCommonTextBlock>(
			UCommonTextBlock::StaticClass(), TEXT("ActionTextBlock"));
		USizeBox* BlockedIcon = Tree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), TEXT("BlockedIcon"));
		UCanvasPanel* BlockedCanvas = Tree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), TEXT("BlockedIconCanvas"));
		UBorder* Shackle = Tree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("BlockedIconShackle"));
		UBorder* Body = Tree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("BlockedIconBody"));
		UCommonTextBlock* BlockedReason = Tree->ConstructWidget<UCommonTextBlock>(
			UCommonTextBlock::StaticClass(), TEXT("BlockedReasonTextBlock"));

		Tree->RootWidget = Root;
		Root->SetBrush(MakeRoundedBrush(
			FVector2D(64.0f, 28.0f),
			FLinearColor(0.012f, 0.018f, 0.028f, 0.72f),
			5.0f,
			FLinearColor(0.85f, 0.9f, 1.0f, 0.12f),
			0.75f));
		Root->SetPadding(FMargin(8.0f, 5.0f));
		Root->SetHorizontalAlignment(HAlign_Center);
		Root->SetVerticalAlignment(VAlign_Center);
		Root->SetVisibility(ESlateVisibility::HitTestInvisible);
		Root->AddChild(Row);

		GlyphSize->SetWidthOverride(24.0f);
		GlyphSize->SetHeightOverride(24.0f);
		GlyphSize->bIsVariable = true;
		GlyphSize->SetVisibility(ESlateVisibility::Collapsed);
		GlyphSize->AddChild(InputAction);
		InputAction->bIsVariable = true;
		InputAction->SetVisibility(ESlateVisibility::Collapsed);
		if (UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(GlyphSize))
		{
			Slot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		ActionText->bIsVariable = true;
		ActionText->SetText(FText::FromString(TEXT("Interact")));
		ConfigurePromptText(*ActionText, 15);
		if (UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(ActionText))
		{
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		BlockedIcon->bIsVariable = true;
		BlockedIcon->SetWidthOverride(13.0f);
		BlockedIcon->SetHeightOverride(15.0f);
		BlockedIcon->SetVisibility(ESlateVisibility::Collapsed);
		BlockedIcon->AddChild(BlockedCanvas);
		Shackle->SetBrush(MakeRoundedBrush(
			FVector2D(7.0f, 8.0f),
			FLinearColor::Transparent,
			3.0f,
			FLinearColor(0.95f, 0.97f, 1.0f, 0.92f),
			1.2f));
		Body->SetBrush(MakeRoundedBrush(
			FVector2D(11.0f, 8.0f),
			FLinearColor(0.95f, 0.97f, 1.0f, 0.92f),
			1.5f));
		if (UCanvasPanelSlot* Slot = BlockedCanvas->AddChildToCanvas(Shackle))
		{
			Slot->SetPosition(FVector2D(3.0f, 0.0f));
			Slot->SetSize(FVector2D(7.0f, 8.0f));
		}
		if (UCanvasPanelSlot* Slot = BlockedCanvas->AddChildToCanvas(Body))
		{
			Slot->SetPosition(FVector2D(1.0f, 6.0f));
			Slot->SetSize(FVector2D(11.0f, 8.0f));
		}
		if (UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(BlockedIcon))
		{
			Slot->SetPadding(FMargin(0.0f, 0.0f, 5.0f, 0.0f));
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		BlockedReason->bIsVariable = true;
		BlockedReason->SetText(FText::FromString(TEXT("Not available")));
		ConfigurePromptText(*BlockedReason, 14);
		if (UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(BlockedReason))
		{
			Slot->SetVerticalAlignment(VAlign_Center);
		}
	}

	void BuildNearbyTree(UWidgetBlueprint& Blueprint)
	{
		UWidgetTree* Tree = ResetWidgetTree(Blueprint);
		USizeBox* Root = Tree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), TEXT("NearbyIndicatorRoot"));
		UImage* Marker = Tree->ConstructWidget<UImage>(
			UImage::StaticClass(), TEXT("NearbyMarker"));
		Tree->RootWidget = Root;
		Root->SetWidthOverride(12.0f);
		Root->SetHeightOverride(12.0f);
		Root->SetVisibility(ESlateVisibility::HitTestInvisible);
		Marker->bIsVariable = true;
		Marker->SetBrush(MakeRoundedBrush(
			FVector2D(12.0f),
			FLinearColor(0.02f, 0.03f, 0.05f, 0.12f),
			6.0f,
			FLinearColor(0.92f, 0.96f, 1.0f, 0.72f),
			1.4f));
		Marker->SetVisibility(ESlateVisibility::Collapsed);
		Root->AddChild(Marker);
	}

	void BuildReticleTree(UWidgetBlueprint& Blueprint)
	{
		UWidgetTree* Tree = ResetWidgetTree(Blueprint);
		USizeBox* Root = Tree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), TEXT("InteractionReticleRoot"));
		UImage* Dot = Tree->ConstructWidget<UImage>(
			UImage::StaticClass(), TEXT("ReticleDot"));
		Tree->RootWidget = Root;
		Root->SetWidthOverride(4.0f);
		Root->SetHeightOverride(4.0f);
		Root->SetVisibility(ESlateVisibility::HitTestInvisible);
		Dot->SetBrush(MakeRoundedBrush(
			FVector2D(4.0f),
			FLinearColor(0.96f, 0.98f, 1.0f, 0.9f),
			2.0f,
			FLinearColor(0.0f, 0.0f, 0.0f, 0.68f),
			0.9f));
		Dot->SetVisibility(ESlateVisibility::HitTestInvisible);
		Root->AddChild(Dot);
	}

	UWidgetBlueprint* FindOrCreateWidgetBlueprint(
		const TCHAR* PackagePath,
		UClass* ParentClass,
		bool& bOutCreated)
	{
		bOutCreated = false;
		if (UWidgetBlueprint* Existing = LoadAsset<UWidgetBlueprint>(PackagePath))
		{
			return Existing;
		}

		const FString PackageString(PackagePath);
		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageString);
		const FString PackageFolder = FPackageName::GetLongPackagePath(PackageString);
		UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
		Factory->ParentClass = ParentClass;
		IAssetTools& AssetTools =
			FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		UWidgetBlueprint* Created = Cast<UWidgetBlueprint>(AssetTools.CreateAsset(
			AssetName,
			PackageFolder,
			UWidgetBlueprint::StaticClass(),
			Factory));
		bOutCreated = Created != nullptr;
		return Created;
	}

	bool AuthorWidgetBlueprint(
		const TCHAR* PackagePath,
		UClass* ParentClass,
		bool (*IsCanonical)(const UWidgetBlueprint&),
		void (*BuildTree)(UWidgetBlueprint&))
	{
		bool bCreated = false;
		UWidgetBlueprint* Blueprint =
			FindOrCreateWidgetBlueprint(PackagePath, ParentClass, bCreated);
		if (!Blueprint)
		{
			UE_LOG(LogTemp, Error, TEXT("Could not create interaction widget %s."), PackagePath);
			return false;
		}

		bool bChanged = bCreated;
		if (Blueprint->ParentClass != ParentClass)
		{
			UBlueprintEditorLibrary::ReparentBlueprint(Blueprint, ParentClass);
			bChanged = true;
		}
		bChanged |= RemoveBlueprintGraphLogic(*Blueprint);
		bChanged |= RemoveWidgetAnimations(*Blueprint);
		if (!IsCanonical(*Blueprint))
		{
			BuildTree(*Blueprint);
			bChanged = true;
		}

		if (bChanged || !Blueprint->GeneratedClass)
		{
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
			FKismetEditorUtilities::CompileBlueprint(Blueprint);
		}
		const bool bValid = Blueprint->GeneratedClass &&
			Blueprint->GeneratedClass->GetSuperClass() == ParentClass &&
			Blueprint->Status != BS_Error && IsCanonical(*Blueprint);
		if (!bValid)
		{
			UE_LOG(LogTemp, Error, TEXT("Interaction widget authoring failed for %s."), PackagePath);
		}
		return bValid;
	}

	FStructProperty* FindExtensionTagProperty()
	{
		return FindFProperty<FStructProperty>(
			UUIExtensionPointWidget::StaticClass(),
			TEXT("ExtensionPointTag"));
	}

	FGameplayTag* GetExtensionTag(UUIExtensionPointWidget* ExtensionPoint)
	{
		FStructProperty* Property = FindExtensionTagProperty();
		return ExtensionPoint && Property
			? Property->ContainerPtrToValuePtr<FGameplayTag>(ExtensionPoint)
			: nullptr;
	}

	bool AuthorHudCrosshairExtensionPoint()
	{
		UWidgetBlueprint* HudBlueprint = LoadAsset<UWidgetBlueprint>(HudLayoutPackage);
		if (!HudBlueprint || !HudBlueprint->WidgetTree)
		{
			UE_LOG(LogTemp, Error, TEXT("Missing authored HUD layout %s."), HudLayoutPackage);
			return false;
		}
		UOverlay* RootOverlay = Cast<UOverlay>(HudBlueprint->WidgetTree->RootWidget);
		const FGameplayTag CrosshairTag = FGameplayTag::RequestGameplayTag(
			TEXT("UI.HUD.Slot.Crosshair"), false);
		if (!RootOverlay || !CrosshairTag.IsValid() || !FindExtensionTagProperty())
		{
			UE_LOG(LogTemp, Error, TEXT("HUD Crosshair extension prerequisites are invalid."));
			return false;
		}

		TArray<UWidget*> Widgets;
		HudBlueprint->WidgetTree->GetAllWidgets(Widgets);
		UUIExtensionPointWidget* Canonical = Cast<UUIExtensionPointWidget>(
			HudBlueprint->WidgetTree->FindWidget(TEXT("CrosshairExtensionPoint")));
		TArray<UUIExtensionPointWidget*> Duplicates;
		for (UWidget* Widget : Widgets)
		{
			UUIExtensionPointWidget* Candidate = Cast<UUIExtensionPointWidget>(Widget);
			const FGameplayTag* CandidateTag = GetExtensionTag(Candidate);
			if (Candidate && Candidate != Canonical && CandidateTag && *CandidateTag == CrosshairTag)
			{
				Duplicates.Add(Candidate);
			}
		}

		bool bChanged = false;
		for (UUIExtensionPointWidget* Duplicate : Duplicates)
		{
			HudBlueprint->OnVariableRemoved(Duplicate->GetFName());
			HudBlueprint->WidgetTree->RemoveWidget(Duplicate);
			bChanged = true;
		}
		if (!Canonical)
		{
			Canonical = HudBlueprint->WidgetTree->ConstructWidget<UUIExtensionPointWidget>(
				UUIExtensionPointWidget::StaticClass(),
				TEXT("CrosshairExtensionPoint"));
			bChanged = Canonical != nullptr;
		}
		if (!Canonical)
		{
			return false;
		}
		if (!HudBlueprint->WidgetVariableNameToGuidMap.Contains(Canonical->GetFName()))
		{
			HudBlueprint->OnVariableAdded(Canonical->GetFName());
			bChanged = true;
		}

		FGameplayTag* AuthoredTag = GetExtensionTag(Canonical);
		if (!AuthoredTag)
		{
			return false;
		}
		if (*AuthoredTag != CrosshairTag)
		{
			Canonical->Modify();
			*AuthoredTag = CrosshairTag;
			bChanged = true;
		}
		Canonical->SetVisibility(ESlateVisibility::HitTestInvisible);
		UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(Canonical->Slot);
		if (!OverlaySlot || Canonical->GetParent() != RootOverlay)
		{
			Canonical->RemoveFromParent();
			OverlaySlot = RootOverlay->AddChildToOverlay(Canonical);
			bChanged = true;
		}
		if (!OverlaySlot)
		{
			return false;
		}
		if (OverlaySlot->GetHorizontalAlignment() != HAlign_Center)
		{
			OverlaySlot->SetHorizontalAlignment(HAlign_Center);
			bChanged = true;
		}
		if (OverlaySlot->GetVerticalAlignment() != VAlign_Center)
		{
			OverlaySlot->SetVerticalAlignment(VAlign_Center);
			bChanged = true;
		}

		if (bChanged)
		{
			HudBlueprint->Modify();
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(HudBlueprint);
			FKismetEditorUtilities::CompileBlueprint(HudBlueprint);
		}
		return HudBlueprint->GeneratedClass && HudBlueprint->Status != BS_Error;
	}

	bool AuthorExperienceRegistration()
	{
		UBlueprint* ExperienceBlueprint = LoadAsset<UBlueprint>(ExperiencePackage);
		UWidgetBlueprint* ReticleBlueprint = LoadAsset<UWidgetBlueprint>(ReticleWidgetPackage);
		URpgExperienceDefinition* Experience = ExperienceBlueprint && ExperienceBlueprint->GeneratedClass
			? Cast<URpgExperienceDefinition>(ExperienceBlueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
		UClass* ReticleClass = ReticleBlueprint ? ReticleBlueprint->GeneratedClass : nullptr;
		const FGameplayTag CrosshairTag = FGameplayTag::RequestGameplayTag(
			TEXT("UI.HUD.Slot.Crosshair"), false);
		if (!ExperienceBlueprint || !Experience || !ReticleClass || !CrosshairTag.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("Could not resolve Experience reticle registration inputs."));
			return false;
		}

		URpgGameFeatureAction_AddWidgets* Destination = nullptr;
		int32 RelatedCount = 0;
		bool bCanonical = false;
		for (UGameFeatureAction* Action : Experience->Actions)
		{
			URpgGameFeatureAction_AddWidgets* AddWidgets =
				Cast<URpgGameFeatureAction_AddWidgets>(Action);
			if (!AddWidgets)
			{
				continue;
			}
			Destination = Destination ? Destination : AddWidgets;
			for (const FRpgGameFeatureWidgetEntry& Entry : AddWidgets->Widgets)
			{
				const bool bRelated = Entry.SlotTag == CrosshairTag ||
					Entry.WidgetClass.ToSoftObjectPath() == FSoftObjectPath(ReticleClass);
				if (!bRelated)
				{
					continue;
				}
				++RelatedCount;
				bCanonical = bCanonical ||
					(Entry.SlotTag == CrosshairTag &&
					 Entry.WidgetClass.Get() == ReticleClass &&
					 Entry.Priority == -1);
			}
		}

		if (RelatedCount == 1 && bCanonical)
		{
			return true;
		}
		ExperienceBlueprint->Modify();
		Experience->Modify();
		if (!Destination)
		{
			Destination = NewObject<URpgGameFeatureAction_AddWidgets>(
				Experience,
				TEXT("Add_Interaction_Hud_Widgets"),
				RF_Transactional);
			Experience->Actions.Add(Destination);
		}
		for (UGameFeatureAction* Action : Experience->Actions)
		{
			if (URpgGameFeatureAction_AddWidgets* AddWidgets =
				Cast<URpgGameFeatureAction_AddWidgets>(Action))
			{
				AddWidgets->Modify();
				AddWidgets->Widgets.RemoveAll(
					[ReticleClass, CrosshairTag](const FRpgGameFeatureWidgetEntry& Entry)
					{
						return Entry.SlotTag == CrosshairTag ||
							Entry.WidgetClass.ToSoftObjectPath() == FSoftObjectPath(ReticleClass);
					});
			}
		}

		FRpgGameFeatureWidgetEntry Entry;
		Entry.WidgetClass = ReticleClass;
		Entry.SlotTag = CrosshairTag;
		Entry.Priority = -1;
		Destination->Widgets.Add(Entry);
#if WITH_EDITORONLY_DATA
		Experience->UpdateAssetBundleData();
#endif
		ExperienceBlueprint->MarkPackageDirty();
		return true;
	}

	bool EnsureDefaultAnchor(
		const TCHAR* PackagePath,
		const FName PreferredParentName,
		const FVector RelativeLocation)
	{
		UBlueprint* Blueprint = LoadAsset<UBlueprint>(PackagePath);
		UBlueprintGeneratedClass* GeneratedClass = Blueprint
			? Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass)
			: nullptr;
		AActor* ActorCDO = GeneratedClass
			? Cast<AActor>(GeneratedClass->GetDefaultObject())
			: nullptr;
		USimpleConstructionScript* SCS = Blueprint ? Blueprint->SimpleConstructionScript : nullptr;
		if (!Blueprint || !GeneratedClass || !ActorCDO || !SCS)
		{
			UE_LOG(LogTemp, Warning, TEXT("Skipping prompt anchor: unsupported Blueprint %s."), PackagePath);
			return false;
		}

		int32 DefaultAnchorCount = 0;
		for (USCS_Node* Node : SCS->GetAllNodes())
		{
			const URpgInteractionPromptAnchorComponent* Anchor = Node
				? Cast<URpgInteractionPromptAnchorComponent>(Node->ComponentTemplate)
				: nullptr;
			DefaultAnchorCount += Anchor &&
				Anchor->AnchorId == FName(TEXT("Default"))
					? 1
					: 0;
		}
		if (DefaultAnchorCount == 1)
		{
			return true;
		}
		if (DefaultAnchorCount > 1)
		{
			UE_LOG(LogTemp, Error, TEXT("%s already has duplicate Default prompt anchors."), PackagePath);
			return false;
		}

		USceneComponent* ParentComponent = nullptr;
		TInlineComponentArray<USceneComponent*> SceneComponents(ActorCDO);
		for (USceneComponent* Component : SceneComponents)
		{
			if (Component && Component->GetFName() == PreferredParentName)
			{
				ParentComponent = Component;
				break;
			}
		}
		ParentComponent = ParentComponent ? ParentComponent : ActorCDO->GetRootComponent();
		if (!ParentComponent)
		{
			UE_LOG(LogTemp, Warning, TEXT("Skipping prompt anchor: %s has no unambiguous scene parent."), PackagePath);
			return false;
		}

		Blueprint->Modify();
		SCS->Modify();
		USCS_Node* AnchorNode = SCS->CreateNode(
			URpgInteractionPromptAnchorComponent::StaticClass(),
			TEXT("InteractionPromptAnchor"));
		URpgInteractionPromptAnchorComponent* AnchorTemplate = AnchorNode
			? Cast<URpgInteractionPromptAnchorComponent>(AnchorNode->ComponentTemplate)
			: nullptr;
		if (!AnchorNode || !AnchorTemplate)
		{
			return false;
		}
		AnchorTemplate->AnchorId = FName(TEXT("Default"));
		AnchorTemplate->SetRelativeLocation(RelativeLocation);

		USCS_Node* ParentNode = nullptr;
		for (USCS_Node* Node : SCS->GetAllNodes())
		{
			if (Node && Node->GetActualComponentTemplate(GeneratedClass) == ParentComponent)
			{
				ParentNode = Node;
				break;
			}
		}
		if (ParentNode)
		{
			ParentNode->AddChildNode(AnchorNode);
		}
		else
		{
			SCS->AddNode(AnchorNode);
			AnchorNode->SetParent(ParentComponent);
		}

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		return Blueprint->Status != BS_Error;
	}
}

bool URpgInteractionHudAssetTools::AuthorInteractionHudAssets()
{
	using namespace RpgInteractionHudAuthoring;
	const bool bFocus = AuthorWidgetBlueprint(
		FocusWidgetPackage,
		URpgInteractionPromptWidget::StaticClass(),
		&IsFocusTreeCanonical,
		&BuildFocusTree);
	const bool bNearby = AuthorWidgetBlueprint(
		NearbyWidgetPackage,
		URpgInteractionPromptWidget::StaticClass(),
		&IsNearbyTreeCanonical,
		&BuildNearbyTree);
	const bool bReticle = AuthorWidgetBlueprint(
		ReticleWidgetPackage,
		URpgInteractionReticleWidget::StaticClass(),
		&IsReticleTreeCanonical,
		&BuildReticleTree);
	const bool bHud = bReticle && AuthorHudCrosshairExtensionPoint();
	const bool bExperience = bHud && AuthorExperienceRegistration();
	return bFocus && bNearby && bReticle && bHud && bExperience;
}

bool URpgInteractionHudAssetTools::AuthorInteractionReferencePromptAnchors()
{
	using namespace RpgInteractionHudAuthoring;
	struct FAnchorTarget
	{
		const TCHAR* PackagePath;
		FName PreferredParent;
		FVector RelativeLocation;
	};
	const FAnchorTarget Targets[] = {
		{
			TEXT("/Game/SurvivalRpg/Interaction/Items/BP_InteractableRock"),
			TEXT("DisplayMesh"),
			FVector(0.0f, 0.0f, 35.0f)
		},
		{
			TEXT("/Game/SurvivalRpg/Storage/BP_StorageUnit_Wood"),
			NAME_None,
			FVector(0.0f, 0.0f, 100.0f)
		},
		{
			TEXT("/Game/SurvivalRpg/Interaction/Reference/BP_InteractableDoor_Reference"),
			TEXT("DoorMesh"),
			FVector::ZeroVector
		},
		{
			TEXT("/GF_Portals_Core/Portals/BP_Portal_RiftGruntTrial"),
			NAME_None,
			FVector(0.0f, 0.0f, 100.0f)
		},
		{
			TEXT("/GF_Portals_Core/Portals/BP_PortalExit_Prototype"),
			NAME_None,
			FVector(0.0f, 0.0f, 100.0f)
		},
	};

	bool bSuccess = true;
	for (int32 TargetIndex = 0; TargetIndex < UE_ARRAY_COUNT(Targets); ++TargetIndex)
	{
		const FAnchorTarget& Target = Targets[TargetIndex];
		const bool bAuthored = EnsureDefaultAnchor(
			Target.PackagePath,
			Target.PreferredParent,
			Target.RelativeLocation);
		if (TargetIndex < 3)
		{
			bSuccess &= bAuthored;
		}
		else if (!bAuthored)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("Optional portal prompt anchor skipped for %s; runtime Actor-Bounds placement remains active."),
				Target.PackagePath);
		}
	}
	return bSuccess;
}
