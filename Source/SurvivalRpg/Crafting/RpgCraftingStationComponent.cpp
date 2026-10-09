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
#include "SurvivalRpg/Base/RpgStorageAccessRules.h"
#include "SurvivalRpg/Base/RpgWorldStorageKnowledgeComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameStateBase.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Crafting/RpgCraftingRecipeDefinition.h"
#include "SurvivalRpg/Crafting/RpgRecipeUnlockComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"
#include "SurvivalRpg/Inventory/Itemization/RpgInventoryFragment_Itemization.h"
#include "SurvivalRpg/Inventory/Itemization/RpgItemizationProfile.h"
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
}

void URpgCraftingStationComponent::SetPersistenceRestorePending(bool bPending)
{
	if (GetOwner() && GetOwner()->HasAuthority()) bPersistenceRestorePending = bPending;
}

void URpgCraftingStationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, LinkedBaseCamp);
	DOREPLIFETIME(ThisClass, CurrentOrder);
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
		URpgInventoryManagerComponent* Inventory)
	{
		if (!ItemDefinition || Count <= 0)
		{
			return;
		}

		for (FRpgCraftingRefundEntry& RefundEntry : RefundEntries)
		{
			if (RefundEntry.ItemDefinition == ItemDefinition && RefundEntry.Inventory == Inventory)
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
	}

	/** Appends exact debits of Costs x Quantity from Sources in order; resets both outputs when they cannot cover it. */
	bool AppendConsumptionOperations(
		const TArray<URpgInventoryManagerComponent*>& Sources,
		const URpgInventoryManagerComponent* PlayerInventory,
		const TArray<FRpgCraftingResourceCost>& Costs,
		int32 Quantity,
		TArray<FRpgInventoryBatchOperation>& OutOperations,
		TArray<FRpgCraftingRefundEntry>& OutRefundEntries)
	{
		for (const FRpgCraftingResourceCost& Cost : Costs)
		{
			const int64 Required = static_cast<int64>(Cost.Count) * Quantity;
			if (Required > MAX_int32) { OutOperations.Reset(); OutRefundEntries.Reset(); return false; }
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
					AddRefundCredit(OutRefundEntries, Cost.ItemDefinition, Debit, Inventory);
					Remaining -= Debit;
				}
			}
			if (Remaining > 0) { OutOperations.Reset(); OutRefundEntries.Reset(); return false; }
		}
		return true;
	}

	bool IsPlayerOwnedInventory(const URpgInventoryManagerComponent* Inventory)
	{
		const AActor* SourceOwner = Inventory ? Inventory->GetOwner() : nullptr;
		return !SourceOwner || SourceOwner->IsA<APlayerState>() || SourceOwner->IsA<APawn>() || SourceOwner->IsA<AController>();
	}

	/** True when the definition rolls its own stats per crafted piece. */
	bool IsItemizedOutput(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		const URpgInventoryItemDefinition* CDO = ItemDefinition ? GetDefault<URpgInventoryItemDefinition>(ItemDefinition) : nullptr;
		const URpgInventoryFragment_Itemization* Fragment = CDO
			? Cast<URpgInventoryFragment_Itemization>(CDO->FindFragmentByClass(URpgInventoryFragment_Itemization::StaticClass()))
			: nullptr;
		return Fragment && Fragment->ItemizationProfile && Fragment->ItemizationProfile->HasValidConfiguration();
	}

	bool HasValidOutputs(const URpgCraftingRecipeDefinition* Recipe)
	{
		if (!Recipe || Recipe->OutputItems.IsEmpty()) { return false; }
		for (const FRpgCraftingOutputItem& Output : Recipe->OutputItems)
		{
			if (!Output.ItemDefinition || Output.Count <= 0) { return false; }
		}
		return true;
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

FName URpgCraftingStationComponent::GetStorageContainerId(const URpgInventoryManagerComponent* Inventory)
{
	const AActor* Owner = Inventory ? Inventory->GetOwner() : nullptr;
	const URpgInventoryContainerComponent* Container = Owner ? Owner->FindComponentByClass<URpgInventoryContainerComponent>() : nullptr;
	return Container && Container->GetInventoryManager() == Inventory ? Container->GetPersistentContainerId() : NAME_None;
}

TArray<URpgInventoryManagerComponent*> URpgCraftingStationComponent::GetConnectedStorageInventories() const
{
	TArray<URpgInventoryManagerComponent*> Results;
	if (!GetOwner()) { return Results; }
	RpgStorageAccessRules::ResolveStorageSources(GetWorld(), GetOwner()->GetActorLocation(), StorageSearchRadius, Results);
	// Credits and targets are saved by persistent id, so chests without one cannot take part in an order.
	Results.RemoveAll([](const URpgInventoryManagerComponent* Inventory) { return GetStorageContainerId(Inventory).IsNone(); });
	return Results;
}

URpgInventoryManagerComponent* URpgCraftingStationComponent::FindConnectedStorageInventory(FName ContainerId) const
{
	if (ContainerId.IsNone()) { return nullptr; }
	for (URpgInventoryManagerComponent* Inventory : GetConnectedStorageInventories())
	{
		if (GetStorageContainerId(Inventory) == ContainerId) { return Inventory; }
	}
	return nullptr;
}

TArray<URpgInventoryManagerComponent*> URpgCraftingStationComponent::GetOutputTargets(const URpgCraftingRecipeDefinition* RecipeDefinition,
	FName TargetContainerId) const
{
	TArray<URpgInventoryManagerComponent*> Results;
	if (!HasValidOutputs(RecipeDefinition)) { return Results; }
	if (!TargetContainerId.IsNone())
	{
		if (URpgInventoryManagerComponent* Target = FindConnectedStorageInventory(TargetContainerId)) { Results.Add(Target); }
		return Results;
	}
	return RankAutomaticOutputTargets(GetConnectedStorageInventories(), RecipeDefinition->OutputItems[0].ItemDefinition);
}

TArray<URpgInventoryManagerComponent*> URpgCraftingStationComponent::RankAutomaticOutputTargets(
	const TArray<URpgInventoryManagerComponent*>& StorageInventories, TSubclassOf<URpgInventoryItemDefinition> OutputDefinition)
{
	struct FRankedTarget
	{
		URpgInventoryManagerComponent* Inventory = nullptr;
		int32 Rank = 0;
		int64 Order = 0;
		int32 Present = 0;
		FName Id;
	};
	TArray<FRankedTarget> Ranked;
	for (URpgInventoryManagerComponent* Inventory : StorageInventories)
	{
		const AActor* Owner = Inventory ? Inventory->GetOwner() : nullptr;
		const URpgInventoryContainerComponent* Container = Owner ? Owner->FindComponentByClass<URpgInventoryContainerComponent>() : nullptr;
		if (!Container || Container->GetInventoryManager() != Inventory || !OutputDefinition) { continue; }
		FRankedTarget Entry;
		Entry.Inventory = Inventory;
		Entry.Id = Container->GetPersistentContainerId();
		Entry.Present = Inventory->GetTotalItemCountByDefinition(OutputDefinition);
		// 0 exact assignment, 1 category assignment, 2 unassigned and already holding the output, 3 unassigned.
		// An assigned chest only takes outputs its assignments name, even when it already holds some of them.
		if (!Container->GetAssignments().IsEmpty())
		{
			Entry.Rank = Container->GetAssignmentRank(OutputDefinition, Entry.Order);
			if (Entry.Rank != 0 && Entry.Rank != 1) { continue; }
		}
		else
		{
			Entry.Rank = Entry.Present > 0 ? 2 : 3;
		}
		Ranked.Add(Entry);
	}
	Ranked.Sort([](const FRankedTarget& A, const FRankedTarget& B)
	{
		if (A.Rank != B.Rank) { return A.Rank < B.Rank; }
		if (A.Present != B.Present) { return A.Present > B.Present; }
		if (A.Rank < 2 && A.Order != B.Order) { return A.Order < B.Order; }
		return A.Id.LexicalLess(B.Id);
	});
	TArray<URpgInventoryManagerComponent*> Results;
	for (const FRankedTarget& Entry : Ranked) { Results.Add(Entry.Inventory); }
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
		if (IsRecipeOfferedByStation(Recipe))
		{
			Results.Add(Recipe);
		}
	}

	return Results;
}

