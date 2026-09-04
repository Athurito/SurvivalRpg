// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "ModularCharacter.h"
#include "GameFramework/Character.h"
#include "RpgDownedComponent.h"
#include "RpgCharacterMovementProfile.h"
#include "RpgCharacterRotationMode.h"
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
struct FGameplayTag;

UCLASS()
class SURVIVALRPG_API ARpgCharacter : public AModularCharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()
 
public:
	// Sets default values for this character's properties
	explicit ARpgCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION(BlueprintCallable, Category = "Rpg|Character")
	ARpgPlayerController* GetRpgPlayerController() const;

	UFUNCTION(BlueprintCallable, Category = "Rpg|Character")
	ARpgPlayerState* GetRpgPlayerState() const;

	UFUNCTION(BlueprintCallable, Category = "Rpg|Character")
	URpgAbilitySystemComponent* GetRpgAbilitySystemComponent() const;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintCallable, Category = "Rpg|Character")
	void ToggleCrouch();

	/**
	 * Returns the rotation mode used for local presentation.
	 * Autonomous proxies may resolve predicted activation-owned tags; simulated proxies return the replicated server value.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rpg|Character|Rotation")
	ERpgCharacterRotationMode GetRotationMode() const;

	/** Pure Aim > CombatStrafe > Free priority resolver shared by runtime code and automation tests. */
	static ERpgCharacterRotationMode ResolveRotationMode(
		bool bAimRequested,
		bool bCombatStrafeRequested,
		ERpgCharacterRotationMode DefaultMode);

	/** Returns the movement-component policy for the supplied rotation mode without mutating an actor. */
	static FRpgCharacterRotationPolicy GetRotationPolicy(
		ERpgCharacterRotationMode InRotationMode,
		float FreeRotationRateYaw = -1.0f);

	/**
	 * Pure server-request gate used by runtime validation and automation tests.
	 * Disable requests are always allowed; enable requires a weapon, an unblocked grounded stance, and no montage.
	 */
	static bool CanApplyExplicitCombatStanceRequest(
		bool bEnable,
		bool bHasWeapon,
		bool bHasBlockingState,
		bool bIsMovingOnGround,
		bool bIsCrouched,
		bool bWantsToCrouch,
		bool bIsAnyMontagePlaying);

	/** Toggles the explicit combat-facing stance through the server-authoritative character request seam. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Character|Rotation")
	void ToggleCombatStance();

	/** Completes a Free-mode handoff only after CMC receives a newer timestamped authority correction. */
	void NotifyOwnerRotationCorrectionReceived(float ClientMoveTimeStamp);

	UFUNCTION(BlueprintCallable, Category = "Rpg|Equipment")
	URpgEquipmentManagerComponent* GetEquipmentManagerComponent() const { return EquipmentManagerComponent; }
	
	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

	virtual void OnRep_Controller() override;
	virtual void OnRep_PlayerState() override;
	
	virtual void FellOutOfWorld(const class UDamageType& dmgType) override;
	virtual void TeleportSucceeded(bool bIsATest) override;

