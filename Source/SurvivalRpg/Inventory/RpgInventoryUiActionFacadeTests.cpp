#include "RpgInventoryUiActionDomainHandlers.h"

#include "Containers/Set.h"
#include "Misc/AutomationTest.h"
#include "Templates/IsConstructible.h"
#include "Templates/UnrealTypeTraits.h"
#include "UObject/UnrealType.h"

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
		FName(TEXT("RequestInventoryMutation")),
		FName(TEXT("RequestMoveInventoryItem")),
		FName(TEXT("RequestTransferInventoryItem")),
		FName(TEXT("RequestExecuteInventoryItemAction")),
		FName(TEXT("RequestApplyInventoryEquipmentIntent")),
		FName(TEXT("RequestQuickTransferItem")),
		FName(TEXT("RequestAssignItemToEquipmentSlot")),
		FName(TEXT("RequestClearEquipmentSlot")),
		FName(TEXT("RequestTransferItemStack")),
		FName(TEXT("RequestTransferItemStackToPlacement")),
		FName(TEXT("RequestApplyInventorySort")),
		FName(TEXT("RequestMoveInventoryEntry")),
		FName(TEXT("RequestMoveInventoryEntryToPlacement")),
		FName(TEXT("RequestMoveItemToInventorySlotAddress")),
		FName(TEXT("RequestEquipSlotContainerItem")),
		FName(TEXT("RequestUnequipSlotContainerItem")),
		FName(TEXT("RequestActivateCarrySlot")),
		FName(TEXT("RequestClearActiveHands")),
		FName(TEXT("RequestMutateQuickAccessBinding")),
		FName(TEXT("RequestBindActionBarToInventorySlot")),
		FName(TEXT("RequestBindActionBarToCarrySlot")),
		FName(TEXT("RequestClearActionBarCarryBinding")),
		FName(TEXT("RequestClearActionBarConsumableBinding")),
		FName(TEXT("RequestSplitItemStack")),
		FName(TEXT("RequestSplitItemStackById")),
		FName(TEXT("RequestUseInventoryItem")),
		FName(TEXT("RequestEquipInventoryItem")),
		FName(TEXT("RequestUnequipInventoryItemToContentSlot")),
		FName(TEXT("RequestDropInventoryItem")),
		FName(TEXT("RequestDropInventoryItemById")),
		FName(TEXT("RequestDepositAllMaterialsToBase")),
		FName(TEXT("RequestDepositMaterialStackToBase")),
		FName(TEXT("RequestWithdrawResourceFromBase")),
		FName(TEXT("RequestStoreItemInstanceInBase")),
		FName(TEXT("RequestTakeItemInstanceFromBase")),
		FName(TEXT("RequestInstallBaseStorageUpgrade")),
		FName(TEXT("RequestApplyBaseResourceSort")),
		FName(TEXT("RequestMoveBaseResourceEntry")),
		FName(TEXT("RequestPlaceBaseBuildable")),
		FName(TEXT("RequestContributeAllToBaseConstructionSite")),
		FName(TEXT("RequestContributeMaterialToBaseConstructionSite")),
		FName(TEXT("RequestCraftRecipe")),
		FName(TEXT("RequestCancelCraftJob")),
		FName(TEXT("RequestPauseCraftingStation")),
		FName(TEXT("RequestResumeCraftingStation")),
		FName(TEXT("RequestSetCraftingOutputAutoDepositEnabled")),
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
		TEXT("The inventory UI action facade retains all Request RPCs"),
		RequestFunctionCount,
		46);
	TestTrue(
		TEXT("Every expected inventory action Request RPC remains reflected"),
		RemainingExpectedRequestFunctions.IsEmpty());

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

#endif // WITH_DEV_AUTOMATION_TESTS