int32 URpgCraftingStationComponent::GetAvailableResourceCount(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	return CountStorageResource(GetConnectedStorageInventories(), ItemDefinition);
}

int32 URpgCraftingStationComponent::GetAffordableUnitCount(const URpgCraftingRecipeDefinition* RecipeDefinition) const
{
	return CountAffordableUnits(GetConnectedStorageInventories(), RecipeDefinition, GetMaxOrderQuantity());
}

int32 URpgCraftingStationComponent::CountStorageResource(const TArray<URpgInventoryManagerComponent*>& StorageInventories,
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
{
	const URpgInventoryFragment_StorageProfile* Profile = URpgInventoryFragment_StorageProfile::ResolveStorageProfile(ItemDefinition);
	if (!ItemDefinition || !Profile || !Profile->CanCraftFromPhysicalStorage()) { return 0; }
	int64 Count = 0;
	for (const URpgInventoryManagerComponent* Inventory : StorageInventories)
	{
		if (!Inventory) { continue; }
		for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
		{
			if (Entry.Instance && Entry.Instance->GetItemDef() == ItemDefinition && Entry.StackCount > 0 &&
				Entry.Instance->CanCollapseIntoDefinitionCount())
			{
				Count += Entry.StackCount;
			}
		}
	}
	return static_cast<int32>(FMath::Min<int64>(Count, MAX_int32));
}

int32 URpgCraftingStationComponent::CountAffordableUnits(const TArray<URpgInventoryManagerComponent*>& StorageInventories,
	const URpgCraftingRecipeDefinition* RecipeDefinition, int32 MaxUnitsWithoutCosts)
{
	TArray<FRpgCraftingResourceCost> Costs;
	if (!RecipeDefinition || !TryBuildAggregatedResourceCosts(RecipeDefinition->RequiredResources, Costs)) { return 0; }
	if (Costs.IsEmpty()) { return FMath::Max(1, MaxUnitsWithoutCosts); }
	int32 Units = MAX_int32;
	for (const FRpgCraftingResourceCost& Cost : Costs)
	{
		Units = FMath::Min(Units, CountStorageResource(StorageInventories, Cost.ItemDefinition) / Cost.Count);
	}
	return FMath::Max(0, Units);
}

bool URpgCraftingStationComponent::CanStartCraftingOrder(AActor* RequestingActor, const URpgCraftingRecipeDefinition* RecipeDefinition,
	int32 Quantity, FName TargetContainerId) const
{
	return RecipeDefinition &&
		!CurrentOrder.IsActive() &&
		Quantity >= 1 && Quantity <= GetMaxOrderQuantity() &&
		HasValidOutputs(RecipeDefinition) &&
		CanActorAccess(RequestingActor) &&
		IsRecipeOfferedByStation(RecipeDefinition) &&
		IsRecipeUnlocked(RecipeDefinition) &&
		!GetOutputTargets(RecipeDefinition, TargetContainerId).IsEmpty() &&
		GetAffordableUnitCount(RecipeDefinition) >= 1;
}

bool URpgCraftingStationComponent::BuildUnitOutputOperations(const URpgCraftingRecipeDefinition* RecipeDefinition,
	URpgInventoryManagerComponent* Target, TArray<FRpgInventoryBatchOperation>& InOutOperations) const
{
	if (!Target || !HasValidOutputs(RecipeDefinition)) { return false; }
	for (const FRpgCraftingOutputItem& Output : RecipeDefinition->OutputItems)
	{
		FRpgInventoryBatchOperation& Operation = InOutOperations.AddDefaulted_GetRef();
		Operation.TargetInventory = Target;
		Operation.ItemDefinition = Output.ItemDefinition;
		Operation.Quantity = Output.Count;
		Operation.ExpectedTargetRevision = Target->GetInventoryRevision();
		if (IsItemizedOutput(Output.ItemDefinition))
		{
			// Every attempt rolls fresh; nothing is committed before the whole unit fits.
			Operation.ItemizationSourceLevel = FMath::Max(1, RecipeDefinition->OutputItemLevel);
			Operation.ItemizationSeed = FMath::Rand();
		}
	}
	return true;
}

bool URpgCraftingStationComponent::PlanUnitStart(const URpgCraftingRecipeDefinition* RecipeDefinition,
	const TArray<URpgInventoryManagerComponent*>& Targets, TArray<FRpgInventoryBatchOperation>& OutConsumption,
	TArray<FRpgCraftingRefundEntry>& OutCredits, ERpgCraftingOrderState& OutBlockedState) const
{
	OutConsumption.Reset();
	OutCredits.Reset();
	if (Targets.IsEmpty())
	{
		OutBlockedState = ERpgCraftingOrderState::WaitingForTarget;
		return false;
	}
	if (!BuildStorageConsumptionPlan(GetConnectedStorageInventories(), RecipeDefinition->RequiredResources, 1, OutConsumption, OutCredits))
	{
		OutBlockedState = ERpgCraftingOrderState::WaitingForMaterials;
		return false;
	}
	// Dry-run the unit's consumption together with its outputs, so room freed in a target counts and no material is
	// consumed while no target can take the result.
	for (URpgInventoryManagerComponent* Target : Targets)
	{
		TArray<FRpgInventoryBatchOperation> Combined = OutConsumption;
		ERpgInventoryMutationResultCode Code;
		const bool bBuilt = BuildUnitOutputOperations(RecipeDefinition, Target, Combined);
		URpgInventoryManagerComponent* Coordinator = bBuilt
			? (Combined[0].SourceInventory ? Combined[0].SourceInventory : Combined[0].TargetInventory)
			: nullptr;
		if (Coordinator && Coordinator->CanApplyInventoryBatch(Combined, Code))
		{
			return true;
		}
	}
	OutConsumption.Reset();
	OutCredits.Reset();
	OutBlockedState = ERpgCraftingOrderState::WaitingForSpace;
	return false;
}

bool URpgCraftingStationComponent::ApplyStationBatch(const TArray<FRpgInventoryBatchOperation>& Operations,
	TFunction<void()> CommitSideEffects, TFunction<bool()> Revalidate)
{
	if (Operations.IsEmpty())
	{
		if (Revalidate && !Revalidate()) { return false; }
		if (CommitSideEffects) { CommitSideEffects(); }
		return true;
	}
	URpgInventoryManagerComponent* Coordinator = Operations[0].SourceInventory ? Operations[0].SourceInventory : Operations[0].TargetInventory;
	return Coordinator && Coordinator->ApplyInventoryBatch(Operations, FGuid::NewGuid(), {}, MoveTemp(CommitSideEffects), MoveTemp(Revalidate)).IsSuccess();
}

bool URpgCraftingStationComponent::StartCraftingOrder(AActor* RequestingActor, URpgCraftingRecipeDefinition* RecipeDefinition,
	int32 Quantity, FName TargetContainerId)
{
	if (bMutationInProgress) { return false; }
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	if (!GetOwner() || !GetOwner()->HasAuthority() || !CanStartCraftingOrder(RequestingActor, RecipeDefinition, Quantity, TargetContainerId)) { return false; }
	TArray<FRpgInventoryBatchOperation> Consumption;
	TArray<FRpgCraftingRefundEntry> Credits;
	ERpgCraftingOrderState BlockedState;
	if (!PlanUnitStart(RecipeDefinition, GetOutputTargets(RecipeDefinition, TargetContainerId), Consumption, Credits, BlockedState)) { return false; }
	const TFunction<bool()> Revalidate = MakeContextRevalidator(RequestingActor);
	const float Duration = GetRecipeCraftTime(RecipeDefinition);
	// The order and its credits exist before inventory observers may save the committed debit.
	auto CommitOrder = [this, RecipeDefinition, Quantity, TargetContainerId, Duration, &Credits]()
	{
		CurrentOrder = FRpgCraftingOrder();
		CurrentOrder.OrderId = FGuid::NewGuid();
		CurrentOrder.Recipe = RecipeDefinition;
		CurrentOrder.TargetContainerId = TargetContainerId;
		CurrentOrder.QuantityTotal = Quantity;
		CurrentOrder.State = ERpgCraftingOrderState::Running;
		CurrentOrder.bUnitPaid = true;
		CurrentOrder.PausedRemainingTime = Duration;
		CurrentOrder.UnitCredits = MoveTemp(Credits);
		MarkCraftingStateDirty();
	};
	if (!ApplyStationBatch(Consumption, CommitOrder, Revalidate)) { return false; }
	StartUnitTimer(Duration);
	return true;
}

bool URpgCraftingStationComponent::StopCraftingOrder(AActor* RequestingActor, FGuid OrderId)
{
	if (bMutationInProgress) { return false; }
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	if (!GetOwner() || !GetOwner()->HasAuthority() || !CurrentOrder.IsActive() || CurrentOrder.OrderId != OrderId ||
		!CanActorAccess(RequestingActor)) { return false; }
	const auto EndOrder = [this, OrderId]()
	{
		ClearOrderTimers();
		CurrentOrder = FRpgCraftingOrder();
		MarkCraftingStateDirty(false, OrderId);
	};
	if (!CurrentOrder.bUnitPaid)
	{
		EndOrder();
		return true;
	}
	if (RefundUnitCredits(EndOrder)) { return true; }
	// The paid unit's materials fit in no connected chest: finish that unit instead of losing it, then end.
	CurrentOrder.QuantityTotal = CurrentOrder.QuantityCompleted + 1;
	MarkCraftingStateDirty();
	return true;
}

bool URpgCraftingStationComponent::SetCraftingOrderTarget(AActor* RequestingActor, FGuid OrderId, FName TargetContainerId)
{
	if (bMutationInProgress) { return false; }
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	if (!GetOwner() || !GetOwner()->HasAuthority() || !CurrentOrder.IsActive() || CurrentOrder.OrderId != OrderId ||
		!CanActorAccess(RequestingActor) || (!TargetContainerId.IsNone() && !FindConnectedStorageInventory(TargetContainerId))) { return false; }
	if (CurrentOrder.TargetContainerId == TargetContainerId) { return true; }
	CurrentOrder.TargetContainerId = TargetContainerId;
	MarkCraftingStateDirty();
	if (!CurrentOrder.bPaused && CurrentOrder.State != ERpgCraftingOrderState::Running)
	{
		ContinueOrderInternal();
	}
	return true;
}

bool URpgCraftingStationComponent::PauseCraftingStation(AActor* RequestingActor)
{
	if (bMutationInProgress) { return false; }
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	if (!GetOwner() || !GetOwner()->HasAuthority() || !CurrentOrder.IsActive() || CurrentOrder.bPaused || !CanActorAccess(RequestingActor))
	{
		return false;
	}

	CurrentOrder.bPaused = true;
	if (CurrentOrder.bUnitPaid && CurrentOrder.State == ERpgCraftingOrderState::Running)
	{
		CurrentOrder.PausedRemainingTime = FMath::Max(0.0f, CurrentOrder.UnitFinishServerTime - GetServerWorldTimeSeconds());
	}
	ClearOrderTimers();
	MarkCraftingStateDirty(true);
	return true;
}

bool URpgCraftingStationComponent::ResumeCraftingStation(AActor* RequestingActor)
{
	if (bMutationInProgress) { return false; }
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	if (!GetOwner() || !GetOwner()->HasAuthority() || !CurrentOrder.IsActive() || !CurrentOrder.bPaused || !CanActorAccess(RequestingActor))
	{
		return false;
	}

	CurrentOrder.bPaused = false;
	MarkCraftingStateDirty(true);
	ContinueOrderInternal();
	return true;
}

void URpgCraftingStationComponent::ContinueOrderInternal()
{
	if (bPersistenceRestorePending || !CurrentOrder.IsActive() || CurrentOrder.bPaused) { return; }
	if (!CurrentOrder.bUnitPaid)
	{
		TryStartNextUnit();
	}
	else if (CurrentOrder.State == ERpgCraftingOrderState::Running)
	{
		StartUnitTimer(CurrentOrder.PausedRemainingTime);
	}
	else
	{
		// A paid unit waits for room or its target; delivering is the next step.
		CompleteActiveUnitInternal();
	}
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
		// Defense in depth: an accidental broad caller list cannot consume another player's inventory.
		if (!Inventory || Inventory == PlayerInventory || IsPlayerOwnedInventory(Inventory)) { continue; }
		Sources.AddUnique(Inventory);
	}
	return AppendConsumptionOperations(Sources, PlayerInventory, Costs, Quantity, OutOperations, OutRefundEntries);
}

