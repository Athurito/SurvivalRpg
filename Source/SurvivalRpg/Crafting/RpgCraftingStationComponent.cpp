#include "RpgCraftingStationComponent.h"

#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/Base/RpgBaseCampActor.h"
#include "SurvivalRpg/Base/RpgBaseStorageComponent.h"
#include "SurvivalRpg/Base/RpgBaseStorageStationComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameStateBase.h"
#include "SurvivalRpg/Crafting/RpgCraftingRecipeDefinition.h"
#include "SurvivalRpg/Crafting/RpgRecipeUnlockComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Interaction/Abilities/RpgGameplayAbility_OpenCraftingStation.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"
#include "SurvivalRpg/Inventory/RpgDroppedInventoryActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgCraftingStationComponent)

DEFINE_LOG_CATEGORY_STATIC(LogRpgCraftingStation, Log, All);
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Rpg_Crafting_Message_StationChanged, "Rpg.Crafting.Message.StationChanged");

URpgCraftingStationComponent::URpgCraftingStationComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	DroppedOutputActorClass = ARpgDroppedInventoryActor::StaticClass();

	OpenCraftingOption.Text = NSLOCTEXT("RpgCrafting", "OpenCraftingStationText", "Open");
	OpenCraftingOption.SubText = NSLOCTEXT("RpgCrafting", "OpenCraftingStationSubText", "Crafting");
	OpenCraftingOption.InteractionAbilityToGrant = URpgGameplayAbility_OpenCraftingStation::StaticClass();
}

void URpgCraftingStationComponent::BeginPlay()
{
	Super::BeginPlay();

	if (OutputInventoryComponent)
	{
		SetOutputInventoryManager(OutputInventoryComponent);
	}
}

void URpgCraftingStationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, LinkedBaseCamp);
	DOREPLIFETIME(ThisClass, bAutoDepositCraftingOutputsEnabled);
	DOREPLIFETIME(ThisClass, bStationPaused);
	DOREPLIFETIME(ThisClass, CraftingJobs);
	DOREPLIFETIME(ThisClass, CraftingStateRevision);
}

void URpgCraftingStationComponent::GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& InteractionBuilder)
{
	if (CanActorAccess(InteractQuery.RequestingAvatar.Get()))
	{
		InteractionBuilder.AddInteractionOption(OpenCraftingOption);
	}
}

namespace
{
	const URpgInventoryFragment_ItemTraits* GetItemTraitsForDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		const URpgInventoryItemDefinition* ItemCDO = ItemDefinition ? GetDefault<URpgInventoryItemDefinition>(ItemDefinition) : nullptr;
		return ItemCDO ? Cast<URpgInventoryFragment_ItemTraits>(ItemCDO->FindFragmentByClass(URpgInventoryFragment_ItemTraits::StaticClass())) : nullptr;
	}

	bool IsMaterialDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		const URpgInventoryFragment_ItemTraits* Traits = GetItemTraitsForDefinition(ItemDefinition);
		return Traits && Traits->IsMaterial();
	}

	bool TryBuildAggregatedResourceCosts(
		const TArray<FRpgCraftingResourceCost>& RequiredItems,
		TArray<FRpgCraftingResourceCost>& OutAggregatedCosts)
	{
		OutAggregatedCosts.Reset();
		for (const FRpgCraftingResourceCost& RequiredItem : RequiredItems)
		{
			if (!RequiredItem.ItemDefinition || RequiredItem.Count <= 0)
			{
				OutAggregatedCosts.Reset();
				return false;
			}

			FRpgCraftingResourceCost* ExistingCost = OutAggregatedCosts.FindByPredicate(
				[ItemDefinition = RequiredItem.ItemDefinition](const FRpgCraftingResourceCost& Candidate)
				{
					return Candidate.ItemDefinition == ItemDefinition;
				});
			if (!ExistingCost)
			{
				OutAggregatedCosts.Add(RequiredItem);
				continue;
			}

			const int64 AggregatedCount = static_cast<int64>(ExistingCost->Count) + static_cast<int64>(RequiredItem.Count);
			if (AggregatedCount > MAX_int32)
			{
				OutAggregatedCosts.Reset();
				return false;
			}

			ExistingCost->Count = static_cast<int32>(AggregatedCount);
		}

		return true;
	}

	void AddRefundCredit(
		TArray<FRpgCraftingRefundEntry>& RefundEntries,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		int32 Count,
		URpgInventoryManagerComponent* Inventory,
		bool bRefundToBaseStorage)
	{
		if (!ItemDefinition || Count <= 0)
		{
			return;
		}

		for (FRpgCraftingRefundEntry& RefundEntry : RefundEntries)
		{
			if (RefundEntry.ItemDefinition == ItemDefinition &&
				RefundEntry.Inventory == Inventory &&
				RefundEntry.bRefundToBaseStorage == bRefundToBaseStorage)
			{
				RefundEntry.Count += Count;
				return;
			}
		}

		FRpgCraftingRefundEntry& NewRefundEntry = RefundEntries.AddDefaulted_GetRef();
		NewRefundEntry.ItemDefinition = ItemDefinition;
		NewRefundEntry.Count = Count;
		NewRefundEntry.Inventory = Inventory;
		NewRefundEntry.bRefundToBaseStorage = bRefundToBaseStorage;
	}
}

