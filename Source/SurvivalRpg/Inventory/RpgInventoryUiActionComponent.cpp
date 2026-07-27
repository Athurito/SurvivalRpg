#include "RpgInventoryUiActionComponent.h"
#include "RpgInventoryUiActionDomainHandlers.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/Pawn.h"
#include "RpgDroppedInventoryActor.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/Base/RpgBaseStorageStationComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"
#include "UObject/UObjectGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryUiActionComponent)

DEFINE_LOG_CATEGORY(LogRpgInventoryUiActions);

namespace
{
	bool IsExactUiActionPlacementSnapshot(
		const FRpgInventoryGridPlacement& A,
		const FRpgInventoryGridPlacement& B)
	{
		return A == B;
	}

	bool IsReplayEpochCurrent(
		const TWeakObjectPtr<URpgInventoryManagerComponent>& Inventory,
		bool bHadInventory,
		uint64 ExpectedEpoch)
	{
		if (!bHadInventory)
		{
			return true;
		}

		const URpgInventoryManagerComponent* CurrentInventory =
			Inventory.Get();
		return CurrentInventory &&
			CurrentInventory->GetMutationEpoch() == ExpectedEpoch;
	}

	FGameplayTag GetActionTagForEquipmentIntent(
		ERpgInventoryEquipmentIntentOperation Operation)
	{
		switch (Operation)
		{
		case ERpgInventoryEquipmentIntentOperation::EquipDefaultAndActivate:
			return RpgGameplayTags::
				Rpg_Inventory_Action_EquipAndActivate;
		case ERpgInventoryEquipmentIntentOperation::MoveToCarry:
			return RpgGameplayTags::Rpg_Inventory_Action_MoveToCarry;
		case ERpgInventoryEquipmentIntentOperation::EquipToSlot:
		case ERpgInventoryEquipmentIntentOperation::UnequipToContent:
		case ERpgInventoryEquipmentIntentOperation::ClearActiveSelection:
		default:
			return RpgGameplayTags::Rpg_Inventory_Action_Equip;
		}
	}
}

URpgInventoryUiActionComponent::URpgInventoryUiActionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	ManualDropActorClass = ARpgDroppedInventoryActor::StaticClass();
}

void URpgInventoryUiActionComponent::RequestMoveInventoryItem_Implementation(
	URpgInventoryManagerComponent* Inventory,
	FRpgInventoryMoveIntent Intent)
{
	FRpgInventoryTransactionActionHandler(*this).MoveInventoryItem(
		Inventory,
		MoveTemp(Intent));
}

void URpgInventoryUiActionComponent::RequestTransferInventoryItem_Implementation(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	FRpgInventoryTransferIntent Intent)
{
	FRpgInventoryTransactionActionHandler(*this).TransferInventoryItem(
		SourceInventory,
		TargetInventory,
		MoveTemp(Intent));
}

void URpgInventoryUiActionComponent::RequestUseInventoryItemById_Implementation(
	URpgInventoryManagerComponent* Inventory,
	FRpgInventoryUseRequest Request)
{
	URpgInventoryManagerComponent* SafeInventory =
		IsValid(Inventory) ? Inventory : nullptr;
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Use,
			ERpgInventoryActionFeedbackResult::ServerRejected,
			SafeInventory,
			nullptr,
			Request.UseCount,
			Request.RequestId,
			Request.ItemId);
		return;
	}

	const FUseRequestAdmission Admission =
		AdmitUseRequest(SafeInventory, Request);
	switch (Admission.Disposition)
	{
	case EUseRequestAdmissionDisposition::InFlight:
		// A reliable retry can re-enter while GAS is still synchronously activating.
		return;

	case EUseRequestAdmissionDisposition::Replay:
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Use,
			Admission.Result,
			SafeInventory,
			Admission.FeedbackItem.Get(),
			Admission.FeedbackUseCount,
			Request.RequestId,
			Request.ItemId);
		return;

	case EUseRequestAdmissionDisposition::Reject:
		// A collision may reference an inventory that has not passed access validation.
		// Reject feedback therefore carries stable identity only, never a freshly resolved item pointer.
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Use,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			SafeInventory,
			nullptr,
			Request.UseCount,
			Request.RequestId,
			Request.ItemId);
		return;

	case EUseRequestAdmissionDisposition::Execute:
	default:
		break;
	}

	const FRpgInventoryItemUseExecutionResult ExecutionResult =
		FRpgInventoryItemUseActionHandler(*this).ExecuteUseRequest(
			SafeInventory,
			Request);
	FinalizeUseRequest(
		SafeInventory,
		Request,
		ExecutionResult.Result,
		ExecutionResult.Item,
		ExecutionResult.FeedbackUseCount);
	SendActionFeedback(
		RpgGameplayTags::Rpg_Inventory_Action_Use,
		ExecutionResult.Result,
		SafeInventory,
		ExecutionResult.Item,
		ExecutionResult.FeedbackUseCount,
		Request.RequestId,
		Request.ItemId);
}

void URpgInventoryUiActionComponent::
	RequestApplyInventoryEquipmentIntent_Implementation(
		URpgInventoryManagerComponent* Inventory,
		FRpgInventoryEquipmentIntent Intent)
{
	FRpgInventoryEquipmentActionHandler(*this).ApplyInventoryEquipmentIntent(
		Inventory,
		MoveTemp(Intent));
}

void URpgInventoryUiActionComponent::RequestQuickTransferItem_Implementation(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	FRpgInventoryQuickTransferRequest Request)
{
	FRpgInventoryTransactionActionHandler(*this).QuickTransferItem(
		SourceInventory,
		TargetInventory,
		MoveTemp(Request));
}

void URpgInventoryUiActionComponent::RequestActivateCarrySlot_Implementation(
	int32 ActionBarSlotIndex,
	FGameplayTag ExpectedCarrySemanticRole)
{
	FRpgInventoryQuickAccessActionHandler(*this).ActivateCarrySlot(
		ActionBarSlotIndex,
		ExpectedCarrySemanticRole);
}

void URpgInventoryUiActionComponent::RequestClearActiveHands_Implementation()
{
	FRpgInventoryQuickAccessActionHandler(*this).ClearActiveHands();
}

void URpgInventoryUiActionComponent::RequestMutateQuickAccessBinding_Implementation(FRpgQuickAccessMutationRequest Request)
{
	FRpgInventoryQuickAccessActionHandler(*this).MutateBinding(
		MoveTemp(Request));
}

void URpgInventoryUiActionComponent::RequestSplitItemStackById_Implementation(
	URpgInventoryManagerComponent* Inventory,
	FRpgInventorySplitRequest Request)
{
	FRpgInventoryTransactionActionHandler(*this).SplitItemStackById(
		Inventory,
		MoveTemp(Request));
}

