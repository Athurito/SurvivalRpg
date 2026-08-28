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

ARpgGaspNetworkMovingBaseFixture::ARpgGaspNetworkMovingBaseFixture(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->SetMobility(EComponentMobility::Movable);
	Collision->InitBoxExtent(FVector(5000.0f, 5000.0f, 25.0f));
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Collision->SetCanEverAffectNavigation(false);
	SetRootComponent(Collision);
}

void ARpgGaspNetworkMovingBaseFixture::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || !bMotionEnabled)
	{
		return;
	}

	MotionTime += FMath::Max(static_cast<double>(DeltaSeconds), 0.0);
	const FVector Translation(
		MotionTime * 1200.0,
		150.0 * FMath::Sin(MotionTime * UE_PI),
		0.0);
	const FRotator Rotation(0.0, MotionTime * 45.0, 0.0);
	SetActorLocationAndRotation(
		MotionOrigin + Translation,
		Rotation,
		false,
		nullptr,
		ETeleportType::None);
	ForceNetUpdate();
}

void ARpgGaspNetworkMovingBaseFixture::StartMotion()
{
	if (!HasAuthority())
	{
		return;
	}

	MotionOrigin = GetActorLocation();
	MotionTime = 0.0;
	bMotionEnabled = true;
	SetActorTickEnabled(true);
}

void ARpgGaspNetworkMovingBaseFixture::StopMotion()
{
	bMotionEnabled = false;
	SetActorTickEnabled(false);
	ForceNetUpdate();
}
