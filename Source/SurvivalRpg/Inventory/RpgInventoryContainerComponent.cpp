#include "RpgInventoryContainerComponent.h"

#include "Net/UnrealNetwork.h"
#include "RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Interaction/Abilities/RpgGameplayAbility_OpenStorageContainer.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryContainerComponent)

URpgInventoryContainerComponent::URpgInventoryContainerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	OpenContainerOption.Text = NSLOCTEXT("RpgInventory", "OpenStorageContainerText", "Open");
	OpenContainerOption.SubText = NSLOCTEXT("RpgInventory", "OpenStorageContainerSubText", "Storage");
	OpenContainerOption.InteractionAbilityToGrant = URpgGameplayAbility_OpenStorageContainer::StaticClass();
}

void URpgInventoryContainerComponent::GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& InteractionBuilder)
{
	if (CanActorAccess(InteractQuery.RequestingAvatar.Get()))
	{
		InteractionBuilder.AddInteractionOption(OpenContainerOption);
	}
}

void URpgInventoryContainerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, bAccessible);
	DOREPLIFETIME(ThisClass, PersistentContainerId);
}

URpgInventoryManagerComponent* URpgInventoryContainerComponent::GetInventoryManager() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<URpgInventoryManagerComponent>() : nullptr;
}

bool URpgInventoryContainerComponent::CanActorAccess(const AActor* RequestingActor) const
{
	const AActor* OwnerActor = GetOwner();
	if (!bAccessible || OwnerActor == nullptr || RequestingActor == nullptr)
	{
		return false;
	}

	if (InteractionRadius <= 0.0f)
	{
		return true;
	}

	return FVector::DistSquared(OwnerActor->GetActorLocation(), RequestingActor->GetActorLocation()) <= FMath::Square(InteractionRadius);
}

void URpgInventoryContainerComponent::SetContainerAccessible(bool bNewAccessible)
{
	if (AActor* OwnerActor = GetOwner(); OwnerActor && OwnerActor->HasAuthority())
	{
		bAccessible = bNewAccessible;
	}
}
