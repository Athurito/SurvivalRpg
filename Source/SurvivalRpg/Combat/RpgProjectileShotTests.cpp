#if WITH_DEV_AUTOMATION_TESTS

#include "RpgProjectileShot.h"

#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgStaminaSet.h"
#include "SurvivalRpg/AbilitySystem/Effects/RpgAbilityTuningEffectComponent.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Tasks/RpgAbilityTask_ProjectileShot.h"
#include "SurvivalRpg/Combat/RpgCombatProjectile.h"
#include "SurvivalRpg/Combat/RpgProjectileShotAutomationTestTypes.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Physics/RpgCollisionChannels.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeDefinition.h"
#include "UObject/StrongObjectPtr.h"

namespace RpgProjectileShotTests
{
	constexpr float StartingStamina = 100.0f;
	constexpr float ReleaseCost = 10.0f;
	constexpr int32 StartingArrows = 5;
	const FVector ShooterLocation(0.0f, 0.0f, 100.0f);

	FRpgProjectileShotDefinition MakeDefinition(TSubclassOf<URpgInventoryItemDefinition> Arrow = URpgProjectileShotAutomationArrowDefinition::StaticClass())
	{
		FRpgProjectileShotDefinition Definition;
		Definition.AmmunitionItemDefinition = Arrow;
		Definition.MinimumNockSeconds = 0.25f;
		Definition.FullDrawSeconds = 1.0f;
		const auto AddLinearKeys = [](FRuntimeFloatCurve& Curve, float AtZero, float AtOne)
		{
			FRichCurve* RichCurve = Curve.GetRichCurve();
			RichCurve->SetKeyInterpMode(RichCurve->AddKey(0.0f, AtZero), RCIM_Linear);
			RichCurve->SetKeyInterpMode(RichCurve->AddKey(1.0f, AtOne), RCIM_Linear);
		};
		AddLinearKeys(Definition.DamageByDrawFraction, 10.0f, 30.0f);
		AddLinearKeys(Definition.LaunchSpeedByDrawFraction, 2000.0f, 6000.0f);
		Definition.StaggerDamage = 20.0f;
		Definition.ReleaseStaminaCost = ReleaseCost;
		Definition.StaminaChangeEffect = URpgProjectileShotAutomationStaminaEffect::StaticClass();
		return Definition;
	}

	/**
	 * A begun-play standalone world with a shooter: a player state that owns the ability system, stamina and inventory,
	 * and a pawn avatar facing +X. Its shot ability is the automation ability; world time advances by hand.
	 */
	class FFixture
	{
	public:
		FFixture()
		{
			GameInstance.Reset(NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient));
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}

		~FFixture()
		{
			if (World && World->HasBegunPlay())
			{
				World->EndPlay(EEndPlayReason::Quit);
			}
			GameInstance->Shutdown();
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}

		bool Initialize(FAutomationTestBase& Test, const bool bPlayerStateOwner = true)
		{
			if (!Test.TestNotNull(TEXT("Standalone shot world exists"), World)) return false;
			// Finished spawns begin play, so a projectile that fails in its BeginPlay fails as in a running game.
			World->InitializeActorsForPlay(FURL());
			World->GetWorldSettings()->NotifyBeginPlay();

			// A plain pawn has no root component, so it gets one to stand where the shot starts.
			Pawn = World->SpawnActor<APawn>();
			if (Pawn)
			{
				USceneComponent* Root = NewObject<USceneComponent>(Pawn, NAME_None, RF_Transient);
				Pawn->SetRootComponent(Root);
				Pawn->AddInstanceComponent(Root);
				Root->RegisterComponent();
				Pawn->SetActorLocationAndRotation(ShooterLocation, FRotator::ZeroRotator);
			}
			AActor* Owner = bPlayerStateOwner ? static_cast<AActor*>(World->SpawnActor<APlayerState>()) : Pawn;
			if (!Test.TestTrue(TEXT("Shooter pawn and owner exist"), Pawn && Owner)) return false;

			ASC = NewObject<URpgAbilitySystemComponent>(Owner, NAME_None, RF_Transient);
			Owner->AddInstanceComponent(ASC);
			ASC->RegisterComponent();
			ASC->AddAttributeSetSubobject(NewObject<URpgStaminaSet>(Owner, NAME_None, RF_Transient));
			ASC->InitAbilityActorInfo(Owner, Pawn);
			ASC->SetNumericAttributeBase(URpgStaminaSet::GetMaxStaminaAttribute(), StartingStamina);
			ASC->SetNumericAttributeBase(URpgStaminaSet::GetStaminaAttribute(), StartingStamina);

			Inventory = NewObject<URpgInventoryManagerComponent>(Owner, NAME_None, RF_Transient);
			Owner->AddInstanceComponent(Inventory);
			Inventory->RegisterComponent();
			return Test.TestTrue(TEXT("The fixture can grant abilities"), ASC->HasGrantAuthority());
		}

