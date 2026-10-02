// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Templates/SubclassOf.h"
#include "RpgGameData.generated.h"

class UGameplayAbility;
class UGameplayEffect;

/**
 * Project-wide gameplay asset references.
 *
 * Mirrors Lyra's GameData asset: globally reusable default assets live here so
 * actor components do not need to own hard-coded ability or gameplay effect defaults.
 */
UCLASS(BlueprintType, Const, meta = (DisplayName = "RPG Game Data"))
class SURVIVALRPG_API URpgGameData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	URpgGameData();

	/** Returns the configured project GameData asset through the RPG asset manager. */
	static const URpgGameData& Get();

public:
	/** Generic instant damage effect. It reads SetByCaller.Damage from the outgoing spec. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay Effects", meta = (AllowedClasses = "/Script/GameplayAbilities.GameplayEffect"))
	TSoftClassPtr<UGameplayEffect> DamageGameplayEffect_SetByCaller;

	/** Generic instant healing effect. It reads SetByCaller.Healing from the outgoing spec. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay Effects", meta = (AllowedClasses = "/Script/GameplayAbilities.GameplayEffect"))
	TSoftClassPtr<UGameplayEffect> HealGameplayEffect_SetByCaller;

	/** Infinite GameplayEffect used by the ASC to add and remove dynamic granted tags. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay Effects", meta = (AllowedClasses = "/Script/GameplayAbilities.GameplayEffect"))
	TSoftClassPtr<UGameplayEffect> DynamicTagGameplayEffect;

	/** Designer-owned collect ability used by world pickups without an explicit override; resolved when gathering options. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction Abilities", meta = (AssetBundles = "Client,Server"))
	TSoftClassPtr<UGameplayAbility> CollectInteractionAbility;

	/** Designer-owned ability for server-validated target commits, including doors and instanced harvesting. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction Abilities", meta = (AssetBundles = "Client,Server"))
	TSoftClassPtr<UGameplayAbility> ExecuteInteractionAbility;

	/** Designer-owned storage access ability used when a container has no explicit ability override. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction Abilities", meta = (AssetBundles = "Client,Server"))
	TSoftClassPtr<UGameplayAbility> OpenStorageInteractionAbility;

	/** Designer-owned crafting access ability used when a station has no explicit ability override. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction Abilities", meta = (AssetBundles = "Client,Server"))
	TSoftClassPtr<UGameplayAbility> OpenCraftingInteractionAbility;

	/** Designer-owned base storage access ability used when a station has no explicit ability override. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction Abilities", meta = (AssetBundles = "Client,Server"))
	TSoftClassPtr<UGameplayAbility> OpenBaseStorageInteractionAbility;

	/** Designer-owned revive ability used when a downed character has no explicit ability override. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction Abilities", meta = (AssetBundles = "Client,Server"))
	TSoftClassPtr<UGameplayAbility> ReviveInteractionAbility;
};
