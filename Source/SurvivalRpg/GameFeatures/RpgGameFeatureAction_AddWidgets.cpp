#include "RpgGameFeatureAction_AddWidgets.h"

#include "CommonActivatableWidget.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFeaturesSubsystemSettings.h"
#include "GameFramework/PlayerController.h"
#include "PrimaryGameLayout.h"
#include "SurvivalRpg/UI/RpgHUD.h"
#include "UIExtensionSystem.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "RpgGameFeatures"

DEFINE_LOG_CATEGORY_STATIC(LogRpgGameFeatureAction_AddWidgets, Log, All);

void URpgGameFeatureAction_AddWidgets::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	FPerContextData& ActiveData = ContextData.FindOrAdd(Context);

	if (!ensure(ActiveData.ComponentRequests.IsEmpty()) || !ensure(ActiveData.ActorData.IsEmpty()))
	{
		Reset(ActiveData);
	}

	Super::OnGameFeatureActivating(Context);
}

void URpgGameFeatureAction_AddWidgets::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);

	if (FPerContextData* ActiveData = ContextData.Find(Context))
	{
		Reset(*ActiveData);
		ContextData.Remove(Context);
	}
}

#if WITH_EDITORONLY_DATA
void URpgGameFeatureAction_AddWidgets::AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData)
{
	for (const FRpgGameFeatureWidgetLayoutEntry& Entry : Layouts)
	{
		if (!Entry.LayoutClass.IsNull())
		{
			AssetBundleData.AddBundleAsset(UGameFeaturesSubsystemSettings::LoadStateClient, Entry.LayoutClass.ToSoftObjectPath().GetAssetPath());
		}
	}

	for (const FRpgGameFeatureWidgetEntry& Entry : Widgets)
	{
		if (!Entry.WidgetClass.IsNull())
		{
			AssetBundleData.AddBundleAsset(UGameFeaturesSubsystemSettings::LoadStateClient, Entry.WidgetClass.ToSoftObjectPath().GetAssetPath());
		}
	}
}
#endif

#if WITH_EDITOR
EDataValidationResult URpgGameFeatureAction_AddWidgets::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	for (int32 EntryIndex = 0; EntryIndex < Layouts.Num(); ++EntryIndex)
	{
		const FRpgGameFeatureWidgetLayoutEntry& Entry = Layouts[EntryIndex];

		if (Entry.LayoutClass.IsNull())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("LayoutHasNullClass", "Null LayoutClass at index {0} in Layouts."), FText::AsNumber(EntryIndex)));
		}

		if (!Entry.LayerTag.IsValid())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("LayoutHasNoLayerTag", "LayerTag is not set at index {0} in Layouts."), FText::AsNumber(EntryIndex)));
		}
	}

	for (int32 EntryIndex = 0; EntryIndex < Widgets.Num(); ++EntryIndex)
	{
		const FRpgGameFeatureWidgetEntry& Entry = Widgets[EntryIndex];

		if (Entry.WidgetClass.IsNull())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("WidgetHasNullClass", "Null WidgetClass at index {0} in Widgets."), FText::AsNumber(EntryIndex)));
		}

		if (!Entry.SlotTag.IsValid())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("WidgetHasNoSlotTag", "SlotTag is not set at index {0} in Widgets."), FText::AsNumber(EntryIndex)));
		}
	}

	return Result;
}
#endif

