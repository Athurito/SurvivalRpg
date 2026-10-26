#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "GameplayEffect.h"
#include "RpgInventoryFragment_WeaponRunes.h"
#include "RpgWeaponRuneDefinition.h"
#include "RpgWeaponSwingRecord.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgEchoStrikeChargeTest,
	"SurvivalRpg.Combat.Runes.Echo.ChargeCountsOnlyHittingSwings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgEchoStrikeChargeTest::RunTest(const FString& Parameters)
{
	FRpgEchoStrikeCharge Charge;
	TestFalse(TEXT("A swing into the air does not trigger"), Charge.RegisterSwing(false, 3, 5.0f, 0.0f));
	TestEqual(TEXT("A swing into the air adds no charge"), Charge.Charge, 0);
	TestFalse(TEXT("The first hit does not trigger"), Charge.RegisterSwing(true, 3, 5.0f, 1.0f));
	TestFalse(TEXT("A miss between hits does not trigger"), Charge.RegisterSwing(false, 3, 5.0f, 1.5f));
	TestEqual(TEXT("A miss between hits keeps the charge"), Charge.Charge, 1);
	TestFalse(TEXT("The second hit does not trigger"), Charge.RegisterSwing(true, 3, 5.0f, 2.0f));
	TestTrue(TEXT("The third hit triggers"), Charge.RegisterSwing(true, 3, 5.0f, 3.0f));
	TestEqual(TEXT("The trigger empties the charge"), Charge.Charge, 0);
	TestFalse(TEXT("The hit after a trigger starts a new count"), Charge.RegisterSwing(true, 3, 5.0f, 3.5f));
	TestEqual(TEXT("The hit after a trigger counts once"), Charge.Charge, 1);

	FRpgEchoStrikeCharge Single;
	TestTrue(TEXT("A trigger count of one fires on every hit"), Single.RegisterSwing(true, 1, 5.0f, 0.0f));
	TestTrue(TEXT("A trigger count below one behaves as one"), Single.RegisterSwing(true, 0, 5.0f, 0.5f));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgEchoStrikeChargeTimeoutTest,
	"SurvivalRpg.Combat.Runes.Echo.ChargeExpiresAfterTheTimeout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgEchoStrikeChargeTimeoutTest::RunTest(const FString& Parameters)
{
	FRpgEchoStrikeCharge Charge;
	Charge.RegisterSwing(true, 3, 5.0f, 0.0f);
	Charge.RegisterSwing(true, 3, 5.0f, 1.0f);
	TestFalse(TEXT("A charge within the timeout stays"), Charge.Expire(5.0f, 6.0f));
	TestEqual(TEXT("A charge within the timeout keeps its hits"), Charge.Charge, 2);
	TestTrue(TEXT("A charge older than the timeout expires"), Charge.Expire(5.0f, 6.5f));
	TestEqual(TEXT("An expired charge is empty"), Charge.Charge, 0);
	TestFalse(TEXT("An empty charge does not expire again"), Charge.Expire(5.0f, 20.0f));

	Charge.RegisterSwing(true, 3, 5.0f, 10.0f);
	Charge.RegisterSwing(true, 3, 5.0f, 11.0f);
	TestFalse(TEXT("A hit after the timeout starts over instead of triggering"), Charge.RegisterSwing(true, 3, 5.0f, 17.0f));
	TestEqual(TEXT("The late hit is the only charge"), Charge.Charge, 1);
	Charge.RegisterSwing(false, 3, 5.0f, 23.0f);
	TestEqual(TEXT("A late miss also expires the charge"), Charge.Charge, 0);

	FRpgEchoStrikeCharge Kept;
	Kept.RegisterSwing(true, 3, 0.0f, 0.0f);
	TestFalse(TEXT("A zero timeout never expires"), Kept.Expire(0.0f, 1000.0f));
	TestEqual(TEXT("A zero timeout keeps the charge"), Kept.Charge, 1);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgWeaponSwingReplayScheduleTest,
	"SurvivalRpg.Combat.Runes.Echo.ReplayRunsTracesAtTheirOffsets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgWeaponSwingReplayScheduleTest::RunTest(const FString& Parameters)
{
	FRpgWeaponSwingRecord Swing;
	Swing.WindowStartTime = 10.0f;
	Swing.WindowEndTime = 10.2f;
	for (const float Offset : { 0.0f, 0.05f, 0.05f, 0.12f, 0.2f })
	{
		Swing.Traces.AddDefaulted_GetRef().TimeOffset = Offset;
	}

	TestTrue(TEXT("The duration is the window length"), FMath::IsNearlyEqual(Swing.GetDuration(), 0.2f, KINDA_SMALL_NUMBER));
	TestEqual(TEXT("Before the replay starts nothing is due"), Swing.FindDueEnd(0, -0.01f), 0);
	TestEqual(TEXT("At the start the first trace is due"), Swing.FindDueEnd(0, 0.0f), 1);
	TestEqual(TEXT("Traces at the same offset are due together"), Swing.FindDueEnd(1, 0.06f), 3);
	TestEqual(TEXT("A frame without a due trace advances nothing"), Swing.FindDueEnd(3, 0.1f), 3);
	TestEqual(TEXT("A long frame runs every trace it passed"), Swing.FindDueEnd(3, 0.5f), 5);
	TestEqual(TEXT("A finished replay stays finished"), Swing.FindDueEnd(5, 1.0f), 5);
	TestEqual(TEXT("A negative start index is clamped"), Swing.FindDueEnd(-2, 0.0f), 1);
	TestEqual(TEXT("A start index past the end is clamped"), Swing.FindDueEnd(9, 1.0f), 5);

	FRpgWeaponSwingRecord Inverted;
	Inverted.WindowStartTime = 2.0f;
	Inverted.WindowEndTime = 1.0f;
	TestEqual(TEXT("An inverted window has no duration"), Inverted.GetDuration(), 0.0f);
	return !HasAnyErrors();
}

#if WITH_EDITOR
#include "Misc/DataValidation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgWeaponRuneEchoValidationTest,
	"SurvivalRpg.Combat.Runes.Echo.DefinitionNeedsEffectAndDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgWeaponRuneEchoValidationTest::RunTest(const FString& Parameters)
{
	URpgWeaponRuneDefinition* Rune = NewObject<URpgWeaponRuneDefinition>();
	{
		FDataValidationContext Context;
		TestTrue(TEXT("A rune without Echo needs nothing"), Rune->IsDataValid(Context) != EDataValidationResult::Invalid);
	}

	Rune->EchoStrike.bEnabled = true;
	{
		FDataValidationContext Context;
		TestTrue(TEXT("Echo without a damage effect is invalid"), Rune->IsDataValid(Context) == EDataValidationResult::Invalid);
	}

	Rune->EchoStrike.DamageEffect = UGameplayEffect::StaticClass();
	Rune->EchoStrike.Damage = 0.0f;
	Rune->EchoStrike.StaggerDamage = 0.0f;
	{
		FDataValidationContext Context;
		TestTrue(TEXT("Echo without damage is invalid"), Rune->IsDataValid(Context) == EDataValidationResult::Invalid);
	}

	Rune->EchoStrike.Damage = 25.0f;
	{
		FDataValidationContext Context;
		TestTrue(TEXT("Echo with an effect and damage is valid"), Rune->IsDataValid(Context) != EDataValidationResult::Invalid);
	}

	URpgWeaponRuneDefinition* Plain = NewObject<URpgWeaponRuneDefinition>();
	URpgInventoryFragment_WeaponRunes* Fragment = NewObject<URpgInventoryFragment_WeaponRunes>();
	TestNull(TEXT("An empty rune list has no Echo rune"), Fragment->FindEchoRune());
	Fragment->Runes = { nullptr, Plain, Rune };
	TestTrue(TEXT("The Echo rune is the first with Echo enabled"), Fragment->FindEchoRune() == Rune);
	return !HasAnyErrors();
}
#endif

#endif