void URpgInventoryUiActionComponent::RequestDropInventoryItemById_Implementation(
	URpgInventoryManagerComponent* Inventory,
	FRpgInventoryManualDropRequest Request)
{
	FRpgInventoryManualDropActionHandler(*this).DropInventoryItemById(
		Inventory,
		MoveTemp(Request));
}

void URpgInventoryUiActionComponent::RequestDepositAllMaterialsToBase_Implementation(URpgBaseStorageStationComponent* Station)
{
	FRpgBaseStorageActionHandler(*this).DepositAllMaterials(Station);
}

void URpgInventoryUiActionComponent::RequestDepositMaterialStackToBase_Implementation(URpgBaseStorageStationComponent* Station, URpgInventoryItemInstance* Item, int32 StackCount)
{
	FRpgBaseStorageActionHandler(*this).DepositMaterialStack(
		Station,
		Item,
		StackCount);
}

void URpgInventoryUiActionComponent::RequestWithdrawResourceFromBase_Implementation(URpgBaseStorageStationComponent* Station, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 StackCount)
{
	FRpgBaseStorageActionHandler(*this).WithdrawResource(
		Station,
		ItemDefinition,
		StackCount);
}

void URpgInventoryUiActionComponent::RequestInstallBaseStorageUpgrade_Implementation(URpgBaseStorageStationComponent* Station, URpgBaseStorageUpgradeDefinition* UpgradeDefinition)
{
	FRpgBaseStorageActionHandler(*this).InstallUpgrade(
		Station,
		UpgradeDefinition);
}

void URpgInventoryUiActionComponent::RequestApplyBaseResourceSort_Implementation(URpgBaseStorageStationComponent* Station, ERpgInventorySortMode SortMode)
{
	FRpgBaseStorageActionHandler(*this).ApplyResourceSort(
		Station,
		SortMode);
}

void URpgInventoryUiActionComponent::RequestMoveBaseResourceEntry_Implementation(URpgBaseStorageStationComponent* Station, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 TargetIndex)
{
	FRpgBaseStorageActionHandler(*this).MoveResourceEntry(
		Station,
		ItemDefinition,
		TargetIndex);
}

void URpgInventoryUiActionComponent::RequestPlaceBaseBuildable_Implementation(ARpgBaseCampActor* BaseCamp, URpgBaseBuildableDefinition* BuildableDefinition, FTransform BuildTransform, bool bAutoContributeFromBase)
{
	FRpgBaseBuildingActionHandler(*this).PlaceBuildable(
		BaseCamp,
		BuildableDefinition,
		BuildTransform,
		bAutoContributeFromBase);
}

void URpgInventoryUiActionComponent::RequestContributeAllToBaseConstructionSite_Implementation(ARpgBaseConstructionSiteActor* ConstructionSite, bool bAllowBaseStorage)
{
	FRpgBaseBuildingActionHandler(*this).ContributeAll(
		ConstructionSite,
		bAllowBaseStorage);
}

void URpgInventoryUiActionComponent::RequestContributeMaterialToBaseConstructionSite_Implementation(ARpgBaseConstructionSiteActor* ConstructionSite, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 StackCount, bool bAllowBaseStorage)
{
	FRpgBaseBuildingActionHandler(*this).ContributeMaterial(
		ConstructionSite,
		ItemDefinition,
		StackCount,
		bAllowBaseStorage);
}

void URpgInventoryUiActionComponent::RequestCraftRecipe_Implementation(URpgCraftingStationComponent* CraftingStation, URpgCraftingRecipeDefinition* RecipeDefinition, int32 Quantity)
{
	FRpgCraftingActionHandler(*this).CraftRecipe(
		CraftingStation,
		RecipeDefinition,
		Quantity);
}

void URpgInventoryUiActionComponent::RequestCancelCraftJob_Implementation(URpgCraftingStationComponent* CraftingStation, FGuid JobId)
{
	FRpgCraftingActionHandler(*this).CancelCraftJob(
		CraftingStation,
		JobId);
}

void URpgInventoryUiActionComponent::RequestPauseCraftingStation_Implementation(URpgCraftingStationComponent* CraftingStation)
{
	FRpgCraftingActionHandler(*this).PauseStation(CraftingStation);
}

void URpgInventoryUiActionComponent::RequestResumeCraftingStation_Implementation(URpgCraftingStationComponent* CraftingStation)
{
	FRpgCraftingActionHandler(*this).ResumeStation(CraftingStation);
}

void URpgInventoryUiActionComponent::RequestSetCraftingOutputAutoDepositEnabled_Implementation(URpgCraftingStationComponent* CraftingStation, bool bEnabled)
{
	FRpgCraftingActionHandler(*this).SetOutputAutoDepositEnabled(
		CraftingStation,
		bEnabled);
}

bool URpgInventoryUiActionComponent::CanAccessInventory(URpgInventoryManagerComponent* Inventory) const
{
	return FRpgInventoryUiActionDomainHandler(*this).
		EvaluateInventoryAccess(Inventory);
}

URpgInventoryManagerComponent* URpgInventoryUiActionComponent::FindPlayerInventory() const
{
	if (const AController* OwnerController = Cast<AController>(GetOwner()))
	{
		if (const ARpgPlayerState* RpgPlayerState = OwnerController->GetPlayerState<ARpgPlayerState>())
		{
			return RpgPlayerState->GetInventoryManagerComponent();
		}
	}

	return nullptr;
}

URpgEquipmentLoadoutComponent* URpgInventoryUiActionComponent::FindEquipmentLoadout() const
{
	if (const ARpgPlayerController* PlayerController = Cast<ARpgPlayerController>(GetOwner()))
	{
		return PlayerController->GetEquipmentLoadoutComponent();
	}

	return nullptr;
}

URpgPlayerInventoryLayoutComponent* URpgInventoryUiActionComponent::FindPlayerInventoryLayout() const
{
	if (const ARpgPlayerController* PlayerController = Cast<ARpgPlayerController>(GetOwner()))
	{
		return PlayerController->GetPlayerInventoryLayoutComponent();
	}

	return GetOwner() ? GetOwner()->FindComponentByClass<URpgPlayerInventoryLayoutComponent>() : nullptr;
}

URpgActionBarComponent* URpgInventoryUiActionComponent::FindActionBar() const
{
	if (const ARpgPlayerController* PlayerController = Cast<ARpgPlayerController>(GetOwner()))
	{
		return PlayerController->GetActionBarComponent();
	}

	return GetOwner() ? GetOwner()->FindComponentByClass<URpgActionBarComponent>() : nullptr;
}

