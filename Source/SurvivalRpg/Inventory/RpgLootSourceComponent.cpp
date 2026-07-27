#include "RpgLootSourceComponent.h"

#include "RpgInventoryContainerComponent.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgLootSourceComponent)

URpgLootSourceComponent::URpgLootSourceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URpgLootSourceComponent::BeginPlay()
{
	Super::BeginPlay();

	BoundHealthComponent = URpgHealthComponent::FindHealthComponent(GetOwner());
	if (BoundHealthComponent)
	{
		BoundHealthComponent->OnDeathFinished.AddDynamic(this, &ThisClass::HandleDeathFinished);
	}

	if (bUnlockContainerOnDeath)
	{
		if (URpgInventoryContainerComponent* ContainerComponent = FindContainerComponent())
		{
			ContainerComponent->SetContainerAccessible(false);
		}
	}
}

void URpgLootSourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BoundHealthComponent)
	{
		BoundHealthComponent->OnDeathFinished.RemoveDynamic(this, &ThisClass::HandleDeathFinished);
		BoundHealthComponent = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void URpgLootSourceComponent::PopulateLoot()
{
	if (bLootPopulated)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	URpgInventoryManagerComponent* InventoryManager = FindInventoryManager();
	if (!InventoryManager)
	{
		return;
	}

	for (const FPickupTemplate& LootTemplate : LootTemplates)
	{
		if (LootTemplate.ItemDef && LootTemplate.StackCount > 0)
		{
			InventoryManager->GrantItemDefinition(LootTemplate.ItemDef, LootTemplate.StackCount);
		}
	}

	for (const FPickupInstance& LootInstance : LootInstances)
	{
		if (LootInstance.Item)
		{
			InventoryManager->BootstrapItemInstance(LootInstance.Item);
		}
	}

	bLootPopulated = true;
}

void URpgLootSourceComponent::HandleDeathFinished(AActor* OwningActor)
{
	PopulateLoot();

	if (bUnlockContainerOnDeath)
	{
		if (URpgInventoryContainerComponent* ContainerComponent = FindContainerComponent())
		{
			ContainerComponent->SetContainerAccessible(true);
		}
	}
}

URpgInventoryManagerComponent* URpgLootSourceComponent::FindInventoryManager() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<URpgInventoryManagerComponent>() : nullptr;
}

URpgInventoryContainerComponent* URpgLootSourceComponent::FindContainerComponent() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<URpgInventoryContainerComponent>() : nullptr;
}
