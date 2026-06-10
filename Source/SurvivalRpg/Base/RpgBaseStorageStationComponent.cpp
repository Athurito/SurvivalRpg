#include "RpgBaseStorageStationComponent.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "RpgBaseCampActor.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgBaseStorageStationComponent)

URpgBaseStorageStationComponent::URpgBaseStorageStationComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void URpgBaseStorageStationComponent::BeginPlay()
{
	Super::BeginPlay();

	ApplyCapacityBonuses(1);
}

void URpgBaseStorageStationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ApplyCapacityBonuses(-1);

	Super::EndPlay(EndPlayReason);
}

void URpgBaseStorageStationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, LinkedBaseCamp);
	DOREPLIFETIME(ThisClass, bAccessible);
}

void URpgBaseStorageStationComponent::GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& InteractionBuilder)
{
	if (CanActorAccess(InteractQuery.RequestingAvatar.Get()))
	{
		InteractionBuilder.AddInteractionOption(OpenStationOption);
	}
}

URpgBaseStorageComponent* URpgBaseStorageStationComponent::GetBaseStorage() const
{
	return LinkedBaseCamp ? LinkedBaseCamp->GetBaseStorageComponent() : nullptr;
}

URpgInventoryManagerComponent* URpgBaseStorageStationComponent::GetArmoryInventory() const
{
	return LinkedBaseCamp ? LinkedBaseCamp->GetArmoryInventoryComponent() : nullptr;
}

bool URpgBaseStorageStationComponent::CanActorAccess(const AActor* RequestingActor) const
{
	const AActor* OwnerActor = GetOwner();
	if (!bAccessible || !LinkedBaseCamp || !OwnerActor || !RequestingActor)
	{
		return false;
	}

	const APawn* RequestingPawn = Cast<APawn>(RequestingActor);
	const AController* RequestingController = Cast<AController>(RequestingActor);
	if (!RequestingController && RequestingPawn)
	{
		RequestingController = RequestingPawn->GetController();
	}

	if (!RequestingController || !RequestingController->IsPlayerController())
	{
		return false;
	}

	if (InteractionRadius <= 0.0f)
	{
		return true;
	}

	return FVector::DistSquared(OwnerActor->GetActorLocation(), RequestingActor->GetActorLocation()) <= FMath::Square(InteractionRadius);
}

void URpgBaseStorageStationComponent::SetStationAccessible(bool bNewAccessible)
{
	if (AActor* OwnerActor = GetOwner(); OwnerActor && OwnerActor->HasAuthority())
	{
		bAccessible = bNewAccessible;
	}
}

void URpgBaseStorageStationComponent::ApplyCapacityBonuses(int32 Sign)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	if (Sign > 0 && bCapacityBonusesApplied)
	{
		return;
	}

	if (Sign < 0 && !bCapacityBonusesApplied)
	{
		return;
	}

	URpgBaseStorageComponent* BaseStorage = GetBaseStorage();
	if (!BaseStorage)
	{
		return;
	}

	for (const FRpgBaseResourceCapacity& Bonus : CapacityBonuses)
	{
		BaseStorage->AddResourceCapacity(Bonus.ItemDefinition, Bonus.Capacity * Sign);
	}

	bCapacityBonusesApplied = Sign > 0;
}
