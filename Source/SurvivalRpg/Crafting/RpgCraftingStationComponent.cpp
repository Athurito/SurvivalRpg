#include "RpgCraftingStationComponent.h"

#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "NativeGameplayTags.h"
#include "Templates/UnrealTemplate.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/Base/RpgBaseCampActor.h"
#include "SurvivalRpg/Base/RpgBaseStorageComponent.h"
#include "SurvivalRpg/Base/RpgStorageAccessRules.h"
#include "SurvivalRpg/Base/RpgBaseStorageStationComponent.h"
#include "SurvivalRpg/Base/RpgWorldStorageKnowledgeComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameStateBase.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Crafting/RpgCraftingRecipeDefinition.h"
#include "SurvivalRpg/Crafting/RpgRecipeUnlockComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"
#include "SurvivalRpg/Inventory/RpgDroppedInventoryActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_StorageProfile.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgPhysicalStorageTypes.h"
#include "SurvivalRpg/System/RpgAssetManager.h"
#include "SurvivalRpg/System/RpgGameData.h"
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

	OpenCraftingOption.InteractionTag = RpgGameplayTags::Rpg_Interaction_Action_OpenCrafting;
	OpenCraftingOption.Prompt.ActionText = NSLOCTEXT("RpgCrafting", "OpenCraftingStationText", "Open");
	OpenCraftingOption.Prompt.TargetText = NSLOCTEXT("RpgCrafting", "OpenCraftingStationSubText", "Crafting Station");
	OpenCraftingOption.Prompt.InteractionPriority = 50;
}

void URpgCraftingStationComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ARpgGameModeBase* GameMode = GetWorld()->GetAuthGameMode<ARpgGameModeBase>())
	{
		GameMode->RegisterPersistentCraftingStation(this);
	}
	if (OutputInventoryComponent)
	{
		SetOutputInventoryManager(OutputInventoryComponent);
	}
}

void URpgCraftingStationComponent::SetPersistenceRestorePending(bool bPending)
{
	if (GetOwner() && GetOwner()->HasAuthority()) bPersistenceRestorePending = bPending;
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
	FInteractionOption Option = OpenCraftingOption;
	if (!Option.InteractionAbilityToGrant)
	{
		Option.InteractionAbilityToGrant = URpgAssetManager::GetSubclass(
			URpgGameData::Get().OpenCraftingInteractionAbility);
	}
	Option.InteractionTag = RpgGameplayTags::Rpg_Interaction_Action_OpenCrafting;
	Option.TargetRef.TargetActor = GetOwner();
	Option.Prompt.InteractionRange = InteractionRadius > 0.0f
		? InteractionRadius
		: Option.Prompt.InteractionRange;

	const AActor* RequestingActor = InteractQuery.RequestingAvatar.Get();
	const APawn* RequestingPawn = Cast<APawn>(RequestingActor);
	const AController* RequestingController = Cast<AController>(RequestingActor);
	if (!RequestingController && RequestingPawn)
	{
		RequestingController = RequestingPawn->GetController();
	}
	const bool bSemanticallyAccessible = !bPersistenceRestorePending && GetOwner() && RequestingController && RequestingController->IsPlayerController();
	Option.Availability = bSemanticallyAccessible
		? ERpgInteractionAvailability::Available
		: ERpgInteractionAvailability::Blocked;
	if (!bSemanticallyAccessible)
	{
		Option.Prompt.BlockedReason = NSLOCTEXT("RpgCrafting", "CraftingStationUnavailable", "Crafting station is unavailable");
	}
	InteractionBuilder.AddInteractionOption(Option);
}

namespace
{

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
	NewRefundEntry.InventoryId = RpgStorageAccessRules::GetPersistentInventoryId(Inventory);
		NewRefundEntry.bRefundToBaseStorage = bRefundToBaseStorage;
	}
}

URpgInventoryManagerComponent* URpgCraftingStationComponent::FindRequestingPlayerInventory(const AActor* RequestingActor)
{
	if (!RequestingActor) { return nullptr; }
	const APawn* Pawn = Cast<APawn>(RequestingActor);
	const AController* Controller = Cast<AController>(RequestingActor);
	if (!Controller && Pawn) { Controller = Pawn->GetController(); }
	const APlayerState* PlayerState = Controller ? Controller->PlayerState.Get() : (Pawn ? Pawn->GetPlayerState() : nullptr);
	if (PlayerState)
	{
		if (URpgInventoryManagerComponent* Inventory = PlayerState->FindComponentByClass<URpgInventoryManagerComponent>()) { return Inventory; }
	}
	return RequestingActor->FindComponentByClass<URpgInventoryManagerComponent>();
}

ARpgBaseCampActor* URpgCraftingStationComponent::ResolveSpatialBaseCamp() const
{
	return GetOwner() ? RpgStorageAccessRules::ResolveBaseAtLocation(GetWorld(), GetOwner()->GetActorLocation()) : nullptr;
}

TArray<URpgInventoryManagerComponent*> URpgCraftingStationComponent::GetResourceInventories(AActor* RequestingActor) const
{
	TArray<URpgInventoryManagerComponent*> Results;
	if (!RequestingActor || !GetOwner()) { return Results; }
	if (URpgInventoryManagerComponent* Inventory = FindRequestingPlayerInventory(RequestingActor)) { Results.Add(Inventory); }
	TArray<URpgInventoryManagerComponent*> StorageSources;
	RpgStorageAccessRules::ResolveStorageSources(GetWorld(), GetOwner()->GetActorLocation(), StorageSearchRadius, StorageSources);
	for (URpgInventoryManagerComponent* Inventory : StorageSources) { Results.AddUnique(Inventory); }
	return Results;
}

