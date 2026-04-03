#include "RpgItemPickup.h"

#include "Engine/ActorChannel.h"
#include "Net/UnrealNetwork.h"
#include "Components/SceneComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentComponent.h"
#include "SurvivalRpg/Items/RpgItemDefinition.h"
#include "SurvivalRpg/Items/RpgItemInstance.h"

ARpgItemPickup::ARpgItemPickup()
{
	bReplicates = true;
	SetReplicateMovement(true);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void ARpgItemPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, ItemInstance);
}

bool ARpgItemPickup::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool bWroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);
	if (ItemInstance != nullptr)
	{
		bWroteSomething |= Channel->ReplicateSubobject(ItemInstance, *Bunch, *RepFlags);
	}
	return bWroteSomething;
}

void ARpgItemPickup::InitializeFromDefinition(URpgItemDefinition* ItemDefinition, const FRpgItemSourceHandle& SourceHandle)
{
	if (ItemDefinition == nullptr)
	{
		return;
	}

	URpgItemInstance* NewInstance = NewObject<URpgItemInstance>(this);
	NewInstance->InitializeItemInstance(ItemDefinition, SourceHandle);
	SetItemInstance(NewInstance);
}

void ARpgItemPickup::SetItemInstance(URpgItemInstance* NewItemInstance)
{
	if (NewItemInstance == nullptr)
	{
		ItemInstance = nullptr;
		return;
	}

	ItemInstance = (NewItemInstance->GetOuter() == this)
		? NewItemInstance
		: NewItemInstance->DuplicateItemInstance(this);
}

URpgItemInstance* ARpgItemPickup::ClaimPickupToEquipment(URpgEquipmentComponent* EquipmentComponent, bool bAutoEquip)
{
	if (!HasAuthority() || EquipmentComponent == nullptr || ItemInstance == nullptr)
	{
		return nullptr;
	}

	URpgItemInstance* ClaimedInstance = EquipmentComponent->RegisterExistingItemInstance(ItemInstance);
	if (ClaimedInstance == nullptr)
	{
		return nullptr;
	}

	if (bAutoEquip)
	{
		EquipmentComponent->TryAutoEquipItem(ClaimedInstance);
	}

	Destroy();
	return ClaimedInstance;
}
