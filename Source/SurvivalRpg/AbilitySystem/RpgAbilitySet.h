// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "RpgAbilitySet.generated.h"


class UGameplayEffect;
class URpgGameplayAbility;
class URpgAbilitySystemComponent;
/**
 * FRpgAbilitySet_GameplayAbility
 *
 *	Data used by the ability set to grant gameplay abilities.
 */
USTRUCT(BlueprintType)
struct FRpgAbilitySet_GameplayAbility
{
	GENERATED_BODY()

public:

	// Gameplay ability class granted by this set.
	UPROPERTY(EditAnywhere)
	TSubclassOf<URpgGameplayAbility> Ability = nullptr;

	// Ability level passed to GAS when the ability spec is created.
	UPROPERTY(EditAnywhere)
	int32 AbilityLevel = 1;

	// Optional input tag used by the input component to trigger this ability.
	UPROPERTY(EditAnywhere, Meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

/**
 * FRpgAbilitySet_GameplayEffect
 *
 *	Data used by the ability set to grant gameplay effects.
 */
USTRUCT(BlueprintType)
struct FRpgAbilitySet_GameplayEffect
{
	GENERATED_BODY()

public:

	// GameplayEffect applied while this ability set is granted.
	UPROPERTY(EditAnywhere)
	TSubclassOf<UGameplayEffect> GameplayEffect = nullptr;

	// Effect level used when applying the granted GameplayEffect.
	UPROPERTY(EditAnywhere)
	float EffectLevel = 1.0f;
};


/**
 * FLyraAbilitySet_GrantedHandles
 *
 *	Data used to store handles to what has been granted by the ability set.
 */
USTRUCT(BlueprintType)
struct FRpgAbilitySet_GrantedHandles
{
	GENERATED_BODY()

public:

	void AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle);
	void AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle);
	void TakeFromAbilitySystem(URpgAbilitySystemComponent* RpgAsc);

protected:

	// Handles to the granted abilities.
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;

	// Handles to the granted gameplay effects.
	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> GameplayEffectHandles;
};


/**
 * URpgAbilitySet
 *
 * Defines a collection of gameplay abilities, gameplay effects, and attribute sets
 * that can be granted to an ability system component.
 */
UCLASS(BlueprintType, Const)
class SURVIVALRPG_API URpgAbilitySet : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
	
public:
	// Grants the ability set to the specified ability system component.
	// The returned handles can be used later to take away anything that was granted.
	void GiveToAbilitySystem(
		URpgAbilitySystemComponent* RpgASC,
		FRpgAbilitySet_GrantedHandles* OutGrantedHandles,
		UObject* SourceObject = nullptr) const;
	void AddGrantedGameplayAbility(TSubclassOf<URpgGameplayAbility> AbilityClass, int32 AbilityLevel = 1, FGameplayTag InputTag = FGameplayTag());

	UFUNCTION(BlueprintCallable, Category = "Rpg|Ability Set")
	void ClearGrantedGameplayAbilities();

	UFUNCTION(BlueprintCallable, Category = "Rpg|Ability Set")
	void AddGrantedGameplayAbilityByTagName(TSubclassOf<URpgGameplayAbility> AbilityClass, int32 AbilityLevel, FName InputTagName);
	
protected:

	// Gameplay abilities granted by this set. Equipment uses this for attack, block, and utility abilities.
	UPROPERTY(EditAnywhere, Category = "Gameplay Abilities", meta=(TitleProperty=Ability))
	TArray<FRpgAbilitySet_GameplayAbility> GrantedGameplayAbilities;

	// Persistent GameplayEffects granted by this set, usually passive stats or stateful equipment effects.
	UPROPERTY(EditAnywhere, Category = "Gameplay Effects", meta=(TitleProperty=GameplayEffect))
	TArray<FRpgAbilitySet_GameplayEffect> GrantedGameplayEffects;
};