void URpgCraftingStationComponent::SetLinkedBaseCamp(ARpgBaseCampActor* NewBaseCamp)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	// Spatial membership is authoritative; stale authored links cannot expand access.
	LinkedBaseCamp = ResolveSpatialBaseCamp();
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

	if (!RecipeDefinition->RequiredWorldKnowledgeTags.IsEmpty())
	{
		const ARpgGameStateBase* GameState = GetWorld()
			? GetWorld()->GetGameState<ARpgGameStateBase>()
			: nullptr;
		const URpgWorldStorageKnowledgeComponent* Knowledge = GameState
			? GameState->GetWorldStorageKnowledgeComponent()
			: nullptr;
		if (!Knowledge || !Knowledge->HasAllKnowledgeTags(
			RecipeDefinition->RequiredWorldKnowledgeTags))
		{
			return false;
		}
	}

	const ARpgBaseCampActor* SpatialBase = ResolveSpatialBaseCamp();
	const FGameplayTagContainer BaseUpgradeTags = SpatialBase ? SpatialBase->GetGrantedStorageUpgradeTags() : FGameplayTagContainer();
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
	if (bMutationInProgress) { return false; }
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	if (!GetOwner() || !GetOwner()->HasAuthority() || !OutputInventoryComponent || !CanCraftRecipeQuantity(RequestingActor, RecipeDefinition, Quantity)) { return false; }
	const TFunction<bool()> Revalidate = MakeContextRevalidator(RequestingActor);
	TArray<FRpgInventoryBatchOperation> Operations;
	TArray<FRpgCraftingRefundEntry> RefundEntries;
	if (!BuildResourceConsumptionPlan(RequestingActor, GetResourceInventories(RequestingActor), RecipeDefinition->RequiredResources, Quantity, Operations, RefundEntries)) { return false; }
	// The job and its credits exist before inventory observers may save the committed resource debit.
	auto CommitJob = [this, RecipeDefinition, Quantity, &RefundEntries]()
	{
		FRpgCraftingJobEntry& Job = CraftingJobs.AddDefaulted_GetRef();
		Job.JobId = FGuid::NewGuid();
		Job.Recipe = RecipeDefinition;
		Job.QuantityTotal = Quantity;
		Job.RefundEntries = MoveTemp(RefundEntries);
		MarkCraftingStateDirty(Job.JobId, Job.State);
	};
	if (!Revalidate()) { return false; }
	if (Operations.IsEmpty()) { CommitJob(); }
	else if (!OutputInventoryComponent->ApplyInventoryBatch(Operations, FGuid::NewGuid(), {}, CommitJob, Revalidate).IsSuccess()) { return false; }
	TryStartNextQueuedJob();
	return true;
}

bool URpgCraftingStationComponent::CraftRecipe(AActor* RequestingActor, URpgCraftingRecipeDefinition* RecipeDefinition)
{
	return QueueCraftRecipe(RequestingActor, RecipeDefinition, 1);
}

bool URpgCraftingStationComponent::CancelCraftJob(AActor* RequestingActor, FGuid JobId)
{
	if (bMutationInProgress) { return false; }
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	if (!GetOwner() || !GetOwner()->HasAuthority() || !CanActorAccess(RequestingActor)) { return false; }
	const int32 JobIndex = FindJobIndex(JobId);
	if (JobIndex == INDEX_NONE) { return false; }
	const ERpgCraftingJobState State = CraftingJobs[JobIndex].State;
	const bool bWasActive = State == ERpgCraftingJobState::Active || State == ERpgCraftingJobState::Paused || State == ERpgCraftingJobState::BlockedOutput;
	if (!RefundRemainingJobCosts(JobId, [this, JobId, State, bWasActive]()
	{
		if (bWasActive)
		{
			GetWorld()->GetTimerManager().ClearTimer(CraftingTimerHandle);
			GetWorld()->GetTimerManager().ClearTimer(OutputRetryTimerHandle);
		}
		CraftingJobs.RemoveAt(FindJobIndex(JobId));
		MarkCraftingStateDirty(JobId, State);
	})) { return false; }
	TryStartNextQueuedJob();
	return true;
}

bool URpgCraftingStationComponent::PauseCraftingStation(AActor* RequestingActor)
{
	if (bMutationInProgress) { return false; }
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
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
	if (bMutationInProgress) { return false; }
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
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
	if (!ItemDefinition) { return 0; }
	URpgInventoryManagerComponent* PlayerInventory = FindRequestingPlayerInventory(RequestingActor);
	const URpgInventoryFragment_StorageProfile* Profile = URpgInventoryFragment_StorageProfile::ResolveStorageProfile(ItemDefinition);
	int64 Count = 0;
	for (URpgInventoryManagerComponent* Inventory : GetResourceInventories(RequestingActor))
	{
		if (Inventory != PlayerInventory && (!Profile || !Profile->CanCraftFromPhysicalStorage())) { continue; }
		Count += GetAvailableInventoryResourceCount(ItemDefinition, { Inventory });
	}
	return static_cast<int32>(FMath::Min<int64>(Count, MAX_int32));
}

bool URpgCraftingStationComponent::ConsumeResources(AActor* RequestingActor, const TArray<FRpgCraftingResourceCost>& RequiredItems)
{
	if (bMutationInProgress) { return false; }
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	TArray<FRpgCraftingRefundEntry> RefundEntries;
	return CanActorAccess(RequestingActor) && ConsumeResourcesWithRefund(RequestingActor, RequiredItems, 1, RefundEntries);
}

