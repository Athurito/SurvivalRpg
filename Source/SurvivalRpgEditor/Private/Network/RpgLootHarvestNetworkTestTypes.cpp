#include "Network/RpgLootHarvestNetworkTestTypes.h"

#include "Components/SceneComponent.h"
#include "Harvesting/RpgHarvestableInstancedMeshComponent.h"
#include "Harvesting/RpgHarvestProfile.h"
#include "SurvivalRpg/Core/Player/RpgBasePlayerState.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgLootHarvestNetworkTestTypes)

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
		Traits->MaxStackSize = 20;
		Traits->bTreatAsMaterial = true;
		Definition.Fragments.Add(Traits);
	}
}

void URpgNetworkAutomationLootSourceComponent::ConfigureLootTable(
	URpgLootTable* InLootTable)
{
	LootTable = InLootTable;
	LootTemplates.Reset();
	LootInstances.Reset();
	SourceLevel = 1;
	bUnlockContainerOnDeath = false;
}

URpgNetworkAutomationMaterialDefinition::URpgNetworkAutomationMaterialDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ConfigureMaterialDefinition(
		*this,
		TEXT("Network Harvest Material A"),
		ObjectInitializer);
}

URpgNetworkAutomationSecondMaterialDefinition::URpgNetworkAutomationSecondMaterialDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ConfigureMaterialDefinition(
		*this,
		TEXT("Network Harvest Material B"),
		ObjectInitializer);
}

ARpgNetworkAutomationLootFixture::ARpgNetworkAutomationLootFixture(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	Inventory = CreateDefaultSubobject<URpgInventoryManagerComponent>(TEXT("Inventory"));
	LootSource =
		CreateDefaultSubobject<URpgNetworkAutomationLootSourceComponent>(TEXT("LootSource"));
}

void ARpgNetworkAutomationHarvesterState::PostInitializeComponents()
{
	// The fixture needs the real ASC/inventory/skill lifecycle but not an Experience-bound PawnData grant.
	ARpgBasePlayerState::PostInitializeComponents();
}

ARpgNetworkAutomationHarvestFixture::ARpgNetworkAutomationHarvestFixture(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);

	HarvestableInstances =
		CreateDefaultSubobject<URpgHarvestableInstancedMeshComponent>(
			TEXT("HarvestableInstances"));
	SetRootComponent(HarvestableInstances);
	if (HarvestableInstances->GetInstanceCount() == 0)
	{
		HarvestableInstances->AddInstance(FTransform::Identity);
	}
}

bool ARpgNetworkAutomationHarvestFixture::ConfigureHarvestProfile(
	URpgHarvestProfile* InProfile)
{
	FObjectProperty* HarvestProfileProperty = FindFProperty<FObjectProperty>(
		URpgHarvestableInstancedMeshComponent::StaticClass(),
		TEXT("HarvestProfile"));
	if (!HarvestableInstances || !HarvestProfileProperty || !InProfile)
	{
		return false;
	}

	HarvestProfileProperty->SetObjectPropertyValue_InContainer(
		HarvestableInstances,
		InProfile);
	return true;
}