void URpgGameFeatureAction_AddWidgets::AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext)
{
	UWorld* World = WorldContext.World();
	UGameInstance* GameInstance = WorldContext.OwningGameInstance;
	FPerContextData& ActiveData = ContextData.FindOrAdd(ChangeContext);

	if (!GameInstance || !World || !World->IsGameWorld() || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (UGameFrameworkComponentManager* ComponentManager = UGameInstance::GetSubsystem<UGameFrameworkComponentManager>(GameInstance))
	{
		UE_LOG(LogRpgGameFeatureAction_AddWidgets, Log, TEXT("Registering widget extension handler in world [%s]. Layouts=%d Widgets=%d"),
			*GetNameSafe(World),
			Layouts.Num(),
			Widgets.Num());

		UGameFrameworkComponentManager::FExtensionHandlerDelegate AddWidgetsDelegate =
			UGameFrameworkComponentManager::FExtensionHandlerDelegate::CreateUObject(
				this,
				&ThisClass::HandleActorExtension,
				ChangeContext);

		ActiveData.ComponentRequests.Add(ComponentManager->AddExtensionHandler(ARpgHUD::StaticClass(), AddWidgetsDelegate));
	}
}

UPrimaryGameLayout* URpgGameFeatureAction_AddWidgets::GetPrimaryGameLayout(ULocalPlayer* LocalPlayer) const
{
	return LocalPlayer ? UPrimaryGameLayout::GetPrimaryGameLayout(LocalPlayer) : nullptr;
}

void URpgGameFeatureAction_AddWidgets::Reset(FPerContextData& ActiveData)
{
	for (TPair<FObjectKey, FPerActorData>& Pair : ActiveData.ActorData)
	{
		for (FUIExtensionHandle& Handle : Pair.Value.ExtensionHandles)
		{
			Handle.Unregister();
		}

		for (TWeakObjectPtr<UCommonActivatableWidget>& AddedLayout : Pair.Value.LayoutsAdded)
		{
			if (UCommonActivatableWidget* Layout = AddedLayout.Get())
			{
				Layout->DeactivateWidget();
			}
		}
	}

	ActiveData.ActorData.Empty();
	ActiveData.ComponentRequests.Empty();
}

void URpgGameFeatureAction_AddWidgets::HandleActorExtension(AActor* Actor, FName EventName, FGameFeatureStateChangeContext ChangeContext)
{
	FPerContextData* ActiveData = ContextData.Find(ChangeContext);
	if (!Actor || !ActiveData)
	{
		return;
	}

	UE_LOG(LogRpgGameFeatureAction_AddWidgets, Log, TEXT("HUD extension event [%s] for actor [%s]."),
		*EventName.ToString(),
		*GetNameSafe(Actor));

	if (EventName == UGameFrameworkComponentManager::NAME_ExtensionRemoved ||
		EventName == UGameFrameworkComponentManager::NAME_ReceiverRemoved)
	{
		RemoveWidgets(Actor, *ActiveData);
	}
	else if (EventName == UGameFrameworkComponentManager::NAME_ExtensionAdded ||
		EventName == UGameFrameworkComponentManager::NAME_GameActorReady)
	{
		AddWidgets(Actor, *ActiveData);
	}
}

void URpgGameFeatureAction_AddWidgets::AddWidgets(AActor* Actor, FPerContextData& ActiveData)
{
	ARpgHUD* HUD = Cast<ARpgHUD>(Actor);
	if (!HUD || ActiveData.ActorData.Contains(HUD))
	{
		return;
	}

	APlayerController* OwningPlayerController = HUD->GetOwningPlayerController();
	if (!OwningPlayerController)
	{
		return;
	}

	ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(OwningPlayerController->Player);
	if (!LocalPlayer)
	{
		UE_LOG(LogRpgGameFeatureAction_AddWidgets, Warning, TEXT("Cannot add widgets for HUD [%s]: owning player is not local."),
			*GetNameSafe(HUD));
		return;
	}

	UPrimaryGameLayout* RootLayout = GetPrimaryGameLayout(LocalPlayer);
	if (!RootLayout)
	{
		UE_LOG(LogRpgGameFeatureAction_AddWidgets, Warning, TEXT("Cannot add widgets for HUD [%s]: no PrimaryGameLayout."),
			*GetNameSafe(HUD));
		return;
	}

	FPerActorData& ActorData = ActiveData.ActorData.FindOrAdd(HUD);

	for (const FRpgGameFeatureWidgetLayoutEntry& Entry : Layouts)
	{
		if (Entry.LayerTag.IsValid())
		{
			if (!RootLayout->GetLayerWidget(Entry.LayerTag))
			{
				UE_LOG(LogRpgGameFeatureAction_AddWidgets, Warning, TEXT("Cannot push layout [%s] for HUD [%s]: PrimaryGameLayout [%s] has no layer [%s]."),
					*Entry.LayoutClass.ToString(),
					*GetNameSafe(HUD),
					*GetNameSafe(RootLayout),
					*Entry.LayerTag.ToString());
				continue;
			}

			if (TSubclassOf<UCommonActivatableWidget> ConcreteWidgetClass = Entry.LayoutClass.LoadSynchronous())
			{
				if (UCommonActivatableWidget* AddedLayout = RootLayout->PushWidgetToLayerStack<UCommonActivatableWidget>(Entry.LayerTag, ConcreteWidgetClass))
				{
					UE_LOG(LogRpgGameFeatureAction_AddWidgets, Log, TEXT("Pushed layout [%s] to layer [%s] for HUD [%s]."),
						*GetNameSafe(AddedLayout),
						*Entry.LayerTag.ToString(),
						*GetNameSafe(HUD));
					ActorData.LayoutsAdded.Add(AddedLayout);
				}
				else
				{
					UE_LOG(LogRpgGameFeatureAction_AddWidgets, Warning, TEXT("Failed to push layout class [%s] to layer [%s] for HUD [%s]."),
						*GetNameSafe(ConcreteWidgetClass),
						*Entry.LayerTag.ToString(),
						*GetNameSafe(HUD));
				}
			}
		}
	}

	if (UUIExtensionSubsystem* ExtensionSubsystem = HUD->GetWorld()->GetSubsystem<UUIExtensionSubsystem>())
	{
		for (const FRpgGameFeatureWidgetEntry& Entry : Widgets)
		{
			if (Entry.SlotTag.IsValid())
			{
				if (TSubclassOf<UUserWidget> ConcreteWidgetClass = Entry.WidgetClass.LoadSynchronous())
				{
					ActorData.ExtensionHandles.Add(ExtensionSubsystem->RegisterExtensionAsWidgetForContext(
						Entry.SlotTag,
						LocalPlayer,
						ConcreteWidgetClass,
						Entry.Priority));
					UE_LOG(LogRpgGameFeatureAction_AddWidgets, Log, TEXT("Registered widget [%s] for slot [%s] on HUD [%s]."),
						*GetNameSafe(ConcreteWidgetClass),
						*Entry.SlotTag.ToString(),
						*GetNameSafe(HUD));
				}
			}
		}
	}
	else if (!Widgets.IsEmpty())
	{
		UE_LOG(LogRpgGameFeatureAction_AddWidgets, Warning, TEXT("Cannot register HUD slot widgets for [%s]: UIExtensionSubsystem is missing."),
			*GetNameSafe(HUD));
	}
}

void URpgGameFeatureAction_AddWidgets::RemoveWidgets(AActor* Actor, FPerContextData& ActiveData)
{
	if (!Actor)
	{
		return;
	}

	if (FPerActorData* ActorData = ActiveData.ActorData.Find(Actor))
	{
		for (FUIExtensionHandle& Handle : ActorData->ExtensionHandles)
		{
			Handle.Unregister();
		}

		for (TWeakObjectPtr<UCommonActivatableWidget>& AddedLayout : ActorData->LayoutsAdded)
		{
			if (UCommonActivatableWidget* Layout = AddedLayout.Get())
			{
				Layout->DeactivateWidget();
			}
		}

		ActiveData.ActorData.Remove(Actor);
	}
}

#undef LOCTEXT_NAMESPACE
