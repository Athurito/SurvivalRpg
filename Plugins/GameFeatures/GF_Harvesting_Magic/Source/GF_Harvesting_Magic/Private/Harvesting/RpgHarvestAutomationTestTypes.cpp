#include "Harvesting/RpgHarvestAutomationTestTypes.h"

#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
#include "Inventory/RpgInventoryFragment_HarvestingTool.h"
#include "Components/BoxComponent.h"
#include "Engine/CollisionProfile.h"
#include "Components/SceneComponent.h"
#include "Harvesting/RpgHarvestableComponent.h"
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

ARpgHarvestAutomationNodeActor::ARpgHarvestAutomationNodeActor(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
	SetReplicateMovement(false);
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	HarvestableNode = CreateDefaultSubobject<URpgHarvestableComponent>(TEXT("HarvestableNode"));
}

void URpgHarvestAutomationNodeStateListener::HandleStateChanged(
	URpgHarvestableComponent* Component,
	const int32 RemainingSections,
	const int32 SectionCount,
	const bool bActive,
	const bool bInitialState)
{
	(void)Component;
	++EventCount;
	LastRemainingSections = RemainingSections;
	LastSectionCount = SectionCount;
	bLastActive = bActive;
	bLastInitialState = bInitialState;
}

ARpgHarvestAutomationCollidableNodeActor::ARpgHarvestAutomationCollidableNodeActor(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
	SetReplicateMovement(false);
	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->InitBoxExtent(FVector(40.0f));
	Collision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	SetRootComponent(Collision);
	HarvestableNode = CreateDefaultSubobject<URpgHarvestableComponent>(TEXT("HarvestableNode"));
}

void URpgHarvestAutomationInstancesComponent::OnInstanceStockChanged_Implementation(
	const int32 InstanceIndex,
	const int32 RemainingSections,
	const int32 SectionCount,
	const bool bActive,
	const bool bInitialState)
{
	(void)SectionCount;
	++EventCount;
	LastInstanceIndex = InstanceIndex;
	LastRemainingSections = RemainingSections;
	bLastActive = bActive;
	bLastInitialState = bInitialState;
}

ARpgHarvestAutomationInstancesActor::ARpgHarvestAutomationInstancesActor(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = false;
	Instances = CreateDefaultSubobject<URpgHarvestAutomationInstancesComponent>(TEXT("Instances"));
	Instances->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	SetRootComponent(Instances);
}

URpgHarvestAutomationTestAbility::URpgHarvestAutomationTestAbility(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HarvestAbilityId = RpgHarvestingMagicGameplayTags::Ability_Harvesting_Manual;
}

void URpgHarvestAutomationPreviewListener::HandlePreviewChanged(const FRpgHarvestPreview& Preview)
{
	++EventCount;
	LastPreview = Preview;
}

ARpgHarvestAutomationProtectionActor::ARpgHarvestAutomationProtectionActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Protection = CreateDefaultSubobject<URpgHarvestProtectionComponent>(TEXT("Protection"));
	SetRootComponent(Protection);
}