bool URpgCraftingStationComponent::BuildStorageConsumptionPlan(
	const TArray<URpgInventoryManagerComponent*>& StorageInventories,
	const TArray<FRpgCraftingResourceCost>& RequiredItems, int32 Quantity,
	TArray<FRpgInventoryBatchOperation>& OutOperations, TArray<FRpgCraftingRefundEntry>& OutRefundEntries)
{
	OutOperations.Reset();
	OutRefundEntries.Reset();
	if (Quantity <= 0) { return false; }
	TArray<FRpgCraftingResourceCost> Costs;
	if (!TryBuildAggregatedResourceCosts(RequiredItems, Costs)) { return false; }
	TArray<URpgInventoryManagerComponent*> Sources;
	for (URpgInventoryManagerComponent* Inventory : StorageInventories)
	{
		// Credits are saved by persistent id; player inventories and unsaveable inventories are never sources.
		if (!Inventory || IsPlayerOwnedInventory(Inventory) || RpgStorageAccessRules::GetPersistentInventoryId(Inventory).IsNone()) { continue; }
		Sources.AddUnique(Inventory);
	}
	return AppendConsumptionOperations(Sources, nullptr, Costs, Quantity, OutOperations, OutRefundEntries);
}

bool URpgCraftingStationComponent::RefundUnitCredits(TFunction<void()> CommitSideEffects)
{
	const TFunction<bool()> Revalidate = MakeContextRevalidator(nullptr, CurrentOrder.OrderId);
	const TArray<URpgInventoryManagerComponent*> Connected = GetConnectedStorageInventories();
	URpgInventoryManagerComponent* Target = FindConnectedStorageInventory(CurrentOrder.TargetContainerId);
	TArray<FRpgInventoryBatchOperation> Operations;
	for (const FRpgCraftingRefundEntry& Refund : CurrentOrder.UnitCredits)
	{
		if (!Refund.ItemDefinition || Refund.Count <= 0) { return false; }
		URpgInventoryManagerComponent* Original = RpgStorageAccessRules::FindPersistentInventory(GetWorld(), Refund.InventoryId);
		if (!IsValid(Original) || !IsValid(Original->GetOwner()) || Original->GetOwner()->IsActorBeingDestroyed()) { Original = nullptr; }
		TArray<URpgInventoryManagerComponent*> Candidates;
		if (Original) { Candidates.Add(Original); }
		for (URpgInventoryManagerComponent* Inventory : Connected) { Candidates.AddUnique(Inventory); }
		if (Target) { Candidates.AddUnique(Target); }
		int32 Remaining = Refund.Count;
		for (URpgInventoryManagerComponent* Candidate : Candidates)
		{
			if (Remaining <= 0) { break; }
			FRpgInventoryBatchOperation Operation;
			Operation.TargetInventory = Candidate;
			Operation.ItemDefinition = Refund.ItemDefinition;
			Operation.ExpectedTargetRevision = Candidate->GetInventoryRevision();
			int32 Low = 0, High = Remaining;
			while (Low < High)
			{
				Operation.Quantity = Low + (High - Low + 1) / 2;
				Operations.Add(Operation);
				ERpgInventoryMutationResultCode Code;
				const bool bFits = Operations[0].TargetInventory->CanApplyInventoryBatch(Operations, Code);
				Operations.Pop();
				if (bFits) { Low = Operation.Quantity; } else { High = Operation.Quantity - 1; }
			}
			if (Low > 0) { Operation.Quantity = Low; Operations.Add(Operation); Remaining -= Low; }
		}
		if (Remaining > 0) { return false; }
	}
	return ApplyStationBatch(Operations, MoveTemp(CommitSideEffects), Revalidate);
}