URpgAbilitySystemComponent* URpgInventoryUiActionComponent::FindPlayerAbilitySystem() const
{
	if (const AController* OwnerController = Cast<AController>(GetOwner()))
	{
		if (const ARpgPlayerState* RpgPlayerState = OwnerController->GetPlayerState<ARpgPlayerState>())
		{
			return RpgPlayerState->GetRpgAbilitySystemComponent();
		}
	}

	return nullptr;
}

FRpgInventoryPlacementPlan
URpgInventoryUiActionComponent::PlanExactTransferPlacement(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	const FRpgInventoryTransferIntent& Intent) const
{
	return FRpgInventoryTransactionQueryHandler(*this).
		PlanExactTransferPlacement(
			SourceInventory,
			TargetInventory,
			Intent);
}

FRpgInventoryPlacementPlan
URpgInventoryUiActionComponent::PlanEquipmentIntentPlacement(
	URpgInventoryManagerComponent* Inventory,
	const FRpgInventoryEquipmentIntent& Intent,
	FRpgInventoryGridPlacement& OutTargetPlacement) const
{
	return FRpgInventoryEquipmentQueryHandler(*this).PlanEquipmentIntentPlacement(
		Inventory,
		Intent,
		OutTargetPlacement);
}

FRpgInventoryPlacementPlan
URpgInventoryUiActionComponent::PlanQuickTransferDestination(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	const FRpgInventoryQuickTransferRequest& Request,
	FRpgInventoryContainerHandle& OutTargetContainer,
	FRpgInventoryGridPlacement& OutTargetPlacement) const
{
	return FRpgInventoryTransactionQueryHandler(*this).
		PlanQuickTransferDestination(
			SourceInventory,
			TargetInventory,
			Request,
			OutTargetContainer,
			OutTargetPlacement);
}

bool URpgInventoryUiActionComponent::FindQuickTransferDestination(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	const FRpgInventoryQuickTransferRequest& Request,
	FRpgInventoryContainerHandle& OutTargetContainer,
	FRpgInventoryGridPlacement& OutTargetPlacement) const
{
	return FRpgInventoryTransactionQueryHandler(*this).
		FindQuickTransferDestination(
			SourceInventory,
			TargetInventory,
			Request,
			OutTargetContainer,
			OutTargetPlacement);
}



bool URpgInventoryUiActionComponent::CanSplitItemStack(
	URpgInventoryManagerComponent* Inventory,
	URpgInventoryItemInstance* Item,
	int32 SplitCount,
	FRpgInventoryGridPlacement TargetPlacement,
	int32& OutSplitCount,
	FRpgInventoryGridPlacement& OutTargetPlacement) const
{
	return FRpgInventoryTransactionQueryHandler(*this).
		CanSplitItemStack(
			Inventory,
			Item,
			SplitCount,
			TargetPlacement,
			OutSplitCount,
			OutTargetPlacement);
}


bool URpgInventoryUiActionComponent::CanAccessBaseStorageStation(const URpgBaseStorageStationComponent* Station) const
{
	if (!Station)
	{
		return false;
	}

	const AController* OwnerController = Cast<AController>(GetOwner());
	const AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	return Station->CanActorAccess(RequestingActor);
}

bool URpgInventoryUiActionComponent::CanMoveItemToFirstCompatibleContentSlot(
	URpgInventoryItemInstance* Item,
	FRpgInventoryGridPlacement& OutTargetPlacement) const
{
	return FRpgInventoryEquipmentQueryHandler(*this)
		.CanMoveItemToFirstCompatibleContentSlot(Item, OutTargetPlacement);
}

bool URpgInventoryUiActionComponent::IsPlayerEquipmentPlacement(
	const FRpgInventoryGridPlacement& Placement) const
{
	const URpgPlayerInventoryLayoutComponent* InventoryLayout =
		FindPlayerInventoryLayout();
	FRpgInventorySlotAddress Address;
	return InventoryLayout && Placement.IsValid() &&
		InventoryLayout->TryMakeSlotAddressFromPlacement(
			Placement,
			Address) &&
		(InventoryLayout->IsGearSlotAddress(Address) ||
			InventoryLayout->IsCarrySlotAddress(Address));
}

void URpgInventoryUiActionComponent::SyncEquipmentLoadoutFromGearSlots() const
{
	URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout();
	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	URpgPlayerInventoryLayoutComponent* InventoryLayout =
		FindPlayerInventoryLayout();
	if (!EquipmentLoadout || !PlayerInventory || !InventoryLayout)
	{
		return;
	}

	const ERpgEquipmentSlot PhysicalSlots[] =
	{
		ERpgEquipmentSlot::Head,
		ERpgEquipmentSlot::Chest,
		ERpgEquipmentSlot::Hands,
		ERpgEquipmentSlot::Legs,
		ERpgEquipmentSlot::Feet,
		ERpgEquipmentSlot::Backpack,
		ERpgEquipmentSlot::Belt,
		ERpgEquipmentSlot::Pouch,
		ERpgEquipmentSlot::ResourceBag
	};

	bool bPhysicalMirrorChanged = false;
	for (const ERpgEquipmentSlot EquipmentSlot : PhysicalSlots)
	{
		FRpgInventorySlotAddress Address;
		URpgInventoryItemInstance* PhysicalItem = nullptr;
		if (InventoryLayout->TryMakeGearSlotAddress(
				EquipmentSlot,
				Address))
		{
			PhysicalItem =
				InventoryLayout->GetItemInSlotAddress(Address);
		}

		if (EquipmentLoadout->GetItemInEquipmentSlot(
				EquipmentSlot) != PhysicalItem)
		{
			bPhysicalMirrorChanged = true;
			break;
		}
	}

	if (bPhysicalMirrorChanged)
	{
		// Rebuild once from the complete Gear snapshot so observers never see per-slot intermediate states.
		EquipmentLoadout->ReconcilePhysicalEquipmentFromInventory();
		return;
	}

	// Carry changes affect load without changing the non-hand Gear mirror. Do not rebuild runtime actors or GAS grants.
	EquipmentLoadout->ReconcileEquipmentLoadFromInventory();
}

