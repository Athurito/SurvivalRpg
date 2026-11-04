#if WITH_DEV_AUTOMATION_TESTS

#include "RpgCombatProjectile.h"

#include "AbilitySystemGlobals.h"
#include "AIController.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayCueManager.h"
#include "GameplayEffect.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/BodyInstance.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgCombatSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgDefenseSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgStaminaSet.h"
#include "SurvivalRpg/AbilitySystem/Executions/RpgDamageExecution.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Combat/RpgCombatImpact.h"
#include "SurvivalRpg/Combat/RpgEmbeddedProjectileHostComponent.h"
#include "SurvivalRpg/Combat/RpgEmbeddedProjectilePickup.h"
#include "SurvivalRpg/Combat/RpgEmbeddedProjectileSubsystem.h"
#include "SurvivalRpg/Combat/RpgProjectileShotAutomationTestTypes.h"
#include "SurvivalRpg/Core/AI/RpgAICharacter.h"
#include "SurvivalRpg/Core/Corpse/RpgCorpseLifecycleComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Core/Game/RpgGameStateBase.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Interaction/IInteractableTarget.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Physics/RpgCollisionChannels.h"
#include "UObject/StrongObjectPtr.h"

namespace RpgCombatProjectileTests
{
	constexpr float StartingHealth = 100.0f;
	constexpr float ShotDamage = 20.0f;

	/** One combatant: a pawn with its own ability system, a faction through an AI controller and a body the projectile channel hits. */
	struct FCombatant
	{
		APawn* Pawn = nullptr;
		AAIController* Controller = nullptr;
		URpgAbilitySystemComponent* ASC = nullptr;
		USphereComponent* Body = nullptr;
	};

	/** A standalone world with a shooter (faction 1) and a target (faction 2) that face each other along X. */
	class FFixture
	{
	public:
		FFixture()
		{
			GameInstance.Reset(NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient));
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();

			DamageEffect.Reset(NewObject<UGameplayEffect>(GetTransientPackage(), NAME_None, RF_Transient));
			DamageEffect->DurationPolicy = EGameplayEffectDurationType::Instant;
			FGameplayEffectExecutionDefinition Execution;
			Execution.CalculationClass = URpgDamageExecution::StaticClass();
			DamageEffect->Executions.Add(Execution);
		}

		~FFixture()
		{
			GameInstance->Shutdown();
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
			GFrameCounter = CachedFrameCounter;
		}

		bool Initialize(FAutomationTestBase& Test)
		{
			if (!Test.TestNotNull(TEXT("Standalone projectile world exists"), World)) return false;
			Shooter = SpawnCombatant(FVector(-1000.0f, 0.0f, 0.0f), FRotator::ZeroRotator, 1);
			Target = SpawnCombatant(FVector::ZeroVector, FRotator(0.0f, 180.0f, 0.0f), 2);
			return Test.TestTrue(TEXT("Shooter and target exist"), Shooter.ASC && Target.ASC);
		}

		FCombatant SpawnCombatant(const FVector& Location, const FRotator& Rotation, uint8 TeamId) const
		{
			FCombatant Combatant;
			Combatant.Pawn = World->SpawnActor<APawn>(APawn::StaticClass(), Location, Rotation);
			Combatant.Body = NewObject<USphereComponent>(Combatant.Pawn, NAME_None, RF_Transient);
			Combatant.Body->InitSphereRadius(40.0f);
			Combatant.Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Combatant.Body->SetCollisionResponseToAllChannels(ECR_Ignore);
			Combatant.Body->SetCollisionResponseToChannel(Rpg_TraceChannel_Projectile, ECR_Block);
			Combatant.Pawn->SetRootComponent(Combatant.Body);
			Combatant.Pawn->AddInstanceComponent(Combatant.Body);
			Combatant.Body->RegisterComponent();
			Combatant.Pawn->SetActorLocationAndRotation(Location, Rotation);

			Combatant.Controller = World->SpawnActor<AAIController>();
			Combatant.Controller->SetGenericTeamId(FGenericTeamId(TeamId));
			Combatant.Controller->Possess(Combatant.Pawn);

			Combatant.ASC = NewObject<URpgAbilitySystemComponent>(Combatant.Pawn, NAME_None, RF_Transient);
			Combatant.Pawn->AddInstanceComponent(Combatant.ASC);
			Combatant.ASC->RegisterComponent();
			Combatant.ASC->AddAttributeSetSubobject(NewObject<URpgHealthSet>(Combatant.Pawn, NAME_None, RF_Transient));
			Combatant.ASC->AddAttributeSetSubobject(NewObject<URpgDefenseSet>(Combatant.Pawn, NAME_None, RF_Transient));
			Combatant.ASC->AddAttributeSetSubobject(NewObject<URpgStaminaSet>(Combatant.Pawn, NAME_None, RF_Transient));
			Combatant.ASC->AddAttributeSetSubobject(NewObject<URpgCombatSet>(Combatant.Pawn, NAME_None, RF_Transient));
			Combatant.ASC->InitAbilityActorInfo(Combatant.Pawn, Combatant.Pawn);

			const TPair<FGameplayAttribute, float> Bases[] = {
				{ URpgHealthSet::GetMaxHealthAttribute(), StartingHealth },
				{ URpgHealthSet::GetHealthAttribute(), StartingHealth },
				{ URpgDefenseSet::GetArmorAttribute(), 0.0f },
				{ URpgDefenseSet::GetStaggerAttribute(), 0.0f },
				{ URpgDefenseSet::GetMaxStaggerAttribute(), 100.0f },
				{ URpgDefenseSet::GetIncomingStaggerDamageMultiplierAttribute(), 1.0f },
				{ URpgDefenseSet::GetStaggeredDamageTakenMultiplierAttribute(), 1.0f },
				{ URpgDefenseSet::GetBlockAngleDegreesAttribute(), 120.0f },
				{ URpgDefenseSet::GetBlockStaminaCostAttribute(), 0.0f },
				{ URpgDefenseSet::GetBlockDamageReductionAttribute(), 0.5f },
				{ URpgDefenseSet::GetPerfectBlockStaminaRestoreAttribute(), 0.0f },
				{ URpgDefenseSet::GetPerfectBlockStaggerDamageAttribute(), 35.0f },
				{ URpgStaminaSet::GetMaxStaminaAttribute(), 100.0f },
				{ URpgStaminaSet::GetStaminaAttribute(), 100.0f },
				{ URpgCombatSet::GetBaseDamageAttribute(), 0.0f },
				{ URpgCombatSet::GetArmorPenetrationAttribute(), 0.0f },
				{ URpgCombatSet::GetCriticalHitChanceAttribute(), 0.0f },
			};
			for (const TPair<FGameplayAttribute, float>& Base : Bases)
			{
				Combatant.ASC->SetNumericAttributeBase(Base.Key, Base.Value);
			}
			return Combatant;
		}

		/** Launches from the shooter with a fresh spec, as an ability, NPC or system without equipment would. */
		ARpgCombatProjectile* Launch(const FVector& Origin, const FVector& Direction) const
		{
			FGameplayEffectContextHandle Context = Shooter.ASC->MakeEffectContext();
			const FGameplayEffectSpecHandle Spec(new FGameplayEffectSpec(DamageEffect.Get(), Context, 1.0f));

			FRpgProjectileLaunchParams Params;
			Params.Origin = Origin;
			Params.Direction = Direction;
			Params.Damage = ShotDamage;
			Params.StaggerDamage = 10.0f;
			Params.ShotId = 7;
			return ARpgCombatProjectile::LaunchWithDamageSpec(World, ARpgCombatProjectile::StaticClass(), Params, Spec, Shooter.Pawn);
		}

		/**
		 * Launches a projectile of Class that returns RecoveryItem when intact. The break roll is fixed to Roll, so a roll
		 * at or above BreakChance keeps it intact; Configure may change the class's recovery data before the impact.
		 */
		ARpgCombatProjectile* LaunchRecoverable(
			const FVector& Origin,
			const FVector& Direction,
			float BreakChance,
			float Roll,
			TSubclassOf<ARpgCombatProjectile> Class = ARpgProjectileRecoveryAutomationProjectile::StaticClass(),
			TSubclassOf<URpgInventoryItemDefinition> RecoveryItem = URpgProjectileRecoveryAutomationArrowDefinition::StaticClass(),
			TFunctionRef<void(FRpgProjectileRecoveryDefinition&)> Configure = [](FRpgProjectileRecoveryDefinition&) {}) const
		{
			const FGameplayEffectSpecHandle Spec(new FGameplayEffectSpec(DamageEffect.Get(), Shooter.ASC->MakeEffectContext(), 1.0f));
			FRpgProjectileLaunchParams Params;
			Params.Origin = Origin;
			Params.Direction = Direction;
			Params.Damage = ShotDamage;
			Params.BreakChance = BreakChance;
			Params.RecoveryItemDefinition = RecoveryItem;
			ARpgCombatProjectile* Projectile = ARpgCombatProjectile::LaunchWithDamageSpec(World, Class, Params, Spec, Shooter.Pawn);
			if (Projectile)
			{
				Projectile->SetBreakRollForTests(Roll);
				if (ARpgProjectileRecoveryAutomationProjectile* Recoverable = Cast<ARpgProjectileRecoveryAutomationProjectile>(Projectile))
				{
					Configure(Recoverable->GetMutableRecoveryForTests());
				}
			}
			return Projectile;
		}

		/** A world block, such as a wall or the floor, that the projectile channel and the drop trace hit. */
		UBoxComponent* SpawnBlock(const FVector& Location, const FVector& Extent, EComponentMobility::Type Mobility) const
		{
			AActor* Block = World->SpawnActor<AActor>();
			UBoxComponent* Box = NewObject<UBoxComponent>(Block, NAME_None, RF_Transient);
			Box->InitBoxExtent(Extent);
			Box->SetRelativeLocation(Location);
			Box->SetMobility(Mobility);
			Box->SetCollisionObjectType(ECC_WorldStatic);
			Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Box->SetCollisionResponseToAllChannels(ECR_Block);
			Block->SetRootComponent(Box);
			Block->AddInstanceComponent(Box);
			Box->RegisterComponent();
			if (Mobility == EComponentMobility::Movable)
			{
				// A moving host must be one every client can resolve; a runtime component only is when it replicates.
				Block->SetReplicates(true);
				Box->SetIsReplicated(true);
			}
			return Box;
		}

		/** A hit on Block whose flight segment ran along Direction into ImpactPoint, facing back along it. */
		static FHitResult MakeBlockHit(UBoxComponent* Block, const FVector& ImpactPoint, const FVector& Direction)
		{
			FHitResult Hit(Block->GetOwner(), Block, ImpactPoint, -Direction);
			Hit.bBlockingHit = true;
			Hit.Location = ImpactPoint;
			Hit.TraceStart = ImpactPoint - Direction * 200.0f;
			Hit.TraceEnd = ImpactPoint;
			Hit.Item = INDEX_NONE;
			return Hit;
		}

		/**
		 * Ticks the world in steps the world settings do not clamp. Each step is a new frame, or the timer manager ticks
		 * only once (see the GAS test pitfalls); the destructor restores the frame counter.
		 */
		void Advance(float Seconds) const
		{
			constexpr float Step = 0.25f;
			for (float Elapsed = 0.0f; Elapsed < Seconds - UE_KINDA_SMALL_NUMBER; Elapsed += Step)
			{
				++GFrameCounter;
				World->Tick(LEVELTICK_All, FMath::Min(Step, Seconds - Elapsed));
			}
		}

		URpgEmbeddedProjectileSubsystem* GetRecovery() const
		{
			return World->GetSubsystem<URpgEmbeddedProjectileSubsystem>();
		}

		/** A hit on the target's body whose flight segment ran along Direction into ImpactPoint. */
		FHitResult MakeTargetHit(const FVector& ImpactPoint, const FVector& Direction) const
		{
			FHitResult Hit(Target.Pawn, Target.Body, ImpactPoint, -Direction);
			Hit.bBlockingHit = true;
			Hit.Location = ImpactPoint;
			Hit.TraceStart = ImpactPoint - Direction * 200.0f;
			Hit.TraceEnd = ImpactPoint;
			Hit.Item = INDEX_NONE;
			return Hit;
		}

		float GetHealth(const FCombatant& Combatant) const
		{
			return Combatant.ASC->GetNumericAttribute(URpgHealthSet::GetHealthAttribute());
		}