void URpgCraftingStationComponent::TryStartNextUnit()
{
	if (bPersistenceRestorePending || !GetOwner() || !GetOwner()->HasAuthority() || !CurrentOrder.IsActive() ||
		CurrentOrder.bPaused || CurrentOrder.bUnitPaid) { return; }
	TArray<FRpgInventoryBatchOperation> Consumption;
	TArray<FRpgCraftingRefundEntry> Credits;
	ERpgCraftingOrderState BlockedState;
	if (!PlanUnitStart(CurrentOrder.Recipe, GetOutputTargets(CurrentOrder.Recipe, CurrentOrder.TargetContainerId), Consumption, Credits, BlockedState))
	{
		EnterWaitingState(BlockedState);
		return;
	}
	const FGuid OrderId = CurrentOrder.OrderId;
	const float Duration = GetRecipeCraftTime(CurrentOrder.Recipe);
	auto CommitUnit = [this, Duration, &Credits]()
	{
		CurrentOrder.State = ERpgCraftingOrderState::Running;
		CurrentOrder.bUnitPaid = true;
		CurrentOrder.PausedRemainingTime = Duration;
		CurrentOrder.UnitCredits = MoveTemp(Credits);
		MarkCraftingStateDirty();
	};
	if (!ApplyStationBatch(Consumption, CommitUnit, MakeContextRevalidator(nullptr, OrderId)))
	{
		// A concurrent change invalidated the plan; the next retry plans again.
		EnterWaitingState(ERpgCraftingOrderState::WaitingForMaterials);
		return;
	}
	StartUnitTimer(Duration);
}

