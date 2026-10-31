#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Animation/AnimMontage.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RpgSwingTraceWindow.h"
#include "SurvivalRpg/Animation/AnimNotify_RpgWeaponAttackWindow.h"

namespace RpgSwingTraceWindowTests
{
	using RpgWeaponTraceInterpolation::FBladeSample;

	/** Montage of Length seconds with one section at zero and the given window notifies; negative times are left out. */
	UAnimMontage* MakeWindowMontage(const float Length, const float StartTime, const float EndTime)
	{
		UAnimMontage* Montage = NewObject<UAnimMontage>(GetTransientPackage(), NAME_None, RF_Transient);
		Montage->SetCompositeLength(Length);
		FCompositeSection& Section = Montage->CompositeSections.AddDefaulted_GetRef();
		Section.SectionName = TEXT("Default");
		Section.SetTime(0.0f);

		auto AddNotify = [Montage](UClass* NotifyClass, const float Time)
		{
			FAnimNotifyEvent& Event = Montage->Notifies.AddDefaulted_GetRef();
			Event.Notify = NewObject<UAnimNotify>(Montage, NotifyClass);
			Event.SetTime(Time);
		};
		if (StartTime >= 0.0f)
		{
			AddNotify(UAnimNotify_RpgWeaponAttackWindowStart::StaticClass(), StartTime);
		}
		if (EndTime >= 0.0f)
		{
			AddNotify(UAnimNotify_RpgWeaponAttackWindowEnd::StaticClass(), EndTime);
		}
		return Montage;
	}

	/** Standalone world whose physics scene answers the window's queries. */
	class FScopedTestWorld
	{
	public:
		FScopedTestWorld()
		{
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			if (!GameInstance)
			{
				return;
			}

			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}

		~FScopedTestWorld()
		{
			UWorld* WorldToDestroy = World;
			if (GameInstance)
			{
				GameInstance->Shutdown();
			}
			if (WorldToDestroy)
			{
				GEngine->DestroyWorldContext(WorldToDestroy);
				WorldToDestroy->DestroyWorld(false);
			}
			if (GameInstance)
			{
				GameInstance->RemoveFromRoot();
			}
		}

		UWorld* GetWorld() const { return World; }

	private:
		TObjectPtr<UGameInstance> GameInstance = nullptr;
		TObjectPtr<UWorld> World = nullptr;
	};

	/** Actor with a box of HalfExtent at Location that blocks only the weapon trace channel. */
	AActor* SpawnWeaponTarget(UWorld& World, const FVector& Location, const FVector& HalfExtent)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags = RF_Transient;
		AActor* Actor = World.SpawnActor<AActor>(SpawnParameters);
		if (!Actor)
		{
			return nullptr;
		}