TArray<URpgInventoryManagerComponent*> URpgCraftingStationComponent::GetResourceInventories(AActor* RequestingActor) const
{
	TArray<URpgInventoryManagerComponent*> Results;

	if (RequestingActor == nullptr)
	{
		return Results;
	}

	if (URpgInventoryManagerComponent* DirectInventory = RequestingActor->FindComponentByClass<URpgInventoryManagerComponent>())
	{
		Results.AddUnique(DirectInventory);
	}

	if (const APawn* RequestingPawn = Cast<APawn>(RequestingActor))
	{
		if (APlayerState* PlayerState = RequestingPawn->GetPlayerState())
		{
			if (URpgInventoryManagerComponent* PlayerInventory = PlayerState->FindComponentByClass<URpgInventoryManagerComponent>())
			{
				Results.AddUnique(PlayerInventory);
			}
		}

		if (AController* Controller = RequestingPawn->GetController())
		{
			if (APlayerState* PlayerState = Controller->PlayerState)
			{
				if (URpgInventoryManagerComponent* PlayerInventory = PlayerState->FindComponentByClass<URpgInventoryManagerComponent>())
				{
					Results.AddUnique(PlayerInventory);
				}
			}
		}
	}

	if (!bUseNearbyCraftingContainers)
	{
		return Results;
	}

	const AActor* StationOwner = GetOwner();
	const UWorld* World = GetWorld();
	if (!StationOwner || !World)
	{
		return Results;
	}

	const float SearchRadiusSq = FMath::Square(StorageSearchRadius);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* CandidateActor = *It;
		if (!CandidateActor || CandidateActor == StationOwner)
		{
			continue;
		}

		URpgInventoryContainerComponent* Container = CandidateActor->FindComponentByClass<URpgInventoryContainerComponent>();
		URpgInventoryManagerComponent* Inventory = CandidateActor->FindComponentByClass<URpgInventoryManagerComponent>();
		if (!Container || !Inventory || !Container->AllowsCraftingAccess() || !Container->IsContainerAccessible())
		{
			continue;
		}

		const bool bSameStorageGroup = !StorageGroupId.IsNone() && Container->GetStorageGroupId() == StorageGroupId;
		const bool bWithinRadius = StorageSearchRadius <= 0.0f ||
			FVector::DistSquared(StationOwner->GetActorLocation(), CandidateActor->GetActorLocation()) <= SearchRadiusSq;

		if (bSameStorageGroup || bWithinRadius)
		{
			Results.AddUnique(Inventory);
		}
	}

	return Results;
}

void URpgCraftingStationComponent::SetLinkedBaseCamp(ARpgBaseCampActor* NewBaseCamp)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	LinkedBaseCamp = NewBaseCamp;
	OwnerActor->ForceNetUpdate();
}

bool URpgCraftingStationComponent::IsRecipeUnlocked(const URpgCraftingRecipeDefinition* RecipeDefinition) const
{
	if (!RecipeDefinition)
	{
		return false;
	}

	if (RecipeDefinition->bUnlockedByDefault)
	{
		return true;
	}

	const UWorld* World = GetWorld();
	const ARpgGameStateBase* GameState = World ? World->GetGameState<ARpgGameStateBase>() : nullptr;
	const URpgRecipeUnlockComponent* RecipeUnlockComponent = GameState ? GameState->GetRecipeUnlockComponent() : nullptr;
	return RecipeUnlockComponent && RecipeUnlockComponent->IsRecipeUnlocked(RecipeDefinition);
}

bool URpgCraftingStationComponent::IsRecipeOfferedByStation(const URpgCraftingRecipeDefinition* RecipeDefinition) const
{
	if (!RecipeDefinition || !AvailableRecipeSet || !AvailableRecipeSet->Recipes.Contains(RecipeDefinition))
	{
		return false;
	}

	if (!RecipeDefinition->RequiredStationTags.IsEmpty() && !StationTags.HasAllExact(RecipeDefinition->RequiredStationTags))
	{
		return false;
	}

	const FGameplayTagContainer BaseUpgradeTags = LinkedBaseCamp ? LinkedBaseCamp->GetGrantedStorageUpgradeTags() : FGameplayTagContainer();
	return RecipeDefinition->RequiredUnlockTags.IsEmpty() || BaseUpgradeTags.HasAllExact(RecipeDefinition->RequiredUnlockTags);
}

TArray<URpgCraftingRecipeDefinition*> URpgCraftingStationComponent::GetAvailableRecipes() const
{
	TArray<URpgCraftingRecipeDefinition*> Results;
	if (!AvailableRecipeSet)
	{
		return Results;
	}

	for (URpgCraftingRecipeDefinition* Recipe : AvailableRecipeSet->Recipes)
	{
		if (!IsRecipeOfferedByStation(Recipe))
		{
			continue;
		}

		Results.Add(Recipe);
	}

	return Results;
}

bool URpgCraftingStationComponent::CanCraftRecipe(AActor* RequestingActor, const URpgCraftingRecipeDefinition* RecipeDefinition) const
{
	return CanCraftRecipeQuantity(RequestingActor, RecipeDefinition, 1);
}

bool URpgCraftingStationComponent::CanCraftRecipeQuantity(AActor* RequestingActor, const URpgCraftingRecipeDefinition* RecipeDefinition, int32 Quantity) const
{
	return Quantity > 0 && Quantity <= GetMaxCraftableQuantity(RequestingActor, RecipeDefinition);
}

int32 URpgCraftingStationComponent::GetMaxCraftableQuantity(AActor* RequestingActor, const URpgCraftingRecipeDefinition* RecipeDefinition) const
{
	if (!RecipeDefinition ||
		!CanActorAccess(RequestingActor) ||
		!IsRecipeOfferedByStation(RecipeDefinition) ||
		!IsRecipeUnlocked(RecipeDefinition) ||
		CraftingJobs.Num() >= FMath::Max(1, MaxQueuedJobs) ||
		!CanAcceptCraftingOutputs(RecipeDefinition->OutputItems))
	{
		return 0;
	}

	TArray<FRpgCraftingResourceCost> AggregatedResourceCosts;
	if (!TryBuildAggregatedResourceCosts(RecipeDefinition->RequiredResources, AggregatedResourceCosts))
	{
		return 0;
	}

	if (AggregatedResourceCosts.IsEmpty())
	{
		return FMath::Max(1, MaxFreeRecipeCraftQuantity);
	}

	int32 MaxQuantity = MAX_int32;
	for (const FRpgCraftingResourceCost& RequiredItem : AggregatedResourceCosts)
	{
		MaxQuantity = FMath::Min(MaxQuantity, GetAvailableResourceCount(RequestingActor, RequiredItem.ItemDefinition) / RequiredItem.Count);
	}

	return FMath::Max(0, MaxQuantity);
}

