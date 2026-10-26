#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RpgWeaponSwingRecorderComponent.generated.h"

class USceneComponent;

/**
 * Presentation for afterimages of a weapon swing. It keeps the recent world transforms of its owner's root component,
 * the weapon mesh, and on request moves a follower point along them with a delay, so a cue can attach an effect that
 * repeats the swing. Every machine records its own animation of the swing, so the afterimage needs no replicated path.
 * Cosmetic: no gameplay reads it, and it never ticks on a dedicated server.
 */
UCLASS(ClassGroup = (Rpg), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgWeaponSwingRecorderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URpgWeaponSwingRecorderComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Starts moving the follower along the recorded transforms, Delay seconds in the past, for Duration seconds, and
	 * returns it for attaching the afterimage effect. A running afterimage restarts. Null on a dedicated server.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Weapon|Afterimage")
	USceneComponent* BeginAfterimage(float Delay, float Duration);

	/** The recorded root transform at WorldTime, interpolated; false before the first sample. */
	bool SampleTransform(float WorldTime, FTransform& OutTransform) const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	struct FSample
	{
		float Time = 0.0f;
		FTransform Transform;
	};

	/** Seconds of history kept; must cover the longest delay plus duration an afterimage asks for. */
	UPROPERTY(EditDefaultsOnly, Category = "Rpg|Weapon|Afterimage", meta = (ClampMin = "0.1", UIMax = "3.0", Units = "s"))
	float HistorySeconds = 1.0f;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Follower;

	TArray<FSample> Samples;
	float AfterimageDelay = 0.0f;
	float AfterimageEndTime = -1.0f;
};
