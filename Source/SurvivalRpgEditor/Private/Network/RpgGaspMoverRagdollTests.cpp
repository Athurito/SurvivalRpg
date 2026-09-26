// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Network/RpgCombatNetworkTestTypes.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Abilities/GameplayAbility.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerStart.h"
#include "InputKeyEventArgs.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeLock.h"
#include "PhysicsControlComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMoverComponent.h"
#include "SurvivalRpg/Core/Character/RpgDeadMovementMode.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/Core/Character/RpgMoverPawn.h"
#include "SurvivalRpg/Core/Character/RpgMoverRagdollComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnGameplayComponent.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceManagerComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Development/RpgDeveloperSettings.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentInstance.h"
#include "SurvivalRpg/Input/RpgInputConfig.h"
#include "UObject/UObjectIterator.h"

namespace RpgGaspMoverRagdollTests
{
	constexpr TCHAR ExperiencePath[] = TEXT("/Game/SurvivalRpg/System/Experiences/RpgGaspMoverRagdollExperience.RpgGaspMoverRagdollExperience_C");
	constexpr TCHAR PawnDataPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Mover/Ragdoll/RPG/DA_PawnData_GaspMoverRagdoll.DA_PawnData_GaspMoverRagdoll");
	constexpr TCHAR PawnPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Mover/Ragdoll/RPG/BP_RpgGasp_MoverRagdoll.BP_RpgGasp_MoverRagdoll_C");
	constexpr TCHAR GameModePath[] = TEXT("/Game/SurvivalRpg/Maps/Test/GaspMoverRagdoll/BP_Rpg_GaspMoverRagdollTestGameMode.BP_Rpg_GaspMoverRagdollTestGameMode_C");
	FPrimaryAssetId ExperienceId() { return FPrimaryAssetId(URpgExperienceDefinition::StaticClass()->GetFName(), TEXT("RpgGaspMoverRagdollExperience")); }
	FGameplayTag Tag(const TCHAR* Name) { return FGameplayTag::RequestGameplayTag(Name); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgGaspMoverRagdollCompositionTest,
	"SurvivalRpg.GASP.Mover.Ragdoll.AssetComposition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgGaspMoverRagdollCompositionTest::RunTest(const FString& Parameters)
{
	using namespace RpgGaspMoverRagdollTests;
	UClass* ExperienceClass = LoadClass<URpgExperienceDefinition>(nullptr, ExperiencePath);
	const URpgExperienceDefinition* Experience = ExperienceClass ? ExperienceClass->GetDefaultObject<URpgExperienceDefinition>() : nullptr;
	const URpgPawnData* Data = LoadObject<URpgPawnData>(nullptr, PawnDataPath);
	UClass* PawnClass = LoadClass<ARpgMoverPawn>(nullptr, PawnPath);
	UClass* ModeClass = LoadClass<ARpgGameModeBase>(nullptr, GameModePath);
	if (!TestNotNull(TEXT("Ragdoll Experience loads"), Experience) || !TestNotNull(TEXT("Ragdoll PawnData loads"), Data)
		|| !TestNotNull(TEXT("Ragdoll uses the existing RPG Mover pawn foundation"), PawnClass)
		|| !TestNotNull(TEXT("Isolated Ragdoll test GameMode loads"), ModeClass)) return false;
	TestTrue(TEXT("Experience and PawnData select the dedicated Ragdoll variant"), Experience->DefaultPawnData == Data && Data->PawnClass == PawnClass);
	const ARpgMoverPawn* Defaults = PawnClass->GetDefaultObject<ARpgMoverPawn>();
	TestTrue(TEXT("Network Prediction retains movement ownership"), Defaults->GetIsReplicated() && !Defaults->IsReplicatingMovement());
	TestNotNull(TEXT("PawnExtension retains ASC and PawnData composition"), URpgPawnExtensionComponent::FindPawnExtensionComponent(Defaults));
	TestNotNull(TEXT("RPG equipment remains composed"), Defaults->FindComponentByClass<URpgEquipmentManagerComponent>());
	const ARpgGameModeBase* Mode = ModeClass->GetDefaultObject<ARpgGameModeBase>();
	TestFalse(TEXT("The authored Ragdoll test mode disables disk persistence"), Mode->bEnableDiskPersistence);
	TestTrue(TEXT("Test persistence identity is separate from production"), Mode->WorldSaveSlotName != GetDefault<ARpgGameModeBase>()->WorldSaveSlotName
		&& Mode->OfflineProfileKey != GetDefault<ARpgGameModeBase>()->OfflineProfileKey);
	return true;
}

#if ENABLE_PIE_NETWORK_TEST
namespace RpgGaspMoverRagdollTests
{
	enum class EScenario : uint8 { Recover, Reenter, DeathInRagdoll, DeathInGetup };
	FTimespan Timeout() { return FTimespan::FromSeconds(35.0); }
	bool ActiveWorld(const UWorld* World)
	{
		if (!GEngine || !World) return false;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE && Context.World() == World)
				return IsValid(World) && !World->bIsTearingDown && !World->IsBeingCleanedUp();
		return false;
	}
	ARpgPlayerState* Player(UWorld* World, int32 Id)
	{
		const AGameStateBase* Game = ActiveWorld(World) ? World->GetGameState() : nullptr;
		if (Game) for (APlayerState* Entry : Game->PlayerArray)
			if (Entry && Entry->GetPlayerId() == Id) return Cast<ARpgPlayerState>(Entry);
		return nullptr;
	}
	APawn* Pawn(UWorld* World, int32 Id) { const ARpgPlayerState* State = Player(World, Id); return State ? State->GetPawn() : nullptr; }
	APawn* LocalPawn(UWorld* World) { const APlayerController* PC = ActiveWorld(World) ? World->GetFirstPlayerController() : nullptr; return PC ? PC->GetPawn() : nullptr; }
	USkeletalMeshComponent* Mesh(const APawn* Character) { return URpgPawnExtensionComponent::FindGameplayMesh(Character); }
	URpgCharacterMoverComponent* Mover(const APawn* Character) { return Character ? Character->FindComponentByClass<URpgCharacterMoverComponent>() : nullptr; }
	URpgMoverRagdollComponent* Ragdoll(const APawn* Character) { return Character ? Character->FindComponentByClass<URpgMoverRagdollComponent>() : nullptr; }
	UPhysicsControlComponent* Controls(const APawn* Character) { return Character ? Character->FindComponentByClass<UPhysicsControlComponent>() : nullptr; }
	URpgHealthComponent* Health(const APawn* Character) { return URpgHealthComponent::FindHealthComponent(Character); }
	URpgAbilitySystemComponent* ASC(const APawn* Character)
	{
		const URpgPawnExtensionComponent* Extension = URpgPawnExtensionComponent::FindPawnExtensionComponent(Character);
		return Extension ? Extension->GetRpgAbilitySystemComponent() : nullptr;
	}
	FGameplayAbilitySpec* RagdollSpec(APawn* Character)
	{
		if (ASC(Character)) for (FGameplayAbilitySpec& Spec : ASC(Character)->GetActivatableAbilities())
			if (!Spec.PendingRemove && Spec.GetDynamicSpecSourceTags().HasTagExact(Tag(TEXT("InputTag.Ability.Ragdoll")))) return &Spec;
		return nullptr;
	}
	bool Equipped(const APawn* Character)
	{
		const URpgEquipmentManagerComponent* Equipment = Character ? Character->FindComponentByClass<URpgEquipmentManagerComponent>() : nullptr;
		if (!Equipment || !Mesh(Character)) return false;
		for (ERpgEquipmentSlot Slot : { ERpgEquipmentSlot::MainHand, ERpgEquipmentSlot::OffHand })
		{
			const URpgEquipmentInstance* Item = Equipment->GetEquipmentInstanceInSlot(Slot);
			if (!Item || Item->GetPawn() != Character || Item->GetSpawnedActors().IsEmpty()) return false;
			for (const AActor* Actor : Item->GetSpawnedActors())
				if (!Actor || !Actor->GetRootComponent() || Actor->GetRootComponent()->GetAttachParent() != Mesh(Character)) return false;
		}
		return true;
	}
	bool ControlsReady(const APawn* Character)
	{
		UPhysicsControlComponent* Physics = Controls(Character);
		USkeletalMeshComponent* Source = Mesh(Character);
		if (!Physics || !Physics->IsRegistered() || !Source || !Source->GetPhysicsAsset()
			|| Physics->GetAllControlNames().IsEmpty() || Physics->GetAllBodyModifierNames().IsEmpty()) return false;
		for (const FName Set : { FName(TEXT("ParentSpace_Arms")), FName(TEXT("ParentSpace_Legs")), FName(TEXT("ParentSpace_Torso")), FName(TEXT("ParentSpace_Head")) })
		{
			const TArray<FName>& Names = Physics->GetControlNamesInSet(Set);
			if (Names.IsEmpty()) return false;
			for (const FName Name : Names) if (!Physics->GetControlExists(Name)) return false;
		}
		for (const FName Name : Physics->GetAllBodyModifierNames()) if (!Physics->GetBodyModifierExists(Name)) return false;
		for (const FName Bone : { FName(TEXT("pelvis")), FName(TEXT("spine_05")), FName(TEXT("thigh_l")), FName(TEXT("upperarm_l")) })
			if (Source->GetBoneIndex(Bone) == INDEX_NONE || !Source->GetBodyInstance(Bone)) return false;
		return true;
	}
	bool Ready(UWorld* World, APawn* Character)
	{
		const AGameStateBase* Game = ActiveWorld(World) ? World->GetGameState() : nullptr;
		const URpgExperienceManagerComponent* Experience = Game ? Game->FindComponentByClass<URpgExperienceManagerComponent>() : nullptr;
		const URpgPawnExtensionComponent* Extension = URpgPawnExtensionComponent::FindPawnExtensionComponent(Character);
		const URpgPawnGameplayComponent* Gameplay = URpgPawnGameplayComponent::FindPawnGameplayComponent(Character);
		const ARpgPlayerState* State = Character ? Character->GetPlayerState<ARpgPlayerState>() : nullptr;
		return Experience && Experience->IsExperienceLoaded() && Experience->GetCurrentExperienceChecked()->GetPrimaryAssetId() == ExperienceId()
			&& Character && Character->IsA<ARpgMoverPawn>() && State && Extension && Gameplay && Mover(Character) && Ragdoll(Character)
			&& Ragdoll(Character)->IsRagdollPresentationReady()
			&& Extension->HasReachedInitState(Tag(TEXT("InitState.GameplayReady"))) && Gameplay->HasReachedInitState(Tag(TEXT("InitState.GameplayReady")))
			&& Extension->GetPawnData<URpgPawnData>() && Extension->GetPawnData<URpgPawnData>()->GetPathName() == PawnDataPath
			&& State->GetPawnData<URpgPawnData>() == Extension->GetPawnData<URpgPawnData>()
			&& ASC(Character) == State->GetRpgAbilitySystemComponent() && ASC(Character) && ASC(Character)->GetAvatarActor() == Character
			&& ASC(Character)->AbilityActorInfo.IsValid() && ASC(Character)->AbilityActorInfo->SkeletalMeshComponent.Get() == Mesh(Character)
			&& Mesh(Character) && Mesh(Character)->GetAnimInstance() && Mover(Character)->GetPrimaryVisualComponent() == Mesh(Character)
			&& Health(Character) && !Health(Character)->IsDeadOrDying() && Equipped(Character) && ControlsReady(Character)
			&& (Character->GetLocalRole() == ROLE_SimulatedProxy || RagdollSpec(Character))
			&& (!Character->IsLocallyControlled() || Gameplay->IsReadyToBindInputs());
	}
	bool EntrySupport(APawn* Character, FHitResult& Floor, float& FloorGap)
	{
		const UCapsuleComponent* Capsule = Character ? Cast<UCapsuleComponent>(Character->GetRootComponent()) : nullptr;
		FloorGap = TNumericLimits<float>::Max();
		if (!Capsule || !Character->GetWorld() || !Capsule->IsQueryCollisionEnabled()) return false;
		const FVector Center = Capsule->GetComponentLocation();
		const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		FCollisionQueryParams Query(SCENE_QUERY_STAT(RpgRagdollTestEntryFloor), false, Character);
		if (!Character->GetWorld()->LineTraceSingleByChannel(Floor, Center, Center - FVector(0, 0, HalfHeight + 10.0f), ECC_Visibility, Query)) return false;
		FloorGap = Center.Z - HalfHeight - Floor.ImpactPoint.Z;
		const UPrimitiveComponent* Support = Floor.GetComponent();
		return Support && Support->Mobility == EComponentMobility::Static && Support->GetCollisionObjectType() == ECC_WorldStatic && !Support->IsSimulatingPhysics()
			&& !Floor.bStartPenetrating && Floor.ImpactNormal.Z >= 0.99f && FMath::Abs(FloorGap) <= 5.0f
			&& Mover(Character) && Mover(Character)->IsTraversalWalkable(Floor);
	}
	bool EntryReady(UWorld* World, APawn* Character)
	{
		FHitResult Floor; float FloorGap;
		return Ready(World, Character) && Ragdoll(Character)->GetRagdollPhase() == ERpgMoverRagdollPhase::Inactive
			&& Mover(Character)->CanBeginRagdoll(Ragdoll(Character)->MaximumEntrySpeed) && EntrySupport(Character, Floor, FloorGap);
	}
	void ReportEntry(const TCHAR* Stage, UWorld* World, APawn* Character)
	{
		URpgCharacterMoverComponent* Movement = Mover(Character);
		URpgMoverRagdollComponent* Component = Ragdoll(Character);
		URpgAbilitySystemComponent* AbilitySystem = ASC(Character);
		const FGameplayAbilitySpec* Spec = RagdollSpec(Character);
		const FMoverDefaultSyncState* Sync = Movement ? Movement->GetSyncState().SyncStateCollection.FindDataByType<FMoverDefaultSyncState>() : nullptr;
		FHitResult Floor; float FloorGap;
		const bool bSupported = EntrySupport(Character, Floor, FloorGap);
		UE_LOG(LogTemp, Display, TEXT("RpgMoverRagdollEntry stage=%s world=%s pawn=%s role=%d ready=%d presentation=%d grounded=%d canBegin=%d speed=%.3f phase=%d ability=%s active=%d inputPressed=%d avatar=%s supported=%d floor=%s normalZ=%.4f gap=%.3f location=%s"),
			Stage, *GetPathNameSafe(World), *GetNameSafe(Character), Character ? static_cast<int32>(Character->GetLocalRole()) : -1,
			Ready(World, Character), Component && Component->IsRagdollPresentationReady(), Movement && Movement->IsOnGround(),
			Movement && Component && Movement->CanBeginRagdoll(Component->MaximumEntrySpeed), Sync ? Sync->GetVelocity_WorldSpace().Size() : -1.0,
			Component ? static_cast<int32>(Component->GetRagdollPhase()) : -1, Spec ? *GetNameSafe(Spec->Ability) : TEXT("missing"),
			Spec && Spec->IsActive(), Spec && Spec->InputPressed, AbilitySystem ? *GetNameSafe(AbilitySystem->GetAvatarActor()) : TEXT("missing"),
			bSupported, *GetPathNameSafe(Floor.GetComponent()), Floor.ImpactNormal.Z, FloorGap,
			Character ? *Character->GetActorLocation().ToCompactString() : TEXT("missing"));
	}
	void ReportPhysics(const TCHAR* Stage, UWorld* World, APawn* Character)
	{
		USkeletalMeshComponent* Source = Mesh(Character);
		if (!Source) return;
		const FBodyInstance* RootBody = Source->GetBodyInstance();
		UE_LOG(LogTemp, Display, TEXT("RpgMoverRagdollPhysics stage=%s world=%s pawn=%s meshSim=%d rootBodySim=%d physicsTransformMode=%d meshCollision=%d worldStaticResponse=%d capsule=%s meshWorld=%s meshRelative=%s"),
			Stage, *GetPathNameSafe(World), *GetNameSafe(Character), Source->IsSimulatingPhysics(),
			RootBody && RootBody->IsInstanceSimulatingPhysics(), static_cast<int32>(Source->PhysicsTransformUpdateMode),
			static_cast<int32>(Source->GetCollisionEnabled()), static_cast<int32>(Source->GetCollisionResponseToChannel(ECC_WorldStatic)),
			*Character->GetActorTransform().ToHumanReadableString(), *Source->GetComponentTransform().ToHumanReadableString(), *Source->GetRelativeTransform().ToHumanReadableString());
		for (const FName Bone : { FName(TEXT("pelvis")), FName(TEXT("spine_05")), FName(TEXT("thigh_l")), FName(TEXT("upperarm_l")) })
		{
			const FBodyInstance* Body = Source->GetBodyInstance(Bone);
			const int32 BoneIndex = Source->GetBoneIndex(Bone);
			if (!Body || BoneIndex == INDEX_NONE)
			{
				UE_LOG(LogTemp, Display, TEXT("RpgMoverRagdollPhysics bone=%s body=%d boneIndex=%d"), *Bone.ToString(), Body != nullptr, BoneIndex);
				continue;
			}
			const FTransform Physical = Body->GetUnrealWorldTransform();
			const FTransform Visual = Source->GetBoneTransform(BoneIndex);
			UE_LOG(LogTemp, Display, TEXT("RpgMoverRagdollPhysics bone=%s sim=%d blend=%.4f collision=%d distance=%.4f physical=%s visual=%s"),
				*Bone.ToString(), Body->IsInstanceSimulatingPhysics(), Body->PhysicsBlendWeight, static_cast<int32>(Body->GetCollisionEnabled()),
				FVector::Distance(Physical.GetLocation(), Visual.GetLocation()), *Physical.ToHumanReadableString(), *Visual.ToHumanReadableString());
		}
	}
	void ReportInput(const TCHAR* Stage, APlayerController* Controller, bool bReportMappings)
	{
		APawn* Character = Controller ? Controller->GetPawn() : nullptr;
		const UEnhancedPlayerInput* PlayerInput = Controller ? Cast<UEnhancedPlayerInput>(Controller->PlayerInput) : nullptr;
		const URpgPawnExtensionComponent* Extension = URpgPawnExtensionComponent::FindPawnExtensionComponent(Character);
		const URpgPawnData* Data = Extension ? Extension->GetPawnData<URpgPawnData>() : nullptr;
		const URpgInputConfig* Config = Data ? Data->InputConfig.Get() : nullptr;
		const UInputAction* Expected = Config ? Config->FindAbilityInputActionForTag(Tag(TEXT("InputTag.Ability.Ragdoll")), false) : nullptr;
		const FInputActionInstance* ActionInstance = PlayerInput && Expected ? PlayerInput->FindActionInstanceData(Expected) : nullptr;
		const FGameplayAbilitySpec* Spec = RagdollSpec(Character);
		UE_LOG(LogTemp, Display, TEXT("RpgMoverRagdollInput stage=%s frame=%llu world=%s controller=%s local=%d playerInput=%s RDown=%d RValue=%.3f config=%s expected=%s instance=%d value=%s triggerEvent=%d specPressed=%d specActive=%d"),
			Stage, GFrameCounter, *GetPathNameSafe(Controller ? Controller->GetWorld() : nullptr), *GetNameSafe(Controller),
			Controller && Controller->IsLocalController(), *GetNameSafe(PlayerInput), PlayerInput && PlayerInput->IsPressed(EKeys::R),
			PlayerInput ? PlayerInput->GetKeyValue(EKeys::R) : -1.0f, *GetPathNameSafe(Config), *GetPathNameSafe(Expected), ActionInstance != nullptr,
			ActionInstance ? *ActionInstance->GetValue().ToString() : TEXT("missing"),
			ActionInstance ? static_cast<int32>(ActionInstance->GetTriggerEvent()) : -1,
			Spec && Spec->InputPressed, Spec && Spec->IsActive());
		if (!bReportMappings || !Controller) return;
		const ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
		const UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
		// Enumerate only already loaded contexts, then ask the real local subsystem which ones are applied.
		if (Subsystem) for (TObjectIterator<UInputMappingContext> It; It; ++It)
		{
			int32 Priority = INDEX_NONE;
			if (Subsystem->HasMappingContext(*It, Priority))
				UE_LOG(LogTemp, Display, TEXT("RpgMoverRagdollInput IMC=%s priority=%d"), *It->GetPathName(), Priority);
		}
		if (PlayerInput) for (const FEnhancedActionKeyMapping& Mapping : PlayerInput->GetEnhancedActionMappingsView())
		{
			if (Mapping.Key != EKeys::R) continue;
			UE_LOG(LogTemp, Display, TEXT("RpgMoverRagdollInput RMapping action=%s expected=%d consume=%d ignored=%d triggers=%d modifiers=%d"),
				*GetPathNameSafe(Mapping.Action), Mapping.Action == Expected, Mapping.Action && Mapping.Action->bConsumeInput,
				Mapping.bShouldBeIgnored, Mapping.Triggers.Num(), Mapping.Modifiers.Num());
		}
		const UEnhancedInputComponent* InputComponent = Character ? Cast<UEnhancedInputComponent>(Character->InputComponent) : nullptr;
		int32 ExpectedBindings = 0;
		if (InputComponent && Expected) for (const auto& Binding : InputComponent->GetActionEventBindings())
		{
			if (Binding->GetAction() != Expected) continue;
			++ExpectedBindings;
			UE_LOG(LogTemp, Display, TEXT("RpgMoverRagdollInput binding component=%s action=%s event=%d target=%s consumes=%d"),
				*GetPathNameSafe(InputComponent), *GetPathNameSafe(Expected), static_cast<int32>(Binding->GetTriggerEvent()),
				*GetPathNameSafe(Binding->GetUObject()), Binding->ShouldConsume());
		}
		UE_LOG(LogTemp, Display, TEXT("RpgMoverRagdollInput expectedBindings=%d inputComponent=%s subsystem=%s"),
			ExpectedBindings, *GetPathNameSafe(InputComponent), *GetNameSafe(Subsystem));
	}
	/** Use ordinary starts and replicated floor collision; no test ever teleports or sets a pawn mode. */
	class FScopedWorld final
	{
	public:
		~FScopedWorld() { FGameModeEvents::OnGameModeInitializedEvent().Remove(Handle); }
		void Start()
		{
			Prefix = TEXT("SurvivalRpg_RagdollAutomation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
			Handle = FGameModeEvents::OnGameModeInitializedEvent().AddRaw(this, &FScopedWorld::Initialize);
		}
	private:
		void Initialize(AGameModeBase* Initialized)
		{
			ARpgGameModeBase* Mode = Cast<ARpgGameModeBase>(Initialized);
			if (!Mode || Mode->GetWorld()->WorldType != EWorldType::PIE) return;
			Mode->bEnableDiskPersistence = false; Mode->WorldSaveSlotName = Prefix;
			Mode->WorldSaveBackupSlotName = Prefix + TEXT("_Backup"); Mode->WorldSaveRecoverySlotName = Prefix + TEXT("_Recovery");
			Mode->OfflineProfileKey = Prefix;
			FActorSpawnParameters Spawn; Spawn.ObjectFlags |= RF_Transient;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Mode->GetWorld()->SpawnActor<ARpgMoverRagdollFloorFixture>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
			for (int32 Index = 0; Index < 3; ++Index)
				Mode->GetWorld()->SpawnActor<APlayerStart>(FVector(0, (Index - 1) * 500.0, 120.0), FRotator::ZeroRotator, Spawn);
		}
		FString Prefix;
		FDelegateHandle Handle;
	};
	class FScopedInput final
	{
	public:
		~FScopedInput() { Stop(); }
		void Start(APawn* Character)
		{
			Stop(); PC = Cast<APlayerController>(Character->GetController());
			PC->SetIgnoreLookInput(true); PC->SetControlRotation(FRotator::ZeroRotator);
			Handle = FWorldDelegates::OnWorldTickStart.AddRaw(this, &FScopedInput::Tick);
			AfterHandle = FWorldDelegates::OnWorldTickEnd.AddRaw(this, &FScopedInput::AfterTick);
		}
		void Press(FKey Key)
		{
			if (Key == EKeys::R) ReportInput(TEXT("BeforePress"), PC.Get(), true);
			SetKey(Key, true); ReleaseKey = Key; ReleaseFrames = 2;
			if (Key == EKeys::R) { DiagnosticFrames = 4; ReportInput(TEXT("AfterPress"), PC.Get(), false); }
		}
		void Move(bool bEnabled) { Axis = bEnabled ? 1.0f : 0.0f; }
		void Stop()
		{
			FWorldDelegates::OnWorldTickStart.Remove(Handle); Handle.Reset();
			FWorldDelegates::OnWorldTickEnd.Remove(AfterHandle); AfterHandle.Reset();
			if (PC.IsValid())
			{
				SetKey(EKeys::R, false); SetKey(EKeys::LeftMouseButton, false);
				PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_LeftY, IE_Axis, 0.0f, 1));
				PC->SetIgnoreLookInput(false);
			}
			PC.Reset(); Axis = 0.0f; ReleaseFrames = 0; DiagnosticFrames = 0;
		}
	private:
		void SetKey(FKey Key, bool bPressed) { if (PC.IsValid()) PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key, bPressed ? IE_Pressed : IE_Released, bPressed ? 1.0f : 0.0f)); }
		void Tick(UWorld* World, ELevelTick, float)
		{
			if (!PC.IsValid() || PC->GetWorld() != World) return;
			PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_LeftY, IE_Axis, Axis, 1));
			if (ReleaseFrames > 0 && --ReleaseFrames == 0) SetKey(ReleaseKey, false);
		}
		void AfterTick(UWorld* World, ELevelTick, float)
		{
			if (!PC.IsValid() || PC->GetWorld() != World || DiagnosticFrames <= 0) return;
			--DiagnosticFrames;
			ReportInput(TEXT("AfterInputTick"), PC.Get(), false);
		}
		TWeakObjectPtr<APlayerController> PC;
		FDelegateHandle Handle, AfterHandle;
		FKey ReleaseKey;
		float Axis = 0.0f;
		int32 ReleaseFrames = 0;
		int32 DiagnosticFrames = 0;
	};
	/** Capture historical cache/body warnings from startup, without suppressing them or retrying until hidden. */
	class FScopedPhysicsWarnings final : public FOutputDevice
	{
	public:
		~FScopedPhysicsWarnings() { Stop(); }
		void Start() { if (GLog && !bListening) { GLog->AddOutputDevice(this); bListening = true; } }
		void Stop() { if (GLog && bListening) GLog->RemoveOutputDevice(this); bListening = false; }
		TArray<FString> Get() { FScopeLock Lock(&Mutex); return Messages; }
		virtual void Serialize(const TCHAR* Data, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			const bool bPhysicsControlWarning = Category == FName(TEXT("LogPhysicsControl"));
			const bool bCompetingMeshMove = Category == FName(TEXT("PIE")) &&
				FCString::Strstr(Data, TEXT("Attempting to move a fully simulated skeletal mesh")) != nullptr;
			const bool bInvalidSimulation = Category == FName(TEXT("LogPhysics")) &&
				FCString::Strstr(Data, TEXT("Invalid Simulate Options")) != nullptr;
			const bool bInvalidMovementInput = Category == FName(TEXT("LogMover")) &&
				FCString::Strstr(Data, TEXT("Unhandled MoveInputType")) != nullptr;
			if ((!bPhysicsControlWarning && !bCompetingMeshMove && !bInvalidSimulation && !bInvalidMovementInput) || Verbosity > ELogVerbosity::Warning) return;
			FScopeLock Lock(&Mutex);
			if (Messages.Num() < 20) Messages.Add(Data);
		}
	private:
		FCriticalSection Mutex;
		TArray<FString> Messages;
		bool bListening = false;
	};
	struct FBodyBaseline
	{
		bool bSimulating = false;
		float BlendWeight = 0.0f;
		ECollisionEnabled::Type Collision = ECollisionEnabled::NoCollision;
	};
	struct FPeer
	{
		TWeakObjectPtr<ARpgPlayerState> PlayerState;
		TWeakObjectPtr<URpgAbilitySystemComponent> AbilitySystem;
		TWeakObjectPtr<APawn> OriginalPawn;
		TWeakObjectPtr<USkeletalMeshComponent> OriginalMesh;
		TWeakObjectPtr<UPhysicsControlComponent> OriginalControls;
		FDelegateHandle ActivatedHandle, EndedHandle;
		FGameplayAbilitySpecHandle RagdollHandle;
		TArray<TWeakObjectPtr<AActor>> EquipmentActors;
		TMap<FName, FPhysicsControlData> InitialControlData;
		TMap<FName, FBodyBaseline> InitialBodyState;
		TArray<FVector> InitialBodyPositions;
		FRpgMoverRagdollState EntryCommand, GetupCommand;
		FVector Anchor = FVector::ZeroVector, MovementStart = FVector::ZeroVector;
		FTransform MeshRelative = FTransform::Identity;
		FTransform LastRagdollCapsule = FTransform::Identity, LastRagdollMesh = FTransform::Identity, LastRagdollPelvis = FTransform::Identity;
		double LastRagdollTime = 0.0;
		ECollisionEnabled::Type CapsuleCollision = ECollisionEnabled::NoCollision;
		ECollisionResponse PawnResponse = ECR_Ignore;
		float RagdollSeconds = 0.0f, ContinuousRagdollSeconds = 0.0f, MaxAnchorDrift = 0.0f, GetupFirstTime = -1.0f, GetupLastTime = -1.0f;
		float ContinuousSupportedPoseSeconds = 0.0f, FloorHeight = 0.0f, LowestBodyHeight = 0.0f, HighestBodyHeight = 0.0f;
		int32 GetupInstance = INDEX_NONE, RetiredGetupInstance = INDEX_NONE, RetiredEpisode = 0, RagdollEntries = 0;
		ERpgMoverRagdollPhase PreviousPhase = ERpgMoverRagdollPhase::Inactive;
		bool bHadNormalBaseline = false, bSawPhysicalPose = false, bPoseMoved = false, bSawGetup = false;
		bool bInvalid = false, bSawDeath = false, bGetupAfterDeath = false, bSawAttack = false;
		bool bTrackMovement = false;
		int32 GetupBoundarySamples = 0;
		int32 Activations = 0, AbilityEnds = 0, EntryDiagnosticSamples = 0;
		int32 PhysicsDiagnosticSamples = 0;
		bool bReportedMissingPhysicalPose = false;
		bool bCurrentSupportedPhysicalPose = false;
	};
	/** Samples naturally completed world ticks. Cache targets are deliberately not used as physical pose evidence. */
	class FScopedObservations final
	{
	public:
		~FScopedObservations() { Stop(); }
		void Start(int32 Id) { Subject = Id; Handle = FWorldDelegates::OnWorldTickEnd.AddRaw(this, &FScopedObservations::Tick); }
		void Add(UWorld* World)
		{
			if (Peers.Contains(World)) return;
			APawn* Character = Pawn(World, Subject);
			if (!Character || !Ragdoll(Character) || !ControlsReady(Character)) return;
			FPeer& Peer = Peers.Add(World);
			Peer.PlayerState = Player(World, Subject); Peer.AbilitySystem = ASC(Character); Peer.OriginalPawn = Character;
			Peer.OriginalMesh = Mesh(Character); Peer.OriginalControls = Controls(Character); Peer.Anchor = Character->GetActorLocation();
			if (const FGameplayAbilitySpec* Spec = RagdollSpec(Character)) Peer.RagdollHandle = Spec->Handle;
			Peer.ActivatedHandle = ASC(Character)->AbilityActivatedCallbacks.AddLambda([this, World](UGameplayAbility* Ability)
			{
				FPeer* Record = Peers.Find(World);
				if (!Record || !Ability || Ability->GetCurrentAbilitySpecHandle() != Record->RagdollHandle) return;
				++Record->Activations; ReportEntry(TEXT("AbilityActivated"), World, Pawn(World, Subject));
			});
			Peer.EndedHandle = ASC(Character)->OnAbilityEnded.AddLambda([this, World](const FAbilityEndedData& Ended)
			{
				FPeer* Record = Peers.Find(World);
				if (!Record || !Ended.AbilityThatEnded || Ended.AbilityThatEnded->GetCurrentAbilitySpecHandle() != Record->RagdollHandle) return;
				++Record->AbilityEnds; ReportEntry(Ended.bWasCancelled ? TEXT("AbilityCancelled") : TEXT("AbilityEnded"), World, Pawn(World, Subject));
			});
			Peer.MeshRelative = Mesh(Character)->GetRelativeTransform();
			Peer.bHadNormalBaseline = Ragdoll(Character)->GetRagdollPhase() == ERpgMoverRagdollPhase::Inactive;
			const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Character->GetRootComponent());
			Peer.CapsuleCollision = Capsule->GetCollisionEnabled(); Peer.PawnResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
			if (Peer.bHadNormalBaseline) for (const FName Name : Controls(Character)->GetAllControlNames())
			{
				FPhysicsControlData Data;
				if (Controls(Character)->GetControlData(Name, Data)) Peer.InitialControlData.Add(Name, Data);
			}
			if (Peer.bHadNormalBaseline) for (const USkeletalBodySetup* Setup : Mesh(Character)->GetPhysicsAsset()->SkeletalBodySetups)
			{
				const FBodyInstance* Body = Setup ? Mesh(Character)->GetBodyInstance(Setup->BoneName) : nullptr;
				if (Body) Peer.InitialBodyState.Add(Setup->BoneName, { Body->IsInstanceSimulatingPhysics(), Body->PhysicsBlendWeight, Body->GetCollisionEnabled() });
			}
			for (const FName Bone : Bones()) Peer.InitialBodyPositions.Add(Mesh(Character)->GetBodyInstance(Bone)->GetUnrealWorldTransform().GetLocation());
			const auto* Equipment = Character->FindComponentByClass<URpgEquipmentManagerComponent>();
			for (ERpgEquipmentSlot Slot : { ERpgEquipmentSlot::MainHand, ERpgEquipmentSlot::OffHand })
				for (AActor* Actor : Equipment->GetEquipmentInstanceInSlot(Slot)->GetSpawnedActors()) Peer.EquipmentActors.Add(Actor);
		}
		bool All(TFunctionRef<bool(UWorld*, const FPeer&)> Predicate, int32 Count = 3) const
		{
			if (Peers.Num() != Count) return false;
			for (const auto& Entry : Peers) if (!Entry.Key.IsValid() || !Predicate(Entry.Key.Get(), Entry.Value)) return false;
			return true;
		}
		const FPeer& Get(UWorld* World) const { return Peers.FindChecked(World); }
		void RetireGetup()
		{
			for (auto& Entry : Peers)
			{
				FPeer& Peer = Entry.Value;
				Peer.RetiredEpisode = Peer.EntryCommand.Episode;
				Peer.RetiredGetupInstance = Peer.GetupInstance; Peer.GetupInstance = INDEX_NONE;
				Peer.bSawGetup = false; Peer.GetupFirstTime = -1.0f; Peer.GetupLastTime = -1.0f;
				Peer.bSawPhysicalPose = false; Peer.bPoseMoved = false;
				Peer.GetupBoundarySamples = 0; Peer.LastRagdollTime = 0.0;
				Peer.PhysicsDiagnosticSamples = 0; Peer.bReportedMissingPhysicalPose = false;
				Peer.ContinuousSupportedPoseSeconds = 0.0f; Peer.bCurrentSupportedPhysicalPose = false;
				Peer.InitialBodyPositions.Reset();
				for (const FName Bone : Bones()) Peer.InitialBodyPositions.Add(Mesh(Pawn(Entry.Key.Get(), Subject))->GetBodyInstance(Bone)->GetUnrealWorldTransform().GetLocation());
			}
		}
		void BeginMovement()
		{
			for (auto& Entry : Peers)
			{
				APawn* Character = Pawn(Entry.Key.Get(), Subject);
				Entry.Value.MovementStart = Character->GetActorLocation(); Entry.Value.bTrackMovement = true; Entry.Value.bSawAttack = false;
			}
		}
		void Stop()
		{
			FWorldDelegates::OnWorldTickEnd.Remove(Handle); Handle.Reset();
			for (auto& Entry : Peers) if (URpgAbilitySystemComponent* AbilitySystem = Entry.Value.AbilitySystem.Get())
			{
				AbilitySystem->AbilityActivatedCallbacks.Remove(Entry.Value.ActivatedHandle);
				AbilitySystem->OnAbilityEnded.Remove(Entry.Value.EndedHandle);
				Entry.Value.ActivatedHandle.Reset(); Entry.Value.EndedHandle.Reset();
			}
		}
		void Report() const
		{
			for (const auto& Entry : Peers)
			{
				const FPeer& Peer = Entry.Value;
				UE_LOG(LogTemp, Display, TEXT("RpgMoverRagdollTest world=%s baseline=%d entries=%d ragdollSeconds=%.3f physicalPose=%d poseMoved=%d anchorDrift=%.3f getup=%d instance=%d times=%.3f/%.3f invalid=%d death=%d getupAfterDeath=%d attack=%d"),
					*GetPathNameSafe(Entry.Key.Get()), Peer.bHadNormalBaseline, Peer.RagdollEntries, Peer.RagdollSeconds, Peer.bSawPhysicalPose,
					Peer.bPoseMoved, Peer.MaxAnchorDrift, Peer.bSawGetup, Peer.GetupInstance, Peer.GetupFirstTime, Peer.GetupLastTime,
					Peer.bInvalid, Peer.bSawDeath, Peer.bGetupAfterDeath, Peer.bSawAttack);
				UE_LOG(LogTemp, Display, TEXT("RpgMoverRagdollTest GAS world=%s activated=%d ended=%d"), *GetPathNameSafe(Entry.Key.Get()), Peer.Activations, Peer.AbilityEnds);
				UE_LOG(LogTemp, Display, TEXT("RpgMoverRagdollTest floor world=%s currentSupportedPose=%d sustained=%.3f floorZ=%.3f boundsAboveFloor=%.3f/%.3f contactMargin=%.3f"),
					*GetPathNameSafe(Entry.Key.Get()), Peer.bCurrentSupportedPhysicalPose, Peer.ContinuousSupportedPoseSeconds,
					Peer.FloorHeight, Peer.LowestBodyHeight, Peer.HighestBodyHeight, GetDefault<UPhysicsSettings>()->MaxContactOffset);
				if (Peer.RagdollEntries == 0) ReportEntry(TEXT("NoEntryAtTeardown"), Entry.Key.Get(), Peer.OriginalPawn.Get());
				if (Peer.RagdollEntries > 0 && !Peer.bCurrentSupportedPhysicalPose) ReportPhysics(TEXT("MissingSupportedPhysicalPoseAtTeardown"), Entry.Key.Get(), Peer.OriginalPawn.Get());
			}
		}
	private:
		static TArray<FName> Bones() { return { TEXT("pelvis"), TEXT("spine_05"), TEXT("thigh_l"), TEXT("upperarm_l") }; }
		void Tick(UWorld* World, ELevelTick, float DeltaSeconds)
		{
			FPeer* Peer = Peers.Find(World);
			APawn* Character = Peer ? Pawn(World, Subject) : nullptr;
			// UnPossess clears PlayerState.Pawn before the old actor finishes its normal death lifetime.
			if (!Character && Peer) Character = Peer->OriginalPawn.Get();
			if (!Peer || !Character || !Ragdoll(Character) || !Mesh(Character) || !Health(Character)) return;
			const ERpgMoverRagdollPhase Phase = Ragdoll(Character)->GetRagdollPhase();
			const FRpgMoverRagdollState Command = Ragdoll(Character)->GetRagdollState();
			const bool bOriginal = Character == Peer->OriginalPawn.Get();
			if (bOriginal)
			{
				if (Peer->EntryDiagnosticSamples++ < 4) ReportEntry(TEXT("InitialObservedTick"), World, Character);
				Peer->bInvalid |= Mesh(Character) != Peer->OriginalMesh.Get() || Controls(Character) != Peer->OriginalControls.Get();
				Peer->bSawDeath |= Health(Character)->IsDeadOrDying();
				// Replicated command phase may lag Health, but the presentation gate must already reject resurrection.
				Peer->bGetupAfterDeath |= Peer->bSawDeath && Ragdoll(Character)->IsRagdollActive();
				if (Peer->bSawDeath && Peer->GetupCommand.GetUpMontage && Mesh(Character)->GetAnimInstance())
					Peer->bGetupAfterDeath |= Mesh(Character)->GetAnimInstance()->Montage_IsPlaying(Peer->GetupCommand.GetUpMontage);
				if (Phase == ERpgMoverRagdollPhase::Ragdoll)
				{
					if (Peer->PreviousPhase != Phase)
					{
						++Peer->RagdollEntries; Peer->EntryCommand = Command;
						Peer->Anchor = Command.Anchor.GetLocation(); Peer->MaxAnchorDrift = 0.0f;
					}
					Peer->RagdollSeconds += DeltaSeconds;
					Peer->ContinuousRagdollSeconds += DeltaSeconds;
					Peer->MaxAnchorDrift = FMath::Max(Peer->MaxAnchorDrift, static_cast<float>(FVector::Distance(Peer->Anchor, Character->GetActorLocation())));
					bool bPhysicalPose = true;
					const TArray<FName> Names = Bones();
					for (int32 Index = 0; Index < Names.Num(); ++Index)
					{
						FBodyInstance* Body = Mesh(Character)->GetBodyInstance(Names[Index]);
						if (!Body || !Body->IsInstanceSimulatingPhysics() || !CollisionEnabledHasPhysics(Body->GetCollisionEnabled())) { bPhysicalPose = false; continue; }
						const FTransform Physical = Body->GetUnrealWorldTransform();
						const FTransform Visual = Mesh(Character)->GetBoneTransform(Mesh(Character)->GetBoneIndex(Names[Index]));
						Peer->bInvalid |= Physical.ContainsNaN() || Visual.ContainsNaN();
						bPhysicalPose &= !Physical.ContainsNaN() && !Visual.ContainsNaN() && Physical.GetLocation().Equals(Visual.GetLocation(), 10.0);
						// The capsule is anchored in this pilot, so body displacement cannot be explained by locomotion.
						Peer->bPoseMoved |= FVector::Distance(Physical.GetLocation(), Peer->InitialBodyPositions[Index]) > 5.0;
					}
					Peer->bSawPhysicalPose |= bPhysicalPose;
					FHitResult Floor; float FloorGap;
					bool bSupported = EntrySupport(Character, Floor, FloorGap);
					Peer->FloorHeight = Floor.ImpactPoint.Z;
					Peer->LowestBodyHeight = TNumericLimits<float>::Max(); Peer->HighestBodyHeight = TNumericLimits<float>::Lowest();
					bool bFoundSimulatedBounds = false;
					for (const USkeletalBodySetup* Setup : Mesh(Character)->GetPhysicsAsset()->SkeletalBodySetups)
					{
						const FBodyInstance* Body = Setup ? Mesh(Character)->GetBodyInstance(Setup->BoneName) : nullptr;
						if (!Body || !Body->IsInstanceSimulatingPhysics()) continue;
						const FBox Bounds = Body->GetBodyBounds();
						if (!Bounds.IsValid || Bounds.Min.ContainsNaN() || Bounds.Max.ContainsNaN()) { bSupported = false; continue; }
						bFoundSimulatedBounds = true;
						bSupported &= CollisionEnabledHasPhysics(Body->GetCollisionEnabled());
						Peer->LowestBodyHeight = FMath::Min(Peer->LowestBodyHeight, static_cast<float>(Bounds.Min.Z - Peer->FloorHeight));
						Peer->HighestBodyHeight = FMath::Max(Peer->HighestBodyHeight, static_cast<float>(Bounds.Max.Z - Peer->FloorHeight));
					}
					// Use the project's physics contact margin and capsule height, not a particular lying pose.
					const float ContactMargin = GetDefault<UPhysicsSettings>()->MaxContactOffset;
					const UCapsuleComponent* Capsule = CastChecked<UCapsuleComponent>(Character->GetRootComponent());
					bSupported &= bFoundSimulatedBounds && Peer->LowestBodyHeight >= -ContactMargin && Peer->LowestBodyHeight <= ContactMargin
						&& Peer->HighestBodyHeight <= 2.0f * Capsule->GetScaledCapsuleHalfHeight() + ContactMargin;
					Peer->bCurrentSupportedPhysicalPose = bSupported && bPhysicalPose;
					Peer->ContinuousSupportedPoseSeconds = Peer->bCurrentSupportedPhysicalPose ? Peer->ContinuousSupportedPoseSeconds + DeltaSeconds : 0.0f;
					if (Peer->PhysicsDiagnosticSamples++ < 4) ReportPhysics(TEXT("FirstRagdollTicks"), World, Character);
					else if (!Peer->bCurrentSupportedPhysicalPose && !Peer->bReportedMissingPhysicalPose && Peer->ContinuousRagdollSeconds > 1.0f)
					{
						Peer->bReportedMissingPhysicalPose = true; ReportPhysics(TEXT("PhysicalPoseMissingAfterOneSecond"), World, Character);
					}
					Peer->LastRagdollCapsule = Character->GetActorTransform();
					Peer->LastRagdollMesh = Mesh(Character)->GetComponentTransform();
					Peer->LastRagdollPelvis = Mesh(Character)->GetBoneTransform(Mesh(Character)->GetBoneIndex(TEXT("pelvis")));
					Peer->LastRagdollTime = World->GetTimeSeconds();
				}
				else Peer->ContinuousRagdollSeconds = 0.0f;
				if (Phase == ERpgMoverRagdollPhase::GettingUp)
				{
					// Bounded passive boundary evidence for visual review, not an invented pose-continuity threshold.
					if (Peer->LastRagdollTime > 0.0 && Peer->GetupBoundarySamples < 4)
					{
						const FTransform CapsuleTransform = Character->GetActorTransform();
						const FTransform MeshTransform = Mesh(Character)->GetComponentTransform();
						const FTransform PelvisTransform = Mesh(Character)->GetBoneTransform(Mesh(Character)->GetBoneIndex(TEXT("pelvis")));
						UE_LOG(LogTemp, Display, TEXT("RpgMoverRagdollBoundary world=%s episode=%d sample=%d elapsed=%.4f capsule=%s->%s mesh=%s->%s pelvis=%s->%s"),
							*GetPathNameSafe(World), Command.Episode, Peer->GetupBoundarySamples++, World->GetTimeSeconds() - Peer->LastRagdollTime,
							*Peer->LastRagdollCapsule.ToHumanReadableString(), *CapsuleTransform.ToHumanReadableString(),
							*Peer->LastRagdollMesh.ToHumanReadableString(), *MeshTransform.ToHumanReadableString(),
							*Peer->LastRagdollPelvis.ToHumanReadableString(), *PelvisTransform.ToHumanReadableString());
					}
					UAnimInstance* Animation = Mesh(Character)->GetAnimInstance();
					UAnimMontage* Montage = Command.GetUpMontage;
					const FAnimMontageInstance* Instance = Montage && Animation ? Animation->GetActiveInstanceForMontage(Montage) : nullptr;
					if (Instance)
					{
						if (!Peer->bSawGetup)
						{
							Peer->GetupInstance = Instance->GetInstanceID(); Peer->GetupFirstTime = Instance->GetPosition(); Peer->GetupCommand = Command;
							UE_LOG(LogTemp, Display, TEXT("RpgMoverRagdollGetup first world=%s role=%d episode=%d entryEpisode=%d revision=%d montage=%s instance=%d selectedStart=%.6f localPosition=%.6f previouslyInvalid=%d"),
								*GetPathNameSafe(World), static_cast<int32>(Character->GetLocalRole()), Command.Episode, Peer->EntryCommand.Episode,
								Command.Revision, *GetPathNameSafe(Montage), Peer->GetupInstance, Command.GetUpStartTime, Instance->GetPosition(), Peer->bInvalid);
						}
						// UE 5.8.2 ASC OnRep_ReplicatedAnimMontage starts proxies at zero and seeks only when
						// abs(error) > MONTAGE_REP_POS_ERR_THRESH (0.1s). Server/owner tasks receive the exact selected start.
						const float StartPositionTolerance = Character->GetLocalRole() == ROLE_SimulatedProxy ? 0.1f : 0.01f;
						Peer->bInvalid |= Peer->bSawGetup && Peer->GetupInstance != Instance->GetInstanceID();
						Peer->bInvalid |= !Ragdoll(Character)->IsGetUpMontage(Montage) || !FMath::IsFinite(Command.GetUpStartTime)
							|| Command.GetUpStartTime < 0.0f || Command.GetUpStartTime >= Montage->GetPlayLength()
							|| Instance->GetPosition() + StartPositionTolerance < Command.GetUpStartTime || Command.Episode != Peer->EntryCommand.Episode;
						Peer->bSawGetup = true; Peer->GetupLastTime = FMath::Max(Peer->GetupLastTime, Instance->GetPosition());
					}
				}
				Peer->PreviousPhase = Phase;
			}
			if (Peer->bTrackMovement && ASC(Character)) Peer->bSawAttack |= ASC(Character)->GetCurrentMontage() != nullptr
				&& Mesh(Character)->GetAnimInstance()->Montage_IsPlaying(ASC(Character)->GetCurrentMontage());
		}
		int32 Subject = INDEX_NONE;
		FDelegateHandle Handle;
		TMap<TWeakObjectPtr<UWorld>, FPeer> Peers;
	};
	struct FState : FBasePIENetworkComponentState {};
}