void URpgInventoryUiActionComponent::SyncActiveHandsFromCarrySlots() const
{
	URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout();
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	if (!EquipmentLoadout || !InventoryLayout)
	{
		return;
	}

	auto IsItemInCarrySlot = [InventoryLayout](
		const URpgInventoryItemInstance* Item,
		ERpgEquipmentSlot EquipmentSlotRole)
	{
		if (!Item)
		{
			return false;
		}

		for (const FRpgInventorySlotGroupView& Group : InventoryLayout->GetSlotGroups())
		{
			if (Group.GroupKind != ERpgInventorySlotGroupKind::Carry ||
				Group.EquipmentSlotRole != EquipmentSlotRole)
			{
				continue;
			}

			for (int32 Y = 0; Y < Group.GridSize.Height; ++Y)
			{
				for (int32 X = 0; X < Group.GridSize.Width; ++X)
				{
					if (InventoryLayout->GetItemInSlotAddress(Group.MakeAddress(X, Y)) == Item)
					{
						return true;
					}
				}
			}
		}

		return false;
	};

	if (URpgInventoryItemInstance* MainHandItem = EquipmentLoadout->GetItemInEquipmentSlot(ERpgEquipmentSlot::MainHand);
		MainHandItem &&
		!IsItemInCarrySlot(
			MainHandItem,
			ERpgEquipmentSlot::MainHand))
	{
		EquipmentLoadout->ClearActiveMainHand();
	}

	if (URpgInventoryItemInstance* OffHandItem = EquipmentLoadout->GetItemInEquipmentSlot(ERpgEquipmentSlot::OffHand);
		OffHandItem &&
		!IsItemInCarrySlot(
			OffHandItem,
			ERpgEquipmentSlot::OffHand))
	{
		EquipmentLoadout->ClearActiveOffHand(true);
	}
}

bool URpgInventoryUiActionComponent::AreUseRequestsEquivalent(
	const FRpgInventoryUseRequest& A,
	const FRpgInventoryUseRequest& B)
{
	return A.RequestId == B.RequestId &&
		A.ItemId == B.ItemId &&
		A.UseCount == B.UseCount;
}

URpgInventoryUiActionComponent::FUseRequestAdmission
	URpgInventoryUiActionComponent::AdmitUseRequest(
	URpgInventoryManagerComponent* Inventory,
	const FRpgInventoryUseRequest& Request)
{
	FUseRequestAdmission Admission;
	if (!Request.RequestId.IsValid())
	{
		return Admission;
	}

	const bool bHasInventory = IsValid(Inventory);
	if (FRecentUseRequestResult* CachedResult =
		RecentUseRequestResults.Find(Request.RequestId))
	{
		// Retain a tombstone for an inventory that disappeared so this cached
		// RequestId cannot execute GAS against an unrelated inventory.
		if (CachedResult->bHadInventory &&
			!CachedResult->Inventory.IsValid())
		{
			return Admission;
		}

		if (CachedResult->bHadInventory != bHasInventory ||
			(bHasInventory && CachedResult->Inventory.Get() != Inventory))
		{
			return Admission;
		}

		const uint64 CurrentMutationEpoch = bHasInventory
			? Inventory->GetMutationEpoch()
			: 0;
		if (CachedResult->InventoryMutationEpoch !=
			CurrentMutationEpoch)
		{
			if (CachedResult->bInFlight)
			{
				// An in-flight entry is never evicted. The changed restore epoch
				// makes this a different fingerprint and therefore a collision.
				return Admission;
			}

			RemoveRecentUseRequest(Request.RequestId);
		}
		else
		{
			if (!AreUseRequestsEquivalent(
					CachedResult->Request,
					Request))
			{
				return Admission;
			}

			if (CachedResult->bInFlight)
			{
				Admission.Disposition =
					EUseRequestAdmissionDisposition::InFlight;
				return Admission;
			}

			// A completed retry must revalidate the authorization that made
			// its original item context safe to expose. Persist the redacted
			// denial so restoring access cannot revive the old object pointer.
			if (bHasInventory && !CanAccessInventory(Inventory))
			{
				CachedResult->Result =
					ERpgInventoryActionFeedbackResult::NoAccess;
				CachedResult->FeedbackItem.Reset();
				CachedResult->FeedbackUseCount = Request.UseCount;
			}

			Admission.Disposition =
				EUseRequestAdmissionDisposition::Replay;
			Admission.Result = CachedResult->Result;
			Admission.FeedbackItem = CachedResult->FeedbackItem;
			Admission.FeedbackUseCount =
				CachedResult->FeedbackUseCount;
			return Admission;
		}
	}

	if (!MakeRoomForUseRequest())
	{
		return Admission;
	}

	FRecentUseRequestResult PendingResult;
	PendingResult.Inventory = Inventory;
	PendingResult.bHadInventory = bHasInventory;
	PendingResult.InventoryMutationEpoch = bHasInventory
		? Inventory->GetMutationEpoch()
		: 0;
	PendingResult.Request = Request;
	RecentUseRequestResults.Add(
		Request.RequestId,
		MoveTemp(PendingResult));
	RecentUseRequestOrder.Add(Request.RequestId);
	Admission.Disposition =
		EUseRequestAdmissionDisposition::Execute;
	return Admission;
}

void URpgInventoryUiActionComponent::FinalizeUseRequest(
	URpgInventoryManagerComponent* Inventory,
	const FRpgInventoryUseRequest& Request,
	ERpgInventoryActionFeedbackResult Result,
	URpgInventoryItemInstance* FeedbackItem,
	int32 FeedbackUseCount)
{
	FRecentUseRequestResult* PendingResult =
		RecentUseRequestResults.Find(Request.RequestId);
	if (!PendingResult || !PendingResult->bInFlight ||
		!AreUseRequestsEquivalent(PendingResult->Request, Request))
	{
		return;
	}

	URpgInventoryItemInstance* AuthorizedFeedbackItem =
		Result == ERpgInventoryActionFeedbackResult::NoAccess
			? nullptr
			: FeedbackItem;
	PendingResult->Result = Result;
	PendingResult->FeedbackItem = AuthorizedFeedbackItem;
	PendingResult->FeedbackUseCount = FeedbackUseCount;
	PendingResult->bInFlight = false;

	if (PendingResult->bHadInventory)
	{
		if (!PendingResult->Inventory.IsValid())
		{
			// Keep the completed tombstone so retries fail closed.
			return;
		}

		if (PendingResult->Inventory.Get() != Inventory ||
			!IsValid(Inventory) ||
			PendingResult->InventoryMutationEpoch !=
				Inventory->GetMutationEpoch())
		{
			// Persistence restore changes MutationEpoch and invalidates replay.
			RemoveRecentUseRequest(Request.RequestId);
		}
	}
	else if (IsValid(Inventory))
	{
		RemoveRecentUseRequest(Request.RequestId);
	}
}

void URpgInventoryUiActionComponent::RemoveRecentUseRequest(
	const FGuid& RequestId)
{
	RecentUseRequestResults.Remove(RequestId);
	RecentUseRequestOrder.Remove(RequestId);
}

