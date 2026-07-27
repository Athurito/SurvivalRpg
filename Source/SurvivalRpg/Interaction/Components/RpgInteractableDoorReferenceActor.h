// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"

#include "RpgInteractableDoorReferenceActor.generated.h"

class UBoxComponent;
class URpgInteractableDoorComponent;
class USceneComponent;
class UStaticMeshComponent;

/**
 * Reference composition for a reusable replicated interaction door.
 * Designers may subclass this actor to replace the meshes and presentation while retaining the door contract.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API ARpgInteractableDoorReferenceActor : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgInteractableDoorReferenceActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void BeginPlay() override;

	/** Non-moving frame/root used as the authored placement origin. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Interaction|Door")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Movable panel whose default presentation rotates between closed and open states. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Interaction|Door")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	/** Query-only interaction volume and prompt anchor attached to the moving door panel. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Interaction|Door")
	TObjectPtr<UBoxComponent> InteractionVolume;

	/** Server-authoritative replicated open/lock state and interaction options. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Interaction|Door")
	TObjectPtr<URpgInteractableDoorComponent> DoorInteraction;

	/** Relative yaw in degrees used by the built-in open presentation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Interaction|Door", meta = (Units = "deg"))
	float OpenYawDegrees = 90.0f;

	/** Presentation hook for Blueprint timelines, audio, and VFX; it must not mutate gameplay state. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rpg|Interaction|Door", meta = (DisplayName = "Animate Door State"))
	void K2_AnimateDoorState(bool bIsOpen, bool bIsLocked);

private:
	UFUNCTION()
	void HandleDoorStateChanged(bool bIsOpen, bool bIsLocked);
};
