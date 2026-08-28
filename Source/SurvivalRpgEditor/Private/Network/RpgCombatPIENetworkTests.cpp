#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Network/RpgCombatNetworkTestTypes.h"
#include "Network/RpgGaspNetworkTestTypes.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagContainer.h"

#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility_BasicWeaponAttack.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "SurvivalRpg/Combat/RpgCombatDeveloperSettings.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceManagerComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Development/RpgDeveloperSettings.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Equipment/RpgWeaponInstance.h"

#if ENABLE_PIE_NETWORK_TEST

namespace RpgCombatPIENetworkTests
{
	constexpr TCHAR PilotExperienceName[] = TEXT("RpgGaspPilotExperience");
	constexpr TCHAR PilotGameModeClassPath[] =
		TEXT("/Game/SurvivalRpg/Core/Game/BP_Rpg_GameMode.BP_Rpg_GameMode_C");
	constexpr TCHAR PilotCharacterClassPrefix[] =
		TEXT("/Game/SurvivalRpg/Core/Character/GASP/BP_Rpg_Character_GASP");
	constexpr TCHAR BasicSwordDefinitionClassPath[] =
		TEXT("/GF_Combat_Core/Equipment/Weapons/ED_BasicSword.ED_BasicSword_C");
	constexpr int32 NormalCompletedAttackCount = 20;
	constexpr int32 CompletedAttackCount = NormalCompletedAttackCount + 1;
	constexpr double CleanupStabilitySeconds = 0.2;
	constexpr float MinimumBladeCenterAdvance = 2.0f;
	constexpr float FastMontageTimingPlayRate = 1.5f;
	const FVector FixedBasicSwordContactOffset(140.0f, -25.0f, 0.0f);

