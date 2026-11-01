#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"

#include "Animation/AnimMontage.h"
#include "AnimNotify_RpgGameplayEvent.h"
#include "AnimNotify_RpgWeaponAttackWindow.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgAnimNotifyEditorPreviewTest,
	"SurvivalRpg.Combat.AnimNotifies.QuietInEditorPreviews",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgAnimNotifyEditorPreviewTest::RunTest(const FString& Parameters)
{
	// Animation editor previews have no ASC, so gameplay-event notifies placed on a montage must not fire there.
	UAnimMontage* Montage = NewObject<UAnimMontage>(GetTransientPackage(), NAME_None, RF_Transient);
	for (UClass* NotifyClass : {
		UAnimNotify_RpgGameplayEvent::StaticClass(),
		UAnimNotify_RpgWeaponAttackWindowStart::StaticClass(),
		UAnimNotify_RpgWeaponAttackWindowEnd::StaticClass()})
	{
		UAnimNotify* Notify = NewObject<UAnimNotify>(Montage, NotifyClass);
		TestFalse(FString::Printf(TEXT("%s fires in animation editor previews"), *NotifyClass->GetName()),
			Notify->ShouldFireInEditor());
	}
	return true;
}

#endif
