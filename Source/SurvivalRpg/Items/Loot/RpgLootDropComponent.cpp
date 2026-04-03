#include "RpgLootDropComponent.h"

#include "RpgItemPickup.h"
#include "RpgLootTable.h"
#include "Engine/World.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/Items/RpgItemDefinition.h"
#include "SurvivalRpg/Items/Fragments/RpgItemFragment_Loot.h"

URpgLootDropComponent::URpgLootDropComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URpgLootDropComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!bDropOnOwnerDeath || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (URpgHealthComponent* HealthComponent = URpgHealthComponent::FindHealthComponent(GetOwner()))
	{
		HealthComponent->OnDeathFinished.AddDynamic(this, &ThisClass::HandleOwnerDeath);
	}
}

TArray<ARpgItemPickup*> URpgLootDropComponent::DropLootAtLocation(const FVector& DropLocation, const FRotator& DropRotation)
{
	TArray<ARpgItemPickup*> SpawnedPickups;
	if (!GetOwner() || !GetOwner()->HasAuthority() || LootTable == nullptr)
	{
		return SpawnedPickups;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return SpawnedPickups;
	}

	FRandomStream RandomStream(FMath::Rand());
	const TArray<TObjectPtr<URpgItemDefinition>> RolledDefinitions = LootTable->RollItemDefinitions(RandomStream);

	for (int32 ItemIndex = 0; ItemIndex < RolledDefinitions.Num(); ++ItemIndex)
	{
		URpgItemDefinition* ItemDefinition = RolledDefinitions[ItemIndex];
		if (ItemDefinition == nullptr)
		{
			continue;
		}

		TSubclassOf<ARpgItemPickup> PickupClass = DefaultPickupActorClass;
		if (const URpgItemFragment_Loot* LootFragment = ItemDefinition->FindFragmentByClass<URpgItemFragment_Loot>())
		{
			if (LootFragment->GetPickupActorClass() != nullptr)
			{
				PickupClass = LootFragment->GetPickupActorClass();
			}
		}

		if (PickupClass == nullptr)
		{
			PickupClass = ARpgItemPickup::StaticClass();
		}

		const FVector RandomOffset(RandomStream.FRandRange(-30.0f, 30.0f), RandomStream.FRandRange(-30.0f, 30.0f), 0.0f);
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = GetOwner();
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		ARpgItemPickup* PickupActor = World->SpawnActor<ARpgItemPickup>(PickupClass, DropLocation + RandomOffset, DropRotation, SpawnParameters);
		if (PickupActor == nullptr)
		{
			continue;
		}

		PickupActor->InitializeFromDefinition(ItemDefinition, FRpgItemSourceHandle());
		SpawnedPickups.Add(PickupActor);
	}

	return SpawnedPickups;
}

void URpgLootDropComponent::HandleOwnerDeath(AActor* OwningActor)
{
	if (OwningActor != nullptr)
	{
		DropLootAtLocation(OwningActor->GetActorLocation(), OwningActor->GetActorRotation());
	}
}
