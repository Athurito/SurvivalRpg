#include "RpgRespawnScreenWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgRespawnScreenNativeContractTest,
	"SurvivalRpg.UI.Respawn.NativeLifecycleContract",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgRespawnScreenNativeContractTest::RunTest(
	const FString& Parameters)
{
	URpgRespawnScreenWidget* Defaults =
		GetMutableDefault<URpgRespawnScreenWidget>();
	if (!TestNotNull(TEXT("Native Respawn presenter defaults exist"), Defaults))
	{
		return false;
	}

	TestTrue(
		TEXT("Respawn screen requests menu-only CommonUI input"),
		Defaults->InputConfig == ERpgWidgetInputMode::Menu);
	TestTrue(
		TEXT("Respawn screen participates in CommonUI back handling"),
		Defaults->bIsBackHandler);
	TestTrue(
		TEXT("Respawn screen is marked as a blocking CommonUI modal"),
		Defaults->bIsModal);
	TestTrue(
		TEXT("Back input is consumed while gameplay still owns the death state"),
		Defaults->NativeOnHandleBackAction());
	return true;
}

#endif
