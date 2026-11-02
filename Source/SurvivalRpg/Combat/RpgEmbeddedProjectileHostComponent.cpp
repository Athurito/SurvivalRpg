#include "RpgEmbeddedProjectileHostComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/Combat/RpgCombatProjectile.h"
#include "SurvivalRpg/Combat/RpgEmbeddedProjectilePickup.h"
#include "SurvivalRpg/Core/Corpse/RpgCorpseLifecycleComponent.h"
#include "SurvivalRpg/Inventory/IPickupable.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgLootSourceComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgEmbeddedProjectileHostComponent)

namespace RpgEmbeddedProjectileHost
{
	/** Length in cm along the flight behind a capsule contact in which the shaft looks for the body. */
	constexpr float BodySearchLength = 100.0f;
}

URpgEmbeddedProjectileHostComponent::URpgEmbeddedProjectileHostComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void URpgEmbeddedProjectileHostComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, Groups);
}

void URpgEmbeddedProjectileHostComponent::BeginPlay()
{
	Super::BeginPlay();

	BindCorpseLoot();
	RefreshVisuals();
}

void URpgEmbeddedProjectileHostComponent::BindCorpseLoot()
{
	if (bCorpseLootBound || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	URpgLootSourceComponent* LootSource = FindLootSource();
	URpgInventoryManagerComponent* Inventory = FindCorpseInventory();
	if (!LootSource || !Inventory)
	{
		return;
	}
	bCorpseLootBound = true;
	LootSource->OnGatherSupplementalLoot.AddUObject(this, &ThisClass::HandleGatherLoot);
	LootSource->OnLootPopulationCompleted.AddUObject(this, &ThisClass::HandleLootPopulated);
	Inventory->OnInventoryPostCommit.AddUObject(this, &ThisClass::HandleInventoryCommitted);
}

void URpgEmbeddedProjectileHostComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (URpgLootSourceComponent* LootSource = FindLootSource())
	{
		LootSource->OnGatherSupplementalLoot.RemoveAll(this);
		LootSource->OnLootPopulationCompleted.RemoveAll(this);
	}
	if (URpgInventoryManagerComponent* Inventory = FindCorpseInventory())
	{
		Inventory->OnInventoryPostCommit.RemoveAll(this);
	}
	// A character that goes keeps no pending state, and nothing drops: its records and shafts go with it.
	Records.Reset();
	bCorpseLootBound = false;
	for (const TPair<int32, TObjectPtr<UStaticMeshComponent>>& Visual : Visuals)
	{
		if (Visual.Value)
		{
			Visual.Value->DestroyComponent();
		}
	}
	Visuals.Reset();
	Super::EndPlay(EndPlayReason);
}

bool URpgEmbeddedProjectileHostComponent::CanRecover() const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || Owner->IsActorBeingDestroyed() || !FindLootSource() || !FindCorpseInventory())
	{
		return false;
	}
	// A completed or expiring corpse is about to go: it takes no new credits.
	const URpgCorpseLifecycleComponent* Corpse = Owner->FindComponentByClass<URpgCorpseLifecycleComponent>();
	return !Corpse || Corpse->GetLifecycleState() < ERpgCorpseLifecycleState::Completed;
}