		UBoxComponent* Box = NewObject<UBoxComponent>(Actor, TEXT("Box"), RF_Transient);
		Box->SetBoxExtent(HalfExtent, false);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionResponseToAllChannels(ECR_Ignore);
		Box->SetCollisionResponseToChannel(Rpg_TraceChannel_Weapon, ECR_Block);
		Actor->AddInstanceComponent(Box);
		Actor->SetRootComponent(Box);
		Box->SetWorldLocation(Location);
		Box->RegisterComponent();
		return Actor;
	}

	/** Blade sample with an identity body whose trace points sit at Radii along the X axis turned YawDegrees about Z. */
	FBladeSample MakeTurnedBlade(const double YawDegrees, const TArray<double>& Radii, const double Height, const double SideOffset = 0.0)
	{
		FBladeSample Sample;
		const FQuat Turn(FVector::UpVector, FMath::DegreesToRadians(YawDegrees));
		for (const double Radius : Radii)
		{
			Sample.TracePointTransforms.Add(FTransform(Turn, Turn.RotateVector(FVector(Radius, SideOffset, Height))));
		}
		return Sample;
	}

	/** Everything one traced motion reported to its sink. */
	struct FRecordedTrace
	{
		TArray<FHitResult> Hits;
		TArray<ERpgSwingTraceRole> HitRoles;
		TArray<ERpgRecordedTraceShape> QueryShapes;
		TArray<FVector> QueryExtents;
	};

	int32 TraceMotion(
		UWorld& World,
		const FRpgSwingTraceParams& Params,
		const FBladeSample& Previous,
		const FBladeSample& Current,
		FRecordedTrace& OutRecorded)
	{
		FRpgSwingTraceSink Sink;
		Sink.OnHits.BindLambda([&OutRecorded](const TArray<FHitResult>& HitResults, const ERpgSwingTraceRole Role)
		{
			OutRecorded.Hits.Append(HitResults);
			OutRecorded.HitRoles.Add(Role);
		});
		Sink.OnTraceQuery.BindLambda([&OutRecorded](const FVector&, const FVector&, const ERpgRecordedTraceShape Shape, const FVector& Extent)
		{
			OutRecorded.QueryShapes.Add(Shape);
			OutRecorded.QueryExtents.Add(Extent);
		});

		FRpgSwingTraceWindow Window;
		return Window.TraceSampleMotionForTests(World, Params, Previous, Current, MoveTemp(Sink));
	}

	bool OnlyHits(const FRecordedTrace& Recorded, const AActor* Target)
	{
		return !Recorded.Hits.IsEmpty() && !Recorded.Hits.ContainsByPredicate(
			[Target](const FHitResult& Hit) { return Hit.GetActor() != Target; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgSwingTraceWindowTimingTest,
	"SurvivalRpg.Combat.SwingTrace.WindowTiming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSwingTraceWindowTimingTest::RunTest(const FString& Parameters)
{
	using namespace RpgSwingTraceWindowTests;

	float RemainingPlayTime = 0.0f;
	float BlendOutTriggerSeconds = 0.0f;
	TestFalse(
		TEXT("A 2x montage rejects a window end whose remaining real time overlaps auto blend-out"),
		FRpgSwingTraceWindow::IsWindowEndBeforeAutoBlendOut(1.0f, 0.6f, 2.0f, 0.25f, RemainingPlayTime, BlendOutTriggerSeconds));
	TestEqual(TEXT("The remaining real time divides by the play rate"), RemainingPlayTime, 0.2f, KINDA_SMALL_NUMBER);
	TestTrue(
		TEXT("A 0.5x montage accepts a window end with sufficient remaining real time"),
		FRpgSwingTraceWindow::IsWindowEndBeforeAutoBlendOut(1.0f, 0.8f, 0.5f, 0.25f, RemainingPlayTime, BlendOutTriggerSeconds));
	TestFalse(
		TEXT("A window end past the montage is invalid"),
		FRpgSwingTraceWindow::IsWindowEndBeforeAutoBlendOut(1.0f, 1.2f, 1.0f, 0.25f, RemainingPlayTime, BlendOutTriggerSeconds));
	TestEqual(TEXT("Invalid input reports no remaining time"), RemainingPlayTime, -1.0f);

	float StartTime = 0.0f;
	float EndTime = 0.0f;
	FString Failure;
	UAnimMontage* Valid = MakeWindowMontage(1.0f, 0.3f, 0.5f);
	TestTrue(TEXT("One ordered window pair before blend-out is valid"), FRpgSwingTraceWindow::ResolveWindowTiming(Valid, 1.0f, StartTime, EndTime, Failure));
	TestEqual(TEXT("The window starts at its start notify"), StartTime, 0.3f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("The window ends at its end notify"), EndTime, 0.5f, KINDA_SMALL_NUMBER);
	TestTrue(TEXT("A valid window reports no failure"), Failure.IsEmpty());

	TestFalse(
		TEXT("At 3x the same window ends inside the 0.25 s auto blend-out"),
		FRpgSwingTraceWindow::ResolveWindowTiming(Valid, 3.0f, StartTime, EndTime, Failure));
	TestFalse(TEXT("A rejected window says why"), Failure.IsEmpty());
	Valid->bEnableAutoBlendOut = false;
	TestTrue(
		TEXT("Without auto blend-out the play rate does not limit the window end"),
		FRpgSwingTraceWindow::ResolveWindowTiming(Valid, 3.0f, StartTime, EndTime, Failure));

	TestFalse(TEXT("A missing montage is rejected"), FRpgSwingTraceWindow::ResolveWindowTiming(nullptr, 1.0f, StartTime, EndTime, Failure));
	TestFalse(TEXT("A zero play rate is rejected"), FRpgSwingTraceWindow::ResolveWindowTiming(MakeWindowMontage(1.0f, 0.3f, 0.5f), 0.0f, StartTime, EndTime, Failure));
	TestFalse(TEXT("A start without an end is rejected"), FRpgSwingTraceWindow::ResolveWindowTiming(MakeWindowMontage(1.0f, 0.3f, -1.0f), 1.0f, StartTime, EndTime, Failure));
	TestFalse(TEXT("An end before its start is rejected"), FRpgSwingTraceWindow::ResolveWindowTiming(MakeWindowMontage(1.0f, 0.5f, 0.3f), 1.0f, StartTime, EndTime, Failure));

	UAnimMontage* BranchingStart = MakeWindowMontage(1.0f, 0.3f, 0.5f);
	BranchingStart->Notifies[0].MontageTickType = EMontageNotifyTickType::BranchingPoint;
	TestFalse(TEXT("A branching-point window notify is rejected"), FRpgSwingTraceWindow::ResolveWindowTiming(BranchingStart, 1.0f, StartTime, EndTime, Failure));

	UAnimMontage* ServerSkipped = MakeWindowMontage(1.0f, 0.3f, 0.5f);
	ServerSkipped->Notifies[1].bTriggerOnDedicatedServer = false;
	TestFalse(TEXT("A window notify a dedicated server skips is rejected"), FRpgSwingTraceWindow::ResolveWindowTiming(ServerSkipped, 1.0f, StartTime, EndTime, Failure));

	UAnimMontage* TwoSections = MakeWindowMontage(1.0f, 0.3f, 0.5f);
	FCompositeSection& Second = TwoSections->CompositeSections.AddDefaulted_GetRef();
	Second.SectionName = TEXT("Second");
	Second.SetTime(0.6f);
	TestFalse(TEXT("A montage with a second section is rejected"), FRpgSwingTraceWindow::ResolveWindowTiming(TwoSections, 1.0f, StartTime, EndTime, Failure));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgSwingTraceRibbonHitsTest,
	"SurvivalRpg.Combat.SwingTrace.RibbonHitsBetweenSamples",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSwingTraceRibbonHitsTest::RunTest(const FString& Parameters)
{
	using namespace RpgSwingTraceWindowTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	if (!TestNotNull(TEXT("Test world exists"), World))
	{
		return false;
	}

	// A 20 cm box 100 cm in front of the body, at blade height.
	AActor* Target = SpawnWeaponTarget(*World, FVector(100.0, 0.0, 100.0), FVector(10.0));
	if (!TestNotNull(TEXT("Target exists"), Target))
	{
		return false;
	}

	FRpgSwingTraceParams Params;
	Params.TracePointSockets = {TEXT("Base"), TEXT("Tip")};
	Params.TraceInterpolationDistance = 12.0f;
	const TArray<double> BaseAndTip = {60.0, 140.0};

	// The blade slides sideways across the box: neither sample, base path nor tip path touches it, the ribbon does.
	const FBladeSample LeftOfBox = MakeTurnedBlade(0.0, BaseAndTip, 100.0, -60.0);
	const FBladeSample RightOfBox = MakeTurnedBlade(0.0, BaseAndTip, 100.0, 60.0);
	{
		FRecordedTrace Recorded;
		const int32 Substeps = TraceMotion(*World, Params, LeftOfBox, RightOfBox, Recorded);
		TestEqual(TEXT("A sliding blade that does not turn needs one sub-step"), Substeps, 1);
		TestTrue(TEXT("The line ribbon between base and tip hits the box between the samples"), OnlyHits(Recorded, Target));
		TestFalse(TEXT("Every hit is reported as authority"), Recorded.HitRoles.Contains(ERpgSwingTraceRole::None)
			|| Recorded.HitRoles.Contains(ERpgSwingTraceRole::OwnerFeedback));
		// 80 cm at 12 cm spacing gives 8 points, each traced along its path, plus both blades and their diagonals.
		TestEqual(TEXT("Every ribbon line is reported as a query"), Recorded.QueryShapes.Num(), 12);
		TestFalse(TEXT("A line ribbon records only lines"), Recorded.QueryShapes.ContainsByPredicate(
			[](const ERpgRecordedTraceShape Shape) { return Shape != ERpgRecordedTraceShape::Line; }));
	}
	{
		FRpgSwingTraceParams PathsOnly = Params;
		PathsOnly.bTraceBetweenSockets = false;
		FRecordedTrace Recorded;
		TraceMotion(*World, PathsOnly, LeftOfBox, RightOfBox, Recorded);
		TestTrue(TEXT("Without the ribbon only base and tip trace their paths, which pass beside the box"), Recorded.Hits.IsEmpty());
		TestEqual(TEXT("Without the ribbon each trace point is one query"), Recorded.QueryShapes.Num(), 2);
	}
	{
		FRpgSwingTraceParams Spheres = Params;
		Spheres.TraceMode = ERpgWeaponAttackTraceMode::SphereSweep;
		Spheres.TraceRadius = 6.0f;
		FRecordedTrace Recorded;
		TraceMotion(*World, Spheres, LeftOfBox, RightOfBox, Recorded);
		TestTrue(TEXT("Sphere sweeps along the ribbon points hit the box"), OnlyHits(Recorded, Target));
		TestTrue(TEXT("Sphere sweeps record their radius"), !Recorded.QueryExtents.IsEmpty() && Recorded.QueryExtents[0].X == 6.0);
		TestFalse(TEXT("A sphere ribbon records only spheres"), Recorded.QueryShapes.ContainsByPredicate(
			[](const ERpgRecordedTraceShape Shape) { return Shape != ERpgRecordedTraceShape::Sphere; }));
	}
	{
		FRpgSwingTraceParams OtherChannel = Params;
		OtherChannel.TraceChannel = ECC_Visibility;
		FRecordedTrace Recorded;
		TraceMotion(*World, OtherChannel, LeftOfBox, RightOfBox, Recorded);
		TestTrue(TEXT("Queries run on the configured channel, which the box ignores"), Recorded.Hits.IsEmpty());
	}
	{
		// A 110 degree turn about the body: every straight chord between the samples stays at most 81 cm in front of
		// the body and misses the box at 90..110 cm; the swing arc through it is traced in sub-steps.
		FRecordedTrace Recorded;
		const int32 Substeps = TraceMotion(*World, Params, MakeTurnedBlade(-55.0, BaseAndTip, 100.0), MakeTurnedBlade(55.0, BaseAndTip, 100.0), Recorded);
		TestEqual(TEXT("A 110 degree turn is traced in at most 15 degree sub-steps"), Substeps, 8);
		TestTrue(TEXT("The sub-stepped arc hits the box the chords pass beside"), OnlyHits(Recorded, Target));
	}
	return true;
}

#endif
