#if WITH_DEV_AUTOMATION_TESTS

#include "RpgProjectileShot.h"

#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameplayTask.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgStaminaSet.h"
#include "SurvivalRpg/AbilitySystem/Effects/RpgAbilityTuningEffectComponent.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Tasks/RpgAbilityTask_ProjectileShot.h"
#include "SurvivalRpg/Combat/RpgCombatProjectile.h"
#include "SurvivalRpg/Combat/RpgEmbeddedProjectilePickup.h"
#include "SurvivalRpg/Combat/RpgProjectileShotAutomationTestTypes.h"
#include "SurvivalRpg/Equipment/RpgRangedWeaponInstance.h"
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

		bool Initialize(FAutomationTestBase& Test, const bool bPlayerStateOwner = true, const bool bCharacterShooter = false)
		{
			if (!Test.TestNotNull(TEXT("Standalone shot world exists"), World)) return false;
			// Finished spawns begin play, so a projectile that fails in its BeginPlay fails as in a running game.
			World->InitializeActorsForPlay(FURL());
			World->GetWorldSettings()->NotifyBeginPlay();

			// A character brings the movement an aim stance reads; flying keeps it free of floor geometry and grounded for
			// the stance. A plain pawn has no root component, so it gets one to stand where the shot starts.
			if (bCharacterShooter)
			{
				ACharacter* Character = World->SpawnActor<ACharacter>();
				Pawn = Character;
				if (Character)
				{
					Character->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
					Character->SetActorLocationAndRotation(ShooterLocation, FRotator::ZeroRotator);
				}
			}
			else
			{
				Pawn = World->SpawnActor<APawn>();
				if (Pawn)
				{
					USceneComponent* Root = NewObject<USceneComponent>(Pawn, NAME_None, RF_Transient);
					Pawn->SetRootComponent(Root);
					Pawn->AddInstanceComponent(Root);
					Root->RegisterComponent();
					Pawn->SetActorLocationAndRotation(ShooterLocation, FRotator::ZeroRotator);
				}
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

		/** Runs every active ticking ability task once, as the ability system ticks them each frame. */
		void TickTasks(const float DeltaSeconds = 1.0f / 60.0f) const
		{
			TArray<UGameplayTask*> Tasks;
			for (auto It = ASC->GetTickingTaskIterator(); It; ++It)
			{
				Tasks.Add(*It);
			}
			for (UGameplayTask* Task : Tasks)
			{
				if (IsValid(Task) && Task->IsActive())
				{
					Task->TickTask(DeltaSeconds);
				}
			}
		}

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
	FRpgProjectileShotPresentationTagsTest,
	"SurvivalRpg.Combat.ProjectileShot.PresentationTagsFollowTheShot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgProjectileShotPresentationTagsTest::RunTest(const FString& Parameters)
{
	using namespace RpgProjectileShotTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this) || !Fixture.AddArrows(StartingArrows) || !Fixture.Grant(MakeDefinition())) return false;
	const auto HasTag = [&Fixture](const FGameplayTag Tag) { return Fixture.ASC->HasMatchingGameplayTag(Tag); };

	// The drawn shot shows as drawing on every machine; a cancel ends it without a release.
	if (!TestTrue(TEXT("The shot activates"), Fixture.Activate())) return false;
	TestTrue(TEXT("A drawn shot is drawing"), HasTag(RpgGameplayTags::State_Ranged_Drawing));
	TestTrue(TEXT("The owner presents its own draw"), Fixture.ASC->IsRangedDrawingForPresentation());
	Fixture.Advance(0.1f);
	float DrawProgress = -1.0f;
	float NockProgress = -1.0f;
	TestTrue(TEXT("The owner knows how far it drew"), Fixture.ASC->GetLocalRangedDrawProgress(DrawProgress, NockProgress));
	TestEqual(TEXT("The draw progress follows the drawn time over the full draw"), DrawProgress, 0.1f, 0.001f);
	TestEqual(TEXT("The nock lies at its share of the full draw"), NockProgress, 0.25f, 0.001f);
	Fixture.Release();
	TestFalse(TEXT("An early release stops drawing"), HasTag(RpgGameplayTags::State_Ranged_Drawing));
	TestFalse(TEXT("An early release shows no release"), HasTag(RpgGameplayTags::State_Ranged_Releasing));

	// A release stops drawing and shows the release for its presentation time.
	TestEqual(TEXT("A full draw releases"), Fixture.Shoot(1.0f), ERpgProjectileShotResult::Released);
	TestFalse(TEXT("The released shot stops drawing"), HasTag(RpgGameplayTags::State_Ranged_Drawing));
	TestTrue(TEXT("The release shows"), HasTag(RpgGameplayTags::State_Ranged_Releasing));
	TestTrue(TEXT("The owner presents its own release"), Fixture.ASC->IsRangedReleasingForPresentation());
	TestFalse(TEXT("The owner no longer presents a draw"), Fixture.ASC->IsRangedDrawingForPresentation());
	TestFalse(TEXT("A released shot reports no draw progress"), Fixture.ASC->GetLocalRangedDrawProgress(DrawProgress, NockProgress));
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

	// The ability stays active after its release, so only the task's own guard can stop a second commit.
	Fixture.Ability->bEndOnRelease = false;
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
	TestTrue(TEXT("The ability is still active after its shot"), Fixture.IsActive());
	Fixture.ASC->CancelAbilityHandle(Fixture.Handle);
	Fixture.Ability->bEndOnRelease = true;

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
	FRpgProjectileShotCancelledDrawTest,
	"SurvivalRpg.Combat.ProjectileShot.CancelledDrawIgnoresALateRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgProjectileShotCancelledDrawTest::RunTest(const FString& Parameters)
{
	using namespace RpgProjectileShotTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this) || !Fixture.AddArrows(StartingArrows) || !Fixture.Grant(MakeDefinition())) return false;

	// A dodge, stagger or weapon removal cancels the drawn shot; its release message arrives afterwards.
	if (!TestTrue(TEXT("The shot activates"), Fixture.Activate())) return false;
	URpgAbilityTask_ProjectileShot* Task = Fixture.Ability->ShotTask;
	Fixture.Advance(1.0f);
	Fixture.ASC->CancelAbilityHandle(Fixture.Handle);
	TestFalse(TEXT("The cancel ends the shot"), Fixture.IsActive());
	TestTrue(TEXT("The cancelled shot is decided"), Task->IsResolvedForTests());

	FRpgProjectileShotRelease Release;
	Release.DrawSeconds = 1.0f;
	Release.AimOrigin = ShooterLocation;
	Release.AimDirection = FVector::ForwardVector;
	Task->ReceiveReleaseForTests(Release);
	Fixture.Release();
	TestEqual(TEXT("A release after the cancel shoots nothing"), Fixture.Ability->ReleasedCount, 0);
	TestEqual(TEXT("It launches nothing"), Fixture.CountLiveProjectiles(), 0);
	TestEqual(TEXT("It consumes no arrow"), Fixture.GetArrows(), StartingArrows);
	TestEqual(TEXT("It pays no stamina"), Fixture.GetStamina(), StartingStamina);
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
	FRpgSkillTreeAbilityTuning AddDrain;
	AddDrain.TuningTag = RpgGameplayTags::Ability_Tuning_Ranged_HoldDrain;
	AddDrain.Operation = ERpgSkillTreeTuningOperation::Add;
	AddDrain.Value = 2.0f;
	FRpgSkillTreeAbilityTuning HalfBreak = HalfCost;
	HalfBreak.TuningTag = RpgGameplayTags::Ability_Tuning_Ranged_BreakChance;
	TStrongObjectPtr<UGameplayEffect> Passive(URpgAbilityTuningEffectComponent::MakeTransientEffectForTests({ HalfCost, HalfDraw, AddDrain, HalfBreak }, 1000.0f));
	const float BaseBreakChance = GetDefault<ARpgProjectileShotAutomationTestProjectile>()->GetRecoveryDefinition().BaseBreakChance;

	if (!TestTrue(TEXT("The shot activates"), Fixture.Activate())) return false;
	Fixture.ASC->ApplyGameplayEffectToSelf(Passive.Get(), 1.0f, Fixture.ASC->MakeEffectContext());
	Fixture.Advance(0.5f);
	Fixture.Release();
	TestEqual(TEXT("A passive gained during the draw keeps this shot's cost"), Fixture.GetStamina(), StartingStamina - ReleaseCost);
	TestEqual(TEXT("A passive gained during the draw keeps this shot's draw time"), Fixture.Ability->LastDrawFraction, 0.5f, 0.001f);
	const ARpgCombatProjectile* FirstArrow = Fixture.Ability->LastProjectile.Get();
	TestEqual(TEXT("A passive gained during the draw keeps this shot's break chance"),
		FirstArrow ? FirstArrow->GetBreakChanceForTests() : -1.0f, BaseBreakChance);

	if (!TestTrue(TEXT("The next shot activates"), Fixture.Activate())) return false;
	TestEqual(TEXT("The next shot drains while held as the passive adds"), Fixture.Ability->ShotTask->GetShotTuning().HoldStaminaPerSecond, 2.0f);
	TestEqual(TEXT("The next shot breaks as the passive tunes"), Fixture.Ability->ShotTask->GetShotTuning().BreakChance, BaseBreakChance * 0.5f);
	Fixture.Advance(0.5f);
	Fixture.Release();
	TestEqual(TEXT("The next shot releases"), Fixture.Ability->LastResult, ERpgProjectileShotResult::Released);
	TestEqual(TEXT("The next shot pays the passive's cost"), Fixture.GetStamina(), StartingStamina - ReleaseCost * 1.5f);
	TestEqual(TEXT("The next shot draws in the passive's time"), Fixture.Ability->LastDrawFraction, 1.0f, 0.001f);
	const ARpgCombatProjectile* NextArrow = Fixture.Ability->LastProjectile.Get();
	TestEqual(TEXT("The next arrow carries the tuned break chance"), NextArrow ? NextArrow->GetBreakChanceForTests() : -1.0f, BaseBreakChance * 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgProjectileShotFreeShotTest,
	"SurvivalRpg.Combat.ProjectileShot.ShotWithoutStaminaEffectStaysFree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgProjectileShotFreeShotTest::RunTest(const FString& Parameters)
{
	using namespace RpgProjectileShotTests;
	FRpgProjectileShotDefinition Definition = MakeDefinition();
	Definition.ReleaseStaminaCost = 0.0f;
	Definition.StaminaChangeEffect = nullptr;
	FFixture Fixture;
	if (!Fixture.Initialize(*this) || !Fixture.AddArrows(StartingArrows) || !Fixture.Grant(Definition)) return false;

	// A passive that adds costs to every shot, applied to a shot without a stamina effect, such as a spell or NPC shot.
	FRpgSkillTreeAbilityTuning AddCost;
	AddCost.TuningTag = RpgGameplayTags::Ability_Tuning_Ranged_ReleaseCost;
	AddCost.Operation = ERpgSkillTreeTuningOperation::Add;
	AddCost.Value = 5.0f;
	FRpgSkillTreeAbilityTuning AddDrain = AddCost;
	AddDrain.TuningTag = RpgGameplayTags::Ability_Tuning_Ranged_HoldDrain;
	TStrongObjectPtr<UGameplayEffect> Passive(URpgAbilityTuningEffectComponent::MakeTransientEffectForTests({ AddCost, AddDrain }, 1000.0f));
	Fixture.ASC->ApplyGameplayEffectToSelf(Passive.Get(), 1.0f, Fixture.ASC->MakeEffectContext());

	if (!TestTrue(TEXT("The shot activates"), Fixture.Activate())) return false;
	TestEqual(TEXT("Without a stamina effect a passive adds no release cost"), Fixture.Ability->ShotTask->GetShotTuning().ReleaseStaminaCost, 0.0f);
	TestEqual(TEXT("Without a stamina effect a passive adds no hold drain"), Fixture.Ability->ShotTask->GetShotTuning().HoldStaminaPerSecond, 0.0f);
	Fixture.Advance(1.0f);
	Fixture.Release();
	TestEqual(TEXT("The free shot still shoots"), Fixture.Ability->LastResult, ERpgProjectileShotResult::Released);
	TestEqual(TEXT("It pays no stamina"), Fixture.GetStamina(), StartingStamina);
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
	FRpgProjectileShotBowContentTest,
	"SurvivalRpg.Combat.ProjectileShot.BowContentShootsTheStandardArrow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgProjectileShotBowContentTest::RunTest(const FString& Parameters)
{
	using namespace RpgProjectileShotTests;
	UClass* ShotClass = LoadClass<UGameplayAbility>(nullptr, TEXT("/GF_Combat_Ranged/GAS/Abilities/GA_BowShot.GA_BowShot_C"));
	UClass* BowClass = LoadClass<URpgRangedWeaponInstance>(nullptr,
		TEXT("/GF_Combat_Ranged/Equipment/Bow/BP_WeaponInstance_Bow.BP_WeaponInstance_Bow_C"));
	UClass* ArrowClass = LoadClass<URpgInventoryItemDefinition>(nullptr, TEXT("/GF_Combat_Ranged/Items/ID_Arrow_Standard.ID_Arrow_Standard_C"));
	UClass* ArrowProjectileClass = LoadClass<ARpgCombatProjectile>(nullptr,
		TEXT("/GF_Combat_Ranged/Projectiles/BP_Projectile_Arrow.BP_Projectile_Arrow_C"));
	const URpgAbilitySet* BowAbilities = LoadObject<URpgAbilitySet>(nullptr, TEXT("/GF_Combat_Ranged/GAS/AbilitySets/AS_Bow.AS_Bow"));
	if (!TestTrue(TEXT("The bow content loads"), ShotClass && BowClass && ArrowClass && ArrowProjectileClass && BowAbilities)) return false;

	FFixture Fixture;
	if (!Fixture.Initialize(*this) || !Fixture.AddArrows(4, ArrowClass)) return false;

	// The bow's ability set grants the shot with the equipped bow as its source, as the equipment manager does.
	URpgRangedWeaponInstance* Bow = NewObject<URpgRangedWeaponInstance>(Fixture.Pawn, BowClass);
	BowAbilities->GiveToAbilitySystem(Fixture.ASC, nullptr, Bow);
	const FGameplayAbilitySpec* Granted = Fixture.ASC->FindAbilitySpecFromClass(ShotClass);
	if (!TestNotNull(TEXT("The bow's ability set grants the shot"), Granted)) return false;
	Fixture.Handle = Granted->Handle;
	const FRpgProjectileShotDefinition& Shot = Bow->GetShotDefinition();
	const auto Press = [&Fixture]()
	{
		Fixture.ASC->AbilityInputTagPressed(RpgGameplayTags::InputTag_Weapon_Primary);
		Fixture.ASC->ProcessAbilityInput(0.0f, false);
	};
	const auto Release = [&Fixture]()
	{
		Fixture.ASC->AbilityInputTagReleased(RpgGameplayTags::InputTag_Weapon_Primary);
		Fixture.ASC->ProcessAbilityInput(0.0f, false);
	};

	// A tap shorter than the nock lowers the bow without cost.
	Press();
	TestTrue(TEXT("Pressing the attack draws the bow"), Fixture.IsActive());
	Fixture.Advance(Shot.MinimumNockSeconds * 0.5f);
	Release();
	TestFalse(TEXT("An early release ends the draw"), Fixture.IsActive());
	TestEqual(TEXT("An early release shoots nothing"), Fixture.CountLiveProjectiles(), 0);
	TestEqual(TEXT("An early release costs no arrow"), Fixture.GetArrows(ArrowClass), 4);
	TestEqual(TEXT("An early release costs no stamina"), Fixture.GetStamina(), StartingStamina);

	// A full draw shoots one standard arrow at the bow's top speed and pays once.
	Press();
	TestTrue(TEXT("Holding the primary input alone enters the aim stance"), Fixture.ASC->IsRangedAimActive());
	Fixture.Advance(Shot.FullDrawSeconds + 0.5f);
	Release();
	TestFalse(TEXT("The release ends the shot"), Fixture.IsActive());
	TestFalse(TEXT("Ending the shot ends its stance"), Fixture.ASC->IsRangedAimActive());
	ARpgCombatProjectile* Arrow = nullptr;
	for (TActorIterator<ARpgCombatProjectile> It(Fixture.World); It; ++It)
	{
		Arrow = IsValid(*It) ? *It : Arrow;
	}
	if (!TestNotNull(TEXT("The release launches an arrow"), Arrow)) return false;
	TestTrue(TEXT("It is the standard arrow's projectile"), Arrow->IsA(ArrowProjectileClass));
	const ARpgCombatProjectile* ArrowDefaults = ArrowProjectileClass->GetDefaultObject<ARpgCombatProjectile>();
	TestEqual(TEXT("The standard arrow is recoverable"), ArrowDefaults->GetImpactRecovery(), ERpgProjectileImpactRecovery::Recoverable);
	const TSubclassOf<ARpgEmbeddedProjectilePickup> ArrowPickupClass = ArrowDefaults->GetRecoveryDefinition().PickupClass;
	TestTrue(TEXT("The standard arrow leaves a content pickup"),
		ArrowPickupClass && ArrowPickupClass != ARpgEmbeddedProjectilePickup::StaticClass());
	TestTrue(TEXT("The shot returns the consumed standard arrow"), Arrow->GetRecoveryItemDefinitionForTests() == ArrowClass);
	TestTrue(TEXT("The standard arrow authors a break chance"), ArrowDefaults->GetRecoveryDefinition().BaseBreakChance > 0.0f);
	TestEqual(TEXT("The shot breaks with the arrow's base chance"), Arrow->GetBreakChanceForTests(),
		ArrowDefaults->GetRecoveryDefinition().BaseBreakChance);
	const float TopSpeed = Shot.LaunchSpeedByDrawFraction.GetRichCurveConst()->Eval(1.0f);
	TestEqual(TEXT("A full draw shoots at the bow curve's top speed"), Arrow->GetProjectileMovement()->Velocity.Size(), static_cast<double>(TopSpeed), 1.0);
	TestEqual(TEXT("The bow curve sets the damage"), GetSetByCaller(Arrow, RpgGameplayTags::SetByCaller_Damage),
		Shot.DamageByDrawFraction.GetRichCurveConst()->Eval(1.0f), 0.01f);
	TestEqual(TEXT("The shot consumes one arrow"), Fixture.GetArrows(ArrowClass), 3);
	TestEqual(TEXT("The shot pays the bow's release cost once"), Fixture.GetStamina(), StartingStamina - Shot.ReleaseStaminaCost, 0.01f);

	// Ranged passives reach the bow through its ability id; one aimed at another ability does not.
	FRpgSkillTreeAbilityTuning BowHalfCost;
	BowHalfCost.AbilityIdTag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Ranged.BowShot"));
	BowHalfCost.TuningTag = RpgGameplayTags::Ability_Tuning_Ranged_ReleaseCost;
	BowHalfCost.Operation = ERpgSkillTreeTuningOperation::Multiply;
	BowHalfCost.Value = 0.5f;
	FRpgSkillTreeAbilityTuning SprintFreeCost = BowHalfCost;
	SprintFreeCost.AbilityIdTag = RpgGameplayTags::Ability_Movement_Sprint;
	SprintFreeCost.Value = 0.0f;
	TStrongObjectPtr<UGameplayEffect> Passive(URpgAbilityTuningEffectComponent::MakeTransientEffectForTests({ BowHalfCost, SprintFreeCost }, 1000.0f));
	Fixture.ASC->ApplyGameplayEffectToSelf(Passive.Get(), 1.0f, Fixture.ASC->MakeEffectContext());
	const float BeforeTunedShot = Fixture.GetStamina();
	Press();
	Fixture.Advance(Shot.FullDrawSeconds + 0.5f);
	Release();
	TestEqual(TEXT("The tuned shot consumes one arrow"), Fixture.GetArrows(ArrowClass), 2);
	TestEqual(TEXT("Only the passive for the bow's ability id changes its cost"), Fixture.GetStamina(),
		BeforeTunedShot - Shot.ReleaseStaminaCost * 0.5f, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgProjectileShotAirbornePressTest,
	"SurvivalRpg.Combat.ProjectileShot.BowShotPressedInTheAirWaitsForItsStance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgProjectileShotAirbornePressTest::RunTest(const FString& Parameters)
{
	using namespace RpgProjectileShotTests;
	UClass* ShotClass = LoadClass<UGameplayAbility>(nullptr, TEXT("/GF_Combat_Ranged/GAS/Abilities/GA_BowShot.GA_BowShot_C"));
	UClass* BowClass = LoadClass<URpgRangedWeaponInstance>(nullptr,
		TEXT("/GF_Combat_Ranged/Equipment/Bow/BP_WeaponInstance_Bow.BP_WeaponInstance_Bow_C"));
	UClass* ArrowClass = LoadClass<URpgInventoryItemDefinition>(nullptr, TEXT("/GF_Combat_Ranged/Items/ID_Arrow_Standard.ID_Arrow_Standard_C"));
	const URpgAbilitySet* BowAbilities = LoadObject<URpgAbilitySet>(nullptr, TEXT("/GF_Combat_Ranged/GAS/AbilitySets/AS_Bow.AS_Bow"));
	if (!TestTrue(TEXT("The bow content loads"), ShotClass && BowClass && ArrowClass && BowAbilities)) return false;

	FFixture Fixture;
	if (!Fixture.Initialize(*this, true, true) || !Fixture.AddArrows(4, ArrowClass)) return false;
	URpgRangedWeaponInstance* Bow = NewObject<URpgRangedWeaponInstance>(Fixture.Pawn, BowClass);
	BowAbilities->GiveToAbilitySystem(Fixture.ASC, nullptr, Bow);
	const FGameplayAbilitySpec* Granted = Fixture.ASC->FindAbilitySpecFromClass(ShotClass);
	if (!TestNotNull(TEXT("The bow's ability set grants the shot"), Granted)) return false;
	Fixture.Handle = Granted->Handle;
	const FRpgProjectileShotDefinition& Shot = Bow->GetShotDefinition();
	UCharacterMovementComponent* Movement = CastChecked<ACharacter>(Fixture.Pawn)->GetCharacterMovement();
	const auto Press = [&Fixture]()
	{
		Fixture.ASC->AbilityInputTagPressed(RpgGameplayTags::InputTag_Weapon_Primary);
		Fixture.ASC->ProcessAbilityInput(0.0f, false);
	};
	const auto Release = [&Fixture]()
	{
		Fixture.ASC->AbilityInputTagReleased(RpgGameplayTags::InputTag_Weapon_Primary);
		Fixture.ASC->ProcessAbilityInput(0.0f, false);
	};
	const auto IsDrawing = [&Fixture]()
	{
		return Fixture.ASC->HasMatchingGameplayTag(RpgGameplayTags::State_Ranged_Drawing) || Fixture.ASC->IsRangedDrawingForPresentation();
	};

	// Pressed after a jump and released still in the air, even after a full draw's time, the shot never draws or pays.
	Movement->SetMovementMode(MOVE_Falling);
	Press();
	Fixture.TickTasks();
	TestTrue(TEXT("A press in the air is kept"), Fixture.IsActive());
	TestFalse(TEXT("The stance waits in the air"), Fixture.ASC->IsRangedAimActive());
	TestFalse(TEXT("The shot does not draw without its stance"), IsDrawing());
	Fixture.Advance(Shot.FullDrawSeconds + 0.5f);
	Fixture.TickTasks();
	TestFalse(TEXT("It still does not draw while falling"), IsDrawing());
	Release();
	TestFalse(TEXT("The release in the air ends the shot"), Fixture.IsActive());
	TestEqual(TEXT("It launches nothing"), Fixture.CountLiveProjectiles(), 0);
	TestEqual(TEXT("It costs no arrow"), Fixture.GetArrows(ArrowClass), 4);
	TestEqual(TEXT("It costs no stamina"), Fixture.GetStamina(), StartingStamina);

	// Held through the landing, the press draws from the landing on, as the stance begins, and shoots once.
	Press();
	Fixture.Advance(1.0f);
	Fixture.TickTasks();
	TestFalse(TEXT("A held press still waits in the air"), IsDrawing());
	Movement->SetMovementMode(MOVE_Flying);
	Fixture.TickTasks();
	TestTrue(TEXT("The stance begins on landing"), Fixture.ASC->IsRangedAimActive());
	TestTrue(TEXT("The draw begins with the stance"), IsDrawing());
	float DrawProgress = -1.0f;
	float NockProgress = -1.0f;
	TestTrue(TEXT("The owner knows how far it drew"), Fixture.ASC->GetLocalRangedDrawProgress(DrawProgress, NockProgress));
	TestEqual(TEXT("The time in the air is no draw time"), DrawProgress, 0.0f, 0.001f);
	Fixture.Advance(Shot.FullDrawSeconds + 0.5f);
	Release();
	TestFalse(TEXT("The release ends the shot"), Fixture.IsActive());
	TestEqual(TEXT("The landed draw launches one arrow"), Fixture.CountLiveProjectiles(), 1);
	TestEqual(TEXT("It consumes one arrow"), Fixture.GetArrows(ArrowClass), 3);
	TestEqual(TEXT("It pays the release cost once"), Fixture.GetStamina(), StartingStamina - Shot.ReleaseStaminaCost, 0.01f);
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
		const ARpgCombatProjectile* Arrow = Fixture.Ability->LastProjectile.Get();
		TestTrue(TEXT("An arrow nobody paid for can never be recovered"), Arrow && !Arrow->GetRecoveryItemDefinitionForTests());
	}
	{
		// Ammunition consumed on impact never comes back, so its unlimited fire takes no recovery token.
		FRpgProjectileShotDefinition Consumed = MakeDefinition();
		Consumed.UnlimitedRecoveryBudget = 2;
		FFixture Fixture;
		if (!Fixture.Initialize(*this, false) || !Fixture.Grant(Consumed, ERpgProjectileAmmunitionPolicy::Unlimited)) return false;
		TestEqual(TEXT("A consumed-on-impact shot releases"), Fixture.Shoot(1.0f), ERpgProjectileShotResult::Released);
		const ARpgCombatProjectile* Spell = Fixture.Ability->LastProjectile.Get();
		TestTrue(TEXT("It carries no recovery item"), Spell && !Spell->GetRecoveryItemDefinitionForTests());
		TestEqual(TEXT("It reserves no recovery token"), Fixture.ASC->GetRangedRecoveryTokensUsed(), 0);
	}
	{
		// An NPC archer's unlimited fire returns ammunition only for its life's recovery budget.
		FRpgProjectileShotDefinition Budgeted = MakeDefinition(URpgProjectileRecoveryAutomationArrowDefinition::StaticClass());
		Budgeted.UnlimitedRecoveryBudget = 2;
		FFixture Fixture;
		if (!Fixture.Initialize(*this, false) || !Fixture.Grant(Budgeted, ERpgProjectileAmmunitionPolicy::Unlimited)) return false;
		const auto ShootRecoverable = [this, &Fixture]()
		{
			const ERpgProjectileShotResult Result = Fixture.Shoot(1.0f);
			const ARpgCombatProjectile* Arrow = Fixture.Ability->LastProjectile.Get();
			TestEqual(TEXT("The budgeted shot releases"), Result, ERpgProjectileShotResult::Released);
			return Arrow && Arrow->GetRecoveryItemDefinitionForTests() != nullptr;
		};
		TestTrue(TEXT("The first shot of a life can be recovered"), ShootRecoverable());
		TestTrue(TEXT("The second shot of a life can be recovered"), ShootRecoverable());
		TestFalse(TEXT("A shot beyond the budget returns nothing"), ShootRecoverable());
		TestEqual(TEXT("The life used its whole budget"), Fixture.ASC->GetRangedRecoveryTokensUsed(), 2);
		FRpgProjectileShotDefinition Generous = Budgeted;
		Generous.UnlimitedRecoveryBudget = 5;
		Fixture.Ability->ShotDefinition = Generous;
		TestFalse(TEXT("A weapon with a bigger budget never raises the life's budget"), ShootRecoverable());

		// A new life, such as a newly spawned enemy, starts with a fresh budget; a failed release keeps it.
		APawn* NextLife = Fixture.World->SpawnActor<APawn>();
		Fixture.ASC->InitAbilityActorInfo(Fixture.Pawn, NextLife);
		TestEqual(TEXT("A new life has used no tokens"), Fixture.ASC->GetRangedRecoveryTokensUsed(), 0);
		FRpgProjectileShotDefinition Failing = MakeDefinition(URpgProjectileShotAutomationFailingArrowDefinition::StaticClass());
		Failing.UnlimitedRecoveryBudget = 2;
		Fixture.Ability->ShotDefinition = Failing;
		TestEqual(TEXT("A failing spawn releases nothing"), Fixture.Shoot(1.0f), ERpgProjectileShotResult::SpawnFailed);
		TestEqual(TEXT("A failed release gives its token back"), Fixture.ASC->GetRangedRecoveryTokensUsed(), 0);
	}
	{
		FFixture Fixture;
		if (!Fixture.Initialize(*this) || !Fixture.AddArrows(StartingArrows) || !Fixture.Grant(MakeDefinition())) return false;
		TestEqual(TEXT("A shooter with arrows shoots"), Fixture.Shoot(1.0f), ERpgProjectileShotResult::Released);
		const ARpgCombatProjectile* Arrow = Fixture.Ability->LastProjectile.Get();
		TestTrue(TEXT("A consumed arrow can be recovered as the same ammunition"),
			Arrow && Arrow->GetRecoveryItemDefinitionForTests() == MakeDefinition().AmmunitionItemDefinition);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgProjectileShotLaunchDirectionTest,
	"SurvivalRpg.Combat.ProjectileShot.LaunchDirectionArcsOntoTheTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgProjectileShotLaunchDirectionTest::RunTest(const FString& Parameters)
{
	constexpr float Speed = 4000.0f;
	constexpr float Gravity = 980.0f;
	const FVector Origin(0.0f, 0.0f, 100.0f);
	// Flies the launch analytically and returns its height above Target where it passes Target's horizontal distance.
	const auto MissAtTarget = [&Origin](const FVector& Target, const FVector& Direction)
	{
		const double Horizontal = (Target - Origin).Size2D();
		const double HorizontalSpeed = Speed * Direction.Size2D();
		const double Seconds = Horizontal / HorizontalSpeed;
		return Origin.Z + Speed * Direction.Z * Seconds - 0.5 * Gravity * FMath::Square(Seconds) - Target.Z;
	};

	for (const FVector& Target : { FVector(1500.0f, 0.0f, 100.0f), FVector(0.0f, 2500.0f, -200.0f), FVector(-900.0f, 600.0f, 400.0f) })
	{
		FVector Direction;
		TestTrue(FString::Printf(TEXT("A target at %s is in reach"), *Target.ToString()),
			RpgProjectileShot::SolveLaunchDirection(Origin, Target, Speed, Gravity, Direction));
		TestTrue(TEXT("The direction is a unit vector"), Direction.IsNormalized());
		TestTrue(TEXT("The direction heads toward the target"), (Direction.GetSafeNormal2D() | (Target - Origin).GetSafeNormal2D()) > 0.999f);
		TestTrue(TEXT("The direction aims above the straight line against gravity"), Direction.Z > (Target - Origin).GetSafeNormal().Z);
		TestEqual(TEXT("The arc passes the target"), MissAtTarget(Target, Direction), 0.0, 1.0);
	}

	const FVector Far(100000.0f, 0.0f, 100.0f);
	FVector Direction;
	TestFalse(TEXT("A target beyond the arc's reach is out of reach"), RpgProjectileShot::SolveLaunchDirection(Origin, Far, Speed, Gravity, Direction));
	TestTrue(TEXT("It is then aimed at directly"), Direction.Equals((Far - Origin).GetSafeNormal(), 1.0e-4f));
	TestTrue(TEXT("Without gravity the target is in reach"), RpgProjectileShot::SolveLaunchDirection(Origin, Far, Speed, 0.0f, Direction));
	TestTrue(TEXT("and aimed at directly"), Direction.Equals((Far - Origin).GetSafeNormal(), 1.0e-4f));
	TestFalse(TEXT("A target straight above has no arc"), RpgProjectileShot::SolveLaunchDirection(Origin, Origin + FVector(0.0f, 0.0f, 500.0f), Speed, Gravity, Direction));
	return true;
}

#endif
