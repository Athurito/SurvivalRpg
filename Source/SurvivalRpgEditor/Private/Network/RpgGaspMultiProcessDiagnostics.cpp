#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Network/RpgGaspNetworkTestTypes.h"
#include "Animation/TrajectoryTypes.h"
#include "BoneControllers/AnimNode_OffsetRootBone.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/NetDriver.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PoseSearch/AnimNode_MotionMatching.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealEdGlobals.h"
#include "UnrealClient.h"
#include "UObject/UnrealType.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMovementComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Development/RpgDeveloperSettings.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

namespace RpgGaspMultiProcessDiagnostics
{
constexpr TCHAR Map[] = TEXT("/Game/SurvivalRpg/Maps/Test/Lvl_ThirdPerson");
constexpr TCHAR GameMode[] = TEXT("/Game/SurvivalRpg/Core/Game/BP_Rpg_GameMode.BP_Rpg_GameMode_C");
constexpr double SegmentDuration = 48.0;

FString Csv(const FString& Value)
{
	const FString Quote = FString::Chr(34);
	return Quote + Value.Replace(*Quote, *(Quote + Quote)) + Quote;
}

// Missing optional diagnostics stay explicitly unavailable when this harness is built against an older baseline.
FString PropertyText(UObject* Object, const TCHAR* Name)
{
	if (const FProperty* Property = FindFProperty<FProperty>(Object->GetClass(), Name))
	{
		FString Result;
		Property->ExportText_InContainer(0, Result, Object, Object, Object, PPF_None);
		return Result;
	}
	return TEXT("unavailable");
}

FString SearchDatabasesText(FAnimNode_MotionMatching* Node, UObject* Owner)
{
	if (Node)
	{
		if (const FArrayProperty* Property = FindFProperty<FArrayProperty>(FAnimNode_MotionMatching::StaticStruct(), TEXT("DatabasesToSearch")))
		{
			// UE fills this transient array for both the configured Database and SetDatabasesToSearch overrides.
			// Reflection avoids unexported/private accessor calls; caller has already joined graph evaluation.
			FString Result;
			Property->ExportText_InContainer(0, Result, Node, nullptr, Owner, PPF_None);
			return Result;
		}
	}
	return TEXT("unavailable");
}

template <typename T>
T* FindNode(UAnimInstance* Anim)
{
	for (TFieldIterator<FStructProperty> It(Anim->GetClass()); It; ++It)
	{
		if (It->Struct == T::StaticStruct())
		{
			return It->ContainerPtrToValuePtr<T>(Anim);
		}
	}
	return nullptr;
}

UWorld* FindPlayWorld()
{
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
		{
			return Context.World();
		}
	}
	return nullptr;
}

struct FInputStep
{
	const TCHAR* Name = TEXT("idle");
	FVector Direction = FVector::ZeroVector;
	float Scale = 0.0f;
	float ControlYaw = 0.0f;
	bool bJump = false;
	bool bCrouch = false;
	ERpgCharacterRotationMode Mode = ERpgCharacterRotationMode::Free;
	float TailFollowupYaw = 0.0f;
	double FollowupAtSeconds = -1.0;
};

FInputStep ResolveStep(double Seconds)
{
	const auto Turn = [](const TCHAR* Name, float Yaw, ERpgCharacterRotationMode Mode, float TailYaw = 0.0f, double FollowupAtSeconds = -1.0)
	{
		return FInputStep{Name, FVector::ZeroVector, 0.0f, Yaw, false, false, Mode, TailYaw, FollowupAtSeconds};
	};
	const ERpgCharacterRotationMode Combat = ERpgCharacterRotationMode::CombatStrafe;
	const ERpgCharacterRotationMode Aim = ERpgCharacterRotationMode::Aim;
	if (Seconds < 2.0) return Turn(TEXT("combat_settle"), 0.0f, Combat);
	if (Seconds < 4.0) return Turn(TEXT("combat_stationary_45"), 45.0f, Combat);
	if (Seconds < 6.0) return Turn(TEXT("combat_stationary_90"), 135.0f, Combat);
	if (Seconds < 8.0) return Turn(TEXT("combat_stationary_180"), 315.0f, Combat);
	if (Seconds < 11.0) return Turn(TEXT("combat_active_followup"), 45.0f, Combat, 90.0f, 8.45);
	if (Seconds < 14.0) return Turn(TEXT("combat_active_opposite"), 225.0f, Combat, -135.0f, 11.45);
	if (Seconds < 16.0) return Turn(TEXT("aim_settle"), 135.0f, Aim);
	if (Seconds < 18.0) return Turn(TEXT("aim_stationary_45"), 180.0f, Aim);
	if (Seconds < 20.0) return Turn(TEXT("aim_stationary_90"), 270.0f, Aim);
	if (Seconds < 22.0) return Turn(TEXT("aim_stationary_180"), 90.0f, Aim);
	if (Seconds < 25.0) return Turn(TEXT("aim_active_followup"), 180.0f, Aim, 90.0f, 22.45);
	if (Seconds < 28.0) return Turn(TEXT("aim_active_opposite"), 0.0f, Aim, -135.0f, 25.45);
	if (Seconds < 29.0) return {};
	if (Seconds < 31.0) return {TEXT("run_before_jump"), FVector::ForwardVector, 1.0f};
	if (Seconds < 32.0) return {TEXT("run_jump_hold_hitch"), FVector::ForwardVector, 1.0f, 0.0f, true};
	if (Seconds < 34.0) return {TEXT("run_land_hold"), FVector::ForwardVector, 1.0f};
	if (Seconds < 34.3) return {TEXT("run_jump_again"), FVector::ForwardVector, 1.0f, 0.0f, true};
	if (Seconds < 37.0) return {TEXT("run_land_stop"), FVector::ForwardVector, 1.0f};
	if (Seconds < 39.0) return {TEXT("run_before_walk_jump"), FVector::ForwardVector, 1.0f};
	if (Seconds < 39.3) return {TEXT("run_jump_to_walk"), FVector::ForwardVector, 1.0f, 0.0f, true};
	if (Seconds < 42.0) return {TEXT("run_land_walk"), FVector::ForwardVector, 1.0f};
	if (Seconds < 44.0) return {TEXT("stop"), FVector::ZeroVector, 0.0f};
	if (Seconds < 46.0) return {TEXT("crouch"), FVector::ForwardVector, 0.5f, 0.0f, false, true};
	return {TEXT("settle"), FVector::ZeroVector, 0.0f};
}