bool URpgInventoryUiActionComponent::MakeRoomForUseRequest()
{
	for (int32 Index = 0;
		Index < RecentUseRequestOrder.Num() &&
		RecentUseRequestResults.Num() >= MaxRecentUseRequestResults;)
	{
		const FGuid CandidateId = RecentUseRequestOrder[Index];
		const FRecentUseRequestResult* Candidate =
			RecentUseRequestResults.Find(CandidateId);
		if (!Candidate || !Candidate->bInFlight)
		{
			RecentUseRequestResults.Remove(CandidateId);
			RecentUseRequestOrder.RemoveAt(
				Index,
				1,
				EAllowShrinking::No);
			continue;
		}

		++Index;
	}

	return RecentUseRequestResults.Num() <
		MaxRecentUseRequestResults;
}

bool URpgInventoryUiActionComponent::AreSplitRequestsEquivalent(
	const FRpgInventorySplitRequest& A,
	const FRpgInventorySplitRequest& B)
{
	return A.RequestId == B.RequestId &&
		A.ItemId == B.ItemId &&
		A.ExpectedEntryId == B.ExpectedEntryId &&
		IsExactUiActionPlacementSnapshot(
			A.ExpectedSourcePlacement,
			B.ExpectedSourcePlacement) &&
		A.ExpectedSourceQuantity == B.ExpectedSourceQuantity &&
		A.SplitCount == B.SplitCount &&
		IsExactUiActionPlacementSnapshot(
			A.TargetPlacement,
			B.TargetPlacement);
}

bool URpgInventoryUiActionComponent::TryReplayRecentSplitResult(
	URpgInventoryManagerComponent* Inventory,
	const FRpgInventorySplitRequest& Request)
{
	if (!Request.RequestId.IsValid())
	{
		return false;
	}

	const FRecentSplitRequestResult* CachedResult =
		RecentSplitRequestResults.Find(Request.RequestId);
	if (!CachedResult)
	{
		return false;
	}

	if (!IsReplayEpochCurrent(
			CachedResult->Inventory,
			CachedResult->bHadInventory,
			CachedResult->InventoryMutationEpoch))
	{
		RecentSplitRequestResults.Remove(Request.RequestId);
		RecentSplitRequestOrder.Remove(Request.RequestId);
		return false;
	}

	if (CachedResult->Inventory.Get() != Inventory ||
		!AreSplitRequestsEquivalent(CachedResult->Request, Request))
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Rejected split RequestId collision for %s."),
			*Request.RequestId.ToString());
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Split,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			Inventory,
			nullptr,
			Request.SplitCount,
			Request.RequestId,
			Request.ItemId);
		return true;
	}

	if (!CanAccessInventory(Inventory))
	{
		SendAndCacheSplitFeedback(
			Inventory,
			Request,
			ERpgInventoryActionFeedbackResult::NoAccess,
			nullptr,
			Request.SplitCount);
		return true;
	}

	URpgInventoryManagerComponent* CachedInventory =
		CachedResult->Inventory.Get();
	SendActionFeedback(
		RpgGameplayTags::Rpg_Inventory_Action_Split,
		CachedResult->Result,
		CachedInventory,
		CachedResult->FeedbackItem.Get(),
		CachedResult->FeedbackStackCount,
		CachedResult->Request.RequestId,
		CachedResult->Request.ItemId);
	return true;
}

void URpgInventoryUiActionComponent::SendAndCacheSplitFeedback(
	URpgInventoryManagerComponent* Inventory,
	const FRpgInventorySplitRequest& Request,
	ERpgInventoryActionFeedbackResult Result,
	URpgInventoryItemInstance* Item,
	int32 FeedbackStackCount)
{
	URpgInventoryItemInstance* AuthorizedFeedbackItem =
		Result == ERpgInventoryActionFeedbackResult::NoAccess
			? nullptr
			: Item;
	if (Request.RequestId.IsValid())
	{
		FRecentSplitRequestResult CachedResult;
		CachedResult.Inventory = Inventory;
		CachedResult.bHadInventory = Inventory != nullptr;
		CachedResult.InventoryMutationEpoch = Inventory
			? Inventory->GetMutationEpoch()
			: 0;
		CachedResult.Request = Request;
		CachedResult.Result = Result;
		CachedResult.FeedbackItem = AuthorizedFeedbackItem;
		CachedResult.FeedbackStackCount = FeedbackStackCount;
		RecentSplitRequestResults.Add(
			Request.RequestId,
			MoveTemp(CachedResult));
		RecentSplitRequestOrder.Remove(Request.RequestId);
		RecentSplitRequestOrder.Add(Request.RequestId);
		while (RecentSplitRequestOrder.Num() >
			MaxRecentSplitRequestResults)
		{
			RecentSplitRequestResults.Remove(
				RecentSplitRequestOrder[0]);
			RecentSplitRequestOrder.RemoveAt(
				0,
				1,
				EAllowShrinking::No);
		}
	}

	SendActionFeedback(
		RpgGameplayTags::Rpg_Inventory_Action_Split,
		Result,
		Inventory,
		AuthorizedFeedbackItem,
		FeedbackStackCount,
		Request.RequestId,
		Request.ItemId);
}

bool URpgInventoryUiActionComponent::AreEquipmentIntentsEquivalent(
	const FRpgInventoryEquipmentIntent& A,
	const FRpgInventoryEquipmentIntent& B)
{
	return A.RequestId == B.RequestId &&
		A.ItemId == B.ItemId &&
		A.ExpectedEntryId == B.ExpectedEntryId &&
		IsExactUiActionPlacementSnapshot(
			A.ExpectedSourcePlacement,
			B.ExpectedSourcePlacement) &&
		A.ExpectedQuantity == B.ExpectedQuantity &&
		A.Operation == B.Operation &&
		A.TargetEquipmentSlot == B.TargetEquipmentSlot &&
		IsExactUiActionPlacementSnapshot(
			A.ExactTargetPlacement,
			B.ExactTargetPlacement);
}

