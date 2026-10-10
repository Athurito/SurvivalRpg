#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "RpgControllerGlyphSubsystem.generated.h"

class UTexture2D;
struct FKey;
struct FSlateBrush;

/** One gamepad glyph set the player can choose, as registered for the current platform in CommonInput. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgGamepadGlyphOption
{
	GENERATED_BODY()

	/** CommonInput gamepad name of the set, for example Generic (Xbox) or PS5. Read-only for UI. */
	UPROPERTY(BlueprintReadOnly, Category = "Input|Glyphs")
	FName GamepadName;

	/** Player-facing name from the controller data's GamepadDisplayName, for example Xbox or PlayStation. */
	UPROPERTY(BlueprintReadOnly, Category = "Input|Glyphs")
	FText DisplayName;
};

/**
 * Applies and remembers which gamepad glyph set (Xbox, PlayStation) a local player's prompts show.
 *
 * Windows reports every XInput controller as the same generic device, so the player picks the set in the settings
 * menu. The choice is local presentation only: it never replicates and gameplay never reads it. It is saved per user
 * in GameUserSettings.ini and applied to the player's UCommonInputSubsystem as soon as the local player exists.
 */
UCLASS(BlueprintType, Config = GameUserSettings)
class SURVIVALRPG_API URpgControllerGlyphSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Gamepad glyph sets registered for the current platform, in the order of CommonInput's ControllerData list. */
	UFUNCTION(BlueprintCallable, Category = "Input|Glyphs")
	static TArray<FRpgGamepadGlyphOption> GetGamepadGlyphOptions();

	/** Whether GamepadName is a gamepad glyph set registered for the current platform. */
	static bool IsGamepadGlyphSetAvailable(FName GamepadName);

	/** Gamepad glyph set this player's prompts show now (CommonInput's current gamepad name). */
	UFUNCTION(BlueprintPure, Category = "Input|Glyphs")
	FName GetGamepadGlyphSet() const;

	/**
	 * Shows GamepadName's glyphs on this player's gamepad prompts and saves the choice for the next start.
	 * Returns false and changes nothing when the set is not registered for this platform or the platform cannot
	 * change its gamepad type.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|Glyphs")
	bool SetGamepadGlyphSet(FName GamepadName);

	/** Brush the current gamepad glyph set draws for Key. Returns false when the set has no glyph for the key. */
	UFUNCTION(BlueprintCallable, Category = "Input|Glyphs")
	bool GetGamepadKeyBrush(const FKey& Key, FSlateBrush& OutBrush) const;

	/** Controller picture of the current gamepad glyph set (its controller data's ControllerTexture), or null. */
	UFUNCTION(BlueprintCallable, Category = "Input|Glyphs")
	UTexture2D* GetGamepadControllerTexture() const;

private:
	/** Glyph set the player chose last; None keeps the platform's default gamepad. Saved in GameUserSettings.ini. */
	UPROPERTY(Config)
	FName SavedGamepadGlyphSet;
};