/** One opt-in editor process owns one PIE world. Files synchronize the test driver, never gameplay state. */
class FSession final : public IAutomationLatentCommand
{
public:
	FSession(FAutomationTestBase* InTest, FString InRole, FString InDirectory, int32 InFps, int32 InPort)
		: Test(InTest), Role(MoveTemp(InRole)), Directory(MoveTemp(InDirectory)), Fps(InFps), Port(InPort)
	{
		StartedAt = FPlatformTime::Seconds();
	}
	virtual ~FSession() override { if (bStarted && !bFinished) Finish(false); }

	virtual bool Update() override
	{
		if (!bStarted)
		{
			Start();
			return false;
		}
		if (bFailed || FPlatformTime::Seconds() - StartedAt > 300.0)
		{
			if (!bFailed) Test->AddError(FString::Printf(TEXT("Multi-process %s timed out in %s"), *Role, *Phase));
			Finish(false);
			return true;
		}
		UWorld* World = FindPlayWorld();
		if (!World) return false;
		if (Role != TEXT("server") && !bTravelRequested)
		{
			bTravelRequested = true;
			const FString URL = FString::Printf(TEXT("127.0.0.1:%d?Name=Gasp_%s"), Port, *Role);
			GEngine->SetClientTravel(World, *URL, TRAVEL_Absolute);
			return false;
		}
		if (!World->GetNetDriver() || (Role != TEXT("server") && World->GetNetMode() != NM_Client)) return false;
		if (!ConfiguredDrivers.Contains(World->GetNetDriver()))
		{
			FPacketSimulationSettings Packets;
			Packets.PktLag = 60;
			Packets.PktLagVariance = 10;
			Packets.PktLoss = 10;
			FParse::Value(FCommandLine::Get(), TEXT("GaspTraceLag="), Packets.PktLag);
			FParse::Value(FCommandLine::Get(), TEXT("GaspTraceLagVariance="), Packets.PktLagVariance);
			FParse::Value(FCommandLine::Get(), TEXT("GaspTraceLoss="), Packets.PktLoss);
			World->GetNetDriver()->SetPacketSimulationSettings(Packets);
			ConfiguredDrivers.Add(World->GetNetDriver());
		}
		if (Role == TEXT("server")) PrepareAuthority(*World);
		APlayerController* PC = World->GetFirstPlayerController();
		ARpgCharacter* Local = PC ? Cast<ARpgCharacter>(PC->GetPawn()) : nullptr;
		if (!Local || !Local->GetPlayerState() || !Local->GetRpgAbilitySystemComponent() ||
			!Local->GetMesh() || !Cast<URpgAnimInstance>(Local->GetMesh()->GetAnimInstance()) ||
			Local->GetActorLocation().Z < 9000.0) return false;
		if (!bReady)
		{
			bReady = true;
			WriteMarker(Role + TEXT(".ready"), FString::Printf(TEXT("pid=%u player_id=%d net_mode=%d\n"),
				FPlatformProcess::GetCurrentProcessId(), Local->GetPlayerState()->GetPlayerId(), World->GetNetMode()));
		}
		if (Role == TEXT("server"))
		{
			if (!Exists(TEXT("start.txt")) && Exists(TEXT("owner.ready")))
			{
				WriteEpoch(TEXT("start.txt"));
			}
			if (StartTicks > 0 && Elapsed(StartTicks) >= SegmentDuration)
			{
				if (!Exists(TEXT("late.request"))) WriteMarker(TEXT("late.request"), TEXT("Launch the late client now.\n"));
				if (Exists(TEXT("late.ready")) && !Exists(TEXT("resume.txt"))) WriteEpoch(TEXT("resume.txt"));
			}
		}
		ReadEpoch(TEXT("start.txt"), StartTicks);
		ReadEpoch(TEXT("resume.txt"), ResumeTicks);
		ReadPlayerId(TEXT("server"), HostPlayerId);
		ReadPlayerId(TEXT("owner"), SubjectPlayerId);
		ReadPlayerId(TEXT("late"), LatePlayerId);
		if (!bCaptureComplete && ResumeTicks > 0 && Elapsed(ResumeTicks) > SegmentDuration + 2.0)
		{
			// Files cannot substitute for replicated pawn/channel creation: require the remote subject in every peer.
			bool bHasSubject = false;
			TSet<int32> PresentPlayerIds;
			for (TActorIterator<ARpgCharacter> It(World); It; ++It)
			{
				if (const APlayerState* PS = It->GetPlayerState())
				{
					PresentPlayerIds.Add(PS->GetPlayerId());
					bHasSubject |= SubjectPlayerId != INDEX_NONE && PS->GetPlayerId() == SubjectPlayerId;
				}
			}
			Test->TestTrue(TEXT("The autonomous subject replicated into this process"), bHasSubject);
			const bool bHasAllPlayers = HostPlayerId != INDEX_NONE && SubjectPlayerId != INDEX_NONE && LatePlayerId != INDEX_NONE &&
				PresentPlayerIds.Contains(HostPlayerId) && PresentPlayerIds.Contains(SubjectPlayerId) && PresentPlayerIds.Contains(LatePlayerId);
			Test->TestTrue(TEXT("The exact listen-host, initial-owner and late-observer PlayerIds are present"), bHasAllPlayers);
			Test->TestTrue(TEXT("Selected Motion Matching clips were captured"), SelectedClipRows > 0);
			Test->TestTrue(TEXT("The subject moved through real CharacterMovement"), bSawSubjectMove);
			Test->TestTrue(TEXT("The subject jumped and physically landed"), bSawSubjectLand);
			Test->TestTrue(TEXT("Stationary CombatStrafe and Aim selected real turn clips"), bSawSubjectCombatTurn && bSawSubjectAimTurn);
			bCaptureComplete = true;
			bCaptureSucceeded = bHasSubject && bHasAllPlayers && SelectedClipRows > 0 && bSawSubjectMove && bSawSubjectLand && bSawSubjectCombatTurn && bSawSubjectAimTurn;
			WriteMarker(Role + TEXT(".captured"), bCaptureSucceeded ? TEXT("success\n") : TEXT("failed\n"));
		}
		if (Role == TEXT("server") && bCaptureComplete && Exists(TEXT("owner.captured")) && Exists(TEXT("late.captured")) && !Exists(TEXT("finish.txt"))) WriteEpoch(TEXT("finish.txt"));
		ReadEpoch(TEXT("finish.txt"), FinishTicks);
		if (bCaptureComplete && FinishTicks > 0 && Elapsed(FinishTicks) >= 0.0) { Finish(bCaptureSucceeded); return true; }
		return false;
	}

private:
	void Start()
	{
		bStarted = true;
		IFileManager::Get().MakeDirectory(*Directory, true);
		CsvPath = Directory / (Role + TEXT(".csv"));
		// Keep one write handle open, allowing readers; reopening an exclusive append handle races live readers on Windows.
		CsvWriter.Reset(IFileManager::Get().CreateFileWriter(*CsvPath, FILEWRITE_Append | FILEWRITE_AllowRead));
		bCaptureScreenshots = FParse::Param(FCommandLine::Get(), TEXT("GaspCaptureScreenshots"));
		Buffer = TEXT("process_role,pid,fps_limit,delta_seconds,utc_ticks,phase,phase_seconds,world_seconds,pawn,player_id,player_name,local_role,locally_controlled,actor_x,actor_y,actor_z,actor_yaw,mesh_yaw,root_yaw,offset_root_yaw,trajectory_yaw_now,trajectory_yaw_future,speed,acceleration,rotation_mode,gait,history_resets,client_corrections,mm_database_role,mm_interrupt,mm_continuing,tir_state,landing_ground_search_released,mm_node,selected_clip,selected_asset_time,elapsed_pose_search,blend_index,blend_clip,blend_asset_time,play_rate,blend_scalar_weight,has_per_bone_blend_profile,input_scale,input_yaw,tail_followup_triggered,hitch_injected,tir_query_angle,tir_accumulated_yaw,tir_state_elapsed,jump_phase,landing_state_elapsed,contact_l,contact_r,foot_placement_alpha,mm_search_databases,selected_asset_fraction,tir_root_feedback,anim_update_counter,bone_revision,engine_frame\n");
		Flush();
		OriginalExperience = GetDefault<URpgDeveloperSettings>()->ExperienceOverride;
		OriginalNetDrivers = GEngine->NetDriverDefinitions;
		GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = FPrimaryAssetId();
		GameModeClass = LoadClass<ARpgGameModeBase>(nullptr, GameMode);
		if (!GameModeClass) { Test->AddError(TEXT("Cannot load GASP game mode")); bFailed = true; return; }
		ARpgGameModeBase* Defaults = CastChecked<ARpgGameModeBase>(GameModeClass->GetDefaultObject());
		bOriginalPersistence = Defaults->bEnableDiskPersistence;
		Defaults->bEnableDiskPersistence = false;
		for (FNetDriverDefinition& Definition : GEngine->NetDriverDefinitions)
		{
			if (Definition.DefName == NAME_GameNetDriver)
			{
				Definition.DriverClassName = FName(TEXT("/Script/OnlineSubsystemUtils.IpNetDriver"));
				Definition.DriverClassNameFallback = Definition.DriverClassName;
			}
		}
		if (IConsoleVariable* Limit = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS")))
		{
			OriginalFps = Limit->GetFloat();
			Limit->Set(static_cast<float>(Fps), ECVF_SetByCode);
		}
		// The same real package exists in each process; an unsaved CQTest map cannot be loaded by IP clients.
		FAutomationEditorCommonUtils::LoadMap(Map);
		ULevelEditorPlaySettings* Settings = NewObject<ULevelEditorPlaySettings>();
		Settings->SetPlayNetMode(Role == TEXT("server") ? PIE_ListenServer : PIE_Standalone);
		Settings->SetPlayNumberOfClients(1);
		Settings->SetRunUnderOneProcess(true);
		Settings->SetServerPort(static_cast<uint16>(Port));
		Settings->bLaunchSeparateServer = false;
		Settings->GameGetsMouseControl = false;
		Settings->NewWindowWidth = 640;
		Settings->NewWindowHeight = 480;
		FRequestPlaySessionParams Params;
		Params.EditorPlaySettings = Settings;
		Params.GameModeOverride = GameModeClass;
		Params.bAllowOnlineSubsystem = false;
		GUnrealEd->RequestPlaySession(Params);
		GUnrealEd->StartQueuedPlaySessionRequest();
		PreTick = FWorldDelegates::OnWorldPreActorTick.AddRaw(this, &FSession::Input);
		PostTick = FWorldDelegates::OnWorldPostActorTick.AddRaw(this, &FSession::Trace);
	}

	void PrepareAuthority(UWorld& World)
	{
		if (!Floor.IsValid()) Floor = World.SpawnActor<ARpgGaspNetworkFloorFixture>(FVector(0, 0, 10000), FRotator::ZeroRotator);
		for (TActorIterator<ARpgCharacter> It(&World); It; ++It)
		{
			if (!It->IsPlayerControlled() || !It->GetPlayerState() || Positioned.Contains(*It)) continue;
			const int32 Index = Positioned.Num();
			It->TeleportTo(FVector(0, Index * 2000.0, 10100.0), FRotator::ZeroRotator);
			It->GetCharacterMovement()->StopMovementImmediately();
			Positioned.Add(*It);
		}
	}

	void Input(UWorld* World, ELevelTick TickType, float DeltaSeconds)
	{
		if (World != FindPlayWorld() || !bReady) return;
		bHitchThisFrame = false;
		bTailTriggered = false;
		CurrentStep = {};
		Phase = TEXT("await_start");
		PhaseSeconds = StartTicks > 0 ? Elapsed(StartTicks) : -1.0;
		if (ResumeTicks > 0 && Elapsed(ResumeTicks) >= 0.0)
		{
			PhaseSeconds = Elapsed(ResumeTicks);
			CurrentStep = ResolveStep(PhaseSeconds);
			Phase = TEXT("after_late/") + FString(CurrentStep.Name);
		}
		else if (PhaseSeconds >= SegmentDuration)
		{
			const bool bForward = (static_cast<int32>((PhaseSeconds - SegmentDuration) / 3.0) % 2) == 0;
			CurrentStep = {TEXT("late_wait_run"), bForward ? FVector::ForwardVector : -FVector::ForwardVector, 1.0f, bForward ? 0.0f : 180.0f};
			Phase = CurrentStep.Name;
		}
		else if (PhaseSeconds >= 0.0)
		{
			CurrentStep = ResolveStep(PhaseSeconds);
			Phase = TEXT("before_late/") + FString(CurrentStep.Name);
		}
		// Existing GAS request tags drive Character's authoritative mode resolver and its normal replication.
		// This is the same rotation-request seam as the PIE contracts, never an AnimInstance state override.
		if (Role == TEXT("server"))
		{
			for (TActorIterator<ARpgCharacter> It(World); It; ++It)
			{
				if (!It->IsPlayerControlled()) continue;
				if (URpgAbilitySystemComponent* ASC = It->GetRpgAbilitySystemComponent())
				{
					ASC->SetLooseGameplayTagCount(RpgGameplayTags::State_Rotation_CombatStrafe, CurrentStep.Mode == ERpgCharacterRotationMode::CombatStrafe ? 1 : 0, EGameplayTagReplicationState::TagAndCountToAll);
					ASC->SetLooseGameplayTagCount(RpgGameplayTags::State_Rotation_Aim, CurrentStep.Mode == ERpgCharacterRotationMode::Aim ? 1 : 0, EGameplayTagReplicationState::TagAndCountToAll);
				}
			}
		}
		if (Role != TEXT("owner") && Role != TEXT("server")) return;
		APlayerController* PC = World->GetFirstPlayerController();
		ARpgCharacter* Character = PC ? Cast<ARpgCharacter>(PC->GetPawn()) : nullptr;
		if (!Character || !Character->IsLocallyControlled()) return;
		if (CurrentStep.TailFollowupYaw != 0.0f)
		{
			// Fixed schedule keeps the input independent of whether a runtime exits its turn clip early.
			bTailTriggered = PhaseSeconds >= CurrentStep.FollowupAtSeconds;
			if (bTailTriggered) CurrentStep.ControlYaw += CurrentStep.TailFollowupYaw;
		}
		const int32 Segment = ResumeTicks > 0 && Elapsed(ResumeTicks) >= 0.0 ? 1 : 0;
		if (!FParse::Param(FCommandLine::Get(), TEXT("GaspTraceNoHitch")) &&
			PhaseSeconds >= 31.3 && PhaseSeconds < 32.0 && HitchSegment != Segment && Phase != TEXT("late_wait_run"))
		{
			HitchSegment = Segment;
			bHitchThisFrame = true;
			FPlatformProcess::Sleep(0.150f);
		}
		// Change input after a real moving touchdown, so this exercises an already selected Run landing.
		// The old schedule released/changed gait in flight and only tested Stand/Walk selection at touchdown.
		if (GroundInputPhase != Phase)
		{
			GroundInputPhase = Phase;
			bGroundInputSawAirborne = false;
			GroundInputTouchdownElapsed = -1.0f;
		}
		if (Phase.EndsWith(TEXT("run_land_stop")) || Phase.EndsWith(TEXT("run_land_walk")))
		{
			const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
			bGroundInputSawAirborne |= Movement->IsFalling();
			if (bGroundInputSawAirborne && Movement->IsMovingOnGround())
			{
				GroundInputTouchdownElapsed = FMath::Max(GroundInputTouchdownElapsed, 0.0f) + DeltaSeconds;
				if (GroundInputTouchdownElapsed >= 0.1f)
				{
					CurrentStep.Scale = Phase.EndsWith(TEXT("run_land_stop")) ? 0.0f : 0.5f;
				}
			}
		}
		PC->SetControlRotation(FRotator(0.0, CurrentStep.ControlYaw, 0.0));
		Character->ConsumeMovementInputVector();
		Character->AddMovementInput(CurrentStep.Direction, CurrentStep.Scale);
		if (CurrentStep.bJump && !bWasJump) Character->Jump();
		if (!CurrentStep.bJump && bWasJump) Character->StopJumping();
		if (CurrentStep.bCrouch != bWasCrouch) { if (CurrentStep.bCrouch) Character->Crouch(); else Character->UnCrouch(); }
		bWasJump = CurrentStep.bJump;
		bWasCrouch = CurrentStep.bCrouch;
	}

	void Trace(UWorld* World, ELevelTick TickType, float DeltaSeconds)
	{
		if (World != FindPlayWorld() || !bReady) return;
		for (TActorIterator<ARpgCharacter> It(World); It; ++It)
		{
			ARpgCharacter* Character = *It;
			USkeletalMeshComponent* Mesh = Character->GetMesh();
			URpgAnimInstance* Anim = Mesh ? Cast<URpgAnimInstance>(Mesh->GetAnimInstance()) : nullptr;
			APlayerState* PS = Character->GetPlayerState();
			if (!Anim || !PS) continue;
			// No worker-owned graph, property or pose reads occur before this existing engine join.
			Mesh->HandleExistingParallelEvaluationTask(true, true);
			FAnimNode_MotionMatching* MM = FindNode<FAnimNode_MotionMatching>(Anim);
			FAnimNode_OffsetRootBone* Offset = FindNode<FAnimNode_OffsetRootBone>(Anim);
			FString TrajectoryNow = TEXT("unavailable"), TrajectoryFuture = TEXT("unavailable");
			if (const FStructProperty* P = FindFProperty<FStructProperty>(Anim->GetClass(), TEXT("LocomotionTrajectory"));
				P && P->Struct == FTransformTrajectory::StaticStruct())
			{
				const FTransformTrajectory& Trajectory = *P->ContainerPtrToValuePtr<FTransformTrajectory>(Anim);
				if (!Trajectory.Samples.IsEmpty())
				{
					TrajectoryNow = FString::SanitizeFloat(Trajectory.GetSampleAtTime(0.0f).Facing.Rotator().Yaw);
					TrajectoryFuture = FString::SanitizeFloat(Trajectory.GetSampleAtTime(0.5f).Facing.Rotator().Yaw);
				}
			}
			const FVector Position = Character->GetActorLocation();
			float LeftContact = 0.0f, RightContact = 0.0f;
			// These are the existing authored contact names from RpgFootPlacementTypes.cpp.
			const FString LeftContactText = Anim->GetCurveValue(TEXT("contact_l"), LeftContact) ? LexToString(LeftContact) : TEXT("unavailable");
			const FString RightContactText = Anim->GetCurveValue(TEXT("contact_r"), RightContact) ? LexToString(RightContact) : TEXT("unavailable");
			const URpgCharacterMovementComponent* CMC = Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement());
			const FObjectPropertyBase* TurnAssetProperty = FindFProperty<FObjectPropertyBase>(Anim->GetClass(), TEXT("TurnInPlaceSelectedAsset"));
			const UObject* LatchedTurnAsset = TurnAssetProperty ? TurnAssetProperty->GetObjectPropertyValue_InContainer(Anim) : nullptr;
			const bool bTurnClip = MM && MM->GetAnimAsset() &&
				(PropertyText(Anim, TEXT("CurrentMotionMatchingDatabaseRole")).Contains(TEXT("TurnInPlace")) ||
				(PropertyText(Anim, TEXT("TurnInPlaceState")) == TEXT("Active") && LatchedTurnAsset == MM->GetAnimAsset()));
			if (bCaptureScreenshots && SubjectPlayerId != INDEX_NONE && PS->GetPlayerId() == SubjectPlayerId)
			{
				CaptureScreenshot(*World, *Character, MM);
			}
			if (SubjectPlayerId != INDEX_NONE && PS->GetPlayerId() == SubjectPlayerId && CMC)
			{
				bSawSubjectMove |= Character->GetVelocity().Size2D() > 20.0f;
				bSawSubjectAirborne |= CMC->IsFalling() && Phase.Contains(TEXT("jump"));
				bSawSubjectLand |= bSawSubjectAirborne && CMC->IsMovingOnGround();
				const bool bStationaryTurn = bTurnClip && Character->GetVelocity().Size2D() <= 3.0f;
				bSawSubjectCombatTurn |= bStationaryTurn && Character->GetRotationMode() == ERpgCharacterRotationMode::CombatStrafe;
				bSawSubjectAimTurn |= bStationaryTurn && Character->GetRotationMode() == ERpgCharacterRotationMode::Aim;
			}
			TArray<FString> Base{
				Role, LexToString(FPlatformProcess::GetCurrentProcessId()), LexToString(Fps), LexToString(DeltaSeconds),
				LexToString(FDateTime::UtcNow().GetTicks()), Phase, LexToString(PhaseSeconds), LexToString(World->GetTimeSeconds()),
				Character->GetPathName(), LexToString(PS->GetPlayerId()), PS->GetPlayerName(), LexToString(static_cast<int32>(Character->GetLocalRole())),
				LexToString(Character->IsLocallyControlled()), LexToString(Position.X), LexToString(Position.Y), LexToString(Position.Z),
				LexToString(Character->GetActorRotation().Yaw), LexToString(Mesh->GetComponentRotation().Yaw),
				LexToString(Mesh->GetSocketTransform(TEXT("root"), RTS_World).Rotator().Yaw),
				Offset ? LexToString(Offset->GetOffsetRootRotation().Rotator().Yaw) : TEXT("unavailable"),
				TrajectoryNow, TrajectoryFuture, LexToString(Character->GetVelocity().Size2D()),
				CMC ? LexToString(CMC->GetCurrentAcceleration().Size2D()) : TEXT("unavailable"),
				LexToString(static_cast<int32>(Character->GetRotationMode())), PropertyText(Anim, TEXT("LocomotionGait")),
				PropertyText(Anim, TEXT("AnimationHistoryResetCount")), CMC ? LexToString(CMC->GetClientCorrectionReceivedCountForTests()) : TEXT("unavailable"),
				PropertyText(Anim, TEXT("CurrentMotionMatchingDatabaseRole")), PropertyText(Anim, TEXT("CurrentMotionMatchingInterruptMode")),
				PropertyText(Anim, TEXT("bCurrentMotionMatchingResultIsContinuingPose")), PropertyText(Anim, TEXT("TurnInPlaceState")),
				PropertyText(Anim, TEXT("bLandingGroundSearchReleased")), MM ? TEXT("present") : TEXT("unavailable"),
				MM ? GetPathNameSafe(MM->GetAnimAsset()) : TEXT("unavailable"), MM ? LexToString(MM->GetCurrentAssetTime()) : TEXT("unavailable"),
				MM ? LexToString(MM->GetMotionMatchingState().ElapsedPoseSearchTime) : TEXT("unavailable")};
			float Remaining = 1.0f;
			const int32 Count = MM ? MM->AnimPlayers.Num() : 0;
			for (int32 Index = 0; Index < FMath::Max(1, Count); ++Index)
			{
				TArray<FString> Row = Base;
				if (Index < Count)
				{
					const FBlendStackAnimPlayer& Player = MM->AnimPlayers[Index];
					const float Alpha = Index == Count - 1 ? 1.0f : Player.GetBlendInWeight();
					Row.Append({LexToString(Index), GetPathNameSafe(Player.GetAnimationAsset()), LexToString(Player.GetCurrentAssetTime()),
						LexToString(Player.GetPlayRate()), LexToString(Remaining * Alpha), TEXT("unavailable")});
					Remaining *= 1.0f - Alpha;
					if (Player.GetAnimationAsset()) ++SelectedClipRows;
				}
				else Row.Append({TEXT("-1"), TEXT("unavailable"), TEXT("unavailable"), TEXT("unavailable"), TEXT("unavailable"), TEXT("unavailable")});
				Row.Append({LexToString(CurrentStep.Scale), LexToString(CurrentStep.ControlYaw), LexToString(bTailTriggered), LexToString(bHitchThisFrame)});
				Row.Append({PropertyText(Anim, TEXT("TurnInPlaceQueryAngle")), PropertyText(Anim, TEXT("TurnInPlaceAccumulatedYaw")),
					PropertyText(Anim, TEXT("TurnInPlaceStateElapsed")), PropertyText(Anim, TEXT("JumpPhase")),
					PropertyText(Anim, TEXT("LandingStateElapsed")), LeftContactText, RightContactText, PropertyText(Anim, TEXT("FootPlacementAlpha"))});
				Row.Add(SearchDatabasesText(MM, Anim));
				const float ClipLength = MM ? MM->GetCurrentAssetLength() : 0.0f;
				Row.Add(ClipLength > UE_SMALL_NUMBER ? LexToString(MM->GetCurrentAssetTime() / ClipLength) : TEXT("unavailable"));
				Row.Add(PropertyText(Anim, TEXT("bTurnInPlaceUsingRootFeedback")));
				Row.Append({LexToString(Anim->GetUpdateCounter().Get()), LexToString(Mesh->GetBoneTransformRevisionNumber()), LexToString(GFrameCounter)});
				for (FString& Cell : Row) Cell = Csv(Cell);
				Buffer += FString::Join(Row, TEXT(",")) + TEXT("\n");
			}
		}
		if (Buffer.Len() > 256 * 1024) Flush();
	}

