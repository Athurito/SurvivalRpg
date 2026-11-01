#if WITH_DEV_AUTOMATION_TESTS

#include "RpgCombatProjectile.h"

#include "AIController.h"
#include "Components/SphereComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Misc/AutomationTest.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgCombatSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgDefenseSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgStaminaSet.h"
#include "SurvivalRpg/AbilitySystem/Executions/RpgDamageExecution.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Combat/RpgCombatImpact.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
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
	return true;
}

#endif
