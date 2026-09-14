// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Network/RpgCombatNetworkTestTypes.h"
#include "Animation/AnimInstance.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "EnhancedActionKeyMapping.h"
#include "EnhancedInputLibrary.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameplayTagContainer.h"
#include "InputKeyEventArgs.h"
#include "InputMappingContext.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/StringBuilder.h"
#include "MoverSimulationTypes.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Animation/RpgRuntimeRetargetProfile.h"
#include "SurvivalRpg/Camera/RpgCameraComponent.h"
#include "SurvivalRpg/Camera/RpgCameraMode.h"
#include "SurvivalRpg/Core/Character/RpgMoverPawn.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnGameplayComponent.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceManagerComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Development/RpgDeveloperSettings.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Input/RpgInputConfig.h"

namespace RpgGaspMoverIntegrationTests
{
	constexpr TCHAR ExperiencePath[] = TEXT("/Game/SurvivalRpg/System/Experiences/RpgGaspMoverExperience.RpgGaspMoverExperience_C");
	constexpr TCHAR PawnDataPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Mover/RPG/DA_PawnData_GaspMover.DA_PawnData_GaspMover");
	constexpr TCHAR PawnClassPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Mover/RPG/BP_RpgGasp_Mover.BP_RpgGasp_Mover_C");
	constexpr TCHAR AnimClassPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Mover/RPG/ABP_RpgGasp_Mover.ABP_RpgGasp_Mover_C");
	constexpr TCHAR GameModePath[] = TEXT("/Game/SurvivalRpg/Maps/Test/GaspMover/BP_Rpg_GaspMoverTestGameMode.BP_Rpg_GaspMoverTestGameMode_C");
	constexpr TCHAR MeshPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin");
	FPrimaryAssetId ExperienceId()
	{
		return FPrimaryAssetId(URpgExperienceDefinition::StaticClass()->GetFName(), TEXT("RpgGaspMoverExperience"));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgGaspMoverCompositionTest,
	"SurvivalRpg.GASP.Mover.AssetComposition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgGaspMoverCompositionTest::RunTest(const FString& Parameters)
{
	using namespace RpgGaspMoverIntegrationTests;
	UClass* ExperienceClass = LoadClass<URpgExperienceDefinition>(nullptr, ExperiencePath);
	const URpgExperienceDefinition* Experience = ExperienceClass ? ExperienceClass->GetDefaultObject<URpgExperienceDefinition>() : nullptr;
	const URpgPawnData* PawnData = LoadObject<URpgPawnData>(nullptr, PawnDataPath);
	UClass* PawnClass = LoadClass<ARpgMoverPawn>(nullptr, PawnClassPath);
	UClass* AnimClass = LoadClass<UAnimInstance>(nullptr, AnimClassPath);
	UClass* GameModeClass = LoadClass<ARpgGameModeBase>(nullptr, GameModePath);
	if (!TestNotNull(TEXT("Mover Experience loads"), Experience) || !TestNotNull(TEXT("Mover PawnData loads"), PawnData)
		|| !TestNotNull(TEXT("Concrete Mover Blueprint uses the RPG pawn foundation"), PawnClass)
		|| !TestNotNull(TEXT("Project-owned Mover AnimBP loads"), AnimClass)
		|| !TestNotNull(TEXT("Isolated Mover GameMode loads"), GameModeClass)) return false;
	TestTrue(TEXT("Experience and PawnData compose the Mover Blueprint"), Experience->DefaultPawnData == PawnData && PawnData->PawnClass == PawnClass);
	TestFalse(TEXT("Mover is an independent pawn movement stack"), PawnClass->IsChildOf(ACharacter::StaticClass()));
	const ARpgMoverPawn* Defaults = PawnClass->GetDefaultObject<ARpgMoverPawn>();
	TestTrue(TEXT("Network Prediction owns movement replication"), Defaults->GetIsReplicated() && !Defaults->IsReplicatingMovement());
	TestNotNull(TEXT("PawnExtension remains the replicated PawnData and ASC foundation"), URpgPawnExtensionComponent::FindPawnExtensionComponent(Defaults));
	TestNotNull(TEXT("PawnGameplay retains RPG player and camera initialization"), URpgPawnGameplayComponent::FindPawnGameplayComponent(Defaults));
	TestTrue(TEXT("PawnData selects an RPG camera"), PawnData->DefaultCameraMode && PawnData->DefaultCameraMode->IsChildOf(URpgCameraMode::StaticClass()));
	TestNotNull(TEXT("Mover receives the existing RPG equipment foundation"), Defaults->FindComponentByClass<URpgEquipmentManagerComponent>());
	if (TestNotNull(TEXT("RPG UI and interaction input is composed separately from source movement"), PawnData->InputConfig.Get()))
	{
		for (const TCHAR* MovementTag : { TEXT("InputTag.Move"), TEXT("InputTag.Look.Mouse"), TEXT("InputTag.Look.Stick"),
			TEXT("InputTag.Crouch"), TEXT("InputTag.Jump"), TEXT("InputTag.StopJump"), TEXT("InputTag.AutoRun") })
			TestNull(FString::Printf(TEXT("Source Mover input retains ownership of %s"), MovementTag), PawnData->InputConfig->FindNativeInputActionForTag(
				FGameplayTag::RequestGameplayTag(MovementTag), false));
	}
	if (TestNotNull(TEXT("Mover exposes an optional designer-owned retarget profile"), PawnData->RuntimeRetargetProfile.Get()))
	{
		TestNull(TEXT("UEFN remains the default visible mesh"), PawnData->RuntimeRetargetProfile->TargetMesh.Get());
		TestNull(TEXT("The default profile selects no alternate skeleton"), PawnData->RuntimeRetargetProfile->Retargeter.Get());
		TestNotNull(TEXT("Optional presentation has a configured AnimBP"), PawnData->RuntimeRetargetProfile->RetargetAnimClass.Get());
	}
	const ARpgGameModeBase* Mode = GameModeClass->GetDefaultObject<ARpgGameModeBase>();
	const ARpgGameModeBase* Production = GetDefault<ARpgGameModeBase>();
	TestFalse(TEXT("Authored test GameMode disables disk persistence"), Mode->bEnableDiskPersistence);
	TestTrue(TEXT("Authored test slots and offline identity are separate from production"), Mode->WorldSaveSlotName != Production->WorldSaveSlotName
		&& Mode->WorldSaveBackupSlotName != Production->WorldSaveBackupSlotName
		&& Mode->WorldSaveRecoverySlotName != Production->WorldSaveRecoverySlotName && Mode->OfflineProfileKey != Production->OfflineProfileKey);
	return true;
}

#if ENABLE_PIE_NETWORK_TEST

namespace RpgGaspMoverIntegrationTests
{
	bool ActiveWorld(const UWorld* World)
	{
		if (!GEngine || !World) return false;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE && Context.World() == World)
				return IsValid(Context.World()) && !World->bIsTearingDown && !World->IsBeingCleanedUp();
		return false;
	}
	ARpgCombatNetworkFloorFixture* FindFloor(UWorld* World)
	{
		if (!ActiveWorld(World)) return nullptr;
		for (TActorIterator<ARpgCombatNetworkFloorFixture> It(World); It; ++It) return *It;
		return nullptr;
	}
	/** Installed before InitGame and retained through network teardown, including failed latent checks. */
	class FScopedSaveIsolation final
	{
	public:
		~FScopedSaveIsolation() { FGameModeEvents::OnGameModeInitializedEvent().Remove(Handle); }
		void Start()
		{
			Prefix = TEXT("SurvivalRpg_MoverAutomation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
			Handle = FGameModeEvents::OnGameModeInitializedEvent().AddRaw(this, &FScopedSaveIsolation::OnInitialized);
		}
		bool IsIsolated(UWorld* World) const
		{
			if (!ActiveWorld(World)) return false;
			if (World->GetNetMode() == NM_Client) return World->GetAuthGameMode() == nullptr;
			const ARpgGameModeBase* Mode = World->GetAuthGameMode<ARpgGameModeBase>();
			return Mode && !Mode->bEnableDiskPersistence && Mode->WorldSaveSlotName.StartsWith(Prefix)
				&& Mode->WorldSaveBackupSlotName == Mode->WorldSaveSlotName + TEXT("_Backup")
				&& Mode->WorldSaveRecoverySlotName == Mode->WorldSaveSlotName + TEXT("_Recovery") && Mode->OfflineProfileKey == Prefix;
		}
	private:
		void OnInitialized(AGameModeBase* Initialized)
		{
			ARpgGameModeBase* Mode = Cast<ARpgGameModeBase>(Initialized);
			if (!Mode || !Mode->GetWorld() || Mode->GetWorld()->WorldType != EWorldType::PIE) return;
			Mode->bEnableDiskPersistence = false;
			Mode->WorldSaveSlotName = FString::Printf(TEXT("%s_%u"), *Prefix, Mode->GetUniqueID());
			Mode->WorldSaveBackupSlotName = Mode->WorldSaveSlotName + TEXT("_Backup");
			Mode->WorldSaveRecoverySlotName = Mode->WorldSaveSlotName + TEXT("_Recovery");
			Mode->OfflineProfileKey = Prefix;

			// The empty CQTest map must support the first host pawn before clients finish joining.
			// These are ordinary spawn points and replicated world collision, with no pawn relocation.
			UWorld* World = Mode->GetWorld();
			FActorSpawnParameters Spawn;
			Spawn.ObjectFlags |= RF_Transient;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			World->SpawnActor<ARpgCombatNetworkFloorFixture>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
			for (int32 Index = 0; Index < 3; ++Index)
				World->SpawnActor<APlayerStart>(FVector(0.0, (Index - 1) * 500.0, 120.0), FRotator::ZeroRotator, Spawn);
		}
		FString Prefix;
		FDelegateHandle Handle;
	};
	APawn* LocalPawn(UWorld* World)
	{
		APlayerController* PC = ActiveWorld(World) ? World->GetFirstPlayerController() : nullptr;
		return PC ? PC->GetPawn() : nullptr;
	}
	APawn* FindPawn(UWorld* World, int32 PlayerId)
	{
		const AGameStateBase* State = ActiveWorld(World) ? World->GetGameState() : nullptr;
		if (!State || PlayerId == INDEX_NONE) return nullptr;
		for (APlayerState* Player : State->PlayerArray)
			if (Player && Player->GetPlayerId() == PlayerId) return Player->GetPawn();
		return nullptr;
	}
	UCharacterMoverComponent* Mover(const APawn* Pawn) { return Pawn ? Pawn->FindComponentByClass<UCharacterMoverComponent>() : nullptr; }
	USkeletalMeshComponent* Mesh(const APawn* Pawn)
	{
		const UCharacterMoverComponent* Movement = Mover(Pawn);
		return Movement ? Movement->GetPrimaryVisualComponent<USkeletalMeshComponent>() : nullptr;
	}
	bool Ready(UWorld* World, APawn* Pawn)
	{
		static const FGameplayTag GameplayReady = FGameplayTag::RequestGameplayTag(TEXT("InitState.GameplayReady"));
		const AGameStateBase* State = ActiveWorld(World) ? World->GetGameState() : nullptr;
		const URpgExperienceManagerComponent* Experience = State ? State->FindComponentByClass<URpgExperienceManagerComponent>() : nullptr;
		const URpgPawnExtensionComponent* Extension = URpgPawnExtensionComponent::FindPawnExtensionComponent(Pawn);
		const URpgPawnGameplayComponent* Gameplay = URpgPawnGameplayComponent::FindPawnGameplayComponent(Pawn);
		const ARpgPlayerState* Player = Pawn ? Pawn->GetPlayerState<ARpgPlayerState>() : nullptr;
		const URpgAbilitySystemComponent* ASC = Extension ? Extension->GetRpgAbilitySystemComponent() : nullptr;
		const USkeletalMeshComponent* Visual = Mesh(Pawn);
		return Experience && Experience->IsExperienceLoaded() && Experience->GetCurrentExperienceChecked()->GetPrimaryAssetId() == ExperienceId()
			&& Pawn && Pawn->GetClass()->GetPathName() == PawnClassPath && Player && Extension && Gameplay && ASC && Visual
			&& Extension->HasReachedInitState(GameplayReady)
			&& Gameplay->HasReachedInitState(GameplayReady)
			&& Extension->GetPawnData<URpgPawnData>() == Player->GetPawnData<URpgPawnData>()
			&& Extension->GetPawnData<URpgPawnData>() && Extension->GetPawnData<URpgPawnData>()->GetPathName() == PawnDataPath
			&& ASC == Player->GetRpgAbilitySystemComponent() && ASC->GetOwnerActor() == Player && ASC->GetAvatarActor() == Pawn
			&& ASC->AbilityActorInfo.IsValid() && ASC->AbilityActorInfo->SkeletalMeshComponent.Get() == Visual
			&& Visual->GetSkeletalMeshAsset() && Visual->GetSkeletalMeshAsset()->GetPathName() == MeshPath
			&& Visual->GetAnimInstance() && Visual->GetAnimInstance()->GetClass()->GetPathName() == AnimClassPath
			&& (!Pawn->IsLocallyControlled() || Gameplay->IsReadyToBindInputs());
	}
	bool CameraReady(APawn* Pawn)
	{
		const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
		const URpgCameraComponent* Camera = Pawn ? Pawn->FindComponentByClass<URpgCameraComponent>() : nullptr;
		const URpgPawnExtensionComponent* Extension = URpgPawnExtensionComponent::FindPawnExtensionComponent(Pawn);
		const URpgPawnData* Data = Extension ? Extension->GetPawnData<URpgPawnData>() : nullptr;
		return PC && PC->IsLocalController() && PC->PlayerCameraManager && PC->GetViewTarget() == Pawn && Camera && Data
			&& Camera->IsActive() && Camera->DetermineCameraModeDelegate.IsBound()
			&& Camera->DetermineCameraModeDelegate.Execute() == Data->DefaultCameraMode
			&& !PC->PlayerCameraManager->GetCameraLocation().ContainsNaN() && !PC->PlayerCameraManager->GetCameraRotation().ContainsNaN()
			&& PC->PlayerCameraManager->GetFOVAngle() > 1.0f && PC->PlayerCameraManager->GetFOVAngle() < 179.0f;
	}
	void ReportWorld(UWorld* World, const TCHAR* Stage)
	{
		if (!ActiveWorld(World)) return;
		APawn* Pawn = LocalPawn(World);
		const UCharacterMoverComponent* Movement = Mover(Pawn);
		UE_LOG(LogTemp, Display, TEXT("RpgMoverReadiness stage=%s world=%s pawn=%s ready=%d camera=%d ground=%d floor=%d position=%s velocity=%s mode=%s"),
			Stage, *World->GetPathName(), *GetNameSafe(Pawn), Ready(World, Pawn), CameraReady(Pawn), Movement && Movement->IsOnGround(),
			FindFloor(World) != nullptr, Pawn ? *Pawn->GetActorLocation().ToCompactString() : TEXT("None"),
			Movement ? *Movement->GetVelocity().ToCompactString() : TEXT("None"), Movement ? *Movement->GetMovementModeName().ToString() : TEXT("None"));
	}
	void ReportHeldInput(APawn* Pawn)
	{
		const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
		const UEnhancedPlayerInput* PlayerInput = PC ? Cast<UEnhancedPlayerInput>(PC->PlayerInput) : nullptr;
		const UInputAction* MoveAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/SurvivalRpg/Characters/GASP/Shared/Input/IA_Move.IA_Move"));
		if (Pawn && PlayerInput && MoveAction)
		{
			UE_LOG(LogTemp, Display, TEXT("RpgMoverHeldInput pawn=%s rawLeftY=%.3f rawLeft2D=%s action=%s boundValue=%s playerValue=%s inputMode=%s"),
				*Pawn->GetPathName(), PlayerInput->GetRawKeyValue(EKeys::Gamepad_LeftY), *PlayerInput->GetRawVectorKeyValue(EKeys::Gamepad_Left2D).ToCompactString(),
				*MoveAction->GetPathName(), *UEnhancedInputLibrary::GetBoundActionValue(Pawn, MoveAction).ToString(),
				*PlayerInput->GetActionValue(MoveAction).ToString(), *PlayerInput->GetCurrentInputMode().ToStringSimple());
			const UEnhancedInputLocalPlayerSubsystem* InputSubsystem = PC->GetLocalPlayer()
				? PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
			for (const TCHAR* ContextPath : {
				TEXT("/Game/SurvivalRpg/Characters/GASP/Mover/RPG/Input/IMC_RpgGasp_Mover.IMC_RpgGasp_Mover"),
				TEXT("/Game/SurvivalRpg/Input/InputMappings/IMC_Movement.IMC_Movement"),
				TEXT("/Game/SurvivalRpg/Input/InputMappings/IMC_MouseLook.IMC_MouseLook") })
			{
				const UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, ContextPath);
				int32 Priority = INDEX_NONE;
				const bool bApplied = Context && InputSubsystem && InputSubsystem->HasMappingContext(Context, Priority);
				UE_LOG(LogTemp, Display, TEXT("RpgMoverInputContext context=%s applied=%d priority=%d"), ContextPath, bApplied, Priority);
			}
			for (const FEnhancedActionKeyMapping& Mapping : PlayerInput->GetEnhancedActionMappingsView())
				if (Mapping.Action == MoveAction || Mapping.Key == EKeys::Gamepad_Left2D || Mapping.Key == EKeys::W)
					UE_LOG(LogTemp, Display, TEXT("RpgMoverResolvedMapping action=%s key=%s ignored=%d consume=%d modifiers=%d triggers=%d"),
						*GetPathNameSafe(Mapping.Action.Get()), *Mapping.Key.ToString(), Mapping.bShouldBeIgnored,
						Mapping.Action && Mapping.Action->bConsumeInput, Mapping.Modifiers.Num(), Mapping.Triggers.Num());
		}
		else UE_LOG(LogTemp, Display, TEXT("RpgMoverHeldInput pawn=%s playerInput=%s moveAction=%s"), *GetPathNameSafe(Pawn), *GetPathNameSafe(PlayerInput), *GetPathNameSafe(MoveAction));

		const int32 PlayerId = Pawn && Pawn->GetPlayerState() ? Pawn->GetPlayerState()->GetPlayerId() : INDEX_NONE;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			APawn* Peer = Context.WorldType == EWorldType::PIE ? FindPawn(Context.World(), PlayerId) : nullptr;
			const UCharacterMoverComponent* Movement = Mover(Peer);
			if (!Movement) continue;
			const FMoverTimeStep& Step = Movement->GetLastTimeStep();
			TAnsiStringBuilder<1024> InputCmd;
			Movement->GetLastInputCmd().ToString(InputCmd);
			UE_LOG(LogTemp, Display, TEXT("RpgMoverHeldSimulation pawn=%s role=%d active=%d registered=%d updated=%s root=%s serverFrame=%d baseMs=%.3f stepMs=%.3f inputCmd=%s"),
				*Peer->GetPathName(), static_cast<int32>(Peer->GetLocalRole()), Movement->IsActive(), Movement->IsRegistered(),
				*GetPathNameSafe(Movement->GetUpdatedComponent()), *GetPathNameSafe(Peer->GetRootComponent()),
				Step.ServerFrame, Step.BaseSimTimeMs, Step.StepMs, ANSI_TO_TCHAR(InputCmd.ToString()));
		}
	}

