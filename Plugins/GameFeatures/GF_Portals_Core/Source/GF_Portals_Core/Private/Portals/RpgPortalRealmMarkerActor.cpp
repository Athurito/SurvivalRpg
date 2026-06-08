#include "Portals/RpgPortalRealmMarkerActor.h"

#include "Components/SceneComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPortalRealmMarkerActor)

ARpgPortalRealmMarkerActor::ARpgPortalRealmMarkerActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);
}