int32 URpgEmbeddedProjectileHostComponent::RecoverProjectile(
	const FHitResult& Hit,
	const FVector& FlightDirection,
	const TSubclassOf<URpgInventoryItemDefinition> Ammunition,
	const FRpgProjectileRecoveryDefinition& Definition)
{
	if (!Ammunition || !CanRecover())
	{
		return 0;
	}
	// Bound here as well, for a host that records before its owner began play.
	BindCorpseLoot();

	// The shaft sits on the body, not on the capsule: on the hit bone, or on the bone nearest the contact, where the
	// flight passes closest to it.
	USkeletalMeshComponent* Mesh = FindMesh();
	const FVector Forward = FlightDirection.IsNearlyZero() ? -Hit.ImpactNormal : FlightDirection.GetSafeNormal();
	FName Bone = NAME_None;
	FVector Contact = Hit.ImpactPoint;
	if (Mesh && Hit.GetComponent() == Mesh && !Hit.BoneName.IsNone())
	{
		Bone = Hit.BoneName;
	}
	else if (Mesh)
	{
		FVector BoneLocation = FVector::ZeroVector;
		Bone = Mesh->FindClosestBone(Hit.ImpactPoint, &BoneLocation, 0.0f, true);
		if (Bone.IsNone())
		{
			Bone = Mesh->FindClosestBone(Hit.ImpactPoint, &BoneLocation, 0.0f, false);
		}
		if (!Bone.IsNone())
		{
			Contact = FMath::ClosestPointOnSegment(BoneLocation, Hit.ImpactPoint, Hit.ImpactPoint + Forward * RpgEmbeddedProjectileHost::BodySearchLength);
		}
		else if (Mesh->GetNumBones() > 0)
		{
			// Without posed bones the shaft still follows the skeleton from its root.
			Bone = Mesh->GetBoneName(0);
		}
	}
	const ARpgEmbeddedProjectilePickup* VisualDefaults = Definition.PickupClass ? Definition.PickupClass->GetDefaultObject<ARpgEmbeddedProjectilePickup>() : nullptr;
	Contact += Forward * (VisualDefaults ? VisualDefaults->GetEmbedDepth() : 0.0f);
	const FTransform BoneTransform = Mesh ? Mesh->GetSocketTransform(Bone) : (GetOwner() ? GetOwner()->GetActorTransform() : FTransform::Identity);
	const FTransform Relative = FTransform(Forward.Rotation(), Contact).GetRelativeTransform(BoneTransform);

	// Beyond the visible maximum a projectile joins the nearest shaft, preferring one on the same bone.
	int32 GroupId = 0;
	if (Groups.Num() >= FMath::Max(1, Definition.MaxVisibleArrowsPerCharacter))
	{
		double BestScore = TNumericLimits<double>::Max();
		for (const FRpgEmbeddedProjectileGroup& Group : Groups)
		{
			const FVector GroupWorld = (Mesh ? Mesh->GetSocketTransform(Group.Bone) : BoneTransform).TransformPosition(Group.RelativeLocation);
			const double Score = FVector::DistSquared(GroupWorld, Contact) + (Group.Bone == Bone ? 0.0 : 1.0e8);
			if (Score < BestScore)
			{
				BestScore = Score;
				GroupId = Group.GroupId;
			}
		}
		ChangeGroupCount(GroupId, 1);
	}
	else
	{
		FRpgEmbeddedProjectileGroup& Group = Groups.AddDefaulted_GetRef();
		Group.GroupId = ++LastGroupId;
		Group.Bone = Bone;
		Group.RelativeLocation = Relative.GetLocation();
		Group.RelativeRotation = Relative.Rotator();
		Group.VisualClass = Definition.PickupClass;
		Group.Count = 1;
		GroupId = Group.GroupId;
		MarkGroupsChanged();
	}

	FRecord& Record = Records.AddDefaulted_GetRef();
	Record.RecordId = ++LastRecordId;
	Record.Ammunition = Ammunition;
	Record.GroupId = GroupId;
	const int32 RecordId = Record.RecordId;

	// A corpse whose loot is already built takes the projectile into it at once.
	OfferPendingToLoot();
	return RecordId;
}

void URpgEmbeddedProjectileHostComponent::CancelRecord(const int32 RecordId)
{
	const int32 Index = Records.IndexOfByPredicate([RecordId](const FRecord& Record) { return Record.RecordId == RecordId; });
	if (Index == INDEX_NONE || Records[Index].State != ERecordState::Pending)
	{
		return;
	}
	const int32 GroupId = Records[Index].GroupId;
	Records.RemoveAt(Index);
	ChangeGroupCount(GroupId, -1);
}

