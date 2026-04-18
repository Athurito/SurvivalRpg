// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "ModularCharacter.h"
#include "GameFramework/Character.h"
#include "RpgDownedComponent.h"
#include "RpgCharacter.generated.h"

class URpgCameraComponent;
class URpgDeathComponent;
class URpgHealthComponent;
class URpgRespawnComponent;
class ARpgPlayerController;
class ARpgPlayerState;
class UAbilitySystemComponent;
class URpgAbilitySystemComponent;
class URpgPawnGameplayComponent;
class URpgPawnExtensionComponent;
class URpgCharacterMovementComponent;
class URpgEquipmentManagerComponent;

UCLASS()
class SURVIVALRPG_API ARpgCharacter : public AModularCharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()
 
public:
	// Sets default values for this character's properties
	explicit ARpgCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void BeginPlay() override;
	
	UFUNCTION(BlueprintCallable, Category = "Rpg|Character")
	ARpgPlayerController* GetRpgPlayerController() const;

	UFUNCTION(BlueprintCallable, Category = "Rpg|Character")
	ARpgPlayerState* GetRpgPlayerState() const;

	UFUNCTION(BlueprintCallable, Category = "Rpg|Character")
	URpgAbilitySystemComponent* GetRpgAbilitySystemComponent() const;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintCallable, Category = "Rpg|Character")
	void ToggleCrouch();

	UFUNCTION(BlueprintCallable, Category = "Rpg|Equipment")
	URpgEquipmentManagerComponent* GetEquipmentManagerComponent() const { return EquipmentManagerComponent; }
	
	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

	virtual void OnRep_Controller() override;
	virtual void OnRep_PlayerState() override;
	
	virtual void FellOutOfWorld(const class UDamageType& dmgType) override;

protected:
	// Called when the game starts or when spawned
	
	virtual void OnAbilitySystemInitialized();
	virtual void OnAbilitySystemUninitialized();
	
	// Begins the death sequence for the character (disables collision, disables movement, etc...)
	UFUNCTION()
	virtual void OnDeathStarted(AActor* OwningActor);

	// Ends the death sequence for the character (detaches controller, destroys pawn, etc...)
	UFUNCTION()
	virtual void OnDeathFinished(AActor* OwningActor);

	UFUNCTION()
	virtual void OnDownedStateChanged(ERpgDownedState NewState);

	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual bool CanJumpInternal_Implementation() const override;
	
	void DisableMovementAndCollision() const;
	void DisableMovementForDowned() const;
	void RestoreMovementAndCollision() const;
	void EnterDeadState();
	
private:
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Character", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgPawnExtensionComponent> PawnExtensionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Equipment", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgEquipmentManagerComponent> EquipmentManagerComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Character", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgHealthComponent> HealthComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Character", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgDeathComponent> DeathComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Character", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgDownedComponent> DownedComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Character", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgCameraComponent> CameraComponent;
};