	/** Sends actual Enhanced Input key/axis events. It never writes simulation input structs, pose or velocity. */
	class FScopedInput final
	{
	public:
		~FScopedInput() { Stop(); }
		void Start(APawn* Pawn)
		{
			Stop();
			Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
			if (!Controller.IsValid()) return;
			Controller->SetIgnoreLookInput(true);
			bIgnoringLook = true;
			Handle = FWorldDelegates::OnWorldTickStart.AddRaw(this, &FScopedInput::Tick);
		}
		APawn* Pawn() const { return Controller.IsValid() ? Controller->GetPawn() : nullptr; }
		void SetMove(float Amount) { ForwardAxis = Amount; }
		void SetLook(float Amount)
		{
			LookAxis = Amount;
			if (!Controller.IsValid()) return;
			if (Amount != 0.0f && bIgnoringLook) { Controller->SetIgnoreLookInput(false); bIgnoringLook = false; }
			else if (Amount == 0.0f && !bIgnoringLook) { Controller->SetIgnoreLookInput(true); bIgnoringLook = true; }
		}
		void Jump() { Key(EKeys::SpaceBar, true); JumpFramesRemaining = 2; }
		void ToggleCrouch() { Key(EKeys::C, true); CrouchFramesRemaining = 2; }
		void Stop()
		{
			FWorldDelegates::OnWorldTickStart.Remove(Handle);
			Handle.Reset();
			Axis(EKeys::Gamepad_LeftY, 0.0f);
			Axis(EKeys::Gamepad_RightX, 0.0f);
			Key(EKeys::SpaceBar, false);
			Key(EKeys::C, false);
			if (Controller.IsValid() && bIgnoringLook) Controller->SetIgnoreLookInput(false);
			Controller.Reset();
			ForwardAxis = LookAxis = 0.0f;
			HeldMoveSeconds = 0.0f;
			bReportedHeldInput = false;
			JumpFramesRemaining = CrouchFramesRemaining = 0;
			bIgnoringLook = false;
		}
		void Report() const
		{
			APawn* Subject = Pawn();
			const UCharacterMoverComponent* Movement = Mover(Subject);
			UE_LOG(LogTemp, Display, TEXT("RpgMoverInput pawn=%s forward=%.2f look=%.2f ignoreMove=%d ignoreLook=%d W=%d position=%s velocity=%s mode=%s"),
				*GetNameSafe(Subject), ForwardAxis, LookAxis, Controller.IsValid() && Controller->IsMoveInputIgnored(),
				Controller.IsValid() && Controller->IsLookInputIgnored(), Controller.IsValid() && Controller->IsInputKeyDown(EKeys::W),
				Subject ? *Subject->GetActorLocation().ToCompactString() : TEXT("None"),
				Movement ? *Movement->GetVelocity().ToCompactString() : TEXT("None"), Movement ? *Movement->GetMovementModeName().ToString() : TEXT("None"));
		}
	private:
		void Key(FKey KeyValue, bool bPressed)
		{
			if (Controller.IsValid()) Controller->InputKey(FInputKeyEventArgs::CreateSimulated(KeyValue,
				bPressed ? IE_Pressed : IE_Released, bPressed ? 1.0f : 0.0f));
		}
		void Axis(FKey KeyValue, float Amount)
		{
			if (Controller.IsValid()) Controller->InputKey(FInputKeyEventArgs::CreateSimulated(KeyValue, IE_Axis, Amount, 1));
		}
		void Tick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
		{
			if (!Controller.IsValid() || Controller->GetWorld() != World) return;
			// Like a real stick, held axes are sampled continuously, including after a viewport focus change.
			Axis(EKeys::Gamepad_LeftY, ForwardAxis);
			Axis(EKeys::Gamepad_RightX, LookAxis);
			if (ForwardAxis != 0.0f && !bReportedHeldInput)
			{
				HeldMoveSeconds += DeltaSeconds;
				if (HeldMoveSeconds >= 1.0f)
				{
					bReportedHeldInput = true;
					ReportHeldInput(Pawn());
				}
			}
			if (JumpFramesRemaining > 0 && --JumpFramesRemaining == 0) Key(EKeys::SpaceBar, false);
			if (CrouchFramesRemaining > 0 && --CrouchFramesRemaining == 0) Key(EKeys::C, false);
		}
		TWeakObjectPtr<APlayerController> Controller;
		FDelegateHandle Handle;
		float ForwardAxis = 0.0f, LookAxis = 0.0f;
		float HeldMoveSeconds = 0.0f;
		int32 JumpFramesRemaining = 0, CrouchFramesRemaining = 0;
		bool bIgnoringLook = false;
		bool bReportedHeldInput = false;
	};

