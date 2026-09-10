// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Network/RpgCombatNetworkTestTypes.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
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
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "UObject/UnrealType.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility_BasicWeaponAttack.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMovementComponent.h"
#include "SurvivalRpg/Core/Character/RpgDownedComponent.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceManagerComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Development/RpgDeveloperSettings.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Equipment/RpgWeaponInstance.h"

namespace RpgGaspCMCIntegrationTests
{
	constexpr TCHAR ExperiencePath[] = TEXT("/Game/SurvivalRpg/System/Experiences/RpgGaspCMCExperience.RpgGaspCMCExperience_C");
	constexpr TCHAR PawnDataPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/CMC/RPG/DA_PawnData_GaspCMC.DA_PawnData_GaspCMC");
	constexpr TCHAR PawnClassPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/CMC/RPG/BP_RpgGasp_CMC.BP_RpgGasp_CMC_C");
	constexpr TCHAR AnimClassPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/CMC/RPG/ABP_RpgGasp_CMC.ABP_RpgGasp_CMC_C");
	constexpr TCHAR GameModePath[] = TEXT("/Game/SurvivalRpg/Maps/Test/GaspCMC/BP_Rpg_GaspCMCTestGameMode.BP_Rpg_GaspCMCTestGameMode_C");
	constexpr TCHAR MeshPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin");

	FPrimaryAssetId ExperienceId()
	{
		return FPrimaryAssetId(URpgExperienceDefinition::StaticClass()->GetFName(), TEXT("RpgGaspCMCExperience"));
	}

	/** Reads the shared presentation struct contract without freezing graph nodes or generated field GUIDs. */
	const FProperty* FindPresentationField(const UAnimInstance* Animation, const TCHAR* Prefix, const void*& OutData)
	{
		const FStructProperty* Properties = Animation
			? FindFProperty<FStructProperty>(Animation->GetClass(), TEXT("CharacterProperties")) : nullptr;
		if (!Properties)
		{
			return nullptr;
		}
		OutData = Properties->ContainerPtrToValuePtr<void>(Animation);
		for (TFieldIterator<FProperty> Field(Properties->Struct); Field; ++Field)
		{
			if (Field->GetName().StartsWith(Prefix))
			{
				return *Field;
			}
		}
		return nullptr;
	}

	bool PresentationEnumIs(const ARpgCharacter* Character, const TCHAR* Prefix, const TCHAR* Enumerator)
	{
		const void* Data = nullptr;
		const UAnimInstance* Animation = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;
		const FByteProperty* Field = CastField<FByteProperty>(FindPresentationField(Animation, Prefix, Data));
		return Field && Field->Enum && Field->GetPropertyValue_InContainer(Data)
			== Field->Enum->GetValueByNameString(Enumerator);
	}

	bool PresentationVelocityMatches(const ARpgCharacter* Character)
	{
		const void* Data = nullptr;
		const UAnimInstance* Animation = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;
		const FStructProperty* Field = CastField<FStructProperty>(FindPresentationField(Animation, TEXT("Velocity_"), Data));
		if (!Field || Field->Struct != TBaseStructure<FVector>::Get())
		{
			return false;
		}
		const FVector& Presented = *Field->ContainerPtrToValuePtr<FVector>(Data);
		const FVector Actual = Character->GetCharacterMovement()->Velocity;
		// Allow one animation-update interval; never compare world transforms across network worlds.
		return !Presented.ContainsNaN() && Presented.Equals(Actual, 100.0);
	}

