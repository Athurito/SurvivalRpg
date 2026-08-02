#include "RpgInventoryUiActionDomainHandlers.h"

#include "RpgInventoryItemInstance.h"

#include "Containers/Set.h"
#include "Misc/AutomationTest.h"
#include "Templates/IsConstructible.h"
#include "Templates/UnrealTypeTraits.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

static_assert(
	!TIsDerivedFrom<FRpgBaseStorageActionHandler, UObject>::Value,
	"Inventory UI action domain handlers must remain non-UObject policy types.");
static_assert(
	!TIsDerivedFrom<FRpgBaseBuildingActionHandler, UObject>::Value,
	"Inventory UI action domain handlers must remain non-UObject policy types.");
static_assert(
	!TIsDerivedFrom<FRpgCraftingActionHandler, UObject>::Value,
	"Inventory UI action domain handlers must remain non-UObject policy types.");
static_assert(
	!TIsDerivedFrom<
		FRpgInventoryTransactionActionHandler,
		UObject>::Value,
	"Inventory UI action domain handlers must remain non-UObject policy types.");
static_assert(
	!TIsDerivedFrom<
		FRpgInventoryEquipmentActionHandler,
		UObject>::Value,
	"Inventory UI action domain handlers must remain non-UObject policy types.");
static_assert(
	!TIsDerivedFrom<
		FRpgInventoryQuickAccessActionHandler,
		UObject>::Value,
	"Inventory UI action domain handlers must remain non-UObject policy types.");
static_assert(
	!TIsDerivedFrom<
		FRpgInventoryItemUseActionHandler,
		UObject>::Value,
	"Inventory UI action domain handlers must remain non-UObject policy types.");
static_assert(
	!TIsDerivedFrom<
		FRpgInventoryManualDropActionHandler,
		UObject>::Value,
	"Inventory UI action domain handlers must remain non-UObject policy types.");

