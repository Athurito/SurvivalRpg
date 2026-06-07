#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RpgPortalDungeonMarkerActor.generated.h"

class USceneComponent;

UENUM(BlueprintType)
enum class ERpgPortalDungeonMarkerRole : uint8
{
	/** Player destination after entering the streamed dungeon. */
	Entry,

	/** Boss spawn location inside the streamed dungeon. */
	BossSpawn,

	/** Exit portal spawn location after the dungeon objective is complete. */
	ExitPortal
};

/**
 * Placed marker used inside streamed portal dungeon levels.
 *
 * This is intentionally separate from Feature encounter points: Feature tags are
 * for overworld GameFeature spawning, while these roles are local dungeon layout data.
 * Each V1 dungeon level must contain exactly one marker for each role.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API ARpgPortalDungeonMarkerActor : public AActor
{
	GENERATED_BODY()

public:
	ARpgPortalDungeonMarkerActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintPure, Category = "Portal|Dungeon")
	ERpgPortalDungeonMarkerRole GetMarkerRole() const { return MarkerRole; }

	UFUNCTION(BlueprintPure, Category = "Portal|Dungeon")
	FTransform GetMarkerTransform() const { return GetActorTransform(); }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal|Dungeon")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Marker meaning consumed by ARpgPortalActor after the dungeon level is shown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Dungeon")
	ERpgPortalDungeonMarkerRole MarkerRole = ERpgPortalDungeonMarkerRole::Entry;
};
