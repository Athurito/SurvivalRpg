#pragma once

#include "Components/BoxComponent.h"
#include "Subsystems/WorldSubsystem.h"

#include "RpgHarvestProtectionComponent.generated.h"

/**
 * Box that protects the harvestable resources inside it from area harvests, such as the trees around a camp, a
 * building, or a quest location. Area powers skip protected resources and their previews report them as protected;
 * a deliberate single-target swing still harvests them.
 *
 * Add it to any actor and size it with the box extent. It has no collision and is hidden in game. Every machine
 * evaluates the box it loaded or received, so the server commit and client previews agree for placed and replicated
 * actors. The box may move; a resource counts as protected while its location lies inside the box.
 */
UCLASS(ClassGroup = (Rpg), meta = (BlueprintSpawnableComponent, DisplayName = "RPG Harvest Protection"))
class GF_HARVESTING_MAGIC_API URpgHarvestProtectionComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	explicit URpgHarvestProtectionComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Returns whether WorldLocation lies inside this box, respecting its rotation and scale. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting|Protection")
	bool ProtectsLocation(const FVector& WorldLocation) const;

protected:
	//~ UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent interface
};

/**
 * Registry of the harvest protection boxes that have begun play in a world. Read by harvestable resources when they
 * evaluate an area harvest, on the server and on clients for previews.
 */
UCLASS()
class GF_HARVESTING_MAGIC_API URpgHarvestProtectionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Returns whether any registered protection box in World contains WorldLocation; false without a world. */
	static bool IsLocationProtected(const UWorld* World, const FVector& WorldLocation);

	/** Returns whether any registered protection box contains WorldLocation. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting|Protection")
	bool IsProtected(const FVector& WorldLocation) const;

	/** Returns the number of registered protection boxes. */
	int32 GetNumProtectionZones() const { return Zones.Num(); }

private:
	friend class URpgHarvestProtectionComponent;

	void RegisterZone(const URpgHarvestProtectionComponent& Zone);
	void UnregisterZone(const URpgHarvestProtectionComponent& Zone);

	TArray<TWeakObjectPtr<const URpgHarvestProtectionComponent>> Zones;
};
