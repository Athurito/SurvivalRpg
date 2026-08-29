// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"

#include "RpgCombatAnimationProfileProviderComponent.generated.h"

class URpgCombatAnimationProfile;

/**
 * GameFeature-owned cosmetic combat-animation profile provider for one character.
 *
 * The component is intentionally tickless and non-replicated. Each world receives it through the
 * active GameFeature, while the AnimInstance derives presentation from already replicated
 * equipment and rotation state. Concrete Blueprint subclasses live with the feature content and
 * bind their designer-owned profile through class defaults.
 */
UCLASS(Abstract, Blueprintable, ClassGroup = (Animation), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgCombatAnimationProfileProviderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	explicit URpgCombatAnimationProfileProviderComponent(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Returns the first active provider attached to Actor, or null while the feature is absent. */
	static const URpgCombatAnimationProfileProviderComponent* FindForActor(
		const AActor* Actor);

	/** Returns the immutable cosmetic profile owned by the active feature. */
	URpgCombatAnimationProfile* GetCombatAnimationProfile() const
	{
		return CombatAnimationProfile.Get();
	}

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/**
	 * Designer-owned combat overlay data supplied only while this component's GameFeature is active.
	 * The value is static class-default content; it is neither replicated nor mutated at runtime.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Animation|Combat")
	TObjectPtr<URpgCombatAnimationProfile> CombatAnimationProfile;

protected:
	virtual void OnUnregister() override;
};
