// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "RpgUiSubsystem.generated.h"

class APlayerController;
class APawn;
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

public:
	UPlayerVitalsViewmodel* GetVitalsViewmodel() const { return VitalsVM; }

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

private:
	UFUNCTION()
	void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);

	void BindToPlayerController(APlayerController* NewPlayerController);
	void UnbindFromPlayerController();
	void BindToPawn(APawn* NewPawn);
	void UnbindFromPawnExtension(bool bResetViewModel = true);
	void HandleAbilitySystemInitialized();
	void HandleAbilitySystemUninitialized();

	UPROPERTY(Transient)
	TObjectPtr<UPlayerVitalsViewmodel> VitalsVM;

	UPROPERTY(Transient)
	TObjectPtr<APlayerController> BoundPlayerController;

	UPROPERTY(Transient)
	TObjectPtr<URpgPawnExtensionComponent> BoundPawnExtension;
};
