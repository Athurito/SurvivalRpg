#include "Harvesting/RpgHarvestAutomationTestTypes.h"

#include "SurvivalRpg/Core/Player/RpgBasePlayerState.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgHarvestAutomationTestTypes)

namespace
{
	void ConfigureMaterialDefinition(
		URpgInventoryItemDefinition& Definition,
		const TCHAR* DisplayName,
		const FObjectInitializer& ObjectInitializer)
	{
		Definition.DisplayName = FText::FromString(DisplayName);

		URpgInventoryFragment_SpatialItem* Spatial =
			ObjectInitializer.CreateDefaultSubobject<URpgInventoryFragment_SpatialItem>(
				&Definition,
				TEXT("Spatial"));
		Spatial->Footprint.Width = 1;
		Spatial->Footprint.Height = 1;
		Definition.Fragments.Add(Spatial);

		URpgInventoryFragment_ItemTraits* Traits =
			ObjectInitializer.CreateDefaultSubobject<URpgInventoryFragment_ItemTraits>(
				&Definition,
				TEXT("Traits"));
		Traits->ItemCategory = ERpgInventoryItemCategory::Material;
		Traits->bCanStack = true;
		Traits->MaxStackSize = 10;
		Traits->bTreatAsMaterial = true;
		Definition.Fragments.Add(Traits);
	}
}

void ARpgHarvestAutomationTestPlayerState::PostInitializeComponents()
{
	// Keep the shared ASC/component lifecycle, but skip ARpgPlayerState's Experience/GameState requirement.
	ARpgBasePlayerState::PostInitializeComponents();
}

URpgHarvestAutomationTestStackItemDefinition::URpgHarvestAutomationTestStackItemDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ConfigureMaterialDefinition(*this, TEXT("Harvest Test Material"), ObjectInitializer);
}

URpgHarvestAutomationTestSecondMaterialDefinition::URpgHarvestAutomationTestSecondMaterialDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ConfigureMaterialDefinition(*this, TEXT("Harvest Test Material B"), ObjectInitializer);
}
