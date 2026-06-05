#include "RpgGameFeatureAction_AddComponents.h"

#include "Components/GameFrameworkComponentManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "RpgGameFeatures"

void URpgGameFeatureAction_AddComponents::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	ComponentRequests.FindOrAdd(Context).Reset();
	Super::OnGameFeatureActivating(Context);
}

void URpgGameFeatureAction_AddComponents::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);
	ComponentRequests.Remove(Context);
}

#if WITH_EDITOR
EDataValidationResult URpgGameFeatureAction_AddComponents::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	for (int32 EntryIndex = 0; EntryIndex < ComponentList.Num(); ++EntryIndex)
	{
		const FRpgGameFeatureComponentEntry& Entry = ComponentList[EntryIndex];
		if (Entry.ActorClass.IsNull())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("ComponentEntryHasNullActor", "Null ActorClass at index {0} in ComponentList."), FText::AsNumber(EntryIndex)));
		}

		if (Entry.ComponentClass.IsNull())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("ComponentEntryHasNullComponent", "Null ComponentClass at index {0} in ComponentList."), FText::AsNumber(EntryIndex)));
		}
	}

	return Result;
}
#endif

void URpgGameFeatureAction_AddComponents::AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext)
{
	UWorld* World = WorldContext.World();
	UGameInstance* GameInstance = WorldContext.OwningGameInstance;
	if (!World || !World->IsGameWorld() || !GameInstance)
	{
		return;
	}

	UGameFrameworkComponentManager* ComponentManager = UGameInstance::GetSubsystem<UGameFrameworkComponentManager>(GameInstance);
	if (!ComponentManager)
	{
		return;
	}

	const ENetMode NetMode = World->GetNetMode();
	const bool bIsClient = NetMode != NM_DedicatedServer;
	const bool bIsServer = NetMode != NM_Client;
	TArray<TSharedPtr<FComponentRequestHandle>>& Handles = ComponentRequests.FindOrAdd(ChangeContext);

	for (const FRpgGameFeatureComponentEntry& Entry : ComponentList)
	{
		const bool bShouldAdd = (bIsClient && Entry.bClientComponent) || (bIsServer && Entry.bServerComponent);
		if (!bShouldAdd || Entry.ActorClass.IsNull() || Entry.ComponentClass.IsNull())
		{
			continue;
		}

		if (TSubclassOf<UActorComponent> ComponentClass = Entry.ComponentClass.LoadSynchronous())
		{
			Handles.Add(ComponentManager->AddComponentRequest(Entry.ActorClass, ComponentClass));
		}
	}
}

#undef LOCTEXT_NAMESPACE
