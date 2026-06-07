#include "RpgPortalEncounterComponent.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "SurvivalRpg/GameFeatures/RpgFeatureEncounterPoint.h"
#include "SurvivalRpg/GameFeatures/RpgFeatureRegionActor.h"
#include "SurvivalRpg/GameFeatures/RpgFeatureTagUtilities.h"
#include "SurvivalRpg/Portals/RpgPortalActor.h"
#include "SurvivalRpg/Portals/RpgPortalEncounterDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPortalEncounterComponent)

DEFINE_LOG_CATEGORY_STATIC(LogRpgPortalEncounterComponent, Log, All);

URpgPortalEncounterComponent::URpgPortalEncounterComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void URpgPortalEncounterComponent::OnRegister()
{
	Super::OnRegister();

	if (const AActor* Owner = GetOwner(); Owner && Owner->HasActorBegunPlay())
	{
		TrySpawnPortalEncounters();
	}
}

void URpgPortalEncounterComponent::BeginPlay()
{
	Super::BeginPlay();

	TrySpawnPortalEncounters();
}

void URpgPortalEncounterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroySpawnedPortals();

	Super::EndPlay(EndPlayReason);
}

void URpgPortalEncounterComponent::TrySpawnPortalEncounters()
{
	if (bSpawnAttempted)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	bSpawnAttempted = true;

	ARpgFeatureRegionActor* RegionActor = Cast<ARpgFeatureRegionActor>(Owner);
	if (!RegionActor)
	{
		UE_LOG(LogRpgPortalEncounterComponent, Warning, TEXT("%s can only spawn portal encounters on ARpgFeatureRegionActor owners."), *GetNameSafe(this));
		return;
	}

	if (SpawnRules.IsEmpty())
	{
		UE_LOG(LogRpgPortalEncounterComponent, Warning, TEXT("%s has no portal spawn rules."), *GetNameSafe(this));
		return;
	}

	TSet<TWeakObjectPtr<ARpgFeatureEncounterPoint>> ClaimedEncounterPoints;
	for (const FRpgPortalEncounterSpawnRule& SpawnRule : SpawnRules)
	{
		if (!RpgFeatureTags::DoesContainerOnlyContainFeatureTags(SpawnRule.RequiredRegionTags)
			|| !RpgFeatureTags::DoesContainerOnlyContainFeatureTags(SpawnRule.RequiredPointTags))
		{
			UE_LOG(LogRpgPortalEncounterComponent, Warning, TEXT("%s skipped portal spawn rule with non-feature required tags."), *GetNameSafe(this));
			continue;
		}

		if (!RegionActor->HasAllFeatureTags(SpawnRule.RequiredRegionTags))
		{
			continue;
		}

		if (SpawnRule.PortalActorClass.IsNull() || SpawnRule.EncounterDefinition.IsNull())
		{
			UE_LOG(LogRpgPortalEncounterComponent, Warning, TEXT("%s has an incomplete portal spawn rule."), *GetNameSafe(this));
			continue;
		}

		for (ARpgFeatureEncounterPoint* EncounterPoint : RegionActor->GetManagedEncounterPoints())
		{
			if (!EncounterPoint || ClaimedEncounterPoints.Contains(EncounterPoint) || !EncounterPoint->HasAllFeatureTags(SpawnRule.RequiredPointTags))
			{
				continue;
			}

			if (ARpgPortalActor* SpawnedPortal = SpawnPortalForPoint(*RegionActor, *EncounterPoint, SpawnRule, SpawnedPortals.Num()))
			{
				SpawnedPortals.Add(SpawnedPortal);
				ClaimedEncounterPoints.Add(EncounterPoint);
			}
		}
	}
}

void URpgPortalEncounterComponent::DestroySpawnedPortals()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	for (ARpgPortalActor* SpawnedPortal : SpawnedPortals)
	{
		if (IsValid(SpawnedPortal))
		{
			SpawnedPortal->Destroy();
		}
	}

	SpawnedPortals.Reset();
}

