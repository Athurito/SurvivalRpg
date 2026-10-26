#include "RpgWeaponSwingRecorderComponent.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgWeaponSwingRecorderComponent)

URpgWeaponSwingRecorderComponent::URpgWeaponSwingRecorderComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	// After animation and attachment, so the samples hold the weapon's final pose of the frame.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetIsReplicatedByDefault(false);
}

void URpgWeaponSwingRecorderComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);
	}
}

void URpgWeaponSwingRecorderComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const UWorld* World = GetWorld();
	const USceneComponent* Root = GetOwner() ? GetOwner()->GetRootComponent() : nullptr;
	if (!World || !Root)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	Samples.Add({ Now, Root->GetComponentTransform() });
	int32 Expired = 0;
	while (Expired < Samples.Num() - 1 && Now - Samples[Expired].Time > HistorySeconds)
	{
		++Expired;
	}
	if (Expired > 0)
	{
		Samples.RemoveAt(0, Expired, EAllowShrinking::No);
	}

	if (Follower && AfterimageEndTime >= 0.0f)
	{
		FTransform Delayed;
		if (Now <= AfterimageEndTime && SampleTransform(Now - AfterimageDelay, Delayed))
		{
			Follower->SetWorldTransform(Delayed);
		}
		else if (Now > AfterimageEndTime)
		{
			AfterimageEndTime = -1.0f;
		}
	}
}

bool URpgWeaponSwingRecorderComponent::SampleTransform(const float WorldTime, FTransform& OutTransform) const
{
	if (Samples.IsEmpty())
	{
		return false;
	}
	if (WorldTime <= Samples[0].Time)
	{
		OutTransform = Samples[0].Transform;
		return true;
	}
	for (int32 Index = 1; Index < Samples.Num(); ++Index)
	{
		const FSample& Next = Samples[Index];
		if (WorldTime <= Next.Time)
		{
			const FSample& Previous = Samples[Index - 1];
			const float Span = FMath::Max(Next.Time - Previous.Time, UE_KINDA_SMALL_NUMBER);
			OutTransform.Blend(Previous.Transform, Next.Transform, FMath::Clamp((WorldTime - Previous.Time) / Span, 0.0f, 1.0f));
			return true;
		}
	}
	OutTransform = Samples.Last().Transform;
	return true;
}

USceneComponent* URpgWeaponSwingRecorderComponent::BeginAfterimage(const float Delay, const float Duration)
{
	AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();
	if (!Owner || !World || GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}

	if (!Follower)
	{
		// Not attached: it moves in world space along the delayed history, independent of the live weapon.
		Follower = NewObject<USceneComponent>(Owner, TEXT("AfterimageFollower"), RF_Transient);
		Follower->SetUsingAbsoluteLocation(true);
		Follower->SetUsingAbsoluteRotation(true);
		Follower->SetUsingAbsoluteScale(true);
		Follower->RegisterComponent();
	}

	AfterimageDelay = FMath::Max(0.0f, Delay);
	AfterimageEndTime = World->GetTimeSeconds() + FMath::Max(0.0f, Duration);
	FTransform Delayed;
	if (SampleTransform(World->GetTimeSeconds() - AfterimageDelay, Delayed))
	{
		Follower->SetWorldTransform(Delayed);
	}
	return Follower;
}
