// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Network/RpgCombatNetworkTestTypes.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AnimNodes/AnimNode_RetargetPoseFromMesh.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputActionValue.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "MotionWarpingComponent.h"
#include "Retargeter/IKRetargeter.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Animation/RpgRuntimeRetargetComponent.h"
#include "SurvivalRpg/Animation/RpgRuntimeRetargetProfile.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMovementComponent.h"
#include "SurvivalRpg/Core/Character/RpgDownedComponent.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Character/RpgPawnGameplayComponent.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceManagerComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Development/RpgDeveloperSettings.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentInstance.h"
#include "SurvivalRpg/Traversal/RpgGameplayAbility_Mantle.h"

namespace RpgRuntimeRetargetTests
{
	constexpr TCHAR PawnDataPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/CMC/RPG/Traversal/DA_PawnData_GaspMantle.DA_PawnData_GaspMantle");
	constexpr TCHAR CmcPawnDataPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/CMC/RPG/DA_PawnData_GaspCMC.DA_PawnData_GaspCMC");
	constexpr TCHAR GameModePath[] = TEXT("/Game/SurvivalRpg/Maps/Test/GaspMantle/BP_Rpg_GaspMantleTestGameMode.BP_Rpg_GaspMantleTestGameMode_C");
	constexpr TCHAR ObstacleClassPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/CMC/RPG/Traversal/BP_RpgMantleObstacle.BP_RpgMantleObstacle_C");
	constexpr TCHAR SourceMeshPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin");
	constexpr TCHAR TargetMeshPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UE5_Mannequins/Meshes/SKM_Manny.SKM_Manny");
	constexpr TCHAR RetargeterPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UE5_Mannequins/Rigs/RTG_UEFN_to_UE5_Mannequin.RTG_UEFN_to_UE5_Mannequin");

	FPrimaryAssetId ExperienceId()
	{
		return FPrimaryAssetId(URpgExperienceDefinition::StaticClass()->GetFName(), TEXT("RpgGaspMantleExperience"));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgRuntimeRetargetCompositionTest,
	"SurvivalRpg.GASP.RuntimeRetarget.OptionalProfileComposition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgRuntimeRetargetCompositionTest::RunTest(const FString& Parameters)
{
	using namespace RpgRuntimeRetargetTests;
	const URpgPawnData* PawnData = LoadObject<URpgPawnData>(nullptr, PawnDataPath);
	const URpgPawnData* CmcPawnData = LoadObject<URpgPawnData>(nullptr, CmcPawnDataPath);
	if (!TestNotNull(TEXT("Traversal PawnData loads"), PawnData)
		|| !TestNotNull(TEXT("Accepted CMC PawnData loads"), CmcPawnData)
		|| !TestNotNull(TEXT("Optional designer profile is composed"), PawnData->RuntimeRetargetProfile.Get())) return false;
	const URpgRuntimeRetargetProfile* Profile = PawnData->RuntimeRetargetProfile;
	TestNull(TEXT("UEFN remains the default visible character"), Profile->TargetMesh.Get());
	TestNull(TEXT("No target skeleton is implicitly selected"), Profile->Retargeter.Get());
	TestNotNull(TEXT("Optional retarget presentation has a configured AnimBP"), Profile->RetargetAnimClass.Get());
	TestNull(TEXT("Existing CMC composition does not acquire an optional target"), CmcPawnData->RuntimeRetargetProfile.Get());
	if (!TestNotNull(TEXT("Traversal pawn class loads"), PawnData->PawnClass.Get())) return false;
	const ARpgCharacter* Pawn = Cast<ARpgCharacter>(PawnData->PawnClass->GetDefaultObject());
	if (!TestNotNull(TEXT("Traversal keeps the RPG character"), Pawn)) return false;
	TestTrue(TEXT("UEFN remains the gameplay mesh"), Pawn->GetMesh()->GetSkeletalMeshAsset()
		&& Pawn->GetMesh()->GetSkeletalMeshAsset()->GetPathName() == SourceMeshPath);
	return true;
}

#if ENABLE_PIE_NETWORK_TEST

namespace RpgRuntimeRetargetTests
{
	bool ActiveWorld(const UWorld* World)
	{
		if (!GEngine || !World) return false;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World() == World)
				return IsValid(Context.World()) && !World->bIsTearingDown && !World->IsBeingCleanedUp();
		}
		return false;
	}

	/** Installed before InitGame and retained through network teardown; never accesses a user save. */
	class FScopedSaveIsolation final
	{
	public:
		~FScopedSaveIsolation() { FGameModeEvents::OnGameModeInitializedEvent().Remove(Handle); }
		void Start()
		{
			Prefix = TEXT("SurvivalRpg_RetargetAutomation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
			Handle = FGameModeEvents::OnGameModeInitializedEvent().AddRaw(this, &FScopedSaveIsolation::OnInitialized);
		}
		bool IsIsolated(UWorld* World) const
		{
			if (!ActiveWorld(World)) return false;
			if (World->GetNetMode() == NM_Client) return World->GetAuthGameMode() == nullptr;
			const ARpgGameModeBase* Mode = World->GetAuthGameMode<ARpgGameModeBase>();
			return Mode && !Mode->bEnableDiskPersistence && Mode->WorldSaveSlotName.StartsWith(Prefix)
				&& Mode->WorldSaveBackupSlotName == Mode->WorldSaveSlotName + TEXT("_Backup")
				&& Mode->WorldSaveRecoverySlotName == Mode->WorldSaveSlotName + TEXT("_Recovery")
				&& Mode->OfflineProfileKey == Prefix;
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
		}
		FString Prefix;
		FDelegateHandle Handle;
	};