void URpgPortalEncounterComponent::ConfigureSingleSpawnRuleByTagNames(
	FName RequiredRegionTagName,
	FName RequiredPointTagName,
	TSubclassOf<ARpgPortalActor> InPortalActorClass,
	URpgPortalEncounterDefinition* InEncounterDefinition)
{
	FRpgPortalEncounterSpawnRule SpawnRule;

	if (!RpgFeatureTags::AddFeatureTagByName(SpawnRule.RequiredRegionTags, RequiredRegionTagName))
	{
		UE_LOG(LogRpgPortalEncounterComponent, Warning, TEXT("%s refused portal spawn rule with non-feature or invalid region tag '%s'."), *GetNameSafe(this), *RequiredRegionTagName.ToString());
		return;
	}

	if (!RpgFeatureTags::AddFeatureTagByName(SpawnRule.RequiredPointTags, RequiredPointTagName))
	{
		UE_LOG(LogRpgPortalEncounterComponent, Warning, TEXT("%s refused portal spawn rule with non-feature or invalid point tag '%s'."), *GetNameSafe(this), *RequiredPointTagName.ToString());
		return;
	}

	SpawnRule.PortalActorClass = InPortalActorClass;
	SpawnRule.EncounterDefinition = InEncounterDefinition;

	SpawnRules.Reset();
	SpawnRules.Add(SpawnRule);
}

ARpgPortalActor* URpgPortalEncounterComponent::SpawnPortalForPoint(ARpgFeatureRegionActor& RegionActor, ARpgFeatureEncounterPoint& EncounterPoint, const FRpgPortalEncounterSpawnRule& SpawnRule, int32 PortalIndex)
{
	UWorld* World = RegionActor.GetWorld();
	if (!World)
	{
		return nullptr;
	}

	TSubclassOf<ARpgPortalActor> PortalClass = SpawnRule.PortalActorClass.LoadSynchronous();
	URpgPortalEncounterDefinition* EncounterDefinition = SpawnRule.EncounterDefinition.LoadSynchronous();
	if (!PortalClass || !EncounterDefinition)
	{
		UE_LOG(LogRpgPortalEncounterComponent, Warning, TEXT("%s failed to load portal class or encounter definition for point %s."), *GetNameSafe(this), *GetNameSafe(&EncounterPoint));
		return nullptr;
	}

	const FTransform SpawnTransform = SpawnRule.SpawnTransformOffset * EncounterPoint.GetEncounterTransform();

	ARpgPortalActor* PortalActor = World->SpawnActorDeferred<ARpgPortalActor>(
		PortalClass,
		SpawnTransform,
		&RegionActor,
		RegionActor.GetInstigator(),
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

	if (!PortalActor)
	{
		UE_LOG(LogRpgPortalEncounterComponent, Warning, TEXT("%s failed to spawn portal at point %s."), *GetNameSafe(this), *GetNameSafe(&EncounterPoint));
		return nullptr;
	}

	PortalActor->ConfigureEncounterDefinition(EncounterDefinition);
	PortalActor->ConfigureDungeonLevelInstanceTransform(BuildDungeonLevelInstanceTransform(RegionActor, EncounterPoint, PortalIndex));
	UGameplayStatics::FinishSpawningActor(PortalActor, SpawnTransform);

	return PortalActor;
}

FTransform URpgPortalEncounterComponent::BuildDungeonLevelInstanceTransform(const ARpgFeatureRegionActor& RegionActor, const ARpgFeatureEncounterPoint& EncounterPoint, int32 PortalIndex) const
{
	const FVector RegionLocation = RegionActor.GetActorLocation();
	const FVector PointLocation = EncounterPoint.GetActorLocation();
	const double SafeGridSize = FMath::Max(1000.0, DungeonPocketRegionGridSize);
	const double SafeRegionStride = FMath::Max(1000.0, DungeonPocketRegionStride);
	const double SafePortalSpacing = FMath::Max(1000.0, DungeonPocketPortalSpacing);

	const int32 RegionGridX = FMath::FloorToInt(RegionLocation.X / SafeGridSize);
	const int32 RegionGridY = FMath::FloorToInt(RegionLocation.Y / SafeGridSize);
	const int32 PointGridX = FMath::FloorToInt(PointLocation.X / SafeGridSize);
	const int32 PointGridY = FMath::FloorToInt(PointLocation.Y / SafeGridSize);

	const FVector PocketLocation(
		DungeonPocketBaseLocation.X + static_cast<double>(RegionGridX) * SafeRegionStride + static_cast<double>(PortalIndex) * SafePortalSpacing,
		DungeonPocketBaseLocation.Y + static_cast<double>(RegionGridY) * SafeRegionStride + static_cast<double>(PointGridX - RegionGridX) * SafePortalSpacing,
		DungeonPocketBaseLocation.Z + static_cast<double>(PointGridY - RegionGridY) * SafePortalSpacing);

	return FTransform(FRotator::ZeroRotator, PocketLocation);
}
