#include "UI/RpgWidgetBlueprintAssetTools.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "WidgetBlueprint.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgWidgetBlueprintAssetTools)

bool URpgWidgetBlueprintAssetTools::RemoveSourceWidget(
	UWidgetBlueprint* WidgetBlueprint,
	const FName WidgetName)
{
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree || WidgetName.IsNone())
	{
		return false;
	}

	UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(WidgetName);
	UPanelWidget* OriginalParent = Widget ? Widget->GetParent() : nullptr;
	if (!Widget || !OriginalParent || WidgetBlueprint->WidgetTree->RootWidget == Widget)
	{
		return false;
	}

	WidgetBlueprint->Modify();
	WidgetBlueprint->WidgetTree->Modify();
	Widget->Modify();

	if (!WidgetBlueprint->WidgetTree->RemoveWidget(Widget))
	{
		return false;
	}

	const FName TransientName = MakeUniqueObjectName(
		GetTransientPackage(),
		Widget->GetClass(),
		FName(*FString::Printf(TEXT("Removed_%s"), *WidgetName.ToString())));
	if (!Widget->Rename(
		*TransientName.ToString(),
		GetTransientPackage(),
		REN_DontCreateRedirectors | REN_NonTransactional))
	{
		OriginalParent->AddChild(Widget);
		return false;
	}

#if WITH_EDITORONLY_DATA
	WidgetBlueprint->OnVariableRemoved(WidgetName);
#endif

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	return WidgetBlueprint->WidgetTree->FindWidget(WidgetName) == nullptr;
}

UUserWidget* URpgWidgetBlueprintAssetTools::CreateInitializedWidgetBlueprintInstance(
	UWidgetBlueprint* WidgetBlueprint)
{
	if (!WidgetBlueprint || !WidgetBlueprint->GeneratedClass || !GEditor ||
		!WidgetBlueprint->GeneratedClass->IsChildOf(UUserWidget::StaticClass()))
	{
		return nullptr;
	}

	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	if (!EditorWorld || EditorWorld->bIsTearingDown)
	{
		return nullptr;
	}

	const TSubclassOf<UUserWidget> WidgetClass(
		WidgetBlueprint->GeneratedClass.Get());
	return UUserWidget::CreateWidgetInstance(
		*EditorWorld,
		WidgetClass,
		NAME_None);
}
