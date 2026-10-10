#pragma once

#include "Components/SizeBox.h"

#include "RpgHudFadeBox.generated.h"

class FActiveTimerHandle;
enum class EActiveTimerReturnType : uint8;

/** Timing settings of one fade box; see URpgHudFadeBox for their meaning. */
struct FRpgHudFadeSettings
{
	float HiddenOpacity = 0.0f;
	float HoldSeconds = 4.0f;
	float FadeInSeconds = 0.15f;
	float FadeOutSeconds = 0.6f;
};

/**
 * Fade state of URpgHudFadeBox, kept free of Slate so it can be advanced with explicit times.
 * A pinned box stays fully visible. After unpinning, or after a pulse, it holds full opacity for HoldSeconds and then
 * fades to HiddenOpacity.
 */
struct SURVIVALRPG_API FRpgHudFadeState
{
	/** Whether the content is wanted right now. */
	bool bPinned = false;

	/** Set by an unpin or a pulse; the next Advance starts the hold at its time. */
	bool bRestartHold = true;

	/** Time until which an unpinned box stays fully visible. */
	double HoldUntil = 0.0;

	/** Current opacity in [0, 1]. */
	float Opacity = 1.0f;

	void SetPinned(bool bInPinned);
	void Pulse();

	/** Opacity the box moves toward at Now. */
	float GetTargetOpacity(double Now, const FRpgHudFadeSettings& Settings) const;

	/** Moves Opacity toward its target. Returns whether the box must keep advancing (fading or holding). */
	bool Advance(double Now, float DeltaSeconds, const FRpgHudFadeSettings& Settings);
};

/**
 * Size box that fades its content by context, for HUD groups such as the vitals, the action bars or an enemy's health
 * bar. Widget Blueprints pin it while the content matters (for example from a view model's bShowVitals) and pulse it
 * on events (for example gained experience). It changes only its own render opacity, never visibility or layout.
 *
 * Presentation only. The timing values are designer-tuned per instance.
 */
UCLASS(meta = (DisplayName = "RPG HUD Fade Box"))
class SURVIVALRPG_API URpgHudFadeBox : public USizeBox
{
	GENERATED_BODY()

public:
	/** Keeps the content fully visible while true. Unpinning holds it for HoldSeconds, then fades it out. */
	UFUNCTION(BlueprintCallable, Category = "HUD Fade")
	void SetPinned(bool bInPinned);

	/** Shows the content now and holds it for HoldSeconds, for one-off events. */
	UFUNCTION(BlueprintCallable, Category = "HUD Fade")
	void Pulse();

	UFUNCTION(BlueprintPure, Category = "HUD Fade")
	bool IsPinned() const { return FadeState.bPinned; }

	/** Opacity once faded: 0 hides the content, 0.4 dims it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD Fade", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float HiddenOpacity = 0.0f;

	/** Seconds the content stays fully visible after an unpin or a pulse. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD Fade", meta = (ClampMin = "0", UIMin = "0", UIMax = "30", Units = "s"))
	float HoldSeconds = 4.0f;

	/** Seconds a fade from hidden to fully visible takes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD Fade", meta = (ClampMin = "0", UIMin = "0", UIMax = "3", Units = "s"))
	float FadeInSeconds = 0.15f;

	/** Seconds a fade from fully visible to hidden takes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD Fade", meta = (ClampMin = "0", UIMin = "0", UIMax = "3", Units = "s"))
	float FadeOutSeconds = 0.6f;

	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	FRpgHudFadeSettings MakeSettings() const;
	void EnsureFadeTimer();
	EActiveTimerReturnType HandleFadeTimer(double InCurrentTime, float InDeltaTime);

	FRpgHudFadeState FadeState;
	TWeakPtr<FActiveTimerHandle> FadeTimerHandle;
};
