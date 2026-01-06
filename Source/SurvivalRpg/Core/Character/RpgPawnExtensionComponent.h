// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/PawnComponent.h"
#include "RpgPawnExtensionComponent.generated.h"


class URpgPawnData;
class URpgAbilitySystemComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgPawnExtensionComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	URpgPawnExtensionComponent(const FObjectInitializer& ObjectInitializer);
	
	/** Returns the pawn extension component if one exists on the specified actor. */
	UFUNCTION(BlueprintPure, Category = "Lyra|Pawn")
	static URpgPawnExtensionComponent* FindPawnExtensionComponent(const AActor* Actor) { return (Actor ? Actor->FindComponentByClass<URpgPawnExtensionComponent>() : nullptr); }
	
	/** Gets the current ability system component, which may be owned by a different actor */
	UFUNCTION(BlueprintPure, Category = "Lyra|Pawn")
	URpgAbilitySystemComponent* GetRpgAbilitySystemComponent() const { return AbilitySystemComponent; }
	
	void TryInitialize();
	void UnInitialize();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
	/** Pointer to the ability system component that is cached for convenience. */
	UPROPERTY(Transient)
	TObjectPtr<URpgAbilitySystemComponent> AbilitySystemComponent;

private:
	
	bool bInitialized = false;
	
};
