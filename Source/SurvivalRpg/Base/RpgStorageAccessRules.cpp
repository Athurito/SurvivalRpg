#include "RpgStorageAccessRules.h"

#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "RpgBaseCampActor.h"
#include "RpgPersonalStorageLockerActor.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Inventory/RpgDroppedInventoryActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_StorageProfile.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

namespace
{
	bool IsSharedStorageActor(const AActor* Actor)
	{
		return IsValid(Actor) && !Actor->IsActorBeingDestroyed() &&
			!Actor->IsA<APawn>() && !Actor->IsA<AController>() && !Actor->IsA<APlayerState>() &&
			!Actor->IsA<ARpgDroppedInventoryActor>() && !Actor->IsA<ARpgPersonalStorageLockerActor>();
	}

	bool IsInSourceArea(const ARpgBaseCampActor* ContextBase, const FVector& ContextLocation,
		float OutsideRadius, const AActor* Candidate)
	{
		if (!IsSharedStorageActor(Candidate))
		{
			return false;
		}
		if (ContextBase)
		{
			return RpgStorageAccessRules::ResolveBaseAtLocation(Candidate->GetWorld(), Candidate->GetActorLocation()) == ContextBase;
		}
		return FMath::IsFinite(OutsideRadius) && OutsideRadius > 0.0f &&
			FVector::DistSquared(ContextLocation, Candidate->GetActorLocation()) <= FMath::Square(OutsideRadius);
	}

	bool HasConflictingContextBase(const UWorld* World, const FVector& ContextLocation, const ARpgBaseCampActor* ResolvedBase)
	{
		if (ResolvedBase) return false;
		for (TActorIterator<ARpgBaseCampActor> It(World); It; ++It)
		{
			if (RpgStorageAccessRules::IsInsideBaseArea(It->GetActorLocation(), It->GetBuildRadius(), ContextLocation)) return true;
		}
		return false;
	}
}

bool RpgStorageAccessRules::IsInsideBaseArea(const FVector& Center, float Radius, const FVector& Location)
{
	return !Center.ContainsNaN() && !Location.ContainsNaN() && FMath::IsFinite(Radius) && Radius > 0.0f &&
		FVector::DistSquared2D(Center, Location) <= FMath::Square(static_cast<double>(Radius));
}

bool RpgStorageAccessRules::BaseAreasConflict(const FVector& CenterA, float RadiusA, const FVector& CenterB, float RadiusB)
{
	if (CenterA.ContainsNaN() || CenterB.ContainsNaN() || !FMath::IsFinite(RadiusA) || !FMath::IsFinite(RadiusB) || RadiusA <= 0.0f || RadiusB <= 0.0f)
	{
		return true;
	}
	return FVector::DistSquared2D(CenterA, CenterB) <= FMath::Square(static_cast<double>(RadiusA) + RadiusB);
}

bool RpgStorageAccessRules::CanPlaceBaseArea(const UWorld* World, const FVector& Center, float Radius, const ARpgBaseCampActor* IgnoredBase)
{
	if (!World || Center.ContainsNaN() || !FMath::IsFinite(Radius) || Radius <= 0.0f)
	{
		return false;
	}
	for (TActorIterator<ARpgBaseCampActor> It(World); It; ++It)
	{
		if (*It != IgnoredBase && !It->IsActorBeingDestroyed() &&
			BaseAreasConflict(Center, Radius, It->GetActorLocation(), It->GetBuildRadius()))
		{
			return false;
		}
	}
	return true;
}

ARpgBaseCampActor* RpgStorageAccessRules::ResolveBaseAtLocation(const UWorld* World, const FVector& Location)
{
	if (!World || Location.ContainsNaN())
	{
		return nullptr;
	}
	ARpgBaseCampActor* Result = nullptr;
	for (TActorIterator<ARpgBaseCampActor> It(World); It; ++It)
	{
		if (It->IsActorBeingDestroyed() || !IsInsideBaseArea(It->GetActorLocation(), It->GetBuildRadius(), Location))
		{
			continue;
		}
		if (Result || !CanPlaceBaseArea(World, It->GetActorLocation(), It->GetBuildRadius(), *It))
		{
			return nullptr;
		}
		Result = *It;
	}
	return Result;
}

