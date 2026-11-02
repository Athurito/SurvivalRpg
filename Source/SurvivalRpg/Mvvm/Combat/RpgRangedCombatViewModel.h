#pragma once

#include "Containers/Ticker.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "MVVMViewModelBase.h"
#include "Templates/SubclassOf.h"

#include "RpgRangedCombatViewModel.generated.h"

class APlayerController;
class UTexture2D;
class URpgAbilitySystemComponent;
class URpgInventoryItemDefinition;
class URpgInventoryManagerComponent;
class URpgRangedWeaponInstance;
struct FRpgInventoryChangeMessage;

/**
 * Read-only HUD state of one local player's ranged weapon (ranged combat roadmap RNG-04): whether the active main hand
 * is a ranged weapon, its ammunition in the player's own inventory, the aim stance and the draw strength.
 *
 * URpgUiSubsystem owns one per local player and keeps it bound across pawns and ability system changes; widgets get it
 * through URpgLocalPlayerViewModelResolver. The weapon is the ranged weapon instance in the bound pawn's main hand, else
 * its off hand, whose shot definition names the ammunition the shot spends; the count follows the inventory messages
 * of the inventory the shot spends from. Aim and draw are the
 * owner's own prediction, read every frame only while a ranged weapon is active. Nothing here
 * shows a trajectory or a landing point: the draw strength describes the draw, not where the arrow goes.
 */
UCLASS(BlueprintType, DisplayName = "Ranged Combat Viewmodel", meta = (MVVMAllowedContextCreationType = "Resolver"))
class SURVIVALRPG_API URpgRangedCombatViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

	/** Counts ammunition in Controller's player state inventory; null unbinds it. */
	void BindPlayerController(APlayerController* Controller);

	/** Reads the weapon, aim and draw of AbilitySystem's pawn, the current one; null unbinds it. */
	void BindAbilitySystem(URpgAbilitySystemComponent* AbilitySystem);

	/** Releases every binding and resets the fields. */
	void Unbind();

	/** Re-reads the pawn's ranged weapon and, when it changed, its ammunition. Called a few times a second. */
	void RefreshWeapon();

	/** Re-reads aim and draw from the bound ability system. Called every frame while a ranged weapon is active. */
	void RefreshRangedState();

	bool GetHasRangedWeapon() const { return bHasRangedWeapon; }
	int32 GetAmmunitionCount() const { return AmmunitionCount; }
	bool GetShotAvailable() const { return bShotAvailable; }
	bool GetAiming() const { return bAiming; }
	bool GetDrawing() const { return bDrawing; }
	float GetDrawFraction() const { return DrawFraction; }
	float GetNockFraction() const { return NockFraction; }
	bool GetNocked() const { return bNocked; }
	bool GetFullyDrawn() const { return bFullyDrawn; }
	bool IsTickingForTests() const { return TickerHandle.IsValid(); }

#if WITH_DEV_AUTOMATION_TESTS
	/** Treats the active weapon as one that shoots Ammunition, as an equipped bow does; null as no ranged weapon. */
	void SetAmmunitionForTests(TSubclassOf<URpgInventoryItemDefinition> Ammunition);
#endif

private:
	/** True while a hand of the pawn holds a ranged weapon; the ammunition HUD and the crosshair show only then. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Ranged Combat|Ammunition", meta = (AllowPrivateAccess = "true"))
	bool bHasRangedWeapon = false;

	/** Items of the weapon's ammunition in the player's inventory, carried bags included; the count the shot spends. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Ranged Combat|Ammunition", meta = (AllowPrivateAccess = "true"))
	int32 AmmunitionCount = 0;

	/** AmmunitionCount as display text. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Ranged Combat|Ammunition", meta = (AllowPrivateAccess = "true"))
	FText AmmunitionCountText;

	/** True while a ranged weapon is active without any of its ammunition. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Ranged Combat|Ammunition", meta = (AllowPrivateAccess = "true"))
	bool bOutOfAmmunition = false;

	/** Display name of the weapon's ammunition, empty without a ranged weapon. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Ranged Combat|Ammunition", meta = (AllowPrivateAccess = "true"))
	FText AmmunitionName;

	/** Icon of the weapon's ammunition, unset without a ranged weapon or icon. Loaded lazily by the widget. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Ranged Combat|Ammunition", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> AmmunitionIcon;

	/** True while a ranged weapon is active and has ammunition. Stamina for the release shows on the stamina bar. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Ranged Combat|Ammunition", meta = (AllowPrivateAccess = "true"))
	bool bShotAvailable = false;

	/** True while the ranged aim stance holds, from a held aim or a draw; the bow crosshair shows only then. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Ranged Combat|Aim", meta = (AllowPrivateAccess = "true"))
	bool bAiming = false;

	/** True while the player draws a shot. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Ranged Combat|Draw", meta = (AllowPrivateAccess = "true"))
	bool bDrawing = false;

	/** Draw strength from 0 to 1 over the full draw time; zero without a draw. Describes the draw, never the flight. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Ranged Combat|Draw", meta = (AllowPrivateAccess = "true", ClampMin = "0", ClampMax = "1"))
	float DrawFraction = 0.0f;

	/** Where nocking ends on the DrawFraction scale; a release before it shoots nothing. Zero without a draw. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Ranged Combat|Draw", meta = (AllowPrivateAccess = "true", ClampMin = "0", ClampMax = "1"))
	float NockFraction = 0.0f;

	/** True once the draw passed the nock, so a release shoots. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Ranged Combat|Draw", meta = (AllowPrivateAccess = "true"))
	bool bNocked = false;

	/** True at full draw. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Ranged Combat|Draw", meta = (AllowPrivateAccess = "true"))
	bool bFullyDrawn = false;

	void HandleInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message);
	bool HandleTick(float DeltaSeconds);
	URpgInventoryManagerComponent* FindInventory() const;
	void ApplyWeapon(bool bWeapon, TSubclassOf<URpgInventoryItemDefinition> Ammunition);
	void RefreshAmmunition();
	void UpdateTicker();
	void ResetRangedState();

	TWeakObjectPtr<APlayerController> ObservedController;
	TWeakObjectPtr<URpgAbilitySystemComponent> ObservedAbilitySystem;
	TWeakObjectPtr<URpgRangedWeaponInstance> ObservedWeapon;
	TSubclassOf<URpgInventoryItemDefinition> AmmunitionDefinition;
	FGameplayMessageListenerHandle InventoryListener;
	FTSTicker::FDelegateHandle TickerHandle;
	float SecondsUntilWeaponCheck = 0.0f;
};
