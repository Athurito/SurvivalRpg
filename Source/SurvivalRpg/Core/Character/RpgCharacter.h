// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "ModularCharacter.h"
#include "GameFramework/Character.h"
#include "RpgCharacter.generated.h"

class URpgHealthComponent;
class ARpgPlayerController;
class ARpgPlayerState;
class UAbilitySystemComponent;
class URpgAbilitySystemComponent;
class URpgPawnGameplayComponent;
class URpgPawnExtensionComponent;
class URpgCharacterMovementComponent;

UCLASS()
class SURVIVALRPG_API ARpgCharacter : public AModularCharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()
 
public:
	// Sets default values for this character's properties
	ARpgCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	UFUNCTION(BlueprintCallable, Category = "Rpg|Character")
	ARpgPlayerController* GetRpgPlayerController() const;

	UFUNCTION(BlueprintCallable, Category = "Rpg|Character")
	ARpgPlayerState* GetRpgPlayerState() const;

	UFUNCTION(BlueprintCallable, Category = "Rpg|Character")
	URpgAbilitySystemComponent* GetRpgAbilitySystemComponent() const;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	
	virtual void OnAbilitySystemInitialized();
	virtual void OnAbilitySystemUninitialized();

	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

	virtual void OnRep_Controller() override;
	virtual void OnRep_PlayerState() override;
	
	virtual void FellOutOfWorld(const class UDamageType& dmgType) override;
	
	// Begins the death sequence for the character (disables collision, disables movement, etc...)
	UFUNCTION()
	virtual void OnDeathStarted(AActor* OwningActor);

	// Ends the death sequence for the character (detaches controller, destroys pawn, etc...)
	UFUNCTION()
	virtual void OnDeathFinished(AActor* OwningActor);

	
	
	
	void DisableMovementAndCollision();
	void DestroyDueToDeath();
	void UninitAndDestroy();
	
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="CharacterMovement", Meta = (AllowPrivateAccess = "true"))
	URpgCharacterMovementComponent* MovementComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Character", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgPawnExtensionComponent> PawnExtensionComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Character", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgHealthComponent> HealthComponent;
};