NETWORK_TEST_CLASS(GaspMoverRagdollPIE, "SurvivalRpg.GASP.Mover.Ragdoll")
{
	using FState = RpgGaspMoverRagdollTests::FState;
	using EScenario = RpgGaspMoverRagdollTests::EScenario;
	RpgGaspMoverRagdollTests::FScopedWorld Isolation;
	RpgGaspMoverRagdollTests::FScopedInput Input;
	RpgGaspMoverRagdollTests::FScopedObservations Observations;
	RpgGaspMoverRagdollTests::FScopedPhysicsWarnings PhysicsWarnings;
	FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
	FPrimaryAssetId PreviousExperience;
	TWeakObjectPtr<UWorld> AuthorityWorld, DrivingWorld;
	FVector EntryWalkStart = FVector::ZeroVector;
	int32 SubjectId = INDEX_NONE;
	bool bConfigured = false;

	BEFORE_EACH()
	{
		using namespace RpgGaspMoverRagdollTests;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{
				TestRunner->AddError(TEXT("Ragdoll automation refuses to interrupt an existing PIE session."));
				return;
			}
		UClass* GameMode = LoadClass<ARpgGameModeBase>(nullptr, GameModePath);
		ASSERT_THAT(IsNotNull(GameMode));
		if (!GameMode) return;
		PhysicsWarnings.Start(); Isolation.Start();
		PreviousExperience = GetDefault<URpgDeveloperSettings>()->ExperienceOverride;
		GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = ExperienceId(); bConfigured = true;
		FNetworkComponentBuilder<FState>().WithClients(1).AsListenServer()
			.WithGameInstanceClass(FSoftClassPath(TEXT("/Game/SurvivalRpg/Core/Game/BP_Rpg_GameInstance.BP_Rpg_GameInstance_C")))
			.WithGameMode(GameMode).Build(Network);
	}
	AFTER_EACH()
	{
		Input.Stop(); Observations.Stop(); PhysicsWarnings.Stop();
		for (const FString& Message : PhysicsWarnings.Get()) TestRunner->AddError(TEXT("Unexpected Ragdoll physics/movement warning: ") + Message);
		Observations.Report();
		if (bConfigured) GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = PreviousExperience;
	}
	TEST_METHOD(RemotePhysicsGetupAndLateJoinRestoreMovementAndCombat) { Queue(EScenario::Recover, false); }
	TEST_METHOD(ListenHostPhysicsGetupReachesBothObservers) { Queue(EScenario::Recover, true); }
	TEST_METHOD(CancelledGetupCannotCompleteTheNextRagdollEpisode) { Queue(EScenario::Reenter, false); }
	TEST_METHOD(DeathDuringRagdollReleasesPhysicsAndRespawns) { Queue(EScenario::DeathInRagdoll, false); }
	TEST_METHOD(DeathDuringGetupCannotReviveTheOldPawn) { Queue(EScenario::DeathInGetup, false); }

	bool AllRagdoll(int32 Count, int32 Entries = 1) const
	{
		using namespace RpgGaspMoverRagdollTests;
		return Observations.All([this, Entries](UWorld* World, const FPeer& Peer)
		{
			APawn* Character = Pawn(World, SubjectId);
			const FPeer& Authority = Observations.Get(AuthorityWorld.Get());
			return Ready(World, Character) && Ragdoll(Character)->GetRagdollPhase() == ERpgMoverRagdollPhase::Ragdoll
				&& Peer.RagdollEntries >= Entries && Peer.ContinuousRagdollSeconds >= FMath::Max(0.25f, Ragdoll(Character)->MinimumRagdollDuration + 0.05f)
				&& Peer.bCurrentSupportedPhysicalPose && Peer.ContinuousSupportedPoseSeconds >= FMath::Max(0.35f, Ragdoll(Character)->MinimumRagdollDuration + 0.05f)
				&& Peer.EntryCommand.Episode > Peer.RetiredEpisode && Peer.EntryCommand.AbilityHandle.IsValid()
				&& Peer.EntryCommand.Episode == Authority.EntryCommand.Episode && Peer.EntryCommand.AbilityHandle == Authority.EntryCommand.AbilityHandle
				&& Peer.EntryCommand.ActivationPredictionKey == Authority.EntryCommand.ActivationPredictionKey
				&& Peer.EntryCommand.bServerInitiatedKey == Authority.EntryCommand.bServerInitiatedKey
				&& (!Peer.bHadNormalBaseline || Peer.bPoseMoved) && Peer.MaxAnchorDrift < 5.0f && !Peer.bInvalid;
		}, Count);
	}
	bool AllGetup() const
	{
		using namespace RpgGaspMoverRagdollTests;
		return Observations.All([this](UWorld* World, const FPeer& Peer)
		{
			APawn* Character = Pawn(World, SubjectId);
			const FPeer& Authority = Observations.Get(AuthorityWorld.Get());
			return Ready(World, Character) && Ragdoll(Character)->GetRagdollPhase() == ERpgMoverRagdollPhase::GettingUp
				&& Peer.bSawGetup && Peer.GetupLastTime >= Peer.GetupFirstTime + 0.03f && !Peer.bInvalid
				&& Peer.GetupCommand.Episode == Authority.GetupCommand.Episode && Peer.GetupCommand.Revision == Authority.GetupCommand.Revision
				&& Peer.GetupCommand.GetUpMontage == Authority.GetupCommand.GetUpMontage
				&& FMath::IsNearlyEqual(Peer.GetupCommand.GetUpStartTime, Authority.GetupCommand.GetUpStartTime);
		});
	}
	bool Restored(UWorld* World, const RpgGaspMoverRagdollTests::FPeer& Peer, bool bCancelled = false) const
	{
		using namespace RpgGaspMoverRagdollTests;
		APawn* Character = Pawn(World, SubjectId);
		if (!Ready(World, Character) || Ragdoll(Character)->GetRagdollPhase() != ERpgMoverRagdollPhase::Inactive
			|| ASC(Character)->GetCurrentMontage() || Mesh(Character)->GetAnimInstance()->GetCurrentActiveMontage()
			|| Mover(Character)->HasTraversalLease() || !Mover(Character)->IsOnGround() || Peer.bInvalid) return false;
		const FRpgMoverRagdollState Terminal = Ragdoll(Character)->GetRagdollState();
		if (Terminal.Episode != Peer.EntryCommand.Episode || Terminal.Revision <= Peer.GetupCommand.Revision
			|| Terminal.bCancelled != bCancelled || Terminal.GetUpMontage || Ragdoll(Character)->IsRagdollActive()) return false;
		// A late joiner has no pre-entry sample; compare the same authored profile's real authority baseline.
		const FPeer& Baseline = Peer.bHadNormalBaseline ? Peer : Observations.Get(AuthorityWorld.Get());
		if (!Baseline.bHadNormalBaseline || Baseline.InitialControlData.IsEmpty() || Baseline.InitialBodyState.IsEmpty()) return false;
		const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Character->GetRootComponent());
		if (Capsule->GetCollisionEnabled() != Baseline.CapsuleCollision || Capsule->GetCollisionResponseToChannel(ECC_Pawn) != Baseline.PawnResponse
			|| !Mesh(Character)->GetRelativeTransform().Equals(Baseline.MeshRelative, 0.1)) return false;
		for (const auto& Entry : Baseline.InitialControlData)
		{
			FPhysicsControlData Actual;
			if (!Controls(Character)->GetControlData(Entry.Key, Actual) || Actual.bEnabled != Entry.Value.bEnabled) return false;
			// Kinematic is a sparse source profile: disabling a control need not erase its inert tuning.
			if (Actual.bEnabled && (!FMath::IsNearlyEqual(Actual.LinearStrength, Entry.Value.LinearStrength)
				|| !FMath::IsNearlyEqual(Actual.AngularStrength, Entry.Value.AngularStrength)
				|| !FMath::IsNearlyEqual(Actual.AngularDampingRatio, Entry.Value.AngularDampingRatio))) return false;
		}
		// Modifier records are private to PhysicsControl; verify their actual body effects through public physics state.
		for (const auto& Entry : Baseline.InitialBodyState)
		{
			const FBodyInstance* Body = Mesh(Character)->GetBodyInstance(Entry.Key);
			if (!Body || Body->IsInstanceSimulatingPhysics() != Entry.Value.bSimulating
				|| !FMath::IsNearlyEqual(Body->PhysicsBlendWeight, Entry.Value.BlendWeight)
				|| Body->GetCollisionEnabled() != Entry.Value.Collision) return false;
		}
		return true;
	}
	void Queue(EScenario Scenario, bool bListenHost)
	{
		using namespace RpgGaspMoverRagdollTests;
		if (!bConfigured) return;
		Network.UntilClient(TEXT("Remote player composes the authored Ragdoll Experience and real physics controls"), 0,
			[](FState& State) { return Ready(State.World, LocalPawn(State.World)) && Mover(LocalPawn(State.World))->IsOnGround(); }, Timeout());
		if (bListenHost)
		{
			Network.UntilServer(TEXT("Listen host composes the same supported pawn"), [](FState& State)
				{ return Ready(State.World, LocalPawn(State.World)) && Mover(LocalPawn(State.World))->IsOnGround(); }, Timeout())
				.ThenServer(TEXT("Drive the actual listen-host controller"), [this](FState& State)
				{
					APawn* Character = LocalPawn(State.World); SubjectId = Character->GetPlayerState()->GetPlayerId(); DrivingWorld = State.World;
					ASSERT_THAT(IsTrue(Character->HasAuthority() && Character->IsLocallyControlled())); Input.Start(Character);
				});
		}
		else
		{
			Network.ThenClient(TEXT("Drive the actual autonomous owner through its input binding"), 0, [this](FState& State)
			{
				APawn* Character = LocalPawn(State.World); SubjectId = Character->GetPlayerState()->GetPlayerId(); DrivingWorld = State.World;
				ASSERT_THAT(IsTrue(Character->GetLocalRole() == ROLE_AutonomousProxy)); Input.Start(Character);
			});
		}
		Network.UntilServer(TEXT("Authority resolves the same healthy equipped subject"), [this](FState& State)
			{ return Ready(State.World, Pawn(State.World, SubjectId)); }, Timeout())
			.UntilClient(TEXT("Initial client resolves the same subject and stable normal presentation"), 0, [this](FState& State)
				{ return Ready(State.World, Pawn(State.World, SubjectId)); }, Timeout())
			.ThenServer(TEXT("Walk through real input so Mover establishes its normal floor height"), [this](FState& State)
			{
				// Walking can retain the fixture's elevated spawn while idle; only actual movement adjusts floor height.
				EntryWalkStart = Pawn(State.World, SubjectId)->GetActorLocation(); Input.Move(true);
			})
			.UntilServer(TEXT("Authority and driver both observe actual horizontal approach movement"), [this](FState& State)
			{
				const APawn* Authority = Pawn(State.World, SubjectId);
				const APawn* Driver = Pawn(DrivingWorld.Get(), SubjectId);
				return Authority && Driver && FVector::DistSquared2D(Authority->GetActorLocation(), EntryWalkStart) > FMath::Square(30.0)
					&& FVector::DistSquared2D(Driver->GetActorLocation(), EntryWalkStart) > FMath::Square(30.0);
			}, Timeout())
			.ThenServer(TEXT("Release movement and let ordinary braking stop the subject"), [this](FState&) { Input.Move(false); })
			.UntilServer(TEXT("Authority and driver settle on supported ground before normal baselines and the only entry press"), [this](FState& State)
				{ return EntryReady(State.World, Pawn(State.World, SubjectId)) && EntryReady(DrivingWorld.Get(), Pawn(DrivingWorld.Get(), SubjectId)); }, Timeout())
			.ThenServer(TEXT("Observe authority before the first actual R press"), [this](FState& State)
			{
				AuthorityWorld = State.World; Observations.Start(SubjectId); Observations.Add(State.World);
				ASSERT_THAT(IsNotNull(RagdollSpec(Pawn(State.World, SubjectId))));
			})
			.ThenClient(TEXT("Observe the initial owner or observer before entry"), 0, [this](FState& State) { Observations.Add(State.World); })
			.ThenServer(TEXT("Press R through the chosen local player's ordinary input"), [this](FState& State)
			{
				ReportEntry(TEXT("BeforeR"), State.World, Pawn(State.World, SubjectId));
				if (DrivingWorld != State.World) ReportEntry(TEXT("BeforeR"), DrivingWorld.Get(), Pawn(DrivingWorld.Get(), SubjectId));
				Input.Press(EKeys::R);
			})
			.UntilServer(TEXT("Both initial peers display genuinely simulated body poses with an anchored capsule"), [this](FState&)
				{ return AllRagdoll(2); }, Timeout())
			.ThenClientJoins()
			.UntilClient(TEXT("A real late joiner reconstructs the subject during the active Ragdoll episode"), 1, [this](FState& State)
			{
				APawn* Character = Pawn(State.World, SubjectId);
				return Ready(State.World, Character) && Character->GetLocalRole() == ROLE_SimulatedProxy
					&& Ragdoll(Character)->GetRagdollPhase() == ERpgMoverRagdollPhase::Ragdoll;
			}, Timeout())
			.ThenClient(TEXT("Retain the late observer's actual physics and lifecycle evidence"), 1, [this](FState& State) { Observations.Add(State.World); })
			.UntilServer(TEXT("Late observer has live bodies and a physical mesh pose too"), [this](FState&) { return AllRagdoll(3); }, Timeout());
		if (Scenario != EScenario::DeathInRagdoll)
		{
			Network.ThenServer(TEXT("A second real R press requests the selected source getup"), [this](FState&) { Input.Press(EKeys::R); })
				.UntilServer(TEXT("The actual getup montage advances on authority, owner and observer"), [this](FState&) { return AllGetup(); }, Timeout());
		}
		if (Scenario == EScenario::Reenter)
		{
			Network.ThenServer(TEXT("Cancel the active server-initiated ability through GAS during getup"), [this](FState& State)
			{
				APawn* Character = Pawn(State.World, SubjectId); FGameplayAbilitySpec* Spec = RagdollSpec(Character);
				ASSERT_THAT(IsTrue(Spec && Spec->IsActive()));
				if (Spec) ASC(Character)->CancelAbilityHandle(Spec->Handle);
			})
				.UntilServer(TEXT("Cancellation releases the original recovery on every peer"), [this](FState&)
					{ return Observations.All([this](UWorld* World, const FPeer& Peer) { return Restored(World, Peer, true); }); }, Timeout())
				.ThenServer(TEXT("Retire old montage instances and start a new episode through real input"), [this](FState&)
					{ Observations.RetireGetup(); Input.Press(EKeys::R); })
				.UntilServer(TEXT("New physical episode survives beyond old montage cleanup"), [this](FState&) { return AllRagdoll(3, 2); }, Timeout())
				.ThenServer(TEXT("Old getup instances cannot still be playing in the new episode"), [this](FState&)
				{
					ASSERT_THAT(IsTrue(Observations.All([this](UWorld* World, const FPeer& Peer)
					{
						const FAnimMontageInstance* Old = Mesh(Pawn(World, SubjectId))->GetAnimInstance()->GetMontageInstanceForID(Peer.RetiredGetupInstance);
						return Peer.RetiredGetupInstance != INDEX_NONE && (!Old || !Old->IsPlaying());
					})));
					Input.Press(EKeys::R);
				})
				.UntilServer(TEXT("The new episode selects and advances a distinct local getup instance"), [this](FState&) { return AllGetup(); }, Timeout())
				.ThenServer(TEXT("No peer reuses the retired getup instance"), [this](FState&)
					{ ASSERT_THAT(IsTrue(Observations.All([](UWorld*, const FPeer& Peer) { return Peer.GetupInstance != Peer.RetiredGetupInstance; }))); });
		}
		if (Scenario == EScenario::DeathInRagdoll || Scenario == EScenario::DeathInGetup) QueueDeath();
		else Network.UntilServer(TEXT("Natural getup returns normal profiles, collision, equipment and montage ownership"), [this](FState&)
			{ return Observations.All([this](UWorld* World, const FPeer& Peer) { return Restored(World, Peer); }); }, Timeout());
		QueueMovementAndCombat();
	}
	void QueueDeath()
	{
		using namespace RpgGaspMoverRagdollTests;
		Network.ThenServer(TEXT("Lethal damage uses the canonical authority health lifecycle"), [this](FState& State)
			{ Health(Pawn(State.World, SubjectId))->DamageSelfDestruct(false); })
			.UntilServer(TEXT("Normal death destroys the old authority pawn and waits for respawn"), [this](FState& State)
				{ const ARpgPlayerState* PlayerState = Player(State.World, SubjectId); return PlayerState && PlayerState->IsWaitingForRespawn() && !PlayerState->GetPawn(); }, Timeout())
			.UntilClients(TEXT("Clients receive final death rather than a living getup"), [this](FState& State)
				{ const ARpgPlayerState* PlayerState = Player(State.World, SubjectId); return PlayerState && PlayerState->IsWaitingForRespawn() && !PlayerState->GetPawn(); }, Timeout())
			.UntilServer(TEXT("Every old physics, mesh and equipment instance is released"), [this](FState&)
			{
				return Observations.All([](UWorld*, const FPeer& Peer)
				{
					if (Peer.OriginalPawn.IsValid() || Peer.OriginalMesh.IsValid() || Peer.OriginalControls.IsValid()) return false;
					for (const auto& Actor : Peer.EquipmentActors) if (Actor.IsValid()) return false;
					return true;
				});
			}, Timeout())
			.ThenServer(TEXT("Death never returns an old pawn to getup"), [this](FState&)
				{ ASSERT_THAT(IsTrue(Observations.All([](UWorld*, const FPeer& Peer) { return Peer.bSawDeath && !Peer.bGetupAfterDeath && !Peer.bInvalid; }))); })
			.UntilClient(TEXT("The ordinary server-authored respawn delay expires"), 0, [this](FState& State)
				{ const ARpgPlayerState* PlayerState = Player(State.World, SubjectId); return PlayerState && PlayerState->CanRespawnNow(); }, Timeout())
			.ThenClient(TEXT("The real owning controller requests respawn once"), 0, [this](FState& State)
			{
				Input.Stop(); ARpgPlayerController* PC = Cast<ARpgPlayerController>(State.World->GetFirstPlayerController());
				ASSERT_THAT(IsTrue(PC && PC->IsLocalController() && !PC->HasAuthority())); if (PC) PC->RequestRespawn();
			})
			.UntilServer(TEXT("All roles compose new ready physics objects on the same persistent PlayerState and ASC"), [this](FState&)
			{
				return Observations.All([this](UWorld* World, const FPeer& Peer)
				{
					APawn* Character = Pawn(World, SubjectId);
					return Ready(World, Character) && Player(World, SubjectId) == Peer.PlayerState.Get() && ASC(Character) == Peer.AbilitySystem.Get()
						&& !Player(World, SubjectId)->IsWaitingForRespawn() && Ragdoll(Character)->GetRagdollPhase() == ERpgMoverRagdollPhase::Inactive
						&& Mover(Character)->GetSyncState().MovementMode != URpgDeadMovementMode::ModeName && !ASC(Character)->GetCurrentMontage();
				});
			}, Timeout())
			.ThenClient(TEXT("Retain the newly possessed owner's real input route"), 0, [this](FState& State) { Input.Start(LocalPawn(State.World)); });
	}
	void QueueMovementAndCombat()
	{
		using namespace RpgGaspMoverRagdollTests;
		Network.ThenServer(TEXT("Exercise ordinary movement after recovery or respawn"), [this](FState&) { Observations.BeginMovement(); Input.Move(true); })
			.UntilServer(TEXT("All peers observe real movement of the restored living pawn"), [this](FState&)
			{
				return Observations.All([this](UWorld* World, const FPeer& Peer)
				{
					APawn* Character = Pawn(World, SubjectId);
					return Ready(World, Character) && Ragdoll(Character)->GetRagdollPhase() == ERpgMoverRagdollPhase::Inactive
						&& Mover(Character)->IsOnGround() && Mover(Character)->GetVelocity().Size2D() > 100.0
						&& FVector::Dist2D(Peer.MovementStart, Character->GetActorLocation()) > 150.0;
				});
			}, Timeout())
			.ThenServer(TEXT("Press the existing weapon input after control is returned"), [this](FState&) { Input.Press(EKeys::LeftMouseButton); })
			.UntilServer(TEXT("Equipment montage reaches authority, owner and observers"), [this](FState&)
				{ return Observations.All([](UWorld*, const FPeer& Peer) { return Peer.bSawAttack && !Peer.bInvalid && !Peer.bGetupAfterDeath; }); }, Timeout())
			.ThenServer(TEXT("Release ordinary movement and input"), [this](FState&) { Input.Stop(); });
	}
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
