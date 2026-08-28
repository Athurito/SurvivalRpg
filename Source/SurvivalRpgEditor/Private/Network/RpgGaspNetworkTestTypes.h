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

/** Editor-only replicated platform used to exercise base-relative CMC corrections under latency. */
UCLASS(NotBlueprintable, Transient)
class ARpgGaspNetworkMovingBaseFixture final : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgGaspNetworkMovingBaseFixture(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void Tick(float DeltaSeconds) override;

	/** Starts deterministic server-owned translation and rotation with replicated movement. */
	void StartMotion();

	/** Stops fixture motion without changing its final replicated transform. */
	void StopMotion();

	/** Returns the primitive that characters should use as their movement base. */
	UBoxComponent* GetMovementSurface() const { return Collision; }

private:
	UPROPERTY()
	TObjectPtr<UBoxComponent> Collision;

	FVector MotionOrigin = FVector::ZeroVector;
	double MotionTime = 0.0;
	bool bMotionEnabled = false;
};
