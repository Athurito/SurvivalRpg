#pragma once

#include "AttributeSet.h"
#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "MVVMViewModelBase.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"

#include "RpgCharacterStatsViewModel.generated.h"

class APlayerController;
class UAbilitySystemComponent;
class UGameplayAbility;
class URpgPlayerProgressionComponent;
struct FOnAttributeChangeData;
struct FRpgEquipmentLoadoutSlotsChangedMessage;

/**
 * Read model of the local player's character values: level and experience, health, stamina, mana, armour and
 * equipment load. The inventory's character stats column and the HUD bind the same instance.
 *
 * It also derives the HUD context: whether the character is in combat and whether every vital is full, which the HUD
 * uses to fade its vitals and action bars.
 *
 * URpgUiSubsystem owns one instance per local player and keeps it bound to the controller, the player state's
 * progression and the pawn's ability system across respawns. Widgets get it through URpgLocalPlayerViewModelResolver.
 * Every field mirrors replicated, server-authored gameplay state; the UI never writes it back.
 */
UCLASS(BlueprintType, meta = (MVVMAllowedContextCreationType = "Resolver"))
class SURVIVALRPG_API URpgCharacterStatsViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

	/** Observes the equipment load of InPlayerController and the progression on its player state, once it exists. */
	void BindPlayerController(APlayerController* InPlayerController);

	/** Observes health, stamina and armour on InAbilitySystem, and the progression on its owning player state. */
	void BindAbilitySystem(UAbilitySystemComponent* InAbilitySystem);

	/** Stops observing the ability system and clears the attribute fields, for example while the pawn is gone. */
	void UnbindAbilitySystem();

	/** Observes one character progression component. Null clears the level and experience fields. */
	void BindProgression(URpgPlayerProgressionComponent* InProgression);

	/** Stops observing everything and clears every field. */
	void Unbind();