	bool Exists(const TCHAR* Name) const { return IFileManager::Get().FileExists(*(Directory / Name)); }
	void WriteMarker(const FString& Name, const FString& Value)
	{
		if (!FFileHelper::SaveStringToFile(Value, *(Directory / Name))) { Test->AddError(TEXT("Cannot write session marker")); bFailed = true; }
	}
	void WriteEpoch(const TCHAR* Name) { WriteMarker(Name, LexToString((FDateTime::UtcNow() + FTimespan::FromSeconds(2.0)).GetTicks())); }
	void ReadEpoch(const TCHAR* Name, int64& Value)
	{
		if (Value != 0 || !Exists(Name)) return;
		FString Text;
		if (FFileHelper::LoadFileToString(Text, *(Directory / Name)) && !LexTryParseString(Value, *Text)) Value = 0;
	}
	void ReadPlayerId(const TCHAR* ProcessRole, int32& Value)
	{
		if (Value != INDEX_NONE) return;
		const FString Name = FString(ProcessRole) + TEXT(".ready");
		FString Text;
		if (Exists(*Name) && FFileHelper::LoadFileToString(Text, *(Directory / Name)))
		{
			FParse::Value(*Text, TEXT("player_id="), Value);
		}
	}
	static double Elapsed(int64 Epoch) { return (FDateTime::UtcNow() - FDateTime(Epoch)).GetTotalSeconds(); }
	void CaptureScreenshot(UWorld& World, ARpgCharacter& Subject, FAnimNode_MotionMatching* MM)
	{
		APlayerController* PC = World.GetFirstPlayerController();
		UGameViewportClient* Viewport = World.GetGameInstance() ? World.GetGameInstance()->GetGameViewportClient() : nullptr;
		if (!PC || !Viewport || !Viewport->Viewport) return;
		// Reuse the real character camera on every peer; optional projection checks prevent recording the wrong target.
		if (PC->GetViewTarget() != &Subject) { PC->SetViewTarget(&Subject); return; }
		const int32 Segment = ResumeTicks > 0 && Elapsed(ResumeTicks) >= 0.0 ? 1 : 0;
		if (!(Phase.StartsWith(TEXT("before_late/")) || Phase.StartsWith(TEXT("after_late/")))) return;
		static constexpr double Times[] = {2.4, 4.4, 6.4, 8.65, 11.65, 16.4, 18.4, 20.4, 31.7, 32.25, 35.5, 40.4};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Times); ++Index)
		{
			const uint32 Bit = 1u << Index;
			if ((ScreenshotMasks[Segment] & Bit) != 0 || PhaseSeconds < Times[Index]) continue;
			const FIntPoint Size = Viewport->Viewport->GetSizeXY();
			FVector2D Feet, Head;
			const bool bProjected = PC->ProjectWorldLocationToScreen(Subject.GetActorLocation() - FVector(0, 0, 90), Feet, true) &&
				PC->ProjectWorldLocationToScreen(Subject.GetActorLocation() + FVector(0, 0, 95), Head, true) &&
				Feet.X >= 0 && Feet.X < Size.X && Feet.Y >= 0 && Feet.Y < Size.Y &&
				Head.X >= 0 && Head.X < Size.X && Head.Y >= 0 && Head.Y < Size.Y;
			if ((!bProjected || FScreenshotRequest::IsScreenshotRequested()) && PhaseSeconds < Times[Index] + 0.75) return;
			ScreenshotMasks[Segment] |= Bit;
			const FString Stem = FString::Printf(TEXT("Screens/%s_s%d_%02d"), *Role, Segment, Index);
			IFileManager::Get().MakeDirectory(*(Directory / TEXT("Screens")), true);
			const FString Status = bProjected && !FScreenshotRequest::IsScreenshotRequested() ? TEXT("requested") : TEXT("skipped_no_projection_or_busy_viewport");
			WriteMarker(Stem + TEXT(".txt"), FString::Printf(TEXT("status=%s\nphase=%s\nphase_seconds=%.6f\nutc_ticks=%lld\nplayer_id=%d\nasset=%s\nasset_time=%.6f\nviewport=%dx%d\n"),
				*Status, *Phase, PhaseSeconds, FDateTime::UtcNow().GetTicks(), SubjectPlayerId,
				*GetPathNameSafe(MM ? MM->GetAnimAsset() : nullptr), MM ? MM->GetCurrentAssetTime() : -1.0f, Size.X, Size.Y));
			if (Status == TEXT("requested"))
			{
				const FString Path = Directory / (Stem + TEXT(".png"));
				FScreenshotRequest::RequestScreenshot(Path, false, false, false, FIntRect(), true);
				ScreenshotPaths.Add(Path);
			}
			return;
		}
	}
	void Flush()
	{
		if (Buffer.IsEmpty()) return;
		if (!CsvWriter) { Test->AddError(TEXT("Cannot open diagnostic CSV writer")); bFailed = true; return; }
		const FTCHARToUTF8 Utf8(*Buffer);
		CsvWriter->Serialize(const_cast<void*>(static_cast<const void*>(Utf8.Get())), Utf8.Length());
		CsvWriter->Flush();
		if (CsvWriter->IsError())
		{
			Test->AddError(TEXT("Cannot flush diagnostic CSV")); bFailed = true;
		}
		Buffer.Reset();
	}
	void Finish(bool bSuccess)
	{
		if (bFinished) return;
		bFinished = true;
		FWorldDelegates::OnWorldPreActorTick.Remove(PreTick);
		FWorldDelegates::OnWorldPostActorTick.Remove(PostTick);
		Flush();
		if (CsvWriter && !CsvWriter->Close()) { Test->AddError(TEXT("Cannot close diagnostic CSV")); bFailed = true; }
		CsvWriter.Reset();
		for (const FString& Path : ScreenshotPaths)
		{
			if (!IFileManager::Get().FileExists(*Path)) Test->AddWarning(FString::Printf(TEXT("Requested screenshot was not written: %s"), *Path));
		}
		WriteMarker(Role + TEXT(".done"), bSuccess && !bFailed ? TEXT("success\n") : TEXT("failed\n"));
		GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = OriginalExperience;
		if (GameModeClass) CastChecked<ARpgGameModeBase>(GameModeClass->GetDefaultObject())->bEnableDiskPersistence = bOriginalPersistence;
		GEngine->NetDriverDefinitions = OriginalNetDrivers;
		if (IConsoleVariable* Limit = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"))) Limit->Set(OriginalFps, ECVF_SetByCode);
		GUnrealEd->RequestEndPlayMap();
	}

	FAutomationTestBase* Test;
	FString Role, Directory, CsvPath, Buffer, Phase = TEXT("startup");
	TUniquePtr<FArchive> CsvWriter;
	TArray<FString> ScreenshotPaths;
	uint32 ScreenshotMasks[2] = {0, 0};
	bool bCaptureScreenshots = false;
	int32 Fps, Port;
	int32 HostPlayerId = INDEX_NONE, SubjectPlayerId = INDEX_NONE, LatePlayerId = INDEX_NONE;
	double StartedAt = 0.0, PhaseSeconds = -1.0;
	int64 StartTicks = 0, ResumeTicks = 0, FinishTicks = 0;
	uint64 SelectedClipRows = 0;
	bool bStarted = false, bTravelRequested = false, bReady = false, bFailed = false;
	bool bFinished = false, bCaptureComplete = false, bCaptureSucceeded = false;
	bool bSawSubjectMove = false, bSawSubjectAirborne = false, bSawSubjectLand = false;
	bool bSawSubjectCombatTurn = false, bSawSubjectAimTurn = false;
	bool bTailTriggered = false, bHitchThisFrame = false;
	FString GroundInputPhase;
	bool bGroundInputSawAirborne = false;
	float GroundInputTouchdownElapsed = -1.0f;
	int32 HitchSegment = INDEX_NONE;
	bool bWasJump = false, bWasCrouch = false, bOriginalPersistence = true;
	float OriginalFps = 0.0f;
	FInputStep CurrentStep;
	FPrimaryAssetId OriginalExperience;
	UClass* GameModeClass = nullptr;
	TArray<FNetDriverDefinition> OriginalNetDrivers;
	TSet<TWeakObjectPtr<UNetDriver>> ConfiguredDrivers;
	TSet<TWeakObjectPtr<ARpgCharacter>> Positioned;
	TWeakObjectPtr<ARpgGaspNetworkFloorFixture> Floor;
	FDelegateHandle PreTick, PostTick;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgGaspMultiProcessDiagnosticTest,
	"SurvivalRpg.Network.GaspMultiProcess.DiagnosticRole",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgGaspMultiProcessDiagnosticTest::RunTest(const FString& Parameters)
{
	FString Role, Directory;
	int32 Fps = 60, Port = 17877;
	if (!FParse::Value(FCommandLine::Get(), TEXT("GaspProcessRole="), Role))
	{
		AddInfo(TEXT("Opt-in multi-process diagnostics skipped; use the launch harness with GaspProcessRole and GaspTraceDir."));
		return true;
	}
	FParse::Value(FCommandLine::Get(), TEXT("GaspTraceDir="), Directory);
	FParse::Value(FCommandLine::Get(), TEXT("GaspTraceFPS="), Fps);
	FParse::Value(FCommandLine::Get(), TEXT("GaspTracePort="), Port);
	Directory = FPaths::ConvertRelativePathToFull(Directory);
	FPaths::NormalizeDirectoryName(Directory);
	FPaths::CollapseRelativeDirectories(Directory);
	// ProjectSavedDir follows -UserDir in isolated editor processes. Validate against physical project roots instead.
	FString SavedRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Saved"));
	FString IntermediateRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Intermediate"));
	FPaths::NormalizeDirectoryName(SavedRoot);
	FPaths::NormalizeDirectoryName(IntermediateRoot);
	const bool bAllowedOutput = (Directory + TEXT("/")).StartsWith(SavedRoot + TEXT("/"), ESearchCase::IgnoreCase) ||
		(Directory + TEXT("/")).StartsWith(IntermediateRoot + TEXT("/"), ESearchCase::IgnoreCase);
	if (!(Role == TEXT("server") || Role == TEXT("owner") || Role == TEXT("late")) ||
		!bAllowedOutput || !(Fps == 15 || Fps == 30 || Fps == 60 || Fps == 120) || Port < 1024 || Port > 65535)
	{
		AddError(FString::Printf(TEXT("Invalid role/FPS/port or trace directory: role=%s fps=%d port=%d directory=%s saved_root=%s"), *Role, Fps, Port, *Directory, *SavedRoot));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(RpgGaspMultiProcessDiagnostics::FSession(this, Role, Directory, Fps, Port));
	return true;
}

#endif
