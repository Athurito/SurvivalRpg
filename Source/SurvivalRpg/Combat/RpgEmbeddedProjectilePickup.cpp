#include "RpgEmbeddedProjectilePickup.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/Combat/RpgCombatProjectile.h"
#include "SurvivalRpg/Combat/RpgEmbeddedProjectileSubsystem.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Physics/RpgCollisionChannels.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgEmbeddedProjectilePickup)

namespace RpgEmbeddedProjectilePickup
{
	/** Radius in cm within which a player standing near the pickup can collect it without aiming at the thin shaft. */
	constexpr float InteractionRadius = 80.0f;
}

ARpgEmbeddedProjectilePickup::ARpgEmbeddedProjectilePickup(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Temporary loot: withdraw-only and closed to crafting scans, so it never registers for world saves either.
	if (URpgInventoryContainerComponent* Container = GetLootContainerComponent())
	{
		Container->ConfigureAsTemporaryLootContainer();
	}

	// The shaft never blocks movement, cameras, projectiles or other traces; only interaction focus finds it.
	DisplayMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DisplayMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	DisplayMesh->SetCollisionResponseToChannel(Rpg_TraceChannel_Interaction, ECR_Overlap);
	DisplayMesh->SetGenerateOverlapEvents(false);
	DisplayMesh->SetCanEverAffectNavigation(false);
	InteractionCollision->InitSphereRadius(RpgEmbeddedProjectilePickup::InteractionRadius);
	InteractionCollision->SetCanEverAffectNavigation(false);
}

bool ARpgEmbeddedProjectilePickup::CanEmbedIn(const UPrimitiveComponent* Host)
{
	const AActor* HostActor = Host ? Host->GetOwner() : nullptr;
	if (!HostActor || HostActor->IsA<ARpgWorldCollectable>())
	{
		return false;
	}
	return Host->Mobility != EComponentMobility::Movable
		|| ((HostActor->GetIsReplicated() || HostActor->IsFullNameStableForNetworking()) && Host->IsSupportedForNetworking());
}

