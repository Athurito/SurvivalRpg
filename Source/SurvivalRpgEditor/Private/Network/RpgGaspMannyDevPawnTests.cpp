// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Network/RpgCombatNetworkTestTypes.h"
#include "Animation/AnimInstance.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Misc/Guid.h"
#include "UObject/UnrealType.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceManagerComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Development/RpgDeveloperSettings.h"

#if ENABLE_PIE_NETWORK_TEST
namespace RpgGaspMannyDevPawnTests
{
	// GASP-14 development variant: the CMC traversal pawn on the Lyra Manny skeleton with the baked set.
	constexpr TCHAR GameModePath[] = TEXT("/Game/SurvivalRpg/Maps/Test/GaspMantle/BP_Rpg_GaspMantleTestGameMode.BP_Rpg_GaspMantleTestGameMode_C");
	constexpr TCHAR SkeletonPath[] = TEXT("/Game/SurvivalRpg/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin");
	constexpr TCHAR AnimClassPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Manny/ABP_RpgGasp_CMC_Manny.ABP_RpgGasp_CMC_Manny_C");
	constexpr TCHAR DatabaseChooserPackage[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Manny/Choosers/CHT_RpgGasp_CMC_PoseSearchDatabases");
	FPrimaryAssetId ExperienceId() { return FPrimaryAssetId(URpgExperienceDefinition::StaticClass()->GetFName(), TEXT("RpgGasp14MannyExperience")); }
	FTimespan Timeout() { return FTimespan::FromSeconds(60.0); }

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
	bool Grounded(const ARpgCharacter* Character)
	{
		return Character && Character->GetCharacterMovement() && Character->GetCharacterMovement()->IsMovingOnGround();
	}
	/** The development Experience is live and the pawn plays the Manny AnimBP on the Lyra Manny skeleton. */
	bool MannyReady(UWorld* World, const ARpgCharacter* Character)
	{
		const AGameStateBase* GameState = ActiveWorld(World) ? World->GetGameState() : nullptr;
		const URpgExperienceManagerComponent* Manager = GameState ? GameState->FindComponentByClass<URpgExperienceManagerComponent>() : nullptr;
		const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
		const USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
		const UAnimInstance* Animation = Mesh ? Mesh->GetAnimInstance() : nullptr;
		return Manager && World->GetNetDriver() && Manager->IsExperienceLoaded()
			&& Manager->GetCurrentExperienceChecked()->GetPrimaryAssetId() == ExperienceId()
			&& Character && Character->GetPlayerState() && Asset && Asset->GetSkeleton() && Asset->GetSkeleton()->GetPathName() == SkeletonPath
			&& Animation && Animation->GetClass()->GetPathName() == AnimClassPath;
	}
	/** True when Motion Matching selected a database that the Manny database chooser can return. */
	bool HasMannySelection(const ARpgCharacter* Character)
	{
		const UAnimInstance* Animation = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;
		const FObjectPropertyBase* Selection = Animation
			? FindFProperty<FObjectPropertyBase>(Animation->GetClass(), TEXT("CurrentSelectedDatabase")) : nullptr;
		const UObject* Database = Selection ? Selection->GetObjectPropertyValue_InContainer(Animation) : nullptr;
		if (!IsValid(Database)) return false;
		TArray<FName> ChooserDatabases;
		FAssetRegistryModule::GetRegistry().GetDependencies(DatabaseChooserPackage, ChooserDatabases);
		return ChooserDatabases.Contains(Database->GetPackage()->GetFName());
	}

	class FScopedSaveIsolation final
	{
	public:
		~FScopedSaveIsolation() { FGameModeEvents::OnGameModeInitializedEvent().Remove(Handle); }
		void Start()
		{
			Prefix = TEXT("SurvivalRpg_GaspMannyDevPawnAutomation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
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

	/** Ordinary forward movement input on the owner, once per owner world tick. */
	class FScopedMovementInput final
	{
	public:
		~FScopedMovementInput() { Stop(); }
		void Start(ARpgCharacter* Character)
		{
			Stop();
			Subject = Character;
			Handle = FWorldDelegates::OnWorldTickStart.AddRaw(this, &FScopedMovementInput::Tick);
		}
		void Stop() { FWorldDelegates::OnWorldTickStart.Remove(Handle); Handle.Reset(); Subject.Reset(); }
	private:
		void Tick(UWorld* World, ELevelTick, float)
		{
			if (ARpgCharacter* Character = Subject.Get(); Character && Character->GetWorld() == World)
				Character->AddMovementInput(Character->GetActorForwardVector());
		}
		TWeakObjectPtr<ARpgCharacter> Subject;
		FDelegateHandle Handle;
	};

	struct FState : FBasePIENetworkComponentState {};
}

NETWORK_TEST_CLASS(GaspMannyDevPawnPIE, "SurvivalRpg.GASP.Manny.DevPawn")
{
	using FState = RpgGaspMannyDevPawnTests::FState;
	// Destruction is reversed: network cleanup finishes while save isolation and the ground are still installed.
	RpgGaspMannyDevPawnTests::FScopedSaveIsolation Isolation;
	FRpgScopedNetworkTestGround Ground;
	RpgGaspMannyDevPawnTests::FScopedMovementInput MovementInput;
	FPIENetworkComponent<FState> Network{TestRunner, TestCommandBuilder, bInitializing};
	FPrimaryAssetId OriginalExperience;
	bool bConfigured = false;
	int32 SubjectId = INDEX_NONE;

	BEFORE_EACH()
	{
		using namespace RpgGaspMannyDevPawnTests;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{
				TestRunner->AddError(TEXT("Manny development automation refuses to interrupt an existing PIE session."));
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
		FNetworkComponentBuilder<FState>().WithClients(1).AsListenServer()
			.WithGameInstanceClass(FSoftClassPath(TEXT("/Game/SurvivalRpg/Core/Game/BP_Rpg_GameInstance.BP_Rpg_GameInstance_C")))
			.WithGameMode(GameModeClass).Build(Network);
	}
	AFTER_EACH()
	{
		MovementInput.Stop();
		if (bConfigured) GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = OriginalExperience;
	}

	/**
	 * The GASP-14 development pawn plays the baked set through the Manny AnimBP: its Motion Matching selects databases
	 * from the Manny chooser on the moving owner, on the listen server and on a client that joins afterwards.
	 */
	TEST_METHOD(MannyPawnSelectsMannyDatabasesOnEveryRole)
	{
		using namespace RpgGaspMannyDevPawnTests;
		if (!bConfigured) return;
		Network.UntilClient(TEXT("Owner spawns the Manny development pawn on the floor"), 0, [](FState& State)
				{ return MannyReady(State.World, LocalCharacter(State.World)) && Grounded(LocalCharacter(State.World)); }, Timeout())
			.ThenClient(TEXT("Capture the owner and walk forward"), 0, [this](FState& State)
			{
				ARpgCharacter* Character = LocalCharacter(State.World);
				ASSERT_THAT(IsTrue(Isolation.IsIsolated(State.World) && Character->GetLocalRole() == ROLE_AutonomousProxy));
				SubjectId = Character->GetPlayerState()->GetPlayerId();
				MovementInput.Start(Character);
			})
			.UntilClient(TEXT("Owner moves with a Manny database selected"), 0, [](FState& State)
			{
				ARpgCharacter* Character = LocalCharacter(State.World);
				return Character && Character->GetVelocity().Size2D() > 150.0 && HasMannySelection(Character);
			}, Timeout())
			.UntilServer(TEXT("Listen server plays the moving remote pawn from the Manny chooser"), [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				return Isolation.IsIsolated(State.World) && MannyReady(State.World, Character)
					&& Character->GetVelocity().Size2D() > 150.0 && HasMannySelection(Character);
			}, Timeout())
			.ThenClient(TEXT("Release movement input"), 0, [this](FState&) { MovementInput.Stop(); })
			.UntilServer(TEXT("Remote pawn stops on the listen server"), [this](FState& State)
				{ ARpgCharacter* Character = FindCharacter(State.World, SubjectId); return Grounded(Character) && Character->GetVelocity().Size2D() < 5.0; }, Timeout())
			.ThenClientJoins()
			.UntilClient(TEXT("Late joiner plays the existing pawn as a Manny simulated proxy"), 1, [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				return Isolation.IsIsolated(State.World) && MannyReady(State.World, Character)
					&& Character->GetLocalRole() == ROLE_SimulatedProxy && HasMannySelection(Character);
			}, Timeout());
	}
};
#endif

#endif