	/** Temporarily models static, named content in this one-process PIE; never calls Modify or saves the asset. */
	class FScopedDefaultProfile final
	{
	public:
		~FScopedDefaultProfile() { Restore(); }
		void Start(URpgRuntimeRetargetProfile* Profile, const URpgRuntimeRetargetProfile& Enabled)
		{
			Restore();
			DefaultProfile.Reset(Profile);
			PreviousMesh.Reset(Profile->TargetMesh.Get());
			PreviousRetargeter.Reset(Profile->Retargeter.Get());
			Profile->TargetMesh = Enabled.TargetMesh;
			Profile->Retargeter = Enabled.Retargeter;
		}
		void Restore()
		{
			if (!DefaultProfile.IsValid()) return;
			DefaultProfile->TargetMesh = PreviousMesh.Get();
			DefaultProfile->Retargeter = PreviousRetargeter.Get();
			DefaultProfile.Reset();
			PreviousMesh.Reset();
			PreviousRetargeter.Reset();
		}
	private:
		TStrongObjectPtr<URpgRuntimeRetargetProfile> DefaultProfile;
		TStrongObjectPtr<USkeletalMesh> PreviousMesh;
		TStrongObjectPtr<UIKRetargeter> PreviousRetargeter;
	};

	ARpgCharacter* LocalCharacter(UWorld* World)
	{
		const APlayerController* PC = ActiveWorld(World) ? World->GetFirstPlayerController() : nullptr;
		return PC ? PC->GetPawn<ARpgCharacter>() : nullptr;
	}
	ARpgCharacter* FindCharacter(UWorld* World, int32 PlayerId)
	{
		if (!ActiveWorld(World) || PlayerId == INDEX_NONE) return nullptr;
		for (TActorIterator<ARpgCharacter> It(World); It; ++It)
			if (It->GetPlayerState() && It->GetPlayerState()->GetPlayerId() == PlayerId) return *It;
		return nullptr;
	}
	URpgRuntimeRetargetComponent* Retarget(ARpgCharacter* Character)
	{
		return Character ? Character->FindComponentByClass<URpgRuntimeRetargetComponent>() : nullptr;
	}
	bool Ready(UWorld* World, ARpgCharacter* Character)
	{
		const AGameStateBase* State = ActiveWorld(World) ? World->GetGameState() : nullptr;
		const URpgExperienceManagerComponent* Manager = State ? State->FindComponentByClass<URpgExperienceManagerComponent>() : nullptr;
		const URpgPawnData* PawnData = LoadObject<URpgPawnData>(nullptr, PawnDataPath);
		return Manager && Manager->IsExperienceLoaded() && Manager->GetCurrentExperienceChecked()->GetPrimaryAssetId() == ExperienceId()
			&& Character && Character->GetPlayerState() && Character->GetRpgAbilitySystemComponent()
			&& Character->GetMesh()->GetAnimInstance() && Retarget(Character) && PawnData && PawnData->RuntimeRetargetProfile
			&& Retarget(Character)->GetRetargetProfile() == PawnData->RuntimeRetargetProfile;
	}
	bool Grounded(ARpgCharacter* Character)
	{
		return Character && Character->GetCharacterMovement()->IsMovingOnGround() && Character->GetVelocity().Size2D() < 5.0;
	}
	bool VisibleTarget(ARpgCharacter* Character)
	{
		const URpgRuntimeRetargetComponent* Component = Retarget(Character);
		const USkeletalMeshComponent* Target = Component ? Component->GetRetargetMesh() : nullptr;
		return Target && Target->IsRegistered() && Target->IsVisible() && !Target->bHiddenInGame
			&& Target->GetAnimInstance() && Target->GetNumComponentSpaceTransforms() > 0;
	}

	/** Feeds normal owning-pawn input; the look lease only excludes unrelated physical mouse deltas. */
	class FScopedInput final
	{
	public:
		~FScopedInput() { Stop(); }
		void Start(ARpgCharacter* Pawn)
		{
			Stop();
			Character = Pawn;
			Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
			if (Controller.IsValid()) Controller->SetIgnoreLookInput(true);
			Handle = FWorldDelegates::OnWorldTickStart.AddRaw(this, &FScopedInput::Tick);
		}
		void Stop()
		{
			FWorldDelegates::OnWorldTickStart.Remove(Handle);
			Handle.Reset();
			if (Controller.IsValid()) Controller->SetIgnoreLookInput(false);
			Controller.Reset();
			Character.Reset();
			bMoving = false;
		}
		bool bMoving = false;
	private:
		void Tick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
		{
			if (bMoving && Character.IsValid() && Character->GetWorld() == World && Character->IsLocallyControlled())
				Character->AddMovementInput(FVector::ForwardVector, 1.0f);
		}
		TWeakObjectPtr<ARpgCharacter> Character;
		TWeakObjectPtr<APlayerController> Controller;
		FDelegateHandle Handle;
	};

	/** Local bone rotations cannot be changed merely by moving the actor or attaching a static target mesh. */
	TArray<FQuat> ReadLimbPose(USkeletalMeshComponent* Mesh)
	{
		TArray<FQuat> Result;
		if (!Mesh) return Result;
		// This public accessor joins any pending evaluation and copies the completed pose; it does not force animation updates.
		const TArray<FTransform> Pose = Mesh->GetBoneSpaceTransforms();
		for (const FName Bone : { FName(TEXT("thigh_l")), FName(TEXT("calf_r")), FName(TEXT("upperarm_l")), FName(TEXT("lowerarm_r")) })
		{
			const int32 Index = Mesh->GetBoneIndex(Bone);
			if (!Pose.IsValidIndex(Index)) return {};
			Result.Add(Pose[Index].GetRotation());
		}
		return Result;
	}
	float PoseDifference(const TArray<FQuat>& Before, const TArray<FQuat>& After)
	{
		if (Before.Num() != 4 || After.Num() != Before.Num()) return 0.0f;
		float Difference = 0.0f;
		for (int32 Index = 0; Index < Before.Num(); ++Index)
			Difference = FMath::Max(Difference, static_cast<float>(Before[Index].AngularDistance(After[Index])));
		return Difference;
	}