		void ResetTarget() const
		{
			Target.ASC->SetNumericAttributeBase(URpgHealthSet::GetHealthAttribute(), StartingHealth);
			Target.ASC->SetLooseGameplayTagCount(RpgGameplayTags::State_Blocking, 0);
			Target.ASC->SetLooseGameplayTagCount(RpgGameplayTags::State_PerfectBlockWindow, 0);
			Target.ASC->SetLooseGameplayTagCount(RpgGameplayTags::State_Evading, 0);
			Target.ASC->SetActiveBlockableDamageTypes(FGameplayTagContainer());
		}

		void RaiseBlock(const FGameplayTagContainer& BlockableDamageTypes, bool bPerfectWindow) const
		{
			Target.ASC->SetLooseGameplayTagCount(RpgGameplayTags::State_Blocking, 1);
			Target.ASC->SetLooseGameplayTagCount(RpgGameplayTags::State_PerfectBlockWindow, bPerfectWindow ? 1 : 0);
			Target.ASC->SetActiveBlockableDamageTypes(BlockableDamageTypes);
		}

		const uint64 CachedFrameCounter = GFrameCounter;
		TStrongObjectPtr<UGameInstance> GameInstance;
		TStrongObjectPtr<UGameplayEffect> DamageEffect;
		UWorld* World = nullptr;
		FCombatant Shooter;
		FCombatant Target;
	};

	FGameplayTagContainer ProjectileBlock()
	{
		FGameplayTagContainer Tags;
		Tags.AddTag(RpgGameplayTags::Damage_Type_Melee);
		Tags.AddTag(RpgGameplayTags::Damage_Type_Projectile);
		return Tags;
	}

	FGameplayTagContainer MeleeOnlyBlock()
	{
		return FGameplayTagContainer(RpgGameplayTags::Damage_Type_Melee);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCombatProjectileDirectionalBlockTest,
	"SurvivalRpg.Combat.Projectile.DirectionalBlock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCombatProjectileDirectionalBlockTest::RunTest(const FString& Parameters)
{
	using namespace RpgCombatProjectileTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;

	// The target faces -X, toward the shooter at X = -1000.
	UAbilitySystemComponent* TargetASC = Fixture.Target.ASC;
	AActor* TargetActor = Fixture.Target.Pawn;
	const FVector FromFront(1.0f, 0.0f, 0.0f);
	const FVector FromBehind(-1.0f, 0.0f, 0.0f);

	TestEqual(TEXT("Without a raised block a projectile hits"),
		RpgCombatImpact::ResolveProjectileOutcome(TargetASC, TargetActor, FromFront), ERpgCombatImpactOutcome::Hit);

	Fixture.RaiseBlock(ProjectileBlock(), false);
	TestEqual(TEXT("A flight into the shield's front is blocked"),
		RpgCombatImpact::ResolveProjectileOutcome(TargetASC, TargetActor, FromFront), ERpgCombatImpactOutcome::Block);
	TestEqual(TEXT("A flight into the target's back hits"),
		RpgCombatImpact::ResolveProjectileOutcome(TargetASC, TargetActor, FromBehind), ERpgCombatImpactOutcome::Hit);
	TestEqual(TEXT("Without a flight direction a projectile hits"),
		RpgCombatImpact::ResolveProjectileOutcome(TargetASC, TargetActor, FVector::ZeroVector), ERpgCombatImpactOutcome::Hit);
	TestEqual(TEXT("Melee still resolves from the attacker's position"),
		RpgCombatImpact::ResolveMeleeOutcome(TargetASC, TargetActor, Fixture.Shooter.Pawn), ERpgCombatImpactOutcome::Block);

	Fixture.RaiseBlock(ProjectileBlock(), true);
	TestEqual(TEXT("The perfect-block window turns a frontal flight into a perfect block"),
		RpgCombatImpact::ResolveProjectileOutcome(TargetASC, TargetActor, FromFront), ERpgCombatImpactOutcome::PerfectBlock);

	TargetASC->SetNumericAttributeBase(URpgDefenseSet::GetBlockAngleDegreesAttribute(), 360.0f);
	TestEqual(TEXT("An all-around block stops a projectile even without a direction"),
		RpgCombatImpact::ResolveProjectileOutcome(TargetASC, TargetActor, FVector::ZeroVector), ERpgCombatImpactOutcome::PerfectBlock);

	FGameplayEffectContextHandle Context = Fixture.Shooter.ASC->MakeEffectContext();
	FHitResult Hit = Fixture.MakeTargetHit(FVector::ZeroVector, FromFront);
	Context.AddHitResult(Hit, true);
	TestTrue(TEXT("The incoming direction comes from the hit's flight segment"),
		RpgCombatImpact::GetIncomingDirection(Context).Equals(FromFront));

	FGameplayEffectContextHandle OriginContext = Fixture.Shooter.ASC->MakeEffectContext();
	Hit.TraceStart = Hit.TraceEnd = Hit.ImpactPoint = FVector(0.0f, 0.0f, 0.0f);
	OriginContext.AddHitResult(Hit, true);
	OriginContext.AddOrigin(FVector(0.0f, -500.0f, 0.0f));
	TestTrue(TEXT("Without a segment the incoming direction runs from the origin to the impact"),
		RpgCombatImpact::GetIncomingDirection(OriginContext).Equals(FVector(0.0f, 1.0f, 0.0f)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCombatProjectileImpactAppliesOnceTest,
	"SurvivalRpg.Combat.Projectile.ImpactAppliesOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCombatProjectileImpactAppliesOnceTest::RunTest(const FString& Parameters)
{
	using namespace RpgCombatProjectileTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;

	TestNull(TEXT("The shooter has no equipment"), Fixture.Shooter.Pawn->FindComponentByClass<URpgEquipmentManagerComponent>());
	const FVector Origin(-900.0f, 0.0f, 0.0f);
	ARpgCombatProjectile* Projectile = Fixture.Launch(Origin, FVector(1.0f, 0.0f, 0.0f));
	if (!TestNotNull(TEXT("A projectile launches without equipment or an ability"), Projectile)) return false;

	TestEqual(TEXT("The projectile keeps the shooter's faction"), Projectile->GetGenericTeamId().GetId(), static_cast<uint8>(1));
	TestEqual(TEXT("The projectile keeps its shot identity"), Projectile->GetShotId(), 7);
	TestTrue(TEXT("The projectile flies from its origin"), Projectile->GetActorLocation().Equals(Origin));
	TestFalse(TEXT("The projectile's own components never collide"), Projectile->GetActorEnableCollision());
	const FGameplayEffectContextHandle Context = Projectile->GetDamageSpecForTests().Data->GetContext();
	TestEqual(TEXT("The projectile is the damage's effect causer"), Context.GetEffectCauser(), static_cast<AActor*>(Projectile));
	TestEqual(TEXT("The shooter stays the damage's instigator"), Context.GetOriginalInstigator(), static_cast<AActor*>(Fixture.Shooter.Pawn));

	const FHitResult Hit = Fixture.MakeTargetHit(FVector(-40.0f, 0.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f));
	Projectile->ResolveImpactForTests(Hit);
	Projectile->ResolveImpactForTests(Hit);

	TestEqual(TEXT("A second impact report deals no second damage"), Fixture.GetHealth(Fixture.Target), StartingHealth - ShotDamage);
	TestEqual(TEXT("The impact resolves as a hit"), Projectile->GetImpactResult(), ERpgProjectileImpactResult::Hit);
	TestTrue(TEXT("The damage context keeps the launch origin after the hit result"),
		Context.HasOrigin() && Context.GetOrigin().Equals(Origin));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCombatProjectileShieldBlockTest,
	"SurvivalRpg.Combat.Projectile.ShieldBlock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCombatProjectileShieldBlockTest::RunTest(const FString& Parameters)
{
	using namespace RpgCombatProjectileTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;

	const FVector IntoFront(1.0f, 0.0f, 0.0f);
	const FVector IntoBack(-1.0f, 0.0f, 0.0f);
	const auto Shoot = [&Fixture](const FVector& Direction)
	{
		ARpgCombatProjectile* Projectile = Fixture.Launch(-Direction * 900.0f, Direction);
		if (Projectile)
		{
			Projectile->ResolveImpactForTests(Fixture.MakeTargetHit(-Direction * 40.0f, Direction));
		}
		return Projectile ? Projectile->GetImpactResult() : ERpgProjectileImpactResult::None;
	};

	// The shooter stands in front of the target in every case, so only the flight direction can decide the block.
	Fixture.RaiseBlock(ProjectileBlock(), false);
	TestEqual(TEXT("A shield blocks a frontal projectile"), Shoot(IntoFront), ERpgProjectileImpactResult::Blocked);
	TestEqual(TEXT("A blocked projectile deals the reduced damage"), Fixture.GetHealth(Fixture.Target), StartingHealth - ShotDamage * 0.5f);

	Fixture.ResetTarget();
	Fixture.RaiseBlock(ProjectileBlock(), false);
	TestEqual(TEXT("A projectile into the back hits although the shooter stands in front"), Shoot(IntoBack), ERpgProjectileImpactResult::Hit);
	TestEqual(TEXT("A projectile into the back deals full damage"), Fixture.GetHealth(Fixture.Target), StartingHealth - ShotDamage);

	Fixture.ResetTarget();
	Fixture.RaiseBlock(MeleeOnlyBlock(), false);
	TestEqual(TEXT("A block that cannot stop projectiles lets a frontal projectile hit"), Shoot(IntoFront), ERpgProjectileImpactResult::Hit);
	TestEqual(TEXT("A melee-only block takes the full projectile damage"), Fixture.GetHealth(Fixture.Target), StartingHealth - ShotDamage);

	Fixture.ResetTarget();
	Fixture.RaiseBlock(ProjectileBlock(), true);
	Fixture.Shooter.ASC->SetLooseGameplayTagCount(RpgGameplayTags::Trait_Staggerable, 1);
	TestEqual(TEXT("A perfect block stops a frontal projectile"), Shoot(IntoFront), ERpgProjectileImpactResult::PerfectBlocked);
	TestEqual(TEXT("A perfect block takes no damage"), Fixture.GetHealth(Fixture.Target), StartingHealth);
	TestEqual(TEXT("A perfect block never staggers the distant shooter"),
		Fixture.Shooter.ASC->GetNumericAttribute(URpgDefenseSet::GetStaggerAttribute()), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCombatProjectileLaunchStateTest,
	"SurvivalRpg.Combat.Projectile.LaunchState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCombatProjectileLaunchStateTest::RunTest(const FString& Parameters)
{
	using namespace RpgCombatProjectileTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;

	constexpr float LaunchBaseDamage = 5.0f;
	Fixture.Shooter.ASC->SetNumericAttributeBase(URpgCombatSet::GetBaseDamageAttribute(), LaunchBaseDamage);
	ARpgCombatProjectile* Projectile = Fixture.Launch(FVector(-900.0f, 0.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f));
	if (!TestNotNull(TEXT("The projectile launches"), Projectile)) return false;

	// Stronger after the launch, then gone: neither may change the projectile in flight.
	Fixture.Shooter.ASC->SetNumericAttributeBase(URpgCombatSet::GetBaseDamageAttribute(), 50.0f);
	Fixture.Shooter.Controller->Destroy();
	Fixture.Shooter.Pawn->Destroy();

	Projectile->ResolveImpactForTests(Fixture.MakeTargetHit(FVector(-40.0f, 0.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f)));
	TestEqual(TEXT("A projectile still hits after its shooter is gone"), Projectile->GetImpactResult(), ERpgProjectileImpactResult::Hit);
	TestEqual(TEXT("The damage uses the shooter's stats from the launch"),
		Fixture.GetHealth(Fixture.Target), StartingHealth - ShotDamage - LaunchBaseDamage);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCombatProjectileFlightSweepTest,
	"SurvivalRpg.Combat.Projectile.FlightSweep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCombatProjectileFlightSweepTest::RunTest(const FString& Parameters)
{
	using namespace RpgCombatProjectileTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;

	const FVector Start(-1100.0f, 0.0f, 0.0f);
	const FVector End(500.0f, 0.0f, 0.0f);
	ARpgCombatProjectile* Projectile = Fixture.Launch(Start, FVector(1.0f, 0.0f, 0.0f));
	if (!TestNotNull(TEXT("The projectile launches"), Projectile)) return false;

	FHitResult Hit;
	TestTrue(TEXT("The flight sweep passes the shooter and hits the target"), Projectile->SweepFlightSegmentForTests(Start, End, Hit));
	TestEqual(TEXT("The sweep reports the target"), Hit.GetActor(), static_cast<AActor*>(Fixture.Target.Pawn));

	Fixture.Target.ASC->SetLooseGameplayTagCount(RpgGameplayTags::State_Evading, 1);
	TestFalse(TEXT("A dodging target lets the projectile fly through"), Projectile->SweepFlightSegmentForTests(Start, End, Hit));
	Fixture.Target.ASC->SetLooseGameplayTagCount(RpgGameplayTags::State_Evading, 0);
	TestFalse(TEXT("A target the flight passed through stays passed"), Projectile->SweepFlightSegmentForTests(Start, End, Hit));

	ARpgCombatProjectile* Second = Fixture.Launch(Start, FVector(1.0f, 0.0f, 0.0f));
	TestTrue(TEXT("A new projectile hits the target that stopped dodging"), Second && Second->SweepFlightSegmentForTests(Start, End, Hit)
		&& Hit.GetActor() == Fixture.Target.Pawn);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCombatProjectileCollisionProfilesTest,
	"SurvivalRpg.Combat.Projectile.CollisionProfiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCombatProjectileCollisionProfilesTest::RunTest(const FString& Parameters)
{
	struct FExpectedResponse
	{
		const TCHAR* Profile;
		ECollisionResponse Response;
	};

	// Projectiles stop on world geometry, character meshes and corpses, and fly through capsules and invisible volumes.
	const FExpectedResponse ExpectedResponses[] = {
		{ TEXT("BlockAll"), ECR_Block },
		{ TEXT("BlockAllDynamic"), ECR_Block },
		{ TEXT("RpgHarvestable"), ECR_Block },
		{ TEXT("RpgPawnMesh"), ECR_Block },
		{ TEXT("Ragdoll"), ECR_Block },
		{ TEXT("RpgPawnCapsule"), ECR_Ignore },
		{ TEXT("CharacterCapsule"), ECR_Ignore },
		{ TEXT("Pawn"), ECR_Ignore },
		{ TEXT("Interactable_OverlapDynamic"), ECR_Ignore },
		{ TEXT("OverlapAll"), ECR_Ignore },
		{ TEXT("OverlapAllDynamic"), ECR_Ignore },
		{ TEXT("Trigger"), ECR_Ignore },
	};

	for (const FExpectedResponse& Expected : ExpectedResponses)
	{
		FCollisionResponseTemplate Template;
		if (!TestTrue(FString::Printf(TEXT("Profile %s exists"), Expected.Profile),
			UCollisionProfile::Get()->GetProfileTemplate(FName(Expected.Profile), Template)))
		{
			continue;
		}

		TestEqual(FString::Printf(TEXT("Profile %s answers the projectile channel as expected"), Expected.Profile),
			Template.ResponseToChannels.GetResponse(Rpg_TraceChannel_Projectile), Expected.Response);
	}

	// Components saved before slot 10 had a name store its responses under the slot's default name, such as the PCG
	// volume brush in Lvl_PortalRealm_RootGrove; the channel redirect keeps them on the projectile channel.
	FCollisionResponse SavedResponse;
	SavedResponse.SetResponsesArray({ FResponseChannel(TEXT("GameTraceChannel10"), ECR_Ignore) });
	TestEqual(TEXT("A response saved for the unnamed slot 10 still applies to the projectile channel"),
		SavedResponse.GetResponse(Rpg_TraceChannel_Projectile), ECR_Ignore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCombatProjectileWorldImpactTest,
	"SurvivalRpg.Combat.Projectile.WorldImpact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCombatProjectileWorldImpactTest::RunTest(const FString& Parameters)
{
	using namespace RpgCombatProjectileTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;

	// A reserved cue tag without a notify, so routing it plays nothing.
	const FGameplayTag CueTag = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Combat.Parry"), false);
	UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	if (!TestTrue(TEXT("The test cue tag exists"), CueTag.IsValid()) || !TestNotNull(TEXT("The cue manager exists"), CueManager)) return false;

	// The impact cue goes through the GameState's ability system, as in a running game.
	ARpgGameStateBase* GameState = Fixture.World->SpawnActor<ARpgGameStateBase>();
	if (!TestNotNull(TEXT("The RPG game state spawns"), GameState)) return false;
	Fixture.World->SetGameState(GameState);
	GameState->GetRpgAbilitySystemComponent()->InitAbilityActorInfo(GameState, GameState);

	TArray<FVector> CueLocations;
	const FDelegateHandle Routed = CueManager->OnGameplayCueRouted().AddLambda(
		[&CueLocations, CueTag](AActor*, const FGameplayTag Tag, const EGameplayCueEvent::Type Event,
			const FGameplayCueParameters& CueParameters, EGameplayCueExecutionOptions)
		{
			if (Event == EGameplayCueEvent::Executed && Tag == CueTag)
			{
				CueLocations.Add(CueParameters.Location);
			}
		});
	ON_SCOPE_EXIT { CueManager->OnGameplayCueRouted().Remove(Routed); };

	// A wall blocks the projectile channel and has no ability system.
	AActor* Wall = Fixture.World->SpawnActor<AActor>();
	USphereComponent* WallBody = NewObject<USphereComponent>(Wall, NAME_None, RF_Transient);
	WallBody->InitSphereRadius(50.0f);
	WallBody->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	WallBody->SetCollisionResponseToAllChannels(ECR_Block);
	Wall->SetRootComponent(WallBody);
	Wall->AddInstanceComponent(WallBody);
	WallBody->RegisterComponent();
	Wall->SetActorLocation(FVector(-450.0f, 0.0f, 0.0f));

	ARpgCombatProjectile* Projectile = Fixture.Launch(FVector(-900.0f, 0.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f));
	if (!TestNotNull(TEXT("The projectile launches"), Projectile)) return false;
	Projectile->SetImpactCueForTests(CueTag, false);

	const FVector WallPoint(-500.0f, 0.0f, 0.0f);
	FHitResult WallHit(Wall, WallBody, WallPoint, FVector(-1.0f, 0.0f, 0.0f));
	WallHit.bBlockingHit = true;
	WallHit.Location = WallPoint - FVector(2.0f, 0.0f, 0.0f);
	WallHit.TraceStart = WallPoint - FVector(200.0f, 0.0f, 0.0f);
	WallHit.TraceEnd = WallPoint;
	WallHit.Item = INDEX_NONE;
	Projectile->ResolveImpactForTests(WallHit);

	TestEqual(TEXT("A wall impact resolves as a world impact"), Projectile->GetImpactResult(), ERpgProjectileImpactResult::World);
	TestTrue(TEXT("The projectile stops at the sweep's location"), Projectile->GetActorLocation().Equals(WallHit.Location));
	TestTrue(TEXT("The stopped projectile lingers, then goes"), Projectile->GetLifeSpan() > 0.0f);
	TestEqual(TEXT("A wall impact damages nobody"), Fixture.GetHealth(Fixture.Target), StartingHealth);
	TestEqual(TEXT("A wall impact plays the impact cue once"), CueLocations.Num(), 1);
	TestTrue(TEXT("The impact cue plays at the impact point"), CueLocations.Num() == 1 && CueLocations[0].Equals(WallPoint));

	// On a target, the impact cue plays only when the projectile class asks for it, as arrows leave it to the hit cue.
	ARpgCombatProjectile* QuietOnTargets = Fixture.Launch(FVector(-900.0f, 0.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f));
	ARpgCombatProjectile* LoudOnTargets = Fixture.Launch(FVector(-900.0f, 0.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f));
	if (!TestTrue(TEXT("Both target projectiles launch"), QuietOnTargets && LoudOnTargets)) return false;
	QuietOnTargets->SetImpactCueForTests(CueTag, false);
	LoudOnTargets->SetImpactCueForTests(CueTag, true);

	QuietOnTargets->ResolveImpactForTests(Fixture.MakeTargetHit(FVector(-40.0f, 0.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f)));
	TestEqual(TEXT("The quiet projectile hits the target"), QuietOnTargets->GetImpactResult(), ERpgProjectileImpactResult::Hit);
	TestEqual(TEXT("Without impact cues on targets, a target hit plays none"), CueLocations.Num(), 1);

	LoudOnTargets->ResolveImpactForTests(Fixture.MakeTargetHit(FVector(-40.0f, 0.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f)));
	TestEqual(TEXT("The loud projectile hits the target"), LoudOnTargets->GetImpactResult(), ERpgProjectileImpactResult::Hit);
	TestEqual(TEXT("With impact cues on targets, a target hit plays one"), CueLocations.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCombatProjectileFlightExpiryTest,
	"SurvivalRpg.Combat.Projectile.FlightExpiry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCombatProjectileFlightExpiryTest::RunTest(const FString& Parameters)
{
	using namespace RpgCombatProjectileTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;

	// Far from both bodies. Nothing moves the projectile in this world, so every flight segment the server sweeps is clear.
	ARpgCombatProjectile* Projectile = Fixture.Launch(FVector(0.0f, 5000.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f));
	if (!TestNotNull(TEXT("The projectile launches"), Projectile)) return false;

	const float MaxFlightSeconds = GetDefault<ARpgCombatProjectile>()->GetMaxFlightSecondsForTests();
	Projectile->Tick(MaxFlightSeconds * 0.9f);
	TestEqual(TEXT("A projectile flies on before its flight time ends"), Projectile->GetImpactResult(), ERpgProjectileImpactResult::None);

	Projectile->Tick(MaxFlightSeconds * 0.2f);
	TestEqual(TEXT("A projectile without an impact expires after its flight time"), Projectile->GetImpactResult(), ERpgProjectileImpactResult::Expired);
	TestTrue(TEXT("The expired projectile lingers, then goes"), Projectile->GetLifeSpan() > 0.0f);
	TestEqual(TEXT("An expired flight damages nobody"), Fixture.GetHealth(Fixture.Target), StartingHealth);
	return true;
}

namespace RpgCombatProjectileTests
{
	bool IsGone(const TWeakObjectPtr<AActor>& Actor)
	{
		return !Actor.IsValid() || Actor->IsActorBeingDestroyed();
	}

	/** A flight mesh under the projectile's root, as an arrow Blueprint has one under its visual root. */
	UStaticMeshComponent* AddFlightMesh(ARpgCombatProjectile* Projectile)
	{
		if (!Projectile)
		{
			return nullptr;
		}
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Projectile, NAME_None, RF_Transient);
		Mesh->SetupAttachment(Projectile->GetRootComponent());
		Mesh->RegisterComponent();
		return Mesh;
	}

	/** One recovery shot into Block at ImpactPoint along +X that stays intact. */
	ARpgEmbeddedProjectilePickup* RecoverInto(
		const FFixture& Fixture,
		UBoxComponent* Block,
		const FVector& ImpactPoint,
		TFunctionRef<void(FRpgProjectileRecoveryDefinition&)> Configure = [](FRpgProjectileRecoveryDefinition&) {},
		TSubclassOf<URpgInventoryItemDefinition> Ammunition = URpgProjectileRecoveryAutomationArrowDefinition::StaticClass())
	{
		const FVector Direction(1.0f, 0.0f, 0.0f);
		ARpgCombatProjectile* Projectile = Fixture.LaunchRecoverable(ImpactPoint - Direction * 300.0f, Direction, 0.3f, 1.0f,
			ARpgProjectileRecoveryAutomationProjectile::StaticClass(), Ammunition, Configure);
		if (!Projectile)
		{
			return nullptr;
		}
		Projectile->ResolveImpactForTests(FFixture::MakeBlockHit(Block, ImpactPoint, Direction));
		return Projectile->GetRecoveredPickup();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCombatProjectileRecoveryWorldImpactTest,
	"SurvivalRpg.Combat.Projectile.Recovery.WorldImpactEmbedsOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCombatProjectileRecoveryWorldImpactTest::RunTest(const FString& Parameters)
{
	using namespace RpgCombatProjectileTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;

	UBoxComponent* Wall = Fixture.SpawnBlock(FVector(500.0f, 0.0f, 0.0f), FVector(10.0f, 2000.0f, 500.0f), EComponentMobility::Static);
	const FVector Direction(1.0f, 0.0f, 0.0f);
	const FVector ImpactPoint(490.0f, 0.0f, 20.0f);
	ARpgCombatProjectile* Projectile = Fixture.LaunchRecoverable(FVector(100.0f, 0.0f, 20.0f), Direction, 0.3f, 0.5f);
	if (!TestNotNull(TEXT("A recoverable arrow launches"), Projectile)) return false;
	const UStaticMeshComponent* FlightMesh = AddFlightMesh(Projectile);
	const FHitResult Hit = FFixture::MakeBlockHit(Wall, ImpactPoint, Direction);
	Projectile->ResolveImpactForTests(Hit);
	Projectile->ResolveImpactForTests(Hit);

	ARpgEmbeddedProjectilePickup* Pickup = Projectile->GetRecoveredPickup();
	if (!TestNotNull(TEXT("An intact arrow leaves a pickup"), Pickup)) return false;
	TestEqual(TEXT("A second impact report recovers nothing more"), Fixture.GetRecovery()->GetPickupCount(), 1);
	TestEqual(TEXT("The impact resolves as embedded"), Projectile->GetRecoveryOutcome(), ERpgProjectileRecoveryOutcome::Embedded);
	TestFalse(TEXT("The pickup takes over from the flight mesh"), FlightMesh && FlightMesh->IsVisible());
	TestEqual(TEXT("The pickup holds the one paid arrow"), Pickup->GetAmmunitionCount(), 1);
	TestTrue(TEXT("The pickup holds the arrow's ammunition"),
		Pickup->GetAmmunitionDefinition() == URpgProjectileRecoveryAutomationArrowDefinition::StaticClass());
	TestTrue(TEXT("The pickup is embedded"), Pickup->IsEmbedded());
	TestTrue(TEXT("The pickup sits in the wall along the flight"), Pickup->GetActorLocation().Equals(ImpactPoint + Direction * 4.0f, 0.1f));
	TestTrue(TEXT("The pickup points along the flight"), Pickup->GetActorForwardVector().Equals(Direction, 0.001f));
	TestTrue(TEXT("The wall hosts the pickup"), Pickup->GetHostComponent() == Wall);
	TestNull(TEXT("A static wall needs no attachment"), Pickup->GetAttachParentActor());
	TestEqual(TEXT("The pickup lasts the recovery lifetime"), Pickup->GetRecoveryDeadline(),
		Fixture.World->GetTimeSeconds() + 600.0, 0.01);

	const URpgInventoryContainerComponent* Container = Pickup->FindComponentByClass<URpgInventoryContainerComponent>();
	if (!TestNotNull(TEXT("The pickup has a loot container"), Container)) return false;
	TestTrue(TEXT("The pickup can be looted"), Container->IsContainerAccessible());
	TestTrue(TEXT("Nobody can deposit into the pickup"), Container->GetTransferPolicy() == ERpgInventoryContainerTransferPolicy::WithdrawOnly);
	TestFalse(TEXT("Crafting never draws from the pickup"), Container->AllowsCraftingAccess());
	TestTrue(TEXT("The pickup is never saved with the world"), Container->GetPersistentContainerId().IsNone());

	TArray<FInteractionOption> Options;
	FInteractionOptionBuilder Builder(TScriptInterface<IInteractableTarget>(Pickup), Options);
	Pickup->GatherInteractionOptions(FInteractionQuery(), Builder);
	TestTrue(TEXT("The prompt shows the quantity"), Options.Num() == 1 && Options[0].Prompt.TargetText.ToString().Contains(TEXT("(1)")));

	// A moving host carries its arrows along.
	UBoxComponent* Door = Fixture.SpawnBlock(FVector(500.0f, 3000.0f, 0.0f), FVector(10.0f, 200.0f, 200.0f), EComponentMobility::Movable);
	ARpgEmbeddedProjectilePickup* DoorPickup = RecoverInto(Fixture, Door, FVector(490.0f, 3000.0f, 0.0f));
	if (!TestNotNull(TEXT("An arrow in a moving host leaves a pickup"), DoorPickup)) return false;
	TestTrue(TEXT("The pickup attaches to the moving host"), DoorPickup->GetAttachParentActor() == Door->GetOwner());
	Door->GetOwner()->SetActorLocation(FVector(500.0f, 3500.0f, 0.0f));
	TestTrue(TEXT("The pickup follows the moving host"), DoorPickup->GetActorLocation().Equals(FVector(494.0f, 3500.0f, 0.0f), 0.1f));

	// A moving host no client can resolve, and any collectable, takes no arrow: it drops to the floor instead.
	UBoxComponent* Floor = Fixture.SpawnBlock(FVector(0.0f, 0.0f, -300.0f), FVector(6000.0f, 6000.0f, 50.0f), EComponentMobility::Static);
	UBoxComponent* LocalDoor = Fixture.SpawnBlock(FVector(500.0f, 4500.0f, 0.0f), FVector(10.0f, 200.0f, 200.0f), EComponentMobility::Movable);
	LocalDoor->GetOwner()->SetReplicates(false);
	LocalDoor->SetIsReplicated(false);
	ARpgEmbeddedProjectilePickup* LocalPickup = RecoverInto(Fixture, LocalDoor, FVector(490.0f, 4500.0f, 0.0f));
	TestTrue(TEXT("An arrow drops below a moving host no client can resolve"),
		LocalPickup && !LocalPickup->IsEmbedded() && LocalPickup->GetHostComponent() == Floor);
	TestTrue(TEXT("Another collectable never hosts an arrow"), !ARpgEmbeddedProjectilePickup::CanEmbedIn(DoorPickup->FindComponentByClass<UStaticMeshComponent>()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCombatProjectileRecoveryDropHostTest,
	"SurvivalRpg.Combat.Projectile.Recovery.DropFollowsOnlyResolvableHosts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCombatProjectileRecoveryDropHostTest::RunTest(const FString& Parameters)
{
	using namespace RpgCombatProjectileTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;

	const auto MakeLocal = [](UBoxComponent* Block)
	{
		Block->GetOwner()->SetReplicates(false);
		Block->SetIsReplicated(false);
	};
	const auto Shoot = [&Fixture](UBoxComponent* Block, const FVector& ImpactPoint, const FVector& Direction)
	{
		ARpgCombatProjectile* Projectile = Fixture.LaunchRecoverable(ImpactPoint - Direction * 300.0f, Direction, 0.3f, 1.0f);
		if (!Projectile)
		{
			return static_cast<ARpgEmbeddedProjectilePickup*>(nullptr);
		}
		Projectile->ResolveImpactForTests(FFixture::MakeBlockHit(Block, ImpactPoint, Direction));
		return Projectile->GetRecoveredPickup();
	};

	// An arrow coming down on a moving block no client can resolve cannot embed there and drops onto the same block
	// behind the contact. Following that block would move the pickup on the server alone, so it stays where it fell.
	UBoxComponent* LocalLift = Fixture.SpawnBlock(FVector(0.0f, 8000.0f, 0.0f), FVector(200.0f, 200.0f, 50.0f), EComponentMobility::Movable);
	MakeLocal(LocalLift);
	if (!TestFalse(TEXT("No client can resolve the local block"), ARpgEmbeddedProjectilePickup::CanEmbedIn(LocalLift))) return false;
	ARpgEmbeddedProjectilePickup* OnLocalLift = Shoot(LocalLift, FVector(0.0f, 8000.0f, 50.0f), FVector(1.0f, 0.0f, -1.0f).GetSafeNormal());
	if (!TestNotNull(TEXT("An arrow on an unresolvable moving block leaves a pickup"), OnLocalLift)) return false;
	TestFalse(TEXT("It lies on the block"), OnLocalLift->IsEmbedded());
	TestNull(TEXT("The unresolvable block does not host it"), OnLocalLift->GetHostComponent());
	TestNull(TEXT("It attaches to nothing"), OnLocalLift->GetAttachParentActor());
	const FVector Resting = OnLocalLift->GetActorLocation();
	TestEqual(TEXT("It rests on the block's top"), Resting.Z, 51.0, 0.5);
	LocalLift->GetOwner()->SetActorLocation(FVector(0.0f, 8500.0f, 0.0f));
	TestTrue(TEXT("It stays where it fell when the block moves"), OnLocalLift->GetActorLocation().Equals(Resting, 0.1f));

	// An arrow off an unresolvable moving door that stands on a resolvable moving lift drops onto the lift and rides it.
	UBoxComponent* Lift = Fixture.SpawnBlock(FVector(0.0f, 12000.0f, 0.0f), FVector(300.0f, 300.0f, 50.0f), EComponentMobility::Movable);
	UBoxComponent* LocalDoor = Fixture.SpawnBlock(FVector(100.0f, 12000.0f, 150.0f), FVector(10.0f, 100.0f, 100.0f), EComponentMobility::Movable);
	MakeLocal(LocalDoor);
	ARpgEmbeddedProjectilePickup* OnLift = Shoot(LocalDoor, FVector(90.0f, 12000.0f, 150.0f), FVector(1.0f, 0.0f, 0.0f));
	if (!TestNotNull(TEXT("An arrow off the door leaves a pickup"), OnLift)) return false;
	TestFalse(TEXT("It lies on the lift"), OnLift->IsEmbedded());
	TestTrue(TEXT("The resolvable lift hosts it"), OnLift->GetHostComponent() == Lift);
	TestTrue(TEXT("It attaches to the resolvable lift"), OnLift->GetAttachParentActor() == Lift->GetOwner());
	const FVector OnLiftBefore = OnLift->GetActorLocation();
	Lift->GetOwner()->SetActorLocation(FVector(0.0f, 12200.0f, 0.0f));
	TestTrue(TEXT("It follows the lift"), OnLift->GetActorLocation().Equals(OnLiftBefore + FVector(0.0f, 200.0f, 0.0f), 0.1f));

	// Ground that never moves hosts a dropped arrow for grouping, even without an owner actor, as level BSP has none.
	UBoxComponent* LevelFloor = NewObject<UBoxComponent>(Fixture.World->PersistentLevel, NAME_None, RF_Transient);
	LevelFloor->InitBoxExtent(FVector(300.0f, 300.0f, 50.0f));
	LevelFloor->SetRelativeLocation(FVector(0.0f, 16000.0f, 0.0f));
	LevelFloor->SetMobility(EComponentMobility::Static);
	LevelFloor->SetCollisionObjectType(ECC_WorldStatic);
	LevelFloor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	LevelFloor->SetCollisionResponseToAllChannels(ECR_Block);
	LevelFloor->RegisterComponentWithWorld(Fixture.World);
	if (!TestNull(TEXT("The level floor has no owner actor"), LevelFloor->GetOwner())) return false;
	ARpgEmbeddedProjectilePickup* OnLevelFloor = Shoot(LevelFloor, FVector(0.0f, 16000.0f, 50.0f), FVector(1.0f, 0.0f, -1.0f).GetSafeNormal());
	if (!TestNotNull(TEXT("An arrow on the level floor leaves a pickup"), OnLevelFloor)) return false;
	TestTrue(TEXT("The static level floor hosts it"), OnLevelFloor->GetHostComponent() == LevelFloor);
	TestNull(TEXT("A static host needs no attachment"), OnLevelFloor->GetAttachParentActor());
	LevelFloor->UnregisterComponent();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCombatProjectileRecoveryBreakRollTest,
	"SurvivalRpg.Combat.Projectile.Recovery.BreakRollAndSurfaces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCombatProjectileRecoveryBreakRollTest::RunTest(const FString& Parameters)
{
	using namespace RpgCombatProjectileTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;

	UBoxComponent* Wall = Fixture.SpawnBlock(FVector(500.0f, 0.0f, 0.0f), FVector(10.0f, 2000.0f, 500.0f), EComponentMobility::Static);
	Fixture.SpawnBlock(FVector(0.0f, 0.0f, -300.0f), FVector(3000.0f, 3000.0f, 50.0f), EComponentMobility::Static);
	TStrongObjectPtr<UPhysicalMaterial> Stone(NewObject<UPhysicalMaterial>(GetTransientPackage(), NAME_None, RF_Transient));
	TStrongObjectPtr<UPhysicalMaterial> Metal(NewObject<UPhysicalMaterial>(GetTransientPackage(), NAME_None, RF_Transient));
	const auto WithSurfaces = [&Stone, &Metal](FRpgProjectileRecoveryDefinition& Recovery)
	{
		FRpgProjectileSurfaceRecoveryRule StoneRule;
		StoneRule.PhysicalMaterial = Stone.Get();
		StoneRule.BreakChanceMultiplier = 2.0f;
		FRpgProjectileSurfaceRecoveryRule MetalRule;
		MetalRule.PhysicalMaterial = Metal.Get();
		MetalRule.bCanEmbed = false;
		Recovery.SurfaceRules = { StoneRule, MetalRule };
	};
	const FVector Direction(1.0f, 0.0f, 0.0f);
	const auto Launch = [&](float BreakChance, float Roll)
	{
		return Fixture.LaunchRecoverable(FVector(100.0f, 0.0f, 0.0f), Direction, BreakChance, Roll,
			ARpgProjectileRecoveryAutomationProjectile::StaticClass(), URpgProjectileRecoveryAutomationArrowDefinition::StaticClass(), WithSurfaces);
	};
	const auto Shoot = [&](float BreakChance, float Roll, UPhysicalMaterial* Material)
	{
		ARpgCombatProjectile* Projectile = Launch(BreakChance, Roll);
		if (!Projectile)
		{
			return ERpgProjectileRecoveryOutcome::None;
		}
		FHitResult Hit = FFixture::MakeBlockHit(Wall, FVector(490.0f, 0.0f, 0.0f), Direction);
		Hit.PhysMaterial = Material;
		Projectile->ResolveImpactForTests(Hit);
		return Projectile->GetRecoveryOutcome();
	};

	TestEqual(TEXT("A roll below the break chance breaks the arrow"), Shoot(0.3f, 0.29f, nullptr), ERpgProjectileRecoveryOutcome::Broken);
	TestEqual(TEXT("A broken arrow leaves no pickup"), Fixture.GetRecovery()->GetPickupCount(), 0);
	TestEqual(TEXT("A roll at the break chance keeps it intact"), Shoot(0.3f, 0.3f, nullptr), ERpgProjectileRecoveryOutcome::Embedded);
	TestEqual(TEXT("A certain break always breaks"), Shoot(1.0f, 0.999f, nullptr), ERpgProjectileRecoveryOutcome::Broken);
	TestEqual(TEXT("Without a break chance it never breaks"), Shoot(0.0f, 0.0f, nullptr), ERpgProjectileRecoveryOutcome::Embedded);
	TestEqual(TEXT("A surface rule scales the break chance"), Shoot(0.3f, 0.5f, Stone.Get()), ERpgProjectileRecoveryOutcome::Broken);
	TestEqual(TEXT("Above the scaled chance it stays intact"), Shoot(0.3f, 0.6f, Stone.Get()), ERpgProjectileRecoveryOutcome::Embedded);

	const int32 PickupsBeforeDrop = Fixture.GetRecovery()->GetPickupCount();
	ARpgCombatProjectile* Glancing = Launch(0.3f, 0.5f);
	if (!TestNotNull(TEXT("The glancing arrow launches"), Glancing)) return false;
	FHitResult MetalHit = FFixture::MakeBlockHit(Wall, FVector(490.0f, 0.0f, 0.0f), Direction);
	MetalHit.PhysMaterial = Metal.Get();
	Glancing->ResolveImpactForTests(MetalHit);
	const ARpgEmbeddedProjectilePickup* Dropped = Glancing->GetRecoveredPickup();
	TestEqual(TEXT("An intact arrow drops off a surface it cannot embed in"), Glancing->GetRecoveryOutcome(), ERpgProjectileRecoveryOutcome::Dropped);
	if (!TestNotNull(TEXT("The dropped arrow leaves a pickup"), Dropped)) return false;
	TestFalse(TEXT("The dropped pickup is not embedded"), Dropped->IsEmbedded());
	TestTrue(TEXT("It lies on the floor in front of the wall"), Dropped->GetActorLocation().Equals(FVector(460.0f, 0.0f, -249.0f), 0.1f));
	TestEqual(TEXT("It adds one pickup"), Fixture.GetRecovery()->GetPickupCount(), PickupsBeforeDrop + 1);

	ARpgCombatProjectile* OnMetalFloor = Fixture.LaunchRecoverable(FVector(0.0f, 1000.0f, 0.0f), FVector(0.0f, 0.0f, -1.0f), 0.3f, 0.5f,
		ARpgProjectileRecoveryAutomationProjectile::StaticClass(), URpgProjectileRecoveryAutomationArrowDefinition::StaticClass(), WithSurfaces);
	if (!TestNotNull(TEXT("The arrow into the metal floor launches"), OnMetalFloor)) return false;
	UBoxComponent* MetalFloor = Fixture.SpawnBlock(FVector(0.0f, 1000.0f, -150.0f), FVector(300.0f, 300.0f, 50.0f), EComponentMobility::Static);
	FHitResult MetalFloorHit = FFixture::MakeBlockHit(MetalFloor, FVector(0.0f, 1000.0f, -100.0f), FVector(0.0f, 0.0f, -1.0f));
	MetalFloorHit.PhysMaterial = Metal.Get();
	OnMetalFloor->ResolveImpactForTests(MetalFloorHit);
	const ARpgEmbeddedProjectilePickup* OnFloor = OnMetalFloor->GetRecoveredPickup();
	TestEqual(TEXT("An arrow on a floor it cannot embed in lies on that floor"), OnMetalFloor->GetRecoveryOutcome(), ERpgProjectileRecoveryOutcome::Dropped);
	TestTrue(TEXT("The floor it hit hosts it"), OnFloor && OnFloor->GetHostComponent() == MetalFloor);

	ARpgCombatProjectile* Untuned = Fixture.LaunchRecoverable(FVector(100.0f, 0.0f, 0.0f), Direction, -1.0f, 1.0f);
	TestEqual(TEXT("A launch without its own chance uses the class's base break chance"), Untuned ? Untuned->GetBreakChanceForTests() : -1.0f, 0.3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCombatProjectileRecoveryCharacterTest,
	"SurvivalRpg.Combat.Projectile.Recovery.CharactersKeepNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCombatProjectileRecoveryCharacterTest::RunTest(const FString& Parameters)
{
	using namespace RpgCombatProjectileTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;

	const FVector Direction(1.0f, 0.0f, 0.0f);
	const UStaticMeshComponent* FlightMesh = nullptr;
	const auto ShootTarget = [&Fixture, &Direction, &FlightMesh]()
	{
		ARpgCombatProjectile* Projectile = Fixture.LaunchRecoverable(FVector(-900.0f, 0.0f, 0.0f), Direction, 0.0f, 1.0f);
		FlightMesh = AddFlightMesh(Projectile);
		if (Projectile)
		{
			Projectile->ResolveImpactForTests(Fixture.MakeTargetHit(FVector(-40.0f, 0.0f, 0.0f), Direction));
		}
		return Projectile;
	};

	ARpgCombatProjectile* Hit = ShootTarget();
	if (!TestNotNull(TEXT("The arrow at the character launches"), Hit)) return false;
	TestEqual(TEXT("The arrow hits the character"), Hit->GetImpactResult(), ERpgProjectileImpactResult::Hit);
	TestEqual(TEXT("A character keeps no arrow"), Hit->GetRecoveryOutcome(), ERpgProjectileRecoveryOutcome::None);
	TestEqual(TEXT("A character hit leaves no pickup"), Fixture.GetRecovery()->GetPickupCount(), 0);
	TestFalse(TEXT("No arrow mesh stays on the character"), FlightMesh && FlightMesh->IsVisible());

	// A pawn whose ability system is gone, as on an NPC corpse, keeps no arrow either.
	APawn* Corpse = Fixture.World->SpawnActor<APawn>(APawn::StaticClass(), FTransform(FVector(0.0f, 2000.0f, 0.0f)));
	UBoxComponent* CorpseBody = NewObject<UBoxComponent>(Corpse, NAME_None, RF_Transient);
	CorpseBody->InitBoxExtent(FVector(40.0f));
	CorpseBody->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CorpseBody->SetCollisionResponseToAllChannels(ECR_Block);
	Corpse->SetRootComponent(CorpseBody);
	Corpse->AddInstanceComponent(CorpseBody);
	CorpseBody->RegisterComponent();
	ARpgCombatProjectile* AtCorpse = Fixture.LaunchRecoverable(FVector(-900.0f, 2000.0f, 0.0f), Direction, 0.0f, 1.0f);
	if (!TestNotNull(TEXT("The arrow at the corpse launches"), AtCorpse)) return false;
	AtCorpse->ResolveImpactForTests(FFixture::MakeBlockHit(CorpseBody, FVector(-40.0f, 2000.0f, 0.0f), Direction));
	TestEqual(TEXT("A pawn without an ability system resolves as a world hit"), AtCorpse->GetImpactResult(), ERpgProjectileImpactResult::World);
	TestEqual(TEXT("A pawn without an ability system keeps no arrow"), AtCorpse->GetRecoveryOutcome(), ERpgProjectileRecoveryOutcome::None);

	// Without ground in reach a blocked arrow is lost.
	Fixture.RaiseBlock(ProjectileBlock(), false);
	ARpgCombatProjectile* Lost = ShootTarget();
	if (!TestNotNull(TEXT("The arrow over the void launches"), Lost)) return false;
	TestEqual(TEXT("The shield stops the arrow over the void"), Lost->GetImpactResult(), ERpgProjectileImpactResult::Blocked);
	TestEqual(TEXT("Without ground a blocked arrow is lost"), Lost->GetRecoveryOutcome(), ERpgProjectileRecoveryOutcome::None);
	TestEqual(TEXT("A lost arrow leaves no pickup"), Fixture.GetRecovery()->GetPickupCount(), 0);

	// A trigger volume between the shield and the floor does not catch the falling arrow.
	UBoxComponent* Floor = Fixture.SpawnBlock(FVector(0.0f, 0.0f, -100.0f), FVector(3000.0f, 3000.0f, 50.0f), EComponentMobility::Static);
	UBoxComponent* Trigger = Fixture.SpawnBlock(FVector(-70.0f, 0.0f, -20.0f), FVector(100.0f, 100.0f, 5.0f), EComponentMobility::Static);
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Overlap);

	for (const bool bPerfect : { false, true })
	{
		Fixture.ResetTarget();
		Fixture.RaiseBlock(ProjectileBlock(), bPerfect);
		ARpgCombatProjectile* Blocked = ShootTarget();
		if (!TestNotNull(TEXT("The blocked arrow launches"), Blocked)) return false;
		TestEqual(TEXT("The shield stops the arrow"), Blocked->GetImpactResult(),
			bPerfect ? ERpgProjectileImpactResult::PerfectBlocked : ERpgProjectileImpactResult::Blocked);
		TestEqual(TEXT("A blocked intact arrow drops"), Blocked->GetRecoveryOutcome(), ERpgProjectileRecoveryOutcome::Dropped);
		const ARpgEmbeddedProjectilePickup* Pickup = Blocked->GetRecoveredPickup();
		if (!TestNotNull(TEXT("The blocked arrow leaves a pickup"), Pickup)) return false;
		TestTrue(TEXT("The pickup never stays on the blocker"), Pickup->GetAttachParentActor() != Fixture.Target.Pawn);
		TestTrue(TEXT("The floor hosts the dropped arrow"), Pickup->GetHostComponent() == Floor);
		TestTrue(TEXT("It lies on the floor in front of the shield"), Pickup->GetActorLocation().Equals(FVector(-70.0f, 0.0f, -49.0f), 0.1f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCombatProjectileRecoveryNothingToRecoverTest,
	"SurvivalRpg.Combat.Projectile.Recovery.ConsumedOrUnpaidLeavesNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCombatProjectileRecoveryNothingToRecoverTest::RunTest(const FString& Parameters)
{
	using namespace RpgCombatProjectileTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;

	UBoxComponent* Wall = Fixture.SpawnBlock(FVector(500.0f, 0.0f, 0.0f), FVector(10.0f, 2000.0f, 500.0f), EComponentMobility::Static);
	const FVector Direction(1.0f, 0.0f, 0.0f);
	bool bMeshesStayed = true;
	const auto Shoot = [&](TSubclassOf<ARpgCombatProjectile> Class, TSubclassOf<URpgInventoryItemDefinition> RecoveryItem)
	{
		// A certain break: any break roll would report Broken.
		ARpgCombatProjectile* Projectile = Fixture.LaunchRecoverable(FVector(100.0f, 0.0f, 0.0f), Direction, 1.0f, 0.0f, Class, RecoveryItem);
		const UStaticMeshComponent* FlightMesh = AddFlightMesh(Projectile);
		if (Projectile)
		{
			Projectile->ResolveImpactForTests(FFixture::MakeBlockHit(Wall, FVector(490.0f, 0.0f, 0.0f), Direction));
		}
		bMeshesStayed &= FlightMesh && FlightMesh->IsVisible();
		return Projectile ? Projectile->GetRecoveryOutcome() : ERpgProjectileRecoveryOutcome::Broken;
	};

	TestEqual(TEXT("A projectile consumed on impact never rolls"),
		Shoot(ARpgProjectileShotAutomationTestProjectile::StaticClass(), URpgProjectileRecoveryAutomationArrowDefinition::StaticClass()),
		ERpgProjectileRecoveryOutcome::None);
	TestEqual(TEXT("An arrow nobody paid for never rolls"),
		Shoot(ARpgProjectileRecoveryAutomationProjectile::StaticClass(), nullptr), ERpgProjectileRecoveryOutcome::None);
	TestTrue(TEXT("A world stop with nothing to recover keeps its mesh while it lingers"), bMeshesStayed);

	ARpgCombatProjectile* Expiring = Fixture.LaunchRecoverable(FVector(0.0f, 5000.0f, 0.0f), Direction, 0.0f, 1.0f);
	if (!TestNotNull(TEXT("The expiring arrow launches"), Expiring)) return false;
	Expiring->Tick(GetDefault<ARpgProjectileRecoveryAutomationProjectile>()->GetMaxFlightSecondsForTests() + 1.0f);
	TestEqual(TEXT("The flight expires"), Expiring->GetImpactResult(), ERpgProjectileImpactResult::Expired);
	TestEqual(TEXT("An expired flight recovers nothing"), Expiring->GetRecoveryOutcome(), ERpgProjectileRecoveryOutcome::None);
	TestEqual(TEXT("Nothing left a pickup"), Fixture.GetRecovery()->GetPickupCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCombatProjectileRecoveryGroupingTest,
	"SurvivalRpg.Combat.Projectile.Recovery.GroupingAndWorldCap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCombatProjectileRecoveryGroupingTest::RunTest(const FString& Parameters)
{
	using namespace RpgCombatProjectileTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;

	UBoxComponent* Wall = Fixture.SpawnBlock(FVector(500.0f, 0.0f, 0.0f), FVector(10.0f, 3000.0f, 500.0f), EComponentMobility::Static);
	UBoxComponent* OtherWall = Fixture.SpawnBlock(FVector(-3000.0f, 0.0f, 0.0f), FVector(10.0f, 3000.0f, 500.0f), EComponentMobility::Static);
	const auto TwoPerHost = [](FRpgProjectileRecoveryDefinition& Recovery)
	{
		Recovery.MaxSeparatePickupsPerHost = 2;
		Recovery.GroupRadius = 100.0f;
	};
	URpgEmbeddedProjectileSubsystem* Recovery = Fixture.GetRecovery();

	ARpgEmbeddedProjectilePickup* First = RecoverInto(Fixture, Wall, FVector(490.0f, 0.0f, 0.0f), TwoPerHost);
	ARpgEmbeddedProjectilePickup* Second = RecoverInto(Fixture, Wall, FVector(490.0f, 500.0f, 0.0f), TwoPerHost);
	if (!TestTrue(TEXT("The first two arrows leave separate pickups"), First && Second && First != Second)) return false;
	const TWeakObjectPtr<AActor> WeakSecond(Second);
	const double FirstDeadline = First->GetRecoveryDeadline();

	Fixture.Advance(5.0f);
	ARpgEmbeddedProjectilePickup* Joined = RecoverInto(Fixture, Wall, FVector(490.0f, 50.0f, 0.0f), TwoPerHost);
	TestTrue(TEXT("A full host takes the next arrow into its nearest group"), Joined == First);
	TestEqual(TEXT("The group carries both arrows"), First->GetAmmunitionCount(), 2);
	TestEqual(TEXT("Grouping adds no actor"), Recovery->GetPickupCount(), 2);
	TestEqual(TEXT("The group keeps the later deadline"), First->GetRecoveryDeadline(), FirstDeadline + 5.0, 0.01);

	ARpgEmbeddedProjectilePickup* Far = RecoverInto(Fixture, Wall, FVector(490.0f, 1500.0f, 0.0f), TwoPerHost);
	TestTrue(TEXT("An arrow outside every group gets its own pickup"), Far && Far != First && Far != Second);
	ARpgEmbeddedProjectilePickup* OtherAmmunition = RecoverInto(Fixture, Wall, FVector(490.0f, 10.0f, 0.0f), TwoPerHost,
		URpgProjectileShotAutomationArrowDefinition::StaticClass());
	TestTrue(TEXT("Another ammunition never joins the group"), OtherAmmunition && OtherAmmunition != First);
	TestEqual(TEXT("Four pickups exist"), Recovery->GetPickupCount(), 4);

	// The world cap expires the pickup closest to its deadline: the second, which no later arrow joined.
	Recovery->SetMaxWorldArrowPickupsForTests(4);
	ARpgEmbeddedProjectilePickup* BeyondCap = RecoverInto(Fixture, OtherWall, FVector(-3010.0f, 0.0f, 0.0f));
	TestNotNull(TEXT("The new pickup beyond the cap stays"), BeyondCap);
	TestTrue(TEXT("The cap expires the oldest pickup early"), IsGone(WeakSecond));
	TestEqual(TEXT("The world keeps its cap"), Recovery->GetPickupCount(), 4);
	TestEqual(TEXT("The group's arrows survive the cap"), First->GetAmmunitionCount(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCombatProjectileRecoveryCollectTest,
	"SurvivalRpg.Combat.Projectile.Recovery.CollectExpireAndLoseHost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCombatProjectileRecoveryCollectTest::RunTest(const FString& Parameters)
{
	using namespace RpgCombatProjectileTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;

	UBoxComponent* Wall = Fixture.SpawnBlock(FVector(500.0f, 0.0f, 0.0f), FVector(10.0f, 3000.0f, 500.0f), EComponentMobility::Static);
	const auto OnePerHost = [](FRpgProjectileRecoveryDefinition& Recovery)
	{
		Recovery.MaxSeparatePickupsPerHost = 1;
		Recovery.RecoveryLifetimeSeconds = 3.0f;
	};
	ARpgEmbeddedProjectilePickup* Group = nullptr;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Group = RecoverInto(Fixture, Wall, FVector(490.0f, 0.0f, 0.0f), OnePerHost);
	}
	if (!TestNotNull(TEXT("Three arrows share one pickup"), Group) || !TestEqual(TEXT("The group holds three"), Group->GetAmmunitionCount(), 3)) return false;
	const TWeakObjectPtr<AActor> WeakGroup(Group);

	// A looter with room for only two arrows: every cell taken, one stack at 18 of 20.
	const auto MakeLooter = [&Fixture](const TCHAR* Name)
	{
		AActor* Owner = Fixture.World->SpawnActor<AActor>();
		URpgInventoryManagerComponent* Inventory = NewObject<URpgInventoryManagerComponent>(Owner, Name, RF_Transient);
		Owner->AddInstanceComponent(Inventory);
		Inventory->RegisterComponent();
		return Inventory;
	};
	URpgInventoryManagerComponent* FullLooter = MakeLooter(TEXT("FullLooter"));
	const FRpgInventoryContainerHandle Root = FRpgInventoryContainerHandle::MakeRoot(FullLooter->GetDefaultContainerId());
	bool bFilled = true;
	for (int32 Y = 0; Y < 6; ++Y)
	{
		for (int32 X = 0; X < 10; ++X)
		{
			FRpgInventoryGridPlacement Placement;
			Placement.SetContainerHandle(Root);
			Placement.X = X;
			Placement.Y = Y;
			const bool bArrowCell = X == 9 && Y == 5;
			const TSubclassOf<URpgInventoryItemDefinition> Item = bArrowCell
				? TSubclassOf<URpgInventoryItemDefinition>(URpgProjectileRecoveryAutomationArrowDefinition::StaticClass())
				: TSubclassOf<URpgInventoryItemDefinition>(URpgInventoryAutomationTestUnitItemDefinition::StaticClass());
			bFilled &= FullLooter->AddItemDefinitionToPlacement(Item, bArrowCell ? 18 : 1, Placement) != nullptr;
		}
	}
	if (!TestTrue(TEXT("The looter's grid is full"), bFilled)) return false;

	TArray<FRpgInventoryItemId> Added;
	Group->GetLootInventoryManager()->CollectRootItemsBatch(FullLooter, { Root }, FGuid::NewGuid(), Added);
	TestEqual(TEXT("A full inventory takes what fits"),
		FullLooter->GetTotalItemCountByDefinition(URpgProjectileRecoveryAutomationArrowDefinition::StaticClass()), 20);
	TestEqual(TEXT("The rest stays in the group"), Group->GetAmmunitionCount(), 1);
	TestFalse(TEXT("A group with arrows left stays"), IsGone(WeakGroup));

	URpgInventoryManagerComponent* SecondLooter = MakeLooter(TEXT("SecondLooter"));
	Group->GetLootInventoryManager()->CollectRootItemsBatch(SecondLooter,
		{ FRpgInventoryContainerHandle::MakeRoot(SecondLooter->GetDefaultContainerId()) }, FGuid::NewGuid(), Added);
	TestEqual(TEXT("A second looter gets only the remainder"),
		SecondLooter->GetTotalItemCountByDefinition(URpgProjectileRecoveryAutomationArrowDefinition::StaticClass()), 1);
	Fixture.Advance(0.25f);
	TestTrue(TEXT("An emptied group goes"), IsGone(WeakGroup));

	// Expiry ends an untouched pickup at its deadline.
	ARpgEmbeddedProjectilePickup* Expiring = RecoverInto(Fixture, Wall, FVector(490.0f, 1000.0f, 0.0f), OnePerHost);
	const TWeakObjectPtr<AActor> WeakExpiring(Expiring);
	Fixture.Advance(2.5f);
	TestFalse(TEXT("A pickup stays before its deadline"), IsGone(WeakExpiring));
	Fixture.Advance(1.0f);
	TestTrue(TEXT("A pickup expires at its deadline"), IsGone(WeakExpiring));

	// A pickup whose moving host goes stays where it was until its own deadline.
	UBoxComponent* Crate = Fixture.SpawnBlock(FVector(500.0f, 2000.0f, 0.0f), FVector(10.0f, 100.0f, 100.0f), EComponentMobility::Movable);
	ARpgEmbeddedProjectilePickup* Orphan = RecoverInto(Fixture, Crate, FVector(490.0f, 2000.0f, 0.0f), OnePerHost);
	if (!TestNotNull(TEXT("The arrow in the crate leaves a pickup"), Orphan)) return false;
	const TWeakObjectPtr<AActor> WeakOrphan(Orphan);
	const FVector OrphanLocation = Orphan->GetActorLocation();
	const double OrphanDeadline = Orphan->GetRecoveryDeadline();
	Crate->GetOwner()->Destroy();
	TestFalse(TEXT("The pickup outlives its host"), IsGone(WeakOrphan));
	TestNull(TEXT("The pickup is detached from the gone host"), Orphan->GetAttachParentActor());
	TestTrue(TEXT("The pickup stays at its last world transform"), Orphan->GetActorLocation().Equals(OrphanLocation, 0.1f));
	TestEqual(TEXT("The pickup keeps its deadline"), Orphan->GetRecoveryDeadline(), OrphanDeadline);
	Fixture.Advance(3.25f);
	TestTrue(TEXT("The orphaned pickup still expires at its deadline"), IsGone(WeakOrphan));
	return true;
}

namespace RpgCombatProjectileTests
{
	/** Gives Pawn what an AI character has to keep arrows: a skeletal mesh, loot inventory, loot source and host. */
	URpgEmbeddedProjectileHostComponent* AddCharacterRecovery(APawn* Pawn)
	{
		const auto Add = [Pawn](UActorComponent* Component)
		{
			Pawn->AddInstanceComponent(Component);
			Component->RegisterComponent();
		};
		USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>(Pawn, NAME_None, RF_Transient);
		Mesh->SetupAttachment(Pawn->GetRootComponent());
		Add(Mesh);
		Add(NewObject<URpgInventoryManagerComponent>(Pawn, NAME_None, RF_Transient));
		Add(NewObject<URpgProjectileRecoveryAutomationLootSource>(Pawn, NAME_None, RF_Transient));
		URpgEmbeddedProjectileHostComponent* Host = NewObject<URpgEmbeddedProjectileHostComponent>(Pawn, NAME_None, RF_Transient);
		Add(Host);
		return Host;
	}

	int32 CountGroupArrows(const URpgEmbeddedProjectileHostComponent* Host)
	{
		int32 Count = 0;
		for (const FRpgEmbeddedProjectileGroup& Group : Host->GetGroups())
		{
			Count += Group.Count;
		}
		return Count;
	}

	/** Arrow content whose pickup has a mesh, so character shafts show. */
	void UseArrowContent(FRpgProjectileRecoveryDefinition& Recovery)
	{
		Recovery.PickupClass = LoadClass<ARpgEmbeddedProjectilePickup>(nullptr,
			TEXT("/GF_Combat_Ranged/Projectiles/BP_EmbeddedPickup_Arrow.BP_EmbeddedPickup_Arrow_C"));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCombatProjectileRecoveryCharacterLootTest,
	"SurvivalRpg.Combat.Projectile.Recovery.AICharacterKeepsArrowsAsCorpseLoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCombatProjectileRecoveryCharacterLootTest::RunTest(const FString& Parameters)
{
	using namespace RpgCombatProjectileTests;
	const ARpgAICharacter* EnemyDefaults = GetDefault<ARpgAICharacter>();
	TestTrue(TEXT("Every AI character keeps arrows"),
		EnemyDefaults && EnemyDefaults->FindComponentByClass<URpgEmbeddedProjectileHostComponent>());

	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	URpgEmbeddedProjectileHostComponent* Host = AddCharacterRecovery(Fixture.Target.Pawn);
	URpgInventoryManagerComponent* Corpse = Fixture.Target.Pawn->FindComponentByClass<URpgInventoryManagerComponent>();
	URpgLootSourceComponent* Loot = Fixture.Target.Pawn->FindComponentByClass<URpgLootSourceComponent>();
	if (!TestTrue(TEXT("The AI target keeps arrows"), Host && Corpse && Loot && Host->CanRecover())) return false;
	const TSubclassOf<URpgInventoryItemDefinition> Arrow = URpgProjectileRecoveryAutomationArrowDefinition::StaticClass();
	const FVector Direction(1.0f, 0.0f, 0.0f);
	ARpgCombatProjectile* LastProjectile = nullptr;
	const auto Shoot = [&Fixture, &Direction, &LastProjectile](const float BreakChance, const float Roll)
	{
		ARpgCombatProjectile* Projectile = Fixture.LaunchRecoverable(FVector(-900.0f, 0.0f, 0.0f), Direction, BreakChance, Roll,
			ARpgProjectileRecoveryAutomationProjectile::StaticClass(), URpgProjectileRecoveryAutomationArrowDefinition::StaticClass(),
			[](FRpgProjectileRecoveryDefinition& Recovery) { UseArrowContent(Recovery); });
		LastProjectile = Projectile;
		if (Projectile)
		{
			Projectile->ResolveImpactForTests(Fixture.MakeTargetHit(FVector(-40.0f, 0.0f, 0.0f), Direction));
		}
		return Projectile ? Projectile->GetRecoveryOutcome() : ERpgProjectileRecoveryOutcome::None;
	};

	// A harvest-only corpse builds no loot, so nothing could ever take an arrow out of it.
	Loot->SetGenerateLootOnDeathEnabled(false);
	TestFalse(TEXT("A body without death loot keeps no arrow"), Host->CanRecover());
	TestEqual(TEXT("An arrow into it is gone"), Shoot(0.0f, 1.0f), ERpgProjectileRecoveryOutcome::None);
	TestEqual(TEXT("It records nothing"), Host->GetPendingCount(), 0);
	Loot->SetGenerateLootOnDeathEnabled(true);

	// The record exists before the damage applies, so an arrow that kills is part of the death loot.
	int32 PendingWhenDamaged = INDEX_NONE;
	const FDelegateHandle DamageObserver = Fixture.Target.ASC->OnGameplayEffectAppliedDelegateToSelf.AddLambda(
		[Host, &PendingWhenDamaged](UAbilitySystemComponent*, const FGameplayEffectSpec&, FActiveGameplayEffectHandle)
		{
			if (PendingWhenDamaged == INDEX_NONE)
			{
				PendingWhenDamaged = Host->GetPendingCount();
			}
		});
	for (int32 Index = 0; Index < 3; ++Index)
	{
		TestEqual(TEXT("An intact arrow lodges in the AI target"), Shoot(0.0f, 1.0f), ERpgProjectileRecoveryOutcome::Embedded);
	}
	Fixture.Target.ASC->OnGameplayEffectAppliedDelegateToSelf.Remove(DamageObserver);
	TestEqual(TEXT("The first arrow was recorded before its damage applied"), PendingWhenDamaged, 1);
	TestEqual(TEXT("Three arrows wait for the corpse loot"), Host->GetPendingCount(), 3);
	TestEqual(TEXT("Three shafts show"), Host->GetGroups().Num(), 3);
	TestEqual(TEXT("This machine attaches three shaft meshes"), Host->GetVisibleShaftCount(), 3);
	const UStaticMeshComponent* Shaft = Host->GetGroups().IsEmpty() ? nullptr : Host->GetShaftForTests(Host->GetGroups()[0].GroupId);
	TestTrue(TEXT("A shaft rides on the character's skeletal mesh"),
		Shaft && Shaft->GetAttachParent() == Fixture.Target.Pawn->FindComponentByClass<USkeletalMeshComponent>());
	TestEqual(TEXT("A broken arrow lodges nothing"), Shoot(1.0f, 0.0f), ERpgProjectileRecoveryOutcome::Broken);
	TestEqual(TEXT("A broken arrow adds no record"), Host->GetPendingCount(), 3);

	Fixture.RaiseBlock(ProjectileBlock(), false);
	Shoot(0.0f, 1.0f);
	TestEqual(TEXT("A blocked arrow never stays on the blocker"), Host->GetPendingCount(), 3);
	TestEqual(TEXT("A blocked arrow adds no shaft"), Host->GetGroups().Num(), 3);
	Fixture.ResetTarget();

	// Death: the loot carries every lodged arrow once.
	Loot->PopulateLoot();
	TestEqual(TEXT("The corpse loot holds the three arrows"), Corpse->GetTotalItemCountByDefinition(Arrow), 3);
	TestEqual(TEXT("No arrow waits any more"), Host->GetPendingCount(), 0);
	TestEqual(TEXT("All three are credited"), Host->GetCreditedCount(), 3);

	// A later hit on the corpse joins its loot at once.
	TestEqual(TEXT("An arrow into the corpse lodges"), Shoot(0.0f, 1.0f), ERpgProjectileRecoveryOutcome::Embedded);
	TestEqual(TEXT("The corpse loot takes the late arrow"), Corpse->GetTotalItemCountByDefinition(Arrow), 4);
	TestEqual(TEXT("The late arrow is credited at once"), Host->GetCreditedCount(), 4);
	TestEqual(TEXT("Four shafts show"), Host->GetGroups().Num(), 4);
	TestTrue(TEXT("A record already in the corpse loot is never withdrawn"),
		LastProjectile && LastProjectile->GetCharacterRecordId() != 0 && !Host->CancelRecord(LastProjectile->GetCharacterRecordId()));
	TestEqual(TEXT("It stays credited"), Host->GetCreditedCount(), 4);

	// Looting retires shafts in credit order; only what leaves the corpse counts.
	Corpse->ConsumeItemsByDefinition(Arrow, 2);
	TestEqual(TEXT("Two taken arrows retire two credits"), Host->GetCreditedCount(), 2);
	TestEqual(TEXT("Their shafts go"), Host->GetGroups().Num(), 2);
	TestEqual(TEXT("This machine removes their meshes"), Host->GetVisibleShaftCount(), 2);
	Corpse->ConsumeItemsByDefinition(URpgInventoryAutomationTestUnitItemDefinition::StaticClass(), 1);
	TestEqual(TEXT("Taking other loot retires no arrow"), Host->GetCreditedCount(), 2);

	// Arrows of the same kind from the loot table are interchangeable: shafts go only once fewer arrows are left.
	if (!TestNotNull(TEXT("The corpse gets two more arrows of the same kind"), Corpse->GrantItemDefinition(Arrow, 2))) return false;
	Corpse->ConsumeItemsByDefinition(Arrow, 2);
	TestEqual(TEXT("Taking as many arrows as the table gave retires nothing"), Host->GetCreditedCount(), 2);
	TestEqual(TEXT("Both shafts still show"), Host->GetGroups().Num(), 2);
	Corpse->ConsumeItemsByDefinition(Arrow, 1);
	TestEqual(TEXT("One more taken arrow retires one credit"), Host->GetCreditedCount(), 1);
	TestEqual(TEXT("One shaft is left"), Host->GetGroups().Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCombatProjectileRecoveryCharacterGroupTest,
	"SurvivalRpg.Combat.Projectile.Recovery.AICharacterGroupsBeyondVisibleMaximum",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCombatProjectileRecoveryCharacterGroupTest::RunTest(const FString& Parameters)
{
	using namespace RpgCombatProjectileTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	URpgEmbeddedProjectileHostComponent* Host = AddCharacterRecovery(Fixture.Target.Pawn);
	URpgInventoryManagerComponent* Corpse = Fixture.Target.Pawn->FindComponentByClass<URpgInventoryManagerComponent>();
	const FVector Direction(1.0f, 0.0f, 0.0f);
	for (int32 Index = 0; Index < 5; ++Index)
	{
		ARpgCombatProjectile* Projectile = Fixture.LaunchRecoverable(FVector(-900.0f, 0.0f, 0.0f), Direction, 0.0f, 1.0f,
			ARpgProjectileRecoveryAutomationProjectile::StaticClass(), URpgProjectileRecoveryAutomationArrowDefinition::StaticClass(),
			[](FRpgProjectileRecoveryDefinition& Recovery) { Recovery.MaxVisibleArrowsPerCharacter = 2; });
		if (Projectile)
		{
			Projectile->ResolveImpactForTests(Fixture.MakeTargetHit(FVector(-40.0f, 0.0f, float(Index) * 10.0f), Direction));
		}
	}
	TestEqual(TEXT("Only the visible maximum of shafts shows"), Host->GetGroups().Num(), 2);
	TestEqual(TEXT("The shafts stand for every arrow"), CountGroupArrows(Host), 5);
	TestEqual(TEXT("Every arrow keeps its own record"), Host->GetPendingCount(), 5);
	Fixture.Target.Pawn->FindComponentByClass<URpgLootSourceComponent>()->PopulateLoot();
	TestEqual(TEXT("Grouping loses no arrow in the corpse loot"),
		Corpse->GetTotalItemCountByDefinition(URpgProjectileRecoveryAutomationArrowDefinition::StaticClass()), 5);
	Corpse->ConsumeItemsByDefinition(URpgProjectileRecoveryAutomationArrowDefinition::StaticClass(), 4);
	TestEqual(TEXT("A group keeps its shaft while it stands for an arrow"), CountGroupArrows(Host), 1);
	TestEqual(TEXT("One shaft is left"), Host->GetGroups().Num(), 1);

	// Beyond the maximum an arrow joins a shaft on its own bone, even when another bone's shaft is nearer. The mesh has no
	// asset, so its named bones resolve to the component, and the engine says so.
	AddExpectedError(TEXT("No SkeletalMesh for Component"), EAutomationExpectedErrorFlags::Contains, 0);
	const FCombatant Other = Fixture.SpawnCombatant(FVector(0.0f, 3000.0f, 0.0f), FRotator::ZeroRotator, 2);
	URpgEmbeddedProjectileHostComponent* OtherHost = AddCharacterRecovery(Other.Pawn);
	USkeletalMeshComponent* OtherMesh = Other.Pawn->FindComponentByClass<USkeletalMeshComponent>();
	FRpgProjectileRecoveryDefinition TwoVisible;
	TwoVisible.MaxVisibleArrowsPerCharacter = 2;
	const FName Chest(TEXT("spine_03"));
	const FName Thigh(TEXT("thigh_l"));
	const FVector ChestPoint(-40.0f, 3000.0f, 50.0f);
	const auto Lodge = [&](const FVector& Point, const FName Bone)
	{
		FHitResult Hit(Other.Pawn, OtherMesh, Point, -Direction);
		Hit.BoneName = Bone;
		return OtherHost->RecoverProjectile(Hit, Direction, URpgProjectileRecoveryAutomationArrowDefinition::StaticClass(), TwoVisible);
	};
	TestTrue(TEXT("An arrow lodges in the chest"), Lodge(ChestPoint, Chest) != 0);
	TestTrue(TEXT("An arrow lodges in the thigh"), Lodge(FVector(-40.0f, 3000.0f, -50.0f), Thigh) != 0);
	TestTrue(TEXT("A third arrow at the chest shaft hits the thigh bone"), Lodge(ChestPoint, Thigh) != 0);
	const FRpgEmbeddedProjectileGroup* ChestGroup = OtherHost->GetGroups().FindByPredicate([&](const FRpgEmbeddedProjectileGroup& Group) { return Group.Bone == Chest; });
	const FRpgEmbeddedProjectileGroup* ThighGroup = OtherHost->GetGroups().FindByPredicate([&](const FRpgEmbeddedProjectileGroup& Group) { return Group.Bone == Thigh; });
	TestEqual(TEXT("Still two shafts"), OtherHost->GetGroups().Num(), 2);
	TestTrue(TEXT("The third arrow joins the thigh's shaft, not the nearer chest shaft"),
		ChestGroup && ThighGroup && ChestGroup->Count == 1 && ThighGroup->Count == 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCombatProjectileRecoveryCorpseTest,
	"SurvivalRpg.Combat.Projectile.Recovery.CorpseTakesArrowsUntilItCompletes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCombatProjectileRecoveryCorpseTest::RunTest(const FString& Parameters)
{
	using namespace RpgCombatProjectileTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;

	// A corpse: a pawn whose ability system is already gone, with the corpse lifecycle of an AI character.
	APawn* Corpse = Fixture.World->SpawnActor<APawn>(APawn::StaticClass(), FTransform(FVector(0.0f, 2000.0f, 0.0f)));
	UBoxComponent* Body = NewObject<UBoxComponent>(Corpse, NAME_None, RF_Transient);
	Body->InitBoxExtent(FVector(40.0f));
	Corpse->SetRootComponent(Body);
	Corpse->AddInstanceComponent(Body);
	Body->RegisterComponent();
	URpgCorpseLifecycleComponent* Lifecycle = NewObject<URpgCorpseLifecycleComponent>(Corpse, NAME_None, RF_Transient);
	Lifecycle->SetupAttachment(Body);
	Corpse->AddInstanceComponent(Lifecycle);
	Lifecycle->RegisterComponent();
	URpgEmbeddedProjectileHostComponent* Host = AddCharacterRecovery(Corpse);
	URpgInventoryManagerComponent* Inventory = Corpse->FindComponentByClass<URpgInventoryManagerComponent>();
	const TSubclassOf<URpgInventoryItemDefinition> Arrow = URpgProjectileRecoveryAutomationArrowDefinition::StaticClass();
	const FVector Direction(1.0f, 0.0f, 0.0f);
	const auto Shoot = [&]()
	{
		ARpgCombatProjectile* Projectile = Fixture.LaunchRecoverable(FVector(-900.0f, 2000.0f, 0.0f), Direction, 0.0f, 1.0f);
		if (Projectile)
		{
			Projectile->ResolveImpactForTests(FFixture::MakeBlockHit(Body, FVector(-40.0f, 2000.0f, 0.0f), Direction));
		}
		return Projectile ? Projectile->GetRecoveryOutcome() : ERpgProjectileRecoveryOutcome::None;
	};

	Lifecycle->NotifyDeathStarted(FVector::ZeroVector);
	TestEqual(TEXT("A dying body keeps the arrow that hits it"), Shoot(), ERpgProjectileRecoveryOutcome::Embedded);
	TestEqual(TEXT("It waits for the corpse loot"), Host->GetPendingCount(), 1);
	Corpse->FindComponentByClass<URpgLootSourceComponent>()->PopulateLoot();
	TestEqual(TEXT("The corpse loot carries it"), Inventory->GetTotalItemCountByDefinition(Arrow), 1);

	// Emptied, the corpse completes and takes no further credits.
	Inventory->ConsumeItemsByDefinition(Arrow, 1);
	Inventory->ConsumeItemsByDefinition(URpgInventoryAutomationTestUnitItemDefinition::StaticClass(), 1);
	Lifecycle->NotifyDeathFinished();
	Lifecycle->SetInventoryRequirementComplete(true);
	Lifecycle->HandleSettleElapsedForTesting();
	if (!TestTrue(TEXT("The emptied corpse completes"), Lifecycle->GetLifecycleState() >= ERpgCorpseLifecycleState::Completed)) return false;
	TestFalse(TEXT("A completed corpse takes no arrow"), Host->CanRecover());
	TestEqual(TEXT("An arrow into a completed corpse is gone"), Shoot(), ERpgProjectileRecoveryOutcome::None);
	TestEqual(TEXT("It adds no loot"), Inventory->GetTotalItemCountByDefinition(Arrow), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCombatProjectileRecoverySaveTest,
	"SurvivalRpg.Combat.Projectile.Recovery.PickupsStayOutOfWorldSaves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCombatProjectileRecoverySaveTest::RunTest(const FString& Parameters)
{
	using namespace RpgCombatProjectileTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	ARpgGameModeBase* Mode = Fixture.World->SpawnActor<ARpgGameModeBase>();
	ARpgInventoryContainerActor* Chest = Fixture.World->SpawnActor<ARpgInventoryContainerActor>();
	if (!TestTrue(TEXT("A world-save owner and an ordinary chest exist"), Mode && Chest)) return false;
	Mode->bEnableDiskPersistence = false;
	// The shipped arrow's pickup Blueprint, so a component override in it cannot slip into world saves unnoticed.
	UBoxComponent* Wall = Fixture.SpawnBlock(FVector(500.0f, 6000.0f, 0.0f), FVector(10.0f, 100.0f, 100.0f), EComponentMobility::Static);
	ARpgEmbeddedProjectilePickup* Pickup = RecoverInto(Fixture, Wall, FVector(490.0f, 6000.0f, 0.0f),
		[](FRpgProjectileRecoveryDefinition& Recovery) { UseArrowContent(Recovery); });
	if (!TestTrue(TEXT("An intact arrow leaves a content pickup holding it"), Pickup && !Pickup->GetClass()->IsNative()
		&& Pickup->GetAmmunitionCount() == 1)) return false;

	// BeginPlay decides which containers become persistent; this world never began play, so run it for both. The spawned
	// mode is not the world's game mode, so the chest takes the branch that assigns its id without registering.
	Chest->DispatchBeginPlay();
	Pickup->DispatchBeginPlay();
	const FName ChestId = Chest->GetContainerComponent()->GetPersistentContainerId();
	if (!TestFalse(TEXT("The chest is a persistent world container"), ChestId.IsNone())) return false;
	TestTrue(TEXT("The pickup's container stays without a persistent id"),
		Pickup->FindComponentByClass<URpgInventoryContainerComponent>()->GetPersistentContainerId().IsNone());

	// The real capture path walks every world container; the temporary pickup must not enter the save.
	Mode->WorldContainerSaveDataMap.Reset();
	Mode->CaptureWorldContainers();
	TestEqual(TEXT("The world save captures only the chest"), Mode->WorldContainerSaveDataMap.Num(), 1);
	TestTrue(TEXT("The captured container is the chest"), Mode->WorldContainerSaveDataMap.Contains(ChestId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCombatProjectileVisualHandOffTest,
	"SurvivalRpg.Combat.Projectile.VisualLaunchHandOff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCombatProjectileVisualHandOffTest::RunTest(const FString& Parameters)
{
	using namespace RpgCombatProjectileTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	// The hand-off is an interface event, and actors process events only once their world initialized them.
	Fixture.World->InitializeActorsForPlay(FURL());

	// The shooter's bow shows a nocked arrow beside and above the launch point.
	ARpgProjectileLaunchPresenterAutomationActor* Bow = Fixture.World->SpawnActor<ARpgProjectileLaunchPresenterAutomationActor>();
	if (!TestNotNull(TEXT("The bow exists"), Bow)) return false;
	Bow->AttachToActor(Fixture.Shooter.Pawn, FAttachmentTransformRules::KeepWorldTransform);
	const FVector Origin(-900.0f, 0.0f, 0.0f);
	const FVector Direction(0.0f, 1.0f, 0.0f);
	Bow->VisualStart = Origin + FVector(-40.0f, -60.0f, 30.0f);

	// This world never began play, so each projectile's BeginPlay, where the hand-off happens, runs here.
	ARpgCombatProjectile* Projectile = Fixture.Launch(Origin, Direction);
	if (!TestNotNull(TEXT("The projectile launches"), Projectile)) return false;
	Projectile->DispatchBeginPlay();
	TestEqual(TEXT("The bow hands its arrow over once"), Bow->HandOffCount, 1);
	TestTrue(TEXT("to this projectile"), Bow->LastProjectile.Get() == Projectile);
	TestTrue(TEXT("The collision keeps the launched path"), Projectile->GetActorLocation().Equals(Origin, 0.1f));
	TestTrue(TEXT("The visuals start at the nocked arrow"), Projectile->GetVisualRootForTests()->GetComponentLocation().Equals(Bow->VisualStart, 0.5f));

	// The visuals catch up with the flight within a few frames.
	for (int32 Frame = 0; Frame < 30; ++Frame)
	{
		Projectile->GetProjectileMovement()->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	}
	TestTrue(TEXT("The visuals join the flight"),
		Projectile->GetVisualRootForTests()->GetComponentLocation().Equals(Projectile->GetActorLocation(), 1.0f));

	// Ammunition shown too far away, or none at all, leaves the visuals on the flight.
	Bow->VisualStart = Origin + FVector(0.0f, 0.0f, Projectile->GetVisualHandOffMaxDistanceForTests() + 100.0f);
	ARpgCombatProjectile* Far = Fixture.Launch(Origin, Direction);
	if (Far) Far->DispatchBeginPlay();
	TestTrue(TEXT("A distant nocked arrow is not a visual start"),
		Far && Far->GetVisualRootForTests()->GetComponentLocation().Equals(Origin, 0.1f));
	Bow->bShowsAmmunition = false;
	ARpgCombatProjectile* Plain = Fixture.Launch(Origin, Direction);
	if (Plain) Plain->DispatchBeginPlay();
	TestTrue(TEXT("Without shown ammunition the visuals start at the launch"),
		Plain && Plain->GetVisualRootForTests()->GetComponentLocation().Equals(Origin, 0.1f));

	// A machine that receives the projectile farther along than the limit, such as a late joiner, never takes an arrow.
	Bow->bShowsAmmunition = true;
	Bow->VisualStart = Origin + FVector(-40.0f, -60.0f, 30.0f);
	const int32 HandOffsBefore = Bow->HandOffCount;
	ARpgCombatProjectile* Received = Fixture.Launch(Origin, Direction);
	if (!TestNotNull(TEXT("A later projectile launches"), Received)) return false;
	const FVector ReceivedAt = Origin + Direction * (Received->GetVisualHandOffMaxDistanceForTests() + 50.0f);
	Received->SetActorLocation(ReceivedAt);
	Received->DispatchBeginPlay();
	TestEqual(TEXT("A projectile received beyond the limit from its launch asks no presenter"), Bow->HandOffCount, HandOffsBefore);
	TestTrue(TEXT("and shows where it was received"), Received->GetVisualRootForTests()->GetComponentLocation().Equals(ReceivedAt, 0.1f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCombatProjectileGravityDelayTest,
	"SurvivalRpg.Combat.Projectile.GravityDelay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCombatProjectileGravityDelayTest::RunTest(const FString& Parameters)
{
	using namespace RpgCombatProjectileTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	// Spawned components activate, so the movement simulates, only once the world initialized its actors.
	Fixture.World->InitializeActorsForPlay(FURL());

	// A 4000 cm/s launch along Y, away from the target: straight for 500 cm, then falling.
	FGameplayEffectContextHandle Context = Fixture.Shooter.ASC->MakeEffectContext();
	const FGameplayEffectSpecHandle Spec(new FGameplayEffectSpec(Fixture.DamageEffect.Get(), Context, 1.0f));
	FRpgProjectileLaunchParams Params;
	Params.Origin = FVector(-900.0f, 0.0f, 500.0f);
	Params.Direction = FVector(0.0f, 1.0f, 0.0f);
	Params.Speed = 4000.0f;
	ARpgCombatProjectile* Projectile = ARpgCombatProjectile::LaunchWithDamageSpec(
		Fixture.World, ARpgGravityDelayAutomationProjectile::StaticClass(), Params, Spec, Fixture.Shooter.Pawn);
	if (!TestNotNull(TEXT("The projectile launches"), Projectile)) return false;
	Projectile->DispatchBeginPlay();
	UProjectileMovementComponent* Movement = Projectile->GetProjectileMovement();
	TestEqual(TEXT("It starts without gravity"), Movement->ProjectileGravityScale, 0.0f);

	// Step the flight as the world would: the movement first, then the projectile.
	constexpr float Step = 1.0f / 60.0f;
	float StraightDrop = 0.0f;
	for (int32 Frame = 0; Frame < 60 && Projectile->GetImpactResult() == ERpgProjectileImpactResult::None; ++Frame)
	{
		Movement->TickComponent(Step, LEVELTICK_All, nullptr);
		Projectile->Tick(Step);
		if (FVector::Dist(Projectile->GetActorLocation(), Params.Origin) < 500.0f)
		{
			StraightDrop = FMath::Max(StraightDrop, Params.Origin.Z - Projectile->GetActorLocation().Z);
		}
	}
	TestEqual(TEXT("It keeps its height for the first 500 cm"), StraightDrop, 0.0f, 0.01f);
	TestEqual(TEXT("Past them, the class's gravity applies"), Movement->ProjectileGravityScale, 1.0f);
	TestTrue(TEXT("and it falls"), Projectile->GetActorLocation().Z < Params.Origin.Z - 50.0f);

	// A machine that receives the projectile already past the delay applies gravity at once.
	ARpgCombatProjectile* Late = ARpgCombatProjectile::LaunchWithDamageSpec(
		Fixture.World, ARpgGravityDelayAutomationProjectile::StaticClass(), Params, Spec, Fixture.Shooter.Pawn);
	if (!TestNotNull(TEXT("A second projectile launches"), Late)) return false;
	Late->SetActorLocation(Params.Origin + FVector(0.0f, 800.0f, 0.0f));
	Late->DispatchBeginPlay();
	TestEqual(TEXT("A projectile received past the delay falls at once"), Late->GetProjectileMovement()->ProjectileGravityScale, 1.0f);
	return true;
}

#endif
