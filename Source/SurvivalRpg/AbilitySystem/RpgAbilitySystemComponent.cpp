// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgAbilitySystemComponent.h"

#include "SurvivalRpg/Core/Player/RpgPlayerState.h"

URpgAbilitySystemComponent::URpgAbilitySystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

bool URpgAbilitySystemComponent::GrantAbilitySet(const URpgAbilitySet* AbilitySet, UObject* SourceObject)
{
	if (!IsValid(AbilitySet))
	{
		return false;
	}

	// Wenn Client: an Server delegieren
	if (!IsOwnerActorAuthoritative())
	{
		Server_GrantAbilitySet(AbilitySet, SourceObject);
		return true; // Anfrage raus, Server repliziert Ergebnis
	}
	OwnerPlayerState->SendAbilitiesChangedEvent();
	return GrantAbilitySet_Internal(AbilitySet, SourceObject);
}

bool URpgAbilitySystemComponent::RemoveAbilitySet(const URpgAbilitySet* AbilitySet)
{
	if (!IsValid(AbilitySet))
	{
		return false;
	}

	if (!IsOwnerActorAuthoritative())
	{
		Server_RemoveAbilitySet(AbilitySet);
		return true;
	}
	OwnerPlayerState->SendAbilitiesChangedEvent();
	return RemoveAbilitySet_Internal(AbilitySet);
}

bool URpgAbilitySystemComponent::HasAbilitySet(const URpgAbilitySet* AbilitySet) const
{
	return IsValid(AbilitySet) && GrantedAbilitySets.Contains(AbilitySet);
}

void URpgAbilitySystemComponent::Server_GrantAbilitySet_Implementation(const URpgAbilitySet* AbilitySet, UObject* SourceObject)
{
	GrantAbilitySet_Internal(AbilitySet, SourceObject);
}

void URpgAbilitySystemComponent::Server_RemoveAbilitySet_Implementation(const URpgAbilitySet* AbilitySet)
{
	RemoveAbilitySet_Internal(AbilitySet);
}

bool URpgAbilitySystemComponent::GrantAbilitySet_Internal(const URpgAbilitySet* AbilitySet, UObject* SourceObject)
{
	if (!IsValid(AbilitySet))
	{
		return false;
	}

	// Optional: doppelt grant verhindern
	if (GrantedAbilitySets.Contains(AbilitySet))
	{
		return false;
	}

	FRpgAbilitySet_GrantedHandles Handles;
	AbilitySet->GiveToAbilitySystem(this, &Handles, SourceObject);

	GrantedAbilitySets.Add(AbilitySet, Handles);
	return true;
}

bool URpgAbilitySystemComponent::RemoveAbilitySet_Internal(const URpgAbilitySet* AbilitySet)
{
	if (!IsValid(AbilitySet))
	{
		return false;
	}

	FRpgAbilitySet_GrantedHandles* Handles = GrantedAbilitySets.Find(AbilitySet);
	if (!Handles)
	{
		return false;
	}

	Handles->TakeFromAbilitySystem(this);
	GrantedAbilitySets.Remove(AbilitySet);
	return true;
}

void URpgAbilitySystemComponent::ApplyDefaultAbilitySetupIfNeeded(UObject* SourceObject)
{
	if (bDefaultSetupApplied)
	{
		return;
	}

	if (!IsOwnerActorAuthoritative())
	{
		return; // Abilities/Effects nur serverseitig geben
	}

	// Safety: ActorInfo muss gültig sein
	if (!AbilityActorInfo.IsValid() || AbilityActorInfo->AvatarActor.Get() == nullptr)
	{
		return;
	}

	if (DefaultAbilitySetup)
	{
		DefaultAbilitySetup->GiveToAbilitySystem(this, &DefaultGrantedHandles, SourceObject);
	}

	bDefaultSetupApplied = true;
}

void URpgAbilitySystemComponent::RemoveDefaultAbilitySetup()
{
	if (!IsOwnerActorAuthoritative())
	{
		return;
	}

	if (bDefaultSetupApplied)
	{
		DefaultGrantedHandles.TakeFromAbilitySystem(this);
		bDefaultSetupApplied = false;
	}
}

void URpgAbilitySystemComponent::ActivateAbilitiesByInputTag(FGameplayTag InputTag, bool bAllowRemoteActivation)
{
	FGameplayTagContainer TagContainer(InputTag);

	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			TryActivateAbility(Spec.Handle, bAllowRemoteActivation);
		}
	}
}

void URpgAbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();
	
	OwnerPlayerState = Cast<ARpgPlayerState>(GetOwner());
}

void URpgAbilitySystemComponent::OnRep_ActivateAbilities()
{
	Super::OnRep_ActivateAbilities();
	
	if (!OwnerPlayerState) return;
	
	bool bAbilitiesChanged = false;
	
	if (LastActiveAbilities.Num() != ActivatableAbilities.Items.Num())
	{
		bAbilitiesChanged = true;
	}
	else
	{
		for (int32 i = 0; i < LastActiveAbilities.Num(); ++i)
		{
			if (LastActiveAbilities[i].Ability != ActivatableAbilities.Items[i].Ability)
			{
				bAbilitiesChanged = true;
				break;
			}
		}
	}
	
	if (bAbilitiesChanged)
	{
		OwnerPlayerState->SendAbilitiesChangedEvent();
		LastActiveAbilities = ActivatableAbilities.Items;
	}
}