bool URpgInventoryUiActionComponent::
	TryReplayRecentEquipmentIntentResult(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryEquipmentIntent& Intent)
{
	if (!Intent.RequestId.IsValid())
	{
		return false;
	}

	const FRecentEquipmentIntentResult* CachedResult =
		RecentEquipmentIntentResults.Find(Intent.RequestId);
	if (!CachedResult)
	{
		return false;
	}

	if (!IsReplayEpochCurrent(
			CachedResult->Inventory,
			CachedResult->bHadInventory,
			CachedResult->InventoryMutationEpoch))
	{
		RecentEquipmentIntentResults.Remove(Intent.RequestId);
		RecentEquipmentIntentOrder.Remove(Intent.RequestId);
		return false;
	}

	if (CachedResult->Inventory.Get() != Inventory ||
		!AreEquipmentIntentsEquivalent(
			CachedResult->Intent,
			Intent))
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Rejected equipment RequestId collision for %s."),
			*Intent.RequestId.ToString());
		SendActionFeedback(
			GetActionTagForEquipmentIntent(Intent.Operation),
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			Inventory,
			nullptr,
			Intent.ExpectedQuantity,
			Intent.RequestId,
			Intent.ItemId);
		return true;
	}

	if (!CanAccessInventory(Inventory))
	{
		SendAndCacheEquipmentIntentFeedback(
			Inventory,
			Intent,
			ERpgInventoryActionFeedbackResult::NoAccess,
			nullptr,
			Intent.ExpectedQuantity);
		return true;
	}

	URpgInventoryManagerComponent* CachedInventory =
		CachedResult->Inventory.Get();
	SendActionFeedback(
		GetActionTagForEquipmentIntent(
			CachedResult->Intent.Operation),
		CachedResult->Result,
		CachedInventory,
		CachedResult->FeedbackItem.Get(),
		CachedResult->FeedbackStackCount,
		CachedResult->Intent.RequestId,
		CachedResult->Intent.ItemId);
	return true;
}

void URpgInventoryUiActionComponent::
	SendAndCacheEquipmentIntentFeedback(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryEquipmentIntent& Intent,
		ERpgInventoryActionFeedbackResult Result,
		URpgInventoryItemInstance* Item,
		int32 FeedbackStackCount)
{
	URpgInventoryItemInstance* AuthorizedFeedbackItem =
		Result == ERpgInventoryActionFeedbackResult::NoAccess
			? nullptr
			: Item;
	if (Intent.RequestId.IsValid())
	{
		FRecentEquipmentIntentResult CachedResult;
		CachedResult.Inventory = Inventory;
		CachedResult.bHadInventory = Inventory != nullptr;
		CachedResult.InventoryMutationEpoch = Inventory
			? Inventory->GetMutationEpoch()
			: 0;
		CachedResult.Intent = Intent;
		CachedResult.Result = Result;
		CachedResult.FeedbackItem = AuthorizedFeedbackItem;
		CachedResult.FeedbackStackCount = FeedbackStackCount;
		RecentEquipmentIntentResults.Add(
			Intent.RequestId,
			MoveTemp(CachedResult));
		RecentEquipmentIntentOrder.Remove(Intent.RequestId);
		RecentEquipmentIntentOrder.Add(Intent.RequestId);
		while (RecentEquipmentIntentOrder.Num() >
			MaxRecentEquipmentIntentResults)
		{
			RecentEquipmentIntentResults.Remove(
				RecentEquipmentIntentOrder[0]);
			RecentEquipmentIntentOrder.RemoveAt(
				0,
				1,
				EAllowShrinking::No);
		}
	}

	SendActionFeedback(
		GetActionTagForEquipmentIntent(Intent.Operation),
		Result,
		Inventory,
		AuthorizedFeedbackItem,
		FeedbackStackCount,
		Intent.RequestId,
		Intent.ItemId);
}

bool URpgInventoryUiActionComponent::AreExactTransferIntentsEquivalent(
	const FRpgInventoryTransferIntent& A,
	const FRpgInventoryTransferIntent& B)
{
	return A.RequestId == B.RequestId &&
		A.ItemId == B.ItemId &&
		A.ExpectedEntryId == B.ExpectedEntryId &&
		IsExactUiActionPlacementSnapshot(
			A.ExpectedSourcePlacement,
			B.ExpectedSourcePlacement) &&
		A.ExpectedSourceQuantity == B.ExpectedSourceQuantity &&
		A.TargetContainer == B.TargetContainer &&
		A.TargetPlacement == B.TargetPlacement &&
		A.Quantity == B.Quantity;
}

bool URpgInventoryUiActionComponent::TryReplayRecentExactTransferResult(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	const FRpgInventoryTransferIntent& Intent)
{
	if (!Intent.RequestId.IsValid())
	{
		return false;
	}

	const FRecentExactTransferResult* CachedResult =
		RecentExactTransferResults.Find(Intent.RequestId);
	if (!CachedResult)
	{
		return false;
	}

	if (!IsReplayEpochCurrent(
			CachedResult->SourceInventory,
			CachedResult->bHadSourceInventory,
			CachedResult->SourceMutationEpoch) ||
		!IsReplayEpochCurrent(
			CachedResult->TargetInventory,
			CachedResult->bHadTargetInventory,
			CachedResult->TargetMutationEpoch))
	{
		RecentExactTransferResults.Remove(Intent.RequestId);
		RecentExactTransferOrder.Remove(Intent.RequestId);
		return false;
	}

	if (CachedResult->SourceInventory.Get() != SourceInventory ||
		CachedResult->TargetInventory.Get() != TargetInventory ||
		!AreExactTransferIntentsEquivalent(
			CachedResult->Intent,
			Intent))
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Rejected exact-transfer RequestId collision for %s."),
			*Intent.RequestId.ToString());
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Transfer,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			SourceInventory,
			nullptr,
			Intent.Quantity,
			Intent.RequestId,
			Intent.ItemId);
		return true;
	}

	// Replay must not outlive the authorization that made the original
	// feedback item safe to expose. Replace the cached result with a redacted
	// denial so a later access change cannot reveal the old item context.
	if (!CanAccessInventory(SourceInventory) ||
		!CanAccessInventory(TargetInventory))
	{
		SendAndCacheExactTransferFeedback(
			SourceInventory,
			TargetInventory,
			Intent,
			ERpgInventoryActionFeedbackResult::NoAccess,
			nullptr,
			Intent.Quantity);
		return true;
	}

	URpgInventoryManagerComponent* CachedSource =
		CachedResult->SourceInventory.Get();
	SendActionFeedback(
		RpgGameplayTags::Rpg_Inventory_Action_Transfer,
		CachedResult->Result,
		CachedSource,
		CachedResult->FeedbackItem.Get(),
		CachedResult->FeedbackStackCount,
		CachedResult->Intent.RequestId,
		CachedResult->Intent.ItemId);
	return true;
}