bool URpgCraftingStationComponent::CanActorAccess(const AActor* RequestingActor) const
{
	const AActor* OwnerActor = GetOwner();
	if (bPersistenceRestorePending || !OwnerActor || !RequestingActor)
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

	const AActor* Avatar = RequestingController->GetPawn() ? RequestingController->GetPawn() : RequestingActor;
	return FVector::DistSquared(OwnerActor->GetActorLocation(), Avatar->GetActorLocation()) <= FMath::Square(InteractionRadius);
}

void URpgCraftingStationComponent::SetOutputInventoryManager(URpgInventoryManagerComponent* InOutputInventory)
{
	if (bMutationInProgress) { return; }
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
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
	if (bPersistenceRestorePending || bMutationInProgress) { return false; }
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	if (!GetOwner() || !GetOwner()->HasAuthority() || !OutputInventoryComponent || !CanAcceptCraftingOutputs(OutputItems)) { return false; }
	const TFunction<bool()> Revalidate = MakeContextRevalidator(nullptr);
	TArray<FRpgInventoryBatchOperation> Operations;
	return Revalidate() && BuildOutputPlan(OutputItems, Operations) && OutputInventoryComponent->ApplyInventoryBatch(Operations, FGuid::NewGuid(), {}, {}, Revalidate).IsSuccess();
}

bool URpgCraftingStationComponent::CraftItems(AActor* RequestingActor, const TArray<FRpgCraftingResourceCost>& RequiredItems, const TArray<FRpgCraftingOutputItem>& OutputItems)
{
	if (bMutationInProgress) { return false; }
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	if (!GetOwner() || !GetOwner()->HasAuthority() || !OutputInventoryComponent || !CanActorAccess(RequestingActor) || !CanAcceptCraftingOutputs(OutputItems)) { return false; }
	const TFunction<bool()> Revalidate = MakeContextRevalidator(RequestingActor);
	TArray<FRpgInventoryBatchOperation> Operations;
	TArray<FRpgCraftingRefundEntry> Refunds;
	return BuildResourceConsumptionPlan(RequestingActor, GetResourceInventories(RequestingActor), RequiredItems, 1, Operations, Refunds) &&
		BuildOutputPlan(OutputItems, Operations) && OutputInventoryComponent->ApplyInventoryBatch(Operations, FGuid::NewGuid(), {}, {}, Revalidate).IsSuccess();
}

bool URpgCraftingStationComponent::FlushOutputToBaseStorage()
{
	if (bPersistenceRestorePending || bMutationInProgress) { return false; }
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	return FlushOutputToBaseStorageInternal();
}

bool URpgCraftingStationComponent::FlushOutputToBaseStorageInternal()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !OutputInventoryComponent || !ShouldAutoDepositCraftingOutputs()) { return false; }
	const TFunction<bool()> Revalidate = MakeContextRevalidator(nullptr);
	TArray<FRpgInventoryBatchOperation> Operations;
	for (const FRpgInventoryEntryView& Entry : OutputInventoryComponent->GetAllEntries())
	{
		if (!Entry.Instance || !Entry.Instance->CanCollapseIntoDefinitionCount()) { continue; }
		TArray<URpgInventoryManagerComponent*> Targets;
		RpgStorageAccessRules::ResolveDepositTargets(GetWorld(), GetOwner()->GetActorLocation(), StorageSearchRadius, Entry.Instance->GetItemDef(), Targets);
		int32 Remaining = Entry.StackCount;
		for (URpgInventoryManagerComponent* Target : Targets)
		{
			if (!Target || Target == OutputInventoryComponent || Remaining <= 0) { continue; }
			FRpgInventoryBatchOperation Operation;
			Operation.SourceInventory = OutputInventoryComponent;
			Operation.TargetInventory = Target;
			Operation.ItemId = Entry.ItemId;
			Operation.ExpectedSourceRevision = OutputInventoryComponent->GetInventoryRevision();
			Operation.ExpectedTargetRevision = Target->GetInventoryRevision();
			int32 Low = 0, High = Remaining;
			while (Low < High)
			{
				Operation.Quantity = Low + (High - Low + 1) / 2;
				Operations.Add(Operation);
				ERpgInventoryMutationResultCode Code;
				const bool bFits = OutputInventoryComponent->CanApplyInventoryBatch(Operations, Code);
				Operations.Pop();
				if (bFits) { Low = Operation.Quantity; } else { High = Operation.Quantity - 1; }
			}
			if (Low > 0) { Operation.Quantity = Low; Operations.Add(Operation); Remaining -= Low; }
		}
	}
	return !Operations.IsEmpty() && OutputInventoryComponent->ApplyInventoryBatch(Operations, FGuid::NewGuid(), {}, {}, Revalidate).IsSuccess();
}

bool URpgCraftingStationComponent::HasCraftingOutputAutoDepositAccess() const
{
	return true;
}

bool URpgCraftingStationComponent::SetCraftingOutputAutoDepositEnabled(AActor* RequestingActor, bool bEnabled)
{
	if (bMutationInProgress) { return false; }
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
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
		FlushOutputToBaseStorageInternal();
	}

	return true;
}

bool URpgCraftingStationComponent::ShouldAutoDepositCraftingOutputs() const
{
	return bAutoDepositCraftingOutputsEnabled;
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
	ARpgBaseCampActor* Base = ResolveSpatialBaseCamp();
	return Base ? Base->GetBaseStorageComponent() : nullptr;
}



