#include "Harvesting/RpgHarvestAutomationTestTypes.h"

#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
#include "Inventory/RpgInventoryFragment_HarvestingTool.h"
#include "Harvesting/RpgHarvestableCorpseComponent.h"
#include "SurvivalRpg/Core/Player/RpgBasePlayerState.h"
#include "SurvivalRpg/Core/Corpse/RpgCorpseLifecycleComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

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

	void ConfigureToolDefinition(
		URpgInventoryItemDefinition& Definition,
		const TCHAR* DisplayName,
		const float HarvestPower,
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
		Traits->ItemCategory = ERpgInventoryItemCategory::Tool;
		Traits->bCanStack = false;
		Traits->MaxStackSize = 1;
		Definition.Fragments.Add(Traits);

		URpgInventoryFragment_HarvestingTool* Tool =
			ObjectInitializer.CreateDefaultSubobject<URpgInventoryFragment_HarvestingTool>(
				&Definition,
				TEXT("HarvestingTool"));
		Tool->ToolTag = RpgHarvestingMagicGameplayTags::Tool_Harvesting_Skinning;
		Tool->HarvestPower = HarvestPower;
		Definition.Fragments.Add(Tool);
	}
}

bool ARpgHarvestAutomationPartialFailureDropActor::TrySetPickupInventory(
	const FInventoryPickup& NewPickupInventory)
{
	URpgInventoryManagerComponent* Inventory = GetLootInventoryManager();
	if (HasAuthority() && Inventory && !NewPickupInventory.Templates.IsEmpty())
	{
		const FPickupTemplate& FirstTemplate = NewPickupInventory.Templates[0];
		if (FirstTemplate.ItemDef && FirstTemplate.StackCount > 0)
		{
			Inventory->GrantItemDefinition(
				FirstTemplate.ItemDef,
				FirstTemplate.StackCount);
		}
	}

	// Simulates a custom drop whose internal population failed after its first row.
	return false;
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

URpgHarvestAutomationTestLowToolDefinition::URpgHarvestAutomationTestLowToolDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ConfigureToolDefinition(*this, TEXT("Low Power Skinning Tool"), 1.0f, ObjectInitializer);
}

URpgHarvestAutomationTestHighToolDefinition::URpgHarvestAutomationTestHighToolDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ConfigureToolDefinition(*this, TEXT("High Power Skinning Tool"), 2.0f, ObjectInitializer);
}

URpgHarvestAutomationTestTieToolDefinition::URpgHarvestAutomationTestTieToolDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ConfigureToolDefinition(*this, TEXT("Equal Power Skinning Tool"), 2.0f, ObjectInitializer);
}

ARpgHarvestAutomationCorpseActor::ARpgHarvestAutomationCorpseActor(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
	SetReplicateMovement(false);
	CorpseLifecycle = CreateDefaultSubobject<URpgCorpseLifecycleComponent>(TEXT("CorpseLifecycle"));
	SetRootComponent(CorpseLifecycle);
	HarvestableCorpse = CreateDefaultSubobject<URpgHarvestableCorpseComponent>(TEXT("HarvestableCorpse"));
}