void RpgStorageAccessRules::ResolveStorageSources(const UWorld* World, const FVector& ContextLocation,
	float OutsideSearchRadius, TArray<URpgInventoryManagerComponent*>& OutSources)
{
	OutSources.Reset();
	if (!World || ContextLocation.ContainsNaN())
	{
		return;
	}
	const ARpgBaseCampActor* ContextBase = ResolveBaseAtLocation(World, ContextLocation);
	if (HasConflictingContextBase(World, ContextLocation, ContextBase)) return;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsInSourceArea(ContextBase, ContextLocation, OutsideSearchRadius, Actor))
		{
			continue;
		}
		if (const URpgInventoryContainerComponent* Container = Actor->FindComponentByClass<URpgInventoryContainerComponent>();
			Container && Container->IsContainerAccessible() && Container->AllowsCraftingAccess() &&
			Container->GetTransferPolicy() == ERpgInventoryContainerTransferPolicy::Bidirectional)
		{
			if (URpgInventoryManagerComponent* Inventory = Container->GetInventoryManager())
			{
				OutSources.AddUnique(Inventory);
			}
		}
	}
	OutSources.Sort([](const URpgInventoryManagerComponent& A, const URpgInventoryManagerComponent& B)
	{
		return GetPersistentInventoryId(&A).LexicalLess(GetPersistentInventoryId(&B));
	});
}

TArray<URpgInventoryContainerComponent*> RpgStorageAccessRules::GetPhysicalStorageTargets(const UWorld* World,
	const FVector& ContextLocation, float OutsideSearchRadius, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
{
	TArray<URpgInventoryContainerComponent*> Results;
	const URpgInventoryFragment_StorageProfile* Profile = URpgInventoryFragment_StorageProfile::ResolveStorageProfile(ItemDefinition);
	if (!World || !ItemDefinition || ContextLocation.ContainsNaN() || !Profile || !Profile->CanAutoDepositPhysical())
	{
		return Results;
	}
	const ARpgBaseCampActor* ContextBase = ResolveBaseAtLocation(World, ContextLocation);
	if (HasConflictingContextBase(World, ContextLocation, ContextBase)) return Results;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (!IsInSourceArea(ContextBase, ContextLocation, OutsideSearchRadius, *It))
		{
			continue;
		}
		URpgInventoryContainerComponent* Container = It->FindComponentByClass<URpgInventoryContainerComponent>();
		int64 Order = 0;
		if (Container && Container->IsContainerAccessible() && Container->AllowsCraftingAccess() &&
			Container->GetInventoryManager() && Container->GetTransferPolicy() == ERpgInventoryContainerTransferPolicy::Bidirectional &&
			Container->GetAssignmentRank(ItemDefinition, Order) != INDEX_NONE)
		{
			Results.Add(Container);
		}
	}
	Results.Sort([ItemDefinition](const URpgInventoryContainerComponent& A, const URpgInventoryContainerComponent& B)
	{
		int64 OrderA = 0, OrderB = 0;
		const int32 RankA = A.GetAssignmentRank(ItemDefinition, OrderA);
		const int32 RankB = B.GetAssignmentRank(ItemDefinition, OrderB);
		if (RankA != RankB) return RankA < RankB;
		const int32 CountA = A.GetInventoryManager()->GetTotalItemCountByDefinition(ItemDefinition);
		const int32 CountB = B.GetInventoryManager()->GetTotalItemCountByDefinition(ItemDefinition);
		if (CountA != CountB) return CountA > CountB;
		if (RankA < 2 && OrderA != OrderB) return OrderA < OrderB;
		return A.GetPersistentContainerId().LexicalLess(B.GetPersistentContainerId());
	});
	return Results;
}