ARpgEmbeddedProjectilePickup* ARpgEmbeddedProjectilePickup::RecoverAmmunition(
	UWorld* World,
	const TSubclassOf<ARpgEmbeddedProjectilePickup> PickupClass,
	const TSubclassOf<URpgInventoryItemDefinition> Ammunition,
	const FRpgProjectileRecoveryDefinition& Definition,
	const FRpgProjectileRecoveryPlacement& Placement)
{
	if (!World || World->GetNetMode() == NM_Client || !PickupClass || !Ammunition)
	{
		return nullptr;
	}

	const double Deadline = World->GetTimeSeconds() + FMath::Max(1.0f, Definition.RecoveryLifetimeSeconds);
	URpgEmbeddedProjectileSubsystem* Subsystem = World->GetSubsystem<URpgEmbeddedProjectileSubsystem>();
	if (Subsystem)
	{
		// A full host takes further projectiles into its nearest group: one more item, no new actor.
		ARpgEmbeddedProjectilePickup* Group = Subsystem->FindGroup(Placement.HostComponent.Get(),
			Placement.Transform.GetLocation(), Ammunition, Definition.MaxSeparatePickupsPerHost, Definition.GroupRadius);
		if (Group && Group->AddRecoveredAmmunition(1, Deadline))
		{
			return Group;
		}
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgEmbeddedProjectilePickup* Pickup = World->SpawnActor<ARpgEmbeddedProjectilePickup>(PickupClass, Placement.Transform, SpawnParameters);
	if (!Pickup)
	{
		return nullptr;
	}

	Pickup->Initialize(Ammunition, Placement, Deadline);
	if (!Pickup->MergePickupTemplate(Ammunition, 1))
	{
		Pickup->Destroy();
		return nullptr;
	}

	if (Subsystem)
	{
		Subsystem->RegisterPickup(Pickup);
	}
	return Pickup;
}

void ARpgEmbeddedProjectilePickup::Initialize(
	const TSubclassOf<URpgInventoryItemDefinition> Ammunition,
	const FRpgProjectileRecoveryPlacement& Placement,
	const double Deadline)
{
	AmmunitionDefinition = Ammunition;
	bEmbedded = Placement.bEmbedded;
	HostComponent = Placement.HostComponent;
	RecoveryDeadline = Deadline;

	if (bEmbedded && EmbedDepth > 0.0f)
	{
		SetActorLocation(GetActorLocation() + GetActorForwardVector() * EmbedDepth);
	}

	// A moving host carries the pickup along, a skeletal one at the hit bone. Every machine reconstructs the
	// attachment from its replication, also after a late join; a static host needs none.
	UPrimitiveComponent* Host = HostComponent.Get();
	if (Host && Host->Mobility == EComponentMobility::Movable)
	{
		AttachToComponent(Host, FAttachmentTransformRules::KeepWorldTransform, Placement.HostBone);
	}

	ScheduleExpiry();
}

bool ARpgEmbeddedProjectilePickup::AddRecoveredAmmunition(const int32 Count, const double Deadline)
{
	if (!HasAuthority() || IsActorBeingDestroyed() || Count <= 0 || !AmmunitionDefinition
		|| !MergePickupTemplate(AmmunitionDefinition, Count))
	{
		return false;
	}

	// The group lasts as long as its latest projectile.
	if (Deadline > RecoveryDeadline)
	{
		RecoveryDeadline = Deadline;
		ScheduleExpiry();
	}
	return true;
}

void ARpgEmbeddedProjectilePickup::ExpireRecovery()
{
	if (HasAuthority() && !IsActorBeingDestroyed())
	{
		Destroy();
	}
}

int32 ARpgEmbeddedProjectilePickup::GetAmmunitionCount() const
{
	const URpgInventoryManagerComponent* Inventory = GetLootInventoryManager();
	return Inventory && AmmunitionDefinition ? Inventory->GetTotalItemCountByDefinition(AmmunitionDefinition) : 0;
}

void ARpgEmbeddedProjectilePickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, AmmunitionDefinition, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ThisClass, bEmbedded, COND_InitialOnly);
}

void ARpgEmbeddedProjectilePickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExpiryTimer);
		if (URpgEmbeddedProjectileSubsystem* Subsystem = World->GetSubsystem<URpgEmbeddedProjectileSubsystem>())
		{
			Subsystem->UnregisterPickup(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

FInteractionOption ARpgEmbeddedProjectilePickup::BuildCollectInteractionOption(const FInteractionQuery& InteractQuery) const
{
	FInteractionOption Result = Super::BuildCollectInteractionOption(InteractQuery);
	const URpgInventoryItemDefinition* Definition = AmmunitionDefinition
		? GetDefault<URpgInventoryItemDefinition>(AmmunitionDefinition)
		: nullptr;
	const FText Name = Definition && !Definition->DisplayName.IsEmpty() ? Definition->DisplayName : Result.Prompt.TargetText;

	// A group shows how many items it carries.
	Result.Prompt.ActionText = NSLOCTEXT("RpgCombat", "CollectRecoveredAmmunition", "Collect");
	Result.Prompt.TargetText = FText::Format(NSLOCTEXT("RpgCombat", "RecoveredAmmunitionTarget", "{0} ({1})"),
		Name, FText::AsNumber(GetAmmunitionCount()));
	return Result;
}

void ARpgEmbeddedProjectilePickup::ScheduleExpiry()
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		return;
	}

	const float Remaining = FMath::Max(0.01f, static_cast<float>(RecoveryDeadline - World->GetTimeSeconds()));
	World->GetTimerManager().SetTimer(ExpiryTimer, FTimerDelegate::CreateUObject(this, &ThisClass::ExpireRecovery), Remaining, false);
}