int32 URpgEmbeddedProjectileHostComponent::GetPendingCount() const
{
	return Records.FilterByPredicate([](const FRecord& Record)
	{
		return Record.State == ERecordState::Pending || Record.State == ERecordState::Queued;
	}).Num();
}

int32 URpgEmbeddedProjectileHostComponent::GetCreditedCount() const
{
	return Records.FilterByPredicate([](const FRecord& Record) { return Record.State == ERecordState::Credited; }).Num();
}

UStaticMeshComponent* URpgEmbeddedProjectileHostComponent::GetShaftForTests(const int32 GroupId) const
{
	const TObjectPtr<UStaticMeshComponent>* Visual = Visuals.Find(GroupId);
	return Visual ? Visual->Get() : nullptr;
}

void URpgEmbeddedProjectileHostComponent::HandleGatherLoot(URpgLootSourceComponent* Source, FInventoryPickup& Payload)
{
	// The death loot is built once: every projectile lodged so far joins it as one more item.
	for (FRecord& Record : Records)
	{
		if (Record.State == ERecordState::Pending)
		{
			FPickupTemplate& Template = Payload.Templates.AddDefaulted_GetRef();
			Template.ItemDef = Record.Ammunition;
			Template.StackCount = 1;
			Record.State = ERecordState::Queued;
		}
	}
}

void URpgEmbeddedProjectileHostComponent::HandleLootPopulated(URpgLootSourceComponent* Source, bool bHasLoot)
{
	URpgInventoryManagerComponent* Inventory = FindCorpseInventory();
	for (FRecord& Record : Records)
	{
		if (Record.State == ERecordState::Queued)
		{
			Record.State = ERecordState::Credited;
		}
	}
	for (const FRecord& Record : Records)
	{
		if (Record.State == ERecordState::Credited && Inventory)
		{
			ObservedCorpseCounts.Add(Record.Ammunition, Inventory->GetTotalItemCountByDefinition(Record.Ammunition));
		}
	}
	// Projectiles that hit while the loot was rolled but before it was built join now.
	OfferPendingToLoot();
}

void URpgEmbeddedProjectileHostComponent::OfferPendingToLoot()
{
	URpgLootSourceComponent* LootSource = FindLootSource();
	URpgInventoryManagerComponent* Inventory = FindCorpseInventory();
	if (!LootSource || !Inventory || !LootSource->CanTakeSupplementalLoot())
	{
		return;
	}

	for (FRecord& Record : Records)
	{
		if (Record.State != ERecordState::Pending)
		{
			continue;
		}
		FInventoryPickup Payload;
		FPickupTemplate& Template = Payload.Templates.AddDefaulted_GetRef();
		Template.ItemDef = Record.Ammunition;
		Template.StackCount = 1;
		if (!LootSource->AddSupplementalLoot(Payload))
		{
			continue;
		}
		Record.State = LootSource->IsLootPopulated() ? ERecordState::Credited : ERecordState::Queued;
		if (Record.State == ERecordState::Credited)
		{
			ObservedCorpseCounts.Add(Record.Ammunition, Inventory->GetTotalItemCountByDefinition(Record.Ammunition));
		}
	}
}

void URpgEmbeddedProjectileHostComponent::HandleInventoryCommitted(URpgInventoryManagerComponent* Inventory)
{
	if (!Inventory || Inventory != FindCorpseInventory())
	{
		return;
	}

	// Only a drop in the aggregate quantity is a take; stacks merged, split or moved inside the corpse are not.
	for (TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>& Observed : ObservedCorpseCounts)
	{
		const int32 Count = Inventory->GetTotalItemCountByDefinition(Observed.Key);
		int32 Taken = Observed.Value - Count;
		Observed.Value = Count;
		for (FRecord& Record : Records)
		{
			if (Taken <= 0)
			{
				break;
			}
			if (Record.State == ERecordState::Credited && Record.Ammunition == Observed.Key)
			{
				Record.State = ERecordState::Retired;
				ChangeGroupCount(Record.GroupId, -1);
				--Taken;
			}
		}
	}
	Records.RemoveAll([](const FRecord& Record) { return Record.State == ERecordState::Retired; });
}