void RpgStorageAccessRules::ResolveDepositTargets(const UWorld* World, const FVector& ContextLocation, float OutsideSearchRadius,
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, TArray<URpgInventoryManagerComponent*>& OutTargets)
{
	OutTargets.Reset();
	for (URpgInventoryContainerComponent* Container : GetPhysicalStorageTargets(World, ContextLocation, OutsideSearchRadius, ItemDefinition))
	{
		OutTargets.Add(Container->GetInventoryManager());
	}
}

FName RpgStorageAccessRules::GetPersistentInventoryId(const URpgInventoryManagerComponent* Inventory)
{
	const AActor* Owner = Inventory ? Inventory->GetOwner() : nullptr;
	if (!Owner)
	{
		return NAME_None;
	}
	if (const URpgInventoryContainerComponent* Container = Owner->FindComponentByClass<URpgInventoryContainerComponent>();
		Container && Container->GetInventoryManager() == Inventory && !Container->GetPersistentContainerId().IsNone())
	{
		return FName(*(TEXT("Container:") + Container->GetPersistentContainerId().ToString()));
	}
	const ARpgGameModeBase* GameMode = Owner->GetWorld() ? Owner->GetWorld()->GetAuthGameMode<ARpgGameModeBase>() : nullptr;
	if (GameMode)
	{
		for (FConstPlayerControllerIterator It = Owner->GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PC = It->Get();
			if (PC && (Owner == PC || Owner == PC->PlayerState || Owner == PC->GetPawn()))
			{
				return FName(*(TEXT("Player:") + GameMode->GetPlayerProfileKey(PC)));
			}
		}
	}
	return NAME_None;
}

URpgInventoryManagerComponent* RpgStorageAccessRules::FindPersistentInventory(const UWorld* World, FName InventoryId)
{
	if (!World || InventoryId.IsNone())
	{
		return nullptr;
	}
	if (const ARpgGameModeBase* GameMode = World->GetAuthGameMode<ARpgGameModeBase>();
		GameMode && InventoryId.ToString().StartsWith(TEXT("Player:")))
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PC = It->Get();
			if (PC && FName(*(TEXT("Player:") + GameMode->GetPlayerProfileKey(PC))) == InventoryId &&
				!GameMode->IsPlayerProfileRestoreComplete(PC))
			{
				// A later profile graph replacement would erase a refund granted into this provisional inventory.
				return nullptr;
			}
		}
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->IsActorBeingDestroyed()) continue;
		TInlineComponentArray<URpgInventoryManagerComponent*> Inventories(*It);
		for (URpgInventoryManagerComponent* Inventory : Inventories)
		{
			if (GetPersistentInventoryId(Inventory) == InventoryId)
			{
				const URpgInventoryContainerComponent* Container = It->FindComponentByClass<URpgInventoryContainerComponent>();
				if (Container && Container->GetInventoryManager() == Inventory && Container->IsConstructionPending())
				{
					// Returning chests restore their saved graph after authored BeginPlay seed grants have finished.
					return nullptr;
				}
				return Inventory;
			}
		}
	}
	return nullptr;
}

int64 RpgStorageAccessRules::AllocateAssignmentOrder(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return 0;
	}
	int64 LastOrder = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (const URpgInventoryContainerComponent* Container = It->FindComponentByClass<URpgInventoryContainerComponent>())
		{
			LastOrder = FMath::Max(LastOrder, Container->ExportPhysicalStorageMetadata().AssignmentOrderHighWaterMark);
		}
		if (const ARpgBaseCampActor* Base = Cast<ARpgBaseCampActor>(*It))
		{
			LastOrder = FMath::Max(LastOrder, Base->GetStorageAssignmentHighWaterMark());
		}
	}
	return LastOrder < MAX_int64 ? LastOrder + 1 : 0;
}