void URpgCraftingStationComponent::StartUnitTimer(float RemainingDuration)
{
	UWorld* World = GetWorld();
	if (bPersistenceRestorePending || !World || !CurrentOrder.IsActive() || !CurrentOrder.bUnitPaid || CurrentOrder.bPaused) { return; }
	const float Now = GetServerWorldTimeSeconds();
	const float FullDuration = GetRecipeCraftTime(CurrentOrder.Recipe);
	const float Remaining = FMath::Clamp(RemainingDuration, 0.0f, FullDuration);
	CurrentOrder.State = ERpgCraftingOrderState::Running;
	CurrentOrder.UnitStartServerTime = Now - (FullDuration - Remaining);
	CurrentOrder.UnitFinishServerTime = Now + Remaining;
	CurrentOrder.PausedRemainingTime = 0.0f;
	World->GetTimerManager().ClearTimer(RetryTimerHandle);
	World->GetTimerManager().ClearTimer(CraftingTimerHandle);
	if (Remaining <= 0.0f)
	{
		CraftingTimerHandle = World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ThisClass::CompleteActiveUnit));
	}
	else
	{
		World->GetTimerManager().SetTimer(CraftingTimerHandle, this, &ThisClass::CompleteActiveUnit, Remaining, false);
	}
	MarkCraftingStateDirty();
}

