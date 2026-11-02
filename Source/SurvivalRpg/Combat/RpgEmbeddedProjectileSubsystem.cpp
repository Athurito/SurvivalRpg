#include "RpgEmbeddedProjectileSubsystem.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "SurvivalRpg/Combat/RpgEmbeddedProjectilePickup.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgEmbeddedProjectileSubsystem)

namespace RpgEmbeddedProjectileSubsystem
{
	bool IsLive(const ARpgEmbeddedProjectilePickup* Pickup)
	{
		return IsValid(Pickup) && !Pickup->IsActorBeingDestroyed();
	}
}

bool URpgEmbeddedProjectileSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void URpgEmbeddedProjectileSubsystem::RegisterPickup(ARpgEmbeddedProjectilePickup* Pickup)
{
	if (!RpgEmbeddedProjectileSubsystem::IsLive(Pickup) || !Pickup->HasAuthority())
	{
		return;
	}

	Pickups.RemoveAll([](const TWeakObjectPtr<ARpgEmbeddedProjectilePickup>& Entry)
	{
		return !RpgEmbeddedProjectileSubsystem::IsLive(Entry.Get());
	});
	Pickups.AddUnique(Pickup);

	// The cap only ever removes older pickups: the one just placed always stays.
	const int32 MaxPickups = FMath::Max(1, MaxWorldArrowPickups);
	while (Pickups.Num() > MaxPickups)
	{
		int32 OldestIndex = INDEX_NONE;
		for (int32 Index = 0; Index < Pickups.Num(); ++Index)
		{
			const ARpgEmbeddedProjectilePickup* Candidate = Pickups[Index].Get();
			if (Candidate != Pickup && (OldestIndex == INDEX_NONE
				|| Candidate->GetRecoveryDeadline() < Pickups[OldestIndex]->GetRecoveryDeadline()))
			{
				OldestIndex = Index;
			}
		}
		if (OldestIndex == INDEX_NONE)
		{
			break;
		}

		ARpgEmbeddedProjectilePickup* Oldest = Pickups[OldestIndex].Get();
		Pickups.RemoveAt(OldestIndex);
		Oldest->ExpireRecovery();
	}
}

void URpgEmbeddedProjectileSubsystem::UnregisterPickup(ARpgEmbeddedProjectilePickup* Pickup)
{
	Pickups.RemoveAll([Pickup](const TWeakObjectPtr<ARpgEmbeddedProjectilePickup>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Pickup;
	});
}

ARpgEmbeddedProjectilePickup* URpgEmbeddedProjectileSubsystem::FindGroup(
	const UPrimitiveComponent* Host,
	const FVector& Location,
	const TSubclassOf<URpgInventoryItemDefinition> Ammunition,
	const int32 MaxSeparatePerHost,
	const float GroupRadius) const
{
	if (!Host || !Ammunition)
	{
		return nullptr;
	}

	int32 PickupsOnHost = 0;
	ARpgEmbeddedProjectilePickup* Nearest = nullptr;
	float NearestDistanceSquared = FMath::Square(FMath::Max(0.0f, GroupRadius));
	for (const TWeakObjectPtr<ARpgEmbeddedProjectilePickup>& Entry : Pickups)
	{
		ARpgEmbeddedProjectilePickup* Pickup = Entry.Get();
		if (!RpgEmbeddedProjectileSubsystem::IsLive(Pickup) || Pickup->GetHostComponent() != Host)
		{
			continue;
		}

		++PickupsOnHost;
		const float DistanceSquared = FVector::DistSquared(Pickup->GetActorLocation(), Location);
		if (Pickup->GetAmmunitionDefinition() == Ammunition && DistanceSquared <= NearestDistanceSquared)
		{
			Nearest = Pickup;
			NearestDistanceSquared = DistanceSquared;
		}
	}

	return PickupsOnHost >= FMath::Max(1, MaxSeparatePerHost) ? Nearest : nullptr;
}

int32 URpgEmbeddedProjectileSubsystem::GetPickupCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<ARpgEmbeddedProjectilePickup>& Entry : Pickups)
	{
		Count += RpgEmbeddedProjectileSubsystem::IsLive(Entry.Get()) ? 1 : 0;
	}
	return Count;
}