	struct FObservation
	{
		TWeakObjectPtr<ARpgCharacter> Character;
		TWeakObjectPtr<USkeletalMeshComponent> Source;
		TWeakObjectPtr<USkeletalMesh> SourceAsset;
		TWeakObjectPtr<UClass> SourceAnimClass;
		TArray<FQuat> InitialSourcePose, InitialTargetPose;
		FVector InitialLocation = FVector::ZeroVector;
		EVisibilityBasedAnimTickOption OriginalTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		bool bOriginalVisible = false, bOriginalHidden = false, bOriginalURO = false;
		float SourcePoseMotion = 0.0f, TargetPoseMotion = 0.0f;
		bool bGameplayMeshChanged = false, bTargetPlayedGameplayMontage = false;
		bool bSawSourceMontage = false, bSawRootMotion = false, bSawCollisionLease = false;
		bool bSawMovingPose = false;
		bool bObserveUnreadyTarget = false, bUnreadyTargetReplacedSource = false;
		int32 UnreadyWorldTicks = 0;
	};

	class FScopedObservations final
	{
	public:
		~FScopedObservations() { Stop(); }
		void Add(ARpgCharacter* Character)
		{
			FObservation& Record = Records.FindOrAdd(Character->GetWorld());
			Record.Character = Character;
			Record.Source = Character->GetMesh();
			Record.SourceAsset = Character->GetMesh()->GetSkeletalMeshAsset();
			Record.SourceAnimClass = Character->GetMesh()->GetAnimClass();
			Record.OriginalTickOption = Character->GetMesh()->VisibilityBasedAnimTickOption;
			Record.bOriginalVisible = Character->GetMesh()->IsVisible();
			Record.bOriginalHidden = Character->GetMesh()->bHiddenInGame;
			Record.bOriginalURO = Character->GetMesh()->bEnableUpdateRateOptimizations;
			if (!Handle.IsValid()) Handle = FWorldDelegates::OnWorldTickEnd.AddRaw(this, &FScopedObservations::Tick);
		}
		void ResetPoseSamples()
		{
			for (auto& Entry : Records)
			{
				FObservation& Record = Entry.Value;
				ARpgCharacter* Character = Record.Character.Get();
				if (!Character) continue;
				Record.InitialLocation = Character->GetActorLocation();
				Record.InitialSourcePose = ReadLimbPose(Character->GetMesh());
				Record.InitialTargetPose = ReadLimbPose(Retarget(Character)->GetRetargetMesh());
				Record.SourcePoseMotion = Record.TargetPoseMotion = 0.0f;
				Record.bSawMovingPose = false;
			}
		}
		const FObservation& Get(UWorld* World) const { return Records.FindChecked(World); }
		void ObserveUnreadyTarget(UWorld* World) { Records.FindChecked(World).bObserveUnreadyTarget = true; }
		bool AllSawMovingPose(int32 ExpectedWorlds) const
		{
			if (Records.Num() != ExpectedWorlds) return false;
			for (const auto& Entry : Records) if (!Entry.Value.bSawMovingPose) return false;
			return true;
		}
		void Stop()
		{
			FWorldDelegates::OnWorldTickEnd.Remove(Handle);
			Handle.Reset();
		}
	private:
		void Tick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
		{
			FObservation* Record = Records.Find(World);
			ARpgCharacter* Character = Record ? Record->Character.Get() : nullptr;
			if (!Character || !ActiveWorld(World)) return;
			USkeletalMeshComponent* Source = Character->GetMesh();
			if (Record->bObserveUnreadyTarget)
			{
				++Record->UnreadyWorldTicks;
				Record->bUnreadyTargetReplacedSource |= !Source->IsVisible() || Source->bHiddenInGame || VisibleTarget(Character);
			}
			URpgAbilitySystemComponent* ASC = Character->GetRpgAbilitySystemComponent();
			Record->bGameplayMeshChanged |= Source != Record->Source.Get() || Source->GetSkeletalMeshAsset() != Record->SourceAsset.Get()
				|| Source->GetAnimClass() != Record->SourceAnimClass.Get() || !ASC || !ASC->AbilityActorInfo.IsValid()
				|| ASC->AbilityActorInfo->SkeletalMeshComponent.Get() != Source;
			if (!VisibleTarget(Character)) return;
			USkeletalMeshComponent* Target = Retarget(Character)->GetRetargetMesh();
			Record->SourcePoseMotion = FMath::Max(Record->SourcePoseMotion, PoseDifference(Record->InitialSourcePose, ReadLimbPose(Source)));
			Record->TargetPoseMotion = FMath::Max(Record->TargetPoseMotion, PoseDifference(Record->InitialTargetPose, ReadLimbPose(Target)));
			Record->bSawMovingPose |= Character->GetVelocity().Size2D() > 150.0
				&& FVector::Dist2D(Record->InitialLocation, Character->GetActorLocation()) > 100.0
				&& Record->SourcePoseMotion > 0.15f && Record->TargetPoseMotion > 0.15f;
			const URpgCharacterMovementComponent* Movement = Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement());
			if (Movement && Movement->GetMantleCollisionComponent())
			{
				Record->bSawCollisionLease = true;
				UAnimMontage* Montage = Source->GetAnimInstance()->GetCurrentActiveMontage();
				Record->bSawSourceMontage |= Montage && (Character->GetLocalRole() == ROLE_SimulatedProxy || ASC->GetCurrentMontage() == Montage);
				Record->bSawRootMotion |= Character->IsPlayingRootMotion();
				Record->bTargetPlayedGameplayMontage |= Montage && Target->GetAnimInstance()->Montage_IsPlaying(Montage);
			}
		}
		TMap<TWeakObjectPtr<UWorld>, FObservation> Records;
		FDelegateHandle Handle;
	};