void URpgInventoryUiActionComponent::SendAndCacheExactTransferFeedback(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	const FRpgInventoryTransferIntent& Intent,
	ERpgInventoryActionFeedbackResult Result,
	URpgInventoryItemInstance* Item,
	int32 FeedbackStackCount)
{
	URpgInventoryItemInstance* AuthorizedFeedbackItem =
		Result == ERpgInventoryActionFeedbackResult::NoAccess
			? nullptr
			: Item;
	if (Intent.RequestId.IsValid())
	{
		FRecentExactTransferResult CachedResult;
		CachedResult.SourceInventory = SourceInventory;
		CachedResult.TargetInventory = TargetInventory;
		CachedResult.bHadSourceInventory = SourceInventory != nullptr;
		CachedResult.bHadTargetInventory = TargetInventory != nullptr;
		CachedResult.SourceMutationEpoch = SourceInventory
			? SourceInventory->GetMutationEpoch()
			: 0;
		CachedResult.TargetMutationEpoch = TargetInventory
			? TargetInventory->GetMutationEpoch()
			: 0;
		CachedResult.Intent = Intent;
		CachedResult.Result = Result;
		CachedResult.FeedbackItem = AuthorizedFeedbackItem;
		CachedResult.FeedbackStackCount = FeedbackStackCount;
		RecentExactTransferResults.Add(
			Intent.RequestId,
			MoveTemp(CachedResult));
		RecentExactTransferOrder.Remove(Intent.RequestId);
		RecentExactTransferOrder.Add(Intent.RequestId);
		while (RecentExactTransferOrder.Num() >
			MaxRecentExactTransferResults)
		{
			RecentExactTransferResults.Remove(
				RecentExactTransferOrder[0]);
			RecentExactTransferOrder.RemoveAt(
				0,
				1,
				EAllowShrinking::No);
		}
	}

	SendActionFeedback(
		RpgGameplayTags::Rpg_Inventory_Action_Transfer,
		Result,
		SourceInventory,
		AuthorizedFeedbackItem,
		FeedbackStackCount,
		Intent.RequestId,
		Intent.ItemId);
}

bool URpgInventoryUiActionComponent::AreQuickTransferRequestsEquivalent(
	const FRpgInventoryQuickTransferRequest& A,
	const FRpgInventoryQuickTransferRequest& B)
{
	return A.RequestId == B.RequestId &&
		A.ItemId == B.ItemId &&
		A.ExpectedEntryId == B.ExpectedEntryId &&
		IsExactUiActionPlacementSnapshot(
			A.ExpectedSourcePlacement,
			B.ExpectedSourcePlacement) &&
		A.ExpectedSourceQuantity == B.ExpectedSourceQuantity &&
		A.StackCount == B.StackCount &&
		A.PreferredTargetContainers == B.PreferredTargetContainers;
}

bool URpgInventoryUiActionComponent::TryReplayRecentQuickTransferResult(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	const FRpgInventoryQuickTransferRequest& Request)
{
	if (!Request.RequestId.IsValid())
	{
		return false;
	}

	const FRecentQuickTransferResult* CachedResult =
		RecentQuickTransferResults.Find(Request.RequestId);
	if (!CachedResult)
	{
		return false;
	}

	if (!IsReplayEpochCurrent(
			CachedResult->SourceInventory,
			CachedResult->bHadSourceInventory,
			CachedResult->SourceMutationEpoch) ||
		!IsReplayEpochCurrent(
			CachedResult->TargetInventory,
			CachedResult->bHadTargetInventory,
			CachedResult->TargetMutationEpoch))
	{
		RecentQuickTransferResults.Remove(Request.RequestId);
		RecentQuickTransferOrder.Remove(Request.RequestId);
		return false;
	}

	if (CachedResult->SourceInventory.Get() != SourceInventory ||
		CachedResult->TargetInventory.Get() != TargetInventory ||
		!AreQuickTransferRequestsEquivalent(
			CachedResult->Request,
			Request))
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Rejected quick-transfer RequestId collision for %s."),
			*Request.RequestId.ToString());
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Transfer,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			SourceInventory,
			nullptr,
			Request.StackCount,
			Request.RequestId,
			Request.ItemId);
		return true;
	}

	if (!CanAccessInventory(SourceInventory) ||
		!CanAccessInventory(TargetInventory))
	{
		SendAndCacheQuickTransferFeedback(
			SourceInventory,
			TargetInventory,
			Request,
			ERpgInventoryActionFeedbackResult::NoAccess,
			nullptr,
			Request.StackCount);
		return true;
	}

	URpgInventoryManagerComponent* CachedSource =
		CachedResult->SourceInventory.Get();
	SendActionFeedback(
		RpgGameplayTags::Rpg_Inventory_Action_Transfer,
		CachedResult->Result,
		CachedSource,
		CachedResult->FeedbackItem.Get(),
		CachedResult->FeedbackStackCount,
		CachedResult->Request.RequestId,
		CachedResult->Request.ItemId);
	return true;
}

void URpgInventoryUiActionComponent::SendAndCacheQuickTransferFeedback(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	const FRpgInventoryQuickTransferRequest& Request,
	ERpgInventoryActionFeedbackResult Result,
	URpgInventoryItemInstance* Item,
	int32 FeedbackStackCount)
{
	URpgInventoryItemInstance* AuthorizedFeedbackItem =
		Result == ERpgInventoryActionFeedbackResult::NoAccess
			? nullptr
			: Item;
	if (Request.RequestId.IsValid())
	{
		FRecentQuickTransferResult CachedResult;
		CachedResult.SourceInventory = SourceInventory;
		CachedResult.TargetInventory = TargetInventory;
		CachedResult.bHadSourceInventory = SourceInventory != nullptr;
		CachedResult.bHadTargetInventory = TargetInventory != nullptr;
		CachedResult.SourceMutationEpoch = SourceInventory
			? SourceInventory->GetMutationEpoch()
			: 0;
		CachedResult.TargetMutationEpoch = TargetInventory
			? TargetInventory->GetMutationEpoch()
			: 0;
		CachedResult.Request = Request;
		CachedResult.Result = Result;
		CachedResult.FeedbackItem = AuthorizedFeedbackItem;
		CachedResult.FeedbackStackCount = FeedbackStackCount;
		RecentQuickTransferResults.Add(
			Request.RequestId,
			MoveTemp(CachedResult));
		RecentQuickTransferOrder.Remove(Request.RequestId);
		RecentQuickTransferOrder.Add(Request.RequestId);
		while (RecentQuickTransferOrder.Num() >
			MaxRecentQuickTransferResults)
		{
			RecentQuickTransferResults.Remove(
				RecentQuickTransferOrder[0]);
			RecentQuickTransferOrder.RemoveAt(
				0,
				1,
				EAllowShrinking::No);
		}
	}

	SendActionFeedback(
		RpgGameplayTags::Rpg_Inventory_Action_Transfer,
		Result,
		SourceInventory,
		AuthorizedFeedbackItem,
		FeedbackStackCount,
		Request.RequestId,
		Request.ItemId);
}

bool URpgInventoryUiActionComponent::AreManualDropRequestsEquivalent(
	const FRpgInventoryManualDropRequest& A,
	const FRpgInventoryManualDropRequest& B)
{
	return A.RequestId == B.RequestId &&
		A.EntryId == B.EntryId &&
		A.ItemId == B.ItemId &&
		IsExactUiActionPlacementSnapshot(
			A.ExpectedSourcePlacement,
			B.ExpectedSourcePlacement) &&
		A.ExpectedSourceQuantity == B.ExpectedSourceQuantity &&
		A.StackCount == B.StackCount &&
		A.bConfirmed == B.bConfirmed;
}

