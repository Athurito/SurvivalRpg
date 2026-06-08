#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RpgPortalRealmMarkerActor.generated.h"

class USceneComponent;

UENUM(BlueprintType)
enum class ERpgPortalRealmMarkerRole : uint8
{
	/** Player destination after entering the streamed realm. */
	Entry,

	/** Boss spawn location inside the streamed realm. */
	BossSpawn,

	/** Exit portal spawn location after the realm objective is complete. */
	ExitPortal
};

/**
 * Placed marker used inside streamed portal realm levels.
 *
 * This is intentionally separate from Feature encounter points: Feature tags are
 * for overworld GameFeature spawning, while these roles are local realm layout data.
 * Each V1 realm level must contain exactly one marker for each role.
 */
UCLASS(Blueprintable)
class GF_PORTALS_CORE_API ARpgPortalRealmMarkerActor : public AActor
{
	GENERATED_BODY()

public:
	ARpgPortalRealmMarkerActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintPure, Category = "Portal|Realm")
	ERpgPortalRealmMarkerRole GetMarkerRole() const { return MarkerRole; }

	UFUNCTION(BlueprintPure, Category = "Portal|Realm")
	FTransform GetMarkerTransform() const { return GetActorTransform(); }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal|Realm")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Marker meaning consumed by ARpgPortalActor after the realm level is shown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Realm")
	ERpgPortalRealmMarkerRole MarkerRole = ERpgPortalRealmMarkerRole::Entry;
};
