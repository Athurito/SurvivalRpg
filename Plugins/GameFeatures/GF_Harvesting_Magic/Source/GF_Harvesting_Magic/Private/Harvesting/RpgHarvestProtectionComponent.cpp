#include "Harvesting/RpgHarvestProtectionComponent.h"

#include "Engine/CollisionProfile.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgHarvestProtectionComponent)

URpgHarvestProtectionComponent::URpgHarvestProtectionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	SetHiddenInGame(true);
	InitBoxExtent(FVector(500.0f, 500.0f, 400.0f));
	ShapeColor = FColor(80, 200, 255);
}

bool URpgHarvestProtectionComponent::ProtectsLocation(const FVector& WorldLocation) const
{
	// The inverse transform removes rotation and scale, so the unscaled extent describes the box.
	const FVector LocalLocation = GetComponentTransform().InverseTransformPosition(WorldLocation);
	const FVector Extent = GetUnscaledBoxExtent();
	return FMath::Abs(LocalLocation.X) <= Extent.X &&
		FMath::Abs(LocalLocation.Y) <= Extent.Y &&
		FMath::Abs(LocalLocation.Z) <= Extent.Z;
}

void URpgHarvestProtectionComponent::BeginPlay()
{
	Super::BeginPlay();
	if (URpgHarvestProtectionSubsystem* Protection = UWorld::GetSubsystem<URpgHarvestProtectionSubsystem>(GetWorld()))
	{
		Protection->RegisterZone(*this);
	}
}

void URpgHarvestProtectionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (URpgHarvestProtectionSubsystem* Protection = UWorld::GetSubsystem<URpgHarvestProtectionSubsystem>(GetWorld()))
	{
		Protection->UnregisterZone(*this);
	}
	Super::EndPlay(EndPlayReason);
}

bool URpgHarvestProtectionSubsystem::IsLocationProtected(const UWorld* World, const FVector& WorldLocation)
{
	const URpgHarvestProtectionSubsystem* Protection = UWorld::GetSubsystem<URpgHarvestProtectionSubsystem>(World);
	return Protection && Protection->IsProtected(WorldLocation);
}

bool URpgHarvestProtectionSubsystem::IsProtected(const FVector& WorldLocation) const
{
	for (const TWeakObjectPtr<const URpgHarvestProtectionComponent>& Zone : Zones)
	{
		if (const URpgHarvestProtectionComponent* ZoneComponent = Zone.Get();
			ZoneComponent && ZoneComponent->ProtectsLocation(WorldLocation))
		{
			return true;
		}
	}
	return false;
}

void URpgHarvestProtectionSubsystem::RegisterZone(const URpgHarvestProtectionComponent& Zone)
{
	Zones.RemoveAll([](const TWeakObjectPtr<const URpgHarvestProtectionComponent>& Existing)
	{
		return !Existing.IsValid();
	});
	Zones.AddUnique(&Zone);
}

void URpgHarvestProtectionSubsystem::UnregisterZone(const URpgHarvestProtectionComponent& Zone)
{
	Zones.RemoveAll([&Zone](const TWeakObjectPtr<const URpgHarvestProtectionComponent>& Existing)
	{
		return !Existing.IsValid() || Existing.Get() == &Zone;
	});
}
