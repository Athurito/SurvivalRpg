#pragma once

#include "GameplayTagContainer.h"

namespace RpgPrimaryGameLayers
{
	/**
	 * Canonical CommonUI layer tags registered by URpgPrimaryGameLayout.
	 *
	 * Screen registries and GameFeature widget actions must target one of these exact tags. Adding another layer
	 * requires extending both this contract and the authored root layout.
	 */
	struct SURVIVALRPG_API FContract
	{
		FGameplayTag Game;
		FGameplayTag GameMenu;
		FGameplayTag Menu;
		FGameplayTag Modal;

		/** Returns true only for one of the four exact layer tags registered by the project root layout. */
		bool Contains(const FGameplayTag& LayerTag) const;
	};

	/** Returns the immutable project-wide CommonUI root-layer contract. */
	SURVIVALRPG_API const FContract& GetContract();
}