bool URpgCraftingStationComponent::QueueCraftRecipe(AActor* RequestingActor, URpgCraftingRecipeDefinition* RecipeDefinition, int32 Quantity)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !CanCraftRecipeQuantity(RequestingActor, RecipeDefinition, Quantity))
	{
		return false;
	}

	TArray<FRpgCraftingRefundEntry> RefundEntries;
	if (!ConsumeResourcesWithRefund(RequestingActor, RecipeDefinition->RequiredResources, Quantity, RefundEntries))
	{
		return false;
	}

	FRpgCraftingJobEntry& NewJob = CraftingJobs.AddDefaulted_GetRef();
	NewJob.JobId = FGuid::NewGuid();
	NewJob.Recipe = RecipeDefinition;
	NewJob.QuantityTotal = Quantity;
	NewJob.QuantityCompleted = 0;
	NewJob.State = ERpgCraftingJobState::Queued;
	NewJob.RefundEntries = MoveTemp(RefundEntries);

	MarkCraftingStateDirty(NewJob.JobId, NewJob.State);
	TryStartNextQueuedJob();
	return true;
}

bool URpgCraftingStationComponent::CraftRecipe(AActor* RequestingActor, URpgCraftingRecipeDefinition* RecipeDefinition)
{
	return QueueCraftRecipe(RequestingActor, RecipeDefinition, 1);
}

bool URpgCraftingStationComponent::CancelCraftJob(AActor* RequestingActor, FGuid JobId)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !CanActorAccess(RequestingActor))
	{
		return false;
	}

	const int32 JobIndex = FindJobIndex(JobId);
	if (JobIndex == INDEX_NONE)
	{
		return false;
	}

	const ERpgCraftingJobState RemovedState = CraftingJobs[JobIndex].State;
	const bool bWasActive = RemovedState == ERpgCraftingJobState::Active || RemovedState == ERpgCraftingJobState::Paused || RemovedState == ERpgCraftingJobState::BlockedOutput;
	if (bWasActive)
	{
		GetWorld()->GetTimerManager().ClearTimer(CraftingTimerHandle);
	}

	RefundRemainingJobCosts(CraftingJobs[JobIndex]);
	CraftingJobs.RemoveAt(JobIndex);
	MarkCraftingStateDirty(JobId, RemovedState);
	TryStartNextQueuedJob();
	return true;
}

bool URpgCraftingStationComponent::PauseCraftingStation(AActor* RequestingActor)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !CanActorAccess(RequestingActor) || bStationPaused)
	{
		return false;
	}

	bStationPaused = true;
	if (const int32 ActiveJobIndex = FindActiveJobIndex(); ActiveJobIndex != INDEX_NONE && CraftingJobs[ActiveJobIndex].State == ERpgCraftingJobState::Active)
	{
		FRpgCraftingJobEntry& ActiveJob = CraftingJobs[ActiveJobIndex];
		ActiveJob.PausedRemainingTime = FMath::Max(0.0f, ActiveJob.FinishServerTime - GetServerWorldTimeSeconds());
		ActiveJob.State = ERpgCraftingJobState::Paused;
		GetWorld()->GetTimerManager().ClearTimer(CraftingTimerHandle);
		MarkCraftingStateDirty(ActiveJob.JobId, ActiveJob.State, true);
		return true;
	}

	MarkCraftingStateDirty(FGuid(), ERpgCraftingJobState::Paused, true);
	return true;
}

bool URpgCraftingStationComponent::ResumeCraftingStation(AActor* RequestingActor)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !CanActorAccess(RequestingActor) || !bStationPaused)
	{
		return false;
	}

	bStationPaused = false;
	const int32 ActiveJobIndex = FindActiveJobIndex();
	if (ActiveJobIndex != INDEX_NONE && CraftingJobs[ActiveJobIndex].State == ERpgCraftingJobState::Paused)
	{
		const float RemainingDuration = CraftingJobs[ActiveJobIndex].PausedRemainingTime;
		StartJobAtIndex(ActiveJobIndex, RemainingDuration, true);
		return true;
	}

	MarkCraftingStateDirty(FGuid(), ERpgCraftingJobState::Queued, true);
	TryStartNextQueuedJob();
	return true;
}

bool URpgCraftingStationComponent::GetActiveCraftingJob(FRpgCraftingJobEntry& OutJob) const
{
	const int32 ActiveJobIndex = FindActiveJobIndex();
	if (ActiveJobIndex == INDEX_NONE)
	{
		return false;
	}

	OutJob = CraftingJobs[ActiveJobIndex];
	return true;
}

int32 URpgCraftingStationComponent::GetAvailableResourceCount(AActor* RequestingActor, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	if (!ItemDefinition)
	{
		return 0;
	}

	int32 TotalCount = 0;
	const TArray<URpgInventoryManagerComponent*> ResourceInventories = GetResourceInventories(RequestingActor);
	if (ResourceConsumeOrder != ERpgCraftingResourceConsumeOrder::BaseOnly)
	{
		TotalCount += GetAvailableInventoryResourceCount(ItemDefinition, ResourceInventories);
	}

	if (ResourceConsumeOrder != ERpgCraftingResourceConsumeOrder::PlayerOnly)
	{
		if (const URpgBaseStorageComponent* BaseStorage = GetLinkedBaseStorage())
		{
			TotalCount += BaseStorage->GetResourceCount(ItemDefinition);
		}
	}

	return TotalCount;
}

