#include "RpgGameFeatureAction_AddInputConfig.h"

#include "Components/GameFrameworkComponentManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "SurvivalRpg/Core/Character/RpgPawnGameplayComponent.h"
#include "SurvivalRpg/Input/RpgInputConfig.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "RpgGameFeatures"

void URpgGameFeatureAction_AddInputConfig::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	FPerContextData& ActiveData = ContextData.FindOrAdd(Context);
	if (!ensure(ActiveData.ExtensionRequestHandles.IsEmpty()) || !ensure(ActiveData.AddedInputConfigEntries.IsEmpty()))
	{
		Reset(ActiveData);
	}

	Super::OnGameFeatureActivating(Context);
}

void URpgGameFeatureAction_AddInputConfig::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);

	if (FPerContextData* ActiveData = ContextData.Find(Context))
	{
		Reset(*ActiveData);
	}
}

#if WITH_EDITOR
EDataValidationResult URpgGameFeatureAction_AddInputConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	for (int32 Index = 0; Index < InputConfigs.Num(); ++Index)
	{
		if (InputConfigs[Index].IsNull())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("NullInputConfig", "Null InputConfig at index {0}."), FText::AsNumber(Index)));
		}
	}

	return Result;
}
#endif

void URpgGameFeatureAction_AddInputConfig::AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext)
{
	UWorld* World = WorldContext.World();
	UGameInstance* GameInstance = WorldContext.OwningGameInstance;
	FPerContextData& ActiveData = ContextData.FindOrAdd(ChangeContext);

	if (GameInstance && World && World->IsGameWorld())
	{
		if (UGameFrameworkComponentManager* ComponentManager = UGameInstance::GetSubsystem<UGameFrameworkComponentManager>(GameInstance))
		{
			UGameFrameworkComponentManager::FExtensionHandlerDelegate AddInputConfigDelegate =
				UGameFrameworkComponentManager::FExtensionHandlerDelegate::CreateUObject(
					this,
					&ThisClass::HandleControllerExtension,
					ChangeContext);

			ActiveData.ExtensionRequestHandles.Add(ComponentManager->AddExtensionHandler(APlayerController::StaticClass(), AddInputConfigDelegate));
		}
	}
}

void URpgGameFeatureAction_AddInputConfig::Reset(FPerContextData& ActiveData)
{
	ActiveData.ExtensionRequestHandles.Empty();

	for (int32 EntryIndex = ActiveData.AddedInputConfigEntries.Num() - 1; EntryIndex >= 0; --EntryIndex)
	{
		RemoveEntry(EntryIndex, ActiveData);
	}
}

void URpgGameFeatureAction_AddInputConfig::HandleControllerExtension(AActor* Actor, FName EventName, FGameFeatureStateChangeContext ChangeContext)
{
	APlayerController* PlayerController = CastChecked<APlayerController>(Actor);
	FPerContextData& ActiveData = ContextData.FindOrAdd(ChangeContext);

	if (EventName == UGameFrameworkComponentManager::NAME_ExtensionRemoved ||
		EventName == UGameFrameworkComponentManager::NAME_ReceiverRemoved)
	{
		RemoveInputConfigForController(PlayerController, ActiveData);
	}
	else if (EventName == UGameFrameworkComponentManager::NAME_ExtensionAdded ||
		EventName == URpgPawnGameplayComponent::NAME_BindInputsNow)
	{
		AddInputConfigForController(PlayerController, ActiveData);
	}
}

void URpgGameFeatureAction_AddInputConfig::AddInputConfigForController(APlayerController* PlayerController, FPerContextData& ActiveData)
{
	if (!PlayerController || !PlayerController->GetLocalPlayer())
	{
		return;
	}

	APawn* Pawn = PlayerController->GetPawn();
	URpgPawnGameplayComponent* PawnGameplayComponent = URpgPawnGameplayComponent::FindPawnGameplayComponent(Pawn);
	if (!PawnGameplayComponent || !PawnGameplayComponent->IsReadyToBindInputs())
	{
		return;
	}

	if (FindEntryIndexForComponent(PawnGameplayComponent, ActiveData) != INDEX_NONE)
	{
		return;
	}

	FComponentInputConfigEntry& Entry = ActiveData.AddedInputConfigEntries.AddDefaulted_GetRef();
	Entry.PawnGameplayComponent = PawnGameplayComponent;

	for (const TSoftObjectPtr<const URpgInputConfig>& InputConfigPtr : InputConfigs)
	{
		if (const URpgInputConfig* InputConfig = InputConfigPtr.LoadSynchronous())
		{
			PawnGameplayComponent->AddAdditionalInputConfig(InputConfig);
			Entry.AddedInputConfigs.Add(InputConfig);
		}
	}
}

void URpgGameFeatureAction_AddInputConfig::RemoveInputConfigForController(APlayerController* PlayerController, FPerContextData& ActiveData)
{
	const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	const URpgPawnGameplayComponent* PawnGameplayComponent = URpgPawnGameplayComponent::FindPawnGameplayComponent(Pawn);
	const int32 EntryIndex = FindEntryIndexForComponent(PawnGameplayComponent, ActiveData);
	if (EntryIndex != INDEX_NONE)
	{
		RemoveEntry(EntryIndex, ActiveData);
	}
}

void URpgGameFeatureAction_AddInputConfig::RemoveEntry(int32 EntryIndex, FPerContextData& ActiveData)
{
	if (!ActiveData.AddedInputConfigEntries.IsValidIndex(EntryIndex))
	{
		return;
	}

	FComponentInputConfigEntry& Entry = ActiveData.AddedInputConfigEntries[EntryIndex];
	if (URpgPawnGameplayComponent* PawnGameplayComponent = Entry.PawnGameplayComponent.Get())
	{
		for (const URpgInputConfig* InputConfig : Entry.AddedInputConfigs)
		{
			PawnGameplayComponent->RemoveAdditionalInputConfig(InputConfig);
		}
	}

	ActiveData.AddedInputConfigEntries.RemoveAtSwap(EntryIndex);
}

int32 URpgGameFeatureAction_AddInputConfig::FindEntryIndexForComponent(const URpgPawnGameplayComponent* PawnGameplayComponent, const FPerContextData& ActiveData) const
{
	if (!PawnGameplayComponent)
	{
		return INDEX_NONE;
	}

	for (int32 EntryIndex = 0; EntryIndex < ActiveData.AddedInputConfigEntries.Num(); ++EntryIndex)
	{
		if (ActiveData.AddedInputConfigEntries[EntryIndex].PawnGameplayComponent.Get() == PawnGameplayComponent)
		{
			return EntryIndex;
		}
	}

	return INDEX_NONE;
}

#undef LOCTEXT_NAMESPACE