int32 URpgCraftingStationComponent::GetAvailableInventoryResourceCount(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, const TArray<URpgInventoryManagerComponent*>& ResourceInventories) const
{
	int64 TotalCount = 0;
	for (URpgInventoryManagerComponent* Inventory : ResourceInventories)
	{
		if (!Inventory)
		{
			continue;
		}

		for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
		{
			if (Entry.Instance && Entry.Instance->GetItemDef() == ItemDefinition &&
				Entry.StackCount > 0 &&
				Entry.Instance->CanCollapseIntoDefinitionCount())
			{
				TotalCount += Entry.StackCount;
			}
		}
	}
	return static_cast<int32>(FMath::Min<int64>(TotalCount, MAX_int32));
}





void URpgCraftingStationComponent::OnRep_CraftingState()
{
	MarkCraftingStateDirty();
}

bool URpgCraftingStationComponent::BuildResourceConsumptionPlan(
	AActor* RequestingActor, const TArray<URpgInventoryManagerComponent*>& StorageInventories,
	const TArray<FRpgCraftingResourceCost>& RequiredItems, int32 Quantity,
	TArray<FRpgInventoryBatchOperation>& OutOperations, TArray<FRpgCraftingRefundEntry>& OutRefundEntries)
{
	OutOperations.Reset();
	OutRefundEntries.Reset();
	if (!RequestingActor || Quantity <= 0) { return false; }
	TArray<FRpgCraftingResourceCost> Costs;
	if (!TryBuildAggregatedResourceCosts(RequiredItems, Costs)) { return false; }
	URpgInventoryManagerComponent* PlayerInventory = FindRequestingPlayerInventory(RequestingActor);
	TArray<URpgInventoryManagerComponent*> Sources;
	if (PlayerInventory) { Sources.Add(PlayerInventory); }
	for (URpgInventoryManagerComponent* Inventory : StorageInventories)
	{
		if (!Inventory || !Inventory->GetOwner()) { continue; }
		// Defense in depth: an accidental broad caller list cannot consume another player's inventory.
		const AActor* SourceOwner = Inventory->GetOwner();
		if (Inventory != PlayerInventory && (SourceOwner->IsA<APlayerState>() || SourceOwner->IsA<APawn>() || SourceOwner->IsA<AController>())) { continue; }
		Sources.AddUnique(Inventory);
	}
	for (const FRpgCraftingResourceCost& Cost : Costs)
	{
		const int64 Required = static_cast<int64>(Cost.Count) * Quantity;
		if (Required > MAX_int32) { return false; }
		int32 Remaining = static_cast<int32>(Required);
		const URpgInventoryFragment_StorageProfile* Profile = URpgInventoryFragment_StorageProfile::ResolveStorageProfile(Cost.ItemDefinition);
		for (URpgInventoryManagerComponent* Inventory : Sources)
		{
			if (Inventory != PlayerInventory && (!Profile || !Profile->CanCraftFromPhysicalStorage())) { continue; }
			for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
			{
				if (Remaining <= 0) { break; }
				if (!Entry.Instance || Entry.Instance->GetItemDef() != Cost.ItemDefinition || Entry.StackCount <= 0 || !Entry.Instance->CanCollapseIntoDefinitionCount()) { continue; }
				const int32 Debit = FMath::Min(Remaining, Entry.StackCount);
				FRpgInventoryBatchOperation& Operation = OutOperations.AddDefaulted_GetRef();
				Operation.SourceInventory = Inventory;
				Operation.ItemId = Entry.ItemId;
				Operation.Quantity = Debit;
				Operation.ExpectedSourceRevision = Inventory->GetInventoryRevision();
				AddRefundCredit(OutRefundEntries, Cost.ItemDefinition, Debit, Inventory, false);
				Remaining -= Debit;
			}
		}
		if (Remaining > 0) { OutOperations.Reset(); OutRefundEntries.Reset(); return false; }
	}
	return true;
}

bool URpgCraftingStationComponent::ConsumeResourcesWithRefund(AActor* RequestingActor,
	const TArray<FRpgCraftingResourceCost>& RequiredItems, int32 Quantity, TArray<FRpgCraftingRefundEntry>& OutRefundEntries)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return false; }
	const TFunction<bool()> Revalidate = MakeContextRevalidator(RequestingActor);
	TArray<FRpgInventoryBatchOperation> Operations;
	if (!BuildResourceConsumptionPlan(RequestingActor, GetResourceInventories(RequestingActor), RequiredItems, Quantity, Operations, OutRefundEntries)) { return false; }
	if (Operations.IsEmpty()) { return true; }
	URpgInventoryManagerComponent* Coordinator = Operations[0].SourceInventory;
	if (!Coordinator || !Coordinator->ApplyInventoryBatch(Operations, FGuid::NewGuid(), {}, {}, Revalidate).IsSuccess()) { OutRefundEntries.Reset(); return false; }
	return true;
}

void URpgCraftingStationComponent::SpendRefundCreditsForCompletedUnit(FRpgCraftingJobEntry& Job)
{
	if (!Job.Recipe) { return; }
	TArray<FRpgCraftingResourceCost> Costs;
	if (!TryBuildAggregatedResourceCosts(Job.Recipe->RequiredResources, Costs)) { return; }
	for (const FRpgCraftingResourceCost& Cost : Costs)
	{
		int32 Remaining = Cost.Count;
		for (FRpgCraftingRefundEntry& Refund : Job.RefundEntries)
		{
			if (Refund.ItemDefinition != Cost.ItemDefinition || Remaining <= 0) { continue; }
			const int32 Spent = FMath::Min(Remaining, Refund.Count);
			Refund.Count -= Spent;
			Remaining -= Spent;
		}
	}
	Job.RefundEntries.RemoveAll([](const FRpgCraftingRefundEntry& Refund) { return Refund.Count <= 0; });
}