bool URpgCraftingStationComponent::ConsumeResources(AActor* RequestingActor, const TArray<FRpgCraftingResourceCost>& RequiredItems)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return false;
	}

	TArray<FRpgCraftingResourceCost> AggregatedRequiredItems;
	if (!TryBuildAggregatedResourceCosts(RequiredItems, AggregatedRequiredItems))
	{
		return false;
	}

	for (const FRpgCraftingResourceCost& RequiredItem : AggregatedRequiredItems)
	{
		if (GetAvailableResourceCount(RequestingActor, RequiredItem.ItemDefinition) < RequiredItem.Count)
		{
			return false;
		}
	}

	TArray<URpgInventoryManagerComponent*> ResourceInventories = GetResourceInventories(RequestingActor);
	for (const FRpgCraftingResourceCost& RequiredItem : AggregatedRequiredItems)
	{
		int32 RemainingCount = RequiredItem.Count;

		auto ConsumeFromBase = [&]()
		{
			if (RemainingCount <= 0)
			{
				return true;
			}

			const URpgBaseStorageComponent* BaseStorage = GetLinkedBaseStorage();
			const int32 AvailableInBase = BaseStorage ? BaseStorage->GetResourceCount(RequiredItem.ItemDefinition) : 0;
			const int32 CountToConsume = FMath::Min(AvailableInBase, RemainingCount);
			if (ConsumeBaseResources(RequiredItem.ItemDefinition, CountToConsume))
			{
				RemainingCount -= CountToConsume;
				return true;
			}

			return false;
		};

		auto ConsumeFromInventories = [&]()
		{
			if (RemainingCount <= 0)
			{
				return true;
			}

			const int32 AvailableInInventories = GetAvailableInventoryResourceCount(RequiredItem.ItemDefinition, ResourceInventories);
			const int32 CountToConsume = FMath::Min(AvailableInInventories, RemainingCount);
			if (ConsumeInventoryResources(RequiredItem.ItemDefinition, CountToConsume, ResourceInventories))
			{
				RemainingCount -= CountToConsume;
				return true;
			}

			return false;
		};

		switch (ResourceConsumeOrder)
		{
		case ERpgCraftingResourceConsumeOrder::BaseThenPlayer:
			if (!ConsumeFromBase() || !ConsumeFromInventories())
			{
				return false;
			}
			break;

		case ERpgCraftingResourceConsumeOrder::PlayerThenBase:
			if (!ConsumeFromInventories() || !ConsumeFromBase())
			{
				return false;
			}
			break;

		case ERpgCraftingResourceConsumeOrder::BaseOnly:
			if (!ConsumeFromBase())
			{
				return false;
			}
			break;

		case ERpgCraftingResourceConsumeOrder::PlayerOnly:
			if (!ConsumeFromInventories())
			{
				return false;
			}
			break;
		}

		if (RemainingCount > 0)
		{
			return false;
		}
	}

	return true;
}

bool URpgCraftingStationComponent::CanActorAccess(const AActor* RequestingActor) const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !RequestingActor)
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

void URpgCraftingStationComponent::SetOutputInventoryManager(URpgInventoryManagerComponent* InOutputInventory)
{
	OutputInventoryComponent = InOutputInventory;
	if (OutputInventoryComponent)
	{
		if (bUseSpatialOutputCapacity)
		{
			// "Unlimited" disables only the legacy entry-count cap. Spatial placement still limits the tray to the
			// authored root-grid dimensions and item footprints.
			OutputInventoryComponent->SetCapacityMode(
				ERpgInventoryCapacityMode::Unlimited);
		}
		else
		{
			OutputInventoryComponent->SetCapacityMode(
				ERpgInventoryCapacityMode::FixedEntries);
			OutputInventoryComponent->SetFixedMaxEntries(OutputSlotCount);
		}
	}
}

bool URpgCraftingStationComponent::CanAcceptCraftingOutputs(const TArray<FRpgCraftingOutputItem>& OutputItems) const
{
	if (OutputItems.IsEmpty())
	{
		return false;
	}

	for (const FRpgCraftingOutputItem& OutputItem : OutputItems)
	{
		if (!OutputItem.ItemDefinition || OutputItem.Count <= 0)
		{
			return false;
		}
	}

	return true;
}

bool URpgCraftingStationComponent::AddCraftingOutputs(const TArray<FRpgCraftingOutputItem>& OutputItems)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !CanAcceptCraftingOutputs(OutputItems))
	{
		return false;
	}

	UE_LOG(LogRpgCraftingStation, Log, TEXT("AddCraftingOutputs: Station=%s AutoDeposit=%s BaseStorage=%s Armory=%s OutputInventory=%s OutputCount=%d"),
		*GetNameSafe(GetOwner()),
		ShouldAutoDepositCraftingOutputs() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(GetLinkedBaseStorage()),
		*GetNameSafe(GetLinkedArmoryInventory()),
		*GetNameSafe(OutputInventoryComponent),
		OutputItems.Num());

	for (const FRpgCraftingOutputItem& OutputItem : OutputItems)
	{
		if (!AddOutputItemOrDrop(OutputItem.ItemDefinition, OutputItem.Count))
		{
			return false;
		}
	}

	return true;
}

bool URpgCraftingStationComponent::CraftItems(AActor* RequestingActor, const TArray<FRpgCraftingResourceCost>& RequiredItems, const TArray<FRpgCraftingOutputItem>& OutputItems)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !CanActorAccess(RequestingActor) || !CanAcceptCraftingOutputs(OutputItems))
	{
		return false;
	}

	return ConsumeResources(RequestingActor, RequiredItems) && AddCraftingOutputs(OutputItems);
}

bool URpgCraftingStationComponent::FlushOutputToBaseStorage()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !OutputInventoryComponent)
	{
		return false;
	}

	bool bMovedAnyOutput = false;
	URpgBaseStorageComponent* BaseStorage = GetLinkedBaseStorage();
	URpgInventoryManagerComponent* ArmoryInventory = GetLinkedArmoryInventory();
	const TArray<FRpgInventoryEntryView> OutputEntries = OutputInventoryComponent->GetAllEntries();
	for (const FRpgInventoryEntryView& OutputEntry : OutputEntries)
	{
		URpgInventoryItemInstance* OutputInstance = OutputEntry.Instance;
		if (!OutputInstance || OutputEntry.StackCount <= 0)
		{
			continue;
		}

		const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition = OutputInstance->GetItemDef();
		if (IsMaterialDefinition(ItemDefinition) && BaseStorage)
		{
			const int32 CountToStore = FMath::Min(OutputEntry.StackCount, BaseStorage->GetFreeResourceCapacity(ItemDefinition));
			if (CountToStore > 0 && OutputInventoryComponent->RemoveItemInstanceStack(OutputInstance, CountToStore))
			{
				BaseStorage->StoreResource(ItemDefinition, CountToStore);
				bMovedAnyOutput = true;
			}
			continue;
		}

		if (bAutoDepositInstanceOutputsToArmory && ArmoryInventory)
		{
			FRpgInventoryMutationRequest TransferRequest;
			TransferRequest.Operation = ERpgInventoryMutationOperation::Transfer;
			TransferRequest.ItemId = OutputEntry.ItemId;
			TransferRequest.Source = OutputEntry.Placement.GetContainerHandle();
			TransferRequest.Target =
				FRpgInventoryContainerHandle::MakeRoot(ArmoryInventory->GetDefaultContainerId());
			TransferRequest.Quantity = OutputEntry.StackCount;
			TransferRequest.EnsureRequestId();
			bMovedAnyOutput |= OutputInventoryComponent
				->ExecuteCrossInventoryTransfer(ArmoryInventory, TransferRequest, false)
				.IsSuccess();
		}
	}

	return bMovedAnyOutput;
}