void URpgCraftingStationComponent::CompleteActiveUnit()
{
	if (bPersistenceRestorePending) return;
	if (bMutationInProgress)
	{
		if (UWorld* World = GetWorld()) { World->GetTimerManager().SetTimer(CraftingTimerHandle, this, &ThisClass::CompleteActiveUnit, 0.01f, false); }
		return;
	}
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	CompleteActiveUnitInternal();
}

void URpgCraftingStationComponent::CompleteActiveUnitInternal()
{
	if (bPersistenceRestorePending || !GetOwner() || !GetOwner()->HasAuthority() || !CurrentOrder.IsActive() ||
		!CurrentOrder.bUnitPaid || CurrentOrder.bPaused) { return; }
	const TArray<URpgInventoryManagerComponent*> Targets = GetOutputTargets(CurrentOrder.Recipe, CurrentOrder.TargetContainerId);
	if (Targets.IsEmpty())
	{
		EnterWaitingState(ERpgCraftingOrderState::WaitingForTarget);
		return;
	}
	const FGuid OrderId = CurrentOrder.OrderId;
	// The context is captured before any dry run stages outputs, so a change during staging still rejects the commit.
	const TFunction<bool()> Revalidate = MakeContextRevalidator(nullptr, OrderId);
	// Deliver into the first target that takes the whole unit; automatic storing moves on when a chest is full.
	TArray<FRpgInventoryBatchOperation> Operations;
	for (URpgInventoryManagerComponent* Target : Targets)
	{
		TArray<FRpgInventoryBatchOperation> Candidate;
		ERpgInventoryMutationResultCode Code;
		if (BuildUnitOutputOperations(CurrentOrder.Recipe, Target, Candidate) && Target->CanApplyInventoryBatch(Candidate, Code))
		{
			Operations = MoveTemp(Candidate);
			break;
		}
	}
	const bool bCommitted = !Operations.IsEmpty() &&
		ApplyStationBatch(Operations, [this, OrderId]()
		{
			CurrentOrder.UnitCredits.Reset();
			CurrentOrder.bUnitPaid = false;
			++CurrentOrder.QuantityCompleted;
			if (CurrentOrder.QuantityCompleted >= CurrentOrder.QuantityTotal)
			{
				CurrentOrder = FRpgCraftingOrder();
				MarkCraftingStateDirty(false, OrderId);
			}
			else
			{
				// Snapshotting between units persists an unpaid unit, never an already-produced one.
				MarkCraftingStateDirty();
			}
		}, Revalidate);
	if (!bCommitted)
	{
		if (!IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed() || !GetWorld()) { return; }
		EnterWaitingState(ERpgCraftingOrderState::WaitingForSpace);
		return;
	}
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(CraftingTimerHandle); }
	TryStartNextUnit();
}

