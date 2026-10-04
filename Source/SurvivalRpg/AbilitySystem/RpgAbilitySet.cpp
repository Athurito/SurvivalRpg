// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgAbilitySet.h"

#include "RpgAbilitySystemComponent.h"
#include "Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/Equipment/RpgWeaponAbilityLoadoutComponent.h"


void FRpgAbilitySet_GrantedHandles::AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle)
{
	if (Handle.IsValid())
	{
		AbilitySpecHandles.Add(Handle);
	}
}

void FRpgAbilitySet_GrantedHandles::AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle)
{
	if (Handle.IsValid())
	{
		GameplayEffectHandles.Add(Handle);
	}
}

void FRpgAbilitySet_GrantedHandles::TakeFromAbilitySystem(URpgAbilitySystemComponent* RpgASC)
{
	check(RpgASC);

	if (!RpgASC->HasGrantAuthority())
	{
		// Must be authoritative to give or take ability sets.
		return;
	}

	for (const FGameplayAbilitySpecHandle& Handle : AbilitySpecHandles)
	{
		if (Handle.IsValid())
		{
			RpgASC->ClearAbility(Handle);
		}
	}

	for (const FActiveGameplayEffectHandle& Handle : GameplayEffectHandles)
	{
		if (Handle.IsValid())
		{
			RpgASC->RemoveActiveGameplayEffect(Handle);
		}
	}
	
	AbilitySpecHandles.Reset();
	GameplayEffectHandles.Reset();
}


void URpgAbilitySet::GiveToAbilitySystem(
	URpgAbilitySystemComponent* RpgASC,
	FRpgAbilitySet_GrantedHandles* OutGrantedHandles,
	UObject* SourceObject) const
{
	check(RpgASC);

	if (!RpgASC->HasGrantAuthority())
	{
		// Must be authoritative to give or take ability sets.
		return;
	}

	// Grant the gameplay abilities.
	for (int32 AbilityIndex = 0; AbilityIndex < GrantedGameplayAbilities.Num(); ++AbilityIndex)
	{
		const FRpgAbilitySet_GameplayAbility& AbilityToGrant = GrantedGameplayAbilities[AbilityIndex];

		if (!IsValid(AbilityToGrant.Ability))
		{
			//UE_LOG(LogRpgAbilitySystem, Error, TEXT("GrantedGameplayAbilities[%d] on ability set [%s] is not valid."), AbilityIndex, *GetNameSafe(this));
			continue;
		}

		bool ShouldActivateAbility = false;
		URpgGameplayAbility* AbilityCDO = AbilityToGrant.Ability->GetDefaultObject<URpgGameplayAbility>();
		if (AbilityCDO)
		{
			ShouldActivateAbility = AbilityCDO->bAutoActivateWhenGranted;
		}

		FGameplayAbilitySpec AbilitySpec(AbilityCDO, AbilityToGrant.AbilityLevel);
		AbilitySpec.SourceObject = SourceObject;
		const int32 DefaultWeaponAbilitySlot = URpgWeaponAbilityLoadoutComponent::GetSlotIndexForInputTag(AbilityToGrant.InputTag);
		if (DefaultWeaponAbilitySlot != INDEX_NONE)
		{
			// Q/E/R input tags are bound at runtime by the weapon ability loadout; a static binding would be cleared
			// on its next refresh. The entry therefore only declares the slot's default occupant.
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(
				URpgWeaponAbilityLoadoutComponent::GetDefaultSelectionTagForSlotIndex(DefaultWeaponAbilitySlot));
		}
		else
		{
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(AbilityToGrant.InputTag);
		}
		AbilitySpec.GetDynamicSpecSourceTags().AddTag(AbilityToGrant.AbilityIdTag);

		const FGameplayAbilitySpecHandle AbilitySpecHandle = RpgASC->GiveAbility(AbilitySpec);

		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddAbilitySpecHandle(AbilitySpecHandle);
		}
		
		if (ShouldActivateAbility)
		{
			RpgASC->TryActivateAbility(AbilitySpecHandle);
		}
	}

	// Grant the gameplay effects.
	for (int32 EffectIndex = 0; EffectIndex < GrantedGameplayEffects.Num(); ++EffectIndex)
	{
		const FRpgAbilitySet_GameplayEffect& EffectToGrant = GrantedGameplayEffects[EffectIndex];

		if (!IsValid(EffectToGrant.GameplayEffect))
		{
			//UE_LOG(LogRpgAbilitySystem, Error, TEXT("GrantedGameplayEffects[%d] on ability set [%s] is not valid"), EffectIndex, *GetNameSafe(this));
			continue;
		}

		const UGameplayEffect* GameplayEffect = EffectToGrant.GameplayEffect->GetDefaultObject<UGameplayEffect>();
		const FActiveGameplayEffectHandle GameplayEffectHandle = RpgASC->ApplyGameplayEffectToSelf(GameplayEffect, EffectToGrant.EffectLevel, RpgASC->MakeEffectContext());

		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddGameplayEffectHandle(GameplayEffectHandle);
		}
	}
}

void URpgAbilitySet::AddGrantedGameplayAbility(TSubclassOf<URpgGameplayAbility> AbilityClass, int32 AbilityLevel, FGameplayTag InputTag, FGameplayTag AbilityIdTag)
{
	FRpgAbilitySet_GameplayAbility& NewAbility = GrantedGameplayAbilities.AddDefaulted_GetRef();
	NewAbility.Ability = AbilityClass;
	NewAbility.AbilityLevel = AbilityLevel;
	NewAbility.InputTag = InputTag;
	NewAbility.AbilityIdTag = AbilityIdTag;
}

void URpgAbilitySet::ClearGrantedGameplayAbilities()
{
	GrantedGameplayAbilities.Reset();
}

void URpgAbilitySet::AddGrantedGameplayAbilityByTagName(TSubclassOf<URpgGameplayAbility> AbilityClass, int32 AbilityLevel, FName InputTagName)
{
	const FGameplayTag InputTag = InputTagName.IsNone() ? FGameplayTag() : FGameplayTag::RequestGameplayTag(InputTagName);
	AddGrantedGameplayAbility(AbilityClass, AbilityLevel, InputTag);
}