bool URpgCraftingStationComponent::HasCraftingOutputAutoDepositAccess() const
{
	const URpgBaseStorageStationComponent* UpgradeProvider = GetOutputAutoDepositUpgradeProvider();
	const bool bProviderHasTag = UpgradeProvider && UpgradeProvider->HasUpgradeTag(RpgGameplayTags::Base_Storage_Upgrade_CraftingOutputAutoDeposit);
	const bool bBaseCampHasTag = LinkedBaseCamp && LinkedBaseCamp->HasStorageUpgradeTag(RpgGameplayTags::Base_Storage_Upgrade_CraftingOutputAutoDeposit);
	return bAlwaysAutoDepositCraftingOutputs || bBaseCampHasTag || bProviderHasTag;
}

bool URpgCraftingStationComponent::SetCraftingOutputAutoDepositEnabled(AActor* RequestingActor, bool bEnabled)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !CanActorAccess(RequestingActor))
	{
		return false;
	}

	if (bAutoDepositCraftingOutputsEnabled == bEnabled)
	{
		return true;
	}

	bAutoDepositCraftingOutputsEnabled = bEnabled;
	MarkCraftingStateDirty();

	if (bEnabled && ShouldAutoDepositCraftingOutputs())
	{
		FlushOutputToBaseStorage();
	}

	return true;
}

bool URpgCraftingStationComponent::ShouldAutoDepositCraftingOutputs() const
{
	const URpgBaseStorageStationComponent* UpgradeProvider = GetOutputAutoDepositUpgradeProvider();
	const bool bProviderHasTag = UpgradeProvider && UpgradeProvider->HasUpgradeTag(RpgGameplayTags::Base_Storage_Upgrade_CraftingOutputAutoDeposit);
	const bool bBaseCampHasTag = LinkedBaseCamp && LinkedBaseCamp->HasStorageUpgradeTag(RpgGameplayTags::Base_Storage_Upgrade_CraftingOutputAutoDeposit);
	const bool bHasAutoDepositAccess = bAlwaysAutoDepositCraftingOutputs || bBaseCampHasTag || bProviderHasTag;
	const bool bShouldAutoDeposit = bAutoDepositCraftingOutputsEnabled && bHasAutoDepositAccess;
	UE_LOG(LogRpgCraftingStation, Verbose, TEXT("ShouldAutoDepositCraftingOutputs: Station=%s Enabled=%s Always=%s BaseCamp=%s BaseHasTag=%s ProviderActor=%s ProviderComponent=%s ProviderHasTag=%s Result=%s"),
		*GetNameSafe(GetOwner()),
		bAutoDepositCraftingOutputsEnabled ? TEXT("true") : TEXT("false"),
		bAlwaysAutoDepositCraftingOutputs ? TEXT("true") : TEXT("false"),
		*GetNameSafe(LinkedBaseCamp),
		bBaseCampHasTag ? TEXT("true") : TEXT("false"),
		*GetNameSafe(OutputAutoDepositUpgradeProviderActor),
		*GetNameSafe(UpgradeProvider),
		bProviderHasTag ? TEXT("true") : TEXT("false"),
		bShouldAutoDeposit ? TEXT("true") : TEXT("false"));
	return bShouldAutoDeposit;
}

URpgBaseStorageStationComponent* URpgCraftingStationComponent::GetOutputAutoDepositUpgradeProvider() const
{
	if (OutputAutoDepositUpgradeProvider)
	{
		return OutputAutoDepositUpgradeProvider;
	}

	return OutputAutoDepositUpgradeProviderActor ? OutputAutoDepositUpgradeProviderActor->FindComponentByClass<URpgBaseStorageStationComponent>() : nullptr;
}

URpgBaseStorageComponent* URpgCraftingStationComponent::GetLinkedBaseStorage() const
{
	return bUseLinkedBaseStorage && LinkedBaseCamp ? LinkedBaseCamp->GetBaseStorageComponent() : nullptr;
}

URpgInventoryManagerComponent* URpgCraftingStationComponent::GetLinkedArmoryInventory() const
{
	return LinkedBaseCamp ? LinkedBaseCamp->GetArmoryInventoryComponent() : nullptr;
}

int32 URpgCraftingStationComponent::GetAvailableInventoryResourceCount(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, const TArray<URpgInventoryManagerComponent*>& ResourceInventories) const
{
	int32 TotalCount = 0;
	for (URpgInventoryManagerComponent* Inventory : ResourceInventories)
	{
		if (Inventory)
		{
			TotalCount += Inventory->GetTotalItemCountByDefinition(ItemDefinition);
		}
	}
	return TotalCount;
}

bool URpgCraftingStationComponent::ConsumeInventoryResources(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count, const TArray<URpgInventoryManagerComponent*>& ResourceInventories) const
{
	int32 RemainingCount = Count;
	for (URpgInventoryManagerComponent* Inventory : ResourceInventories)
	{
		if (!Inventory || RemainingCount <= 0)
		{
			break;
		}

		const int32 AvailableInInventory = Inventory->GetTotalItemCountByDefinition(ItemDefinition);
		const int32 CountToConsume = FMath::Min(AvailableInInventory, RemainingCount);
		if (CountToConsume > 0)
		{
			if (!Inventory->ConsumeItemsByDefinition(ItemDefinition, CountToConsume))
			{
				return false;
			}
			RemainingCount -= CountToConsume;
		}
	}

	return RemainingCount <= 0;
}

