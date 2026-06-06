#include "RpgFeatureRegionActor.h"

#include "Components/GameFrameworkComponentManager.h"
#include "Components/SceneComponent.h"
#include "SurvivalRpg/GameFeatures/RpgFeatureEncounterPoint.h"
#include "SurvivalRpg/GameFeatures/RpgFeatureTagUtilities.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgFeatureRegionActor)

ARpgFeatureRegionActor::ARpgFeatureRegionActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);
}

void ARpgFeatureRegionActor::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
}

void ARpgFeatureRegionActor::BeginPlay()
{
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(this, UGameFrameworkComponentManager::NAME_GameActorReady);

	Super::BeginPlay();
}

void ARpgFeatureRegionActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);

	Super::EndPlay(EndPlayReason);
}

bool ARpgFeatureRegionActor::HasAllFeatureTags(const FGameplayTagContainer& RequiredTags) const
{
	return RpgFeatureTags::DoesContainerOnlyContainFeatureTags(FeatureTags)
		&& RpgFeatureTags::DoesContainerOnlyContainFeatureTags(RequiredTags)
		&& (RequiredTags.IsEmpty() || FeatureTags.HasAll(RequiredTags));
}

void ARpgFeatureRegionActor::ClearFeatureTags()
{
	FeatureTags.Reset();
}

void ARpgFeatureRegionActor::AddFeatureTagByName(FName TagName)
{
	if (!RpgFeatureTags::AddFeatureTagByName(FeatureTags, TagName))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s ignored non-feature or invalid tag '%s'."), *GetNameSafe(this), *TagName.ToString());
	}
}

void ARpgFeatureRegionActor::GetManagedEncounterPointsMatchingTags(const FGameplayTagContainer& RequiredTags, TArray<ARpgFeatureEncounterPoint*>& OutEncounterPoints) const
{
	OutEncounterPoints.Reset();

	for (ARpgFeatureEncounterPoint* EncounterPoint : ManagedEncounterPoints)
	{
		if (EncounterPoint && EncounterPoint->HasAllFeatureTags(RequiredTags))
		{
			OutEncounterPoints.Add(EncounterPoint);
		}
	}
}
