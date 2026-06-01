#include "RpgEquipmentInstance.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "RpgEquipmentDefinition.h"

#include "Components/SceneComponent.h"
#include "Iris/ReplicationSystem/ReplicationFragmentUtil.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgEquipmentInstance)

URpgEquipmentInstance::URpgEquipmentInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UWorld* URpgEquipmentInstance::GetWorld() const
{
	if (APawn* OwningPawn = GetPawn())
	{
		return OwningPawn->GetWorld();
	}

	return nullptr;
}

void URpgEquipmentInstance::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, Instigator);
	DOREPLIFETIME(ThisClass, EquippedSlot);
	DOREPLIFETIME(ThisClass, SpawnedActors);
}

void URpgEquipmentInstance::RegisterReplicationFragments(UE::Net::FFragmentRegistrationContext& Context, UE::Net::EFragmentRegistrationFlags RegistrationFlags)
{
	UE::Net::FReplicationFragmentUtil::CreateAndRegisterFragmentsForObject(this, Context, RegistrationFlags);
}

APawn* URpgEquipmentInstance::GetPawn() const
{
	return Cast<APawn>(GetOuter());
}

APawn* URpgEquipmentInstance::GetTypedPawn(TSubclassOf<APawn> PawnType) const
{
	if (UClass* ActualPawnType = PawnType)
	{
		return GetOuter() && GetOuter()->IsA(ActualPawnType) ? Cast<APawn>(GetOuter()) : nullptr;
	}

	return nullptr;
}

void URpgEquipmentInstance::SpawnEquipmentActors(const TArray<FRpgEquipmentActorToSpawn>& ActorsToSpawn)
{
	APawn* OwningPawn = GetPawn();
	UWorld* World = GetWorld();
	if (OwningPawn == nullptr || World == nullptr)
	{
		return;
	}

	USceneComponent* AttachTarget = OwningPawn->GetRootComponent();
	if (const ACharacter* Character = Cast<ACharacter>(OwningPawn))
	{
		AttachTarget = Character->GetMesh();
	}

	for (const FRpgEquipmentActorToSpawn& SpawnInfo : ActorsToSpawn)
	{
		if (SpawnInfo.ActorToSpawn == nullptr || AttachTarget == nullptr)
		{
			continue;
		}

		AActor* NewActor = World->SpawnActorDeferred<AActor>(SpawnInfo.ActorToSpawn, FTransform::Identity, OwningPawn);
		if (NewActor == nullptr)
		{
			continue;
		}

		NewActor->FinishSpawning(FTransform::Identity, true);

		TInlineComponentArray<USceneComponent*> SceneComponents;
		NewActor->GetComponents(SceneComponents);
		for (USceneComponent* SceneComponent : SceneComponents)
		{
			if (SceneComponent)
			{
				SceneComponent->SetMobility(EComponentMobility::Movable);
			}
		}

		NewActor->SetActorRelativeTransform(SpawnInfo.AttachTransform);
		NewActor->AttachToComponent(AttachTarget, FAttachmentTransformRules::KeepRelativeTransform, SpawnInfo.GetAttachSocketForSlot(EquippedSlot));
		SpawnedActors.Add(NewActor);
	}
}

void URpgEquipmentInstance::DestroyEquipmentActors()
{
	for (AActor* Actor : SpawnedActors)
	{
		if (Actor != nullptr)
		{
			Actor->Destroy();
		}
	}

	SpawnedActors.Reset();
}

void URpgEquipmentInstance::OnEquipped()
{
	K2_OnEquipped();
}

void URpgEquipmentInstance::OnUnequipped()
{
	K2_OnUnequipped();
}

void URpgEquipmentInstance::OnRep_Instigator()
{
}
