#include "RpgExperienceRewardComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

URpgExperienceRewardComponent::URpgExperienceRewardComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void URpgExperienceRewardComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(URpgExperienceRewardComponent, EnemyLevel);
}

void URpgExperienceRewardComponent::SetEnemyLevel(int32 NewEnemyLevel)
{
	AActor* Owner = GetOwner();
	if (Owner && !Owner->HasAuthority())
	{
		return;
	}

	EnemyLevel = FMath::Max(1, NewEnemyLevel);

	if (Owner)
	{
		Owner->ForceNetUpdate();
	}
}
