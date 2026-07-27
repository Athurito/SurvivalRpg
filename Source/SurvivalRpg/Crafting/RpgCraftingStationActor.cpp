#include "RpgCraftingStationActor.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "RpgCraftingStationComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgCraftingStationActor)

ARpgCraftingStationActor::ARpgCraftingStationActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
	SetReplicatingMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InteractionCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionCollision"));
	InteractionCollision->SetupAttachment(SceneRoot);
	InteractionCollision->InitSphereRadius(120.0f);
	InteractionCollision->SetCollisionProfileName(TEXT("Interactable_OverlapDynamic"));
	InteractionCollision->SetGenerateOverlapEvents(true);

	CraftingStationComponent = CreateDefaultSubobject<URpgCraftingStationComponent>(TEXT("CraftingStationComponent"));

	OutputInventoryComponent = CreateDefaultSubobject<URpgInventoryManagerComponent>(TEXT("OutputInventoryComponent"));
	CraftingStationComponent->SetOutputInventoryManager(OutputInventoryComponent);
}
