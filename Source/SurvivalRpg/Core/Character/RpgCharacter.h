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

/**
 * Compact server-owned acceleration state replicated to simulated proxies for locomotion animation.
 * XY direction and magnitude are quantized independently; Z preserves signed acceleration.
 */
USTRUCT()
struct SURVIVALRPG_API FRpgReplicatedAcceleration
{
	GENERATED_BODY()

	/** Quantizes a movement acceleration vector against the owning movement component's maximum. */
	void SetFromAcceleration(const FVector& InAcceleration, double MaxAcceleration);

	/** Reconstructs the acceleration vector for remote movement simulation. */
	FVector ToAcceleration(double MaxAcceleration) const;

	/** XY acceleration direction mapped from [0, 2 PI] to [0, 255]. */
	UPROPERTY()
	uint8 AccelXYRadians = 0;

	/** XY acceleration magnitude mapped from [0, MaxAcceleration] to [0, 255]. */
	UPROPERTY()
	uint8 AccelXYMagnitude = 0;

	/** Signed Z acceleration mapped from [-MaxAcceleration, MaxAcceleration] to [-127, 127]. */
	UPROPERTY()
	int8 AccelZ = 0;
};

UCLASS()
class SURVIVALRPG_API ARpgCharacter : public AModularCharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()
 
public:
	// Sets default values for this character's properties
	explicit ARpgCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;
	
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
	
	/** Begins death by disabling controller movement and capsule collision; mesh collision stays available for corpse physics. */
	UFUNCTION()
	virtual void OnDeathStarted(AActor* OwningActor);

	/** Ends death and detaches non-player pawns; specialized corpse components own their despawn lifetime. */
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
	/** Detaches the controller without disabling whole-actor collision needed by ragdoll meshes and corpse anchors. */
	void EnterDeadState();
	
private:
	/** Latest server acceleration, replicated only to simulated proxies and consumed by CharacterMovement. */
	UPROPERTY(Transient, ReplicatedUsing = OnRep_ReplicatedAcceleration)
	FRpgReplicatedAcceleration ReplicatedAcceleration;

	/** Reconstructs acceleration for remote CharacterMovement simulation after replication. */
	UFUNCTION()
	void OnRep_ReplicatedAcceleration();
	
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