bool URpgCraftingStationComponent::ConsumeBaseResources(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count) const
{
	if (Count <= 0)
	{
		return true;
	}

	URpgBaseStorageComponent* BaseStorage = GetLinkedBaseStorage();
	return BaseStorage && BaseStorage->WithdrawResource(ItemDefinition, Count);
}

void URpgCraftingStationComponent::OnRep_CraftingState()
{
	MarkCraftingStateDirty();
}

bool URpgCraftingStationComponent::ConsumeResourcesWithRefund(
	AActor* RequestingActor,
	const TArray<FRpgCraftingResourceCost>& RequiredItems,
	int32 Quantity,
	TArray<FRpgCraftingRefundEntry>& OutRefundEntries)
{
	OutRefundEntries.Reset();
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || Quantity <= 0)
	{
		return false;
	}

	TArray<FRpgCraftingResourceCost> AggregatedRequiredItems;
	if (!TryBuildAggregatedResourceCosts(RequiredItems, AggregatedRequiredItems))
	{
		return false;
	}

	for (FRpgCraftingResourceCost& RequiredItem : AggregatedRequiredItems)
	{
		const int64 RequiredCount = static_cast<int64>(RequiredItem.Count) * static_cast<int64>(Quantity);
		if (RequiredCount > MAX_int32 ||
			GetAvailableResourceCount(RequestingActor, RequiredItem.ItemDefinition) < RequiredCount)
		{
			return false;
		}

		RequiredItem.Count = static_cast<int32>(RequiredCount);
	}

	TArray<URpgInventoryManagerComponent*> ResourceInventories = GetResourceInventories(RequestingActor);
	auto RefundAndFail = [this, &OutRefundEntries]()
	{
		for (const FRpgCraftingRefundEntry& RefundEntry : OutRefundEntries)
		{
			RefundResourceCredit(RefundEntry);
		}
		OutRefundEntries.Reset();
		return false;
	};

	for (const FRpgCraftingResourceCost& RequiredItem : AggregatedRequiredItems)
	{
		int32 RemainingCount = RequiredItem.Count;

		auto ConsumeFromBase = [&]()
		{
			if (RemainingCount <= 0)
			{
				return true;
			}

			URpgBaseStorageComponent* BaseStorage = GetLinkedBaseStorage();
			const int32 AvailableInBase = BaseStorage ? BaseStorage->GetResourceCount(RequiredItem.ItemDefinition) : 0;
			const int32 CountToConsume = FMath::Min(AvailableInBase, RemainingCount);
			if (CountToConsume <= 0)
			{
				return true;
			}

			if (!ConsumeBaseResources(RequiredItem.ItemDefinition, CountToConsume))
			{
				return false;
			}

			AddRefundCredit(OutRefundEntries, RequiredItem.ItemDefinition, CountToConsume, nullptr, true);
			RemainingCount -= CountToConsume;
			return true;
		};

		auto ConsumeFromInventories = [&]()
		{
			if (RemainingCount <= 0)
			{
				return true;
			}

			for (URpgInventoryManagerComponent* Inventory : ResourceInventories)
			{
				if (!Inventory || RemainingCount <= 0)
				{
					break;
				}

				const int32 AvailableInInventory = Inventory->GetTotalItemCountByDefinition(RequiredItem.ItemDefinition);
				const int32 CountToConsume = FMath::Min(AvailableInInventory, RemainingCount);
				if (CountToConsume <= 0)
				{
					continue;
				}

				if (!Inventory->ConsumeItemsByDefinition(RequiredItem.ItemDefinition, CountToConsume))
				{
					return false;
				}

				AddRefundCredit(OutRefundEntries, RequiredItem.ItemDefinition, CountToConsume, Inventory, false);
				RemainingCount -= CountToConsume;
			}

			return true;
		};

		switch (ResourceConsumeOrder)
		{
		case ERpgCraftingResourceConsumeOrder::BaseThenPlayer:
			if (!ConsumeFromBase() || !ConsumeFromInventories())
			{
				return RefundAndFail();
			}
			break;

		case ERpgCraftingResourceConsumeOrder::PlayerThenBase:
			if (!ConsumeFromInventories() || !ConsumeFromBase())
			{
				return RefundAndFail();
			}
			break;

		case ERpgCraftingResourceConsumeOrder::BaseOnly:
			if (!ConsumeFromBase())
			{
				return RefundAndFail();
			}
			break;

		case ERpgCraftingResourceConsumeOrder::PlayerOnly:
			if (!ConsumeFromInventories())
			{
				return RefundAndFail();
			}
			break;
		}

		if (RemainingCount > 0)
		{
			return RefundAndFail();
		}
	}

	return true;
}

void URpgCraftingStationComponent::SpendRefundCreditsForCompletedUnit(FRpgCraftingJobEntry& Job)
{
	if (!Job.Recipe)
	{
		return;
	}

	TArray<FRpgCraftingResourceCost> AggregatedRequiredItems;
	if (!TryBuildAggregatedResourceCosts(Job.Recipe->RequiredResources, AggregatedRequiredItems))
	{
		return;
	}

	auto SpendFromCredits = [&Job](TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32& RemainingCount, bool bPreferBase)
	{
		for (FRpgCraftingRefundEntry& RefundEntry : Job.RefundEntries)
		{
			if (RemainingCount <= 0)
			{
				return;
			}

			if (RefundEntry.ItemDefinition != ItemDefinition || RefundEntry.Count <= 0 || RefundEntry.bRefundToBaseStorage != bPreferBase)
			{
				continue;
			}

			const int32 CountToSpend = FMath::Min(RefundEntry.Count, RemainingCount);
			RefundEntry.Count -= CountToSpend;
			RemainingCount -= CountToSpend;
		}
	};

	for (const FRpgCraftingResourceCost& RequiredItem : AggregatedRequiredItems)
	{
		int32 RemainingCount = RequiredItem.Count;
		switch (ResourceConsumeOrder)
		{
		case ERpgCraftingResourceConsumeOrder::BaseThenPlayer:
			SpendFromCredits(RequiredItem.ItemDefinition, RemainingCount, true);
			SpendFromCredits(RequiredItem.ItemDefinition, RemainingCount, false);
			break;

		case ERpgCraftingResourceConsumeOrder::PlayerThenBase:
			SpendFromCredits(RequiredItem.ItemDefinition, RemainingCount, false);
			SpendFromCredits(RequiredItem.ItemDefinition, RemainingCount, true);
			break;

		case ERpgCraftingResourceConsumeOrder::BaseOnly:
			SpendFromCredits(RequiredItem.ItemDefinition, RemainingCount, true);
			break;

		case ERpgCraftingResourceConsumeOrder::PlayerOnly:
			SpendFromCredits(RequiredItem.ItemDefinition, RemainingCount, false);
			break;
		}
	}

	Job.RefundEntries.RemoveAll([](const FRpgCraftingRefundEntry& RefundEntry)
	{
		return RefundEntry.Count <= 0;
	});
}

