#include "RpgPrimaryGameLayerContract.h"

#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

namespace RpgPrimaryGameLayers
{
	bool FContract::Contains(const FGameplayTag& LayerTag) const
	{
		return LayerTag == Game ||
			LayerTag == GameMenu ||
			LayerTag == Menu ||
			LayerTag == Modal;
	}

	const FContract& GetContract()
	{
		static const FContract Contract = {
			RpgGameplayTags::UI_Layer_Game,
			RpgGameplayTags::UI_Layer_GameMenu,
			RpgGameplayTags::UI_Layer_Menu,
			RpgGameplayTags::UI_Layer_Modal
		};
		return Contract;
	}
}
