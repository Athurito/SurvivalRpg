#pragma once

#include "GameFramework/Actor.h"

#include "RpgPersonalStorageLockerActor.generated.h"

class APlayerController;
class ARpgBaseCampActor;
class URpgInventoryManagerComponent;
class USceneComponent;

/**
 * Owner-relevant physical inventory for one player at one base.
 *
 * The server retains the actor and its concrete item graph across disconnects. Only the owning connection receives
 * the actor, inventory FastArray, or item subobjects; shared base crafting must never treat this inventory as a source.
 */
UCLASS(NotBlueprintable)
class SURVIVALRPG_API ARpgPersonalStorageLockerActor : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgPersonalStorageLockerActor(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Initializes stable locker identity and assigns the only client allowed to receive its inventory. */
	void InitializeLocker(
		ARpgBaseCampActor* InBaseCamp,
		APlayerController* OwningController,
		const FString& InProfileKey);

	/** Reassigns owner relevancy after the same stable profile reconnects. */
	void ReassignOwningController(APlayerController* OwningController);

	/** Concrete owner-only item graph. UI may observe it; all mutation still routes through inventory transactions. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Storage|Personal")
	URpgInventoryManagerComponent* GetInventoryManager() const { return InventoryManager; }

	/** Stable base id represented by this private locker. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Base Storage|Personal")
	FName GetBaseId() const { return BaseId; }

	/** Server-local base actor that owns this locker lifecycle; clients should use the replicated BaseId for presentation. */
	ARpgBaseCampActor* GetBaseCamp() const { return OwningBaseCamp; }

	/** Stable host profile key. Exposed only on this owner-relevant actor. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Base Storage|Personal")
	FString GetProfileKey() const { return ProfileKey; }

private:
	/** Non-moving root used for ownership/relevancy only; presentation belongs to the base terminal. */
	UPROPERTY(VisibleAnywhere, Category = "Base Storage|Personal")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Owner-only inventory limited to twenty entries (the V1 4x5 personal domain budget). */
	UPROPERTY(VisibleAnywhere, Category = "Base Storage|Personal")
	TObjectPtr<URpgInventoryManagerComponent> InventoryManager;

	/** Non-replicated authoritative lifecycle owner; BaseId remains the stable save and client identity. */
	UPROPERTY(Transient)
	TObjectPtr<ARpgBaseCampActor> OwningBaseCamp;

	/** Stable base identity persisted by the owning base save envelope. */
	UPROPERTY(Replicated)
	FName BaseId = NAME_None;

	/** Stable host profile identity used to reconnect the locker after a player rejoins. */
	UPROPERTY(Replicated)
	FString ProfileKey;
};
