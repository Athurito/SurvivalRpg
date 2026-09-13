// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "ModularPawn.h"

#include "RpgMoverPawn.generated.h"

class UAbilitySystemComponent;
class URpgAbilitySystemComponent;
class URpgDeathComponent;
class URpgEquipmentManagerComponent;
class URpgHealthComponent;
class URpgPawnExtensionComponent;
class URpgPawnGameplayComponent;

/**
 * Experience-composed pawn foundation for Blueprint-authored Mover characters.
 * PawnExtension connects the PlayerState-owned ASC; the Blueprint owns its collision, mesh,
 * Mover simulation/input producer and RPG camera. Existing RPG components own equipment and health;
 * player death returns to the GameMode's respawn flow. Downed and ragdoll remain separate integrations.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API ARpgMoverPawn : public AModularPawn, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ARpgMoverPawn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Returns the externally owned ASC after PawnExtension has bound this pawn as its avatar. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Mover")
	URpgAbilitySystemComponent* GetRpgAbilitySystemComponent() const;

	/** Exposes the same PlayerState-owned ASC to engine gameplay-ability callers. */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** Advances Experience initialization when local player input becomes available. */
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** Refreshes the ASC actor information and pawn initialization after authoritative possession. */
	virtual void PossessedBy(AController* NewController) override;

	/** Refreshes the external ASC binding when this pawn loses its controller. */
	virtual void UnPossessed() override;

	/** Rechecks initialization when the controller reference arrives on a client. */
	virtual void OnRep_Controller() override;

	/** Rechecks player/ASC initialization for initial replication and late joiners. */
	virtual void OnRep_PlayerState() override;

	/** Routes a world-boundary death through the authoritative RPG damage and respawn lifecycle. */
	virtual void FellOutOfWorld(const UDamageType& DamageType) override;

protected:
	/** Binds pawn-local health/death observers to the external ASC and restores health for a new server avatar. */
	virtual void OnAbilitySystemInitialized();

	/** Removes this pawn's health/death observers before the external ASC moves to another avatar. */
	virtual void OnAbilitySystemUninitialized();

	/** Idempotently stops Mover and root collision for authoritative death and replicated presentation. */
	UFUNCTION()
	virtual void OnDeathStarted(AActor* OwningActor);

	/** Only the server hands a player's completed death to the existing inventory-drop/respawn flow. */
	UFUNCTION()
	virtual void OnDeathFinished(AActor* OwningActor);

private:
	/** Owns replicated PawnData and ASC avatar binding; it never owns or grants a second ASC. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Pawn", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgPawnExtensionComponent> PawnExtensionComponent;

	/**
	 * Reuses player initialization, Experience input contexts and PawnData camera selection.
	 * Mover PawnData must omit native movement-action bindings handled by its Blueprint input producer.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Pawn", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgPawnGameplayComponent> PawnGameplayComponent;

	/** Reuses the server-owned equipment state, grants and replicated actors on this pawn's gameplay mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgEquipmentManagerComponent> EquipmentManagerComponent;

	/** Mirrors external GAS health attributes and replicates this avatar's monotonic death state. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Health", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgHealthComponent> HealthComponent;

	/** Reuses server-owned out-of-health handling and the Experience's existing death ability. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Health", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgDeathComponent> DeathComponent;
};
