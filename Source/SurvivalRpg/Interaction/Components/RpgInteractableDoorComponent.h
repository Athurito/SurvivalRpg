// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "SurvivalRpg/Interaction/IInteractableTarget.h"

#include "RpgInteractableDoorComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRpgInteractableDoorStateChanged, bool, bIsOpen, bool, bIsLocked);

/**
 * Reusable server-authoritative door interaction state.
 *
 * The owning actor supplies mesh/collision/animation presentation. This component owns only
 * replicated gameplay state and the Open, Close, and optional Unlock interaction contract.
 */
UCLASS(Blueprintable, ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgInteractableDoorComponent : public UActorComponent, public IInteractableTarget
{
	GENERATED_BODY()

public:
	explicit URpgInteractableDoorComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& InteractionBuilder) override;
	virtual bool CommitInteraction(const FInteractionQuery& AuthoritativeQuery, const FInteractionOption& ValidatedOption) override;

	/** Returns the replicated authoritative open state. UI and animation should read, never own, this value. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Interaction|Door")
	bool IsDoorOpen() const { return bIsOpen; }

	/** Returns the replicated authoritative lock state. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Interaction|Door")
	bool IsDoorLocked() const { return bIsLocked; }

	/** Sets the authoritative open state. Opening fails while locked; closing is always allowed. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rpg|Interaction|Door")
	bool SetDoorOpen(bool bNewOpen);

	/** Sets the authoritative lock state. Locking also closes an open door in the same replicated revision. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rpg|Interaction|Door")
	bool SetDoorLocked(bool bNewLocked);

	/**
	 * Cosmetic/client estimate of whether the requester can unlock this door.
	 * The server calls the same hook again before committing and must remain authoritative.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Rpg|Interaction|Door")
	bool CanRequesterUnlock(AActor* RequestingActor) const;
	virtual bool CanRequesterUnlock_Implementation(AActor* RequestingActor) const;

	/** Consumes a key, quest requirement, or other server-owned unlock cost. Default denies the request. */
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly, Category = "Rpg|Interaction|Door")
	bool CommitUnlockRequirement(AActor* RequestingActor);
	virtual bool CommitUnlockRequirement_Implementation(AActor* RequestingActor);

	/** Fired on authority mutations and replicated client updates for animation/audio presentation. */
	UPROPERTY(BlueprintAssignable, Category = "Rpg|Interaction|Door")
	FRpgInteractableDoorStateChanged OnDoorStateChanged;

protected:
	UFUNCTION()
	void OnRep_DoorState();

	/** Blueprint presentation hook. It must not mutate authoritative door state. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rpg|Interaction|Door", meta = (DisplayName = "On Door State Changed"))
	void K2_OnDoorStateChanged(bool bNewOpen, bool bNewLocked);

	/** Static prompt configuration for opening an unlocked closed door. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Interaction|Door")
	FInteractionOption OpenInteractionOption;

	/** Static prompt configuration for closing an open door. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Interaction|Door")
	FInteractionOption CloseInteractionOption;

	/** Static prompt configuration used when interaction-based unlocking is enabled. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Interaction|Door")
	FInteractionOption UnlockInteractionOption;

	/** Allows the door prompt to invoke the target-defined unlock hooks. Disabled by default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Interaction|Door")
	bool bAllowInteractionUnlock = false;

	/** Initial and runtime open state. Runtime mutation is authority-only and replicated for late join. */
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_DoorState, BlueprintReadOnly, Category = "Rpg|Interaction|Door")
	bool bIsOpen = false;

	/** Initial and runtime lock state. Runtime mutation is authority-only and replicated for late join. */
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_DoorState, BlueprintReadOnly, Category = "Rpg|Interaction|Door")
	bool bIsLocked = false;

	/** Monotonic replicated state version copied into interaction requests to reject stale actions. */
	UPROPERTY(ReplicatedUsing = OnRep_DoorState, BlueprintReadOnly, Category = "Rpg|Interaction|Door")
	int32 DoorStateRevision = 0;

private:
	void NotifyAuthoritativeStateChanged();
	void NotifyPresentationStateChanged();

	bool bHasPresentedState = false;
	bool bLastPresentedOpen = false;
	bool bLastPresentedLocked = false;
	int32 LastPresentedRevision = INDEX_NONE;
};