void URpgCraftingStationComponent::RefundRemainingJobCosts(FRpgCraftingJobEntry& Job)
{
	for (const FRpgCraftingRefundEntry& RefundEntry : Job.RefundEntries)
	{
		RefundResourceCredit(RefundEntry);
	}

	Job.RefundEntries.Reset();
}

bool URpgCraftingStationComponent::RefundResourceCredit(const FRpgCraftingRefundEntry& RefundEntry)
{
	if (!RefundEntry.ItemDefinition || RefundEntry.Count <= 0)
	{
		return true;
	}

	if (RefundEntry.bRefundToBaseStorage)
	{
		if (URpgBaseStorageComponent* BaseStorage = GetLinkedBaseStorage())
		{
			if (BaseStorage->CanStoreResource(RefundEntry.ItemDefinition, RefundEntry.Count) &&
				BaseStorage->StoreResource(RefundEntry.ItemDefinition, RefundEntry.Count))
			{
				return true;
			}
		}

		return SpawnOrMergeDroppedOutput(RefundEntry.ItemDefinition, RefundEntry.Count);
	}

	if (RefundEntry.Inventory &&
		RefundEntry.Inventory->CanAddItemDefinition(RefundEntry.ItemDefinition, RefundEntry.Count))
	{
		RefundEntry.Inventory->GrantItemDefinition(RefundEntry.ItemDefinition, RefundEntry.Count);
		return true;
	}

	return SpawnOrMergeDroppedOutput(RefundEntry.ItemDefinition, RefundEntry.Count);
}

void URpgCraftingStationComponent::TryStartNextQueuedJob()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || bStationPaused || HasActiveOrPausedJob())
	{
		return;
	}

	for (int32 JobIndex = 0; JobIndex < CraftingJobs.Num(); ++JobIndex)
	{
		if (CraftingJobs[JobIndex].State == ERpgCraftingJobState::Queued)
		{
			StartJobAtIndex(JobIndex);
			return;
		}
	}
}

void URpgCraftingStationComponent::StartJobAtIndex(int32 JobIndex, float DurationOverride, bool bPauseStateChanged)
{
	if (!CraftingJobs.IsValidIndex(JobIndex))
	{
		return;
	}

	FRpgCraftingJobEntry& Job = CraftingJobs[JobIndex];
	if (!Job.Recipe || Job.QuantityCompleted >= Job.QuantityTotal)
	{
		CraftingJobs.RemoveAt(JobIndex);
		MarkCraftingStateDirty();
		TryStartNextQueuedJob();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Now = GetServerWorldTimeSeconds();
	const float FullCraftDuration = GetRecipeCraftTime(Job.Recipe);
	const float RemainingDuration = DurationOverride >= 0.0f ? FMath::Max(0.0f, DurationOverride) : FullCraftDuration;
	const float PreviousElapsedDuration = DurationOverride >= 0.0f ? FMath::Max(0.0f, FullCraftDuration - RemainingDuration) : 0.0f;
	Job.State = ERpgCraftingJobState::Active;
	Job.StartServerTime = Now - PreviousElapsedDuration;
	Job.FinishServerTime = Now + RemainingDuration;
	Job.PausedRemainingTime = 0.0f;

	World->GetTimerManager().ClearTimer(CraftingTimerHandle);
	if (RemainingDuration <= 0.0f)
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ThisClass::CompleteActiveJobUnit));
	}
	else
	{
		World->GetTimerManager().SetTimer(CraftingTimerHandle, this, &ThisClass::CompleteActiveJobUnit, RemainingDuration, false);
	}

	MarkCraftingStateDirty(Job.JobId, Job.State, bPauseStateChanged);
}

void URpgCraftingStationComponent::CompleteActiveJobUnit()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	const int32 ActiveJobIndex = FindActiveJobIndex();
	if (ActiveJobIndex == INDEX_NONE || !CraftingJobs.IsValidIndex(ActiveJobIndex))
	{
		TryStartNextQueuedJob();
		return;
	}

	FRpgCraftingJobEntry& Job = CraftingJobs[ActiveJobIndex];
	if (!Job.Recipe || Job.State != ERpgCraftingJobState::Active)
	{
		return;
	}

	if (!AddCraftingOutputs(Job.Recipe->OutputItems))
	{
		Job.State = ERpgCraftingJobState::BlockedOutput;
		MarkCraftingStateDirty(Job.JobId, Job.State);
		return;
	}

	SpendRefundCreditsForCompletedUnit(Job);
	++Job.QuantityCompleted;

	if (Job.QuantityCompleted >= Job.QuantityTotal)
	{
		const FGuid FinishedJobId = Job.JobId;
		CraftingJobs.RemoveAt(ActiveJobIndex);
		MarkCraftingStateDirty(FinishedJobId, ERpgCraftingJobState::Completed);
		TryStartNextQueuedJob();
		return;
	}

	StartJobAtIndex(ActiveJobIndex);
}

int32 URpgCraftingStationComponent::FindActiveJobIndex() const
{
	for (int32 JobIndex = 0; JobIndex < CraftingJobs.Num(); ++JobIndex)
	{
		const ERpgCraftingJobState State = CraftingJobs[JobIndex].State;
		if (State == ERpgCraftingJobState::Active ||
			State == ERpgCraftingJobState::Paused ||
			State == ERpgCraftingJobState::BlockedOutput)
		{
			return JobIndex;
		}
	}

	return INDEX_NONE;
}

