// Fill out your copyright notice in the Description page of Project Settings.

#include "RpgGameFeatureAction_AddAbilities.h"

#include "AbilitySystemComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/Core/Player/RpgBasePlayerState.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "RpgGameFeatures"

void URpgGameFeatureAction_AddAbilities::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	FPerContextData& ActiveData = ContextData.FindOrAdd(Context);

	if (!ensure(ActiveData.ActiveExtensions.IsEmpty()) || !ensure(ActiveData.ComponentRequests.IsEmpty()))
	{
		Reset(ActiveData);
	}

	Super::OnGameFeatureActivating(Context);
}

void URpgGameFeatureAction_AddAbilities::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);

	if (FPerContextData* ActiveData = ContextData.Find(Context))
	{
		Reset(*ActiveData);
	}
}

#if WITH_EDITOR
EDataValidationResult URpgGameFeatureAction_AddAbilities::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	for (int32 EntryIndex = 0; EntryIndex < AbilitiesList.Num(); ++EntryIndex)
	{
		const FRpgGameFeatureAbilitiesEntry& Entry = AbilitiesList[EntryIndex];
		if (Entry.ActorClass.IsNull())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("EntryHasNullActor", "Null ActorClass at index {0} in AbilitiesList."), FText::AsNumber(EntryIndex)));
		}

		if (Entry.GrantedAbilities.IsEmpty() && Entry.GrantedAttributes.IsEmpty() && Entry.GrantedAbilitySets.IsEmpty())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("EntryHasNoGrants", "Index {0} in AbilitiesList has no grants."), FText::AsNumber(EntryIndex)));
		}
	}

	return Result;
}
#endif

void URpgGameFeatureAction_AddAbilities::AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext)
{
	UWorld* World = WorldContext.World();
	UGameInstance* GameInstance = WorldContext.OwningGameInstance;
	FPerContextData& ActiveData = ContextData.FindOrAdd(ChangeContext);

	if (GameInstance && World && World->IsGameWorld())
	{
		if (UGameFrameworkComponentManager* ComponentManager = UGameInstance::GetSubsystem<UGameFrameworkComponentManager>(GameInstance))
		{
			for (int32 EntryIndex = 0; EntryIndex < AbilitiesList.Num(); ++EntryIndex)
			{
				const FRpgGameFeatureAbilitiesEntry& Entry = AbilitiesList[EntryIndex];
				if (!Entry.ActorClass.IsNull())
				{
					UGameFrameworkComponentManager::FExtensionHandlerDelegate AddAbilitiesDelegate =
						UGameFrameworkComponentManager::FExtensionHandlerDelegate::CreateUObject(
							this,
							&ThisClass::HandleActorExtension,
							EntryIndex,
							ChangeContext);

					ActiveData.ComponentRequests.Add(ComponentManager->AddExtensionHandler(Entry.ActorClass, AddAbilitiesDelegate));
				}
			}
		}
	}
}

void URpgGameFeatureAction_AddAbilities::Reset(FPerContextData& ActiveData)
{
	while (!ActiveData.ActiveExtensions.IsEmpty())
	{
		auto ExtensionIt = ActiveData.ActiveExtensions.CreateIterator();
		RemoveActorAbilities(ExtensionIt->Key, ActiveData);
	}

	ActiveData.ComponentRequests.Empty();
}

void URpgGameFeatureAction_AddAbilities::HandleActorExtension(AActor* Actor, FName EventName, int32 EntryIndex, FGameFeatureStateChangeContext ChangeContext)
{
	FPerContextData* ActiveData = ContextData.Find(ChangeContext);
	if (!AbilitiesList.IsValidIndex(EntryIndex) || !ActiveData)
	{
		return;
	}

	const FRpgGameFeatureAbilitiesEntry& Entry = AbilitiesList[EntryIndex];
	if (EventName == UGameFrameworkComponentManager::NAME_ExtensionRemoved ||
		EventName == UGameFrameworkComponentManager::NAME_ReceiverRemoved)
	{
		RemoveActorAbilities(Actor, *ActiveData);
	}
	else if (EventName == UGameFrameworkComponentManager::NAME_ExtensionAdded ||
		EventName == ARpgBasePlayerState::NAME_RpgAbilityReady)
	{
		AddActorAbilities(Actor, Entry, *ActiveData);
	}
}

