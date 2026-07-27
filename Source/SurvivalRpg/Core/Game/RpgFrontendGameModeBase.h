#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayTagContainer.h"

#include "RpgFrontendGameModeBase.generated.h"

/**
 * Lightweight composition root for non-gameplay maps such as Boot and Main Menu.
 *
 * Frontend maps configure only an initial UI.Screen tag. CommonGame owns the
 * viewport root, while the map-owned frontend HUD opens and closes that screen.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API ARpgFrontendGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	explicit ARpgFrontendGameModeBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Screen opened for each local player while this frontend map is active. Static designer configuration. */
	FGameplayTag GetInitialScreenTag() const { return InitialScreenTag; }

protected:
	/** Registry key for the map's single CommonUI screen, for example UI.Screen.Boot or UI.Screen.MainMenu. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Frontend", meta = (Categories = "UI.Screen"))
	FGameplayTag InitialScreenTag;
};
