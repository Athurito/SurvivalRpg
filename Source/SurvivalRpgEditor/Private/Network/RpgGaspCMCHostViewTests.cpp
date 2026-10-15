// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Network/RpgCombatNetworkTestTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Misc/Guid.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMovementComponent.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceManagerComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Development/RpgDeveloperSettings.h"

#if ENABLE_PIE_NETWORK_TEST
namespace RpgGaspCMCHostViewTests
{
	// The mantle Experience spawns the CMC GASP pawn armed, so it stands in the combat stance and faces the camera.
	constexpr TCHAR GameModePath[] = TEXT("/Game/SurvivalRpg/Maps/Test/GaspMantle/BP_Rpg_GaspMantleTestGameMode.BP_Rpg_GaspMantleTestGameMode_C");
	FPrimaryAssetId ExperienceId() { return FPrimaryAssetId(URpgExperienceDefinition::StaticClass()->GetFName(), TEXT("RpgGaspMantleExperience")); }
	FTimespan Timeout() { return FTimespan::FromSeconds(60.0); }
	constexpr double SettleSeconds = .25, WindowSeconds = 1.5;
	// Frame-to-frame yaw changes below this are noise, not a visible change of direction.
	constexpr double FlipThreshold = .05;

	bool ActiveWorld(const UWorld* World)
	{
		if (!GEngine || !World) return false;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE && Context.World() == World)
				return IsValid(World) && !World->bIsTearingDown && !World->IsBeingCleanedUp();
		return false;
	}
	ARpgCharacter* LocalCharacter(UWorld* World)
	{
		const APlayerController* Controller = ActiveWorld(World) ? World->GetFirstPlayerController() : nullptr;
		return Controller ? Cast<ARpgCharacter>(Controller->GetPawn()) : nullptr;
	}
	ARpgCharacter* FindCharacter(UWorld* World, int32 PlayerId)
	{
		if (!ActiveWorld(World) || PlayerId == INDEX_NONE) return nullptr;
		for (TActorIterator<ARpgCharacter> It(World); It; ++It)
			if (It->GetPlayerState() && It->GetPlayerState()->GetPlayerId() == PlayerId) return *It;
		return nullptr;
	}
	URpgCharacterMovementComponent* Movement(const ARpgCharacter* Character)
	{
		return Character ? Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	}
	/** The Experience, pawn and RPG animation base are live, and the pawn stands armed on the floor. */
	bool ReadyInStance(UWorld* World, const ARpgCharacter* Character)
	{
		const AGameStateBase* GameState = ActiveWorld(World) ? World->GetGameState() : nullptr;
		const URpgExperienceManagerComponent* Manager = GameState ? GameState->FindComponentByClass<URpgExperienceManagerComponent>() : nullptr;
		const UAnimInstance* Animation = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;
		return World->GetNetDriver() && Manager && Manager->IsExperienceLoaded()
			&& Manager->GetCurrentExperienceChecked()->GetPrimaryAssetId() == ExperienceId()
			&& Character && Character->GetPlayerState() && Animation && Animation->IsA<URpgAnimInstance>()
			&& Movement(Character) && Movement(Character)->IsMovingOnGround() && Movement(Character)->IsInCombatStance()
			&& Character->GetVelocity().Size2D() < 5.0;
	}

	class FScopedSaveIsolation final
	{
	public:
		~FScopedSaveIsolation() { FGameModeEvents::OnGameModeInitializedEvent().Remove(Handle); }
		void Start()
		{
			Prefix = TEXT("SurvivalRpg_GaspCMCHostViewAutomation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
			Handle = FGameModeEvents::OnGameModeInitializedEvent().AddRaw(this, &FScopedSaveIsolation::OnInitialized);
		}
		bool IsIsolated(UWorld* World) const
		{
			if (!ActiveWorld(World)) return false;
			if (World->GetNetMode() == NM_Client) return World->GetAuthGameMode() == nullptr;
			const ARpgGameModeBase* GameMode = World->GetAuthGameMode<ARpgGameModeBase>();
			return GameMode && !GameMode->bEnableDiskPersistence && GameMode->WorldSaveSlotName.StartsWith(Prefix)
				&& GameMode->OfflineProfileKey == Prefix;
		}
	private:
		void OnInitialized(AGameModeBase* Initialized)
		{
			ARpgGameModeBase* GameMode = Cast<ARpgGameModeBase>(Initialized);
			if (!GameMode || !GameMode->GetWorld() || GameMode->GetWorld()->WorldType != EWorldType::PIE) return;
			GameMode->bEnableDiskPersistence = false;
			GameMode->WorldSaveSlotName = FString::Printf(TEXT("%s_%u"), *Prefix, GameMode->GetUniqueID());
			GameMode->WorldSaveBackupSlotName = GameMode->WorldSaveSlotName + TEXT("_Backup");
			GameMode->WorldSaveRecoverySlotName = GameMode->WorldSaveSlotName + TEXT("_Recovery");
			GameMode->OfflineProfileKey = Prefix;
		}
		FString Prefix;
		FDelegateHandle Handle;
	};

	/** Yaw samples of one role's view of the turning pawn, taken after its world finished ticking. */
	struct FRoleSamples
	{
		TArray<double> RootYaw, ActorYaw, Times;
		/** Root yaw relative to the capsule while the pawn stands still, from the mesh's base rotation offset. */
		double RestOffset = 0.;
		bool bLeftStance = false;

		/** Consecutive frames whose root yaw moved in opposite directions. */
		int32 Flips() const
		{
			int32 Count = 0;
			for (int32 Index = 2; Index < RootYaw.Num(); ++Index)
			{
				const double Previous = FMath::FindDeltaAngleDegrees(RootYaw[Index - 2], RootYaw[Index - 1]);
				const double Current = FMath::FindDeltaAngleDegrees(RootYaw[Index - 1], RootYaw[Index]);
				Count += FMath::Abs(Previous) > FlipThreshold && FMath::Abs(Current) > FlipThreshold && (Previous > 0.) != (Current > 0.);
			}
			return Count;
		}
		/** Mean change of the root's per-frame yaw step, in degrees per frame. */
		double Roughness() const
		{
			double Sum = 0.;
			for (int32 Index = 2; Index < RootYaw.Num(); ++Index)
				Sum += FMath::Abs(FMath::FindDeltaAngleDegrees(RootYaw[Index - 1], RootYaw[Index]) - FMath::FindDeltaAngleDegrees(RootYaw[Index - 2], RootYaw[Index - 1]));
			return RootYaw.Num() > 2 ? Sum / (RootYaw.Num() - 2) : 0.;
		}
		double ActorTurn() const
		{
			double Sum = 0.;
			for (int32 Index = 1; Index < ActorYaw.Num(); ++Index) Sum += FMath::Abs(FMath::FindDeltaAngleDegrees(ActorYaw[Index - 1], ActorYaw[Index]));
			return Sum;
		}
		double Duration() const { return Times.Num() > 1 ? Times.Last() - Times[0] : 0.; }
		/** Abrupt root stops per second: the root turns faster than 150 deg/s and is below 20 deg/s within three samples. */
		double HardStopsPerSecond() const
		{
			TArray<double> Speed;
			for (int32 Index = 1; Index < RootYaw.Num(); ++Index)
			{
				const double Step = Times[Index] - Times[Index - 1];
				Speed.Add(Step > 0. ? FMath::Abs(FMath::FindDeltaAngleDegrees(RootYaw[Index - 1], RootYaw[Index])) / Step : 0.);
			}
			int32 Count = 0;
			bool bTurning = false;
			for (int32 Index = 0; Index < Speed.Num(); ++Index)
			{
				bTurning |= Speed[Index] > 150.;
				if (bTurning && Speed[Index] < 20.)
				{
					bool bAbrupt = false;
					for (int32 Back = FMath::Max(0, Index - 3); Back < Index; ++Back) bAbrupt |= Speed[Back] > 150.;
					Count += bAbrupt;
					bTurning = false;
				}
			}
			return Duration() > 0. ? Count / Duration() : 0.;
		}
		/**
		 * Largest angle by which the root ran ahead of the capsule in the direction the camera turns, in degrees. Logged
		 * only: a listen server poses a remote pawn per received move, and its capsule yaw sampled at the end of the tick
		 * trails that pose by up to about 50 deg at 90 deg/s.
		 */
		double MaxLead(double Rate) const
		{
			double Lead = -180.;
			for (int32 Index = 0; Index < RootYaw.Num(); ++Index)
				Lead = FMath::Max(Lead, FMath::Sign(Rate) * FRotator::NormalizeAxis(RootYaw[Index] - ActorYaw[Index] - RestOffset));
			return Lead;
		}
	};

	/**
	 * Turns the owner's camera at a fixed rate through its control rotation, the way mouse look does, and records
	 * the root bone's world yaw of the same pawn as owner, on the listen server and as a simulated proxy.
	 */
	class FTurnInPlace final
	{
	public:
		~FTurnInPlace() { Stop(); }
		bool Start(ARpgCharacter* Owner, int32 InSubjectId)
		{
			Controller = Owner ? Cast<APlayerController>(Owner->GetController()) : nullptr;
			if (!Controller.IsValid() || !Controller->IsLocalController()) return false;
			SubjectId = InSubjectId;
			Yaw = Controller->GetControlRotation().Yaw;
			Controller->SetIgnoreLookInput(true);
			TickStartHandle = FWorldDelegates::OnWorldTickStart.AddRaw(this, &FTurnInPlace::Drive);
			TickEndHandle = FWorldDelegates::OnWorldTickEnd.AddRaw(this, &FTurnInPlace::Sample);
			return true;
		}
		void AddRole(const FString& Role, UWorld* World) { Worlds.Add(World, Role); }
		void Begin(double InRate) { Rate = InRate; Elapsed.Reset(); Samples.Reset(); }
		void Hold() { Rate = 0.; }
		bool Complete() const
		{
			if (Samples.Num() != Worlds.Num()) return false;
			for (const auto& Pair : Samples) if (Pair.Value.Duration() < WindowSeconds) return false;
			return true;
		}
		const TMap<FString, FRoleSamples>& GetSamples() const { return Samples; }
		void Stop()
		{
			FWorldDelegates::OnWorldTickStart.Remove(TickStartHandle); TickStartHandle.Reset();
			FWorldDelegates::OnWorldTickEnd.Remove(TickEndHandle); TickEndHandle.Reset();
			if (Controller.IsValid()) Controller->SetIgnoreLookInput(false);
			Controller.Reset(); Worlds.Reset(); Samples.Reset();
		}
	private:
		void Drive(UWorld* World, ELevelTick, float DeltaSeconds)
		{
			if (Rate == 0. || !Controller.IsValid() || Controller->GetWorld() != World) return;
			Yaw = FRotator::NormalizeAxis(Yaw + Rate * DeltaSeconds);
			Controller->SetControlRotation(FRotator(-10., Yaw, 0.));
		}
		void Sample(UWorld* World, ELevelTick, float DeltaSeconds)
		{
			const FString* Role = Worlds.Find(World);
			if (!Role || Rate == 0. || !ActiveWorld(World)) return;
			double& RoleElapsed = Elapsed.FindOrAdd(*Role);
			RoleElapsed += DeltaSeconds;
			const ARpgCharacter* Character = FindCharacter(World, SubjectId);
			FRoleSamples& RoleSamples = Samples.FindOrAdd(*Role);
			if (RoleElapsed < SettleSeconds || !Character || RoleSamples.Duration() >= WindowSeconds) return;
			RoleSamples.bLeftStance |= !Movement(Character)->IsInCombatStance() || Character->GetVelocity().Size2D() > 5.0;
			RoleSamples.RestOffset = Character->GetBaseRotationOffsetRotator().Yaw;
			RoleSamples.RootYaw.Add(Character->GetMesh()->GetSocketRotation(TEXT("root")).Yaw);
			RoleSamples.ActorYaw.Add(Character->GetActorRotation().Yaw);
			RoleSamples.Times.Add(World->GetRealTimeSeconds());
		}
		TWeakObjectPtr<APlayerController> Controller;
		TMap<TWeakObjectPtr<UWorld>, FString> Worlds;
		TMap<FString, double> Elapsed;
		TMap<FString, FRoleSamples> Samples;
		FDelegateHandle TickStartHandle, TickEndHandle;
		int32 SubjectId = INDEX_NONE;
		double Rate = 0., Yaw = 0.;
	};

	/** Yaw of the root bone relative to the capsule in one role's view, in degrees. */
	double RootOffset(const ARpgCharacter* Character)
	{
		return FRotator::NormalizeAxis(Character->GetMesh()->GetSocketRotation(TEXT("root")).Yaw - Character->GetActorRotation().Yaw);
	}

	/**
	 * Snaps or sweeps the owner's camera by a fixed angle, the way a quick mouse flick or a fast mouse swipe does,
	 * and compares each role's root offset to the capsule before the turn and after the pawn had time to step around.
	 */
	class FFlickTurn final
	{
	public:
		~FFlickTurn() { Stop(); }
		bool Start(ARpgCharacter* Owner, int32 InSubjectId)
		{
			Controller = Owner ? Cast<APlayerController>(Owner->GetController()) : nullptr;
			if (!Controller.IsValid() || !Controller->IsLocalController()) return false;
			SubjectId = InSubjectId;
			Controller->SetIgnoreLookInput(true);
			return true;
		}
		void AddRole(const FString& Role, UWorld* World) { Worlds.Add(Role, World); }
		int32 GetSubjectId() const { return SubjectId; }
		/**
		 * Records every role's resting offset and capsule yaw, then turns the camera by Angle degrees: at once
		 * without a rate, otherwise at Rate degrees per second of the owner's world time.
		 */
		bool Flick(double Angle, double Rate = 0.)
		{
			if (!Controller.IsValid() || SweepRemaining != 0.) return false;
			RestOffset.Reset(); RestYaw.Reset();
			for (const auto& Pair : Worlds)
			{
				const ARpgCharacter* Character = FindCharacter(Pair.Value.Get(), SubjectId);
				if (!Character) return false;
				RestOffset.Add(Pair.Key, RootOffset(Character));
				RestYaw.Add(Pair.Key, Character->GetActorRotation().Yaw);
			}
			FlickTime = FPlatformTime::Seconds();
			if (Rate <= 0.)
			{
				Turn(Angle);
				return true;
			}
			SweepRemaining = Angle;
			SweepRate = Rate;
			TickStartHandle = FWorldDelegates::OnWorldTickStart.AddRaw(this, &FFlickTurn::Sweep);
			return true;
		}
		bool Settled() const { return SweepRemaining == 0. && FPlatformTime::Seconds() - FlickTime >= FlickSettleSeconds; }
		const TMap<FString, TWeakObjectPtr<UWorld>>& GetWorlds() const { return Worlds; }
		double GetRestOffset(const FString& Role) const { return RestOffset.FindRef(Role); }
		double GetRestYaw(const FString& Role) const { return RestYaw.FindRef(Role); }
		void Stop()
		{
			FWorldDelegates::OnWorldTickStart.Remove(TickStartHandle); TickStartHandle.Reset();
			if (Controller.IsValid()) Controller->SetIgnoreLookInput(false);
			Controller.Reset(); Worlds.Reset(); RestOffset.Reset(); RestYaw.Reset();
			SweepRemaining = 0.;
		}
		static constexpr double FlickSettleSeconds = 2.5;
	private:
		void Turn(double Angle)
		{
			const FRotator Rotation = Controller->GetControlRotation();
			Controller->SetControlRotation(FRotator(-10., FRotator::NormalizeAxis(Rotation.Yaw + Angle), 0.));
		}
		void Sweep(UWorld* World, ELevelTick, float DeltaSeconds)
		{
			if (!Controller.IsValid() || Controller->GetWorld() != World || SweepRemaining == 0.) return;
			const double Step = FMath::Sign(SweepRemaining) * FMath::Min(FMath::Abs(SweepRemaining), SweepRate * DeltaSeconds);
			Turn(Step);
			SweepRemaining -= Step;
			if (FMath::IsNearlyZero(SweepRemaining))
			{
				SweepRemaining = 0.;
				FlickTime = FPlatformTime::Seconds();
				FWorldDelegates::OnWorldTickStart.Remove(TickStartHandle); TickStartHandle.Reset();
			}
		}
		TWeakObjectPtr<APlayerController> Controller;
		TMap<FString, TWeakObjectPtr<UWorld>> Worlds;
		TMap<FString, double> RestOffset, RestYaw;
		FDelegateHandle TickStartHandle;
		int32 SubjectId = INDEX_NONE;
		double FlickTime = 0., SweepRemaining = 0., SweepRate = 0.;
	};

	struct FState : FBasePIENetworkComponentState {};
}

