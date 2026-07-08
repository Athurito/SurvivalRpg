#pragma once

#include "GameFramework/Actor.h"

#include "RpgBaseStorageStationActor.generated.h"

class URpgBaseStorageStationComponent;
class USceneComponent;
class USphereComponent;

/**
 * Placeable base storage terminal or resource unit with native interaction collision.
 *
 * Designers can subclass this actor for visuals while storage truth stays in the linked base camp.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API ARpgBaseStorageStationActor : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgBaseStorageStationActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Interaction/access component that links this physical station to a base camp. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Storage")
	URpgBaseStorageStationComponent* GetStorageStationComponent() const { return StorageStationComponent; }

protected:
	/** Simple root so Blueprint children can attach station meshes and VFX. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Base Storage")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Native overlap used by Lyra-style interaction scans to discover this station. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Base Storage")
	TObjectPtr<USphereComponent> InteractionCollision;

	/** Server-authoritative station metadata, capacity contribution, and upgrade state. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Base Storage")
	TObjectPtr<URpgBaseStorageStationComponent> StorageStationComponent;
};
