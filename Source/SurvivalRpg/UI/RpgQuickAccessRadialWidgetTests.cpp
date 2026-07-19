#if WITH_DEV_AUTOMATION_TESTS

#include "SurvivalRpg/Core/Player/RpgPlayerGameplayInputRouterComponent.h"

#include "Misc/AutomationTest.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgQuickAccessRadialSelectionMathTest,
	"SurvivalRpg.UI.QuickAccessRadial.SelectionMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgQuickAccessRadialSelectionMathTest::RunTest(
	const FString& Parameters)
{
	constexpr float DeadZone = 0.45f;
	constexpr float Diagonal = 0.70710678f;

	struct FDirectionExpectation
	{
		FVector2D Input;
		int32 ExpectedSlot = INDEX_NONE;
	};

	const FDirectionExpectation Directions[] =
	{
		{ FVector2D(0.0f, 1.0f), 0 },
		{ FVector2D(Diagonal, Diagonal), 1 },
		{ FVector2D(1.0f, 0.0f), 2 },
		{ FVector2D(Diagonal, -Diagonal), 3 },
		{ FVector2D(0.0f, -1.0f), 4 },
		{ FVector2D(-Diagonal, -Diagonal), 5 },
		{ FVector2D(-1.0f, 0.0f), 6 },
		{ FVector2D(-Diagonal, Diagonal), 7 }
	};

	for (const FDirectionExpectation& Direction : Directions)
	{
		TestEqual(
			FString::Printf(
				TEXT("Stick %s maps to clockwise slot %d"),
				*Direction.Input.ToString(),
				Direction.ExpectedSlot),
			URpgPlayerGameplayInputRouterComponent::
				ResolveQuickAccessRadialSelection(
					Direction.Input,
					DeadZone),
			Direction.ExpectedSlot);
	}

	TestEqual(
		TEXT("Zero input has no selection"),
		URpgPlayerGameplayInputRouterComponent::
			ResolveQuickAccessRadialSelection(
				FVector2D::ZeroVector,
				DeadZone),
		INDEX_NONE);
	TestEqual(
		TEXT("Zero input remains unselected with a zero dead zone"),
		URpgPlayerGameplayInputRouterComponent::
			ResolveQuickAccessRadialSelection(
				FVector2D::ZeroVector,
				0.0f),
		INDEX_NONE);
	TestEqual(
		TEXT("Input below the dead zone has no selection"),
		URpgPlayerGameplayInputRouterComponent::
			ResolveQuickAccessRadialSelection(
				FVector2D(0.44f, 0.0f),
				DeadZone),
		INDEX_NONE);
	TestEqual(
		TEXT("Input on the dead-zone boundary selects a segment"),
		URpgPlayerGameplayInputRouterComponent::
			ResolveQuickAccessRadialSelection(
				FVector2D(0.45f, 0.0f),
				DeadZone),
		2);
	TestEqual(
		TEXT("A non-finite vector has no selection"),
		URpgPlayerGameplayInputRouterComponent::
			ResolveQuickAccessRadialSelection(
				FVector2D(
					std::numeric_limits<float>::quiet_NaN(),
					1.0f),
				DeadZone),
		INDEX_NONE);
	TestEqual(
		TEXT("Angles just left of up wrap back to segment zero"),
		URpgPlayerGameplayInputRouterComponent::
			ResolveQuickAccessRadialSelection(
				FVector2D(-0.01f, 1.0f),
				DeadZone),
		0);

	return true;
}

#endif