void URpgCraftingStationComponent::EnterWaitingState(ERpgCraftingOrderState WaitingState)
{
	UWorld* World = GetWorld();
	if (!World || !CurrentOrder.IsActive()) { return; }
	World->GetTimerManager().ClearTimer(CraftingTimerHandle);
	if (CurrentOrder.State != WaitingState)
	{
		CurrentOrder.State = WaitingState;
		MarkCraftingStateDirty();
	}
	if (!CurrentOrder.bPaused)
	{
		World->GetTimerManager().SetTimer(RetryTimerHandle, this, &ThisClass::RetryWaitingOrder, FMath::Max(0.1f, WaitingRetryInterval), false);
	}
}

void URpgCraftingStationComponent::RetryWaitingOrder()
{
	if (bPersistenceRestorePending) return;
	if (bMutationInProgress)
	{
		if (UWorld* World = GetWorld()) { World->GetTimerManager().SetTimer(RetryTimerHandle, this, &ThisClass::RetryWaitingOrder, FMath::Max(0.1f, WaitingRetryInterval), false); }
		return;
	}
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	ContinueOrderInternal();
}

void URpgCraftingStationComponent::ClearOrderTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CraftingTimerHandle);
		World->GetTimerManager().ClearTimer(RetryTimerHandle);
	}
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
		// Preserve the paid unit before this actor disappears from world enumeration.
		if (ARpgGameModeBase* GameMode = World->GetAuthGameMode<ARpgGameModeBase>())
		{
			GameMode->UnregisterPersistentCraftingStation(this);
		}
		World->GetTimerManager().ClearTimer(CraftingTimerHandle);
		World->GetTimerManager().ClearTimer(RetryTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

TFunction<bool()> URpgCraftingStationComponent::MakeContextRevalidator(AActor* RequestingActor, FGuid ExpectedOrderId) const
{
	const TWeakObjectPtr<const URpgCraftingStationComponent> Station(this);
	const TWeakObjectPtr<AActor> Requester(RequestingActor);
	const bool bCheckAccess = RequestingActor != nullptr;
	const FTransform InitialTransform = GetOwner() ? GetOwner()->GetActorTransform() : FTransform::Identity;
	const float InitialRadius = StorageSearchRadius;
	const int32 InitialStateRevision = CraftingStateRevision;
	const FName InitialTargetId = CurrentOrder.TargetContainerId;
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
	return [Station, Requester, bCheckAccess, InitialTransform, InitialRadius, InitialStateRevision, InitialTargetId,
		ExpectedOrderId, InitialBase, InitialBaseCenter, InitialBaseRadius, Inventories, Containers]()
	{
		if (!Station.IsValid() || Station->IsPersistenceRestorePending() || !IsValid(Station->GetOwner()) || Station->GetOwner()->IsActorBeingDestroyed() ||
			!Station->GetOwner()->HasAuthority() || !Station->GetOwner()->GetActorTransform().Equals(InitialTransform) ||
			Station->StorageSearchRadius != InitialRadius || Station->CraftingStateRevision != InitialStateRevision ||
			Station->CurrentOrder.OrderId != ExpectedOrderId || Station->CurrentOrder.TargetContainerId != InitialTargetId ||
			(bCheckAccess && (!Requester.IsValid() || !Station->CanActorAccess(Requester.Get())))) { return false; }
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
	Save.bHasOrder = CurrentOrder.IsActive();
	if (!Save.bHasOrder) { return Save; }
	FRpgCraftingOrderSaveData& Order = Save.Order;
	Order.OrderId = CurrentOrder.OrderId;
	Order.Recipe = CurrentOrder.Recipe;
	Order.TargetContainerId = CurrentOrder.TargetContainerId;
	Order.QuantityTotal = CurrentOrder.QuantityTotal;
	Order.QuantityCompleted = CurrentOrder.QuantityCompleted;
	Order.State = static_cast<uint8>(CurrentOrder.State);
	Order.bPaused = CurrentOrder.bPaused;
	Order.bUnitPaid = CurrentOrder.bUnitPaid;
	const bool bTimerRunning = CurrentOrder.bUnitPaid && !CurrentOrder.bPaused && CurrentOrder.State == ERpgCraftingOrderState::Running &&
		GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(CraftingTimerHandle);
	Order.RemainingTime = bTimerRunning
		? FMath::Max(0.0f, CurrentOrder.UnitFinishServerTime - GetServerWorldTimeSeconds())
		: CurrentOrder.PausedRemainingTime;
	for (const FRpgCraftingRefundEntry& Refund : CurrentOrder.UnitCredits)
	{
		FRpgCraftingRefundSaveData& SavedRefund = Order.UnitCredits.AddDefaulted_GetRef();
		SavedRefund.ItemDefinition = Refund.ItemDefinition;
		SavedRefund.Count = Refund.Count;
		SavedRefund.InventoryId = Refund.InventoryId;
	}
	return Save;
}

bool URpgCraftingStationComponent::RestoreCraftingState(const FRpgCraftingStationSaveData& Save)
{
	if (bMutationInProgress) { return false; }
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	if (!GetOwner() || !GetOwner()->HasAuthority() || Save.StationId != GetPersistentStationId()) { return false; }
	FRpgCraftingOrder Restored;
	if (Save.bHasOrder)
	{
		const FRpgCraftingOrderSaveData& Saved = Save.Order;
		if (!Saved.OrderId.IsValid() || Saved.QuantityTotal <= 0 || Saved.QuantityCompleted < 0 ||
			Saved.QuantityCompleted >= Saved.QuantityTotal || Saved.State > static_cast<uint8>(ERpgCraftingOrderState::WaitingForTarget) ||
			!FMath::IsFinite(Saved.RemainingTime) || Saved.RemainingTime < 0.0f) { return false; }
		URpgCraftingRecipeDefinition* Recipe = Saved.Recipe.LoadSynchronous();
		if (!HasValidOutputs(Recipe)) { return false; }
		Restored.OrderId = Saved.OrderId;
		Restored.Recipe = Recipe;
		Restored.TargetContainerId = Saved.TargetContainerId;
		Restored.QuantityTotal = Saved.QuantityTotal;
		Restored.QuantityCompleted = Saved.QuantityCompleted;
		Restored.State = static_cast<ERpgCraftingOrderState>(Saved.State);
		Restored.bPaused = Saved.bPaused;
		Restored.bUnitPaid = Saved.bUnitPaid;
		Restored.PausedRemainingTime = FMath::Min(Saved.RemainingTime, GetRecipeCraftTime(Recipe));
		for (const FRpgCraftingRefundSaveData& SavedRefund : Saved.UnitCredits)
		{
			TSubclassOf<URpgInventoryItemDefinition> Definition = SavedRefund.ItemDefinition.LoadSynchronous();
			if (!Definition || SavedRefund.Count <= 0 || !URpgInventoryFragment_StorageProfile::IsDefinitionIntrinsicallyCollapsible(Definition)) { return false; }
			FRpgCraftingRefundEntry& Refund = Restored.UnitCredits.AddDefaulted_GetRef();
			Refund.ItemDefinition = Definition;
			Refund.Count = SavedRefund.Count;
			Refund.InventoryId = SavedRefund.InventoryId;
			Refund.Inventory = RpgStorageAccessRules::FindPersistentInventory(GetWorld(), Refund.InventoryId);
		}
		// Credits must cover precisely the paid unit. Corrupt snapshots cannot mint refunds.
		TArray<FRpgCraftingResourceCost> Costs;
		if (!TryBuildAggregatedResourceCosts(Recipe->RequiredResources, Costs)) { return false; }
		TMap<UClass*, int64> Credits;
		for (const FRpgCraftingRefundEntry& Refund : Restored.UnitCredits) { Credits.FindOrAdd(Refund.ItemDefinition.Get()) += Refund.Count; }
		if (Restored.bUnitPaid)
		{
			for (const FRpgCraftingResourceCost& Cost : Costs)
			{
				const int64* Credit = Credits.Find(Cost.ItemDefinition.Get());
				if (!Credit || *Credit != Cost.Count) { return false; }
				Credits.Remove(Cost.ItemDefinition.Get());
			}
		}
		if (!Credits.IsEmpty()) { return false; }
	}
	ClearOrderTimers();
	CurrentOrder = MoveTemp(Restored);
	MarkCraftingStateDirty();
	return true;
}

void URpgCraftingStationComponent::ResumeRestoredCrafting()
{
	if (bPersistenceRestorePending || bMutationInProgress) { return; }
	TGuardValue<bool> MutationGuard(bMutationInProgress, true);
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
	ContinueOrderInternal();
}

void URpgCraftingStationComponent::MarkCraftingStateDirty(bool bPauseStateChanged, FGuid FinishedOrderId)
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
	Message.OrderId = FinishedOrderId.IsValid() ? FinishedOrderId : CurrentOrder.OrderId;
	Message.OrderState = CurrentOrder.State;
	Message.bOrderFinished = FinishedOrderId.IsValid();
	Message.bPauseStateChanged = bPauseStateChanged;

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
	MessageSubsystem.BroadcastMessage(TAG_Rpg_Crafting_Message_StationChanged, Message);
}
