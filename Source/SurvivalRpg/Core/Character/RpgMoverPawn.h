// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "ModularPawn.h"

#include "RpgMoverPawn.generated.h"

class UAbilitySystemComponent;
class URpgAbilitySystemComponent;
class URpgEquipmentManagerComponent;
class URpgPawnExtensionComponent;
class URpgPawnGameplayComponent;

/**
 * Experience-composed pawn foundation for Blueprint-authored Mover characters.
 * PawnExtension connects the PlayerState-owned ASC; the Blueprint owns its collision, mesh,
 * Mover simulation/input producer and RPG camera. The existing equipment manager owns equipped state;
 * CharacterMovement, health, death and downed behavior remain separate integration work.
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
};