	TArray<FQuat> LimbPose(USkeletalMeshComponent* Visual)
	{
		TArray<FQuat> Result;
		if (!Visual) return Result;
		// The public copy waits for outstanding evaluation without forcing an animation tick.
		const TArray<FTransform> Pose = Visual->GetBoneSpaceTransforms();
		for (const FName Bone : { FName(TEXT("thigh_l")), FName(TEXT("calf_r")), FName(TEXT("upperarm_l")), FName(TEXT("lowerarm_r")) })
		{
			const int32 Index = Visual->GetBoneIndex(Bone);
			if (!Pose.IsValidIndex(Index)) return {};
			Result.Add(Pose[Index].GetRotation());
		}
		return Result;
	}
	struct FObservation
	{
		TWeakObjectPtr<APawn> Pawn;
		FVector StartPosition = FVector::ZeroVector;
		TArray<FQuat> InitialPose;
		float PoseMotion = 0.0f, HeightGain = 0.0f;
		bool bMoved = false, bAirborne = false, bLanded = false;
	};
	/** Samples every peer together so short jump and pose states are not lost between latent commands. */
	class FScopedObservations final
	{
	public:
		~FScopedObservations() { Stop(); }
		void Start(int32 InPlayerId, const FVector& InExpectedDirection = FVector::ZeroVector)
		{
			Stop();
			PlayerId = InPlayerId;
			ExpectedDirection = InExpectedDirection.GetSafeNormal2D();
			Records.Reset();
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				APawn* Pawn = Context.WorldType == EWorldType::PIE ? FindPawn(Context.World(), PlayerId) : nullptr;
				if (!Pawn) continue;
				FObservation& Record = Records.FindOrAdd(Context.World());
				Record.Pawn = Pawn;
				Record.StartPosition = Pawn->GetActorLocation();
				Record.InitialPose = LimbPose(Mesh(Pawn));
			}
			Handle = FWorldDelegates::OnWorldTickEnd.AddRaw(this, &FScopedObservations::Tick);
		}
		bool AllMoved(int32 ExpectedWorlds) const
		{
			if (Records.Num() != ExpectedWorlds) return false;
			for (const auto& Entry : Records) if (!Entry.Value.bMoved) return false;
			return true;
		}
		bool AllStopped(int32 ExpectedWorlds) const
		{
			if (Records.Num() != ExpectedWorlds) return false;
			for (const auto& Entry : Records)
			{
				const UCharacterMoverComponent* Movement = Mover(Entry.Value.Pawn.Get());
				if (!Movement || !Movement->IsOnGround() || Movement->GetVelocity().Size2D() >= 5.0) return false;
			}
			return true;
		}
		bool AllCrouched(int32 ExpectedWorlds, bool bCrouched) const
		{
			if (Records.Num() != ExpectedWorlds) return false;
			for (const auto& Entry : Records)
			{
				const UCharacterMoverComponent* Movement = Mover(Entry.Value.Pawn.Get());
				if (!Movement || Movement->IsCrouching() != bCrouched || !Movement->IsOnGround()) return false;
			}
			return true;
		}
		bool AllJumpedAndLanded(int32 ExpectedWorlds) const
		{
			if (Records.Num() != ExpectedWorlds) return false;
			for (const auto& Entry : Records)
				if (!Entry.Value.bAirborne || !Entry.Value.bLanded || Entry.Value.HeightGain < 25.0f) return false;
			return true;
		}
		void Report() const
		{
			for (const auto& Entry : Records)
			{
				const FObservation& Record = Entry.Value;
				UE_LOG(LogTemp, Display, TEXT("RpgMoverObservation world=%s role=%d moved=%d poseDegrees=%.2f airborne=%d landed=%d height=%.2f"),
					*GetPathNameSafe(Entry.Key.Get()), Record.Pawn.IsValid() ? static_cast<int32>(Record.Pawn->GetLocalRole()) : -1,
					Record.bMoved, FMath::RadiansToDegrees(Record.PoseMotion), Record.bAirborne, Record.bLanded, Record.HeightGain);
			}
		}
		void Stop() { FWorldDelegates::OnWorldTickEnd.Remove(Handle); Handle.Reset(); }
	private:
		void Tick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
		{
			if (!ActiveWorld(World)) return;
			FObservation* Record = Records.Find(World);
			APawn* Pawn = Record ? Record->Pawn.Get() : nullptr;
			const UCharacterMoverComponent* Movement = Mover(Pawn);
			if (!Record || !Pawn || !Movement) return;
			const TArray<FQuat> Pose = LimbPose(Mesh(Pawn));
			if (Pose.Num() == 4 && Record->InitialPose.Num() == Pose.Num())
				for (int32 Index = 0; Index < Pose.Num(); ++Index)
					Record->PoseMotion = FMath::Max(Record->PoseMotion, static_cast<float>(Pose[Index].AngularDistance(Record->InitialPose[Index])));
			Record->bMoved |= Movement->GetVelocity().Size2D() > 100.0 && FVector::Dist2D(Record->StartPosition, Pawn->GetActorLocation()) > 150.0
				&& Record->PoseMotion > 0.15f
				&& (ExpectedDirection.IsNearlyZero() || FVector::DotProduct(Movement->GetVelocity().GetSafeNormal2D(), ExpectedDirection) > 0.85);
			Record->HeightGain = FMath::Max(Record->HeightGain, static_cast<float>(Pawn->GetActorLocation().Z - Record->StartPosition.Z));
			Record->bAirborne |= Movement->IsFalling();
			Record->bLanded |= Record->bAirborne && Record->HeightGain > 25.0f && Movement->IsOnGround();
		}
		int32 PlayerId = INDEX_NONE;
		FVector ExpectedDirection = FVector::ZeroVector;
		TMap<TWeakObjectPtr<UWorld>, FObservation> Records;
		FDelegateHandle Handle;
	};
	struct FState : FBasePIENetworkComponentState {};
	FTimespan Timeout() { return FTimespan::FromSeconds(60.0); }
}