		bool AddArrows(const int32 Count, TSubclassOf<URpgInventoryItemDefinition> Arrow = URpgProjectileShotAutomationArrowDefinition::StaticClass())
		{
			return Inventory->AddItemDefinition(Arrow, Count) != nullptr;
		}

		URpgProjectileShotAutomationAbility* Grant(const FRpgProjectileShotDefinition& Definition,
			const ERpgProjectileAmmunitionPolicy Policy = ERpgProjectileAmmunitionPolicy::ShooterInventory)
		{
			FGameplayAbilitySpec Spec(URpgProjectileShotAutomationAbility::StaticClass(), 1);
			Spec.GetDynamicSpecSourceTags().AddTag(RpgGameplayTags::InputTag_Weapon_Primary);
			Handle = ASC->GiveAbility(Spec);
			const FGameplayAbilitySpec* Granted = ASC->FindAbilitySpecFromHandle(Handle);
			Ability = Granted ? Cast<URpgProjectileShotAutomationAbility>(Granted->GetPrimaryInstance()) : nullptr;
			if (Ability)
			{
				Ability->ShotDefinition = Definition;
				Ability->AmmunitionPolicy = Policy;
			}
			return Ability;
		}

		bool Activate()
		{
			Ability->ResetObservations();
			return ASC->TryActivateAbility(Handle);
		}

		bool IsActive() const
		{
			const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
			return Spec && Spec->IsActive();
		}

		void Advance(const double Seconds) const { World->TimeSeconds += Seconds; }

		void Release() const
		{
			if (Ability->ShotTask)
			{
				Ability->ShotTask->ReleaseShot();
			}
		}

		float GetStamina() const { return ASC->GetNumericAttribute(URpgStaminaSet::GetStaminaAttribute()); }

		int32 GetArrows(TSubclassOf<URpgInventoryItemDefinition> Arrow = URpgProjectileShotAutomationArrowDefinition::StaticClass()) const
		{
			return Inventory->GetTotalItemCountByDefinition(Arrow);
		}

		int32 CountLiveProjectiles() const
		{
			int32 Count = 0;
			for (TActorIterator<ARpgCombatProjectile> It(World); It; ++It)
			{
				Count += IsValid(*It) && !It->IsActorBeingDestroyed() ? 1 : 0;
			}
			return Count;
		}

		/** Draws for Seconds and releases; returns how the shot ended. */
		ERpgProjectileShotResult Shoot(const float Seconds)
		{
			if (!Activate())
			{
				return Ability->LastResult;
			}
			Advance(Seconds);
			Release();
			return Ability->LastResult;
		}