NETWORK_TEST_CLASS(GaspCMCHostViewPIE, "SurvivalRpg.GASP.CMC.HostView")
{
	using FState = RpgGaspCMCHostViewTests::FState;
	// Destruction is reversed: network cleanup finishes while save isolation and the ground are still installed.
	RpgGaspCMCHostViewTests::FScopedSaveIsolation Isolation;
	FRpgScopedNetworkTestGround Ground;
	RpgGaspCMCHostViewTests::FTurnInPlace Turn;
	RpgGaspCMCHostViewTests::FFlickTurn Flick;
	FPIENetworkComponent<FState> Network{TestRunner, TestCommandBuilder, bInitializing};
	FPrimaryAssetId OriginalExperience;
	bool bConfigured = false;
	int32 SubjectId = INDEX_NONE;

	BEFORE_EACH()
	{
		using namespace RpgGaspCMCHostViewTests;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{
				TestRunner->AddError(TEXT("Host-view automation refuses to interrupt an existing PIE session."));
				return;
			}
		}
		UClass* GameModeClass = LoadClass<ARpgGameModeBase>(nullptr, GameModePath);
		ASSERT_THAT(IsNotNull(GameModeClass));
		if (!GameModeClass) return;
		Isolation.Start();
		Ground.Start();
		OriginalExperience = GetDefault<URpgDeveloperSettings>()->ExperienceOverride;
		GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = ExperienceId();
		bConfigured = true;
		FNetworkComponentBuilder<FState>().WithClients(2).AsListenServer()
			.WithGameInstanceClass(FSoftClassPath(TEXT("/Game/SurvivalRpg/Core/Game/BP_Rpg_GameInstance.BP_Rpg_GameInstance_C")))
			.WithGameMode(GameModeClass).Build(Network);
	}
	AFTER_EACH()
	{
		Turn.Stop();
		Flick.Stop();
		if (bConfigured) GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = OriginalExperience;
	}

	/**
	 * A listen server ticks a remote client's pose per received move inside the move's deferred capsule update.
	 * Offset Root Bone must still hold the root as steadily in the host's view as on the owner and on another
	 * client, for a client present from the start and for a late joiner. On every role the turn-in-place steps
	 * must play out without frequent dead stops.
	 */
	TEST_METHOD(RemoteStanceTurnInPlaceStaysSteadyOnTheHost)
	{
		using namespace RpgGaspCMCHostViewTests;
		if (!bConfigured) return;
		QueueSubject(0, 1);
		QueueTurnWindow(90., TEXT("first client"));
		QueueTurnWindow(180., TEXT("first client"));
		Network.ThenServer(TEXT("Stop turning the first client"), [this](FState&) { Turn.Stop(); })
			.ThenClientJoins();
		QueueSubject(2, 0);
		QueueTurnWindow(90., TEXT("late joiner"));
	}

	/**
	 * A standing pawn in the combat stance turns its capsule with the camera at once. Its feet must follow
	 * through turn-in-place steps on the owner, the listen server, a proxy and a late joiner, instead of the root
	 * staying where it stood. A fast camera sweep must finish its turn as well instead of stopping halfway.
	 */
	TEST_METHOD(StandingStanceTurnFollowsTheCameraOnEveryRole)
	{
		using namespace RpgGaspCMCHostViewTests;
		if (!bConfigured) return;
		Network.UntilClient(TEXT("Owner stands armed in the combat stance"), 0, [](FState& State)
				{ return ReadyInStance(State.World, LocalCharacter(State.World)); }, Timeout())
			.ThenClient(TEXT("Capture the owner and its camera"), 0, [this](FState& State)
			{
				ARpgCharacter* Character = LocalCharacter(State.World);
				ASSERT_THAT(IsTrue(Isolation.IsIsolated(State.World) && Character->GetLocalRole() == ROLE_AutonomousProxy));
				ASSERT_THAT(IsTrue(Flick.Start(Character, Character->GetPlayerState()->GetPlayerId())));
				Flick.AddRole(TEXT("owner"), State.World);
			})
			.UntilServer(TEXT("Listen server holds the remote owner's pawn in the stance"), [this](FState& State)
				{ return ReadyInStance(State.World, FindCharacter(State.World, Flick.GetSubjectId())); }, Timeout())
			.ThenServer(TEXT("Record the host view"), [this](FState& State) { Flick.AddRole(TEXT("server"), State.World); })
			.UntilClient(TEXT("Observer sees the pawn as a simulated proxy in the stance"), 1, [this](FState& State)
			{
				const ARpgCharacter* Character = FindCharacter(State.World, Flick.GetSubjectId());
				return ReadyInStance(State.World, Character) && Character->GetLocalRole() == ROLE_SimulatedProxy;
			}, Timeout())
			.ThenClient(TEXT("Record the proxy view"), 1, [this](FState& State) { Flick.AddRole(TEXT("proxy"), State.World); });
		QueueFlick(180.);
		QueueFlick(-90.);
		QueueFlick(450., 720.);
		Network.ThenClientJoins()
			.UntilClient(TEXT("Late joiner sees the pawn as a simulated proxy in the stance"), 2, [this](FState& State)
			{
				const ARpgCharacter* Character = FindCharacter(State.World, Flick.GetSubjectId());
				return ReadyInStance(State.World, Character) && Character->GetLocalRole() == ROLE_SimulatedProxy;
			}, Timeout())
			.ThenClient(TEXT("Record the late joiner's view"), 2, [this](FState& State) { Flick.AddRole(TEXT("late joiner"), State.World); });
		QueueFlick(180.);
		QueueFlick(-450., 720.);
	}

	/** Snaps the owner's camera by Angle degrees, or sweeps it at Rate degrees per second, and checks every role once the pawn settled. */
	void QueueFlick(double Angle, double Rate = 0.)
	{
		using namespace RpgGaspCMCHostViewTests;
		// Turn-in-place starts at 50 deg, as in GASP, and the aim offset covers the rest. A fast sweep may end with the
		// root having met the capsule while the camera still moved on, but never further behind than that threshold.
		const double MaxLag = Rate > 0. ? 50. : 60.;
		Network.ThenServer(TEXT("Turn the owner's camera while standing"), [this, Angle, Rate](FState&) { ASSERT_THAT(IsTrue(Flick.Flick(Angle, Rate))); })
			.UntilServer(TEXT("Give the pawn time to step around"), [this](FState&) { return Flick.Settled(); }, Timeout())
			.ThenServer(TEXT("Every role's root followed the capsule"), [this, Angle, Rate, MaxLag](FState&)
			{
				for (const auto& Pair : Flick.GetWorlds())
				{
					const ARpgCharacter* Character = FindCharacter(Pair.Value.Get(), Flick.GetSubjectId());
					ASSERT_THAT(IsNotNull(Character));
					if (!Character) return;
					const double CapsuleTurn = FMath::FindDeltaAngleDegrees(Flick.GetRestYaw(Pair.Key), Character->GetActorRotation().Yaw);
					const double Lag = FMath::Abs(FMath::FindDeltaAngleDegrees(Flick.GetRestOffset(Pair.Key), RootOffset(Character)));
					UE_LOG(LogTemp, Display, TEXT("RpgGaspCMCStandingTurn flick=%.0f rate=%.0f role=%s capsuleTurn=%.1f rootLag=%.1f"), Angle, Rate, *Pair.Key, CapsuleTurn, Lag);
					TestRunner->TestTrue(FString::Printf(TEXT("%s sees the capsule turn %.0f deg (%.1f)"), *Pair.Key, Angle, CapsuleTurn),
						FMath::Abs(FMath::FindDeltaAngleDegrees(CapsuleTurn, Angle)) < 10.);
					TestRunner->TestFalse(FString::Printf(TEXT("%s keeps the pawn standing in the stance"), *Pair.Key),
						!Movement(Character)->IsInCombatStance() || Character->GetVelocity().Size2D() > 5.0);
					TestRunner->TestTrue(FString::Printf(TEXT("%s root follows a %.0f deg turn at %.0f deg/s (%.1f deg behind)"), *Pair.Key, Angle, Rate, Lag), Lag < MaxLag);
				}
			});
	}

	/** Captures the owning client's pawn and the views of it on the listen server and on the observing client. */
	void QueueSubject(int32 OwnerClient, int32 ObserverClient)
	{
		using namespace RpgGaspCMCHostViewTests;
		Network.UntilClient(TEXT("Owner stands armed in the combat stance"), OwnerClient, [](FState& State)
				{ return ReadyInStance(State.World, LocalCharacter(State.World)); }, Timeout())
			.ThenClient(TEXT("Capture the owner and its camera turn driver"), OwnerClient, [this](FState& State)
			{
				ARpgCharacter* Character = LocalCharacter(State.World);
				ASSERT_THAT(IsTrue(Isolation.IsIsolated(State.World) && Character->GetLocalRole() == ROLE_AutonomousProxy));
				SubjectId = Character->GetPlayerState()->GetPlayerId();
				ASSERT_THAT(IsTrue(Turn.Start(Character, SubjectId)));
				Turn.AddRole(TEXT("owner"), State.World);
			})
			.UntilServer(TEXT("Listen server holds the remote owner's pawn in the stance"), [this](FState& State)
			{
				const ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				return State.World->GetNetMode() == NM_ListenServer && ReadyInStance(State.World, Character)
					&& Character->HasAuthority() && Character->GetRemoteRole() == ROLE_AutonomousProxy;
			}, Timeout())
			.ThenServer(TEXT("Record the host view"), [this](FState& State)
				{ ASSERT_THAT(IsTrue(Isolation.IsIsolated(State.World))); Turn.AddRole(TEXT("server"), State.World); })
			.UntilClient(TEXT("Observer sees the pawn as a simulated proxy in the stance"), ObserverClient, [this](FState& State)
			{
				const ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				return ReadyInStance(State.World, Character) && Character->GetLocalRole() == ROLE_SimulatedProxy;
			}, Timeout())
			.ThenClient(TEXT("Record the proxy view"), ObserverClient, [this](FState& State) { Turn.AddRole(TEXT("proxy"), State.World); });
	}

	void QueueTurnWindow(double Rate, const TCHAR* Subject)
	{
		using namespace RpgGaspCMCHostViewTests;
		const FString Label(Subject);
		Network.ThenServer(TEXT("Turn the owner's camera in place"), [this, Rate](FState&) { Turn.Begin(Rate); })
			.UntilServer(TEXT("Every role recorded the turn"), [this](FState&) { return Turn.Complete(); }, Timeout())
			.ThenServer(TEXT("Compare the host's root yaw with the owner and the proxy"), [this, Rate, Label](FState&)
			{
				Turn.Hold();
				const TMap<FString, FRoleSamples>& Samples = Turn.GetSamples();
				for (const auto& Pair : Samples)
				{
					UE_LOG(LogTemp, Display, TEXT("RpgGaspCMCHostView subject=%s rate=%.0f role=%s samples=%d duration=%.3f actorTurn=%.1f rootFlips=%d rootRoughness=%.3f hardStopsPerSecond=%.2f maxLead=%.1f leftStance=%d"),
						*Label, Rate, *Pair.Key, Pair.Value.RootYaw.Num(), Pair.Value.Duration(), Pair.Value.ActorTurn(),
						Pair.Value.Flips(), Pair.Value.Roughness(), Pair.Value.HardStopsPerSecond(), Pair.Value.MaxLead(Rate), Pair.Value.bLeftStance);
					TestRunner->TestFalse(FString::Printf(TEXT("%s %s stays standing in the stance at %.0f deg/s"), *Label, *Pair.Key, Rate), Pair.Value.bLeftStance);
					// A started turn-in-place step plays until the root is close to the capsule instead of being cut to the
					// idle pose below 50 deg, which stopped the root dead several times per second.
					TestRunner->TestTrue(FString::Printf(TEXT("%s %s root turns without frequent dead stops at %.0f deg/s (%.2f per s)"), *Label, *Pair.Key, Rate, Pair.Value.HardStopsPerSecond()),
						Pair.Value.HardStopsPerSecond() <= 3.);
				}
				const FRoleSamples* Server = Samples.Find(TEXT("server"));
				const FRoleSamples* Proxy = Samples.Find(TEXT("proxy"));
				ASSERT_THAT(IsTrue(Server && Proxy));
				if (!Server || !Proxy) return;
				// The capsule follows the camera; prove the host actually saw the pawn turn before judging its root.
				TestRunner->TestTrue(FString::Printf(TEXT("Host sees the %s's capsule turn at %.0f deg/s (%.1f deg)"), *Label, Rate, Server->ActorTurn()),
					Server->ActorTurn() >= .6 * Rate * Server->Duration());
				TestRunner->TestTrue(FString::Printf(TEXT("Host root of the %s changes direction no more than the proxy at %.0f deg/s (%d vs %d)"), *Label, Rate, Server->Flips(), Proxy->Flips()),
					Server->Flips() <= Proxy->Flips() + 2);
				TestRunner->TestTrue(FString::Printf(TEXT("Host root of the %s is as steady as the proxy at %.0f deg/s (%.3f vs %.3f deg/frame)"), *Label, Rate, Server->Roughness(), Proxy->Roughness()),
					Server->Roughness() <= Proxy->Roughness() + .05);
			});
	}
};
#endif // ENABLE_PIE_NETWORK_TEST

#endif // WITH_DEV_AUTOMATION_TESTS