protected:
	/**
	 * Includes acceleration in UE 5.8's replicated movement payload for simulated proxies.
	 * The engine reconstructs both acceleration and analog input magnitude from this single snapshot.
	 */
	virtual bool ShouldReplicateAcceleration() const override;

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
	/** Reconciles the authoritative or locally predicted tag request into a presentation policy. */
	void RefreshRotationMode();

	/** Resolves request tags against the current PawnData fallback without mutating state. */
	ERpgCharacterRotationMode ResolveRequestedRotationMode() const;

	/** Returns the PawnData fallback, preserving CombatStrafe for legacy pawns without data. */
	ERpgCharacterRotationMode GetDefaultRotationMode() const;

	/** Applies one resolved controller/movement rotation contract. */
	void ApplyRotationPolicy(ERpgCharacterRotationMode InRotationMode);

	/** Invalidates older owner handoffs whenever authority changes rotation policy or possession. */
	void AdvanceRotationModeRevision();

	/** Requests a timestamped CMC correction after this owner has actually applied the current Free policy. */
	void RequestFreeRotationSynchronization(ERpgCharacterRotationMode AppliedMode);

	/** Binds request-tag changes on the ASC currently using this pawn as its avatar. */
	void BindRotationModeAbilitySystem(URpgAbilitySystemComponent* AbilitySystemComponent);

	/** Removes request-tag delegates from the previous avatar ASC. */
	void UnbindRotationModeAbilitySystem();

	void HandleRotationRequestTagChanged(const FGameplayTag Tag, int32 NewCount);
	/** Applies locally available PawnData movement tuning before input or animation consumes it. */
	void HandlePawnDataReady();
	void HandleEquipmentChanged();
	bool CanEnterExplicitCombatStance() const;
	void SetExplicitCombatStance(bool bEnabled);

	/** Reliable owning-client request; enable is validated against weapon equipment and blocking state on the server. */
	UFUNCTION(Server, Reliable)
	void ServerToggleCombatStance();

	/** Server-owned rotation truth replicated to owners and simulated proxies for reconciliation and late join. */
	UPROPERTY(Transient, ReplicatedUsing = OnRep_RotationMode)
	ERpgCharacterRotationMode RotationMode = ERpgCharacterRotationMode::CombatStrafe;

	/** Owner-only authority revision; zero is uninitialized and stale handoff acknowledgements are ignored. */
	UPROPERTY(Transient, ReplicatedUsing = OnRep_RotationMode)
	uint16 RotationModeRevision = 0;

	/** Local owner revision already requested; retained after confirmation to avoid repeated idle RPCs. */
	uint16 RequestedFreeRotationRevision = 0;

	/** Owner acknowledges the applied policy and last pre-handoff SavedMove; does not author rotation. */
	UFUNCTION(Server, Reliable)
	void ServerAcknowledgeFreeRotationMode(uint16 Revision, float ClientMoveTimeStamp);

	/** Stops retransmitting handoff corrections only after the owner receives one from a newer move. */
	UFUNCTION(Server, Reliable)
	void ServerConfirmFreeRotationSynchronization(uint16 Revision, float ClientMoveTimeStamp);

	/** Applies newly replicated server truth; autonomous proxies may still overlay predicted ability tags cosmetically. */
	UFUNCTION()
	void OnRep_RotationMode();

	/**
	 * Server-owned semantic teleport epoch sent only to simulated proxies.
	 * Ordinary replicated movement corrections never mutate it.
	 */
	UPROPERTY(Transient, ReplicatedUsing = OnRep_AnimationTeleportEpoch)
	uint16 AnimationTeleportEpoch = 0;

	/** Converts a replicated gameplay teleport into one local presentation-history edge. */
	UFUNCTION()
	void OnRep_AnimationTeleportEpoch();

	/**
	 * Current authority-owned standing Walk/Run classification for simulated proxies, with or without input.
	 * Idle clears at physical stop or when leaving standing ground movement; no pose/history is replicated.
	 */
	UPROPERTY(Transient, ReplicatedUsing = OnRep_GroundMovementGait)
	ERpgLocomotionGait GroundMovementGait = ERpgLocomotionGait::Idle;

	/** Applies current server gait to the simulated-proxy CharacterMovement resolver. */
	UFUNCTION()
	void OnRep_GroundMovementGait();

	/** Publishes an authority gait transition so new/existing simulated proxies reconstruct the same state. */
	void SetAuthoritativeGroundMovementGait(ERpgLocomotionGait NewCoastGait);

	/** ASC whose rotation request-tag delegates are currently registered. */
	TWeakObjectPtr<URpgAbilitySystemComponent> RotationModeAbilitySystem;

	/** Server-only ownership bit for the single additive explicit-stance loose-tag count. */
	bool bExplicitCombatStanceRequested = false;

	/** Last policy applied to CharacterMovement, used to detect Free-to-controller-facing transitions. */
	ERpgCharacterRotationMode LastAppliedRotationMode = ERpgCharacterRotationMode::CombatStrafe;
	bool bHasAppliedRotationPolicy = false;

	friend class URpgCharacterMovementComponent;

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