		TStrongObjectPtr<UGameInstance> GameInstance;
		UWorld* World = nullptr;
		APawn* Pawn = nullptr;
		URpgAbilitySystemComponent* ASC = nullptr;
		URpgInventoryManagerComponent* Inventory = nullptr;
		URpgProjectileShotAutomationAbility* Ability = nullptr;
		FGameplayAbilitySpecHandle Handle;
	};

	float GetSetByCaller(const ARpgCombatProjectile* Projectile, const FGameplayTag Tag)
	{
		const FGameplayEffectSpec* Spec = Projectile ? Projectile->GetDamageSpecForTests().Data.Get() : nullptr;
		return Spec ? Spec->GetSetByCallerMagnitude(Tag, false, -1.0f) : -1.0f;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgProjectileShotNockCancelTest,
	"SurvivalRpg.Combat.ProjectileShot.EarlyReleaseCancelsWithoutCost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgProjectileShotNockCancelTest::RunTest(const FString& Parameters)
{
	using namespace RpgProjectileShotTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this) || !Fixture.AddArrows(StartingArrows) || !Fixture.Grant(MakeDefinition())) return false;

	TestEqual(TEXT("A release before the nocking time cancels"), Fixture.Shoot(0.1f), ERpgProjectileShotResult::EarlyRelease);
	TestFalse(TEXT("The cancelled shot ends its ability"), Fixture.IsActive());
	TestEqual(TEXT("An early release launches nothing"), Fixture.CountLiveProjectiles(), 0);
	TestEqual(TEXT("An early release consumes no arrow"), Fixture.GetArrows(), StartingArrows);
	TestEqual(TEXT("An early release pays no stamina"), Fixture.GetStamina(), StartingStamina);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgProjectileShotDrawTest,
	"SurvivalRpg.Combat.ProjectileShot.DrawCurvesSetDamageAndSpeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgProjectileShotDrawTest::RunTest(const FString& Parameters)
{
	using namespace RpgProjectileShotTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this) || !Fixture.AddArrows(StartingArrows) || !Fixture.Grant(MakeDefinition())) return false;

	// A partial draw, released through the ability's input as a player does.
	if (!TestTrue(TEXT("The shot activates"), Fixture.Activate())) return false;
	Fixture.Advance(0.5f);
	Fixture.ASC->AbilityInputTagReleased(RpgGameplayTags::InputTag_Weapon_Primary);
	Fixture.ASC->ProcessAbilityInput(0.0f, false);
	TestEqual(TEXT("The input release shoots"), Fixture.Ability->LastResult, ERpgProjectileShotResult::Released);
	TestEqual(TEXT("Half the full draw time is half the draw"), Fixture.Ability->LastDrawFraction, 0.5f, 0.001f);
	const ARpgCombatProjectile* Partial = Fixture.Ability->LastProjectile.Get();
	if (!TestNotNull(TEXT("The server reports the launched projectile"), Partial)) return false;
	TestEqual(TEXT("The partial draw's speed comes from the curve"), Partial->GetProjectileMovement()->Velocity.Size(), 4000.0, 1.0);
	TestEqual(TEXT("The partial draw's damage comes from the curve"), GetSetByCaller(Partial, RpgGameplayTags::SetByCaller_Damage), 20.0f, 0.01f);
	TestEqual(TEXT("The partial draw scales the stagger damage"), GetSetByCaller(Partial, RpgGameplayTags::SetByCaller_StaggerDamage), 10.0f, 0.01f);
	TestEqual(TEXT("The shot carries its shot identity"), Partial->GetShotId() != 0, true);
	TestEqual(TEXT("One shot consumes one arrow"), Fixture.GetArrows(), StartingArrows - 1);
	TestEqual(TEXT("One shot pays the release cost once"), Fixture.GetStamina(), StartingStamina - ReleaseCost);

	// Drawing beyond the full draw time stays at full draw.
	TestEqual(TEXT("A long draw shoots"), Fixture.Shoot(3.0f), ERpgProjectileShotResult::Released);
	TestEqual(TEXT("A long draw is a full draw"), Fixture.Ability->LastDrawFraction, 1.0f, 0.001f);
	TestEqual(TEXT("A full draw shoots at the curve's top speed"),
		Fixture.Ability->LastProjectile.IsValid() ? Fixture.Ability->LastProjectile->GetProjectileMovement()->Velocity.Size() : 0.0, 6000.0, 1.0);
	TestTrue(TEXT("The projectile leaves from the muzzle in front of the shooter"),
		Fixture.Ability->LastProjectile.IsValid()
		&& Fixture.Ability->LastProjectile->GetLaunchOrigin().Equals(ShooterLocation + FVector(60.0f, 0.0f, 50.0f), 1.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgProjectileShotAmmunitionTest,
	"SurvivalRpg.Combat.ProjectileShot.AmmunitionIsCheckedAtDrawAndRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgProjectileShotAmmunitionTest::RunTest(const FString& Parameters)
{
	using namespace RpgProjectileShotTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this) || !Fixture.AddArrows(1) || !Fixture.Grant(MakeDefinition())) return false;

	TestEqual(TEXT("The last arrow shoots"), Fixture.Shoot(1.0f), ERpgProjectileShotResult::Released);
	TestEqual(TEXT("The last arrow is gone"), Fixture.GetArrows(), 0);
	TestEqual(TEXT("Without arrows the bow does not draw"), Fixture.Shoot(1.0f), ERpgProjectileShotResult::NoAmmunition);
	TestEqual(TEXT("A refused draw pays nothing"), Fixture.GetStamina(), StartingStamina - ReleaseCost);

	// The arrow leaves the inventory while the bow is drawn.
	Fixture.AddArrows(1);
	if (!TestTrue(TEXT("A new arrow draws"), Fixture.Activate())) return false;
	Fixture.Advance(1.0f);
	Fixture.Inventory->ConsumeItemsByDefinition(URpgProjectileShotAutomationArrowDefinition::StaticClass(), 1);
	Fixture.Release();
	TestEqual(TEXT("An arrow removed during the draw cancels the release"), Fixture.Ability->LastResult, ERpgProjectileShotResult::NoAmmunition);
	TestEqual(TEXT("That release pays no stamina"), Fixture.GetStamina(), StartingStamina - ReleaseCost);
	TestEqual(TEXT("Only the first release launched a projectile"), Fixture.CountLiveProjectiles(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgProjectileShotStaminaTest,
	"SurvivalRpg.Combat.ProjectileShot.InsufficientStaminaPaysNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgProjectileShotStaminaTest::RunTest(const FString& Parameters)
{
	using namespace RpgProjectileShotTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this) || !Fixture.AddArrows(StartingArrows) || !Fixture.Grant(MakeDefinition())) return false;

	Fixture.ASC->SetNumericAttributeBase(URpgStaminaSet::GetStaminaAttribute(), ReleaseCost - 1.0f);
	TestEqual(TEXT("Too little stamina cancels the release"), Fixture.Shoot(1.0f), ERpgProjectileShotResult::InsufficientStamina);
	TestEqual(TEXT("It launches nothing"), Fixture.CountLiveProjectiles(), 0);
	TestEqual(TEXT("It consumes no arrow"), Fixture.GetArrows(), StartingArrows);
	TestEqual(TEXT("It pays no stamina"), Fixture.GetStamina(), ReleaseCost - 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgProjectileShotDuplicateReleaseTest,
	"SurvivalRpg.Combat.ProjectileShot.DuplicateReleaseShootsOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgProjectileShotDuplicateReleaseTest::RunTest(const FString& Parameters)
{
	using namespace RpgProjectileShotTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this) || !Fixture.AddArrows(StartingArrows) || !Fixture.Grant(MakeDefinition())) return false;

	if (!TestTrue(TEXT("The shot activates"), Fixture.Activate())) return false;
	URpgAbilityTask_ProjectileShot* Task = Fixture.Ability->ShotTask;
	Fixture.Advance(1.0f);
	FRpgProjectileShotRelease Release;
	Release.DrawSeconds = 1.0f;
	Release.AimOrigin = ShooterLocation;
	Release.AimDirection = FVector::ForwardVector;
	Release.ShotId = 77;
	Task->ReceiveReleaseForTests(Release);
	Task->ReceiveReleaseForTests(Release);
	Fixture.Release();

	TestEqual(TEXT("The shot releases once"), Fixture.Ability->ReleasedCount, 1);
	TestEqual(TEXT("Repeated release messages launch one projectile"), Fixture.CountLiveProjectiles(), 1);
	TestEqual(TEXT("They consume one arrow"), Fixture.GetArrows(), StartingArrows - 1);
	TestEqual(TEXT("They pay the release cost once"), Fixture.GetStamina(), StartingStamina - ReleaseCost);
	TestEqual(TEXT("The projectile keeps the owner's shot identity"),
		Fixture.Ability->LastProjectile.IsValid() ? Fixture.Ability->LastProjectile->GetShotId() : 0, 77);

	// The server never counts more draw than it observed.
	if (!TestTrue(TEXT("The next shot activates"), Fixture.Activate())) return false;
	Fixture.Advance(0.3f);
	Release.DrawSeconds = 5.0f;
	Fixture.Ability->ShotTask->ReceiveReleaseForTests(Release);
	TestEqual(TEXT("An overstated draw is clamped to the observed draw and its tolerance"), Fixture.Ability->LastDrawFraction,
		0.3f + RpgProjectileShot::DrawTimeTolerance, 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgProjectileShotSpawnFailureTest,
	"SurvivalRpg.Combat.ProjectileShot.SpawnFailureRefundsEverything",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgProjectileShotSpawnFailureTest::RunTest(const FString& Parameters)
{
	using namespace RpgProjectileShotTests;
	FFixture Fixture;
	const TSubclassOf<URpgInventoryItemDefinition> FailingArrow = URpgProjectileShotAutomationFailingArrowDefinition::StaticClass();
	if (!Fixture.Initialize(*this) || !Fixture.AddArrows(StartingArrows, FailingArrow) || !Fixture.Grant(MakeDefinition(FailingArrow))) return false;

	TestEqual(TEXT("A projectile that fails to spawn cancels the release"), Fixture.Shoot(1.0f), ERpgProjectileShotResult::SpawnFailed);
	TestEqual(TEXT("No projectile is left"), Fixture.CountLiveProjectiles(), 0);
	TestEqual(TEXT("The consumed arrow is given back"), Fixture.GetArrows(FailingArrow), StartingArrows);
	TestEqual(TEXT("The paid stamina is given back"), Fixture.GetStamina(), StartingStamina);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgProjectileShotTuningTest,
	"SurvivalRpg.Combat.ProjectileShot.TuningIsFixedPerActivation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgProjectileShotTuningTest::RunTest(const FString& Parameters)
{
	using namespace RpgProjectileShotTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this) || !Fixture.AddArrows(StartingArrows) || !Fixture.Grant(MakeDefinition())) return false;

	// A passive that halves the release cost and the draw time, gained while the bow is drawn.
	FRpgSkillTreeAbilityTuning HalfCost;
	HalfCost.TuningTag = RpgGameplayTags::Ability_Tuning_Ranged_ReleaseCost;
	HalfCost.Operation = ERpgSkillTreeTuningOperation::Multiply;
	HalfCost.Value = 0.5f;
	FRpgSkillTreeAbilityTuning HalfDraw = HalfCost;
	HalfDraw.TuningTag = RpgGameplayTags::Ability_Tuning_Ranged_DrawTime;
	TStrongObjectPtr<UGameplayEffect> Passive(URpgAbilityTuningEffectComponent::MakeTransientEffectForTests({ HalfCost, HalfDraw }, 1000.0f));

	if (!TestTrue(TEXT("The shot activates"), Fixture.Activate())) return false;
	Fixture.ASC->ApplyGameplayEffectToSelf(Passive.Get(), 1.0f, Fixture.ASC->MakeEffectContext());
	Fixture.Advance(0.5f);
	Fixture.Release();
	TestEqual(TEXT("A passive gained during the draw keeps this shot's cost"), Fixture.GetStamina(), StartingStamina - ReleaseCost);
	TestEqual(TEXT("A passive gained during the draw keeps this shot's draw time"), Fixture.Ability->LastDrawFraction, 0.5f, 0.001f);

	TestEqual(TEXT("The next shot releases"), Fixture.Shoot(0.5f), ERpgProjectileShotResult::Released);
	TestEqual(TEXT("The next shot pays the passive's cost"), Fixture.GetStamina(), StartingStamina - ReleaseCost * 1.5f);
	TestEqual(TEXT("The next shot draws in the passive's time"), Fixture.Ability->LastDrawFraction, 1.0f, 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgProjectileShotNestedBagTest,
	"SurvivalRpg.Combat.ProjectileShot.ArrowsInACarriedBagShoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgProjectileShotNestedBagTest::RunTest(const FString& Parameters)
{
	using namespace RpgProjectileShotTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this) || !Fixture.Grant(MakeDefinition())) return false;

	URpgInventoryItemInstance* Bag = Fixture.Inventory->AddItemDefinition(URpgInventoryAutomationTestBagItemDefinition::StaticClass(), 1);
	if (!TestNotNull(TEXT("A carried bag exists"), Bag)) return false;
	FRpgInventoryGridPlacement InBag;
	InBag.SetContainerHandle(FRpgInventoryContainerHandle::MakeItemOwned(Bag->GetItemId(), TEXT("Main"), 1));
	InBag.X = 0;
	InBag.Y = 0;
	if (!TestNotNull(TEXT("Arrows lie only in the bag"), Fixture.Inventory->AddItemDefinitionToPlacement(
		URpgProjectileShotAutomationArrowDefinition::StaticClass(), 2, InBag))) return false;

	TestEqual(TEXT("Arrows in the bag count"), Fixture.GetArrows(), 2);
	TestEqual(TEXT("Arrows in the bag shoot"), Fixture.Shoot(1.0f), ERpgProjectileShotResult::Released);
	TestEqual(TEXT("The shot consumed an arrow from the bag"), Fixture.GetArrows(), 1);
	TestTrue(TEXT("The bag itself stays"), Fixture.Inventory->ContainsItemInstance(Bag));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgProjectileShotObstructionTest,
	"SurvivalRpg.Combat.ProjectileShot.LaunchObstructionStopsTheShotAtTheWall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgProjectileShotObstructionTest::RunTest(const FString& Parameters)
{
	using namespace RpgProjectileShotTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this) || !Fixture.AddArrows(StartingArrows) || !Fixture.Grant(MakeDefinition())) return false;

	// A wall between the shooter's body and its muzzle; the camera itself sees past it.
	AActor* Wall = Fixture.World->SpawnActor<AActor>();
	USphereComponent* WallBody = NewObject<USphereComponent>(Wall, NAME_None, RF_Transient);
	WallBody->InitSphereRadius(10.0f);
	WallBody->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	WallBody->SetCollisionResponseToAllChannels(ECR_Block);
	Wall->SetRootComponent(WallBody);
	Wall->AddInstanceComponent(WallBody);
	WallBody->RegisterComponent();
	const FVector WallCenter = ShooterLocation + FVector(35.0f, 0.0f, 50.0f);
	Wall->SetActorLocation(WallCenter);

	TestEqual(TEXT("An obstructed shot still releases"), Fixture.Shoot(1.0f), ERpgProjectileShotResult::Released);
	const ARpgCombatProjectile* Projectile = Fixture.Ability->LastProjectile.Get();
	if (!TestNotNull(TEXT("The obstructed shot launched"), Projectile)) return false;
	TestTrue(TEXT("It launches on the shooter's side of the wall"), Projectile->GetLaunchOrigin().X < WallCenter.X - 10.0f + 0.5f);
	TestTrue(TEXT("It flies into the wall"), (WallCenter - Projectile->GetLaunchOrigin()).GetSafeNormal() .Dot(
		Projectile->GetProjectileMovement()->Velocity.GetSafeNormal()) > 0.5f);

	FRpgProjectileShotGeometry Geometry;
	TestFalse(TEXT("A camera far from the shooter is refused"), RpgProjectileShot::ResolveGeometry(
		MakeDefinition(), Fixture.Pawn, ShooterLocation + FVector(RpgProjectileShot::MaxAimOriginDistance + 100.0f, 0.0f, 0.0f),
		FVector::ForwardVector, Geometry));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgProjectileShotHoldDrainTest,
	"SurvivalRpg.Combat.ProjectileShot.HoldDrainExhaustionLowersTheBow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgProjectileShotHoldDrainTest::RunTest(const FString& Parameters)
{
	using namespace RpgProjectileShotTests;
	FFixture Fixture;
	FRpgProjectileShotDefinition Definition = MakeDefinition();
	Definition.MinimumNockSeconds = 0.0f;
	Definition.HoldStaminaPerSecond = 40.0f;
	if (!Fixture.Initialize(*this) || !Fixture.AddArrows(StartingArrows) || !Fixture.Grant(Definition)) return false;

	if (!TestTrue(TEXT("The shot activates"), Fixture.Activate())) return false;
	URpgAbilityTask_ProjectileShot* Task = Fixture.Ability->ShotTask;
	Task->TickTask(1.0f);
	TestEqual(TEXT("Holding drains stamina"), Fixture.GetStamina(), StartingStamina - 40.0f, 0.01f);
	Task->TickTask(1.0f);
	Task->TickTask(1.0f);
	TestEqual(TEXT("Exhaustion lowers the bow"), Fixture.Ability->LastResult, ERpgProjectileShotResult::Exhausted);
	TestEqual(TEXT("It never fires on its own"), Fixture.CountLiveProjectiles(), 0);
	TestEqual(TEXT("It consumes no arrow"), Fixture.GetArrows(), StartingArrows);
	TestEqual(TEXT("The paid drain stays paid"), Fixture.GetStamina(), 0.0f, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgProjectileShotAmmunitionPolicyTest,
	"SurvivalRpg.Combat.ProjectileShot.UnlimitedAmmunitionIsAnExplicitPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgProjectileShotAmmunitionPolicyTest::RunTest(const FString& Parameters)
{
	using namespace RpgProjectileShotTests;
	{
		// An NPC-like shooter: its ability system lives on the pawn, whose inventory is no quiver.
		FFixture Fixture;
		if (!Fixture.Initialize(*this, false) || !Fixture.AddArrows(StartingArrows) || !Fixture.Grant(MakeDefinition())) return false;
		TestEqual(TEXT("A shooter without a player inventory has no ammunition"), Fixture.Shoot(1.0f), ERpgProjectileShotResult::NoAmmunition);
		TestEqual(TEXT("Its pawn's items are never taken as arrows"), Fixture.GetArrows(), StartingArrows);
	}
	{
		FFixture Fixture;
		if (!Fixture.Initialize(*this, false) || !Fixture.Grant(MakeDefinition(), ERpgProjectileAmmunitionPolicy::Unlimited)) return false;
		TestEqual(TEXT("The unlimited policy shoots without items"), Fixture.Shoot(1.0f), ERpgProjectileShotResult::Released);
		TestEqual(TEXT("It still pays the release cost"), Fixture.GetStamina(), StartingStamina - ReleaseCost);
	}
	return true;
}

#endif