	UPrimitiveComponent* Obstacle(UWorld* World)
	{
		if (!ActiveWorld(World)) return nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetClass()->GetPathName() != ObstacleClassPath) continue;
			TInlineComponentArray<UPrimitiveComponent*> Components(*It);
			for (UPrimitiveComponent* Component : Components)
				if (Component->IsQueryCollisionEnabled() && Component->GetCollisionResponseToChannel(ECC_GameTraceChannel1) == ECR_Block) return Component;
		}
		return nullptr;
	}
	bool CompletedMantle(ARpgCharacter* Character)
	{
		if (!Grounded(Character)) return false;
		const UPrimitiveComponent* Block = Obstacle(Character->GetWorld());
		const URpgCharacterMovementComponent* Movement = Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement());
		const UMotionWarpingComponent* Warping = Character->FindComponentByClass<UMotionWarpingComponent>();
		if (!Block || !Movement || Movement->GetMantleCollisionComponent() || !Warping
			|| Warping->FindWarpTarget(TEXT("FrontLedge"))) return false;
		const FBox Bounds = Block->Bounds.GetBox();
		const FVector Position = Character->GetActorLocation();
		return Movement->CurrentFloor.HitResult.GetComponent() == Block && Position.X > Bounds.Min.X && Position.X < Bounds.Max.X
			&& Position.Y > Bounds.Min.Y && Position.Y < Bounds.Max.Y
			&& FMath::Abs(Position.Z - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - Bounds.Max.Z) < 8.0;
	}
	bool EquipmentOnSource(ARpgCharacter* Character)
	{
		const URpgEquipmentManagerComponent* Equipment = Character ? Character->GetEquipmentManagerComponent() : nullptr;
		const URpgEquipmentInstance* Item = Equipment ? Equipment->GetEquipmentInstanceInSlot(ERpgEquipmentSlot::MainHand) : nullptr;
		if (!Item || Item->GetSpawnedActors().IsEmpty()) return false;
		for (const AActor* Actor : Item->GetSpawnedActors())
			if (!Actor || !Actor->GetRootComponent() || Actor->GetRootComponent()->GetAttachParent() != Character->GetMesh()) return false;
		return true;
	}
	struct FState : FBasePIENetworkComponentState
	{
		ARpgCombatNetworkFloorFixture* Floor = nullptr;
		TWeakObjectPtr<ARpgCharacter> PawnBeforeDeath;
		TWeakObjectPtr<USkeletalMeshComponent> FollowerBeforeDeath;
		TWeakObjectPtr<URpgAbilitySystemComponent> PersistentASC;
	};
	ARpgPlayerState* FindPlayerState(UWorld* World, int32 PlayerId)
	{
		const AGameStateBase* State = ActiveWorld(World) ? World->GetGameState() : nullptr;
		if (!State) return nullptr;
		for (APlayerState* Player : State->PlayerArray)
			if (Player && Player->GetPlayerId() == PlayerId) return Cast<ARpgPlayerState>(Player);
		return nullptr;
	}
	ARpgCharacter* AutomaticallyRetargetedRespawn(const FState& State, int32 PlayerId)
	{
		const ARpgPlayerState* Player = FindPlayerState(State.World, PlayerId);
		ARpgCharacter* Character = Player ? Player->GetPawn<ARpgCharacter>() : nullptr;
		const URpgHealthComponent* Health = URpgHealthComponent::FindHealthComponent(Character);
		const URpgAbilitySystemComponent* ASC = Character ? Character->GetRpgAbilitySystemComponent() : nullptr;
		return Ready(State.World, Character) && Character != State.PawnBeforeDeath.Get() && VisibleTarget(Character)
			&& !Player->IsWaitingForRespawn() && Health && Health->GetHealth() > 0.0f && !Health->IsDeadOrDying()
			&& ASC == State.PersistentASC.Get() && ASC == Player->GetRpgAbilitySystemComponent()
			&& ASC->GetAvatarActor() == Character && ASC->GetOwnerActor() == Player && ASC->AbilityActorInfo.IsValid()
			&& ASC->AbilityActorInfo->SkeletalMeshComponent.Get() == Character->GetMesh()
			&& Character->GetMesh()->GetSkeletalMeshAsset()
			&& Character->GetMesh()->GetSkeletalMeshAsset()->GetPathName() == SourceMeshPath ? Character : nullptr;
	}
	FTimespan Timeout() { return FTimespan::FromSeconds(60.0); }
}