	const FGameplayTag& PrimaryWeaponInputTag()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(
			TEXT("InputTag.Weapon.Primary"));
		return Tag;
	}

	const FGameplayTag& PrimaryWeaponAttackTag()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(
			TEXT("Weapon.Attack.Primary"));
		return Tag;
	}

	struct FNetworkState : public FBasePIENetworkComponentState
	{
		ARpgGaspNetworkFloorFixture* Floor = nullptr;
		ARpgCombatNetworkTargetFixture* Target = nullptr;
		int32 SubjectPlayerId = INDEX_NONE;
		TWeakObjectPtr<URpgWeaponInstance> AuthorityWeapon;
		FGameplayAbilitySpecHandle AttackAbilityHandle;

		uint32 WindowOpenBaseline = 0;
		uint32 WindowCloseBaseline = 0;
		uint32 TraceSampleBaseline = 0;
		uint32 DamageHitBaseline = 0;
		float TargetHealthBeforeAttack = 0.0f;
		FVector BladeCenterAtWindowOpen = FVector::ZeroVector;
		float MaximumBladeCenterAdvance = 0.0f;
		float ExpectedEffectivePlayRate = 0.0f;
		float OriginalAttackPlayRate = 0.0f;
		bool bAttackPlayRateOverridden = false;

		uint32 StableTraceSampleCount = 0;
		uint32 StableDamageHitCount = 0;
		int32 StableHealthDropCount = 0;
		float StableTargetHealth = 0.0f;
		double CleanupStableStartTime = -1.0;
		bool bObservedPostCleanupMutation = false;
	};

	FTimespan NetworkTimeout()
	{
		return FTimespan::FromSeconds(90.0);
	}

	FTimespan AttackWindowPoseTimeout()
	{
		return FTimespan::FromSeconds(0.5);
	}

	ARpgCharacter* FindLocalCharacter(UWorld* World)
	{
		const APlayerController* PlayerController = IsValid(World)
			? World->GetFirstPlayerController()
			: nullptr;
		return PlayerController ? Cast<ARpgCharacter>(PlayerController->GetPawn()) : nullptr;
	}

	ARpgCharacter* FindCharacterByPlayerId(UWorld* World, const int32 PlayerId)
	{
		if (!IsValid(World) || PlayerId == INDEX_NONE)
		{
			return nullptr;
		}

		for (TActorIterator<ARpgCharacter> It(World); It; ++It)
		{
			const APlayerState* PlayerState = It->GetPlayerState();
			if (PlayerState && PlayerState->GetPlayerId() == PlayerId)
			{
				return *It;
			}
		}
		return nullptr;
	}

	bool IsPilotExperienceReady(FNetworkState& State, const int32 ExpectedClients)
	{
		if (!IsValid(State.World))
		{
			return false;
		}

		const ENetMode NetMode = State.World->GetNetMode();
		const bool bExpectedWorld = State.World->GetNetDriver() &&
			((NetMode == NM_ListenServer &&
				State.World->GetNetDriver()->ClientConnections.Num() >= ExpectedClients) ||
			 NetMode == NM_Client);
		const AGameStateBase* GameState = State.World->GetGameState();
		const URpgExperienceManagerComponent* ExperienceManager = GameState
			? GameState->FindComponentByClass<URpgExperienceManagerComponent>()
			: nullptr;
		if (!bExpectedWorld || !ExperienceManager || !ExperienceManager->IsExperienceLoaded())
		{
			return false;
		}

		const URpgExperienceDefinition* Experience =
			ExperienceManager->GetCurrentExperienceChecked();
		const FPrimaryAssetId ExpectedExperienceId(
			URpgExperienceDefinition::StaticClass()->GetFName(),
			PilotExperienceName);
		return Experience && Experience->GetPrimaryAssetId() == ExpectedExperienceId;
	}

	bool IsPilotCharacterReady(const ARpgCharacter* Character)
	{
		const USkeletalMeshComponent* Mesh = IsValid(Character) ? Character->GetMesh() : nullptr;
		return IsValid(Character) &&
			Character->GetClass()->GetPathName().StartsWith(PilotCharacterClassPrefix) &&
			IsValid(Character->GetPlayerState()) &&
			IsValid(Character->GetRpgAbilitySystemComponent()) &&
			Mesh && IsValid(Cast<URpgAnimInstance>(Mesh->GetAnimInstance()));
	}

	FGameplayAbilitySpec* FindPrimaryAttackSpec(
		URpgAbilitySystemComponent* AbilitySystem,
		const URpgWeaponInstance* ExpectedWeapon = nullptr)
	{
		if (!AbilitySystem)
		{
			return nullptr;
		}

		for (FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
		{
			if (!Spec.Ability || Spec.PendingRemove ||
				!Spec.Ability->IsA<URpgGameplayAbility_BasicWeaponAttack>() ||
				!Spec.GetDynamicSpecSourceTags().HasTagExact(
					PrimaryWeaponInputTag()))
			{
				continue;
			}

			if (!ExpectedWeapon || Spec.SourceObject.Get() == ExpectedWeapon)
			{
				return &Spec;
			}
		}
		return nullptr;
	}

	int32 CountInputMatchingSpecs(
		const URpgAbilitySystemComponent* AbilitySystem,
		const FGameplayTag& InputTag)
	{
		int32 MatchingSpecCount = 0;
		if (!AbilitySystem)
		{
			return MatchingSpecCount;
		}

		for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
			{
				++MatchingSpecCount;
			}
		}
		return MatchingSpecCount;
	}

	URpgGameplayAbility_BasicWeaponAttack* GetAttackAbilityInstance(
		FGameplayAbilitySpec* AttackSpec)
	{
		return AttackSpec
			? Cast<URpgGameplayAbility_BasicWeaponAttack>(AttackSpec->GetPrimaryInstance())
			: nullptr;
	}

	bool ResolveAuthorityAttack(
		FNetworkState& State,
		ARpgCharacter*& OutCharacter,
		URpgAbilitySystemComponent*& OutAbilitySystem,
		FGameplayAbilitySpec*& OutAttackSpec,
		URpgGameplayAbility_BasicWeaponAttack*& OutAttackAbility)
	{
		OutCharacter = FindCharacterByPlayerId(State.World, State.SubjectPlayerId);
		OutAbilitySystem = OutCharacter ? OutCharacter->GetRpgAbilitySystemComponent() : nullptr;
		OutAttackSpec = OutAbilitySystem && State.AttackAbilityHandle.IsValid()
			? OutAbilitySystem->FindAbilitySpecFromHandle(State.AttackAbilityHandle)
			: FindPrimaryAttackSpec(OutAbilitySystem, State.AuthorityWeapon.Get());
		OutAttackAbility = GetAttackAbilityInstance(OutAttackSpec);
		return OutCharacter && OutAbilitySystem && OutAttackSpec && OutAttackAbility &&
			OutCharacter->HasAuthority() &&
			OutCharacter->GetRemoteRole() == ROLE_AutonomousProxy;
	}

	bool IsAuthorityAttackReady(FNetworkState& State)
	{
		ARpgCharacter* Character = FindCharacterByPlayerId(State.World, State.SubjectPlayerId);
		URpgAbilitySystemComponent* AbilitySystem = Character
			? Character->GetRpgAbilitySystemComponent()
			: nullptr;
		URpgEquipmentManagerComponent* EquipmentManager = Character
			? Character->FindComponentByClass<URpgEquipmentManagerComponent>()
			: nullptr;
		URpgWeaponInstance* Weapon = State.AuthorityWeapon.Get();
		FGameplayAbilitySpec* AttackSpec = FindPrimaryAttackSpec(AbilitySystem, Weapon);
		URpgGameplayAbility_BasicWeaponAttack* AttackAbility =
			GetAttackAbilityInstance(AttackSpec);

		if (!Character || !AbilitySystem || !EquipmentManager || !Weapon || !AttackSpec ||
			!AttackAbility || !State.Target ||
			EquipmentManager->GetEquipmentInstanceInSlot(ERpgEquipmentSlot::MainHand) != Weapon ||
			Weapon->GetSpawnedActors().IsEmpty() ||
			!Weapon->FindAttackDefinition(PrimaryWeaponAttackTag()))
		{
			return false;
		}

		State.AttackAbilityHandle = AttackSpec->Handle;
		return Character->HasAuthority() &&
			Character->GetRemoteRole() == ROLE_AutonomousProxy &&
			Character->GetCharacterMovement()->IsMovingOnGround() &&
			!AttackSpec->IsActive() && !AbilitySystem->GetCurrentMontage() &&
			!AttackAbility->HasResidualAttackRuntimeStateForTests();
	}

	bool IsClientAttackReady(FNetworkState& State)
	{
		ARpgCharacter* Character = FindLocalCharacter(State.World);
		URpgAbilitySystemComponent* AbilitySystem = Character
			? Character->GetRpgAbilitySystemComponent()
			: nullptr;
		URpgEquipmentManagerComponent* EquipmentManager = Character
			? Character->FindComponentByClass<URpgEquipmentManagerComponent>()
			: nullptr;
		URpgWeaponInstance* Weapon = EquipmentManager
			? Cast<URpgWeaponInstance>(EquipmentManager->GetEquipmentInstanceInSlot(
				ERpgEquipmentSlot::MainHand))
			: nullptr;
		FGameplayAbilitySpec* AttackSpec = AbilitySystem && State.AttackAbilityHandle.IsValid()
			? AbilitySystem->FindAbilitySpecFromHandle(State.AttackAbilityHandle)
			: FindPrimaryAttackSpec(AbilitySystem, Weapon);
		URpgGameplayAbility_BasicWeaponAttack* AttackAbility =
			GetAttackAbilityInstance(AttackSpec);

		return IsPilotCharacterReady(Character) &&
			Character->GetLocalRole() == ROLE_AutonomousProxy &&
			Character->GetCharacterMovement()->IsMovingOnGround() &&
			CountInputMatchingSpecs(AbilitySystem, PrimaryWeaponInputTag()) == 1 &&
			Weapon && !Weapon->GetSpawnedActors().IsEmpty() &&
			AttackSpec && AttackSpec->SourceObject.Get() == Weapon &&
			AttackAbility && !AttackSpec->IsActive() &&
			!AbilitySystem->GetCurrentMontage() &&
			!AttackAbility->HasResidualAttackRuntimeStateForTests();
	}

	bool TryResolveSocketLocation(
		const ARpgCharacter* Character,
		const URpgWeaponInstance* Weapon,
		const FName SocketName,
		FVector& OutLocation)
	{
		if (!Weapon || SocketName.IsNone())
		{
			return false;
		}

		for (AActor* SpawnedActor : Weapon->GetSpawnedActors())
		{
			if (!IsValid(SpawnedActor))
			{
				continue;
			}

			TInlineComponentArray<USceneComponent*> SceneComponents;
			SpawnedActor->GetComponents(SceneComponents);
			for (const USceneComponent* SceneComponent : SceneComponents)
			{
				if (SceneComponent && SceneComponent->GetFName() == SocketName)
				{
					OutLocation = SceneComponent->GetComponentLocation();
					return true;
				}
				if (SceneComponent && SceneComponent->DoesSocketExist(SocketName))
				{
					OutLocation = SceneComponent->GetSocketLocation(SocketName);
					return true;
				}
			}
		}

		const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
		if (Mesh && Mesh->DoesSocketExist(SocketName))
		{
			OutLocation = Mesh->GetSocketLocation(SocketName);
			return true;
		}
		return false;
	}

	bool TryGetCurrentBladeCenter(
		const ARpgCharacter* Character,
		const URpgWeaponInstance* Weapon,
		FVector& OutBladeCenter)
	{
		const FRpgWeaponAttackDefinition* AttackDefinition = Weapon
			? Weapon->FindAttackDefinition(PrimaryWeaponAttackTag())
			: nullptr;
		if (!AttackDefinition || AttackDefinition->TracePointSockets.Num() < 2)
		{
			return false;
		}

		FVector BladeBase = FVector::ZeroVector;
		FVector BladeTip = FVector::ZeroVector;
		if (!TryResolveSocketLocation(
				Character,
				Weapon,
				AttackDefinition->TracePointSockets[0],
				BladeBase) ||
			!TryResolveSocketLocation(
				Character,
				Weapon,
				AttackDefinition->TracePointSockets.Last(),
				BladeTip))
		{
			return false;
		}

		OutBladeCenter = (BladeBase + BladeTip) * 0.5;
		return !OutBladeCenter.ContainsNaN();
	}

	void MoveTargetAwayFromWeapon(FNetworkState& State, const ARpgCharacter* Character)
	{
		if (State.Target && Character)
		{
			State.Target->SetActorLocation(
				Character->GetActorLocation() + FVector(0.0, 0.0, 10000.0),
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
		}
	}

	void PlaceTargetAtFixedBasicSwordContact(
		FNetworkState& State,
		const ARpgCharacter* Character)
	{
		if (State.Target && Character)
		{
			// Content-specific integration fixture for the current BasicSword primary montage,
			// Quinn retarget, mesh offset, and blade sockets. Recalibrate when that slice changes.
			State.Target->SetActorLocation(
				Character->GetActorTransform().TransformPosition(
					FixedBasicSwordContactOffset),
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
		}
	}

	bool HasCleanAuthorityAttack(FNetworkState& State)
	{
		ARpgCharacter* Character = nullptr;
		URpgAbilitySystemComponent* AbilitySystem = nullptr;
		FGameplayAbilitySpec* AttackSpec = nullptr;
		URpgGameplayAbility_BasicWeaponAttack* AttackAbility = nullptr;
		return ResolveAuthorityAttack(
				State,
				Character,
				AbilitySystem,
				AttackSpec,
				AttackAbility) &&
			!AttackSpec->IsActive() && !AbilitySystem->GetCurrentMontage() &&
			!AttackAbility->IsAttackWindowOpenForTests() &&
			!AttackAbility->HasPendingAttackTimersForTests() &&
			!AttackAbility->HasResidualAttackRuntimeStateForTests();
	}

	bool HasStableAuthorityCleanup(FNetworkState& State)
	{
		ARpgCharacter* Character = nullptr;
		URpgAbilitySystemComponent* AbilitySystem = nullptr;
		FGameplayAbilitySpec* AttackSpec = nullptr;
		URpgGameplayAbility_BasicWeaponAttack* AttackAbility = nullptr;
		if (!ResolveAuthorityAttack(
				State,
				Character,
				AbilitySystem,
				AttackSpec,
				AttackAbility) ||
			!State.Target || !State.Target->GetHealthSet())
		{
			return false;
		}

		const uint32 TraceSamples = AttackAbility->GetAuthorityTraceSampleCountForTests();
		const uint32 DamageHits = AttackAbility->GetAuthorityDamageHitCountForTests();
		const int32 HealthDrops = State.Target->GetHealthDropCount();
		const float Health = State.Target->GetHealthSet()->GetHealth();
		const bool bClean = HasCleanAuthorityAttack(State);
		if (!bClean || TraceSamples != State.StableTraceSampleCount ||
			DamageHits != State.StableDamageHitCount ||
			HealthDrops != State.StableHealthDropCount ||
			!FMath::IsNearlyEqual(Health, State.StableTargetHealth))
		{
			State.bObservedPostCleanupMutation = true;
			State.StableTraceSampleCount = TraceSamples;
			State.StableDamageHitCount = DamageHits;
			State.StableHealthDropCount = HealthDrops;
			State.StableTargetHealth = Health;
			State.CleanupStableStartTime = State.World->GetTimeSeconds();
			return false;
		}

		return State.CleanupStableStartTime >= 0.0 &&
			State.World->GetTimeSeconds() - State.CleanupStableStartTime >=
				CleanupStabilitySeconds;
	}

	void BeginCleanupObservation(
		FNetworkState& State,
		const URpgGameplayAbility_BasicWeaponAttack& AttackAbility)
	{
		State.StableTraceSampleCount = AttackAbility.GetAuthorityTraceSampleCountForTests();
		State.StableDamageHitCount = AttackAbility.GetAuthorityDamageHitCountForTests();
		State.StableHealthDropCount = State.Target ? State.Target->GetHealthDropCount() : INDEX_NONE;
		State.StableTargetHealth = State.Target && State.Target->GetHealthSet()
			? State.Target->GetHealthSet()->GetHealth()
			: -1.0f;
		State.CleanupStableStartTime = IsValid(State.World)
			? State.World->GetTimeSeconds()
			: -1.0;
		State.bObservedPostCleanupMutation = false;
	}
}

NETWORK_TEST_CLASS(CombatRemoteMeleePIE, "SurvivalRpg.Network")
{
	using FNetworkState = RpgCombatPIENetworkTests::FNetworkState;

	FPIENetworkComponent<FNetworkState> Network{
		TestRunner,
		TestCommandBuilder,
		bInitializing};
	FPacketSimulationSettings PacketSettings;
	FPrimaryAssetId OriginalExperienceOverride;
	int32 SubjectPlayerId = INDEX_NONE;
	FGameplayAbilitySpecHandle AuthorityAttackAbilityHandle;
	bool bOriginalDiskPersistence = true;
	bool bOriginalAttackLifecycleLogging = false;
	UClass* PilotGameModeClass = nullptr;
	TArray<TSharedPtr<FString>> StepDescriptions;

	const TCHAR* MakeStepDescription(const int32 AttackIndex, const TCHAR* Phase)
	{
		TSharedPtr<FString> Description = MakeShared<FString>(FString::Printf(
			TEXT("Attack %02d/%02d: %s"),
			AttackIndex + 1,
			RpgCombatPIENetworkTests::CompletedAttackCount,
			Phase));
		const TCHAR* Result = **Description;
		StepDescriptions.Add(MoveTemp(Description));
		return Result;
	}

	void QueueCompletedAttack(
		const int32 AttackIndex,
		const float MontagePlayRateOverride = 0.0f)
	{
		using namespace RpgCombatPIENetworkTests;

		Network
			.ThenServer(
				MakeStepDescription(AttackIndex, TEXT("prepare authority baselines")),
				[this, MontagePlayRateOverride](FNetworkState& State)
				{
					ARpgCharacter* Character = nullptr;
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					FGameplayAbilitySpec* AttackSpec = nullptr;
					URpgGameplayAbility_BasicWeaponAttack* AttackAbility = nullptr;
					ASSERT_THAT(IsTrue(ResolveAuthorityAttack(
						State,
						Character,
						AbilitySystem,
						AttackSpec,
						AttackAbility)));
					ASSERT_THAT(IsNotNull(State.Target));
					ASSERT_THAT(IsTrue(HasCleanAuthorityAttack(State)));
					if (!Character || !AttackAbility || !State.Target ||
						!State.Target->GetHealthSet())
					{
						return;
					}

					State.WindowOpenBaseline =
						AttackAbility->GetAuthorityWindowOpenCountForTests();
					State.WindowCloseBaseline =
						AttackAbility->GetAuthorityWindowCloseCountForTests();
					State.TraceSampleBaseline =
						AttackAbility->GetAuthorityTraceSampleCountForTests();
					State.DamageHitBaseline =
						AttackAbility->GetAuthorityDamageHitCountForTests();
					State.TargetHealthBeforeAttack = State.Target->GetHealthSet()->GetHealth();
					State.BladeCenterAtWindowOpen = FVector::ZeroVector;
					State.MaximumBladeCenterAdvance = 0.0f;
					State.ExpectedEffectivePlayRate = 0.0f;
					State.OriginalAttackPlayRate = 0.0f;
					State.bAttackPlayRateOverridden = false;
					if (MontagePlayRateOverride > UE_SMALL_NUMBER)
					{
						ASSERT_THAT(IsTrue(State.AuthorityWeapon->SetAttackMontagePlayRateForTests(
							PrimaryWeaponAttackTag(),
							MontagePlayRateOverride,
							State.OriginalAttackPlayRate)));
						State.bAttackPlayRateOverridden = true;
					}
					const FRpgWeaponAttackDefinition* AttackDefinition =
						State.AuthorityWeapon->FindAttackDefinition(PrimaryWeaponAttackTag());
					ASSERT_THAT(IsNotNull(AttackDefinition));
					if (AttackDefinition && AttackDefinition->Montage)
					{
						float TaskPlayRate = AttackDefinition->MontagePlayRate;
						UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Rate(
							TaskPlayRate);
						State.ExpectedEffectivePlayRate =
							TaskPlayRate * AttackDefinition->Montage->RateScale;
					}
					State.Target->ResetDamageObservations();
					PlaceTargetAtFixedBasicSwordContact(State, Character);
				})
			.ThenClient(
				MakeStepDescription(AttackIndex, TEXT("owner submits primary input through the ASC")),
				0,
				[this, MontagePlayRateOverride](FNetworkState& State)
				{
					ARpgCharacter* Character = FindLocalCharacter(State.World);
					URpgAbilitySystemComponent* AbilitySystem = Character
						? Character->GetRpgAbilitySystemComponent()
						: nullptr;
					URpgEquipmentManagerComponent* EquipmentManager = Character
						? Character->FindComponentByClass<URpgEquipmentManagerComponent>()
						: nullptr;
					URpgWeaponInstance* Weapon = EquipmentManager
						? Cast<URpgWeaponInstance>(EquipmentManager->GetEquipmentInstanceInSlot(
							ERpgEquipmentSlot::MainHand))
						: nullptr;
					FGameplayAbilitySpec* AttackSpec =
						AbilitySystem && State.AttackAbilityHandle.IsValid()
							? AbilitySystem->FindAbilitySpecFromHandle(State.AttackAbilityHandle)
							: nullptr;
					ASSERT_THAT(IsNotNull(Character));
					ASSERT_THAT(IsNotNull(AbilitySystem));
					ASSERT_THAT(IsNotNull(Weapon));
					ASSERT_THAT(IsNotNull(AttackSpec));
					ASSERT_THAT(IsTrue(State.AttackAbilityHandle.IsValid() &&
						CountInputMatchingSpecs(AbilitySystem, PrimaryWeaponInputTag()) == 1 &&
						Weapon && AttackSpec && AttackSpec->Handle == State.AttackAbilityHandle &&
						AttackSpec->SourceObject.Get() == Weapon && !AttackSpec->IsActive()));
					if (!AbilitySystem || !Weapon || !AttackSpec)
					{
						return;
					}
					if (MontagePlayRateOverride > UE_SMALL_NUMBER)
					{
						ASSERT_THAT(IsTrue(Weapon->SetAttackMontagePlayRateForTests(
							PrimaryWeaponAttackTag(),
							MontagePlayRateOverride,
							State.OriginalAttackPlayRate)));
						State.bAttackPlayRateOverridden = true;
					}

					AbilitySystem->AbilityInputTagPressed(
						PrimaryWeaponInputTag());
					AbilitySystem->ProcessAbilityInput(1.0f / 60.0f, false);
					AbilitySystem->AbilityInputTagReleased(
						PrimaryWeaponInputTag());
					AbilitySystem->ProcessAbilityInput(1.0f / 60.0f, false);
				})
			.UntilServer(
				MakeStepDescription(AttackIndex, TEXT("authority opens exactly one attack window")),
				[](FNetworkState& State)
				{
					ARpgCharacter* Character = nullptr;
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					FGameplayAbilitySpec* AttackSpec = nullptr;
					URpgGameplayAbility_BasicWeaponAttack* AttackAbility = nullptr;
					const bool bWindowOpened = ResolveAuthorityAttack(
							State,
							Character,
							AbilitySystem,
							AttackSpec,
							AttackAbility) &&
						AttackSpec->IsActive() && AbilitySystem->GetCurrentMontage() &&
						AttackAbility->IsAttackWindowOpenForTests() &&
						AttackAbility->GetAuthorityWindowOpenCountForTests() >=
							State.WindowOpenBaseline + 1;
					const UAnimInstance* AnimInstance = Character
						? Character->GetMesh()->GetAnimInstance()
						: nullptr;
					const UAnimMontage* CurrentMontage = AbilitySystem
						? AbilitySystem->GetCurrentMontage()
						: nullptr;
					const bool bExpectedRate = AnimInstance && CurrentMontage &&
						FMath::IsNearlyEqual(
							AnimInstance->Montage_GetEffectivePlayRate(CurrentMontage),
							State.ExpectedEffectivePlayRate,
							KINDA_SMALL_NUMBER);
					return bWindowOpened && bExpectedRate && TryGetCurrentBladeCenter(
						Character,
						State.AuthorityWeapon.Get(),
						State.BladeCenterAtWindowOpen);
				},
				NetworkTimeout())
			.UntilServer(
				MakeStepDescription(AttackIndex, TEXT("authority blade pose advances during the window")),
				[](FNetworkState& State)
				{
					ARpgCharacter* Character = nullptr;
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					FGameplayAbilitySpec* AttackSpec = nullptr;
					URpgGameplayAbility_BasicWeaponAttack* AttackAbility = nullptr;
					FVector CurrentBladeCenter = FVector::ZeroVector;
					if (!ResolveAuthorityAttack(
							State,
							Character,
							AbilitySystem,
							AttackSpec,
							AttackAbility) ||
						!AttackAbility->IsAttackWindowOpenForTests() ||
						!TryGetCurrentBladeCenter(
							Character,
							State.AuthorityWeapon.Get(),
							CurrentBladeCenter))
					{
						return false;
					}

					State.MaximumBladeCenterAdvance = FMath::Max(
						State.MaximumBladeCenterAdvance,
						FVector::Distance(State.BladeCenterAtWindowOpen, CurrentBladeCenter));
					return State.MaximumBladeCenterAdvance >= MinimumBladeCenterAdvance;
				},
				AttackWindowPoseTimeout())
			.UntilServer(
				MakeStepDescription(AttackIndex, TEXT("authority applies damage and fully cleans up")),
				[](FNetworkState& State)
				{
					ARpgCharacter* Character = nullptr;
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					FGameplayAbilitySpec* AttackSpec = nullptr;
					URpgGameplayAbility_BasicWeaponAttack* AttackAbility = nullptr;
					return ResolveAuthorityAttack(
							State,
							Character,
							AbilitySystem,
							AttackSpec,
							AttackAbility) &&
						State.Target && State.Target->GetHealthSet() &&
						AttackAbility->GetAuthorityWindowCloseCountForTests() >=
							State.WindowCloseBaseline + 1 &&
						AttackAbility->GetAuthorityTraceSampleCountForTests() >
							State.TraceSampleBaseline &&
						AttackAbility->GetAuthorityDamageHitCountForTests() >=
							State.DamageHitBaseline + 1 &&
						State.Target->GetHealthDropCount() >= 1 &&
						State.Target->GetHealthSet()->GetHealth() <
							State.TargetHealthBeforeAttack &&
						HasCleanAuthorityAttack(State);
				},
				NetworkTimeout())
			.ThenServer(
				MakeStepDescription(AttackIndex, TEXT("verify exact authority deltas")),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = nullptr;
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					FGameplayAbilitySpec* AttackSpec = nullptr;
					URpgGameplayAbility_BasicWeaponAttack* AttackAbility = nullptr;
					ASSERT_THAT(IsTrue(ResolveAuthorityAttack(
						State,
						Character,
						AbilitySystem,
						AttackSpec,
						AttackAbility)));
					ASSERT_THAT(IsNotNull(State.Target));
					if (!Character || !AttackAbility || !State.Target ||
						!State.Target->GetHealthSet())
					{
						return;
					}

					ASSERT_THAT(IsTrue(
						AttackAbility->GetAuthorityWindowOpenCountForTests() ==
							State.WindowOpenBaseline + 1));
					ASSERT_THAT(IsTrue(
						AttackAbility->GetAuthorityWindowCloseCountForTests() ==
							State.WindowCloseBaseline + 1));
					ASSERT_THAT(IsTrue(
						AttackAbility->GetAuthorityTraceSampleCountForTests() >
							State.TraceSampleBaseline));
					ASSERT_THAT(IsTrue(
						State.MaximumBladeCenterAdvance >= MinimumBladeCenterAdvance));
					ASSERT_THAT(IsTrue(
						AttackAbility->GetAuthorityDamageHitCountForTests() ==
							State.DamageHitBaseline + 1));
					ASSERT_THAT(IsTrue(State.Target->GetHealthDropCount() == 1));
					ASSERT_THAT(IsTrue(State.Target->GetLastDamageSource() ==
						State.AuthorityWeapon.Get()));
					ASSERT_THAT(IsTrue(State.Target->GetLastDamageInstigator() ==
						Character->GetPlayerState()));
					ASSERT_THAT(IsTrue(State.Target->GetLastDamageCauser() == Character));
					ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(
						State.Target->GetLastHealthBeforeDamage(),
						State.TargetHealthBeforeAttack)));
					ASSERT_THAT(IsTrue(
						State.Target->GetLastHealthAfterDamage() <
							State.Target->GetLastHealthBeforeDamage()));
					ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(
						State.Target->GetHealthSet()->GetHealth(),
						State.Target->GetLastHealthAfterDamage())));
					ASSERT_THAT(IsTrue(HasCleanAuthorityAttack(State)));
					BeginCleanupObservation(State, *AttackAbility);
				})
			.UntilClient(
				MakeStepDescription(AttackIndex, TEXT("owner ability and montage settle")),
				0,
				[](FNetworkState& State)
				{
					return IsClientAttackReady(State);
				},
				NetworkTimeout())
			.UntilServer(
				MakeStepDescription(AttackIndex, TEXT("trace and health stay stable for 0.2 seconds")),
				[](FNetworkState& State)
				{
					return HasStableAuthorityCleanup(State);
				},
				NetworkTimeout())
			.ThenServer(
				MakeStepDescription(AttackIndex, TEXT("confirm no post-cleanup mutation")),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsFalse(State.bObservedPostCleanupMutation));
				});

		if (MontagePlayRateOverride > UE_SMALL_NUMBER)
		{
			Network
				.ThenServer(
					MakeStepDescription(AttackIndex, TEXT("restore authority attack play rate")),
					[this](FNetworkState& State)
					{
						float ReplacedPlayRate = 0.0f;
						ASSERT_THAT(IsTrue(State.bAttackPlayRateOverridden &&
							State.AuthorityWeapon.IsValid() &&
							State.AuthorityWeapon->SetAttackMontagePlayRateForTests(
								PrimaryWeaponAttackTag(),
								State.OriginalAttackPlayRate,
								ReplacedPlayRate)));
						ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(
							ReplacedPlayRate,
							FastMontageTimingPlayRate)));
						State.bAttackPlayRateOverridden = false;
					})
				.ThenClient(
					MakeStepDescription(AttackIndex, TEXT("restore owner attack play rate")),
					0,
					[this](FNetworkState& State)
					{
						ARpgCharacter* Character = FindLocalCharacter(State.World);
						URpgEquipmentManagerComponent* EquipmentManager = Character
							? Character->FindComponentByClass<URpgEquipmentManagerComponent>()
							: nullptr;
						URpgWeaponInstance* Weapon = EquipmentManager
							? Cast<URpgWeaponInstance>(EquipmentManager->GetEquipmentInstanceInSlot(
								ERpgEquipmentSlot::MainHand))
							: nullptr;
						float ReplacedPlayRate = 0.0f;
						ASSERT_THAT(IsTrue(State.bAttackPlayRateOverridden && Weapon &&
							Weapon->SetAttackMontagePlayRateForTests(
								PrimaryWeaponAttackTag(),
								State.OriginalAttackPlayRate,
								ReplacedPlayRate)));
						ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(
							ReplacedPlayRate,
							FastMontageTimingPlayRate)));
						State.bAttackPlayRateOverridden = false;
					});
		}
	}

	void QueueCancelledAttack()
	{
		using namespace RpgCombatPIENetworkTests;

		Network
			.ThenServer(
				TEXT("Cancelled attack: prepare authority baselines and keep target away"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = nullptr;
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					FGameplayAbilitySpec* AttackSpec = nullptr;
					URpgGameplayAbility_BasicWeaponAttack* AttackAbility = nullptr;
					ASSERT_THAT(IsTrue(ResolveAuthorityAttack(
						State,
						Character,
						AbilitySystem,
						AttackSpec,
						AttackAbility)));
					ASSERT_THAT(IsTrue(HasCleanAuthorityAttack(State)));
					if (!Character || !AttackAbility || !State.Target ||
						!State.Target->GetHealthSet())
					{
						return;
					}

					State.WindowOpenBaseline =
						AttackAbility->GetAuthorityWindowOpenCountForTests();
					State.WindowCloseBaseline =
						AttackAbility->GetAuthorityWindowCloseCountForTests();
					State.TraceSampleBaseline =
						AttackAbility->GetAuthorityTraceSampleCountForTests();
					State.DamageHitBaseline =
						AttackAbility->GetAuthorityDamageHitCountForTests();
					State.TargetHealthBeforeAttack = State.Target->GetHealthSet()->GetHealth();
					State.Target->ResetDamageObservations();
					MoveTargetAwayFromWeapon(State, Character);
				})
			.ThenClient(
				TEXT("Cancelled attack: owner submits primary input through the ASC"),
				0,
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindLocalCharacter(State.World);
					URpgAbilitySystemComponent* AbilitySystem = Character
						? Character->GetRpgAbilitySystemComponent()
						: nullptr;
					URpgEquipmentManagerComponent* EquipmentManager = Character
						? Character->FindComponentByClass<URpgEquipmentManagerComponent>()
						: nullptr;
					URpgWeaponInstance* Weapon = EquipmentManager
						? Cast<URpgWeaponInstance>(EquipmentManager->GetEquipmentInstanceInSlot(
							ERpgEquipmentSlot::MainHand))
						: nullptr;
					FGameplayAbilitySpec* AttackSpec =
						AbilitySystem && State.AttackAbilityHandle.IsValid()
							? AbilitySystem->FindAbilitySpecFromHandle(State.AttackAbilityHandle)
							: nullptr;
					ASSERT_THAT(IsNotNull(AbilitySystem));
					ASSERT_THAT(IsNotNull(Weapon));
					ASSERT_THAT(IsNotNull(AttackSpec));
					ASSERT_THAT(IsTrue(State.AttackAbilityHandle.IsValid() &&
						CountInputMatchingSpecs(AbilitySystem, PrimaryWeaponInputTag()) == 1 &&
						Weapon && AttackSpec && AttackSpec->Handle == State.AttackAbilityHandle &&
						AttackSpec->SourceObject.Get() == Weapon && !AttackSpec->IsActive()));
					if (!AbilitySystem || !Weapon || !AttackSpec)
					{
						return;
					}

					AbilitySystem->AbilityInputTagPressed(
						PrimaryWeaponInputTag());
					AbilitySystem->ProcessAbilityInput(1.0f / 60.0f, false);
					AbilitySystem->AbilityInputTagReleased(
						PrimaryWeaponInputTag());
					AbilitySystem->ProcessAbilityInput(1.0f / 60.0f, false);
				})
			.UntilServer(
				TEXT("Cancelled attack: authority window is open"),
				[](FNetworkState& State)
				{
					ARpgCharacter* Character = nullptr;
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					FGameplayAbilitySpec* AttackSpec = nullptr;
					URpgGameplayAbility_BasicWeaponAttack* AttackAbility = nullptr;
					return ResolveAuthorityAttack(
							State,
							Character,
							AbilitySystem,
							AttackSpec,
							AttackAbility) &&
						AttackSpec->IsActive() && AbilitySystem->GetCurrentMontage() &&
						AttackAbility->IsAttackWindowOpenForTests() &&
						AttackAbility->GetAuthorityWindowOpenCountForTests() >=
							State.WindowOpenBaseline + 1;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Cancelled attack: authority cancels the exact granted handle"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = nullptr;
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					FGameplayAbilitySpec* AttackSpec = nullptr;
					URpgGameplayAbility_BasicWeaponAttack* AttackAbility = nullptr;
					ASSERT_THAT(IsTrue(ResolveAuthorityAttack(
						State,
						Character,
						AbilitySystem,
						AttackSpec,
						AttackAbility)));
					ASSERT_THAT(IsTrue(AttackSpec && AttackSpec->IsActive()));
					ASSERT_THAT(IsTrue(AttackAbility &&
						AttackAbility->IsAttackWindowOpenForTests()));
					ASSERT_THAT(IsTrue(AttackAbility &&
						AttackAbility->GetAuthorityWindowOpenCountForTests() ==
							State.WindowOpenBaseline + 1));
					if (AbilitySystem && State.AttackAbilityHandle.IsValid())
					{
						AbilitySystem->CancelAbilityHandle(State.AttackAbilityHandle);
					}
				})
			.UntilServer(
				TEXT("Cancelled attack: authority closes and fully cleans up"),
				[](FNetworkState& State)
				{
					ARpgCharacter* Character = nullptr;
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					FGameplayAbilitySpec* AttackSpec = nullptr;
					URpgGameplayAbility_BasicWeaponAttack* AttackAbility = nullptr;
					return ResolveAuthorityAttack(
							State,
							Character,
							AbilitySystem,
							AttackSpec,
							AttackAbility) &&
						AttackAbility->GetAuthorityWindowCloseCountForTests() >=
							State.WindowCloseBaseline + 1 &&
						HasCleanAuthorityAttack(State);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Cancelled attack: verify close, no damage, and no timers"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = nullptr;
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					FGameplayAbilitySpec* AttackSpec = nullptr;
					URpgGameplayAbility_BasicWeaponAttack* AttackAbility = nullptr;
					ASSERT_THAT(IsTrue(ResolveAuthorityAttack(
						State,
						Character,
						AbilitySystem,
						AttackSpec,
						AttackAbility)));
					if (!AttackAbility || !State.Target || !State.Target->GetHealthSet())
					{
						return;
					}

					ASSERT_THAT(IsTrue(
						AttackAbility->GetAuthorityWindowOpenCountForTests() ==
							State.WindowOpenBaseline + 1));
					ASSERT_THAT(IsTrue(
						AttackAbility->GetAuthorityWindowCloseCountForTests() ==
							State.WindowCloseBaseline + 1));
					ASSERT_THAT(IsTrue(
						AttackAbility->GetAuthorityTraceSampleCountForTests() >
							State.TraceSampleBaseline));
					ASSERT_THAT(IsTrue(
						AttackAbility->GetAuthorityDamageHitCountForTests() ==
							State.DamageHitBaseline));
					ASSERT_THAT(IsTrue(State.Target->GetHealthDropCount() == 0));
					ASSERT_THAT(IsNull(State.Target->GetLastDamageSource()));
					ASSERT_THAT(IsNull(State.Target->GetLastDamageCauser()));
					ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(
						State.Target->GetHealthSet()->GetHealth(),
						State.TargetHealthBeforeAttack)));
					ASSERT_THAT(IsTrue(HasCleanAuthorityAttack(State)));
					BeginCleanupObservation(State, *AttackAbility);
				})
			.UntilClient(
				TEXT("Cancelled attack: owner ability and montage settle"),
				0,
				[](FNetworkState& State)
				{
					return IsClientAttackReady(State);
				},
				NetworkTimeout())
			.UntilServer(
				TEXT("Cancelled attack: trace and health stay stable for 0.2 seconds"),
				[](FNetworkState& State)
				{
					return HasStableAuthorityCleanup(State);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Cancelled attack: confirm no post-cleanup mutation"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsFalse(State.bObservedPostCleanupMutation));
				});
	}

	BEFORE_EACH()
	{
		using namespace RpgCombatPIENetworkTests;
		StepDescriptions.Reset();
		SubjectPlayerId = INDEX_NONE;
		AuthorityAttackAbilityHandle = FGameplayAbilitySpecHandle();

		URpgDeveloperSettings* DeveloperSettings =
			GetMutableDefault<URpgDeveloperSettings>();
		URpgCombatDeveloperSettings* CombatSettings =
			GetMutableDefault<URpgCombatDeveloperSettings>();
		OriginalExperienceOverride = DeveloperSettings->ExperienceOverride;
		bOriginalAttackLifecycleLogging =
			CombatSettings->bLogWeaponAttackLifecycle;
		CombatSettings->bLogWeaponAttackLifecycle = true;
		PilotGameModeClass = LoadClass<ARpgGameModeBase>(
			nullptr,
			PilotGameModeClassPath);
		ASSERT_THAT(IsNotNull(PilotGameModeClass));
		ARpgGameModeBase* GameModeDefaults = PilotGameModeClass
			? Cast<ARpgGameModeBase>(PilotGameModeClass->GetDefaultObject())
			: nullptr;
		ASSERT_THAT(IsNotNull(GameModeDefaults));
		bOriginalDiskPersistence = GameModeDefaults
			? GameModeDefaults->bEnableDiskPersistence
			: true;
		DeveloperSettings->ExperienceOverride = FPrimaryAssetId(
			URpgExperienceDefinition::StaticClass()->GetFName(),
			PilotExperienceName);
		if (GameModeDefaults)
		{
			GameModeDefaults->bEnableDiskPersistence = false;
		}

		PacketSettings = FPacketSimulationSettings();
		PacketSettings.PktLag = 60;
		PacketSettings.PktLagVariance = 10;
		FNetworkComponentBuilder<FNetworkState>()
			.WithClients(1)
			.AsListenServer()
			.WithPacketSimulationSettings(&PacketSettings)
			.WithGameInstanceClass(FSoftClassPath(
				TEXT("/Game/SurvivalRpg/Core/Game/BP_Rpg_GameInstance.BP_Rpg_GameInstance_C")))
			.WithGameMode(PilotGameModeClass)
			.Build(Network);
	}

	AFTER_EACH()
	{
		GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride =
			OriginalExperienceOverride;
		GetMutableDefault<URpgCombatDeveloperSettings>()
			->bLogWeaponAttackLifecycle = bOriginalAttackLifecycleLogging;
		if (IsValid(PilotGameModeClass))
		{
			CastChecked<ARpgGameModeBase>(PilotGameModeClass->GetDefaultObject())
				->bEnableDiskPersistence = bOriginalDiskPersistence;
		}
	}

	TEST_METHOD(RemoteClientAttackWindowDamageAndCancellation)
	{
		using namespace RpgCombatPIENetworkTests;

		Network
			.SpawnAndReplicate<
				ARpgGaspNetworkFloorFixture,
				&FNetworkState::Floor>(
				NetworkTimeout())
			.SpawnAndReplicate<
				ARpgCombatNetworkTargetFixture,
				&FNetworkState::Target>(
				[](ARpgCombatNetworkTargetFixture& Target)
				{
					Target.SetActorLocation(FVector(0.0, 0.0, 10000.0));
				},
				NetworkTimeout())
			.UntilServer(
				TEXT("GASP Pilot Experience loads on the listen server"),
				[](FNetworkState& State)
				{
					return IsPilotExperienceReady(State, 1);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Remote client owns a grounded GASP Pilot pawn"),
				0,
				[](FNetworkState& State)
				{
					ARpgCharacter* Character = FindLocalCharacter(State.World);
					return IsPilotExperienceReady(State, 1) &&
						IsPilotCharacterReady(Character) &&
						Character->GetLocalRole() == ROLE_AutonomousProxy &&
						Character->GetCharacterMovement()->IsMovingOnGround();
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Capture the remote autonomous pawn identity"),
				0,
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindLocalCharacter(State.World);
					ASSERT_THAT(IsNotNull(Character));
					ASSERT_THAT(IsNotNull(Character ? Character->GetPlayerState() : nullptr));
					if (!Character || !Character->GetPlayerState())
					{
						return;
					}
					SubjectPlayerId = Character->GetPlayerState()->GetPlayerId();
					ASSERT_THAT(IsTrue(SubjectPlayerId != INDEX_NONE));
					State.SubjectPlayerId = SubjectPlayerId;
				})
			.ThenServer(
				TEXT("Equip the real Basic Sword into the remote pawn MainHand"),
				[this](FNetworkState& State)
				{
					State.SubjectPlayerId = SubjectPlayerId;
					ARpgCharacter* Subject = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					ASSERT_THAT(IsNotNull(Subject));
					ASSERT_THAT(IsTrue(Subject && Subject->HasAuthority()));
					ASSERT_THAT(IsTrue(Subject &&
						Subject->GetRemoteRole() == ROLE_AutonomousProxy));
					if (!Subject)
					{
						return;
					}

					for (TActorIterator<ARpgCharacter> It(State.World); It; ++It)
					{
						ARpgCharacter* Character = *It;
						const FVector Location = Character == Subject
							? FVector(0.0, 0.0, 100.0)
							: FVector(10000.0, 0.0, 100.0);
						Character->TeleportTo(Location, FRotator::ZeroRotator);
						Character->GetCharacterMovement()->StopMovementImmediately();
						Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
						Character->ForceNetUpdate();
					}

					TSubclassOf<URpgEquipmentDefinition> BasicSwordDefinition =
						LoadClass<URpgEquipmentDefinition>(
							nullptr,
							BasicSwordDefinitionClassPath);
					ASSERT_THAT(IsNotNull(BasicSwordDefinition.Get()));
					URpgEquipmentManagerComponent* EquipmentManager =
						Subject->FindComponentByClass<URpgEquipmentManagerComponent>();
					ASSERT_THAT(IsNotNull(EquipmentManager));
					if (!EquipmentManager || !BasicSwordDefinition)
					{
						return;
					}

					EquipmentManager->UnequipItemInSlot(ERpgEquipmentSlot::MainHand);
					State.AuthorityWeapon = Cast<URpgWeaponInstance>(
						EquipmentManager->EquipItemInSlot(
							BasicSwordDefinition,
							ERpgEquipmentSlot::MainHand));
					ASSERT_THAT(IsTrue(State.AuthorityWeapon.IsValid()));
					MoveTargetAwayFromWeapon(State, Subject);
					Subject->ForceNetUpdate();
				})
			.UntilServer(
				TEXT("Authority has the real sword, exact primary spec, and idle ability"),
				[](FNetworkState& State)
				{
					return IsAuthorityAttackReady(State);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Capture the authority primary ability handle"),
				[this](FNetworkState& State)
				{
					AuthorityAttackAbilityHandle = State.AttackAbilityHandle;
					ASSERT_THAT(IsTrue(AuthorityAttackAbilityHandle.IsValid()));
				})
			.UntilClient(
				TEXT("Owner receives the authority sword source object and exact primary ability grant"),
				0,
				[this](FNetworkState& State)
				{
					State.AttackAbilityHandle = AuthorityAttackAbilityHandle;
					return IsClientAttackReady(State);
				},
				NetworkTimeout());

		for (int32 AttackIndex = 0; AttackIndex < NormalCompletedAttackCount; ++AttackIndex)
		{
			QueueCompletedAttack(AttackIndex);
		}
		QueueCompletedAttack(
			NormalCompletedAttackCount,
			FastMontageTimingPlayRate);
		QueueCancelledAttack();
	}
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
