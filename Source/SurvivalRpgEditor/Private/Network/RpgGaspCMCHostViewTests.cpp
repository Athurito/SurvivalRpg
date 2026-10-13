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

	struct FState : FBasePIENetworkComponentState {};
}

NETWORK_TEST_CLASS(GaspCMCHostViewPIE, "SurvivalRpg.GASP.CMC.HostView")
{
	using FState = RpgGaspCMCHostViewTests::FState;
	// Destruction is reversed: network cleanup finishes while save isolation and the ground are still installed.
	RpgGaspCMCHostViewTests::FScopedSaveIsolation Isolation;
	FRpgScopedNetworkTestGround Ground;
	RpgGaspCMCHostViewTests::FTurnInPlace Turn;
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
		if (bConfigured) GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = OriginalExperience;
	}

	/**
	 * A listen server ticks a remote client's pose per received move inside the move's deferred capsule update.
	 * Offset Root Bone must still hold the root as steadily in the host's view as on the owner and a second client.
	 */
	TEST_METHOD(RemoteStanceTurnInPlaceStaysSteadyOnTheHost)
	{
		using namespace RpgGaspCMCHostViewTests;
		if (!bConfigured) return;
		Network.UntilClient(TEXT("Owner stands armed in the combat stance"), 0, [](FState& State)
				{ return ReadyInStance(State.World, LocalCharacter(State.World)); }, Timeout())
			.ThenClient(TEXT("Capture the owner and its camera turn driver"), 0, [this](FState& State)
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
			.UntilClient(TEXT("Second client sees the pawn as a simulated proxy in the stance"), 1, [this](FState& State)
			{
				const ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				return ReadyInStance(State.World, Character) && Character->GetLocalRole() == ROLE_SimulatedProxy;
			}, Timeout())
			.ThenClient(TEXT("Record the proxy view"), 1, [this](FState& State) { Turn.AddRole(TEXT("proxy"), State.World); });
		for (const double Rate : {90., 180.})
		{
			Network.ThenServer(TEXT("Turn the owner's camera in place"), [this, Rate](FState&) { Turn.Begin(Rate); })
				.UntilServer(TEXT("Every role recorded the turn"), [this](FState&) { return Turn.Complete(); }, Timeout())
				.ThenServer(TEXT("Compare the host's root yaw with the owner and the proxy"), [this, Rate](FState&)
				{
					Turn.Hold();
					const TMap<FString, FRoleSamples>& Samples = Turn.GetSamples();
					for (const auto& Pair : Samples)
					{
						UE_LOG(LogTemp, Display, TEXT("RpgGaspCMCHostView rate=%.0f role=%s samples=%d duration=%.3f actorTurn=%.1f rootFlips=%d rootRoughness=%.3f leftStance=%d"),
							Rate, *Pair.Key, Pair.Value.RootYaw.Num(), Pair.Value.Duration(), Pair.Value.ActorTurn(),
							Pair.Value.Flips(), Pair.Value.Roughness(), Pair.Value.bLeftStance);
						TestRunner->TestFalse(FString::Printf(TEXT("%s stays standing in the stance at %.0f deg/s"), *Pair.Key, Rate), Pair.Value.bLeftStance);
					}
					const FRoleSamples* Server = Samples.Find(TEXT("server"));
					const FRoleSamples* Proxy = Samples.Find(TEXT("proxy"));
					ASSERT_THAT(IsTrue(Server && Proxy));
					if (!Server || !Proxy) return;
					// The capsule follows the camera; prove the host actually saw the pawn turn before judging its root.
					TestRunner->TestTrue(FString::Printf(TEXT("Host sees the capsule turn at %.0f deg/s (%.1f deg)"), Rate, Server->ActorTurn()),
						Server->ActorTurn() >= .6 * Rate * Server->Duration());
					TestRunner->TestTrue(FString::Printf(TEXT("Host root changes direction no more than the proxy at %.0f deg/s (%d vs %d)"), Rate, Server->Flips(), Proxy->Flips()),
						Server->Flips() <= Proxy->Flips() + 2);
					TestRunner->TestTrue(FString::Printf(TEXT("Host root is as steady as the proxy at %.0f deg/s (%.3f vs %.3f deg/frame)"), Rate, Server->Roughness(), Proxy->Roughness()),
						Server->Roughness() <= Proxy->Roughness() + .05);
				});
		}
	}
};
#endif // ENABLE_PIE_NETWORK_TEST

#endif // WITH_DEV_AUTOMATION_TESTS