	bool HasMotionMatchingSelection(const ARpgCharacter* Character)
	{
		const UAnimInstance* Animation = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;
		const FObjectPropertyBase* Selection = Animation
			? FindFProperty<FObjectPropertyBase>(Animation->GetClass(), TEXT("CurrentSelectedDatabase")) : nullptr;
		return Selection && IsValid(Selection->GetObjectPropertyValue_InContainer(Animation));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgGaspCMCCompositionTest,
	"SurvivalRpg.GASP.CMC.AssetComposition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgGaspCMCCompositionTest::RunTest(const FString& Parameters)
{
	using namespace RpgGaspCMCIntegrationTests;
	const UClass* ExperienceClass = LoadClass<URpgExperienceDefinition>(nullptr, ExperiencePath);
	const URpgExperienceDefinition* Experience = ExperienceClass ? ExperienceClass->GetDefaultObject<URpgExperienceDefinition>() : nullptr;
	const URpgPawnData* PawnData = LoadObject<URpgPawnData>(nullptr, PawnDataPath);
	UClass* PawnClass = LoadClass<ARpgCharacter>(nullptr, PawnClassPath);
	UClass* AnimClass = LoadClass<URpgAnimInstance>(nullptr, AnimClassPath);
	const UClass* GameModeClass = LoadClass<ARpgGameModeBase>(nullptr, GameModePath);
	if (!TestNotNull(TEXT("CMC Experience loads"), Experience) || !TestNotNull(TEXT("CMC PawnData loads"), PawnData)
		|| !TestNotNull(TEXT("CMC pawn remains an RPG character"), PawnClass)
		|| !TestNotNull(TEXT("CMC AnimBP preserves the RPG listen-server timing parent"), AnimClass)
		|| !TestNotNull(TEXT("Isolated test GameMode loads"), GameModeClass))
	{
		return false;
	}
	TestTrue(TEXT("Experience selects the CMC PawnData"), Experience->DefaultPawnData == PawnData);
	TestTrue(TEXT("PawnData selects the CMC pawn"), PawnData->PawnClass.Get() == PawnClass);
	TestTrue(TEXT("RPG input, camera and inventory composition remain present"),
		PawnData->InputConfig && PawnData->DefaultCameraMode && PawnData->InventoryLayoutDefinition);
	const ARpgCharacter* Pawn = PawnClass->GetDefaultObject<ARpgCharacter>();
	TestTrue(TEXT("The pawn retains the RPG CMC implementation"), Pawn->GetCharacterMovement()->IsA<URpgCharacterMovementComponent>());
	USkeletalMeshComponent* Mesh = Pawn->GetMesh();
	TestTrue(TEXT("UEFN is the gameplay, equipment and montage mesh"),
		Mesh->GetSkeletalMeshAsset() && Mesh->GetSkeletalMeshAsset()->GetPathName() == MeshPath);
	TestTrue(TEXT("GetMesh uses the adapted CMC AnimBP"), Mesh->GetAnimClass() == AnimClass);
	TestTrue(TEXT("Montage root motion remains owned by CMC/GAS"),
		AnimClass->GetDefaultObject<UAnimInstance>()->RootMotionMode == ERootMotionMode::RootMotionFromMontagesOnly);
	const ARpgGameModeBase* GameMode = GameModeClass->GetDefaultObject<ARpgGameModeBase>();
	const ARpgGameModeBase* Baseline = GetDefault<ARpgGameModeBase>();
	TestFalse(TEXT("Authored CMC test GameMode has disk persistence disabled"), GameMode->bEnableDiskPersistence);
	TestTrue(TEXT("Authored test slots and profile differ from production defaults"),
		GameMode->WorldSaveSlotName != Baseline->WorldSaveSlotName &&
		GameMode->WorldSaveBackupSlotName != Baseline->WorldSaveBackupSlotName &&
		GameMode->WorldSaveRecoverySlotName != Baseline->WorldSaveRecoverySlotName &&
		GameMode->OfflineProfileKey != Baseline->OfflineProfileKey);
	return true;
}

#if ENABLE_PIE_NETWORK_TEST

namespace RpgGaspCMCIntegrationTests
{
	/** Installed before CQTest creates any PIE world; remains active through its network teardown. */
	class FScopedSaveIsolation final
	{
	public:
		~FScopedSaveIsolation() { FGameModeEvents::OnGameModeInitializedEvent().Remove(Handle); }
		void Start()
		{
			Prefix = TEXT("SurvivalRpg_GaspCMCAutomation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
			Handle = FGameModeEvents::OnGameModeInitializedEvent().AddRaw(this, &FScopedSaveIsolation::OnInitialized);
		}
		bool IsIsolated(UWorld* World) const
		{
			if (!World || World->WorldType != EWorldType::PIE) return false;
			if (World->GetNetMode() == NM_Client) return World->GetAuthGameMode() == nullptr;
			const ARpgGameModeBase* GameMode = World->GetAuthGameMode<ARpgGameModeBase>();
			return GameMode && !GameMode->bEnableDiskPersistence && GameMode->WorldSaveSlotName.StartsWith(Prefix)
				&& GameMode->WorldSaveBackupSlotName == GameMode->WorldSaveSlotName + TEXT("_Backup")
				&& GameMode->WorldSaveRecoverySlotName == GameMode->WorldSaveSlotName + TEXT("_Recovery")
				&& GameMode->OfflineProfileKey.StartsWith(Prefix);
		}
	private:
		void OnInitialized(AGameModeBase* Initialized)
		{
			ARpgGameModeBase* GameMode = Cast<ARpgGameModeBase>(Initialized);
			if (!GameMode || !GameMode->GetWorld() || GameMode->GetWorld()->WorldType != EWorldType::PIE) return;
			// Broadcast occurs inside Super::InitGame, before RPG save loading, even for entry/travel worlds.
			GameMode->bEnableDiskPersistence = false;
			GameMode->WorldSaveSlotName = FString::Printf(TEXT("%s_%u"), *Prefix, GameMode->GetUniqueID());
			GameMode->WorldSaveBackupSlotName = GameMode->WorldSaveSlotName + TEXT("_Backup");
			GameMode->WorldSaveRecoverySlotName = GameMode->WorldSaveSlotName + TEXT("_Recovery");
			GameMode->OfflineProfileKey = Prefix;
		}
		FString Prefix;
		FDelegateHandle Handle;
	};

	/** Feeds ordinary owning-client movement input every tick, without editing replicated transforms. */
	class FScopedMovementInput final
	{
	public:
		~FScopedMovementInput() { Stop(); }
		void Start(ARpgCharacter* InCharacter)
		{
			Character = InCharacter;
			Handle = FWorldDelegates::OnWorldTickStart.AddRaw(this, &FScopedMovementInput::OnTick);
		}
		void Stop()
		{
			FWorldDelegates::OnWorldTickStart.Remove(Handle);
			Handle.Reset();
			Character.Reset();
		}
	private:
		void OnTick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
		{
			if (Character.IsValid() && Character->GetWorld() == World && Character->IsLocallyControlled())
			{
				Character->AddMovementInput(FVector::ForwardVector, 1.0f);
			}
		}
		TWeakObjectPtr<ARpgCharacter> Character;
		FDelegateHandle Handle;
	};

	struct FState : FBasePIENetworkComponentState
	{
		ARpgCombatNetworkFloorFixture* Floor = nullptr;
		FVector StartLocation = FVector::ZeroVector;
		double StartTime = -1.0;
		TWeakObjectPtr<UAnimMontage> Montage;
		TWeakObjectPtr<UAnimMontage> ExpectedMontage;
		float MontageStartPosition = 0.0f;
		float MontageRate = 0.0f;
		uint32 WindowOpenBaseline = 0;
		uint32 WindowCloseBaseline = 0;
		TWeakObjectPtr<ARpgCharacter> PawnBeforeDeath;
		TWeakObjectPtr<URpgAbilitySystemComponent> PersistentASC;
	};

	ARpgCharacter* LocalCharacter(UWorld* World)
	{
		const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
		return Controller ? Cast<ARpgCharacter>(Controller->GetPawn()) : nullptr;
	}
	ARpgCharacter* FindCharacter(UWorld* World, int32 PlayerId)
	{
		if (!World || PlayerId == INDEX_NONE) return nullptr;
		for (TActorIterator<ARpgCharacter> It(World); It; ++It)
		{
			if (It->GetPlayerState() && It->GetPlayerState()->GetPlayerId() == PlayerId) return *It;
		}
		return nullptr;
	}
	bool Ready(UWorld* World, const ARpgCharacter* Character)
	{
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		const URpgExperienceManagerComponent* Manager = GameState ? GameState->FindComponentByClass<URpgExperienceManagerComponent>() : nullptr;
		const UAnimInstance* Animation = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;
		return World && World->GetNetDriver() && Manager && Manager->IsExperienceLoaded()
			&& Manager->GetCurrentExperienceChecked()->GetPrimaryAssetId() == ExperienceId()
			&& Character && Character->GetClass()->GetPathName() == PawnClassPath
			&& Character->GetPlayerState() && Character->GetRpgAbilitySystemComponent()
			&& Animation && Animation->IsA<URpgAnimInstance>() && Animation->GetClass()->GetPathName() == AnimClassPath;
	}
	bool Grounded(const ARpgCharacter* Character)
	{
		return Character && Character->GetCharacterMovement()->IsMovingOnGround()
			&& PresentationEnumIs(Character, TEXT("MovementMode_"), TEXT("NewEnumerator4"));
	}
	ARpgPlayerState* FindPlayerState(UWorld* World, int32 PlayerId)
	{
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		if (!GameState) return nullptr;
		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (PlayerState && PlayerState->GetPlayerId() == PlayerId) return Cast<ARpgPlayerState>(PlayerState);
		}
		return nullptr;
	}
	ARpgCharacter* RespawnedCharacter(const FState& State, int32 PlayerId)
	{
		const ARpgPlayerState* PlayerState = FindPlayerState(State.World, PlayerId);
		// Resolve the current possession through PlayerState; a detached corpse may still exist.
		ARpgCharacter* Character = PlayerState ? PlayerState->GetPawn<ARpgCharacter>() : nullptr;
		const URpgHealthComponent* Health = URpgHealthComponent::FindHealthComponent(Character);
		const URpgAbilitySystemComponent* ASC = Character ? Character->GetRpgAbilitySystemComponent() : nullptr;
		return Ready(State.World, Character) && Character != State.PawnBeforeDeath.Get()
			&& !PlayerState->IsWaitingForRespawn() && Health && Health->GetHealth() > 0.0f && !Health->IsDeadOrDying()
			&& ASC && ASC == State.PersistentASC.Get() && ASC == PlayerState->GetRpgAbilitySystemComponent()
			&& ASC->GetAvatarActor() == Character && ASC->GetOwnerActor() == PlayerState
			&& Character->GetMesh()->GetSkeletalMeshAsset()
			&& Character->GetMesh()->GetSkeletalMeshAsset()->GetPathName() == MeshPath ? Character : nullptr;
	}
	const FGameplayTag& PrimaryInput()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Weapon.Primary"));
		return Tag;
	}
	URpgGameplayAbility_BasicWeaponAttack* Attack(ARpgCharacter* Character)
	{
		URpgAbilitySystemComponent* ASC = Character ? Character->GetRpgAbilitySystemComponent() : nullptr;
		URpgEquipmentManagerComponent* Equipment = Character ? Character->GetEquipmentManagerComponent() : nullptr;
		URpgWeaponInstance* Weapon = Equipment
			? Cast<URpgWeaponInstance>(Equipment->GetEquipmentInstanceInSlot(ERpgEquipmentSlot::MainHand)) : nullptr;
		if (!ASC || !Weapon || Weapon->GetSpawnedActors().IsEmpty()) return nullptr;
		for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (!Spec.PendingRemove && Spec.SourceObject.Get() == Weapon && Spec.GetDynamicSpecSourceTags().HasTagExact(PrimaryInput()))
			{
				if (auto* Ability = Cast<URpgGameplayAbility_BasicWeaponAttack>(Spec.GetPrimaryInstance())) return Ability;
			}
		}
		return nullptr;
	}
	FTimespan Timeout() { return FTimespan::FromSeconds(90.0); }
}

