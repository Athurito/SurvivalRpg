#if WITH_DEV_AUTOMATION_TESTS

#include "RpgCombatProjectile.h"

#include "AbilitySystemGlobals.h"
#include "AIController.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
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
#include "SurvivalRpg/Combat/RpgEmbeddedProjectilePickup.h"
#include "SurvivalRpg/Combat/RpgEmbeddedProjectileSubsystem.h"
#include "SurvivalRpg/Combat/RpgProjectileShotAutomationTestTypes.h"
#include "SurvivalRpg/Core/Game/RpgGameStateBase.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Interaction/IInteractableTarget.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
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
	const FHitResult Hit = FFixture::MakeBlockHit(Wall, ImpactPoint, Direction);
	Projectile->ResolveImpactForTests(Hit);
	Projectile->ResolveImpactForTests(Hit);

	ARpgEmbeddedProjectilePickup* Pickup = Projectile->GetRecoveredPickup();
	if (!TestNotNull(TEXT("An intact arrow leaves a pickup"), Pickup)) return false;
	TestEqual(TEXT("A second impact report recovers nothing more"), Fixture.GetRecovery()->GetPickupCount(), 1);
	TestEqual(TEXT("The impact resolves as embedded"), Projectile->GetRecoveryOutcome(), ERpgProjectileRecoveryOutcome::Embedded);
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

	UBoxComponent* Floor = Fixture.SpawnBlock(FVector(0.0f, 0.0f, -100.0f), FVector(3000.0f, 3000.0f, 50.0f), EComponentMobility::Static);
	const FVector Direction(1.0f, 0.0f, 0.0f);
	const auto ShootTarget = [&Fixture, &Direction]()
	{
		ARpgCombatProjectile* Projectile = Fixture.LaunchRecoverable(FVector(-900.0f, 0.0f, 0.0f), Direction, 0.0f, 1.0f);
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
	const auto Shoot = [&](TSubclassOf<ARpgCombatProjectile> Class, TSubclassOf<URpgInventoryItemDefinition> RecoveryItem)
	{
		// A certain break: any break roll would report Broken.
		ARpgCombatProjectile* Projectile = Fixture.LaunchRecoverable(FVector(100.0f, 0.0f, 0.0f), Direction, 1.0f, 0.0f, Class, RecoveryItem);
		if (Projectile)
		{
			Projectile->ResolveImpactForTests(FFixture::MakeBlockHit(Wall, FVector(490.0f, 0.0f, 0.0f), Direction));
		}
		return Projectile ? Projectile->GetRecoveryOutcome() : ERpgProjectileRecoveryOutcome::Broken;
	};

	TestEqual(TEXT("A projectile consumed on impact never rolls"),
		Shoot(ARpgProjectileShotAutomationTestProjectile::StaticClass(), URpgProjectileRecoveryAutomationArrowDefinition::StaticClass()),
		ERpgProjectileRecoveryOutcome::None);
	TestEqual(TEXT("An arrow nobody paid for never rolls"),
		Shoot(ARpgProjectileRecoveryAutomationProjectile::StaticClass(), nullptr), ERpgProjectileRecoveryOutcome::None);

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

#endif