bool URpgCraftingStationComponent::RefundRemainingJobCosts(FGuid JobId, TFunction<void()> CommitSideEffects)
{
	const int32 JobIndex = FindJobIndex(JobId);
	if (!OutputInventoryComponent || !CraftingJobs.IsValidIndex(JobIndex)) { return false; }
	const TFunction<bool()> Revalidate = MakeContextRevalidator(nullptr, true, JobId);
	const TArray<FRpgCraftingRefundEntry> RefundEntries = CraftingJobs[JobIndex].RefundEntries;
	TArray<FRpgInventoryBatchOperation> Operations;
	for (const FRpgCraftingRefundEntry& Refund : RefundEntries)
	{
		if (!Refund.ItemDefinition || Refund.Count <= 0) { return false; }
		URpgInventoryManagerComponent* Original = Refund.InventoryId.IsNone()
			? Refund.Inventory.Get() : RpgStorageAccessRules::FindPersistentInventory(GetWorld(), Refund.InventoryId);
		if (!IsValid(Original) || !IsValid(Original->GetOwner()) || Original->GetOwner()->IsActorBeingDestroyed()) { Original = nullptr; }
		int32 Remaining = Refund.Count;
		TArray<URpgInventoryManagerComponent*> Targets;
		if (Original) { Targets.Add(Original); }
		Targets.AddUnique(OutputInventoryComponent);
		for (URpgInventoryManagerComponent* Target : Targets)
		{
			FRpgInventoryBatchOperation Operation;
			Operation.TargetInventory = Target;
			Operation.ItemDefinition = Refund.ItemDefinition;
			Operation.ExpectedTargetRevision = Target->GetInventoryRevision();
			int32 Low = 0, High = Remaining;
			while (Low < High)
			{
				Operation.Quantity = Low + (High - Low + 1) / 2;
				Operations.Add(Operation);
				ERpgInventoryMutationResultCode Code;
				const bool bFits = OutputInventoryComponent->CanApplyInventoryBatch(Operations, Code);
				Operations.Pop();
				if (bFits) { Low = Operation.Quantity; } else { High = Operation.Quantity - 1; }
			}
			if (Low > 0) { Operation.Quantity = Low; Operations.Add(Operation); Remaining -= Low; }
		}
		if (Remaining > 0) { return false; }
	}
	auto CommitRefund = [this, JobId, &CommitSideEffects]()
	{
		CraftingJobs[FindJobIndex(JobId)].RefundEntries.Reset();
		if (CommitSideEffects) { CommitSideEffects(); }
	};
	if (!Revalidate()) { return false; }
	if (Operations.IsEmpty()) { CommitRefund(); return true; }
	return OutputInventoryComponent->ApplyInventoryBatch(Operations, FGuid::NewGuid(), {}, CommitRefund, Revalidate).IsSuccess();
}



void URpgCraftingStationComponent::TryStartNextQueuedJob()
{
	if (bPersistenceRestorePending) return;
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
	if (bPersistenceRestorePending) return;
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
		CraftingTimerHandle = World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ThisClass::CompleteActiveJobUnit));
	}
	else
	{
		World->GetTimerManager().SetTimer(CraftingTimerHandle, this, &ThisClass::CompleteActiveJobUnit, RemainingDuration, false);
	}

	MarkCraftingStateDirty(Job.JobId, Job.State, bPauseStateChanged);
}

void URpgCraftingStationComponent::CompleteActiveJobUnit()
{
	if (bPersistenceRestorePending) return;
	if (bMutationInProgress)
	{
		if (UWorld* World = GetWorld()) { World->GetTimerManager().SetTimer(CraftingTimerHandle, this, &ThisClass::CompleteActiveJobUnit, 0.01f, false); }
		return;
	}
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	CompleteActiveJobUnitInternal();
}