protected:
	/** Character level, at least 1. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|Level", meta = (AllowPrivateAccess = "true"))
	int32 CharacterLevel = 1;

	/** CharacterLevel as display text. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|Level", meta = (AllowPrivateAccess = "true"))
	FText CharacterLevelText;

	/** Experience gathered toward the next level. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|Level", meta = (AllowPrivateAccess = "true"))
	float CharacterXP = 0.0f;

	/** Experience the next level costs; zero without an XP curve. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|Level", meta = (AllowPrivateAccess = "true"))
	float CharacterXPToNextLevel = 0.0f;

	/** CharacterXP / CharacterXPToNextLevel in [0, 1]; 1 at the maximum level, 0 without an XP curve. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|Level", meta = (AllowPrivateAccess = "true"))
	float CharacterLevelProgress = 0.0f;

	/** Experience as display text: "120 / 400", "Max level", or "120 XP" without an XP curve. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|Level", meta = (AllowPrivateAccess = "true"))
	FText CharacterXPText;

	/** Current health. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|Health", meta = (AllowPrivateAccess = "true"))
	float Health = 0.0f;

	/** Maximum health. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|Health", meta = (AllowPrivateAccess = "true"))
	float MaxHealth = 0.0f;

	/** Health / MaxHealth in [0, 1]. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|Health", meta = (AllowPrivateAccess = "true"))
	float HealthProgress = 0.0f;

	/** Health as display text, for example "84 / 100". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|Health", meta = (AllowPrivateAccess = "true"))
	FText HealthText;

	/** Current stamina. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|Stamina", meta = (AllowPrivateAccess = "true"))
	float Stamina = 0.0f;

	/** Maximum stamina. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|Stamina", meta = (AllowPrivateAccess = "true"))
	float MaxStamina = 0.0f;

	/** Stamina / MaxStamina in [0, 1]. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|Stamina", meta = (AllowPrivateAccess = "true"))
	float StaminaProgress = 0.0f;

	/** Stamina as display text, for example "60 / 100". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|Stamina", meta = (AllowPrivateAccess = "true"))
	FText StaminaText;

	/** Whether the ability system owns URpgManaSet. Without it the mana fields stay zero and the HUD hides the mana bar. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|Mana", meta = (AllowPrivateAccess = "true"))
	bool bHasMana = false;

	/** Current mana; zero without a mana set. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|Mana", meta = (AllowPrivateAccess = "true"))
	float Mana = 0.0f;

	/** Maximum mana; zero without a mana set. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|Mana", meta = (AllowPrivateAccess = "true"))
	float MaxMana = 0.0f;

	/** Mana / MaxMana in [0, 1]. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|Mana", meta = (AllowPrivateAccess = "true"))
	float ManaProgress = 0.0f;

	/** Mana as display text, for example "40 / 100"; empty without a mana set. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|Mana", meta = (AllowPrivateAccess = "true"))
	FText ManaText;

	/** Armour attribute of the defense set. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|Defense", meta = (AllowPrivateAccess = "true"))
	float Armor = 0.0f;

	/** Armor as display text, rounded. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|Defense", meta = (AllowPrivateAccess = "true"))
	FText ArmorText;

	/** Gear and Carry equipment load in kilograms. Container contents do not count. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|Load", meta = (AllowPrivateAccess = "true", Units = "kg"))
	float EquipmentLoad = 0.0f;

	/** First load in kilograms that counts as heavy; the load bar is full there. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|Load", meta = (AllowPrivateAccess = "true", Units = "kg"))
	float HeavyLoadThreshold = 0.0f;

	/** EquipmentLoad / HeavyLoadThreshold in [0, 1]. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|Load", meta = (AllowPrivateAccess = "true"))
	float EquipmentLoadProgress = 0.0f;

	/** Load tier that selects the dodge profile. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|Load", meta = (AllowPrivateAccess = "true"))
	ERpgEquipmentLoadTier EquipmentLoadTier = ERpgEquipmentLoadTier::Light;

	/** Load as display text, for example "12.5 / 23 kg". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|Load", meta = (AllowPrivateAccess = "true"))
	FText EquipmentLoadText;

	/** EquipmentLoadTier as display text: Light, Medium or Heavy. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|Load", meta = (AllowPrivateAccess = "true"))
	FText EquipmentLoadTierText;

	/**
	 * Whether the character is in combat: it took damage or used an ability its equipment grants (attacks, block,
	 * weapon and tool abilities) within URpgUISettings::HudCombatHoldSeconds. Local presentation state of the owning
	 * client; gameplay never reads it.
	 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|HUD", meta = (AllowPrivateAccess = "true"))
	bool bInCombat = false;

	/** Whether health, stamina and, with a mana set, mana are all at their maximum. False without an ability system. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|HUD", meta = (AllowPrivateAccess = "true"))
	bool bVitalsFull = false;

	/** Whether the HUD should show the vitals: in combat or while a vital is not full. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Character Stats|HUD", meta = (AllowPrivateAccess = "true"))
	bool bShowVitals = false;

private:
	UFUNCTION()
	void HandleCharacterLevelChanged(int32 NewLevel);

	UFUNCTION()
	void HandleCharacterXPChanged(float CurrentXP, float XPToNextLevel);

	void HandleAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void HandleAbilityActivated(UGameplayAbility* Ability);
	void HandleEquipmentLoadoutChanged(FGameplayTag Channel, const FRpgEquipmentLoadoutSlotsChangedMessage& Message);

	void ReleaseAbilitySystem();
	void ReleaseProgression();
	void RefreshAttributes();
	void RefreshProgression();
	void ApplyEquipmentLoad(float NewLoad, float NewHeavyThreshold, ERpgEquipmentLoadTier NewTier);
	void BindProgressionOfPlayerState(const APlayerController* PlayerController, const UAbilitySystemComponent* AbilitySystem);
	void SetText(FText& Field, const FText& NewValue, UE::FieldNotification::FFieldId FieldId);
	void EnterCombat();
	void ExitCombat();
	void ClearCombat();
	void RefreshHudContext();
	UWorld* GetObservedWorld() const;

	TWeakObjectPtr<APlayerController> ObservedPlayerController;
	TWeakObjectPtr<UAbilitySystemComponent> ObservedAbilitySystem;
	TWeakObjectPtr<URpgPlayerProgressionComponent> ObservedProgression;
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> AttributeHandles;
	FDelegateHandle AbilityActivatedHandle;
	FGameplayMessageListenerHandle LoadoutChangedHandle;
	FTimerHandle CombatTimerHandle;
};
