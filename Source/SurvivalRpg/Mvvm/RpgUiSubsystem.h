// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "RpgUiSubsystem.generated.h"

class URpgPawnExtensionComponent;
class UAbilitySystemComponent;
class UPlayerVitalsViewmodel;
/**
 * 
 */
UCLASS()
class SURVIVALRPG_API URpgUiSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
	
	
// public:
// 	UPlayerVitalsViewmodel* GetVitalsViewmodel();
// 	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
//
// private:
// 	UPROPERTY(Transient)
// 	TObjectPtr<UPlayerVitalsViewmodel> VitalsVM;
// 	
// 	
// 	UPROPERTY()
// 	UAbilitySystemComponent* CachedASC;
// 	
//
// 	UFUNCTION()
// 	void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);
// 	void HandleAscReady();
// 	void BindToControllerSafe();
// 	
// 	UPROPERTY()
// 	URpgPawnExtensionComponent* BoundExt = nullptr;
//
// 	FDelegateHandle H_AscInit;
};
