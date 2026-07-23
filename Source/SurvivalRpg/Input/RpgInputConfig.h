// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "RpgInputConfig.generated.h"


class UInputAction;

/** Associates one Enhanced Input action with the semantic input tag consumed by gameplay code. */
USTRUCT(BlueprintType)
struct FRpgInputAction
{
	GENERATED_BODY()

public:

	/** Enhanced Input action asset bound for this mapping. Static designer-authored data. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<const UInputAction> InputAction = nullptr;

	/** Semantic input tag routed by the owning input component. Must be a strict descendant of InputTag. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

/**
 * Designer-authored mapping between Enhanced Input actions and semantic gameplay tags.
 *
 * Native mappings are bound explicitly by gameplay code, while ability mappings are
 * forwarded to abilities carrying the same input tag.
 */
UCLASS(BlueprintType, Const)
class SURVIVALRPG_API URpgInputConfig : public UDataAsset
{
	GENERATED_BODY()
	explicit URpgInputConfig(const FObjectInitializer& ObjectInitializer);
	
public:
	/** Finds the native action mapped to InputTag, or null when the config contains no such mapping. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Pawn")
	const UInputAction* FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = true) const;

	/** Finds the ability action mapped to InputTag, or null when the config contains no such mapping. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Pawn")
	const UInputAction* FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = true) const;

	/** Removes runtime-added ability mappings from this config instance. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Input")
	void ClearAbilityInputActions();

	/** Adds an ability mapping resolved from InputTagName; intended for controlled runtime composition. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Input")
	void AddAbilityInputActionByTagName(const UInputAction* InputAction, FName InputTagName);

#if WITH_EDITOR
	/**
	 * Rejects unusable entries and ambiguous duplicate tags while allowing one
	 * InputAction asset to serve multiple distinct semantic tags.
	 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
	
public:
	
	/** Designer-authored actions that the owning gameplay code binds explicitly by semantic input tag. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Meta = (TitleProperty = "InputAction"))
	TArray<FRpgInputAction> NativeInputActions;

	/**
	 * Actions forwarded to abilities carrying the matching semantic tag.
	 * DataAssets provide the authored defaults; controlled runtime composition may mutate a transient config instance.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Meta = (TitleProperty = "InputAction"))
	TArray<FRpgInputAction> AbilityInputActions;
};