void URpgCraftingStationComponent::CompleteActiveJobUnitInternal()
{
	if (bPersistenceRestorePending) return;
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
	const int32 Index = FindActiveJobIndex();
	if (!CraftingJobs.IsValidIndex(Index)) { TryStartNextQueuedJob(); return; }
	const FRpgCraftingJobEntry Job = CraftingJobs[Index];
	if (!Job.Recipe || Job.State != ERpgCraftingJobState::Active) { return; }
	const FGuid JobId = Job.JobId;
	const TFunction<bool()> Revalidate = MakeContextRevalidator(nullptr, false, JobId);
	TArray<FRpgInventoryBatchOperation> Operations;
	const bool bPrepared = OutputInventoryComponent && BuildOutputPlan(Job.Recipe->OutputItems, Operations);
	const bool bCommitted = bPrepared && Revalidate() && OutputInventoryComponent->ApplyInventoryBatch(Operations, FGuid::NewGuid(), {}, [this, JobId]()
	{
		const int32 CommittedIndex = FindJobIndex(JobId);
		FRpgCraftingJobEntry& CommittedJob = CraftingJobs[CommittedIndex];
		SpendRefundCreditsForCompletedUnit(CommittedJob);
		++CommittedJob.QuantityCompleted;
		if (CommittedJob.QuantityCompleted >= CommittedJob.QuantityTotal)
		{
			const FGuid FinishedId = CommittedJob.JobId;
			CraftingJobs.RemoveAt(CommittedIndex);
			MarkCraftingStateDirty(FinishedId, ERpgCraftingJobState::Completed);
		}
		else
		{
			// Snapshotting between units persists a queued unit, never an already-produced active unit.
			CommittedJob.State = ERpgCraftingJobState::Queued;
			MarkCraftingStateDirty(CommittedJob.JobId, CommittedJob.State);
		}
	}, Revalidate).IsSuccess();
	if (!bCommitted)
	{
		if (!IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed() || !GetWorld()) { return; }
		const int32 CurrentIndex = FindJobIndex(JobId);
		if (!CraftingJobs.IsValidIndex(CurrentIndex)) { return; }
		CraftingJobs[CurrentIndex].State = ERpgCraftingJobState::BlockedOutput;
		GetWorld()->GetTimerManager().SetTimer(OutputRetryTimerHandle, this, &ThisClass::RetryBlockedOutput, 0.5f, false);
		MarkCraftingStateDirty(JobId, ERpgCraftingJobState::BlockedOutput);
		return;
	}
	TryStartNextQueuedJob();
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







void URpgCraftingStationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		// Preserve paid jobs and tray items before this actor disappears from world enumeration.
		if (ARpgGameModeBase* GameMode = World->GetAuthGameMode<ARpgGameModeBase>())
		{
			GameMode->UnregisterPersistentCraftingStation(this);
		}
		World->GetTimerManager().ClearTimer(CraftingTimerHandle);
		World->GetTimerManager().ClearTimer(OutputRetryTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

TFunction<bool()> URpgCraftingStationComponent::MakeContextRevalidator(AActor* RequestingActor, bool bRefund, FGuid ExpectedJobId) const
{
	const TWeakObjectPtr<const URpgCraftingStationComponent> Station(this);
	const TWeakObjectPtr<AActor> Requester(RequestingActor);
	const bool bCheckAccess = RequestingActor != nullptr;
	const FTransform InitialTransform = GetOwner() ? GetOwner()->GetActorTransform() : FTransform::Identity;
	const float InitialRadius = StorageSearchRadius;
	const bool bInitialAutoDeposit = bAutoDepositCraftingOutputsEnabled;
	const bool bInitialPaused = bStationPaused;
	const int32 InitialStateRevision = CraftingStateRevision;
	const TWeakObjectPtr<URpgInventoryManagerComponent> InitialOutputInventory(OutputInventoryComponent);
	const TWeakObjectPtr<ARpgBaseCampActor> InitialBase(ResolveSpatialBaseCamp());
	const FVector InitialBaseCenter = InitialBase.IsValid() ? InitialBase->GetActorLocation() : FVector::ZeroVector;
	const float InitialBaseRadius = InitialBase.IsValid() ? InitialBase->GetBuildRadius() : 0.0f;
	TArray<URpgInventoryManagerComponent*> Domain;
	if (GetOwner()) { RpgStorageAccessRules::ResolveStorageSources(GetWorld(), GetOwner()->GetActorLocation(), StorageSearchRadius, Domain); }
	TArray<TPair<TWeakObjectPtr<URpgInventoryManagerComponent>, int32>> Inventories;
	TArray<TPair<TWeakObjectPtr<URpgInventoryContainerComponent>, int32>> Containers;
	for (URpgInventoryManagerComponent* Inventory : Domain)
	{
		Inventories.Emplace(Inventory, Inventory->GetInventoryRevision());
		if (URpgInventoryContainerComponent* Container = Inventory->GetOwner()->FindComponentByClass<URpgInventoryContainerComponent>())
		{
			Containers.Emplace(Container, Container->GetSettingsRevision());
		}
	}
	return [Station, Requester, bCheckAccess, bRefund, InitialTransform, InitialRadius, bInitialAutoDeposit, bInitialPaused,
		InitialStateRevision, InitialOutputInventory, ExpectedJobId, InitialBase, InitialBaseCenter, InitialBaseRadius, Inventories, Containers]()
	{
		if (!Station.IsValid() || Station->IsPersistenceRestorePending() || !IsValid(Station->GetOwner()) || Station->GetOwner()->IsActorBeingDestroyed() ||
			!Station->GetOwner()->HasAuthority() || !Station->GetOwner()->GetActorTransform().Equals(InitialTransform) ||
			Station->StorageSearchRadius != InitialRadius || Station->bAutoDepositCraftingOutputsEnabled != bInitialAutoDeposit ||
			Station->CraftingStateRevision != InitialStateRevision || Station->OutputInventoryComponent != InitialOutputInventory.Get() ||
			(ExpectedJobId.IsValid() && Station->FindJobIndex(ExpectedJobId) == INDEX_NONE) ||
			Station->bStationPaused != bInitialPaused || (bCheckAccess && (!Requester.IsValid() || !Station->CanActorAccess(Requester.Get())))) { return false; }
		if (bRefund) { return true; }
		ARpgBaseCampActor* CurrentBase = Station->ResolveSpatialBaseCamp();
		if (CurrentBase != InitialBase.Get() || (CurrentBase && (CurrentBase->GetActorLocation() != InitialBaseCenter || CurrentBase->GetBuildRadius() != InitialBaseRadius))) { return false; }
		TArray<URpgInventoryManagerComponent*> CurrentDomain;
		RpgStorageAccessRules::ResolveStorageSources(Station->GetWorld(), Station->GetOwner()->GetActorLocation(), InitialRadius, CurrentDomain);
		if (CurrentDomain.Num() != Inventories.Num()) { return false; }
		for (const auto& Pair : Inventories)
		{
			if (!Pair.Key.IsValid() || !CurrentDomain.Contains(Pair.Key.Get()) || Pair.Key->GetInventoryRevision() != Pair.Value) { return false; }
		}
		for (const auto& Pair : Containers)
		{
			if (!Pair.Key.IsValid() || Pair.Key->GetSettingsRevision() != Pair.Value) { return false; }
		}
		return true;
	};
}

bool URpgCraftingStationComponent::BuildOutputPlan(const TArray<FRpgCraftingOutputItem>& OutputItems,
	TArray<FRpgInventoryBatchOperation>& InOutOperations) const
{
	if (!OutputInventoryComponent || !GetOwner() || !CanAcceptCraftingOutputs(OutputItems)) { return false; }
	for (const FRpgCraftingOutputItem& Output : OutputItems)
	{
		int32 Remaining = Output.Count;
		TArray<URpgInventoryManagerComponent*> Targets;
		if (ShouldAutoDepositCraftingOutputs())
		{
			RpgStorageAccessRules::ResolveDepositTargets(GetWorld(), GetOwner()->GetActorLocation(), StorageSearchRadius, Output.ItemDefinition, Targets);
		}
		Targets.AddUnique(OutputInventoryComponent);
		for (URpgInventoryManagerComponent* Target : Targets)
		{
			if (!Target || Remaining <= 0) { continue; }
			FRpgInventoryBatchOperation Operation;
			Operation.TargetInventory = Target;
			Operation.ItemDefinition = Output.ItemDefinition;
			Operation.ExpectedTargetRevision = Target->GetInventoryRevision();
			int32 Low = 0, High = Remaining;
			while (Low < High)
			{
				Operation.Quantity = Low + (High - Low + 1) / 2;
				InOutOperations.Add(Operation);
				ERpgInventoryMutationResultCode Code;
				const bool bFits = OutputInventoryComponent->CanApplyInventoryBatch(InOutOperations, Code);
				InOutOperations.Pop();
				if (bFits) { Low = Operation.Quantity; } else { High = Operation.Quantity - 1; }
			}
			if (Low > 0) { Operation.Quantity = Low; InOutOperations.Add(Operation); Remaining -= Low; }
		}
		if (Remaining > 0) { return false; }
	}
	return true;
}

void URpgCraftingStationComponent::RetryBlockedOutput()
{
	if (bPersistenceRestorePending) return;
	if (bMutationInProgress)
	{
		if (UWorld* World = GetWorld()) { World->GetTimerManager().SetTimer(OutputRetryTimerHandle, this, &ThisClass::RetryBlockedOutput, 0.5f, false); }
		return;
	}
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	const int32 Index = FindActiveJobIndex();
	if (!CraftingJobs.IsValidIndex(Index) || CraftingJobs[Index].State != ERpgCraftingJobState::BlockedOutput) { return; }
	if (bStationPaused)
	{
		GetWorld()->GetTimerManager().SetTimer(OutputRetryTimerHandle, this, &ThisClass::RetryBlockedOutput, 0.5f, false);
		return;
	}
	CraftingJobs[Index].State = ERpgCraftingJobState::Active;
	CompleteActiveJobUnitInternal();
}

FName URpgCraftingStationComponent::GetPersistentStationId() const
{
	if (!PersistentStationId.IsNone()) { return PersistentStationId; }
	if (!GetOwner()) { return NAME_None; }
	if (const URpgInventoryContainerComponent* Container = GetOwner()->FindComponentByClass<URpgInventoryContainerComponent>())
	{
		if (!Container->GetPersistentContainerId().IsNone()) { return Container->GetPersistentContainerId(); }
	}
	return GetOwner()->GetFName();
}

FRpgCraftingStationSaveData URpgCraftingStationComponent::ExportCraftingState() const
{
	FRpgCraftingStationSaveData Save;
	Save.StationId = GetPersistentStationId();
	Save.bPaused = bStationPaused;
	Save.bAutoDepositOutputs = bAutoDepositCraftingOutputsEnabled;
	if (OutputInventoryComponent)
	{
		Save.OutputGridSize = OutputInventoryComponent->GetDefaultGridSize();
		Save.OutputInventoryGraph = OutputInventoryComponent->ExportInventoryGraph();
	}
	for (const FRpgCraftingJobEntry& Job : CraftingJobs)
	{
		FRpgCraftingJobSaveData& SavedJob = Save.Jobs.AddDefaulted_GetRef();
		SavedJob.JobId = Job.JobId;
		SavedJob.Recipe = Job.Recipe;
		SavedJob.QuantityTotal = Job.QuantityTotal;
		SavedJob.QuantityCompleted = Job.QuantityCompleted;
		SavedJob.State = static_cast<uint8>(Job.State);
		SavedJob.RemainingTime = Job.State == ERpgCraftingJobState::Active
			? FMath::Max(0.0f, Job.FinishServerTime - GetServerWorldTimeSeconds()) : Job.PausedRemainingTime;
		for (const FRpgCraftingRefundEntry& Refund : Job.RefundEntries)
		{
			FRpgCraftingRefundSaveData& SavedRefund = SavedJob.Refunds.AddDefaulted_GetRef();
			SavedRefund.ItemDefinition = Refund.ItemDefinition;
			SavedRefund.Count = Refund.Count;
			SavedRefund.InventoryId = Refund.InventoryId;
		}
	}
	return Save;
}

bool URpgCraftingStationComponent::RestoreCraftingState(const FRpgCraftingStationSaveData& Save)
{
	if (bMutationInProgress) { return false; }
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	if (!GetOwner() || !GetOwner()->HasAuthority() || !OutputInventoryComponent || Save.StationId != GetPersistentStationId() ||
		!Save.OutputGridSize.IsValid()) { return false; }
	TArray<FRpgCraftingJobEntry> Restored;
	TSet<FGuid> JobIds;
	int32 RunningCount = 0;
	for (const FRpgCraftingJobSaveData& SavedJob : Save.Jobs)
	{
		if (!SavedJob.JobId.IsValid() || JobIds.Contains(SavedJob.JobId) || SavedJob.QuantityTotal <= 0 || SavedJob.QuantityCompleted < 0 ||
			SavedJob.QuantityCompleted >= SavedJob.QuantityTotal || SavedJob.State > static_cast<uint8>(ERpgCraftingJobState::BlockedOutput) ||
			!FMath::IsFinite(SavedJob.RemainingTime) || SavedJob.RemainingTime < 0.0f) { return false; }
		URpgCraftingRecipeDefinition* Recipe = SavedJob.Recipe.LoadSynchronous();
		if (!Recipe || !CanAcceptCraftingOutputs(Recipe->OutputItems)) { return false; }
		JobIds.Add(SavedJob.JobId);
		FRpgCraftingJobEntry& Job = Restored.AddDefaulted_GetRef();
		Job.JobId = SavedJob.JobId;
		Job.Recipe = Recipe;
		Job.QuantityTotal = SavedJob.QuantityTotal;
		Job.QuantityCompleted = SavedJob.QuantityCompleted;
		Job.State = static_cast<ERpgCraftingJobState>(SavedJob.State);
		Job.PausedRemainingTime = SavedJob.RemainingTime;
		if (Job.State != ERpgCraftingJobState::Queued && ++RunningCount > 1) { return false; }
		if (Job.State == ERpgCraftingJobState::Active) { Job.State = ERpgCraftingJobState::Paused; }
		for (const FRpgCraftingRefundSaveData& SavedRefund : SavedJob.Refunds)
		{
			TSubclassOf<URpgInventoryItemDefinition> Definition = SavedRefund.ItemDefinition.LoadSynchronous();
			if (!Definition || SavedRefund.Count <= 0 || !URpgInventoryFragment_StorageProfile::IsDefinitionIntrinsicallyCollapsible(Definition)) { return false; }
			FRpgCraftingRefundEntry& Refund = Job.RefundEntries.AddDefaulted_GetRef();
			Refund.ItemDefinition = Definition;
			Refund.Count = SavedRefund.Count;
			Refund.InventoryId = SavedRefund.InventoryId;
			Refund.Inventory = RpgStorageAccessRules::FindPersistentInventory(GetWorld(), Refund.InventoryId);
		}
		// Credits must cover precisely the unfinished units. Corrupt snapshots cannot mint refunds.
		TArray<FRpgCraftingResourceCost> Costs;
		if (!TryBuildAggregatedResourceCosts(Recipe->RequiredResources, Costs)) { return false; }
		TMap<UClass*, int64> Credits;
		for (const FRpgCraftingRefundEntry& Refund : Job.RefundEntries) { Credits.FindOrAdd(Refund.ItemDefinition.Get()) += Refund.Count; }
		for (const FRpgCraftingResourceCost& Cost : Costs)
		{
			const int64 Required = static_cast<int64>(Cost.Count) * (Job.QuantityTotal - Job.QuantityCompleted);
			const int64* Credit = Credits.Find(Cost.ItemDefinition.Get());
			if (!Credit || *Credit != Required) { return false; }
			Credits.Remove(Cost.ItemDefinition.Get());
		}
		if (!Credits.IsEmpty()) { return false; }
	}
	FRpgInventoryMutationResult RestoreResult;
	// Validate the saved root bounds before resizing or publishing any part of the live tray.
	if (!OutputInventoryComponent->ValidateInventoryGraphForRestore(Save.OutputInventoryGraph, RestoreResult, &Save.OutputGridSize)) { return false; }
	const FRpgInventoryGridSize PreviousGrid = OutputInventoryComponent->GetDefaultGridSize();
	if (!OutputInventoryComponent->ExpandDefaultGridToMinimum(Save.OutputGridSize)) { return false; }
	if (!OutputInventoryComponent->RestoreInventoryGraph(Save.OutputInventoryGraph, RestoreResult))
	{
		OutputInventoryComponent->SetDefaultGridSize(PreviousGrid);
		return false;
	}
	if (!OutputInventoryComponent->SetDefaultGridSize(Save.OutputGridSize)) { return false; }
	GetWorld()->GetTimerManager().ClearTimer(CraftingTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(OutputRetryTimerHandle);
	CraftingJobs = MoveTemp(Restored);
	bStationPaused = Save.bPaused;
	bAutoDepositCraftingOutputsEnabled = Save.bAutoDepositOutputs;
	MarkCraftingStateDirty();
	return true;
}

void URpgCraftingStationComponent::ResumeRestoredCrafting()
{
	if (bPersistenceRestorePending || bMutationInProgress) { return; }
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
	const int32 Index = FindActiveJobIndex();
	if (CraftingJobs.IsValidIndex(Index))
	{
		if (CraftingJobs[Index].State == ERpgCraftingJobState::BlockedOutput)
		{
			GetWorld()->GetTimerManager().SetTimer(OutputRetryTimerHandle, this, &ThisClass::RetryBlockedOutput, 0.5f, false);
		}
		else if (!bStationPaused) { StartJobAtIndex(Index, CraftingJobs[Index].PausedRemainingTime); }
	}
	else if (!bStationPaused) { TryStartNextQueuedJob(); }
}

void URpgCraftingStationComponent::MarkCraftingStateDirty(FGuid ChangedJobId, ERpgCraftingJobState ChangedState, bool bPauseStateChanged)
{
	if (GetOwner() && GetOwner()->HasAuthority()) { ++CraftingStateRevision; }
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || !IsRegistered() || HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return;
	}

	if (AActor* OwnerActor = GetOwner())
	{
		if (OwnerActor->HasAuthority())
		{
			OwnerActor->ForceNetUpdate();
			if (ARpgGameModeBase* GameMode = World->GetAuthGameMode<ARpgGameModeBase>()) { GameMode->MarkCraftingSaveDirty(this); }
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