NETWORK_TEST_CLASS(GaspMoverExperiencePIE, "SurvivalRpg.GASP.Mover")
{
	using FState = RpgGaspMoverIntegrationTests::FState;
	RpgGaspMoverIntegrationTests::FScopedSaveIsolation Isolation;
	RpgGaspMoverIntegrationTests::FScopedInput Input;
	RpgGaspMoverIntegrationTests::FScopedObservations Observations;
	FPIENetworkComponent<FState> Network{TestRunner, TestCommandBuilder, bInitializing};
	FPrimaryAssetId PreviousExperience;
	int32 SubjectId = INDEX_NONE;
	bool bConfigured = false, bHost = false;
	float InitialControlYaw = 0.0f, InitialCameraYaw = 0.0f;
	FVector InitialCameraLocation = FVector::ZeroVector;
	FVector InitialPawnLocation = FVector::ZeroVector, InitialCameraComponentLocation = FVector::ZeroVector;
	bool bCameraSnapshotTaken = false;
	FVector PreviousIdleCameraLocation = FVector::ZeroVector;
	float PreviousCameraCacheTime = -1.0f, StableCameraSince = -1.0f;
	int32 StableCameraSamples = 0;

	BEFORE_EACH()
	{
		using namespace RpgGaspMoverIntegrationTests;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{
				TestRunner->AddError(TEXT("Mover automation refuses to interrupt an existing PIE session."));
				return;
			}
		}
		UClass* GameMode = LoadClass<ARpgGameModeBase>(nullptr, GameModePath);
		ASSERT_THAT(IsNotNull(GameMode));
		if (!GameMode) return;
		Isolation.Start();
		PreviousExperience = GetDefault<URpgDeveloperSettings>()->ExperienceOverride;
		GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = ExperienceId();
		bConfigured = true;
		FNetworkComponentBuilder<FState>().WithClients(1).AsListenServer()
			.WithGameInstanceClass(FSoftClassPath(TEXT("/Game/SurvivalRpg/Core/Game/BP_Rpg_GameInstance.BP_Rpg_GameInstance_C")))
			.WithGameMode(GameMode).Build(Network);
	}
	AFTER_EACH()
	{
		if (TestRunner->HasAnyErrors())
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
				if (Context.WorldType == EWorldType::PIE)
					RpgGaspMoverIntegrationTests::ReportWorld(Context.World(), TEXT("failed_teardown"));
			if (bCameraSnapshotTaken && Input.Pawn())
			{
				APawn* Pawn = Input.Pawn();
				const APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
				const URpgCameraComponent* Camera = Pawn->FindComponentByClass<URpgCameraComponent>();
				UE_LOG(LogTemp, Display, TEXT("RpgMoverCameraFollow initialManager=%s currentManager=%s initialPawn=%s currentPawn=%s initialComponent=%s currentComponent=%s ready=%d"),
					*InitialCameraLocation.ToCompactString(), PC && PC->PlayerCameraManager ? *PC->PlayerCameraManager->GetCameraLocation().ToCompactString() : TEXT("None"),
					*InitialPawnLocation.ToCompactString(), *Pawn->GetActorLocation().ToCompactString(), *InitialCameraComponentLocation.ToCompactString(),
					Camera ? *Camera->GetComponentLocation().ToCompactString() : TEXT("None"), RpgGaspMoverIntegrationTests::CameraReady(Pawn));
			}
		}
		Input.Report();
		Observations.Report();
		Input.Stop();
		Observations.Stop();
		if (bConfigured) GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = PreviousExperience;
	}
	TEST_METHOD(RemoteInputCameraJumpCrouchAndLateJoin) { Queue(false); }
	TEST_METHOD(ListenHostInputCameraJumpCrouchAndLateJoin) { Queue(true); }

	bool IdleCameraSettled()
	{
		using namespace RpgGaspMoverIntegrationTests;
		APawn* Pawn = Input.Pawn();
		if (!CameraReady(Pawn) || !Mover(Pawn)->IsOnGround() || Mover(Pawn)->GetVelocity().Size2D() >= 5.0)
		{
			StableCameraSince = -1.0f;
			StableCameraSamples = 0;
			return false;
		}
		const APlayerCameraManager* Manager = Cast<APlayerController>(Pawn->GetController())->PlayerCameraManager;
		const float CacheTime = Manager->GetCameraCacheTime();
		if (CacheTime <= PreviousCameraCacheTime) return false;
		const FVector Location = Manager->GetCameraLocation();
		if (StableCameraSince < 0.0f || !Location.Equals(PreviousIdleCameraLocation, 1.0))
		{
			StableCameraSince = CacheTime;
			StableCameraSamples = 0;
		}
		PreviousCameraCacheTime = CacheTime;
		PreviousIdleCameraLocation = Location;
		++StableCameraSamples;
		return StableCameraSamples >= 3 && CacheTime - StableCameraSince >= 0.25f;
	}

	void VerifyPawn(UWorld* World)
	{
		using namespace RpgGaspMoverIntegrationTests;
		APawn* Pawn = FindPawn(World, SubjectId);
		ASSERT_THAT(IsTrue(Ready(World, Pawn)));
		ASSERT_THAT(IsTrue(Isolation.IsIsolated(World)));
		if (!Pawn || !Mover(Pawn)) return;
		const UCharacterMoverComponent* Movement = Mover(Pawn);
		ASSERT_THAT(IsTrue(Movement->GetUpdatedComponent() == Pawn->GetRootComponent()));
		ASSERT_THAT(IsNotNull(Movement->GetUpdatedComponent<UCapsuleComponent>()));
		ASSERT_THAT(IsTrue(Movement->BackendClass && Movement->BackendClass->GetPathName() == TEXT("/Script/Mover.MoverNetworkPredictionLiaisonComponent")));
		ASSERT_THAT(IsTrue(Movement->bSyncInputsForSimProxy));
		ASSERT_THAT(IsFalse(Pawn->IsReplicatingMovement()));
		ASSERT_THAT(IsTrue(Pawn->FindComponentByClass<USkeletalMeshComponent>() == Mesh(Pawn)));
	}
	void Queue(bool bInHost)
	{
		using namespace RpgGaspMoverIntegrationTests;
		if (!bConfigured) return;
		bHost = bInHost;
		Network.ThenServer(TEXT("Persistence is isolated before Mover pawn initialization"), [this](FState& State)
			{
				ASSERT_THAT(IsTrue(Isolation.IsIsolated(State.World)));
				ASSERT_THAT(IsNotNull(FindFloor(State.World)));
				ReportWorld(State.World, TEXT("initial"));
			})
			.ThenClients(TEXT("Record initial client composition before readiness waits"), [](FState& State) { ReportWorld(State.World, TEXT("initial")); })
			.UntilClient(TEXT("Initial client receives the floor that existed before player spawn"), 0, [](FState& State)
				{ return FindFloor(State.World) != nullptr; }, Timeout())
			.UntilClient(TEXT("Client Mover finishes PawnData, ASC, input and camera initialization"), 0, [](FState& State)
				{ return Ready(State.World, LocalPawn(State.World)) && CameraReady(LocalPawn(State.World)) && Mover(LocalPawn(State.World))->IsOnGround(); }, Timeout())
			.UntilServer(TEXT("Listen host also finishes normal Mover composition"), [](FState& State)
				{ return Ready(State.World, LocalPawn(State.World)) && CameraReady(LocalPawn(State.World)) && Mover(LocalPawn(State.World))->IsOnGround(); }, Timeout())
			.ThenClient(TEXT("Select the remote owner when testing autonomous prediction"), 0, [this](FState& State)
			{
				if (bHost) return;
				APawn* Pawn = LocalPawn(State.World);
				ASSERT_THAT(IsTrue(Pawn->GetLocalRole() == ROLE_AutonomousProxy));
				SubjectId = Pawn->GetPlayerState()->GetPlayerId();
				Input.Start(Pawn);
			})
			.ThenServer(TEXT("Select the local host when testing listen-server input"), [this](FState& State)
			{
				if (!bHost) return;
				APawn* Pawn = LocalPawn(State.World);
				ASSERT_THAT(IsTrue(Pawn->HasAuthority() && Pawn->IsLocallyControlled()));
				SubjectId = Pawn->GetPlayerState()->GetPlayerId();
				Input.Start(Pawn);
			})
			.UntilServer(TEXT("Authority has the selected initialized subject"), [this](FState& State)
				{ return Ready(State.World, FindPawn(State.World, SubjectId)); }, Timeout())
			.UntilClient(TEXT("First client has the selected initialized subject"), 0, [this](FState& State)
				{ return Ready(State.World, FindPawn(State.World, SubjectId)); }, Timeout())
			.UntilServer(TEXT("The idle camera has evaluated and settled before its movement baseline"), [this](FState&)
				{ return IdleCameraSettled(); }, FTimespan::FromSeconds(5.0))
			.ThenServer(TEXT("Drive the original Blueprint movement input on a normal floor"), [this](FState& State)
			{
				VerifyPawn(State.World);
				// A render-only offset must move the camera pivot while the collision and gameplay view stay put.
				ARpgMoverPawn* CameraPawn = CastChecked<ARpgMoverPawn>(Input.Pawn());
				UCharacterMoverComponent* CameraMover = Mover(CameraPawn);
				USceneComponent* Visual = CameraMover->GetPrimaryVisualComponent();
				ASSERT_THAT(IsNotNull(Visual));
				const FTransform OriginalVisual = Visual->GetComponentTransform();
				const FVector CollisionLocation = CameraPawn->GetActorLocation();
				const FVector GameplayView = CameraPawn->GetPawnViewLocation();
				const FTransform SmoothedActor(FRotator(0.0, 37.0, 0.0), CollisionLocation + FVector(-8.0, 3.0, 2.0));
				Visual->SetWorldTransform(CameraMover->GetBaseVisualComponentTransform() * SmoothedActor);
				const TOptional<FVector> CameraPivot = CameraPawn->GetCameraPivotLocation();
				Visual->SetWorldTransform(OriginalVisual);
				ASSERT_THAT(IsTrue(CameraPivot.IsSet()));
				ASSERT_THAT(IsTrue(CameraPivot.GetValue().Equals(SmoothedActor.GetLocation() + GameplayView - CollisionLocation, 0.001)));
				ASSERT_THAT(IsTrue(CameraPawn->GetActorLocation().Equals(CollisionLocation, 0.001)));
				ASSERT_THAT(IsTrue(CameraPawn->GetPawnViewLocation().Equals(GameplayView, 0.001)));
				InitialCameraLocation = Cast<APlayerController>(Input.Pawn()->GetController())->PlayerCameraManager->GetCameraLocation();
				InitialPawnLocation = Input.Pawn()->GetActorLocation();
				InitialCameraComponentLocation = Input.Pawn()->FindComponentByClass<URpgCameraComponent>()->GetComponentLocation();
				bCameraSnapshotTaken = true;
				Observations.Start(SubjectId);
				Input.SetMove(1.0f);
			})
			.UntilServer(TEXT("Both roles move with changing UEFN limb poses"), [this](FState&) { return Observations.AllMoved(2); }, Timeout())
			.ThenServer(TEXT("Release movement before waiting for the RPG camera to follow"), [this](FState&) { Input.SetMove(0.0f); })
			.UntilServer(TEXT("The active RPG camera follows its pawn after movement"), [this](FState&)
			{
				if (!CameraReady(Input.Pawn())) return false;
				const APlayerController* PC = Cast<APlayerController>(Input.Pawn()->GetController());
				return FVector::Dist2D(InitialCameraLocation, PC->PlayerCameraManager->GetCameraLocation()) > 75.0;
			}, FTimespan::FromSeconds(5.0))
			.UntilServer(TEXT("All roles return to supported idle"), [this](FState&) { return Observations.AllStopped(2); }, Timeout())
			.ThenServer(TEXT("Turn the real view using the original look-stick input action"), [this](FState&)
			{
				const APlayerController* PC = Cast<APlayerController>(Input.Pawn()->GetController());
				InitialControlYaw = PC->GetControlRotation().Yaw;
				InitialCameraYaw = PC->PlayerCameraManager->GetCameraRotation().Yaw;
				Input.SetLook(0.5f);
			})
			.UntilServer(TEXT("Control rotation and rendered camera respond to look input"), [this](FState&)
			{
				const APlayerController* PC = Input.Pawn() ? Cast<APlayerController>(Input.Pawn()->GetController()) : nullptr;
				return PC && CameraReady(Input.Pawn()) && FMath::Abs(FMath::FindDeltaAngleDegrees(InitialControlYaw, PC->GetControlRotation().Yaw)) > 35.0
					&& FMath::Abs(FMath::FindDeltaAngleDegrees(InitialCameraYaw, PC->PlayerCameraManager->GetCameraRotation().Yaw)) > 30.0;
			}, Timeout())
			.ThenServer(TEXT("Move again after the view turn"), [this](FState&)
			{
				Input.SetLook(0.0f);
				const APlayerController* PC = Cast<APlayerController>(Input.Pawn()->GetController());
				Observations.Start(SubjectId, FRotator(0.0f, PC->GetControlRotation().Yaw, 0.0f).Vector());
				Input.SetMove(1.0f);
			})
			.UntilServer(TEXT("Both roles move and animate in the new view direction"), [this](FState&) { return Observations.AllMoved(2); }, Timeout())
			.ThenServer(TEXT("Release movement before jumping"), [this](FState&) { Input.SetMove(0.0f); })
			.UntilServer(TEXT("Both roles settle before Space input"), [this](FState&) { return Observations.AllStopped(2); }, Timeout())
			.ThenServer(TEXT("Press Space through the original Mover input producer"), [this](FState&)
				{ Observations.Start(SubjectId); Input.Jump(); })
			.UntilServer(TEXT("Owner and other peer both observe upward travel, falling and normal landing"), [this](FState&)
				{ return Observations.AllJumpedAndLanded(2); }, Timeout())
			.ThenServer(TEXT("Toggle crouch through the original simulated input path"), [this](FState&) { Input.ToggleCrouch(); })
			.UntilServer(TEXT("Both simulation roles enter crouch"), [this](FState&) { return Observations.AllCrouched(2, true); }, Timeout())
			.ThenClientJoins()
			.UntilClient(TEXT("Late join receives PawnData, ASC, UEFN and the existing crouched simulated pawn"), 1, [this](FState& State)
			{
				APawn* Pawn = FindPawn(State.World, SubjectId);
				return Ready(State.World, Pawn) && Pawn->GetLocalRole() == ROLE_SimulatedProxy && Mover(Pawn)->IsOnGround() && Mover(Pawn)->IsCrouching();
			}, Timeout())
			.ThenClient(TEXT("Verify the late proxy uses the original Network Prediction movement stack"), 1, [this](FState& State) { VerifyPawn(State.World); })
			.ThenServer(TEXT("Uncrouch and include the new observer in live pose measurements"), [this](FState&)
				{ Observations.Start(SubjectId); Input.ToggleCrouch(); })
			.UntilServer(TEXT("All three roles return to standing"), [this](FState&) { return Observations.AllCrouched(3, false); }, Timeout())
			.ThenServer(TEXT("Move with the late observer present"), [this](FState&)
				{ Observations.Start(SubjectId); Input.SetMove(1.0f); })
			.UntilServer(TEXT("Every peer now animates and moves the same subject"), [this](FState&) { return Observations.AllMoved(3); }, Timeout())
			.ThenServer(TEXT("Release all test-owned movement input"), [this](FState&) { Input.SetMove(0.0f); })
			.UntilServer(TEXT("Mover settles on all three peers"), [this](FState&) { return Observations.AllStopped(3); }, Timeout())
			.ThenServer(TEXT("Authority and local camera retain the initialized RPG contracts"), [this](FState& State)
				{ VerifyPawn(State.World); ASSERT_THAT(IsTrue(CameraReady(Input.Pawn()))); })
			.ThenClients(TEXT("Both clients retain the same PawnData, ASC and UEFN presentation"), [this](FState& State) { VerifyPawn(State.World); });
	}
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