bool URpgInventoryUiActionComponent::TryReplayRecentManualDropResult(
	URpgInventoryManagerComponent* Inventory,
	const FRpgInventoryManualDropRequest& Request)
{
	if (!Request.RequestId.IsValid())
	{
		return false;
	}

	const FRecentManualDropResult* CachedResult =
		RecentManualDropResults.Find(Request.RequestId);
	if (!CachedResult)
	{
		return false;
	}

	if (!IsReplayEpochCurrent(
			CachedResult->Inventory,
			CachedResult->bHadInventory,
			CachedResult->InventoryMutationEpoch) ||
		!IsReplayEpochCurrent(
			CachedResult->TargetInventory,
			CachedResult->bHadTargetInventory,
			CachedResult->TargetMutationEpoch))
	{
		RecentManualDropResults.Remove(Request.RequestId);
		RecentManualDropOrder.Remove(Request.RequestId);
		return false;
	}

	if (CachedResult->Inventory.Get() != Inventory ||
		!AreManualDropRequestsEquivalent(CachedResult->Request, Request))
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Rejected manual-drop RequestId collision for %s."),
			*Request.RequestId.ToString());
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Drop,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			Inventory,
			nullptr,
			Request.StackCount,
			Request.RequestId,
			Request.ItemId);
		return true;
	}

	URpgInventoryManagerComponent* CachedTargetInventory =
		CachedResult->TargetInventory.Get();
	// The target is a server-created drop result, not a client-selected
	// inventory. Revalidate only the requested source; the target epoch above
	// remains part of replay integrity without changing completed-result access.
	if (!CanAccessInventory(Inventory))
	{
		SendAndCacheManualDropFeedback(
			Inventory,
			Request,
			ERpgInventoryActionFeedbackResult::NoAccess,
			nullptr,
			Request.StackCount,
			CachedTargetInventory);
		return true;
	}

	URpgInventoryManagerComponent* CachedInventory =
		CachedResult->Inventory.Get();
	SendActionFeedback(
		RpgGameplayTags::Rpg_Inventory_Action_Drop,
		CachedResult->Result,
		CachedInventory,
		CachedResult->FeedbackItem.Get(),
		CachedResult->FeedbackStackCount,
		CachedResult->Request.RequestId,
		CachedResult->Request.ItemId);
	return true;
}

void URpgInventoryUiActionComponent::SendAndCacheManualDropFeedback(
	URpgInventoryManagerComponent* Inventory,
	const FRpgInventoryManualDropRequest& Request,
	ERpgInventoryActionFeedbackResult Result,
	URpgInventoryItemInstance* Item,
	int32 FeedbackStackCount,
	URpgInventoryManagerComponent* TargetInventory)
{
	URpgInventoryItemInstance* AuthorizedFeedbackItem =
		Result == ERpgInventoryActionFeedbackResult::NoAccess
			? nullptr
			: Item;
	if (Request.RequestId.IsValid())
	{
		FRecentManualDropResult CachedResult;
		CachedResult.Inventory = Inventory;
		CachedResult.TargetInventory = TargetInventory;
		CachedResult.bHadInventory = Inventory != nullptr;
		CachedResult.bHadTargetInventory =
			TargetInventory != nullptr;
		CachedResult.InventoryMutationEpoch = Inventory
			? Inventory->GetMutationEpoch()
			: 0;
		CachedResult.TargetMutationEpoch = TargetInventory
			? TargetInventory->GetMutationEpoch()
			: 0;
		CachedResult.Request = Request;
		CachedResult.Result = Result;
		CachedResult.FeedbackItem = AuthorizedFeedbackItem;
		CachedResult.FeedbackStackCount = FeedbackStackCount;
		RecentManualDropResults.Add(Request.RequestId, MoveTemp(CachedResult));
		RecentManualDropOrder.Remove(Request.RequestId);
		RecentManualDropOrder.Add(Request.RequestId);
		while (RecentManualDropOrder.Num() > MaxRecentManualDropResults)
		{
			RecentManualDropResults.Remove(RecentManualDropOrder[0]);
			RecentManualDropOrder.RemoveAt(
				0,
				1,
				EAllowShrinking::No);
		}
	}

	SendActionFeedback(
		RpgGameplayTags::Rpg_Inventory_Action_Drop,
		Result,
		Inventory,
		AuthorizedFeedbackItem,
		FeedbackStackCount,
		Request.RequestId,
		Request.ItemId);
}

void URpgInventoryUiActionComponent::SendActionFeedback(
	FGameplayTag ActionTag,
	ERpgInventoryActionFeedbackResult Result,
	URpgInventoryManagerComponent* Inventory,
	URpgInventoryItemInstance* Item,
	int32 StackCount,
	const FGuid& RequestId,
	FRpgInventoryItemId ItemId) const
{
	FRpgInventoryActionFeedbackMessage Message;
	Message.RequestId = RequestId;
	Message.ItemId = ItemId.IsValid() ? ItemId : (Item ? Item->GetItemId() : FRpgInventoryItemId());
	Message.ActionTag = ActionTag;
	Message.Result = Result;
	Message.InventoryOwner = Inventory;
	// Stable ItemId remains authoritative. A live object may cross the client
	// boundary only while the current caller can still access its owning inventory.
	Message.Item =
		Result != ERpgInventoryActionFeedbackResult::NoAccess &&
		Inventory && Item && CanAccessInventory(Inventory) &&
		Inventory->ContainsItemInstance(Item)
			? Item
			: nullptr;
	Message.StackCount = StackCount;

	const_cast<URpgInventoryUiActionComponent*>(this)->ClientBroadcastInventoryActionFeedback(Message);
}

void URpgInventoryUiActionComponent::ClientBroadcastInventoryActionFeedback_Implementation(const FRpgInventoryActionFeedbackMessage& Message)
{
	if (!GetWorld())
	{
		return;
	}

	APlayerController* LocalRecipient = Cast<APlayerController>(GetOwner());
	if (!ensureMsgf(
			LocalRecipient,
			TEXT("Inventory UI feedback requires a PlayerController-owned action component: %s"),
			*GetNameSafe(this)))
	{
		return;
	}

	FRpgInventoryActionFeedbackMessage LocalMessage = Message;
	LocalMessage.Recipient = LocalRecipient;
	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(GetWorld());
	MessageSubsystem.BroadcastMessage(
		RpgGameplayTags::Rpg_Inventory_Message_ActionFeedback,
		LocalMessage);
}