NETWORK_TEST_CLASS(GaspCMCExperiencePIE, "SurvivalRpg.GASP.CMC")
{
	using FState = RpgGaspCMCIntegrationTests::FState;
	// Destruction is reversed: network cleanup finishes while save isolation is still installed.
	RpgGaspCMCIntegrationTests::FScopedSaveIsolation Isolation;
	RpgGaspCMCIntegrationTests::FScopedMovementInput MovementInput;
	FPIENetworkComponent<FState> Network{TestRunner, TestCommandBuilder, bInitializing};
	FPrimaryAssetId OriginalExperience;
	bool bConfigured = false;
	int32 SubjectId = INDEX_NONE;

	BEFORE_EACH()
	{
		using namespace RpgGaspCMCIntegrationTests;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{
				TestRunner->AddError(TEXT("CMC integration automation refuses to interrupt an existing PIE session."));
				return;
			}
		}
		UClass* GameModeClass = LoadClass<ARpgGameModeBase>(nullptr, GameModePath);
		ASSERT_THAT(IsNotNull(GameModeClass));
		if (!GameModeClass) return;
		Isolation.Start();
		OriginalExperience = GetDefault<URpgDeveloperSettings>()->ExperienceOverride;
		GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = ExperienceId();
		bConfigured = true;
		// CQTest creates its own temporary map. The authored CMC demonstration map is tested separately.
		FNetworkComponentBuilder<FState>().WithClients(1).AsListenServer()
			.WithGameInstanceClass(FSoftClassPath(TEXT("/Game/SurvivalRpg/Core/Game/BP_Rpg_GameInstance.BP_Rpg_GameInstance_C")))
			.WithGameMode(GameModeClass).Build(Network);
	}
	AFTER_EACH()
	{
		MovementInput.Stop();
		if (bConfigured) GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = OriginalExperience;
	}

	TEST_METHOD(RemoteMovementLateJoinAndEquipmentMontage)
	{
		using namespace RpgGaspCMCIntegrationTests;
		if (!bConfigured) return;
		Network.ThenServer(TEXT("Server persistence was isolated before InitGame"), [this](FState& State)
			{ ASSERT_THAT(IsTrue(Isolation.IsIsolated(State.World))); })
			.SpawnAndReplicate<ARpgCombatNetworkFloorFixture, &FState::Floor>(Timeout())
			.UntilClient(TEXT("Remote owner receives the CMC Experience and grounded RPG pawn"), 0, [](FState& State)
				{ return Ready(State.World, LocalCharacter(State.World)) && Grounded(LocalCharacter(State.World)); }, Timeout())
			.ThenClient(TEXT("Capture autonomous identity and start ordinary CMC input"), 0, [this](FState& State)
			{
				ARpgCharacter* Character = LocalCharacter(State.World);
				ASSERT_THAT(IsTrue(Isolation.IsIsolated(State.World)));
				ASSERT_THAT(IsTrue(Character->GetLocalRole() == ROLE_AutonomousProxy));
				SubjectId = Character->GetPlayerState()->GetPlayerId();
				State.StartLocation = Character->GetActorLocation();
				MovementInput.Start(Character);
			})
			.UntilClient(TEXT("Owner accelerates and its animation snapshot tracks real movement"), 0, [](FState& State)
			{
				ARpgCharacter* Character = LocalCharacter(State.World);
				return Character && Character->GetVelocity().Size2D() > 150.0
					&& FVector::Dist2D(State.StartLocation, Character->GetActorLocation()) > 200.0
					&& Grounded(Character) && PresentationVelocityMatches(Character) && HasMotionMatchingSelection(Character);
			}, Timeout())
			.UntilServer(TEXT("Listen server sees the remote pawn moving with current animation inputs"), [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				return Ready(State.World, Character) && State.World->GetNetMode() == NM_ListenServer
					&& Character->HasAuthority() && Character->GetRemoteRole() == ROLE_AutonomousProxy
					&& Character->GetVelocity().Size2D() > 150.0 && PresentationVelocityMatches(Character)
					&& HasMotionMatchingSelection(Character);
			}, Timeout())
			.ThenClient(TEXT("Release movement input"), 0, [this](FState&) { MovementInput.Stop(); })
			.UntilServer(TEXT("Remote pawn returns to idle on the listen server"), [this](FState& State)
				{ ARpgCharacter* Character = FindCharacter(State.World, SubjectId); return Grounded(Character) && Character->GetVelocity().Size2D() < 5.0 && PresentationVelocityMatches(Character); }, Timeout())
			.ThenClient(TEXT("Owner requests crouch through CMC"), 0, [](FState& State) { LocalCharacter(State.World)->Crouch(); })
			.UntilServer(TEXT("Crouch reaches server gameplay and animation state"), [this](FState& State)
				{ ARpgCharacter* Character = FindCharacter(State.World, SubjectId); return Character && Character->bIsCrouched && PresentationEnumIs(Character, TEXT("Stance_"), TEXT("NewEnumerator1")); }, Timeout())
			.ThenClientJoins()
			.UntilClient(TEXT("Late join receives the CMC Experience and existing crouched simulated proxy"), 1, [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				return Isolation.IsIsolated(State.World) && Ready(State.World, Character)
					&& Character->GetLocalRole() == ROLE_SimulatedProxy && Character->bIsCrouched
					&& PresentationEnumIs(Character, TEXT("Stance_"), TEXT("NewEnumerator1"));
			}, Timeout())
			.ThenClient(TEXT("Owner stands and jumps through ordinary CMC"), 0, [](FState& State)
				{ LocalCharacter(State.World)->UnCrouch(); LocalCharacter(State.World)->Jump(); })
			.UntilServer(TEXT("Server animation observes the owner in air"), [this](FState& State)
				{ ARpgCharacter* Character = FindCharacter(State.World, SubjectId); return Character && Character->GetCharacterMovement()->IsFalling() && PresentationEnumIs(Character, TEXT("MovementMode_"), TEXT("NewEnumerator5")); }, Timeout())
			.ThenClient(TEXT("Release jump"), 0, [](FState& State) { LocalCharacter(State.World)->StopJumping(); })
			.UntilServer(TEXT("Owner lands and animation returns to ground"), [this](FState& State)
				{ return Grounded(FindCharacter(State.World, SubjectId)); }, Timeout())
			.UntilClient(TEXT("Late-joined observer also resolves the landed standing pawn"), 1, [this](FState& State)
				{ ARpgCharacter* Character = FindCharacter(State.World, SubjectId); return Grounded(Character) && !Character->bIsCrouched && PresentationEnumIs(Character, TEXT("Stance_"), TEXT("NewEnumerator0")); }, Timeout())
			.ThenServer(TEXT("Equip the existing sword through the authoritative RPG equipment component"), [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				UClass* Sword = LoadClass<URpgEquipmentDefinition>(nullptr, TEXT("/GF_Combat_Core/Equipment/Weapons/ED_BasicSword.ED_BasicSword_C"));
				ASSERT_THAT(IsNotNull(Sword));
				URpgEquipmentManagerComponent* Equipment = Character->GetEquipmentManagerComponent();
				ASSERT_THAT(IsNotNull(Equipment));
				if (!Sword || !Equipment) return;
				Equipment->UnequipItemInSlot(ERpgEquipmentSlot::MainHand);
				ASSERT_THAT(IsNotNull(Equipment->EquipItemInSlot(Sword, ERpgEquipmentSlot::MainHand)));
			})
			.UntilClient(TEXT("Owner receives equipment-granted primary ability"), 0, [](FState& State)
				{ return Grounded(LocalCharacter(State.World)) && Attack(LocalCharacter(State.World)); }, Timeout())
			.ThenServer(TEXT("Record server attack lifecycle baselines"), [this](FState& State)
			{
				URpgGameplayAbility_BasicWeaponAttack* Ability = Attack(FindCharacter(State.World, SubjectId));
				ASSERT_THAT(IsNotNull(Ability));
				if (!Ability) return;
				State.WindowOpenBaseline = Ability->GetAuthorityWindowOpenCountForTests();
				State.WindowCloseBaseline = Ability->GetAuthorityWindowCloseCountForTests();
				const URpgWeaponInstance* Weapon = Cast<URpgWeaponInstance>(FindCharacter(State.World, SubjectId)
					->GetEquipmentManagerComponent()->GetEquipmentInstanceInSlot(ERpgEquipmentSlot::MainHand));
				const FRpgWeaponAttackDefinition* Definition = Weapon
					? Weapon->FindAttackDefinition(FGameplayTag::RequestGameplayTag(TEXT("Weapon.Attack.Primary"))) : nullptr;
				ASSERT_THAT(IsNotNull(Definition));
				if (Definition) State.ExpectedMontage = Definition->Montage.Get();
				ASSERT_THAT(IsTrue(State.ExpectedMontage.IsValid()));
			})
			.ThenClient(TEXT("Activate the real primary ability through ASC input"), 0, [](FState& State)
			{
				URpgAbilitySystemComponent* ASC = LocalCharacter(State.World)->GetRpgAbilitySystemComponent();
				ASC->AbilityInputTagPressed(PrimaryInput());
				ASC->ProcessAbilityInput(1.0f / 60.0f, false);
				ASC->AbilityInputTagReleased(PrimaryInput());
				ASC->ProcessAbilityInput(1.0f / 60.0f, false);
			})
			.UntilServer(TEXT("Server advances the equipment montage at its effective playback rate"), [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				UAnimInstance* Animation = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;
				UAnimMontage* Montage = Animation ? Animation->GetCurrentActiveMontage() : nullptr;
				if (!Montage || Montage != State.ExpectedMontage.Get()) return false;
				if (!State.Montage.IsValid())
				{
					State.Montage = Montage;
					State.MontageStartPosition = Animation->Montage_GetPosition(Montage);
					State.MontageRate = Animation->Montage_GetEffectivePlayRate(Montage);
					State.StartTime = State.World->GetTimeSeconds();
				}
				const double Elapsed = State.World->GetTimeSeconds() - State.StartTime;
				if (Elapsed < 0.2 || State.Montage.Get() != Montage) return false;
				const double Advance = Animation->Montage_GetPosition(Montage) - State.MontageStartPosition;
				const double Expected = Elapsed * State.MontageRate;
				return Expected > 0.0 && Advance >= Expected * 0.75 && Advance <= Expected * 1.25;
			}, Timeout())
			.UntilServer(TEXT("Attack notifies finish and GAS returns to clean locomotion"), [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				URpgGameplayAbility_BasicWeaponAttack* Ability = Attack(Character);
				return Ability && Ability->GetAuthorityWindowOpenCountForTests() == State.WindowOpenBaseline + 1
					&& Ability->GetAuthorityWindowCloseCountForTests() == State.WindowCloseBaseline + 1
					&& !Ability->HasResidualAttackRuntimeStateForTests() && !Character->GetRpgAbilitySystemComponent()->GetCurrentMontage()
					&& Grounded(Character) && PresentationVelocityMatches(Character);
			}, Timeout())
			.ThenClients(TEXT("Record each client's pawn and persistent ASC before death"), [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				ASSERT_THAT(IsTrue(Ready(State.World, Character)));
				if (!Character) return;
				State.PawnBeforeDeath = Character;
				State.PersistentASC = Character->GetRpgAbilitySystemComponent();
			})
			.ThenServer(TEXT("Enter final death through the authoritative health and downed components"), [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				URpgHealthComponent* Health = URpgHealthComponent::FindHealthComponent(Character);
				ASSERT_THAT(IsNotNull(Health));
				if (!Health) return;
				ASSERT_THAT(IsTrue(Character->HasAuthority()));
				State.PawnBeforeDeath = Character;
				State.PersistentASC = Character->GetRpgAbilitySystemComponent();
				Health->DamageSelfDestruct(false);
				if (URpgDownedComponent* Downed = URpgDownedComponent::FindDownedComponent(Character); Downed && Downed->IsDowned())
				{
					Downed->ForceDeathFromDowned();
				}
			})
			.UntilClient(TEXT("Owner receives pending respawn and the server's elapsed respawn delay"), 0, [this](FState& State)
			{
				const ARpgPlayerState* PlayerState = FindPlayerState(State.World, SubjectId);
				return PlayerState && PlayerState->IsWaitingForRespawn() && PlayerState->CanRespawnNow();
			}, Timeout())
			.ThenClient(TEXT("Owner requests respawn once through the real controller RPC"), 0, [this](FState& State)
			{
				ARpgPlayerController* Controller = Cast<ARpgPlayerController>(State.World->GetFirstPlayerController());
				ASSERT_THAT(IsNotNull(Controller));
				if (!Controller) return;
				const ARpgPlayerState* PlayerState = Controller->GetPlayerState<ARpgPlayerState>();
				const bool bCanRequest = State.World->GetNetMode() == NM_Client && !Controller->HasAuthority()
					&& Controller->IsLocalController() && PlayerState && PlayerState->GetPlayerId() == SubjectId
					&& PlayerState->IsWaitingForRespawn() && PlayerState->CanRespawnNow();
				ASSERT_THAT(IsTrue(bCanRequest));
				// Native latent execution preserves normal RPC callspace; no editor script execution guard.
				if (bCanRequest) Controller->RequestRespawn();
			})
			.UntilClient(TEXT("Owner possesses a new healthy CMC pawn with the persistent ASC rebound"), 0, [this](FState& State)
			{
				ARpgCharacter* Character = RespawnedCharacter(State, SubjectId);
				return Character && Character == LocalCharacter(State.World)
					&& Character->GetLocalRole() == ROLE_AutonomousProxy && Grounded(Character);
			}, Timeout())
			.ThenClient(TEXT("Drive the newly possessed CMC pawn through ordinary movement input"), 0, [this](FState& State)
			{
				ARpgCharacter* Character = LocalCharacter(State.World);
				State.StartLocation = Character->GetActorLocation();
				MovementInput.Start(Character);
			})
			.UntilClient(TEXT("Respawned owner moves and selects a motion-matching database"), 0, [this](FState& State)
			{
				ARpgCharacter* Character = RespawnedCharacter(State, SubjectId);
				return Character && Character->GetVelocity().Size2D() > 150.0
					&& FVector::Dist2D(State.StartLocation, Character->GetActorLocation()) > 200.0
					&& Grounded(Character) && PresentationVelocityMatches(Character) && HasMotionMatchingSelection(Character);
			}, Timeout())
			.UntilServer(TEXT("Listen server sees the new healthy pawn moving with restored GAS and animation"), [this](FState& State)
			{
				ARpgCharacter* Character = RespawnedCharacter(State, SubjectId);
				return Character && Character->HasAuthority() && Character->GetRemoteRole() == ROLE_AutonomousProxy
					&& Character->GetVelocity().Size2D() > 150.0 && Grounded(Character)
					&& PresentationVelocityMatches(Character) && HasMotionMatchingSelection(Character);
			}, Timeout())
			.UntilClient(TEXT("Late-joined observer also receives the newly spawned healthy CMC pawn"), 1, [this](FState& State)
			{
				ARpgCharacter* Character = RespawnedCharacter(State, SubjectId);
				return Character && Character->GetLocalRole() == ROLE_SimulatedProxy && HasMotionMatchingSelection(Character);
			}, Timeout())
			.ThenClient(TEXT("Release the respawned pawn's movement input"), 0, [this](FState&) { MovementInput.Stop(); })
			.ThenServer(TEXT("Persistence remains disabled through test completion"), [this](FState& State)
				{ ASSERT_THAT(IsTrue(Isolation.IsIsolated(State.World))); });
	}
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
