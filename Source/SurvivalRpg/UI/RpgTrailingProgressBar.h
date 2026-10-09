#pragma once

#include "Components/ProgressBar.h"

#include "RpgTrailingProgressBar.generated.h"

class FActiveTimerHandle;
enum class EActiveTimerReturnType : uint8;

/**
 * Trail state of URpgTrailingProgressBar, kept free of Slate so it can be advanced with explicit times.
 * A rise shows at once. A drop waits DelaySeconds after the last drop, then drains at DrainPerSecond.
 */
struct SURVIVALRPG_API FRpgProgressTrailState
{
	/** Fraction the bar draws, in [0, 1]. */
	float Displayed = 0.0f;

	/** Fraction the bar follows, in [0, 1]. */
	float Target = 0.0f;

	/** Set by a drop; the next Advance starts the delay at its time. */
	bool bRestartDelay = false;

	/** Time from which a drop drains. */
	double DrainStart = 0.0;

	void SetTarget(float InTarget);

	/** Moves Displayed toward Target. Returns whether the trail must keep advancing. */
	bool Advance(double Now, float DeltaSeconds, float DelaySeconds, float DrainPerSecond);
};

/**
 * Progress bar that trails its value: drops linger briefly before they drain, so a hit stays readable. Lay it under a
 * normal progress bar bound to the same value, give it a lighter fill, and bind TargetPercent instead of Percent.
 *
 * Presentation only. The delay and drain speed are designer-tuned per instance.
 */
UCLASS(meta = (DisplayName = "RPG Trailing Progress Bar"))
class SURVIVALRPG_API URpgTrailingProgressBar : public UProgressBar
{
	GENERATED_BODY()

public:
	/** Fraction the bar follows, in [0, 1]. A rise shows at once; a drop drains after TrailDelaySeconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter = "SetTargetPercent", Category = "Progress", meta = (UIMin = "0", UIMax = "1"))
	float TargetPercent = 0.0f;

	/** Seconds a drop lingers before it drains. Each further drop restarts the delay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progress", meta = (ClampMin = "0", UIMin = "0", UIMax = "3", Units = "s"))
	float TrailDelaySeconds = 0.5f;

	/** Drain speed in bar fractions per second; 1 drains a full bar in one second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progress", meta = (ClampMin = "0.01", UIMin = "0.05", UIMax = "5"))
	float TrailDrainPerSecond = 0.6f;

	UFUNCTION(BlueprintCallable, Category = "Progress")
	void SetTargetPercent(float InTargetPercent);

	float GetTargetPercent() const { return TargetPercent; }

	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

private:
	void EnsureTrailTimer();
	EActiveTimerReturnType HandleTrailTimer(double InCurrentTime, float InDeltaTime);

	FRpgProgressTrailState TrailState;
	bool bTrailInitialized = false;
	TWeakPtr<FActiveTimerHandle> TrailTimerHandle;
};
