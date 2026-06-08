#include "RpgFeatureEncounterPoint.h"

#include "Components/SceneComponent.h"
#include "SurvivalRpg/GameFeatures/RpgFeatureTagUtilities.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgFeatureEncounterPoint)

ARpgFeatureEncounterPoint::ARpgFeatureEncounterPoint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);
}

bool ARpgFeatureEncounterPoint::HasAllFeatureTags(const FGameplayTagContainer& RequiredTags) const
{
	return RpgFeatureTags::DoesContainerOnlyContainFeatureTags(FeatureTags)
		&& RpgFeatureTags::DoesContainerOnlyContainFeatureTags(RequiredTags)
		&& (RequiredTags.IsEmpty() || FeatureTags.HasAll(RequiredTags));
}

void ARpgFeatureEncounterPoint::ClearFeatureTags()
{
	FeatureTags.Reset();
}

void ARpgFeatureEncounterPoint::AddFeatureTagByName(FName TagName)
{
	if (!RpgFeatureTags::AddFeatureTagByName(FeatureTags, TagName))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s ignored non-feature or invalid tag '%s'."), *GetNameSafe(this), *TagName.ToString());
	}
}
