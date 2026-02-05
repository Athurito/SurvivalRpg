// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RpgCharacter.generated.h"

class URpgPawnGameplayComponent;
class URpgPawnExtensionComponent;
class URpgCharacterMovementComponent;

UCLASS()
class SURVIVALRPG_API ARpgCharacter : public ACharacter
{
	GENERATED_BODY()
 
public:
	// Sets default values for this character's properties
	explicit ARpgCharacter(const FObjectInitializer& ObjectInitializer);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_Controller() override;
	virtual void OnRep_PlayerState() override;
	

public:
	void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust);
	void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust);
	void ToggleCrouch();
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="CharacterMovement", Meta = (AllowPrivateAccess = "true"))
	URpgCharacterMovementComponent* MovementComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Character", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgPawnExtensionComponent> PawnExtensionComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Character", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgPawnGameplayComponent> PawnGameplayComponent;
};
