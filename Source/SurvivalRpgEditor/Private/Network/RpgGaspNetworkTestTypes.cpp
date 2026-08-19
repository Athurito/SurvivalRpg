#include "Network/RpgGaspNetworkTestTypes.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGaspNetworkTestTypes)

ARpgGaspNetworkFloorFixture::ARpgGaspNetworkFloorFixture(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetMobility(EComponentMobility::Static);
	SetRootComponent(SceneRoot);

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->SetupAttachment(SceneRoot);
	Collision->SetMobility(EComponentMobility::Static);
	Collision->SetRelativeLocation(FVector(0.0, 0.0, -50.0));
	Collision->InitBoxExtent(FVector(50000.0, 50000.0, 50.0));
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Collision->SetCanEverAffectNavigation(false);
}
