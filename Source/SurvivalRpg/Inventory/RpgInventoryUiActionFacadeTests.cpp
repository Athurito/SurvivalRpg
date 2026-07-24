#include "RpgInventoryUiActionDomainHandlers.h"

#include "Misc/AutomationTest.h"
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
	}

	TestTrue(
		TEXT("The inventory UI action facade declares Request RPCs"),
		RequestFunctionCount > 0);

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
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