void URpgEmbeddedProjectileHostComponent::ChangeGroupCount(const int32 GroupId, const int32 Delta)
{
	const int32 Index = Groups.IndexOfByPredicate([GroupId](const FRpgEmbeddedProjectileGroup& Group) { return Group.GroupId == GroupId; });
	if (Index == INDEX_NONE)
	{
		return;
	}
	Groups[Index].Count += Delta;
	if (Groups[Index].Count <= 0)
	{
		Groups.RemoveAt(Index);
	}
	MarkGroupsChanged();
}

void URpgEmbeddedProjectileHostComponent::MarkGroupsChanged()
{
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
	RefreshVisuals();
}

USkeletalMeshComponent* URpgEmbeddedProjectileHostComponent::FindMesh() const
{
	const AActor* Owner = GetOwner();
	if (const ACharacter* Character = Cast<ACharacter>(Owner))
	{
		return Character->GetMesh();
	}
	return Owner ? Owner->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

URpgLootSourceComponent* URpgEmbeddedProjectileHostComponent::FindLootSource() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<URpgLootSourceComponent>() : nullptr;
}

URpgInventoryManagerComponent* URpgEmbeddedProjectileHostComponent::FindCorpseInventory() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<URpgInventoryManagerComponent>() : nullptr;
}

void URpgEmbeddedProjectileHostComponent::OnRep_Groups()
{
	RefreshVisuals();
}

void URpgEmbeddedProjectileHostComponent::RefreshVisuals()
{
	const UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer || !IsRegistered())
	{
		return;
	}

	TSet<int32> Live;
	USkeletalMeshComponent* Mesh = FindMesh();
	for (const FRpgEmbeddedProjectileGroup& Group : Groups)
	{
		Live.Add(Group.GroupId);
		if (Visuals.Contains(Group.GroupId))
		{
			continue;
		}
		const ARpgEmbeddedProjectilePickup* VisualDefaults = Group.VisualClass ? Group.VisualClass->GetDefaultObject<ARpgEmbeddedProjectilePickup>() : nullptr;
		const UStaticMeshComponent* Template = VisualDefaults ? VisualDefaults->GetDisplayMeshComponent() : nullptr;
		USceneComponent* Parent = Mesh ? static_cast<USceneComponent*>(Mesh) : (GetOwner() ? GetOwner()->GetRootComponent() : nullptr);
		if (!Template || !Template->GetStaticMesh() || !Parent)
		{
			continue;
		}

		// The shaft follows this machine's own bone, also through its local ragdoll.
		UStaticMeshComponent* Shaft = NewObject<UStaticMeshComponent>(GetOwner(), NAME_None, RF_Transient);
		Shaft->SetStaticMesh(Template->GetStaticMesh());
		Shaft->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Shaft->SetGenerateOverlapEvents(false);
		Shaft->SetCanEverAffectNavigation(false);
		Shaft->SetupAttachment(Parent, Group.Bone);
		Shaft->SetRelativeTransform(Template->GetRelativeTransform() * FTransform(Group.RelativeRotation, Group.RelativeLocation));
		Shaft->RegisterComponent();
		Visuals.Add(Group.GroupId, Shaft);
	}

	for (auto It = Visuals.CreateIterator(); It; ++It)
	{
		if (!Live.Contains(It.Key()))
		{
			if (It.Value())
			{
				It.Value()->DestroyComponent();
			}
			It.RemoveCurrent();
		}
	}
}
