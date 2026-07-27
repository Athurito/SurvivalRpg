// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgWorldCollectable.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Interaction/Abilities/RpgGameplayAbility_Collect.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"

namespace
{
	FText ResolvePickupDisplayName(const FInventoryPickup& Pickup)
	{
		for (const FPickupInstance& Instance : Pickup.Instances)
		{
			const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition = Instance.Item
				? Instance.Item->GetItemDef()
				: TSubclassOf<URpgInventoryItemDefinition>();
			if (const URpgInventoryItemDefinition* Definition =
				ItemDefinition ? GetDefault<URpgInventoryItemDefinition>(ItemDefinition) : nullptr;
				Definition && !Definition->DisplayName.IsEmpty())
			{
				return Definition->DisplayName;
			}
		}

		for (const FPickupTemplate& Template : Pickup.Templates)
		{
			if (const URpgInventoryItemDefinition* Definition =
				Template.ItemDef ? GetDefault<URpgInventoryItemDefinition>(Template.ItemDef) : nullptr;
				Definition && !Definition->DisplayName.IsEmpty())
			{
				return Definition->DisplayName;
			}
		}

		return NSLOCTEXT("RpgInteraction", "CollectableFallbackTarget", "Item");
	}
}


ARpgWorldCollectable::ARpgWorldCollectable()
{
	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);
	bReplicates = true;
	InteractionCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionCollision"));
	InteractionCollision->SetupAttachment(SceneRoot);
	InteractionCollision->InitSphereRadius(120.0f);
	InteractionCollision->SetCollisionProfileName(TEXT("Interactable_OverlapDynamic"));
	InteractionCollision->SetGenerateOverlapEvents(true);

	DisplayMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayMesh"));
	DisplayMesh->SetupAttachment(SceneRoot);
	DisplayMesh->SetCollisionProfileName(TEXT("Interactable_BlockDynamic"));

	Option.InteractionTag = RpgGameplayTags::Rpg_Interaction_Action_Collect;
	Option.Prompt.ActionText = NSLOCTEXT("RpgInteraction", "CollectInteractionText", "Pick Up");
	Option.Prompt.TargetText = NSLOCTEXT("RpgInteraction", "CollectableFallbackTarget", "Item");
	Option.Prompt.InteractionPriority = 20;
	Option.InteractionAbilityToGrant = URpgGameplayAbility_Collect::StaticClass();
}

void ARpgWorldCollectable::GatherInteractionOptions(const FInteractionQuery& InteractQuery,
                                                    FInteractionOptionBuilder& InteractionBuilder)
{
	InteractionBuilder.AddInteractionOption(BuildCollectInteractionOption(InteractQuery));
}

FInteractionOption ARpgWorldCollectable::BuildCollectInteractionOption(const FInteractionQuery& InteractQuery) const
{
	(void)InteractQuery;

	FInteractionOption Result = Option;
	Result.InteractionTag = RpgGameplayTags::Rpg_Interaction_Action_Collect;
	Result.TargetRef.TargetActor = const_cast<ARpgWorldCollectable*>(this);
	Result.Availability = ERpgInteractionAvailability::Available;
	if (Result.Prompt.ActionText.IsEmpty())
	{
		Result.Prompt.ActionText = NSLOCTEXT("RpgInteraction", "CollectInteractionText", "Pick Up");
	}
	Result.Prompt.TargetText = ResolvePickupDisplayName(GetPickupInventory());
	return Result;
}

FInventoryPickup ARpgWorldCollectable::GetPickupInventory() const
{
	return StaticInventory;
}
