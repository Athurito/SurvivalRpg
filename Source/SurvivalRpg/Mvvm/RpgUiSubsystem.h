// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "RpgUiSubsystem.generated.h"

class APlayerController;
class APawn;
class URpgCharacterStatsViewModel;
class URpgPickupFeedViewModel;
class URpgRangedCombatViewModel;
class URpgPawnExtensionComponent;
/**
 * Owns the view models that follow one local player across pawns. Keeps them bound to the player controller, its
 * player state and the current pawn's ability system: character stats, the pickup feed and the ranged weapon HUD.
 */
UCLASS()
class SURVIVALRPG_API URpgUiSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	/** Level, experience, health, stamina, armour and equipment load of this local player. */
	URpgCharacterStatsViewModel* GetCharacterStatsViewModel() const { return CharacterStatsVM; }

	/** Items this local player gains from the world, for the HUD pickup notifications. */
	URpgPickupFeedViewModel* GetPickupFeedViewModel() const { return PickupFeedVM; }

	/** Aim, draw strength and ammunition of this local player's ranged weapon. */
	URpgRangedCombatViewModel* GetRangedCombatViewModel() const { return RangedCombatVM; }

	/** Returns the owned view model of exactly ExpectedType, or null. URpgLocalPlayerViewModelResolver hands it to widgets. */
	UObject* FindViewModel(const UClass* ExpectedType) const;

	/** Whether this subsystem owns a view model of exactly Class. */
	static bool ProvidesViewModelClass(const UClass* Class);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

private:
	UFUNCTION()
	void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);

	void BindToPlayerController(APlayerController* NewPlayerController);
	void UnbindFromPlayerController();
	void BindToPawn(APawn* NewPawn);
	void UnbindFromPawnExtension(bool bResetViewModel = true);
	void HandleAbilitySystemInitialized();
	void HandleAbilitySystemUninitialized();

	UPROPERTY(Transient)
	TObjectPtr<URpgCharacterStatsViewModel> CharacterStatsVM;

	UPROPERTY(Transient)
	TObjectPtr<URpgPickupFeedViewModel> PickupFeedVM;

	UPROPERTY(Transient)
	TObjectPtr<URpgRangedCombatViewModel> RangedCombatVM;

	UPROPERTY(Transient)
	TObjectPtr<APlayerController> BoundPlayerController;

	UPROPERTY(Transient)
	TObjectPtr<URpgPawnExtensionComponent> BoundPawnExtension;
};