void URpgGameFeatureAction_AddAbilities::AddActorAbilities(AActor* Actor, const FRpgGameFeatureAbilitiesEntry& AbilitiesEntry, FPerContextData& ActiveData)
{
	check(Actor);

	if (!Actor->HasAuthority() || ActiveData.ActiveExtensions.Find(Actor))
	{
		return;
	}

	URpgAbilitySystemComponent* RpgASC = Cast<URpgAbilitySystemComponent>(
		FindOrAddComponentForActor(URpgAbilitySystemComponent::StaticClass(), Actor, AbilitiesEntry, ActiveData));
	if (!RpgASC)
	{
		UE_LOG(LogGameFeatures, Error, TEXT("Failed to find/add an RPG ability component to '%s'."), *Actor->GetPathName());
		return;
	}

	FActorExtensions AddedExtensions;
	AddedExtensions.Abilities.Reserve(AbilitiesEntry.GrantedAbilities.Num());
	AddedExtensions.Attributes.Reserve(AbilitiesEntry.GrantedAttributes.Num());
	AddedExtensions.AbilitySetHandles.Reserve(AbilitiesEntry.GrantedAbilitySets.Num());

	for (const FRpgGameFeatureAbilityGrant& Ability : AbilitiesEntry.GrantedAbilities)
	{
		if (!Ability.AbilityType.IsNull())
		{
			TSubclassOf<URpgGameplayAbility> AbilityClass = Ability.AbilityType.LoadSynchronous();
			FGameplayAbilitySpec NewAbilitySpec(AbilityClass);
			AddedExtensions.Abilities.Add(RpgASC->GiveAbility(NewAbilitySpec));
		}
	}

	for (const FRpgGameFeatureAttributeSetGrant& Attributes : AbilitiesEntry.GrantedAttributes)
	{
		if (!Attributes.AttributeSetType.IsNull())
		{
			TSubclassOf<UAttributeSet> SetType = Attributes.AttributeSetType.LoadSynchronous();
			if (SetType)
			{
				UAttributeSet* NewSet = NewObject<UAttributeSet>(RpgASC->GetOwner(), SetType);
				if (!Attributes.InitializationData.IsNull())
				{
					if (UDataTable* InitData = Attributes.InitializationData.LoadSynchronous())
					{
						NewSet->InitFromMetaDataTable(InitData);
					}
				}

				AddedExtensions.Attributes.Add(NewSet);
				RpgASC->AddAttributeSetSubobject(NewSet);
			}
		}
	}

	for (const TSoftObjectPtr<const URpgAbilitySet>& SetPtr : AbilitiesEntry.GrantedAbilitySets)
	{
		if (const URpgAbilitySet* Set = SetPtr.LoadSynchronous())
		{
			Set->GiveToAbilitySystem(RpgASC, &AddedExtensions.AbilitySetHandles.AddDefaulted_GetRef());
		}
	}

	ActiveData.ActiveExtensions.Add(Actor, AddedExtensions);

	// Direct GameFeature grants bypass URpgAbilitySet's normal notification seam.
	// Refresh owner-facing quick-access bindings after the complete grant batch so
	// saved ability IDs (for example Stoneburst) resolve immediately and uniquely.
	if (ARpgBasePlayerState* PlayerState = Cast<ARpgBasePlayerState>(Actor))
	{
		PlayerState->SendAbilitiesChangedEvent();
	}
}

void URpgGameFeatureAction_AddAbilities::RemoveActorAbilities(AActor* Actor, FPerContextData& ActiveData)
{
	if (FActorExtensions* ActorExtensions = ActiveData.ActiveExtensions.Find(Actor))
	{
		if (URpgAbilitySystemComponent* RpgASC = Actor->FindComponentByClass<URpgAbilitySystemComponent>())
		{
			for (UAttributeSet* AttribSetInstance : ActorExtensions->Attributes)
			{
				RpgASC->RemoveSpawnedAttribute(AttribSetInstance);
			}

			for (FGameplayAbilitySpecHandle AbilityHandle : ActorExtensions->Abilities)
			{
				RpgASC->SetRemoveAbilityOnEnd(AbilityHandle);
			}

			for (FRpgAbilitySet_GrantedHandles& SetHandle : ActorExtensions->AbilitySetHandles)
			{
				SetHandle.TakeFromAbilitySystem(RpgASC);
			}
		}

		ActiveData.ActiveExtensions.Remove(Actor);

		if (ARpgBasePlayerState* PlayerState = Cast<ARpgBasePlayerState>(Actor))
		{
			PlayerState->SendAbilitiesChangedEvent();
		}
	}
}

UActorComponent* URpgGameFeatureAction_AddAbilities::FindOrAddComponentForActor(UClass* ComponentType, AActor* Actor, const FRpgGameFeatureAbilitiesEntry& AbilitiesEntry, FPerContextData& ActiveData)
{
	UActorComponent* Component = Actor->FindComponentByClass(ComponentType);
	bool bMakeComponentRequest = (Component == nullptr);

	if (Component && Component->CreationMethod == EComponentCreationMethod::Native)
	{
		UObject* ComponentArchetype = Component->GetArchetype();
		bMakeComponentRequest = ComponentArchetype->HasAnyFlags(RF_ClassDefaultObject);
	}

	if (bMakeComponentRequest)
	{
		if (UWorld* World = Actor->GetWorld())
		{
			if (UGameInstance* GameInstance = World->GetGameInstance())
			{
				if (UGameFrameworkComponentManager* ComponentManager = UGameInstance::GetSubsystem<UGameFrameworkComponentManager>(GameInstance))
				{
					ActiveData.ComponentRequests.Add(ComponentManager->AddComponentRequest(AbilitiesEntry.ActorClass, ComponentType));
				}
			}
		}

		if (!Component)
		{
			Component = Actor->FindComponentByClass(ComponentType);
			ensureAlways(Component);
		}
	}

	return Component;
}

#undef LOCTEXT_NAMESPACE