#define RPG_ASSERT_COMMAND_HANDLER_CONSTRUCTION(HandlerType) \
	static_assert( \
		TIsConstructible< \
			HandlerType, \
			URpgInventoryUiActionComponent&>::Value, \
		#HandlerType " must accept a mutable inventory UI action component."); \
	static_assert( \
		!TIsConstructible< \
			HandlerType, \
			const URpgInventoryUiActionComponent&>::Value, \
		#HandlerType " must reject the read-only inventory UI action component context.")

RPG_ASSERT_COMMAND_HANDLER_CONSTRUCTION(FRpgBaseStorageActionHandler);
RPG_ASSERT_COMMAND_HANDLER_CONSTRUCTION(FRpgBaseBuildingActionHandler);
RPG_ASSERT_COMMAND_HANDLER_CONSTRUCTION(FRpgCraftingActionHandler);
RPG_ASSERT_COMMAND_HANDLER_CONSTRUCTION(
	FRpgInventoryTransactionActionHandler);
RPG_ASSERT_COMMAND_HANDLER_CONSTRUCTION(
	FRpgInventoryEquipmentActionHandler);
RPG_ASSERT_COMMAND_HANDLER_CONSTRUCTION(
	FRpgInventoryQuickAccessActionHandler);
RPG_ASSERT_COMMAND_HANDLER_CONSTRUCTION(
	FRpgInventoryItemUseActionHandler);
RPG_ASSERT_COMMAND_HANDLER_CONSTRUCTION(
	FRpgInventoryManualDropActionHandler);

#undef RPG_ASSERT_COMMAND_HANDLER_CONSTRUCTION

static_assert(
	TIsConstructible<
		FRpgInventoryTransactionQueryHandler,
		const URpgInventoryUiActionComponent&>::Value,
	"Inventory transaction queries must accept the read-only facade context.");
static_assert(
	TIsConstructible<
		FRpgInventoryEquipmentQueryHandler,
		const URpgInventoryUiActionComponent&>::Value,
	"Inventory equipment queries must accept the read-only facade context.");

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryUiActionFacadeRpcOwnershipContractTest,
	"SurvivalRpg.Inventory.UiActions.Facade.RpcOwnershipContract",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryUiActionFacadeRpcOwnershipContractTest::RunTest(
	const FString& Parameters)
{
	const UClass* ActionComponentClass =
		URpgInventoryUiActionComponent::StaticClass();
	TSet<FName> RemainingExpectedRequestFunctions = {
		FName(TEXT("RequestMoveInventoryItem")),
		FName(TEXT("RequestTransferInventoryItem")),
		FName(TEXT("RequestUseInventoryItemById")),
		FName(TEXT("RequestApplyInventoryEquipmentIntent")),
		FName(TEXT("RequestQuickTransferItem")),
		FName(TEXT("RequestActivateCarrySlot")),
		FName(TEXT("RequestClearActiveHands")),
		FName(TEXT("RequestMutateQuickAccessBinding")),
		FName(TEXT("RequestSplitItemStackById")),
		FName(TEXT("RequestDropInventoryItemById")),
		FName(TEXT("RequestSmartDepositToBase")),
		FName(TEXT("RequestDepositItemToBase")),
		FName(TEXT("RequestWithdrawFromBase")),
		FName(TEXT("RequestInstallBaseStorageUpgradeById")),
		FName(TEXT("RequestDecommissionBaseStorageUpgrade")),
		FName(TEXT("RequestStabilizeContainedItem")),
		FName(TEXT("RequestExtractContainedItem")),
		FName(TEXT("RequestCleanseBaseStorageRiftStrain")),
		FName(TEXT("RequestPlaceBaseBuildable")),
		FName(TEXT("RequestContributeAllToBaseConstructionSite")),
		FName(TEXT("RequestContributeMaterialToBaseConstructionSite")),
		FName(TEXT("RequestCraftRecipe")),
		FName(TEXT("RequestCancelCraftJob")),
		FName(TEXT("RequestPauseCraftingStation")),
		FName(TEXT("RequestResumeCraftingStation")),
		FName(TEXT("RequestSetCraftingOutputAutoDepositEnabled")),
	};
	static const FName RetiredLegacyRequestFunctions[] = {
		TEXT("RequestInventoryMutation"),
		TEXT("RequestAssignItemToEquipmentSlot"),
		TEXT("RequestClearEquipmentSlot"),
		TEXT("RequestTransferItemStack"),
		TEXT("RequestTransferItemStackToPlacement"),
		TEXT("RequestMoveItemToInventorySlotAddress"),
		TEXT("RequestEquipSlotContainerItem"),
		TEXT("RequestUnequipSlotContainerItem"),
		TEXT("RequestBindActionBarToInventorySlot"),
		TEXT("RequestBindActionBarToCarrySlot"),
		TEXT("RequestClearActionBarCarryBinding"),
		TEXT("RequestClearActionBarConsumableBinding"),
		TEXT("RequestSplitItemStack"),
		TEXT("RequestEquipInventoryItem"),
		TEXT("RequestUnequipInventoryItemToContentSlot"),
		TEXT("RequestDropInventoryItem"),
		TEXT("RequestStoreItemInstanceInBase"),
		TEXT("RequestTakeItemInstanceFromBase"),
		TEXT("RequestDepositAllMaterialsToBase"),
		TEXT("RequestDepositMaterialStackToBase"),
		TEXT("RequestWithdrawResourceFromBase"),
		TEXT("RequestInstallBaseStorageUpgrade"),
		TEXT("RequestApplyBaseResourceSort"),
		TEXT("RequestMoveBaseResourceEntry"),
	};
	int32 RequestFunctionCount = 0;
	for (TFieldIterator<UFunction> FunctionIt(
			 ActionComponentClass,
			 EFieldIteratorFlags::ExcludeSuper);
		 FunctionIt;
		 ++FunctionIt)
	{
		const UFunction* Function = *FunctionIt;
		if (!Function ||
			!Function->GetName().StartsWith(TEXT("Request")))
		{
			continue;
		}

		++RequestFunctionCount;
		const FString FunctionLabel = Function->GetName();
		TestTrue(
			*FString::Printf(
				TEXT("%s is an expected inventory action Request RPC"),
				*FunctionLabel),
			RemainingExpectedRequestFunctions.Remove(
				Function->GetFName()) > 0);
		TestTrue(
			*FString::Printf(
				TEXT("%s remains a Server RPC"),
				*FunctionLabel),
			Function->HasAnyFunctionFlags(FUNC_NetServer));
		TestTrue(
			*FString::Printf(
				TEXT("%s remains Reliable"),
				*FunctionLabel),
			Function->HasAnyFunctionFlags(FUNC_NetReliable));
		TestFalse(
			*FString::Printf(
				TEXT("%s is never a Client RPC"),
				*FunctionLabel),
			Function->HasAnyFunctionFlags(FUNC_NetClient));
		TestFalse(
			*FString::Printf(
				TEXT("%s is never a Multicast RPC"),
				*FunctionLabel),
			Function->HasAnyFunctionFlags(FUNC_NetMulticast));
		TestTrue(
			*FString::Printf(
				TEXT("%s remains BlueprintCallable"),
				*FunctionLabel),
			Function->HasAnyFunctionFlags(FUNC_BlueprintCallable));
	}

	TestEqual(
		TEXT("The inventory UI action facade retains exactly its canonical and feature Request RPCs"),
		RequestFunctionCount,
		26);
	TestTrue(
		TEXT("Every canonical or feature inventory action Request RPC remains reflected"),
		RemainingExpectedRequestFunctions.IsEmpty());
	for (const FName FunctionName : RetiredLegacyRequestFunctions)
	{
		TestNull(
			*FString::Printf(
				TEXT("The retired inventory action RPC %s is absent"),
				*FunctionName.ToString()),
			ActionComponentClass->FindFunctionByName(FunctionName));
	}
	TestNull(
		TEXT("The legacy entry-id move RPC is retired"),
		ActionComponentClass->FindFunctionByName(
			TEXT("RequestMoveInventoryEntryToPlacement")));
	TestNull(
		TEXT("The generic item-action RPC is retired"),
		ActionComponentClass->FindFunctionByName(
			TEXT("RequestExecuteInventoryItemAction")));
	TestNull(
		TEXT("The pointer-based item-use RPC is retired"),
		ActionComponentClass->FindFunctionByName(
			TEXT("RequestUseInventoryItem")));
	TestNotNull(
		TEXT("The canonical Use request exposes RequestId"),
		FRpgInventoryUseRequest::StaticStruct()->FindPropertyByName(
			TEXT("RequestId")));
	TestNotNull(
		TEXT("The canonical Use request exposes ItemId"),
		FRpgInventoryUseRequest::StaticStruct()->FindPropertyByName(
			TEXT("ItemId")));
	TestNotNull(
		TEXT("The canonical Use request exposes UseCount"),
		FRpgInventoryUseRequest::StaticStruct()->FindPropertyByName(
			TEXT("UseCount")));
	static const FName SplitRequestProperties[] = {
		TEXT("RequestId"),
		TEXT("ItemId"),
		TEXT("ExpectedEntryId"),
		TEXT("ExpectedSourcePlacement"),
		TEXT("ExpectedSourceQuantity"),
		TEXT("SplitCount"),
		TEXT("TargetPlacement"),
	};
	for (const FName PropertyName : SplitRequestProperties)
	{
		TestNotNull(
			*FString::Printf(
				TEXT("The canonical Split request exposes %s"),
				*PropertyName.ToString()),
			FRpgInventorySplitRequest::StaticStruct()
				->FindPropertyByName(PropertyName));
	}
	struct FCanonicalStructRequestSignature
	{
		const TCHAR* FunctionName;
		const TCHAR* ParameterName;
		const UScriptStruct* RequestStruct;
	};
	static const FCanonicalStructRequestSignature
		CanonicalStructRequestSignatures[] = {
			{
				TEXT("RequestMoveInventoryItem"),
				TEXT("Intent"),
				FRpgInventoryMoveIntent::StaticStruct(),
			},
			{
				TEXT("RequestTransferInventoryItem"),
				TEXT("Intent"),
				FRpgInventoryTransferIntent::StaticStruct(),
			},
			{
				TEXT("RequestUseInventoryItemById"),
				TEXT("Request"),
				FRpgInventoryUseRequest::StaticStruct(),
			},
			{
				TEXT("RequestApplyInventoryEquipmentIntent"),
				TEXT("Intent"),
				FRpgInventoryEquipmentIntent::StaticStruct(),
			},
			{
				TEXT("RequestQuickTransferItem"),
				TEXT("Request"),
				FRpgInventoryQuickTransferRequest::StaticStruct(),
			},
			{
				TEXT("RequestSplitItemStackById"),
				TEXT("Request"),
				FRpgInventorySplitRequest::StaticStruct(),
			},
			{
				TEXT("RequestMutateQuickAccessBinding"),
				TEXT("Request"),
				FRpgQuickAccessMutationRequest::StaticStruct(),
			},
			{
				TEXT("RequestDropInventoryItemById"),
				TEXT("Request"),
				FRpgInventoryManualDropRequest::StaticStruct(),
			},
		};
	for (const FCanonicalStructRequestSignature& Signature :
		CanonicalStructRequestSignatures)
	{
		const UFunction* Function =
			ActionComponentClass->FindFunctionByName(
				Signature.FunctionName);
		const FStructProperty* RequestProperty = Function
			? FindFProperty<FStructProperty>(
				Function,
				Signature.ParameterName)
			: nullptr;
		TestTrue(
			*FString::Printf(
				TEXT("The canonical %s RPC accepts %s through %s"),
				Signature.FunctionName,
				*Signature.RequestStruct->GetName(),
				Signature.ParameterName),
			RequestProperty &&
				RequestProperty->Struct == Signature.RequestStruct);
	}
	static const FName RetiredTransferPreviewFunctions[] = {
		TEXT("CanTransferItemStack"),
		TEXT("CanTransferItemStackToPlacement"),
	};
	for (const FName FunctionName : RetiredTransferPreviewFunctions)
	{
		TestNull(
			*FString::Printf(
				TEXT("The legacy %s pointer preview is retired"),
				*FunctionName.ToString()),
			ActionComponentClass->FindFunctionByName(FunctionName));
	}

	const UFunction* FeedbackFunction =
		ActionComponentClass->FindFunctionByName(
			FName(TEXT("ClientBroadcastInventoryActionFeedback")));
	if (!TestNotNull(
			TEXT("The owning-client feedback RPC remains reflected"),
			FeedbackFunction))
	{
		return false;
	}

	TestTrue(
		TEXT("Inventory action feedback remains a Client RPC"),
		FeedbackFunction->HasAnyFunctionFlags(FUNC_NetClient));
	TestTrue(
		TEXT("Inventory action feedback remains Reliable"),
		FeedbackFunction->HasAnyFunctionFlags(FUNC_NetReliable));
	TestFalse(
		TEXT("Inventory action feedback is never a Server RPC"),
		FeedbackFunction->HasAnyFunctionFlags(FUNC_NetServer));
	TestFalse(
		TEXT("Inventory action feedback is never a Multicast RPC"),
		FeedbackFunction->HasAnyFunctionFlags(FUNC_NetMulticast));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryUseRequestReplayCacheContractTest,
	"SurvivalRpg.Inventory.UiActions.Use.ReplayCacheContract",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryUseRequestReplayCacheContractTest::RunTest(
	const FString& Parameters)
{
	URpgInventoryUiActionComponent* ActionComponent =
		NewObject<URpgInventoryUiActionComponent>();
	URpgInventoryManagerComponent* Inventory =
		NewObject<URpgInventoryManagerComponent>();
	if (!TestNotNull(TEXT("The Use facade fixture exists"), ActionComponent) ||
		!TestNotNull(TEXT("The Use inventory fixture exists"), Inventory))
	{
		return false;
	}

	FRpgInventoryUseRequest Request;
	Request.RequestId = FGuid::NewGuid();
	Request.ItemId = FRpgInventoryItemId::NewId();
	Request.UseCount = 2;
	const auto FirstAdmission =
		ActionComponent->AdmitUseRequest(Inventory, Request);
	TestTrue(
		TEXT("A new stable Use request is admitted exactly once"),
		FirstAdmission.Disposition ==
			URpgInventoryUiActionComponent::
				EUseRequestAdmissionDisposition::Execute);

	const auto InFlightRetry =
		ActionComponent->AdmitUseRequest(Inventory, Request);
	TestTrue(
		TEXT("An identical in-flight retry is silent"),
		InFlightRetry.Disposition ==
			URpgInventoryUiActionComponent::
				EUseRequestAdmissionDisposition::InFlight);

	URpgInventoryItemInstance* AuthorizedFeedbackItem =
		NewObject<URpgInventoryItemInstance>();
	ActionComponent->FinalizeUseRequest(
		Inventory,
		Request,
		ERpgInventoryActionFeedbackResult::Success,
		AuthorizedFeedbackItem,
		1);
	const auto CompletedRetry =
		ActionComponent->AdmitUseRequest(Inventory, Request);
	TestTrue(
		TEXT("An identical completed retry replays without execution"),
		CompletedRetry.Disposition ==
			URpgInventoryUiActionComponent::
				EUseRequestAdmissionDisposition::Replay);
	TestEqual(
		TEXT("An ownerless replay fails closed after access revalidation"),
		CompletedRetry.Result,
		ERpgInventoryActionFeedbackResult::NoAccess);
	TestEqual(
		TEXT("The redacted replay retains the requested feedback quantity"),
		CompletedRetry.FeedbackUseCount,
		Request.UseCount);
	TestNull(
		TEXT("Access revalidation redacts the previously cached item"),
		CompletedRetry.FeedbackItem.Get());

	FRpgInventoryUseRequest Collision = Request;
	++Collision.UseCount;
	const auto CollisionAdmission =
		ActionComponent->AdmitUseRequest(Inventory, Collision);
	TestTrue(
		TEXT("A RequestId collision with a different fingerprint is rejected"),
		CollisionAdmission.Disposition ==
			URpgInventoryUiActionComponent::
				EUseRequestAdmissionDisposition::Reject);
	TestEqual(
		TEXT("A RequestId collision maps to InvalidRequest feedback"),
		CollisionAdmission.Result,
		ERpgInventoryActionFeedbackResult::InvalidRequest);
	TestNull(
		TEXT("A rejected Use admission never authorizes an item pointer"),
		CollisionAdmission.FeedbackItem.Get());
	URpgInventoryManagerComponent* OtherInventory =
		NewObject<URpgInventoryManagerComponent>();
	const auto InventoryIdentityCollision =
		ActionComponent->AdmitUseRequest(OtherInventory, Request);
	TestTrue(
		TEXT("The weak inventory identity participates in the fingerprint"),
		InventoryIdentityCollision.Disposition ==
			URpgInventoryUiActionComponent::
				EUseRequestAdmissionDisposition::Reject);

	FRpgInventoryUseRequest InvalidRequest = Request;
	InvalidRequest.RequestId.Invalidate();
	const auto InvalidAdmission =
		ActionComponent->AdmitUseRequest(Inventory, InvalidRequest);
	TestTrue(
		TEXT("An invalid caller RequestId is rejected before execution"),
		InvalidAdmission.Disposition ==
			URpgInventoryUiActionComponent::
				EUseRequestAdmissionDisposition::Reject);

	FRpgInventoryUseRequest RestoreRequest;
	RestoreRequest.RequestId = FGuid::NewGuid();
	RestoreRequest.ItemId = FRpgInventoryItemId::NewId();
	RestoreRequest.UseCount = 1;
	ActionComponent->AdmitUseRequest(Inventory, RestoreRequest);
	ActionComponent->FinalizeUseRequest(
		Inventory,
		RestoreRequest,
		ERpgInventoryActionFeedbackResult::CannotUse,
		nullptr,
		1);
	if (URpgInventoryUiActionComponent::FRecentUseRequestResult*
		RestoreRecord = ActionComponent->RecentUseRequestResults.Find(
			RestoreRequest.RequestId))
	{
		++RestoreRecord->InventoryMutationEpoch;
	}
	const auto PostRestoreAdmission =
		ActionComponent->AdmitUseRequest(Inventory, RestoreRequest);
	TestTrue(
		TEXT("A changed restore epoch invalidates a completed replay record"),
		PostRestoreAdmission.Disposition ==
			URpgInventoryUiActionComponent::
				EUseRequestAdmissionDisposition::Execute);
	ActionComponent->FinalizeUseRequest(
		Inventory,
		RestoreRequest,
		ERpgInventoryActionFeedbackResult::CannotUse,
		nullptr,
		1);

	FRpgInventoryUseRequest RestoreDuringFlightRequest;
	RestoreDuringFlightRequest.RequestId = FGuid::NewGuid();
	RestoreDuringFlightRequest.ItemId = FRpgInventoryItemId::NewId();
	RestoreDuringFlightRequest.UseCount = 1;
	ActionComponent->AdmitUseRequest(
		Inventory,
		RestoreDuringFlightRequest);
	if (URpgInventoryUiActionComponent::FRecentUseRequestResult*
		InFlightRestoreRecord =
			ActionComponent->RecentUseRequestResults.Find(
				RestoreDuringFlightRequest.RequestId))
	{
		++InFlightRestoreRecord->InventoryMutationEpoch;
	}
	const auto RestoreDuringFlightAdmission =
		ActionComponent->AdmitUseRequest(
			Inventory,
			RestoreDuringFlightRequest);
	TestTrue(
		TEXT("A restore-epoch collision cannot evict an in-flight request"),
		RestoreDuringFlightAdmission.Disposition ==
			URpgInventoryUiActionComponent::
				EUseRequestAdmissionDisposition::Reject);
	TestTrue(
		TEXT("The restore-epoch collision leaves the in-flight guard installed"),
		ActionComponent->RecentUseRequestResults.Contains(
			RestoreDuringFlightRequest.RequestId) &&
		ActionComponent->RecentUseRequestResults[
			RestoreDuringFlightRequest.RequestId].bInFlight);

	FRpgInventoryUseRequest DestroyedInventoryRequest;
	DestroyedInventoryRequest.RequestId = FGuid::NewGuid();
	DestroyedInventoryRequest.ItemId = FRpgInventoryItemId::NewId();
	DestroyedInventoryRequest.UseCount = 1;
	ActionComponent->AdmitUseRequest(
		Inventory,
		DestroyedInventoryRequest);
	ActionComponent->FinalizeUseRequest(
		Inventory,
		DestroyedInventoryRequest,
		ERpgInventoryActionFeedbackResult::Success,
		nullptr,
		1);
	if (URpgInventoryUiActionComponent::FRecentUseRequestResult*
		DestroyedRecord = ActionComponent->RecentUseRequestResults.Find(
			DestroyedInventoryRequest.RequestId))
	{
		DestroyedRecord->Inventory.Reset();
	}
	const auto DestroyedInventoryAdmission =
		ActionComponent->AdmitUseRequest(
			Inventory,
			DestroyedInventoryRequest);
	TestTrue(
		TEXT("A destroyed cached inventory fails closed"),
		DestroyedInventoryAdmission.Disposition ==
			URpgInventoryUiActionComponent::
				EUseRequestAdmissionDisposition::Reject);

	ActionComponent->RecentUseRequestResults.Reset();
	ActionComponent->RecentUseRequestOrder.Reset();
	TArray<FRpgInventoryUseRequest> InFlightRequests;
	InFlightRequests.Reserve(
		URpgInventoryUiActionComponent::MaxRecentUseRequestResults);
	for (int32 Index = 0;
		Index < URpgInventoryUiActionComponent::
			MaxRecentUseRequestResults;
		++Index)
	{
		FRpgInventoryUseRequest InFlightRequest;
		InFlightRequest.RequestId = FGuid::NewGuid();
		InFlightRequest.ItemId = FRpgInventoryItemId::NewId();
		InFlightRequest.UseCount = 1;
		const auto Admission = ActionComponent->AdmitUseRequest(
			Inventory,
			InFlightRequest);
		TestTrue(
			TEXT("Each request inside the replay bound is admitted"),
			Admission.Disposition ==
				URpgInventoryUiActionComponent::
					EUseRequestAdmissionDisposition::Execute);
		InFlightRequests.Add(InFlightRequest);
	}

	FRpgInventoryUseRequest OverflowRequest;
	OverflowRequest.RequestId = FGuid::NewGuid();
	OverflowRequest.ItemId = FRpgInventoryItemId::NewId();
	OverflowRequest.UseCount = 1;
	const auto OverflowAdmission =
		ActionComponent->AdmitUseRequest(Inventory, OverflowRequest);
	TestTrue(
		TEXT("A full cache never evicts an in-flight request"),
		OverflowAdmission.Disposition ==
			URpgInventoryUiActionComponent::
				EUseRequestAdmissionDisposition::Reject);
	TestEqual(
		TEXT("The Use replay cache remains bounded"),
		ActionComponent->RecentUseRequestResults.Num(),
		URpgInventoryUiActionComponent::MaxRecentUseRequestResults);

	ActionComponent->FinalizeUseRequest(
		Inventory,
		InFlightRequests[0],
		ERpgInventoryActionFeedbackResult::CannotUse,
		nullptr,
		1);
	const auto AdmissionAfterCompletion =
		ActionComponent->AdmitUseRequest(Inventory, OverflowRequest);
	TestTrue(
		TEXT("The oldest completed result can make room for a new request"),
		AdmissionAfterCompletion.Disposition ==
			URpgInventoryUiActionComponent::
				EUseRequestAdmissionDisposition::Execute);
	TestFalse(
		TEXT("Capacity trimming does not retain the evicted completed request"),
		ActionComponent->RecentUseRequestResults.Contains(
			InFlightRequests[0].RequestId));
	TestEqual(
		TEXT("Capacity trimming keeps the bounded size"),
		ActionComponent->RecentUseRequestResults.Num(),
		URpgInventoryUiActionComponent::MaxRecentUseRequestResults);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
