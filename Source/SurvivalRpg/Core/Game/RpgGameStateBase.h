// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "ModularGameState.h"
#include "RpgGameStateBase.generated.h"

class UAbilitySystemComponent;
class URpgAbilitySystemComponent;
class URpgExperienceManagerComponent;
class URpgRecipeUnlockComponent;

/**
 * GameState used by the runtime gameplay map.
 *
 * It owns the replicated experience manager and a game-wide ASC for global gameplay effects/cues.
 */
UCLASS()
class SURVIVALRPG_API ARpgGameStateBase : public AModularGameStateBase, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	explicit ARpgGameStateBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ AActor interface
	virtual void PostInitializeComponents() override;
	//~ End AActor interface

	//~ IAbilitySystemInterface interface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~ End IAbilitySystemInterface interface

	/** Gets the game-wide ability system component. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|GameState")
	URpgAbilitySystemComponent* GetRpgAbilitySystemComponent() const { return AbilitySystemComponent; }

	/** Gets the session-wide recipe unlock store shared by every player. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|GameState")
	URpgRecipeUnlockComponent* GetRecipeUnlockComponent() const { return RecipeUnlockComponent; }

private:
	/** Handles loading and managing the current gameplay experience. */
	UPROPERTY()
	TObjectPtr<URpgExperienceManagerComponent> ExperienceManagerComponent;

	/** Ability system component for game-wide gameplay effects and cues. */
	UPROPERTY(VisibleAnywhere, Category = "Rpg|GameState")
	TObjectPtr<URpgAbilitySystemComponent> AbilitySystemComponent;

	/** Replicated session-global recipe unlocks used by crafting stations and crafting UI. */
	UPROPERTY(VisibleAnywhere, Category = "Rpg|GameState")
	TObjectPtr<URpgRecipeUnlockComponent> RecipeUnlockComponent;
};