NETWORK_TEST_CLASS(GaspRuntimeRetargetPIE, "SurvivalRpg.GASP.RuntimeRetarget")
{
	using FState = RpgRuntimeRetargetTests::FState;
	RpgRuntimeRetargetTests::FScopedSaveIsolation Isolation;
	RpgRuntimeRetargetTests::FScopedInput Input;
	RpgRuntimeRetargetTests::FScopedObservations Observations;
	RpgRuntimeRetargetTests::FScopedDefaultProfile DefaultProfileOverride;
	TStrongObjectPtr<URpgRuntimeRetargetProfile> EnabledProfile;
	FPIENetworkComponent<FState> Network{TestRunner, TestCommandBuilder, bInitializing};
	FPrimaryAssetId OriginalExperience;
	int32 SubjectId = INDEX_NONE;
	bool bConfigured = false;

	BEFORE_EACH()
	{
		using namespace RpgRuntimeRetargetTests;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{
				TestRunner->AddError(TEXT("Runtime retarget automation refuses to interrupt an existing PIE session."));
				return;
			}
		}
		const URpgPawnData* PawnData = LoadObject<URpgPawnData>(nullptr, PawnDataPath);
		UClass* GameMode = LoadClass<ARpgGameModeBase>(nullptr, GameModePath);
		ASSERT_THAT(IsNotNull(PawnData));
		ASSERT_THAT(IsNotNull(GameMode));
		if (!PawnData || !PawnData->RuntimeRetargetProfile || !GameMode) return;
		EnabledProfile.Reset(NewObject<URpgRuntimeRetargetProfile>(GetTransientPackage()));
		EnabledProfile->TargetMesh = LoadObject<USkeletalMesh>(nullptr, TargetMeshPath);
		EnabledProfile->Retargeter = LoadObject<UIKRetargeter>(nullptr, RetargeterPath);
		EnabledProfile->RetargetAnimClass = PawnData->RuntimeRetargetProfile->RetargetAnimClass;
		ASSERT_THAT(IsNotNull(EnabledProfile->TargetMesh.Get()));
		ASSERT_THAT(IsNotNull(EnabledProfile->Retargeter.Get()));
		ASSERT_THAT(IsNotNull(EnabledProfile->RetargetAnimClass.Get()));
		Isolation.Start();
		OriginalExperience = GetDefault<URpgDeveloperSettings>()->ExperienceOverride;
		GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = ExperienceId();
		bConfigured = true;
		FNetworkComponentBuilder<FState>().WithClients(1).AsListenServer()
			.WithGameInstanceClass(FSoftClassPath(TEXT("/Game/SurvivalRpg/Core/Game/BP_Rpg_GameInstance.BP_Rpg_GameInstance_C")))
			.WithGameMode(GameMode).Build(Network);
	}
	AFTER_EACH()
	{
		Input.Stop();
		Observations.Stop();
		DefaultProfileOverride.Restore();
		if (bConfigured) GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = OriginalExperience;
	}

	void EnableLocally(ARpgCharacter* Character)
	{
		using namespace RpgRuntimeRetargetTests;
		URpgRuntimeRetargetComponent* Component = Retarget(Character);
		ASSERT_THAT(IsNotNull(Component));
		if (!Component) return;
		ASSERT_THAT(IsNull(Component->GetRetargetMesh()));
		ASSERT_THAT(IsTrue(Character->GetMesh()->IsVisible() && !Character->GetMesh()->bHiddenInGame));
		Observations.Add(Character);
		// ApplyProfile is local presentation configuration. This deliberately makes no profile replication claim.
		ASSERT_THAT(IsTrue(Component->ApplyProfile(EnabledProfile.Get())));
	}
	void VerifyActiveContract(ARpgCharacter* Character)
	{
		using namespace RpgRuntimeRetargetTests;
		ASSERT_THAT(IsTrue(VisibleTarget(Character)));
		if (!VisibleTarget(Character)) return;
		USkeletalMeshComponent* Target = Retarget(Character)->GetRetargetMesh();
		ASSERT_THAT(IsTrue(Target != Character->GetMesh() && Target->GetAttachParent() == Character->GetMesh()));
		ASSERT_THAT(IsTrue(Target->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !Target->IsSimulatingPhysics()));
		ASSERT_THAT(IsTrue(Target->PrimaryComponentTick.TickGroup == TG_PostPhysics));
		ASSERT_THAT(IsTrue(Retarget(Character)->GetRetargeter() == EnabledProfile->Retargeter));
		ASSERT_THAT(IsFalse(Observations.Get(Character->GetWorld()).bGameplayMeshChanged));
	}
	void VerifyFallback(ARpgCharacter* Character)
	{
		using namespace RpgRuntimeRetargetTests;
		URpgRuntimeRetargetComponent* Component = Retarget(Character);
		const FObservation& Record = Observations.Get(Character->GetWorld());
		TWeakObjectPtr<USkeletalMeshComponent> OldTarget = Component->GetRetargetMesh();
		ASSERT_THAT(IsTrue(Component->ApplyProfile(nullptr)));
		ASSERT_THAT(IsNull(Component->GetRetargetMesh()));
		ASSERT_THAT(IsTrue(!OldTarget.IsValid() || !OldTarget->IsRegistered()));
		ASSERT_THAT(IsTrue(Character->GetMesh() == Record.Source.Get()
			&& Character->GetMesh()->GetSkeletalMeshAsset() == Record.SourceAsset.Get()
			&& Character->GetMesh()->GetAnimClass() == Record.SourceAnimClass.Get()));
		ASSERT_THAT(IsTrue(Character->GetMesh()->IsVisible() == Record.bOriginalVisible
			&& Character->GetMesh()->bHiddenInGame == Record.bOriginalHidden
			&& Character->GetMesh()->VisibilityBasedAnimTickOption == Record.OriginalTickOption
			&& Character->GetMesh()->bEnableUpdateRateOptimizations == Record.bOriginalURO));
		TStrongObjectPtr<URpgRuntimeRetargetProfile> Invalid(NewObject<URpgRuntimeRetargetProfile>(GetTransientPackage()));
		Invalid->TargetMesh = EnabledProfile->TargetMesh;
		Invalid->RetargetAnimClass = EnabledProfile->RetargetAnimClass;
		ASSERT_THAT(IsFalse(Component->ApplyProfile(Invalid.Get()))); // No retargeter: remain on a usable source mesh.
		ASSERT_THAT(IsNull(Component->GetRetargetMesh()));
		ASSERT_THAT(IsTrue(Character->GetMesh()->IsVisible() == Record.bOriginalVisible && !Record.bGameplayMeshChanged));
		Component->RefreshFromPawnData();
		const URpgPawnData* PawnData = LoadObject<URpgPawnData>(nullptr, PawnDataPath);
		ASSERT_THAT(IsTrue(Component->GetRetargetProfile() == PawnData->RuntimeRetargetProfile));
		ASSERT_THAT(IsNull(Component->GetRetargetMesh()));
		ASSERT_THAT(IsTrue(EquipmentOnSource(Character)));
	}
	void BeginMissingSourceProbe(ARpgCharacter* Character)
	{
		using namespace RpgRuntimeRetargetTests;
		URpgRuntimeRetargetComponent* Component = Retarget(Character);
		ASSERT_THAT(IsTrue(Component->ApplyProfile(EnabledProfile.Get())));
		USkeletalMeshComponent* Target = Component->GetRetargetMesh();
		ASSERT_THAT(IsNotNull(Target));
		if (!Target) return;
		// Join any registration-time evaluation before changing this instance, without ticking it or touching its CDO.
		Target->GetBoneSpaceTransforms();
		UAnimInstance* Animation = Target->GetAnimInstance();
		const IAnimClassInterface* AnimClass = Animation ? IAnimClassInterface::GetFromClass(Animation->GetClass()) : nullptr;
		ASSERT_THAT(IsNotNull(AnimClass));
		if (!AnimClass) return;
		bool bChangedInstanceNode = false;
		for (const FStructProperty* Property : AnimClass->GetAnimNodeProperties())
		{
			if (!Property || Property->Struct != FAnimNode_RetargetPoseFromMesh::StaticStruct()) continue;
			FAnimNode_RetargetPoseFromMesh* Node = Property->ContainerPtrToValuePtr<FAnimNode_RetargetPoseFromMesh>(Animation);
			Node->RetargetFrom = ERetargetSourceMode::CustomSkeletalMeshComponent;
			Node->SourceMeshComponent.Reset();
			bChangedInstanceNode = true;
		}
		ASSERT_THAT(IsTrue(bChangedInstanceNode));
		ASSERT_THAT(IsTrue(Character->GetMesh()->IsVisible() && !Target->IsVisible()));
		Observations.ObserveUnreadyTarget(Character->GetWorld());
	}
	void CaptureBeforeDeath(FState& State)
	{
		using namespace RpgRuntimeRetargetTests;
		ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
		ASSERT_THAT(IsTrue(Ready(State.World, Character) && VisibleTarget(Character)));
		if (!Character) return;
		State.PawnBeforeDeath = Character;
		State.FollowerBeforeDeath = Retarget(Character)->GetRetargetMesh();
		State.PersistentASC = Character->GetRpgAbilitySystemComponent();
	}
	void VerifyRespawn(FState& State)
	{
		using namespace RpgRuntimeRetargetTests;
		ARpgCharacter* Character = AutomaticallyRetargetedRespawn(State, SubjectId);
		ASSERT_THAT(IsNotNull(Character));
		ASSERT_THAT(IsFalse(State.PawnBeforeDeath.IsValid()));
		ASSERT_THAT(IsTrue(!State.FollowerBeforeDeath.IsValid() || !State.FollowerBeforeDeath->IsRegistered()));
		if (!Character) return;
		ASSERT_THAT(IsTrue(Retarget(Character)->GetRetargetMesh()->GetAttachParent() == Character->GetMesh()));
		ASSERT_THAT(IsTrue(Retarget(Character)->GetRetargetMesh()->GetSkeletalMeshAsset() == EnabledProfile->TargetMesh));
		ASSERT_THAT(IsTrue(Isolation.IsIsolated(State.World)));
	}

	TEST_METHOD(RemotePoseLateJoinTraversalEquipmentAndFallback)
	{
		using namespace RpgRuntimeRetargetTests;
		if (!bConfigured) return;
		Network.ThenServer(TEXT("Persistence is isolated before any pawn initializes"), [this](FState& State)
			{ ASSERT_THAT(IsTrue(Isolation.IsIsolated(State.World))); })
			.SpawnAndReplicate<ARpgCombatNetworkFloorFixture, &FState::Floor>(Timeout())
			.UntilClient(TEXT("Default traversal pawn is ready without an optional target"), 0, [](FState& State)
				{ return Ready(State.World, LocalCharacter(State.World)) && Grounded(LocalCharacter(State.World)); }, Timeout())
			.ThenClient(TEXT("Enable transient Manny only for the owning client's presentation"), 0, [this](FState& State)
			{
				ARpgCharacter* Character = LocalCharacter(State.World);
				SubjectId = Character->GetPlayerState()->GetPlayerId();
				ASSERT_THAT(IsTrue(Character->GetLocalRole() == ROLE_AutonomousProxy));
				Input.Start(Character);
				EnableLocally(Character);
			})
			.UntilServer(TEXT("Listen server has the same authoritative pawn"), [this](FState& State)
				{ return Ready(State.World, FindCharacter(State.World, SubjectId)); }, Timeout())
			.ThenServer(TEXT("Enable the server's local visual target independently"), [this](FState& State)
				{ EnableLocally(FindCharacter(State.World, SubjectId)); })
			.UntilClient(TEXT("Owner target completes its first pose before becoming visible"), 0, [](FState& State)
				{ return VisibleTarget(LocalCharacter(State.World)); }, Timeout())
			.UntilServer(TEXT("Server target also completes its initial pose"), [this](FState& State)
				{ return VisibleTarget(FindCharacter(State.World, SubjectId)); }, Timeout())
			.ThenClient(TEXT("Capture limb poses and drive normal CMC input"), 0, [this](FState& State)
				{ VerifyActiveContract(LocalCharacter(State.World)); Observations.ResetPoseSamples(); Input.bMoving = true; })
			.UntilServer(TEXT("Both owner and server retargeted limbs animate under actual movement"), [this](FState& State)
			{
				return Observations.AllSawMovingPose(2);
			}, Timeout())
			.ThenClient(TEXT("Release movement before late join"), 0, [this](FState&) { Input.bMoving = false; })
			.UntilServer(TEXT("Remote source returns to supported idle"), [this](FState& State)
				{ return Grounded(FindCharacter(State.World, SubjectId)); }, Timeout())
			.ThenClientJoins()
			.UntilClient(TEXT("Late join composes the existing pawn with UEFN default presentation"), 1, [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				return Isolation.IsIsolated(State.World) && Ready(State.World, Character) && Character->GetLocalRole() == ROLE_SimulatedProxy;
			}, Timeout())
			.ThenClient(TEXT("Late observer opts into the same transient visual profile locally"), 1, [this](FState& State)
				{ EnableLocally(FindCharacter(State.World, SubjectId)); })
			.UntilClient(TEXT("Late proxy's retarget graph produces its first visible pose"), 1, [this](FState& State)
				{ return VisibleTarget(FindCharacter(State.World, SubjectId)); }, Timeout())
			.ThenClient(TEXT("Restart real owner input with the late observer present"), 0, [this](FState&)
				{ Observations.ResetPoseSamples(); Input.bMoving = true; })
			.UntilClient(TEXT("Late proxy animates retargeted limbs from replicated source movement"), 1, [this](FState& State)
				{ return Observations.Get(State.World).bSawMovingPose; }, Timeout())
			.ThenClient(TEXT("Release movement before the traversal fixture"), 0, [this](FState&) { Input.bMoving = false; })
			.UntilServer(TEXT("Owner is stationary before prepared mantle entry"), [this](FState& State)
				{ return Grounded(FindCharacter(State.World, SubjectId)); }, Timeout())
			.ThenServer(TEXT("Equip an existing RPG sword and prepare a replicated source cube"), [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				UClass* Sword = LoadClass<URpgEquipmentDefinition>(nullptr, TEXT("/GF_Combat_Core/Equipment/Weapons/ED_BasicSword.ED_BasicSword_C"));
				ASSERT_THAT(IsNotNull(Sword));
				if (!Sword) return;
				URpgEquipmentManagerComponent* Equipment = Character->GetEquipmentManagerComponent();
				Equipment->UnequipItemInSlot(ERpgEquipmentSlot::MainHand);
				ASSERT_THAT(IsNotNull(Equipment->EquipItemInSlot(Sword, ERpgEquipmentSlot::MainHand)));
				UClass* BlockClass = LoadClass<AActor>(nullptr, ObstacleClassPath);
				ASSERT_THAT(IsNotNull(BlockClass));
				if (!BlockClass) return;
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				ASSERT_THAT(IsNotNull(State.World->SpawnActor<AActor>(BlockClass,
					FTransform(FRotator::ZeroRotator, FVector(1000.0, 1800.0, 0.0), FVector(4.0, 4.0, 1.0)), Params)));
				const UPrimitiveComponent* Block = Obstacle(State.World);
				ASSERT_THAT(IsNotNull(Block));
				if (!Block) return;
				const FBox Bounds = Block->Bounds.GetBox();
				// Fixture placement only. Subsequent movement is the real contextual input -> predicted GAS -> CMC root-motion path.
				ASSERT_THAT(IsTrue(Character->TeleportTo(FVector(Bounds.Min.X - 100.0, Bounds.GetCenter().Y,
					Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.5), FRotator::ZeroRotator)));
				APlayerController* PC = Cast<APlayerController>(Character->GetController());
				PC->SetControlRotation(FRotator::ZeroRotator);
				PC->ClientSetRotation(FRotator::ZeroRotator, true);
				Character->GetCharacterMovement()->StopMovementImmediately();
				Character->ForceNetUpdate();
			})
			.UntilClient(TEXT("Owner receives prepared geometry, entry heading and equipment on the source"), 0, [this](FState& State)
			{
				ARpgCharacter* Character = LocalCharacter(State.World);
				const UPrimitiveComponent* Block = Obstacle(State.World);
				if (!Block || !Grounded(Character) || !EquipmentOnSource(Character)) return false;
				const FBox Bounds = Block->Bounds.GetBox();
				return FMath::Abs(Character->GetActorLocation().X - (Bounds.Min.X - 100.0)) < 12.0
					&& FMath::Abs(Character->GetActorLocation().Y - Bounds.GetCenter().Y) < 12.0
					&& FMath::Abs(FRotator::NormalizeAxis(Character->GetControlRotation().Yaw)) < 1.0;
			}, Timeout())
			.UntilClient(TEXT("Late proxy receives the equipped source socket"), 1, [this](FState& State)
				{ return EquipmentOnSource(FindCharacter(State.World, SubjectId)); }, Timeout())
			.ThenClient(TEXT("Start an actual contextual mantle with retargeting enabled"), 0, [this](FState& State)
			{
				Observations.ResetPoseSamples();
				URpgPawnGameplayComponent* Gameplay = URpgPawnGameplayComponent::FindPawnGameplayComponent(LocalCharacter(State.World));
				ASSERT_THAT(IsNotNull(Gameplay));
				if (Gameplay) Gameplay->Input_Jump(FInputActionValue(true));
			})
			.UntilServer(TEXT("Authoritative source root motion completes the real mantle on the cube"), [this](FState& State)
				{ return CompletedMantle(FindCharacter(State.World, SubjectId)); }, Timeout())
			.UntilClient(TEXT("Owner completes the same mantle"), 0, [](FState& State)
				{ return CompletedMantle(LocalCharacter(State.World)); }, Timeout())
			.UntilClient(TEXT("Late observer also resolves the supported mantle endpoint"), 1, [this](FState& State)
				{ return CompletedMantle(FindCharacter(State.World, SubjectId)); }, Timeout())
			.ThenClient(TEXT("Release the contextual jump input"), 0, [](FState& State)
				{ URpgPawnGameplayComponent::FindPawnGameplayComponent(LocalCharacter(State.World))->Input_StopJump(FInputActionValue(false)); })
			.ThenServer(TEXT("Server preserves source montage authority, equipment and optional-pose lifecycle"), [this](FState& State)
				{ VerifyTraversalAndClear(FindCharacter(State.World, SubjectId)); })
			.ThenClients(TEXT("Owner and late observer preserve the same source contracts and restore default presentation"), [this](FState& State)
				{ VerifyTraversalAndClear(FindCharacter(State.World, SubjectId)); })
			.ThenClient(TEXT("Valid assets with an unresolved live retarget source must not hide UEFN"), 0, [this](FState& State)
				{ BeginMissingSourceProbe(LocalCharacter(State.World)); })
			.UntilClient(TEXT("Allow three world frames for the invalid node's finalize and deferred cleanup"), 0, [this](FState& State)
				{ return Observations.Get(State.World).UnreadyWorldTicks >= 3; }, Timeout())
			.ThenClient(TEXT("Unready target never replaced the visible gameplay mesh"), 0, [this](FState& State)
			{
				ARpgCharacter* Character = LocalCharacter(State.World);
				ASSERT_THAT(IsFalse(Observations.Get(State.World).bUnreadyTargetReplacedSource));
				ASSERT_THAT(IsTrue(Character->GetMesh()->IsVisible() && !Character->GetMesh()->bHiddenInGame));
				ASSERT_THAT(IsNull(Retarget(Character)->GetRetargetMesh()));
				ASSERT_THAT(IsTrue(Retarget(Character)->ApplyProfile(nullptr)));
				Retarget(Character)->RefreshFromPawnData();
				Observations.Stop();
			})
			.ThenServer(TEXT("Temporarily configure the named default profile in memory for normal respawn composition"), [this](FState& State)
			{
				const URpgPawnData* PawnData = LoadObject<URpgPawnData>(nullptr, PawnDataPath);
				// Test-scoped content only: the RAII owner restores both references even if a later latent assertion fails.
				DefaultProfileOverride.Start(const_cast<URpgRuntimeRetargetProfile*>(PawnData->RuntimeRetargetProfile.Get()), *EnabledProfile.Get());
				Retarget(FindCharacter(State.World, SubjectId))->RefreshFromPawnData();
			})
			.ThenClients(TEXT("Existing peers refresh the same static PawnData appearance"), [this](FState& State)
				{ Retarget(FindCharacter(State.World, SubjectId))->RefreshFromPawnData(); })
			.UntilServer(TEXT("Server shows its default-profile follower before death"), [this](FState& State)
				{ return VisibleTarget(FindCharacter(State.World, SubjectId)); }, Timeout())
			.UntilClient(TEXT("Owner shows its default-profile follower before death"), 0, [](FState& State)
				{ return VisibleTarget(LocalCharacter(State.World)); }, Timeout())
			.UntilClient(TEXT("Late observer shows its default-profile follower before death"), 1, [this](FState& State)
				{ return VisibleTarget(FindCharacter(State.World, SubjectId)); }, Timeout())
			.ThenClients(TEXT("Capture current client pawns, followers and persistent ASCs"), [this](FState& State) { CaptureBeforeDeath(State); })
			.ThenServer(TEXT("Enter final player death through authoritative health and downed components"), [this](FState& State)
			{
				CaptureBeforeDeath(State);
				ARpgCharacter* Character = State.PawnBeforeDeath.Get();
				URpgHealthComponent* Health = URpgHealthComponent::FindHealthComponent(Character);
				ASSERT_THAT(IsNotNull(Health));
				if (!Health) return;
				Health->DamageSelfDestruct(false);
				if (URpgDownedComponent* Downed = URpgDownedComponent::FindDownedComponent(Character); Downed && Downed->IsDowned())
					Downed->ForceDeathFromDowned();
			})
			.UntilClient(TEXT("Owner receives the authoritative respawn delay"), 0, [this](FState& State)
			{
				const ARpgPlayerState* Player = FindPlayerState(State.World, SubjectId);
				return Player && Player->IsWaitingForRespawn() && Player->CanRespawnNow();
			}, Timeout())
			.ThenClient(TEXT("Request respawn once through the actual owner controller RPC"), 0, [this](FState& State)
			{
				ARpgPlayerController* Controller = Cast<ARpgPlayerController>(State.World->GetFirstPlayerController());
				ASSERT_THAT(IsNotNull(Controller));
				if (!Controller) return;
				ASSERT_THAT(IsTrue(Controller->IsLocalController() && !Controller->HasAuthority()));
				Controller->RequestRespawn();
			})
			.UntilClient(TEXT("New autonomous pawn automatically composes its follower from PawnData"), 0, [this](FState& State)
				{ return AutomaticallyRetargetedRespawn(State, SubjectId) == LocalCharacter(State.World) && LocalCharacter(State.World); }, Timeout())
			.UntilServer(TEXT("New authoritative pawn automatically composes the same appearance"), [this](FState& State)
				{ return AutomaticallyRetargetedRespawn(State, SubjectId) != nullptr; }, Timeout())
			.UntilClient(TEXT("Late observer also composes the newly replicated pawn without ApplyProfile"), 1, [this](FState& State)
				{ return AutomaticallyRetargetedRespawn(State, SubjectId) != nullptr; }, Timeout())
			.ThenServer(TEXT("Authority released the old follower and retained the persistent gameplay ASC"), [this](FState& State) { VerifyRespawn(State); })
			.ThenClients(TEXT("Both clients released old followers and rebound to the new gameplay mesh"), [this](FState& State) { VerifyRespawn(State); })
			.ThenServer(TEXT("Restore the named asset's original references before PIE teardown"), [this](FState& State)
			{
				ARpgCharacter* Character = FindPlayerState(State.World, SubjectId)->GetPawn<ARpgCharacter>();
				ASSERT_THAT(IsTrue(Retarget(Character)->ApplyProfile(nullptr)));
				DefaultProfileOverride.Restore();
				Retarget(Character)->RefreshFromPawnData();
				ASSERT_THAT(IsNull(Retarget(Character)->GetRetargetMesh()));
				ASSERT_THAT(IsTrue(Character->GetMesh()->IsVisible() && !Character->GetMesh()->bHiddenInGame));
			})
			.ThenClients(TEXT("Restore default UEFN presentation on each newly spawned client pawn"), [this](FState& State)
			{
				ARpgCharacter* Character = FindPlayerState(State.World, SubjectId)->GetPawn<ARpgCharacter>();
				ASSERT_THAT(IsTrue(Retarget(Character)->ApplyProfile(nullptr)));
				Retarget(Character)->RefreshFromPawnData();
				ASSERT_THAT(IsNull(Retarget(Character)->GetRetargetMesh()));
				ASSERT_THAT(IsTrue(Character->GetMesh()->IsVisible() && !Character->GetMesh()->bHiddenInGame));
			});
	}

	void VerifyTraversalAndClear(ARpgCharacter* Character)
	{
		using namespace RpgRuntimeRetargetTests;
		VerifyActiveContract(Character);
		const FObservation& Record = Observations.Get(Character->GetWorld());
		ASSERT_THAT(IsTrue(Record.bSawSourceMontage && Record.bSawRootMotion && Record.bSawCollisionLease));
		ASSERT_THAT(IsTrue(Record.SourcePoseMotion > 0.15f && Record.TargetPoseMotion > 0.15f));
		ASSERT_THAT(IsFalse(Record.bTargetPlayedGameplayMontage || Record.bGameplayMeshChanged));
		ASSERT_THAT(IsTrue(EquipmentOnSource(Character)));
		UE_LOG(LogTemp, Display, TEXT("RpgRuntimeRetarget role=%d sourcePoseDegrees=%.2f targetPoseDegrees=%.2f sourceMontage=%d rootMotion=%d"),
			static_cast<int32>(Character->GetLocalRole()), FMath::RadiansToDegrees(Record.SourcePoseMotion),
			FMath::RadiansToDegrees(Record.TargetPoseMotion), Record.bSawSourceMontage, Record.bSawRootMotion);
		VerifyFallback(Character);
	}
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