int32 URpgCraftingStationComponent::FindJobIndex(FGuid JobId) const
{
	if (!JobId.IsValid())
	{
		return INDEX_NONE;
	}

	for (int32 JobIndex = 0; JobIndex < CraftingJobs.Num(); ++JobIndex)
	{
		if (CraftingJobs[JobIndex].JobId == JobId)
		{
			return JobIndex;
		}
	}

	return INDEX_NONE;
}

bool URpgCraftingStationComponent::HasActiveOrPausedJob() const
{
	return FindActiveJobIndex() != INDEX_NONE;
}

float URpgCraftingStationComponent::GetServerWorldTimeSeconds() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const AGameStateBase* GameState = World->GetGameState())
		{
			return GameState->GetServerWorldTimeSeconds();
		}

		return World->GetTimeSeconds();
	}

	return 0.0f;
}

float URpgCraftingStationComponent::GetRecipeCraftTime(const URpgCraftingRecipeDefinition* RecipeDefinition) const
{
	return RecipeDefinition ? FMath::Max(0.0f, RecipeDefinition->CraftTime) : 0.0f;
}

bool URpgCraftingStationComponent::AddOutputItemOrDrop(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count)
{
	if (!ItemDefinition || Count <= 0)
	{
		return false;
	}

	int32 RemainingCount = Count;
	const bool bAutoDeposit = ShouldAutoDepositCraftingOutputs();
	if (bAutoDeposit && IsMaterialDefinition(ItemDefinition))
	{
		if (URpgBaseStorageComponent* BaseStorage = GetLinkedBaseStorage())
		{
			const int32 CountToStore = FMath::Min(RemainingCount, BaseStorage->GetFreeResourceCapacity(ItemDefinition));
			if (CountToStore > 0 && BaseStorage->StoreResource(ItemDefinition, CountToStore))
			{
				RemainingCount -= CountToStore;
			}
		}
	}
	else if (bAutoDeposit && bAutoDepositInstanceOutputsToArmory)
	{
		if (URpgInventoryManagerComponent* ArmoryInventory = GetLinkedArmoryInventory())
		{
			if (ArmoryInventory->CanAddItemDefinition(ItemDefinition, RemainingCount))
			{
				ArmoryInventory->GrantItemDefinition(ItemDefinition, RemainingCount);
				RemainingCount = 0;
			}
		}
	}

	while (RemainingCount > 0 && OutputInventoryComponent)
	{
		int32 CountToAdd = RemainingCount;
		while (CountToAdd > 0 && !OutputInventoryComponent->CanAddItemDefinition(ItemDefinition, CountToAdd))
		{
			--CountToAdd;
		}

		if (CountToAdd <= 0)
		{
			break;
		}

		OutputInventoryComponent->GrantItemDefinition(ItemDefinition, CountToAdd);
		RemainingCount -= CountToAdd;
	}

	return RemainingCount <= 0 || SpawnOrMergeDroppedOutput(ItemDefinition, RemainingCount);
}

bool URpgCraftingStationComponent::SpawnOrMergeDroppedOutput(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count)
{
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !World || !ItemDefinition || Count <= 0)
	{
		return false;
	}

	if (TryMergeDroppedOutput(ItemDefinition, Count))
	{
		return true;
	}

	TSubclassOf<ARpgDroppedInventoryActor> DropClass = DroppedOutputActorClass;
	if (!DropClass)
	{
		DropClass = ARpgDroppedInventoryActor::StaticClass();
	}
	if (!DropClass)
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = OwnerActor;
	SpawnParameters.Instigator = Cast<APawn>(OwnerActor);
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	const FVector SpawnLocation = OwnerActor->GetActorLocation() + OwnerActor->GetActorForwardVector() * 80.0f + FVector(0.0f, 0.0f, 40.0f);
	ARpgDroppedInventoryActor* DropActor = World->SpawnActor<ARpgDroppedInventoryActor>(DropClass, SpawnLocation, OwnerActor->GetActorRotation(), SpawnParameters);
	if (!DropActor)
	{
		return false;
	}

	FInventoryPickup Pickup;
	FPickupTemplate& Template = Pickup.Templates.AddDefaulted_GetRef();
	Template.ItemDef = ItemDefinition;
	Template.StackCount = Count;
	DropActor->SetPickupInventory(Pickup);
	return true;
}

bool URpgCraftingStationComponent::TryMergeDroppedOutput(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count) const
{
	const AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!OwnerActor || !World || !ItemDefinition || Count <= 0 || OutputDropMergeRadius <= 0.0f)
	{
		return false;
	}

	const float MergeRadiusSq = FMath::Square(OutputDropMergeRadius);
	const FVector Origin = OwnerActor->GetActorLocation();
	for (TActorIterator<ARpgDroppedInventoryActor> It(World); It; ++It)
	{
		ARpgDroppedInventoryActor* DropActor = *It;
		if (!DropActor || DropActor->IsPendingKillPending())
		{
			continue;
		}

		if (FVector::DistSquared(Origin, DropActor->GetActorLocation()) > MergeRadiusSq)
		{
			continue;
		}

		if (DropActor->CanMergePickupTemplate(ItemDefinition) && DropActor->MergePickupTemplate(ItemDefinition, Count))
		{
			return true;
		}
	}

	return false;
}

void URpgCraftingStationComponent::MarkCraftingStateDirty(FGuid ChangedJobId, ERpgCraftingJobState ChangedState, bool bPauseStateChanged)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || !IsRegistered() || HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return;
	}

	if (AActor* OwnerActor = GetOwner())
	{
		if (OwnerActor->HasAuthority())
		{
			++CraftingStateRevision;
			OwnerActor->ForceNetUpdate();
		}
	}

	FRpgCraftingStationChangeMessage Message;
	Message.Station = const_cast<URpgCraftingStationComponent*>(this);
	Message.JobId = ChangedJobId;
	Message.JobState = ChangedState;
	Message.bPauseStateChanged = bPauseStateChanged;

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
	MessageSubsystem.BroadcastMessage(TAG_Rpg_Crafting_Message_StationChanged, Message);
}
