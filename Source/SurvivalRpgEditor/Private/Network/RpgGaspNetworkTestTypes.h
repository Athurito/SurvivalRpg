#pragma once

#include "GameFramework/Actor.h"

#include "RpgGaspNetworkTestTypes.generated.h"

class UBoxComponent;
class USceneComponent;

/** Editor-only replicated floor that gives every PIE world the same network-addressable movement base. */
UCLASS(NotBlueprintable, Transient)
class ARpgGaspNetworkFloorFixture final : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgGaspNetworkFloorFixture(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY()
	TObjectPtr<UBoxComponent> Collision;
};
