#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystem/Abilities/RpgGameplayAbility_Harvest.h"
#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
#include "Harvesting/RpgHarvestBurn.h"
#include "Harvesting/RpgHarvestableComponent.h"
#include "Harvesting/RpgHarvestAutomationTestTypes.h"
#include "Harvesting/RpgHarvestAutomationTestWorld.h"
#include "Harvesting/RpgHarvestProfile.h"
#include "Harvesting/RpgHarvestRewardService.h"
#include "Harvesting/RpgHarvestRuneStrike.h"
#include "Harvesting/RpgHarvestStockRules.h"
#include "Harvesting/RpgHarvestSwarm.h"
#include "Harvesting/RpgHarvestTargetingComponent.h"
#include "SurvivalRpg/Animation/AnimNotify_RpgGameplayEvent.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgFocusSet.h"
#include "SurvivalRpg/AbilitySystem/Effects/RpgAbilityTuningEffectComponent.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentInstance.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootTable.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillGameplayTags.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillProgressionComponent.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeComponent.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeDefinition.h"
#include "SurvivalRpg/UI/IndicatorSystem/RpgIndicatorManagerComponent.h"
#include "Blueprint/UserWidget.h"

#include "AbilitySystemGlobals.h"
#include "Animation/AnimMontage.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/DecalActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayCueManager.h"
#include "GameplayEffect.h"
#include "Misc/ScopeExit.h"
#include "NativeGameplayTags.h"
#include "UObject/UnrealType.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestTuningTest_Tree, "SkillTree.Tree.HarvestTuningTest");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestTuningTest_Wide, "SkillTree.Node.HarvestTuningTest.Wide");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestTuningTest_Reach, "SkillTree.Node.HarvestTuningTest.Reach");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestTuningTest_Brood, "SkillTree.Node.HarvestTuningTest.Brood");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestTuningTest_Blast, "SkillTree.Node.HarvestTuningTest.Blast");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestRuneFlowTest_Echo, "SkillTree.Node.HarvestRuneFlowTest.Echo");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestRuneFlowTest_EveryHit, "SkillTree.Node.HarvestRuneFlowTest.EveryHit");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestRuneFlowTest_Transfer, "SkillTree.Node.HarvestRuneFlowTest.Transfer");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestRuneFlowTest_Split, "SkillTree.Node.HarvestRuneFlowTest.Split");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestSpreadTest_Leap, "SkillTree.Node.HarvestSpreadTest.Leap");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestSpreadTest_Pierce, "SkillTree.Node.HarvestSpreadTest.Pierce");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestSpreadTest_Fork, "SkillTree.Node.HarvestSpreadTest.Fork");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestSpreadTest_Burn, "SkillTree.Node.HarvestSpreadTest.Burn");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestSpreadTest_Wander, "SkillTree.Node.HarvestSpreadTest.Wander");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestWorkerTest_Worker, "SkillTree.Node.HarvestWorkerTest.Worker");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestWorkerTest_Pack, "SkillTree.Node.HarvestWorkerTest.Pack");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestCatalogTest_AllRunes, "SkillTree.Node.HarvestCatalogTest.AllRunes");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestTuningTest_Cooldown, "Cooldown.HarvestTuningTest");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestWorkTest_Tree, "SkillTree.Tree.HarvestWorkTest");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestWorkTest_Cleave, "SkillTree.Node.HarvestWorkTest.Cleave");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestWorkTest_Shatter, "SkillTree.Node.HarvestWorkTest.Shatter");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestWorkTest_FollowUp, "SkillTree.Node.HarvestWorkTest.FollowUp");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestYieldTest_Form, "Harvest.Form.AutomationTest");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestStrideTest_Cue, "GameplayCue.HarvestStrideTest");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestFormCueTest_Plain, "GameplayCue.HarvestFormCueTest.Plain");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestFormCueTest_Form, "GameplayCue.HarvestFormCueTest.Form");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestTargetCueTest, "GameplayCue.HarvestTargetCueTest");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestSwingTest_Hit, "GameplayCue.HarvestSwingTest.Hit");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestSwingTest_NoYield, "GameplayCue.HarvestSwingTest.NoYield");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestSwingTest_Wide, "SkillTree.Node.HarvestSwingTest.Wide");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HarvestFocusTest_Spell, "Harvest.Spell.AutomationTest");

namespace RpgHarvestAbilityTests
{
	using RpgHarvestAutomation::FScopedTestWorld;

	constexpr int32 YieldPerSection = 2;

	/** Harvester avatar, player state, and ASC wired like a possessed player without a controller. */
	struct FHarvesterFixture
	{
		APawn* Pawn = nullptr;
		ARpgHarvestAutomationTestPlayerState* PlayerState = nullptr;
		URpgAbilitySystemComponent* AbilitySystem = nullptr;

		bool IsValid() const
		{
			return Pawn && PlayerState && AbilitySystem;
		}
	};

	FHarvesterFixture SpawnHarvester(UWorld* World)
	{
		FHarvesterFixture Fixture;
		if (!World)
		{
			return Fixture;
		}

		FActorSpawnParameters StateParameters;
		StateParameters.Name = MakeUniqueObjectName(
			World,
			ARpgHarvestAutomationTestPlayerState::StaticClass(),
			TEXT("AbilityHarvesterState"));
		StateParameters.ObjectFlags = RF_Transient;
		StateParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Fixture.PlayerState = World->SpawnActor<ARpgHarvestAutomationTestPlayerState>(StateParameters);

		FActorSpawnParameters PawnParameters;
		PawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Fixture.Pawn = World->SpawnActor<APawn>(APawn::StaticClass(), FTransform::Identity, PawnParameters);
		Fixture.AbilitySystem = Fixture.PlayerState
			? Cast<URpgAbilitySystemComponent>(Fixture.PlayerState->GetAbilitySystemComponent())
			: nullptr;
		if (Fixture.IsValid())
		{
			Fixture.Pawn->SetPlayerState(Fixture.PlayerState);
			Fixture.AbilitySystem->InitAbilityActorInfo(Fixture.PlayerState, Fixture.Pawn);
		}
		return Fixture;
	}

	URpgHarvestProfile* MakeProfile(UObject* Outer, const int32 SectionCount)
	{
		URpgHarvestProfile* Profile = NewObject<URpgHarvestProfile>(Outer);
		URpgLootTable* Table = NewObject<URpgLootTable>(Profile);
		FRpgLootGroup& Group = Table->Groups.AddDefaulted_GetRef();
		Group.Mode = ERpgLootGroupMode::Independent;
		Group.GroupChancePercent = 100.0f;
		FRpgLootEntry& Entry = Group.Entries.AddDefaulted_GetRef();
		Entry.ItemDefinition = URpgHarvestAutomationTestStackItemDefinition::StaticClass();
		Entry.MinimumQuantity = YieldPerSection;
		Entry.MaximumQuantity = YieldPerSection;
		Entry.ChancePercent = 100.0f;
		Profile->LootTable = Table;
		Profile->SkillTag = RpgTradeSkillGameplayTags::Skill_Gathering_Mining;
		Profile->MinimumSkillLevel = 1;
		Profile->SkillExperience = 10;
		Profile->SectionCount = SectionCount;
		return Profile;
	}

	ARpgHarvestAutomationCollidableNodeActor* SpawnNode(UWorld* World, const FVector& Location, URpgHarvestProfile* Profile)
	{
		ARpgHarvestAutomationCollidableNodeActor* Node = World
			? World->SpawnActorDeferred<ARpgHarvestAutomationCollidableNodeActor>(
				ARpgHarvestAutomationCollidableNodeActor::StaticClass(),
				FTransform(Location),
				nullptr,
				nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn)
			: nullptr;
		FObjectProperty* ProfileProperty = FindFProperty<FObjectProperty>(
			URpgHarvestableComponent::StaticClass(),
			TEXT("HarvestProfile"));
		if (!Node || !ProfileProperty)
		{
			return nullptr;
		}
		ProfileProperty->SetObjectPropertyValue_InContainer(Node->HarvestableNode, Profile);
		Node->FinishSpawning(FTransform(Location));
		if (!Node->HasActorBegunPlay())
		{
			Node->DispatchBeginPlay();
		}
		return Node;
	}

	struct FGrantedAbility
	{
		FGameplayAbilitySpecHandle Handle;
		URpgHarvestAutomationTestAbility* Instance = nullptr;
	};

	FGrantedAbility GrantAbility(
		URpgAbilitySystemComponent* AbilitySystem,
		UObject* SourceObject = nullptr,
		const bool bBindPrimaryInput = false)
	{
		FGrantedAbility Granted;
		FGameplayAbilitySpec Spec(URpgHarvestAutomationTestAbility::StaticClass(), 1, INDEX_NONE, SourceObject);
		if (bBindPrimaryInput)
		{
			Spec.GetDynamicSpecSourceTags().AddTag(
				FGameplayTag::RequestGameplayTag(TEXT("InputTag.Weapon.Primary")));
		}
		Granted.Handle = AbilitySystem->GiveAbility(Spec);
		const FGameplayAbilitySpec* GrantedSpec = AbilitySystem->FindAbilitySpecFromHandle(Granted.Handle);
		Granted.Instance = GrantedSpec
			? Cast<URpgHarvestAutomationTestAbility>(GrantedSpec->GetPrimaryInstance())
			: nullptr;
		return Granted;
	}

	FRpgHarvestPreview Evaluate(URpgAbilitySystemComponent* AbilitySystem, const FGrantedAbility& Granted)
	{
		FRpgHarvestPreview Preview;
		const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromHandle(Granted.Handle);
		if (Spec && Granted.Instance && AbilitySystem->AbilityActorInfo.IsValid())
		{
			Granted.Instance->EvaluateTargets(*Spec, *AbilitySystem->AbilityActorInfo, Preview);
		}
		return Preview;
	}

	int32 CountMaterial(const ARpgHarvestAutomationTestPlayerState* PlayerState)
	{
		return PlayerState && PlayerState->GetInventoryManagerComponent()
			? PlayerState->GetInventoryManagerComponent()->GetTotalItemCountByDefinition(
				URpgHarvestAutomationTestStackItemDefinition::StaticClass())
			: 0;
	}

	TArray<ARpgDroppedInventoryActor*> GetWorldDrops(UWorld* World)
	{
		TArray<ARpgDroppedInventoryActor*> Drops;
		for (TActorIterator<ARpgDroppedInventoryActor> It(World); It; ++It)
		{
			if (IsValid(*It) && !It->IsActorBeingDestroyed())
			{
				Drops.Add(*It);
			}
		}
		return Drops;
	}

	UAnimMontage* MakeMontage(const TArray<TPair<FGameplayTag, float>>& Notifies, const float Length)
	{
		UAnimMontage* Montage = NewObject<UAnimMontage>(GetTransientPackage());
		Montage->SetCompositeLength(Length);
		FStructProperty* EventTagProperty = FindFProperty<FStructProperty>(
			UAnimNotify_RpgGameplayEvent::StaticClass(),
			TEXT("EventTag"));
		for (const TPair<FGameplayTag, float>& NotifyDefinition : Notifies)
		{
			UAnimNotify_RpgGameplayEvent* Notify = NewObject<UAnimNotify_RpgGameplayEvent>(Montage);
			if (EventTagProperty)
			{
				*EventTagProperty->ContainerPtrToValuePtr<FGameplayTag>(Notify) = NotifyDefinition.Key;
			}
			FAnimNotifyEvent& Event = Montage->Notifies.AddDefaulted_GetRef();
			Event.Notify = Notify;
			Event.SetTime(NotifyDefinition.Value);
		}
		return Montage;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestAbilityCommitDelayTest,
	"SurvivalRpg.Harvesting.Ability.CommitDelayFromAuthoredNotify",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestAbilityCommitDelayTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestAbilityTests;

	const FGameplayTag CommitTag = RpgHarvestingMagicGameplayTags::GameplayEvent_Harvesting_Commit;
	const FGameplayTag OtherTag = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.ItemUse.Apply"), false);
	float Delay = 0.0f;
	FString Failure;

	UAnimMontage* Montage = MakeMontage({{CommitTag, 0.4f}, {OtherTag, 0.1f}}, 1.0f);
	TestTrue(
		TEXT("The single commit notify resolves"),
		URpgGameplayAbility_Harvest::ResolveCommitDelay(Montage, CommitTag, 1.0f, Delay, Failure));
	TestTrue(TEXT("The delay equals the authored notify time"), FMath::IsNearlyEqual(Delay, 0.4f, 0.001f));
	TestTrue(
		TEXT("A faster play rate shortens the delay"),
		URpgGameplayAbility_Harvest::ResolveCommitDelay(Montage, CommitTag, 2.0f, Delay, Failure) &&
			FMath::IsNearlyEqual(Delay, 0.2f, 0.001f));

	TestFalse(
		TEXT("A montage without the commit notify is rejected"),
		URpgGameplayAbility_Harvest::ResolveCommitDelay(MakeMontage({{OtherTag, 0.2f}}, 1.0f), CommitTag, 1.0f, Delay, Failure));
	TestFalse(
		TEXT("Two commit notifies are rejected"),
		URpgGameplayAbility_Harvest::ResolveCommitDelay(
			MakeMontage({{CommitTag, 0.2f}, {CommitTag, 0.6f}}, 1.0f),
			CommitTag,
			1.0f,
			Delay,
			Failure));
	TestFalse(
		TEXT("A commit notify at or beyond the montage end is rejected"),
		URpgGameplayAbility_Harvest::ResolveCommitDelay(MakeMontage({{CommitTag, 1.0f}}, 1.0f), CommitTag, 1.0f, Delay, Failure));
	TestFalse(
		TEXT("A non-positive play rate is rejected"),
		URpgGameplayAbility_Harvest::ResolveCommitDelay(Montage, CommitTag, 0.0f, Delay, Failure));
	TestFalse(
		TEXT("A missing montage is rejected"),
		URpgGameplayAbility_Harvest::ResolveCommitDelay(nullptr, CommitTag, 1.0f, Delay, Failure));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestAbilityTargetingTest,
	"SurvivalRpg.Harvesting.Ability.TargetingSelectsReachableTargets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestAbilityTargetingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestAbilityTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	const float EyeHeight = Harvester.Pawn->BaseEyeHeight;

	ARpgHarvestAutomationCollidableNodeActor* Near =
		SpawnNode(World, FVector(150.0, 0.0, EyeHeight), MakeProfile(World, 4));
	const FGrantedAbility Granted = GrantAbility(Harvester.AbilitySystem);
	if (!TestNotNull(TEXT("Near node exists"), Near) ||
		!TestNotNull(TEXT("Granted test ability instance exists"), Granted.Instance))
	{
		return false;
	}

	FRpgHarvestTargetingParams Single;
	Single.MaxAimDistance = 1000.0f;
	Single.MaxReachFromAvatar = 250.0f;
	Granted.Instance->ConfigureTargeting(Single);

	FRpgHarvestPreview Preview = Evaluate(Harvester.AbilitySystem, Granted);
	if (!TestEqual(TEXT("The view ray selects one target"), Preview.Targets.Num(), 1))
	{
		return false;
	}
	TestTrue(TEXT("The selected receiver is the node's harvest component"), Preview.Targets[0].Receiver.Get() == Near->HarvestableNode);
	TestTrue(TEXT("The near node is in reach"), Preview.Targets[0].bInReach);
	TestTrue(TEXT("The near node would be harvested"), Preview.Targets[0].WouldHarvest());
	TestEqual(TEXT("The evaluation previews one section"), Preview.Targets[0].Result.SectionsTaken, 1);
	TestEqual(TEXT("The evaluation previews the stock left"), Preview.Targets[0].Result.RemainingSections, 3);
	TestFalse(TEXT("A single-target ability reports no area"), Preview.bHasArea);

	Near->SetActorLocation(FVector(600.0, 0.0, EyeHeight));
	Preview = Evaluate(Harvester.AbilitySystem, Granted);
	TestEqual(TEXT("A distant node is still selected for the preview"), Preview.Targets.Num(), 1);
	TestTrue(
		TEXT("A distant node is out of reach and would not be harvested"),
		Preview.Targets.Num() == 1 && !Preview.Targets[0].bInReach && !Preview.Targets[0].WouldHarvest());

	ARpgHarvestAutomationCollidableNodeActor* Side =
		SpawnNode(World, FVector(600.0, 160.0, EyeHeight), MakeProfile(World, 4));
	ARpgHarvestAutomationCollidableNodeActor* Far =
		SpawnNode(World, FVector(600.0, -260.0, EyeHeight), MakeProfile(World, 4));
	FRpgHarvestTargetingParams Area;
	Area.Shape = ERpgHarvestTargetShape::AreaAtAimPoint;
	Area.MaxAimDistance = 1000.0f;
	Area.MaxReachFromAvatar = 700.0f;
	Area.AreaRadius = 300.0f;
	Area.MaxTargets = 2;
	Granted.Instance->ConfigureTargeting(Area);
	Preview = Evaluate(Harvester.AbilitySystem, Granted);
	TestTrue(TEXT("An area ability reports its area"), Preview.bHasArea && FMath::IsNearlyEqual(Preview.AreaRadius, 300.0f));
	if (!TestEqual(TEXT("The area is capped at MaxTargets"), Preview.Targets.Num(), 2))
	{
		return false;
	}
	TestTrue(TEXT("The aimed node is the nearest area target"), Preview.Targets[0].Receiver.Get() == Near->HarvestableNode);
	TestTrue(TEXT("The second nearest node fills the remaining slot"), Preview.Targets[1].Receiver.Get() == Side->HarvestableNode);
	TestTrue(TEXT("Area targets inherit the aim point reach"), Preview.Targets[0].bInReach && Preview.Targets[1].bInReach);
	TestNotNull(TEXT("The farthest node exists outside the cap"), Far);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestAbilityCommitLifecycleTest,
	"SurvivalRpg.Harvesting.Ability.CommitOnceAndCancelYieldsNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestAbilityCommitLifecycleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestAbilityTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	ARpgHarvestAutomationCollidableNodeActor* Node =
		SpawnNode(World, FVector(150.0, 0.0, Harvester.Pawn->BaseEyeHeight), MakeProfile(World, 4));
	const FGrantedAbility Granted = GrantAbility(Harvester.AbilitySystem);
	if (!TestNotNull(TEXT("Lifecycle node exists"), Node) ||
		!TestNotNull(TEXT("Lifecycle ability instance exists"), Granted.Instance))
	{
		return false;
	}
	Granted.Instance->ConfigureCommitDelay(0.5f);
	URpgHarvestAutomationHarvestedListener* Harvested = NewObject<URpgHarvestAutomationHarvestedListener>();
	Node->HarvestableNode->OnHarvested.AddDynamic(Harvested, &URpgHarvestAutomationHarvestedListener::HandleHarvested);
	TestWorld.PrimeTimerManager();

	TestTrue(TEXT("The delayed harvest activates"), Harvester.AbilitySystem->TryActivateAbility(Granted.Handle));
	TestTrue(TEXT("The activation waits for its commit"), Granted.Instance->IsActive());
	TestEqual(TEXT("Nothing is extracted before the commit time"), Node->HarvestableNode->GetRemainingSections(), 4);
	Harvester.AbilitySystem->CancelAbilityHandle(Granted.Handle);
	TestFalse(TEXT("Cancelling ends the activation"), Granted.Instance->IsActive());
	TestWorld.AdvanceTimers(1.0f);
	TestEqual(TEXT("A cancelled activation never extracts stock"), Node->HarvestableNode->GetRemainingSections(), 4);
	TestEqual(TEXT("A cancelled activation never grants loot"), CountMaterial(Harvester.PlayerState), 0);

	TestTrue(TEXT("The harvest activates again"), Harvester.AbilitySystem->TryActivateAbility(Granted.Handle));
	TestWorld.AdvanceTimers(0.6f);
	TestEqual(TEXT("The commit extracts exactly one section"), Node->HarvestableNode->GetRemainingSections(), 3);
	TestEqual(TEXT("The commit grants the section reward"), CountMaterial(Harvester.PlayerState), YieldPerSection);
	TestFalse(TEXT("The montage-free activation ends after its commit"), Granted.Instance->IsActive());
	TestWorld.AdvanceTimers(1.0f);
	TestEqual(TEXT("No late second commit happens"), Node->HarvestableNode->GetRemainingSections(), 3);

	Granted.Instance->ConfigureCommitDelay(0.0f);
	Granted.Instance->ConfigureSections(3);
	TestTrue(TEXT("An immediate multi-section harvest activates"), Harvester.AbilitySystem->TryActivateAbility(Granted.Handle));
	TestFalse(TEXT("The immediate harvest ends at once"), Granted.Instance->IsActive());
	TestFalse(TEXT("The request takes the remaining sections and depletes the node"), Node->HarvestableNode->IsHarvestable());
	TestEqual(TEXT("The total yield equals the defined stock"), CountMaterial(Harvester.PlayerState), 4 * YieldPerSection);
	if (TestEqual(TEXT("Both commits report their results"), Harvested->Results.Num(), 2))
	{
		const FRpgHarvestResult& First = Harvested->Results[0];
		const FRpgHarvestResult& Last = Harvested->Results[1];
		TestTrue(TEXT("A swing is the harvester's own accepted hit"), First.IsAcceptedDirectHit() && Last.IsAcceptedDirectHit());
		TestTrue(
			TEXT("Every execution gets its own root action"),
			First.Provenance.RootActionId > 0 && Last.Provenance.RootActionId > 0 &&
				First.Provenance.RootActionId != Last.Provenance.RootActionId);
		TestTrue(
			TEXT("The last swing applies exactly the remaining work"),
			Last.WorkApplied == 3 * RpgHarvestWork::UnitsPerSection && Last.OverkillWork == 0 && Last.RemainingWork == 0);
	}

	TestTrue(TEXT("An activation on the empty node still activates"), Harvester.AbilitySystem->TryActivateAbility(Granted.Handle));
	TestEqual(TEXT("A swing on an empty node grants nothing"), CountMaterial(Harvester.PlayerState), 4 * YieldPerSection);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestAbilityHoldToAimTest,
	"SurvivalRpg.Harvesting.Ability.HoldToAimExecutesOnRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestAbilityHoldToAimTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestAbilityTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	ARpgHarvestAutomationCollidableNodeActor* Node =
		SpawnNode(World, FVector(150.0, 0.0, Harvester.Pawn->BaseEyeHeight), MakeProfile(World, 4));
	const FGrantedAbility Granted = GrantAbility(Harvester.AbilitySystem);
	FGameplayAbilitySpec* Spec = Harvester.AbilitySystem->FindAbilitySpecFromHandle(Granted.Handle);
	if (!TestNotNull(TEXT("Aim node exists"), Node) ||
		!TestNotNull(TEXT("Aim ability instance exists"), Granted.Instance) ||
		!TestNotNull(TEXT("Aim ability spec exists"), Spec))
	{
		return false;
	}
	Granted.Instance->ConfigureAimWhileInputHeld(true);
	Granted.Instance->ConfigureSections(2);

	Spec->InputPressed = true;
	TestTrue(TEXT("A held aim ability activates"), Harvester.AbilitySystem->TryActivateAbility(Granted.Handle));
	TestTrue(TEXT("The ability keeps aiming while the input is held"), Granted.Instance->IsActive());
	TestEqual(TEXT("Aiming extracts nothing"), Node->HarvestableNode->GetRemainingSections(), 4);
	Harvester.AbilitySystem->CancelAbilityHandle(Granted.Handle);
	TestFalse(TEXT("Cancelling the aim ends the ability"), Granted.Instance->IsActive());
	TestEqual(TEXT("A cancelled aim extracts nothing"), Node->HarvestableNode->GetRemainingSections(), 4);

	Spec = Harvester.AbilitySystem->FindAbilitySpecFromHandle(Granted.Handle);
	Spec->InputPressed = true;
	TestTrue(TEXT("The aim ability activates again"), Harvester.AbilitySystem->TryActivateAbility(Granted.Handle));
	static_cast<UAbilitySystemComponent*>(Harvester.AbilitySystem)->AbilitySpecInputReleased(*Spec);
	TestEqual(TEXT("Releasing the input executes the harvest"), Node->HarvestableNode->GetRemainingSections(), 2);
	TestEqual(TEXT("The release harvest grants its sections"), CountMaterial(Harvester.PlayerState), 2 * YieldPerSection);
	TestFalse(TEXT("The released harvest ends after its commit"), Granted.Instance->IsActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestAbilitySkillUnlockTest,
	"SurvivalRpg.Harvesting.Ability.SkillUnlockGatesActivation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestAbilitySkillUnlockTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestAbilityTests;

	FScopedTestWorld TestWorld;
	FHarvesterFixture Harvester = SpawnHarvester(TestWorld.GetWorld());
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	const FGrantedAbility Granted = GrantAbility(Harvester.AbilitySystem);
	if (!TestNotNull(TEXT("Unlock ability instance exists"), Granted.Instance))
	{
		return false;
	}
	Granted.Instance->ConfigureSkillUnlock(RpgTradeSkillGameplayTags::Skill_Gathering_Mining, 2);

	FGameplayTagContainer FailureTags;
	TestFalse(
		TEXT("A locked harvest ability cannot activate"),
		static_cast<const UGameplayAbility*>(Granted.Instance)->CanActivateAbility(
			Granted.Handle,
			Harvester.AbilitySystem->AbilityActorInfo.Get(),
			nullptr,
			nullptr,
			&FailureTags));
	TestTrue(
		TEXT("The failure names the skill unlock"),
		FailureTags.HasTagExact(RpgHarvestingMagicGameplayTags::Ability_ActivateFail_Harvesting_SkillLevel));

	FTradeSkillState MiningState;
	MiningState.SkillTag = RpgTradeSkillGameplayTags::Skill_Gathering_Mining;
	MiningState.Level = 2;
	TestTrue(
		TEXT("Progression reaches the unlock level"),
		Harvester.PlayerState->GetTradeSkillProgressionComponent()->RestoreSkillStates({MiningState}));
	TestTrue(
		TEXT("The unlocked ability can activate"),
		static_cast<const UGameplayAbility*>(Granted.Instance)->CanActivateAbility(
			Granted.Handle,
			Harvester.AbilitySystem->AbilityActorInfo.Get(),
			nullptr,
			nullptr,
			nullptr));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestAbilityToolAndPreviewTest,
	"SurvivalRpg.Harvesting.Ability.ToolSourceAndPrimaryPreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestAbilityToolAndPreviewTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestAbilityTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}

	URpgHarvestProfile* ToolProfile = MakeProfile(World, 4);
	ToolProfile->RequiredToolTag = RpgHarvestingMagicGameplayTags::Tool_Harvesting_Skinning;
	ARpgHarvestAutomationCollidableNodeActor* Node =
		SpawnNode(World, FVector(150.0, 0.0, Harvester.Pawn->BaseEyeHeight), ToolProfile);

	URpgInventoryItemInstance* ToolItem = Harvester.PlayerState->GetInventoryManagerComponent()->GrantItemDefinition(
		URpgHarvestAutomationTestLowToolDefinition::StaticClass());
	URpgEquipmentInstance* Equipment = NewObject<URpgEquipmentInstance>(Harvester.Pawn);
	if (!TestNotNull(TEXT("Tool node exists"), Node) ||
		!TestNotNull(TEXT("Tool item exists"), ToolItem) ||
		!TestNotNull(TEXT("Equipment instance exists"), Equipment))
	{
		return false;
	}
	Equipment->SetInstigator(ToolItem);

	const FGrantedAbility ToolSwing = GrantAbility(Harvester.AbilitySystem, Equipment, true);
	const FGrantedAbility BareHands = GrantAbility(Harvester.AbilitySystem);
	if (!TestNotNull(TEXT("Tool swing exists"), ToolSwing.Instance) ||
		!TestNotNull(TEXT("Tool-less swing exists"), BareHands.Instance))
	{
		return false;
	}

	FRpgHarvestPreview ToolPreview = Evaluate(Harvester.AbilitySystem, ToolSwing);
	TestTrue(
		TEXT("The tool category comes from the source equipment's item"),
		ToolPreview.Targets.Num() == 1 && ToolPreview.Targets[0].WouldHarvest());
	FRpgHarvestPreview BarePreview = Evaluate(Harvester.AbilitySystem, BareHands);
	TestTrue(
		TEXT("A tool-less swing is previewed as WrongTool"),
		BarePreview.Targets.Num() == 1 && BarePreview.Targets[0].Result.Outcome == ERpgHarvestOutcome::WrongTool);

	FActorSpawnParameters ControllerParameters;
	ControllerParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APlayerController* Controller = World->SpawnActor<APlayerController>(
		APlayerController::StaticClass(),
		FTransform::Identity,
		ControllerParameters);
	if (!TestNotNull(TEXT("Local controller exists"), Controller))
	{
		return false;
	}
	Controller->SetAsLocalPlayerController();
	Controller->PlayerState = Harvester.PlayerState;
	URpgHarvestTargetingComponent* Targeting = NewObject<URpgHarvestTargetingComponent>(Controller);
	Targeting->RegisterComponent();
	URpgHarvestAutomationPreviewListener* Listener = NewObject<URpgHarvestAutomationPreviewListener>();
	Targeting->OnPreviewChanged.AddDynamic(Listener, &URpgHarvestAutomationPreviewListener::HandlePreviewChanged);
	TestTrue(TEXT("The component is found on its controller"), URpgHarvestTargetingComponent::FindForController(Controller) == Targeting);

	Targeting->RefreshPreview();
	TestEqual(TEXT("The first preview notifies listeners"), Listener->EventCount, 1);
	const FRpgHarvestPreview& Current = Listener->LastPreview;
	TestTrue(
		TEXT("The always-on preview uses the primary tool swing"),
		Current.HasAbility() && !Current.bIsAiming && Current.Targets.Num() == 1 && Current.Targets[0].WouldHarvest());

	Targeting->RefreshPreview();
	TestEqual(TEXT("An unchanged preview does not notify again"), Listener->EventCount, 1);

	FRpgHarvestRequest Request;
	Request.Harvester = Harvester.Pawn;
	Request.AbilityId = RpgHarvestingMagicGameplayTags::Ability_Harvesting_Manual;
	Request.ToolTag = RpgHarvestingMagicGameplayTags::Tool_Harvesting_Skinning;
	Request.Hit = FHitResult(Node, Node->Collision.Get(), Node->GetActorLocation(), FVector::UpVector);
	Request.ExpectedRevision = 0;
	TestTrue(TEXT("Another harvester extracts a section"), Node->HarvestableNode->CommitHarvest_Implementation(Request).IsSuccess());
	Targeting->RefreshPreview();
	TestEqual(TEXT("A stock change notifies listeners"), Listener->EventCount, 2);
	TestTrue(
		TEXT("The refreshed preview shows the stock left"),
		Listener->LastPreview.Targets.Num() == 1 && Listener->LastPreview.Targets[0].Result.RemainingSections == 2);

	Targeting->SetAimingAbility(BareHands.Handle, true);
	TestTrue(TEXT("A held aiming ability replaces the primary preview"), Targeting->GetCurrentPreview().bIsAiming);
	Targeting->SetAimingAbility(BareHands.Handle, false);
	TestFalse(TEXT("Releasing the aiming ability restores the primary preview"), Targeting->GetCurrentPreview().bIsAiming);

	Harvester.AbilitySystem->ClearAbility(ToolSwing.Handle);
	Targeting->RefreshPreview();
	TestFalse(TEXT("Without a primary harvest ability the preview is empty"), Targeting->GetCurrentPreview().HasAbility());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestAbilityIndicatorTest,
	"SurvivalRpg.Harvesting.Ability.TargetStatusAndIndicatorLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestAbilityIndicatorTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestAbilityTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	const float EyeHeight = Harvester.Pawn->BaseEyeHeight;
	ARpgHarvestAutomationCollidableNodeActor* Node =
		SpawnNode(World, FVector(150.0, 0.0, EyeHeight), MakeProfile(World, 4));
	URpgEquipmentInstance* Equipment = NewObject<URpgEquipmentInstance>(Harvester.Pawn);
	const FGrantedAbility Swing = GrantAbility(Harvester.AbilitySystem, Equipment, true);
	if (!TestNotNull(TEXT("Indicator node exists"), Node) ||
		!TestNotNull(TEXT("Primary swing exists"), Swing.Instance))
	{
		return false;
	}

	FActorSpawnParameters ControllerParameters;
	ControllerParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APlayerController* Controller = World->SpawnActor<APlayerController>(
		APlayerController::StaticClass(),
		FTransform::Identity,
		ControllerParameters);
	if (!TestNotNull(TEXT("Local controller exists"), Controller))
	{
		return false;
	}
	Controller->SetAsLocalPlayerController();
	Controller->PlayerState = Harvester.PlayerState;
	URpgIndicatorManagerComponent* IndicatorManager = NewObject<URpgIndicatorManagerComponent>(Controller);
	IndicatorManager->RegisterComponent();
	URpgHarvestAutomationTargetingComponent* Targeting = NewObject<URpgHarvestAutomationTargetingComponent>(Controller);
	Targeting->ConfigureIndicator(TSoftClassPtr<UUserWidget>(UUserWidget::StaticClass()));
	Targeting->RegisterComponent();

	ERpgHarvestTargetStatus Status = ERpgHarvestTargetStatus::None;
	int32 Remaining = 0;
	int32 SectionCount = 0;
	int32 ToTake = 0;
	TestFalse(TEXT("Nothing is summarized before the first refresh"), Targeting->GetPrimaryTargetStatus(Status, Remaining, SectionCount, ToTake));

	Targeting->RefreshPreview();
	TestTrue(TEXT("The primary target is summarized"), Targeting->GetPrimaryTargetStatus(Status, Remaining, SectionCount, ToTake));
	TestTrue(TEXT("A reachable stocked target is harvestable"), Status == ERpgHarvestTargetStatus::Harvestable);
	TestEqual(TEXT("The summary reports the current stock"), Remaining, 4);
	TestEqual(TEXT("The summary reports the section count"), SectionCount, 4);
	TestEqual(TEXT("The summary reports the sections a swing takes"), ToTake, 1);
	if (!TestEqual(TEXT("One indicator is registered for the target"), IndicatorManager->GetIndicators().Num(), 1))
	{
		return false;
	}
	UIndicatorDescriptor* FirstIndicator = IndicatorManager->GetIndicators()[0];
	TestTrue(TEXT("The indicator anchors to the hit component"), FirstIndicator->GetSceneComponent() == Node->Collision);
	TestTrue(TEXT("The indicator exposes the targeting read model"), FirstIndicator->GetDataObject() == Targeting);

	Targeting->RefreshPreview();
	TestEqual(TEXT("An unchanged target keeps its single indicator"), IndicatorManager->GetIndicators().Num(), 1);
	TestTrue(TEXT("An unchanged target keeps the same descriptor"), IndicatorManager->GetIndicators()[0] == FirstIndicator);

	Node->SetActorLocation(FVector(600.0, 0.0, EyeHeight));
	Targeting->RefreshPreview();
	TestTrue(TEXT("A distant target is summarized as out of reach"), Targeting->GetPrimaryTargetStatus(Status, Remaining, SectionCount, ToTake));
	TestTrue(TEXT("The distant target reports OutOfReach"), Status == ERpgHarvestTargetStatus::OutOfReach);
	TestEqual(TEXT("An out-of-reach target takes nothing"), ToTake, 0);

	Node->SetActorLocation(FVector(150.0, 0.0, EyeHeight));
	FRpgHarvestRequest Request;
	Request.Harvester = Harvester.Pawn;
	Request.AbilityId = RpgHarvestingMagicGameplayTags::Ability_Harvesting_Manual;
	Request.RequestedSections = 4;
	Request.Hit = FHitResult(Node, Node->Collision.Get(), Node->GetActorLocation(), FVector::UpVector);
	Request.ExpectedRevision = 0;
	TestTrue(TEXT("Another harvester empties the node"), Node->HarvestableNode->CommitHarvest_Implementation(Request).bDepleted);
	Targeting->RefreshPreview();
	TestTrue(TEXT("The emptied target is still summarized"), Targeting->GetPrimaryTargetStatus(Status, Remaining, SectionCount, ToTake));
	TestTrue(TEXT("The emptied target reports Depleted"), Status == ERpgHarvestTargetStatus::Depleted);
	TestEqual(TEXT("The emptied target reports no stock"), Remaining, 0);

	Harvester.AbilitySystem->ClearAbility(Swing.Handle);
	Targeting->RefreshPreview();
	TestFalse(TEXT("Without a harvest ability nothing is summarized"), Targeting->GetPrimaryTargetStatus(Status, Remaining, SectionCount, ToTake));
	TestEqual(TEXT("Without a target the indicator is removed"), IndicatorManager->GetIndicators().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestAbilityAimedAreaIndicatorTest,
	"SurvivalRpg.Harvesting.Ability.AimedAreaIndicatesEveryTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestAbilityAimedAreaIndicatorTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestAbilityTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	const float EyeHeight = Harvester.Pawn->BaseEyeHeight;
	ARpgHarvestAutomationCollidableNodeActor* Near =
		SpawnNode(World, FVector(600.0, 0.0, EyeHeight), MakeProfile(World, 4));
	ARpgHarvestAutomationCollidableNodeActor* Side =
		SpawnNode(World, FVector(600.0, 160.0, EyeHeight), MakeProfile(World, 4));
	const FGrantedAbility Aimed = GrantAbility(Harvester.AbilitySystem);
	if (!TestNotNull(TEXT("Near node exists"), Near) ||
		!TestNotNull(TEXT("Side node exists"), Side) ||
		!TestNotNull(TEXT("Aimed ability exists"), Aimed.Instance))
	{
		return false;
	}
	FRpgHarvestTargetingParams Area;
	Area.Shape = ERpgHarvestTargetShape::AreaAtAimPoint;
	Area.MaxAimDistance = 1000.0f;
	Area.MaxReachFromAvatar = 700.0f;
	Area.AreaRadius = 300.0f;
	Area.MaxTargets = 3;
	Aimed.Instance->ConfigureTargeting(Area);
	Aimed.Instance->ConfigureSections(3);
	Aimed.Instance->ConfigureAimWhileInputHeld(true);

	FActorSpawnParameters ControllerParameters;
	ControllerParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APlayerController* Controller = World->SpawnActor<APlayerController>(
		APlayerController::StaticClass(),
		FTransform::Identity,
		ControllerParameters);
	if (!TestNotNull(TEXT("Local controller exists"), Controller))
	{
		return false;
	}
	Controller->SetAsLocalPlayerController();
	Controller->PlayerState = Harvester.PlayerState;
	URpgIndicatorManagerComponent* IndicatorManager = NewObject<URpgIndicatorManagerComponent>(Controller);
	IndicatorManager->RegisterComponent();
	URpgHarvestAutomationTargetingComponent* Targeting = NewObject<URpgHarvestAutomationTargetingComponent>(Controller);
	Targeting->ConfigureIndicator(TSoftClassPtr<UUserWidget>(UUserWidget::StaticClass()));
	Targeting->ConfigureAreaMarker(TSoftClassPtr<AActor>(ADecalActor::StaticClass()));
	Targeting->RegisterComponent();

	Targeting->RefreshPreview();
	TestEqual(TEXT("An unheld aim ability indicates nothing"), IndicatorManager->GetIndicators().Num(), 0);
	TestNull(TEXT("An unheld aim ability shows no area marker"), Targeting->GetAreaMarkerForTest());

	Targeting->SetAimingAbility(Aimed.Handle, true);
	if (!TestEqual(TEXT("Holding the aim ability indicates every target in the area"), IndicatorManager->GetIndicators().Num(), 2))
	{
		return false;
	}
	ERpgHarvestTargetStatus Status = ERpgHarvestTargetStatus::None;
	int32 Remaining = 0;
	int32 SectionCount = 0;
	int32 ToTake = 0;
	TestTrue(TEXT("The side target is summarized by its component"), Targeting->GetTargetStatus(Side->Collision, Status, Remaining, SectionCount, ToTake));
	TestTrue(TEXT("The side target is harvestable"), Status == ERpgHarvestTargetStatus::Harvestable);
	TestEqual(TEXT("The side target shows the sections the aimed ability takes"), ToTake, 3);
	TestEqual(TEXT("The side target shows its stock"), Remaining, 4);
	TestFalse(
		TEXT("A component outside the preview is not summarized"),
		Targeting->GetTargetStatus(Harvester.Pawn->GetRootComponent(), Status, Remaining, SectionCount, ToTake));

	UIndicatorDescriptor* NearIndicator = nullptr;
	for (UIndicatorDescriptor* Indicator : IndicatorManager->GetIndicators())
	{
		if (Indicator && Indicator->GetSceneComponent() == Near->Collision)
		{
			NearIndicator = Indicator;
		}
	}
	TestNotNull(TEXT("The aimed node has its own indicator"), NearIndicator);
	AActor* AreaMarker = Targeting->GetAreaMarkerForTest();
	if (TestNotNull(TEXT("Holding an area ability shows the area marker"), AreaMarker))
	{
		TestFalse(TEXT("The area marker is visible while aiming"), AreaMarker->IsHidden());
		TestTrue(
			TEXT("The area marker sits at the aim point"),
			AreaMarker->GetActorLocation().Equals(Targeting->GetCurrentPreview().AimPoint, 0.1));
		TestTrue(
			TEXT("The area marker is scaled to the area radius"),
			AreaMarker->GetActorScale3D().Equals(FVector(3.0), KINDA_SMALL_NUMBER));
	}
	Targeting->RefreshPreview();
	TestEqual(TEXT("An unchanged area keeps its indicators"), IndicatorManager->GetIndicators().Num(), 2);
	TestTrue(TEXT("An unchanged target keeps its descriptor"), IndicatorManager->GetIndicators().Contains(NearIndicator));

	Targeting->SetAimingAbility(Aimed.Handle, false);
	TestEqual(TEXT("Releasing the aim removes the area indicators"), IndicatorManager->GetIndicators().Num(), 0);
	TestTrue(TEXT("Releasing the aim hides the area marker"), AreaMarker && AreaMarker->IsHidden());
	TestTrue(TEXT("The hidden marker is kept for the next aim"), Targeting->GetAreaMarkerForTest() == AreaMarker);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestAbilityAreaReachClampTest,
	"SurvivalRpg.Harvesting.Ability.AreaAimStopsAtReach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestAbilityAreaReachClampTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestAbilityTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}

	auto SpawnBlock = [World](const FVector& Location, const FVector& Extent) -> UBoxComponent*
	{
		AActor* Block = World->SpawnActor<AActor>();
		UBoxComponent* Box = Block ? NewObject<UBoxComponent>(Block) : nullptr;
		if (Box)
		{
			Box->InitBoxExtent(Extent);
			Box->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Block->SetRootComponent(Box);
			Box->RegisterComponent();
			Block->SetActorLocation(Location);
		}
		return Box;
	};
	// Ground below, and a wall far beyond the reach that the level view ray hits.
	UBoxComponent* Floor = SpawnBlock(FVector(0.0, 0.0, -150.0), FVector(5000.0, 5000.0, 50.0));
	UBoxComponent* Wall = SpawnBlock(FVector(1500.0, 0.0, 300.0), FVector(50.0, 2000.0, 600.0));
	ARpgHarvestAutomationCollidableNodeActor* Node =
		SpawnNode(World, FVector(550.0, 0.0, -60.0), MakeProfile(World, 4));
	const FGrantedAbility Area = GrantAbility(Harvester.AbilitySystem);
	if (!TestNotNull(TEXT("Floor exists"), Floor) || !TestNotNull(TEXT("Wall exists"), Wall) ||
		!TestNotNull(TEXT("Node exists"), Node) || !TestNotNull(TEXT("Area ability exists"), Area.Instance))
	{
		return false;
	}

	FRpgHarvestTargetingParams Params;
	Params.Shape = ERpgHarvestTargetShape::AreaAtAimPoint;
	Params.MaxAimDistance = 3000.0f;
	Params.MaxReachFromAvatar = 700.0f;
	Params.AreaRadius = 300.0f;
	Params.MaxTargets = 3;
	Area.Instance->ConfigureTargeting(Params);
	const FRpgHarvestPreview Preview = Evaluate(Harvester.AbilitySystem, Area);
	const FVector AvatarLocation = Harvester.Pawn->GetActorLocation();
	TestTrue(
		TEXT("An aim point beyond the reach moves back within it"),
		FVector::Dist(AvatarLocation, Preview.AimPoint) <= Params.MaxReachFromAvatar + 1.0);
	TestTrue(TEXT("The moved aim point lies on the ground"), FMath::IsNearlyEqual(Preview.AimPoint.Z, -100.0, 2.0));
	if (!TestEqual(TEXT("The area at the reach collects the node there"), Preview.Targets.Num(), 1))
	{
		return false;
	}
	TestTrue(TEXT("What the area shows can be harvested"), Preview.Targets[0].bInReach && Preview.Targets[0].WouldHarvest());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestAbilityAreaGroundFallbackTest,
	"SurvivalRpg.Harvesting.Ability.AreaAimFallsBackToGround",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestAbilityAreaGroundFallbackTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestAbilityTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}

	// Open ground below a level view ray: the ray itself hits nothing.
	AActor* Floor = World->SpawnActor<AActor>();
	UBoxComponent* FloorBox = Floor ? NewObject<UBoxComponent>(Floor) : nullptr;
	if (!TestNotNull(TEXT("Floor exists"), FloorBox))
	{
		return false;
	}
	FloorBox->InitBoxExtent(FVector(5000.0, 5000.0, 50.0));
	FloorBox->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Floor->SetRootComponent(FloorBox);
	FloorBox->RegisterComponent();
	Floor->SetActorLocation(FVector(0.0, 0.0, -150.0));
	ARpgHarvestAutomationCollidableNodeActor* Node =
		SpawnNode(World, FVector(600.0, 0.0, -60.0), MakeProfile(World, 4));
	const FGrantedAbility Area = GrantAbility(Harvester.AbilitySystem);
	if (!TestNotNull(TEXT("Ground node exists"), Node) || !TestNotNull(TEXT("Area ability exists"), Area.Instance))
	{
		return false;
	}

	FRpgHarvestTargetingParams Params;
	Params.Shape = ERpgHarvestTargetShape::AreaAtAimPoint;
	Params.MaxAimDistance = 1000.0f;
	Params.MaxReachFromAvatar = 700.0f;
	Params.AreaRadius = 300.0f;
	Params.MaxTargets = 3;
	Area.Instance->ConfigureTargeting(Params);
	const FRpgHarvestPreview Preview = Evaluate(Harvester.AbilitySystem, Area);
	const FVector AvatarLocation = Harvester.Pawn->GetActorLocation();
	TestTrue(TEXT("An unaimed area still reports its area"), Preview.bHasArea);
	TestTrue(TEXT("The aim point drops onto the ground"), FMath::IsNearlyEqual(Preview.AimPoint.Z, -100.0, 2.0));
	TestTrue(
		TEXT("The dropped aim point stays within reach"),
		FVector::Dist(AvatarLocation, Preview.AimPoint) <= Params.MaxReachFromAvatar + 1.0);
	if (!TestEqual(TEXT("The ground area collects the nearby node"), Preview.Targets.Num(), 1))
	{
		return false;
	}
	TestTrue(TEXT("The ground area target is the node"), Preview.Targets[0].Receiver.Get() == Node->HarvestableNode);
	TestTrue(TEXT("The ground area target is in reach"), Preview.Targets[0].bInReach);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestAbilityCommitAimTest,
	"SurvivalRpg.Harvesting.Ability.CommitUsesAimAtExecution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestAbilityCommitAimTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestAbilityTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	ARpgHarvestAutomationCollidableNodeActor* Node =
		SpawnNode(World, FVector(150.0, 0.0, Harvester.Pawn->BaseEyeHeight), MakeProfile(World, 4));
	const FGrantedAbility Granted = GrantAbility(Harvester.AbilitySystem);
	if (!TestNotNull(TEXT("Aim node exists"), Node) || !TestNotNull(TEXT("Aim ability instance exists"), Granted.Instance))
	{
		return false;
	}
	Granted.Instance->ConfigureCommitDelay(0.3f);
	TestWorld.PrimeTimerManager();

	// The bare test pawn has no root; give it one so turning it turns its eyes view.
	USceneComponent* PawnRoot = NewObject<USceneComponent>(Harvester.Pawn);
	Harvester.Pawn->SetRootComponent(PawnRoot);
	PawnRoot->RegisterComponent();
	const FRotator AimRotation = Harvester.Pawn->GetActorRotation();
	const FRotator AwayRotation = AimRotation + FRotator(0.0, 180.0, 0.0);

	// A swing commits on what was aimed at when it started, even if the view turns before the commit time.
	TestTrue(TEXT("The swing activates while aiming at the node"), Harvester.AbilitySystem->TryActivateAbility(Granted.Handle));
	Harvester.Pawn->SetActorRotation(AwayRotation);
	TestWorld.AdvanceTimers(0.4f);
	TestEqual(TEXT("The swing harvests the node it started on"), Node->HarvestableNode->GetRemainingSections(), 3);

	// A held aim commits on what was aimed at when the input was released.
	Harvester.Pawn->SetActorRotation(AimRotation);
	Granted.Instance->ConfigureAimWhileInputHeld(true);
	FGameplayAbilitySpec* Spec = Harvester.AbilitySystem->FindAbilitySpecFromHandle(Granted.Handle);
	if (!TestNotNull(TEXT("Aim ability spec exists"), Spec))
	{
		return false;
	}
	Spec->InputPressed = true;
	TestTrue(TEXT("The held aim activates"), Harvester.AbilitySystem->TryActivateAbility(Granted.Handle));
	static_cast<UAbilitySystemComponent*>(Harvester.AbilitySystem)->AbilitySpecInputReleased(*Spec);
	Harvester.Pawn->SetActorRotation(AwayRotation);
	TestWorld.AdvanceTimers(0.4f);
	TestEqual(TEXT("The released aim harvests the node it was aimed at"), Node->HarvestableNode->GetRemainingSections(), 2);

	// Turning away before the release aims elsewhere: the commit follows the aim at release.
	Spec = Harvester.AbilitySystem->FindAbilitySpecFromHandle(Granted.Handle);
	Spec->InputPressed = true;
	Harvester.Pawn->SetActorRotation(AimRotation);
	TestTrue(TEXT("The held aim activates again"), Harvester.AbilitySystem->TryActivateAbility(Granted.Handle));
	Harvester.Pawn->SetActorRotation(AwayRotation);
	static_cast<UAbilitySystemComponent*>(Harvester.AbilitySystem)->AbilitySpecInputReleased(*Spec);
	Harvester.Pawn->SetActorRotation(AimRotation);
	TestWorld.AdvanceTimers(0.4f);
	TestEqual(TEXT("An aim released while looking away harvests nothing"), Node->HarvestableNode->GetRemainingSections(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestAbilityWeakPointTest,
	"SurvivalRpg.Harvesting.Ability.SwingStrikesWeakPoint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestAbilityWeakPointTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestAbilityTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	URpgHarvestProfile* Profile = MakeProfile(World, 4);
	Profile->WeakPointBonusSections = 1;
	ARpgHarvestAutomationCollidableNodeActor* Node =
		SpawnNode(World, FVector(150.0, 0.0, Harvester.Pawn->BaseEyeHeight), Profile);
	const FGrantedAbility Granted = GrantAbility(Harvester.AbilitySystem);
	// The view ray strikes the node's front face at its center, 40 cm in front of the actor location.
	if (!TestNotNull(TEXT("Weak point node exists"), Node) ||
		!TestNotNull(TEXT("Granted test ability instance exists"), Granted.Instance) ||
		!TestTrue(
			TEXT("Weak points are configured"),
			RpgHarvestAutomation::ConfigureWeakPoints(
				Node->HarvestableNode,
				{FVector(-40.0, 0.0, 0.0), FVector(-40.0, 0.0, 60.0), FVector(-40.0, 0.0, -60.0)},
				20.0f)))
	{
		return false;
	}

	FRpgHarvestTargetingParams Single;
	Single.MaxAimDistance = 1000.0f;
	Single.MaxReachFromAvatar = 250.0f;
	Granted.Instance->ConfigureTargeting(Single);

	FRpgHarvestPreview Preview = Evaluate(Harvester.AbilitySystem, Granted);
	TestTrue(
		TEXT("An ability that cannot hit weak points previews its sections only"),
		Preview.Targets.Num() == 1 && Preview.Targets[0].Result.SectionsTaken == 1 && !Preview.Targets[0].Result.bWeakPointHit);

	Granted.Instance->ConfigureWeakPointHits(true);
	Preview = Evaluate(Harvester.AbilitySystem, Granted);
	TestTrue(
		TEXT("A swing aimed at the weak point previews the bonus section"),
		Preview.Targets.Num() == 1 && Preview.Targets[0].Result.SectionsTaken == 2 && Preview.Targets[0].Result.bWeakPointHit);

	TestTrue(TEXT("The swing activates"), Harvester.AbilitySystem->TryActivateAbility(Granted.Handle));
	TestEqual(TEXT("The swing extracts the bonus section"), Node->HarvestableNode->GetRemainingSections(), 2);
	TestEqual(TEXT("Both sections are rewarded"), CountMaterial(Harvester.PlayerState), 2 * YieldPerSection);

	Preview = Evaluate(Harvester.AbilitySystem, Granted);
	TestTrue(
		TEXT("The weak point moved away from the aim"),
		Preview.Targets.Num() == 1 && Preview.Targets[0].Result.SectionsTaken == 1 && !Preview.Targets[0].Result.bWeakPointHit);

	// An area power selects the node through its actor location; a weak point exactly there must not count.
	RpgHarvestAutomation::ConfigureWeakPoints(Node->HarvestableNode, {FVector::ZeroVector}, 20.0f);
	FRpgHarvestTargetingParams Area;
	Area.Shape = ERpgHarvestTargetShape::AreaAtAimPoint;
	Area.MaxAimDistance = 1000.0f;
	Area.MaxReachFromAvatar = 700.0f;
	Area.AreaRadius = 300.0f;
	Area.MaxTargets = 1;
	Granted.Instance->ConfigureTargeting(Area);
	Preview = Evaluate(Harvester.AbilitySystem, Granted);
	TestTrue(
		TEXT("Area powers never strike weak points"),
		Preview.Targets.Num() == 1 && Preview.Targets[0].Result.SectionsTaken == 1 && !Preview.Targets[0].Result.bWeakPointHit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestAbilityInstancesTest,
	"SurvivalRpg.Harvesting.Ability.InstancedTargetsAndIndicators",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestAbilityInstancesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestAbilityTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	const float EyeHeight = Harvester.Pawn->BaseEyeHeight;
	URpgHarvestInstanceStockComponent* Stock = RpgHarvestAutomation::AddInstanceStock(World);
	// Three 100 cm cubes in one instanced component, like one PCG mesh entry in one partition cell.
	ARpgHarvestAutomationInstancesActor* Field = RpgHarvestAutomation::SpawnInstances(
		World,
		MakeProfile(World, 4),
		{FVector(0.0, 0.0, 0.0), FVector(0.0, 160.0, 0.0), FVector(0.0, -160.0, 0.0)},
		FVector(150.0, 0.0, EyeHeight));
	const FGrantedAbility Swing = GrantAbility(Harvester.AbilitySystem);
	const FGrantedAbility Area = GrantAbility(Harvester.AbilitySystem);
	if (!TestNotNull(TEXT("Instance stock exists"), Stock) ||
		!TestNotNull(TEXT("Instanced field exists"), Field) ||
		!TestNotNull(TEXT("Swing exists"), Swing.Instance) ||
		!TestNotNull(TEXT("Area ability exists"), Area.Instance))
	{
		return false;
	}
	URpgHarvestableInstancesComponent* Instances = Field->Instances;

	FRpgHarvestTargetingParams Single;
	Single.MaxAimDistance = 1000.0f;
	Single.MaxReachFromAvatar = 250.0f;
	Swing.Instance->ConfigureTargeting(Single);
	FRpgHarvestPreview Preview = Evaluate(Harvester.AbilitySystem, Swing);
	if (!TestEqual(TEXT("The swing selects one instance"), Preview.Targets.Num(), 1))
	{
		return false;
	}
	TestTrue(TEXT("The swing targets the instanced component"), Preview.Targets[0].Receiver.Get() == Instances);
	TestEqual(TEXT("The swing targets the instance in its view"), Preview.Targets[0].Hit.Item, 0);
	TestTrue(
		TEXT("The swing previews one section of the instance's stock"),
		Preview.Targets[0].WouldHarvest() && Preview.Targets[0].Result.SectionsTaken == 1 &&
			Preview.Targets[0].Result.RemainingSections == 3);
	TestTrue(TEXT("The swing activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TestEqual(TEXT("The swing extracts one section"), Instances->GetRemainingSections(0), 3);
	TestEqual(TEXT("The swing rewards one section"), CountMaterial(Harvester.PlayerState), YieldPerSection);

	FRpgHarvestTargetingParams AreaTargeting;
	AreaTargeting.Shape = ERpgHarvestTargetShape::AreaAtAimPoint;
	AreaTargeting.MaxAimDistance = 1000.0f;
	AreaTargeting.MaxReachFromAvatar = 700.0f;
	AreaTargeting.AreaRadius = 300.0f;
	AreaTargeting.MaxTargets = 3;
	Area.Instance->ConfigureTargeting(AreaTargeting);
	Area.Instance->ConfigureSections(3);
	Area.Instance->ConfigureAimWhileInputHeld(true);

	FActorSpawnParameters ControllerParameters;
	ControllerParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APlayerController* Controller = World->SpawnActor<APlayerController>(
		APlayerController::StaticClass(),
		FTransform::Identity,
		ControllerParameters);
	if (!TestNotNull(TEXT("Local controller exists"), Controller))
	{
		return false;
	}
	Controller->SetAsLocalPlayerController();
	Controller->PlayerState = Harvester.PlayerState;
	URpgIndicatorManagerComponent* IndicatorManager = NewObject<URpgIndicatorManagerComponent>(Controller);
	IndicatorManager->RegisterComponent();
	URpgHarvestAutomationTargetingComponent* Targeting = NewObject<URpgHarvestAutomationTargetingComponent>(Controller);
	Targeting->ConfigureIndicator(TSoftClassPtr<UUserWidget>(UUserWidget::StaticClass()));
	Targeting->RegisterComponent();

	Targeting->SetAimingAbility(Area.Handle, true);
	Preview = Targeting->GetCurrentPreview();
	if (!TestEqual(TEXT("The area selects every instance in range"), Preview.Targets.Num(), 3) ||
		!TestEqual(TEXT("Every instance gets its own indicator"), IndicatorManager->GetIndicators().Num(), 3))
	{
		return false;
	}
	TSet<int32> PreviewedInstances;
	for (const FRpgHarvestTargetEvaluation& Target : Preview.Targets)
	{
		PreviewedInstances.Add(Target.Hit.Item);
	}
	TestEqual(TEXT("Instances of one component stay distinct targets"), PreviewedInstances.Num(), 3);

	TSet<int32> IndicatedInstances;
	for (UIndicatorDescriptor* Indicator : IndicatorManager->GetIndicators())
	{
		const FRpgHarvestTargetEvaluation* Target = Targeting->FindIndicatedTarget(Indicator);
		FTransform InstanceTransform;
		if (!TestNotNull(TEXT("Each indicator resolves its target"), Target) ||
			!TestTrue(TEXT("Each indicator anchors to the instanced component"), Indicator->GetSceneComponent() == Instances) ||
			!TestTrue(TEXT("Each indicated target is an instance"), Instances->GetInstanceTransform(Target->Hit.Item, InstanceTransform, true)))
		{
			continue;
		}
		IndicatedInstances.Add(Target->Hit.Item);
		TestTrue(TEXT("Instance indicators project from a point"), Indicator->GetProjectionMode() == EActorCanvasProjectionMode::ComponentPoint);
		TestTrue(
			TEXT("Each indicator marks the top of its own instance"),
			Indicator->HasWorldPositionOverride() &&
				Indicator->GetWorldPositionOverride().Equals(InstanceTransform.GetLocation() + FVector(0.0, 0.0, 50.0), 0.5));

		ERpgHarvestTargetStatus Status = ERpgHarvestTargetStatus::None;
		int32 Remaining = 0;
		int32 SectionCount = 0;
		int32 ToTake = 0;
		TestTrue(
			TEXT("Each indicator summarizes its own instance"),
			Targeting->GetIndicatedTargetStatus(Indicator, Status, Remaining, SectionCount, ToTake) &&
				Status == ERpgHarvestTargetStatus::Harvestable &&
				Remaining == Instances->GetRemainingSections(Target->Hit.Item) &&
				ToTake == FMath::Min(3, Remaining));
	}
	TestEqual(TEXT("The indicators mark three different instances"), IndicatedInstances.Num(), 3);

	Targeting->SetAimingAbility(Area.Handle, false);
	TestEqual(TEXT("Releasing the aim removes the instance indicators"), IndicatorManager->GetIndicators().Num(), 0);

	Area.Instance->ConfigureAimWhileInputHeld(false);
	TestTrue(TEXT("The area ability activates"), Harvester.AbilitySystem->TryActivateAbility(Area.Handle));
	TestEqual(TEXT("The area takes the swung instance's last sections"), Instances->GetRemainingSections(0), 0);
	TestEqual(TEXT("The area takes three sections from a side instance"), Instances->GetRemainingSections(1), 1);
	TestEqual(TEXT("The area takes three sections from the other side instance"), Instances->GetRemainingSections(2), 1);
	TestEqual(TEXT("Every harvested instance is stored"), Stock->GetNumChangedInstances(), 3);
	TestEqual(TEXT("Every extracted section is rewarded"), CountMaterial(Harvester.PlayerState), 10 * YieldPerSection);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestAbilityAreaDeliveryTest,
	"SurvivalRpg.Harvesting.Ability.AreaRewardsArriveAsOneDelivery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestAbilityAreaDeliveryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestAbilityTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	FHarvesterFixture FullHarvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixtures exist"), Harvester.IsValid() && FullHarvester.IsValid()))
	{
		return false;
	}
	const float EyeHeight = Harvester.Pawn->BaseEyeHeight;

	// Three resources around the aim point and an area power that empties each of them, like Death Wave.
	auto SpawnField = [World, EyeHeight]()
	{
		return TArray<ARpgHarvestAutomationCollidableNodeActor*>{
			SpawnNode(World, FVector(400.0, 0.0, EyeHeight), MakeProfile(World, 4)),
			SpawnNode(World, FVector(400.0, 160.0, EyeHeight), MakeProfile(World, 4)),
			SpawnNode(World, FVector(400.0, -160.0, EyeHeight), MakeProfile(World, 4))};
	};
	FRpgHarvestTargetingParams AreaTargeting;
	AreaTargeting.Shape = ERpgHarvestTargetShape::AreaAtAimPoint;
	AreaTargeting.MaxAimDistance = 1000.0f;
	AreaTargeting.MaxReachFromAvatar = 700.0f;
	AreaTargeting.AreaRadius = 300.0f;
	AreaTargeting.MaxTargets = 3;
	const FGrantedAbility Area = GrantAbility(Harvester.AbilitySystem);
	const FGrantedAbility FullArea = GrantAbility(FullHarvester.AbilitySystem);
	if (!TestNotNull(TEXT("Area ability exists"), Area.Instance) ||
		!TestNotNull(TEXT("Full-inventory area ability exists"), FullArea.Instance))
	{
		return false;
	}
	for (const FGrantedAbility* Granted : {&Area, &FullArea})
	{
		Granted->Instance->ConfigureTargeting(AreaTargeting);
		Granted->Instance->ConfigureSections(URpgHarvestProfile::MaxSectionCount);
	}

	TArray<ARpgHarvestAutomationCollidableNodeActor*> Field = SpawnField();
	TestTrue(TEXT("The area harvest activates"), Harvester.AbilitySystem->TryActivateAbility(Area.Handle));
	for (const ARpgHarvestAutomationCollidableNodeActor* Node : Field)
	{
		TestFalse(TEXT("The area empties every resource completely"), Node && Node->HarvestableNode->IsHarvestable());
	}
	TestEqual(TEXT("The complete stock of every resource reaches the inventory"), CountMaterial(Harvester.PlayerState), 3 * 4 * YieldPerSection);
	TestEqual(TEXT("A fitting area reward spawns no drop"), GetWorldDrops(World).Num(), 0);

	for (ARpgHarvestAutomationCollidableNodeActor* Node : Field)
	{
		Node->Destroy();
	}
	Field = SpawnField();
	URpgInventoryManagerComponent* FullInventory = FullHarvester.PlayerState->GetInventoryManagerComponent();
	FullInventory->SetFixedMaxEntries(0);
	FullInventory->SetCapacityMode(ERpgInventoryCapacityMode::FixedEntries);
	TestTrue(TEXT("The area harvest activates with a full inventory"), FullHarvester.AbilitySystem->TryActivateAbility(FullArea.Handle));
	for (const ARpgHarvestAutomationCollidableNodeActor* Node : Field)
	{
		TestFalse(TEXT("A full inventory still empties every resource"), Node && Node->HarvestableNode->IsHarvestable());
	}
	const TArray<ARpgDroppedInventoryActor*> Drops = GetWorldDrops(World);
	if (!TestEqual(TEXT("The overflow of every target arrives in exactly one drop"), Drops.Num(), 1))
	{
		return false;
	}
	const URpgInventoryManagerComponent* DropInventory = Drops[0]->GetLootInventoryManager();
	TestTrue(
		TEXT("The single drop holds the complete area reward"),
		DropInventory &&
			DropInventory->GetTotalItemCountByDefinition(URpgHarvestAutomationTestStackItemDefinition::StaticClass()) ==
				3 * 4 * YieldPerSection);
	TestTrue(
		TEXT("An area drop lands at the harvester"),
		FVector::Dist2D(Drops[0]->GetActorLocation(), FullHarvester.Pawn->GetActorLocation()) < 100.0);
	TestEqual(TEXT("Nothing reaches the full inventory"), CountMaterial(FullHarvester.PlayerState), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestProtectionTest,
	"SurvivalRpg.Harvesting.Protection.AreaHarvestSkipsProtectedResources",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestProtectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestAbilityTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	const float EyeHeight = Harvester.Pawn->BaseEyeHeight;
	ARpgHarvestAutomationCollidableNodeActor* Aimed = SpawnNode(World, FVector(400.0, 0.0, EyeHeight), MakeProfile(World, 4));
	ARpgHarvestAutomationCollidableNodeActor* Side = SpawnNode(World, FVector(400.0, 160.0, EyeHeight), MakeProfile(World, 4));
	ARpgHarvestAutomationCollidableNodeActor* Far = SpawnNode(World, FVector(400.0, -200.0, EyeHeight), MakeProfile(World, 4));

	// A camp protects the aimed resource; its box does not reach the others.
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgHarvestAutomationProtectionActor* Camp = World->SpawnActor<ARpgHarvestAutomationProtectionActor>(
		ARpgHarvestAutomationProtectionActor::StaticClass(),
		FTransform(FVector(400.0, 0.0, EyeHeight)),
		SpawnParameters);
	if (!TestNotNull(TEXT("Aimed resource exists"), Aimed) ||
		!TestNotNull(TEXT("Side resource exists"), Side) ||
		!TestNotNull(TEXT("Far resource exists"), Far) ||
		!TestNotNull(TEXT("Protection camp exists"), Camp))
	{
		return false;
	}
	Camp->Protection->SetBoxExtent(FVector(80.0, 80.0, 200.0));
	if (!Camp->HasActorBegunPlay())
	{
		Camp->DispatchBeginPlay();
	}
	const URpgHarvestProtectionSubsystem* Protection = World->GetSubsystem<URpgHarvestProtectionSubsystem>();
	if (!TestNotNull(TEXT("The world has a protection registry"), Protection))
	{
		return false;
	}
	TestEqual(TEXT("The camp registers its protection box"), Protection->GetNumProtectionZones(), 1);
	TestTrue(TEXT("The box protects the aimed resource"), Protection->IsProtected(Aimed->GetActorLocation()));
	TestFalse(TEXT("The box leaves the side resource unprotected"), Protection->IsProtected(Side->GetActorLocation()));

	const FGrantedAbility Area = GrantAbility(Harvester.AbilitySystem);
	const FGrantedAbility Swing = GrantAbility(Harvester.AbilitySystem);
	if (!TestNotNull(TEXT("Area ability exists"), Area.Instance) || !TestNotNull(TEXT("Swing exists"), Swing.Instance))
	{
		return false;
	}
	FRpgHarvestTargetingParams AreaTargeting;
	AreaTargeting.Shape = ERpgHarvestTargetShape::AreaAtAimPoint;
	AreaTargeting.MaxAimDistance = 1000.0f;
	AreaTargeting.MaxReachFromAvatar = 700.0f;
	AreaTargeting.AreaRadius = 300.0f;
	AreaTargeting.MaxTargets = 2;
	Area.Instance->ConfigureTargeting(AreaTargeting);

	FRpgHarvestPreview Preview = Evaluate(Harvester.AbilitySystem, Area);
	int32 WouldHarvest = 0;
	const FRpgHarvestTargetEvaluation* AimedTarget = nullptr;
	for (const FRpgHarvestTargetEvaluation& Target : Preview.Targets)
	{
		WouldHarvest += Target.WouldHarvest() ? 1 : 0;
		AimedTarget = Target.Receiver.Get() == Aimed->HarvestableNode ? &Target : AimedTarget;
	}
	TestEqual(TEXT("The preview lists the protected resource and two harvestable ones"), Preview.Targets.Num(), 3);
	TestEqual(TEXT("The protected resource does not use up a target slot"), WouldHarvest, 2);
	TestTrue(
		TEXT("The preview reports the protected resource as protected"),
		AimedTarget && AimedTarget->Result.Outcome == ERpgHarvestOutcome::Protected && !AimedTarget->WouldHarvest());

	TestTrue(TEXT("The area harvest activates"), Harvester.AbilitySystem->TryActivateAbility(Area.Handle));
	TestEqual(TEXT("The area skips the protected resource"), Aimed->HarvestableNode->GetRemainingSections(), 4);
	TestEqual(TEXT("The area harvests the side resource"), Side->HarvestableNode->GetRemainingSections(), 3);
	TestEqual(TEXT("The area harvests the far resource"), Far->HarvestableNode->GetRemainingSections(), 3);

	FRpgHarvestTargetingParams Single;
	Single.MaxAimDistance = 1000.0f;
	Single.MaxReachFromAvatar = 500.0f;
	Swing.Instance->ConfigureTargeting(Single);
	TestTrue(TEXT("A deliberate swing activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TestEqual(TEXT("A deliberate swing still harvests a protected resource"), Aimed->HarvestableNode->GetRemainingSections(), 3);

	// Instanced resources check the same boxes at their authored locations.
	RpgHarvestAutomation::AddInstanceStock(World);
	ARpgHarvestAutomationInstancesActor* Instances = RpgHarvestAutomation::SpawnInstances(
		World,
		MakeProfile(World, 4),
		{FVector::ZeroVector, FVector(0.0, 300.0, 0.0)},
		FVector(400.0, 2000.0, 0.0));
	ARpgHarvestAutomationProtectionActor* Grove = World->SpawnActor<ARpgHarvestAutomationProtectionActor>(
		ARpgHarvestAutomationProtectionActor::StaticClass(),
		FTransform(FVector(400.0, 2000.0, 0.0)),
		SpawnParameters);
	if (!TestNotNull(TEXT("Instanced resources exist"), Instances) || !TestNotNull(TEXT("Grove protection exists"), Grove))
	{
		return false;
	}
	Grove->Protection->SetBoxExtent(FVector(100.0, 100.0, 100.0));
	if (!Grove->HasActorBegunPlay())
	{
		Grove->DispatchBeginPlay();
	}
	FRpgHarvestRequest InstanceRequest =
		RpgHarvestAutomation::MakeInstanceRequest(Instances->Instances, 0, Harvester.Pawn);
	InstanceRequest.bAreaHarvest = true;
	TestTrue(
		TEXT("An area request rejects a protected instance"),
		IRpgHarvestableTarget::Execute_EvaluateHarvest(Instances->Instances, InstanceRequest).Outcome == ERpgHarvestOutcome::Protected);
	FRpgHarvestRequest OutsideRequest =
		RpgHarvestAutomation::MakeInstanceRequest(Instances->Instances, 1, Harvester.Pawn);
	OutsideRequest.bAreaHarvest = true;
	TestTrue(
		TEXT("An area request harvests an instance outside the box"),
		IRpgHarvestableTarget::Execute_EvaluateHarvest(Instances->Instances, OutsideRequest).IsSuccess());
	InstanceRequest.bAreaHarvest = false;
	TestTrue(
		TEXT("A single-target request harvests a protected instance"),
		IRpgHarvestableTarget::Execute_EvaluateHarvest(Instances->Instances, InstanceRequest).IsSuccess());

	// Removing the camp's protection, for example when it is torn down, ends its protection.
	Camp->Protection->DestroyComponent();
	TestEqual(TEXT("A removed protection box unregisters"), Protection->GetNumProtectionZones(), 1);
	Preview = Evaluate(Harvester.AbilitySystem, Area);
	TestTrue(
		TEXT("Without the camp the area harvests the formerly protected resource"),
		Preview.Targets.Num() == 2 && Preview.Targets[0].Receiver.Get() == Aimed->HarvestableNode &&
			Preview.Targets[0].WouldHarvest());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestAbilityPresentationWaveTest,
	"SurvivalRpg.Harvesting.Ability.AreaPresentationTravelsAsWave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestAbilityPresentationWaveTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestAbilityTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	const float EyeHeight = Harvester.Pawn->BaseEyeHeight;
	URpgHarvestInstanceStockComponent* Stock = RpgHarvestAutomation::AddInstanceStock(World);
	// Three trees 300, 424 and 600 cm from the harvester inside one area, and one far outside it.
	ARpgHarvestAutomationInstancesActor* Field = RpgHarvestAutomation::SpawnInstances(
		World,
		MakeProfile(World, 4),
		{FVector(0.0, 0.0, 0.0), FVector(0.0, 300.0, 0.0), FVector(300.0, 0.0, 0.0), FVector(0.0, -1500.0, 0.0)},
		FVector(300.0, 0.0, EyeHeight));
	const FGrantedAbility Wave = GrantAbility(Harvester.AbilitySystem);
	if (!TestNotNull(TEXT("Instance stock exists"), Stock) ||
		!TestNotNull(TEXT("Instanced field exists"), Field) ||
		!TestNotNull(TEXT("Wave ability exists"), Wave.Instance))
	{
		return false;
	}
	FRpgHarvestTargetingParams AreaTargeting;
	AreaTargeting.Shape = ERpgHarvestTargetShape::AreaAtAimPoint;
	AreaTargeting.MaxAimDistance = 1000.0f;
	AreaTargeting.MaxReachFromAvatar = 700.0f;
	AreaTargeting.AreaRadius = 450.0f;
	AreaTargeting.MaxTargets = 3;
	Wave.Instance->ConfigureTargeting(AreaTargeting);
	Wave.Instance->ConfigureSections(URpgHarvestProfile::MaxSectionCount);
	Wave.Instance->ConfigurePresentationWave(500.0f);
	TestWorld.PrimeTimerManager();

	URpgHarvestAutomationInstancesComponent* Instances = Field->Instances;
	auto PresentedScale = [Instances](const int32 InstanceIndex)
	{
		FTransform Transform;
		return Instances->GetInstanceTransform(InstanceIndex, Transform, false) ? Transform.GetScale3D().X : -1.0;
	};

	TestTrue(TEXT("The wave activates"), Harvester.AbilitySystem->TryActivateAbility(Wave.Handle));
	for (int32 InstanceIndex = 0; InstanceIndex < 3; ++InstanceIndex)
	{
		TestEqual(TEXT("Every tree loses its stock at once"), Instances->GetRemainingSections(InstanceIndex), 0);
	}
	TestEqual(TEXT("Every tree's wood is delivered at once"), CountMaterial(Harvester.PlayerState), 3 * 4 * YieldPerSection);
	TestTrue(TEXT("The nearest tree falls at once"), FMath::IsNearlyZero(PresentedScale(0)));
	TestTrue(TEXT("The side tree still stands"), FMath::IsNearlyEqual(PresentedScale(1), 1.0));
	FVector SideDirection;
	TestTrue(
		TEXT("Every machine reads the side tree's direction away from the harvester"),
		Instances->GetInstanceHarvestDirection(1, SideDirection) &&
			SideDirection.Equals(FVector(UE_INV_SQRT_2, UE_INV_SQRT_2, 0.0), 0.03));
	FVector UntouchedDirection;
	TestFalse(TEXT("An untouched tree has no harvest direction"), Instances->GetInstanceHarvestDirection(3, UntouchedDirection));
	TestTrue(TEXT("The far tree still stands"), FMath::IsNearlyEqual(PresentedScale(2), 1.0));

	FIntVector FarKey;
	Instances->GetInstanceKey(2, FarKey);
	TestTrue(
		TEXT("The far tree waits for its distance at the wave speed"),
		FMath::IsNearlyEqual(Stock->GetRemainingPresentationDelay(FarKey), 0.6f, 0.02f));

	TestWorld.AdvanceTimers(0.3f);
	TestTrue(TEXT("The side tree falls once the wave reaches it"), FMath::IsNearlyZero(PresentedScale(1)));
	TestTrue(TEXT("The far tree still waits"), FMath::IsNearlyEqual(PresentedScale(2), 1.0));
	TestTrue(
		TEXT("Elapsed server time counts against the delay"),
		FMath::IsNearlyEqual(Stock->GetRemainingPresentationDelay(FarKey), 0.3f, 0.02f));

	TestWorld.AdvanceTimers(0.35f);
	TestTrue(TEXT("The far tree falls last"), FMath::IsNearlyZero(PresentedScale(2)));
	TestFalse(TEXT("A delayed fall is presented as a live change"), Instances->bLastInitialState);
	TestEqual(TEXT("Each tree is presented exactly once"), Instances->EventCount, 3);
	return true;
}

namespace RpgHarvestSwarmTests
{
	using namespace RpgHarvestAbilityTests;

	/** Grants a swarm ability whose CreatureCount creatures clear up to three resources within 450 cm of the aim point, two sections per strike. */
	FGrantedAbility GrantSwarmAbility(
		URpgAbilitySystemComponent* AbilitySystem,
		const int32 CreatureCount,
		UObject* SourceObject = nullptr)
	{
		FGrantedAbility Granted = GrantAbility(AbilitySystem, SourceObject);
		if (!Granted.Instance)
		{
			return Granted;
		}
		FRpgHarvestTargetingParams AreaTargeting;
		AreaTargeting.Shape = ERpgHarvestTargetShape::AreaAtAimPoint;
		AreaTargeting.MaxAimDistance = 1000.0f;
		AreaTargeting.MaxReachFromAvatar = 700.0f;
		AreaTargeting.AreaRadius = 450.0f;
		AreaTargeting.MaxTargets = 3;
		Granted.Instance->ConfigureTargeting(AreaTargeting);
		Granted.Instance->ConfigureSections(2);

		FRpgHarvestSwarmParams Swarm;
		Swarm.CreatureCount = CreatureCount;
		Swarm.FlightSpeed = 1000.0f;
		Swarm.EmergeSeconds = 0.5f;
		Swarm.LaunchIntervalSeconds = 0.1f;
		Swarm.StrikeIntervalSeconds = 0.3f;
		Swarm.MaxReassignments = 2;
		Swarm.MaxLifetimeSeconds = 8.0f;
		Granted.Instance->ConfigureSwarm(ARpgHarvestSwarm::StaticClass(), Swarm);
		return Granted;
	}

	/** Three trees 50, 304 and 350 cm from the aim point on the first one, as in the presentation wave test. */
	ARpgHarvestAutomationInstancesActor* SpawnGrove(UWorld* World, const float EyeHeight)
	{
		return RpgHarvestAutomation::SpawnInstances(
			World,
			MakeProfile(World, 4),
			{FVector(0.0, 0.0, 0.0), FVector(0.0, 300.0, 0.0), FVector(300.0, 0.0, 0.0)},
			FVector(300.0, 0.0, EyeHeight));
	}

	ARpgHarvestSwarm* FindActiveSwarm(UWorld* World)
	{
		for (TActorIterator<ARpgHarvestSwarm> It(World); It; ++It)
		{
			if (IsValid(*It) && !It->IsActorBeingDestroyed() && !It->IsFinished())
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** Advances the test world in frame-sized steps, so the swarm handles departures and arrivals in order. */
	void Advance(const RpgHarvestAutomation::FScopedTestWorld& TestWorld, const double Seconds)
	{
		constexpr double FrameSeconds = 0.02;
		for (double Elapsed = 0.0; Elapsed < Seconds - UE_KINDA_SMALL_NUMBER; Elapsed += FrameSeconds)
		{
			TestWorld.AdvanceTimers(static_cast<float>(FMath::Min(FrameSeconds, Seconds - Elapsed)));
		}
	}

	FRpgHarvestTargetEvaluation MakePlannedTarget(const int32 Available, const bool bHarvestable = true, const bool bInReach = true)
	{
		FRpgHarvestTargetEvaluation Target;
		Target.bInReach = bInReach;
		Target.Result.Outcome = bHarvestable ? ERpgHarvestOutcome::Harvested : ERpgHarvestOutcome::Depleted;
		Target.Result.SectionsTaken = bHarvestable ? FMath::Min(2, Available) : 0;
		Target.Result.RemainingSections = Available - Target.Result.SectionsTaken;
		return Target;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestSwarmPlannerTest,
	"SurvivalRpg.Harvesting.Swarm.PlannerSharesStockWithoutOverbooking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestSwarmPlannerTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestSwarmTests;

	// A large resource, a nearly empty one, a depleted one, and one out of reach.
	const TArray<FRpgHarvestTargetEvaluation> Targets = {
		MakePlannedTarget(4),
		MakePlannedTarget(1),
		MakePlannedTarget(0, false),
		MakePlannedTarget(3, true, false)};
	TArray<int32> TargetIndices;
	TArray<int32> Sections;

	FRpgHarvestSwarmPlanner::Distribute(Targets, 5, 2, TargetIndices, Sections);
	TestTrue(
		TEXT("Creatures cover the nearest resource's stock before the next resource"),
		TargetIndices == TArray<int32>({0, 0, 1}));
	TestTrue(TEXT("No creature reserves stock another one already holds"), Sections == TArray<int32>({2, 2, 1}));

	FRpgHarvestSwarmPlanner::Distribute(Targets, 1, 2, TargetIndices, Sections);
	TestTrue(
		TEXT("A single creature takes the nearest resource"),
		TargetIndices == TArray<int32>({0}) && Sections == TArray<int32>({2}));

	FRpgHarvestSwarmPlanner::Distribute(Targets, 8, 3, TargetIndices, Sections);
	TestTrue(
		TEXT("Creatures share a resource only for the stock left"),
		TargetIndices == TArray<int32>({0, 0, 1}) && Sections == TArray<int32>({3, 1, 1}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestSwarmArrivalTest,
	"SurvivalRpg.Harvesting.Swarm.CreaturesHarvestOnArrivalForTheSummoner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestSwarmArrivalTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestSwarmTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	URpgHarvestInstanceStockComponent* Stock = RpgHarvestAutomation::AddInstanceStock(World);
	ARpgHarvestAutomationInstancesActor* Grove = SpawnGrove(World, Harvester.Pawn->BaseEyeHeight);
	const FGrantedAbility Summon = GrantSwarmAbility(Harvester.AbilitySystem, 4);
	if (!TestNotNull(TEXT("Instance stock exists"), Stock) ||
		!TestNotNull(TEXT("Grove exists"), Grove) ||
		!TestNotNull(TEXT("Swarm ability exists"), Summon.Instance))
	{
		return false;
	}
	TestWorld.PrimeTimerManager();
	URpgHarvestAutomationInstancesComponent* Trees = Grove->Instances;

	// Four creatures with two sections per strike work through all three trees until they are empty.
	const FRpgHarvestPreview Preview = Evaluate(Harvester.AbilitySystem, Summon);
	TArray<int32> PreviewSections;
	for (const FRpgHarvestTargetEvaluation& Target : Preview.Targets)
	{
		PreviewSections.Add(Target.WouldHarvest() ? Target.Result.SectionsTaken : 0);
	}
	TestTrue(TEXT("The preview marks the whole stock of every tree in the area"), PreviewSections == TArray<int32>({4, 4, 4}));

	TestTrue(TEXT("The swarm is summoned"), Harvester.AbilitySystem->TryActivateAbility(Summon.Handle));
	ARpgHarvestSwarm* Swarm = FindActiveSwarm(World);
	if (!TestNotNull(TEXT("The commit summons a swarm"), Swarm))
	{
		return false;
	}
	TestEqual(TEXT("The swarm has every creature"), Swarm->GetCreatures().Num(), 4);
	for (int32 TreeIndex = 0; TreeIndex < 3; ++TreeIndex)
	{
		TestEqual(TEXT("Nothing is harvested at the summon"), Trees->GetRemainingSections(TreeIndex), 4);
	}
	TestTrue(TEXT("The player state is the beneficiary"), Swarm->GetBeneficiary() == Harvester.PlayerState);

	// Switching tools removes the ability; the summoned swarm keeps working.
	Harvester.AbilitySystem->ClearAbility(Summon.Handle);

	const double FirstArrival = Swarm->GetCreatures()[0].ArrivalServerTime;
	Advance(TestWorld, FirstArrival - Swarm->GetServerWorldTimeSeconds() - 0.03);
	TestEqual(TEXT("A creature harvests only when it arrives"), Trees->GetRemainingSections(0), 4);
	Advance(TestWorld, 0.06);
	TestEqual(TEXT("The first creature takes its sections on arrival"), Trees->GetRemainingSections(0), 2);
	TestTrue(
		TEXT("The first creature struck and keeps working"),
		Swarm->GetCreatures()[0].Strikes == 1 && Swarm->GetCreatures()[0].SectionsTaken == 2 &&
			Swarm->GetCreatures()[0].State == ERpgHarvestSwarmCreatureState::Flying);
	TestEqual(TEXT("The rewards wait for the whole swarm"), CountMaterial(Harvester.PlayerState), 0);
	TestEqual(TEXT("The far tree is still standing while the first trees are worked"), Trees->GetRemainingSections(2), 4);

	Advance(TestWorld, 5.0);
	TestTrue(TEXT("Every creature finished"), Swarm->IsFinished());
	for (int32 TreeIndex = 0; TreeIndex < 3; ++TreeIndex)
	{
		TestEqual(TEXT("The swarm fells every tree it selected, one after another"), Trees->GetRemainingSections(TreeIndex), 0);
	}
	for (const FRpgHarvestSwarmCreature& Creature : Swarm->GetCreatures())
	{
		TestTrue(TEXT("Every creature harvested before it finished"), Creature.State == ERpgHarvestSwarmCreatureState::Harvested);
	}
	TestEqual(TEXT("The swarm counts every section"), Swarm->GetHarvestedSections(), 12);
	TestEqual(TEXT("All rewards reach the summoner's inventory"), CountMaterial(Harvester.PlayerState), 12 * YieldPerSection);
	TestTrue(TEXT("The rewards arrive as one inventory delivery"), Swarm->GetDelivery() == ERpgHarvestDelivery::Inventory);
	TestEqual(TEXT("No drop appears"), GetWorldDrops(World).Num(), 0);

	// The side tree falls away from the swarm that struck it, not from the player.
	FVector SideDirection;
	const FVector ExpectedDirection = (FVector(300.0, 300.0, 0.0) - Swarm->GetActorLocation()).GetSafeNormal2D();
	TestTrue(
		TEXT("The swarm is the physical harvester"),
		Trees->GetInstanceHarvestDirection(1, SideDirection) && SideDirection.Equals(ExpectedDirection, 0.04));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestSwarmReassignmentTest,
	"SurvivalRpg.Harvesting.Swarm.CreaturesReassignWhenResourcesEmptyEarly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestSwarmReassignmentTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestSwarmTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	FHarvesterFixture Other = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixtures exist"), Harvester.IsValid() && Other.IsValid()))
	{
		return false;
	}
	RpgHarvestAutomation::AddInstanceStock(World);
	ARpgHarvestAutomationInstancesActor* Grove = SpawnGrove(World, Harvester.Pawn->BaseEyeHeight);
	const FGrantedAbility Summon = GrantSwarmAbility(Harvester.AbilitySystem, 2);
	if (!TestNotNull(TEXT("Grove exists"), Grove) || !TestNotNull(TEXT("Swarm ability exists"), Summon.Instance))
	{
		return false;
	}
	TestWorld.PrimeTimerManager();
	URpgHarvestAutomationInstancesComponent* Trees = Grove->Instances;
	auto EmptyTree = [Trees, &Other](const int32 TreeIndex)
	{
		const FRpgHarvestRequest Request = RpgHarvestAutomation::MakeInstanceRequest(Trees, TreeIndex, Other.Pawn, 4);
		return IRpgHarvestableTarget::Execute_CommitHarvest(Trees, Request).IsSuccess();
	};

	// Both creatures reserve the nearest tree; another player empties it before they arrive.
	TestTrue(TEXT("The swarm is summoned"), Harvester.AbilitySystem->TryActivateAbility(Summon.Handle));
	ARpgHarvestSwarm* Swarm = FindActiveSwarm(World);
	if (!TestNotNull(TEXT("The commit summons a swarm"), Swarm))
	{
		return false;
	}
	TestTrue(TEXT("Another player empties the reserved tree"), EmptyTree(0));
	Advance(TestWorld, 3.0);
	TestTrue(TEXT("Every creature finished"), Swarm->IsFinished());
	for (const FRpgHarvestSwarmCreature& Creature : Swarm->GetCreatures())
	{
		TestTrue(
			TEXT("Each creature moves on from the emptied tree and harvests the others"),
			Creature.State == ERpgHarvestSwarmCreatureState::Harvested && Creature.Leg >= 2 && Creature.SectionsTaken == 4);
	}
	TestEqual(TEXT("The rerouted creatures empty the side tree without overbooking it"), Trees->GetRemainingSections(1), 0);
	TestEqual(TEXT("The rerouted creatures empty the far tree"), Trees->GetRemainingSections(2), 0);
	TestEqual(TEXT("The summoner receives what the swarm harvested"), CountMaterial(Harvester.PlayerState), 8 * YieldPerSection);
	TestEqual(TEXT("The other player keeps the nearest tree's wood"), CountMaterial(Other.PlayerState), 4 * YieldPerSection);

	// Nothing is left in the area, so a second swarm finds nothing and dissipates.
	TestTrue(TEXT("A second swarm is summoned"), Harvester.AbilitySystem->TryActivateAbility(Summon.Handle));
	ARpgHarvestSwarm* LateSwarm = FindActiveSwarm(World);
	if (!TestNotNull(TEXT("The second commit summons a swarm"), LateSwarm))
	{
		return false;
	}
	Advance(TestWorld, 3.0);
	TestTrue(TEXT("The swarm in an emptied area finishes"), LateSwarm->IsFinished());
	for (const FRpgHarvestSwarmCreature& Creature : LateSwarm->GetCreatures())
	{
		TestTrue(TEXT("Creatures without stock left dissipate"), Creature.State == ERpgHarvestSwarmCreatureState::Dissipated);
	}
	TestEqual(TEXT("An empty area yields nothing"), LateSwarm->GetHarvestedSections(), 0);
	TestTrue(TEXT("An empty area delivers nothing"), LateSwarm->GetDelivery() == ERpgHarvestDelivery::None);
	TestEqual(TEXT("The summoner's rewards are unchanged"), CountMaterial(Harvester.PlayerState), 8 * YieldPerSection);
	TestEqual(TEXT("No drop appears"), GetWorldDrops(World).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestSwarmSummonerDeathTest,
	"SurvivalRpg.Harvesting.Swarm.SummonerDeathEndsTheSwarm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestSwarmSummonerDeathTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestSwarmTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	RpgHarvestAutomation::AddInstanceStock(World);
	ARpgHarvestAutomationInstancesActor* Grove = SpawnGrove(World, Harvester.Pawn->BaseEyeHeight);
	const FGrantedAbility Summon = GrantSwarmAbility(Harvester.AbilitySystem, 3);
	if (!TestNotNull(TEXT("Grove exists"), Grove) || !TestNotNull(TEXT("Swarm ability exists"), Summon.Instance))
	{
		return false;
	}
	TestWorld.PrimeTimerManager();
	URpgHarvestAutomationInstancesComponent* Trees = Grove->Instances;

	TestTrue(TEXT("The swarm is summoned"), Harvester.AbilitySystem->TryActivateAbility(Summon.Handle));
	ARpgHarvestSwarm* Swarm = FindActiveSwarm(World);
	if (!TestNotNull(TEXT("The commit summons a swarm"), Swarm))
	{
		return false;
	}
	Advance(TestWorld, Swarm->GetCreatures()[0].ArrivalServerTime - Swarm->GetServerWorldTimeSeconds() + 0.03);
	TestEqual(TEXT("The first creature harvested before the summoner died"), Trees->GetRemainingSections(0), 2);
	TestTrue(
		TEXT("Every creature is still at work"),
		Swarm->GetCreatures()[0].State == ERpgHarvestSwarmCreatureState::Flying &&
			Swarm->GetCreatures()[1].State == ERpgHarvestSwarmCreatureState::Flying &&
			Swarm->GetCreatures()[2].State == ERpgHarvestSwarmCreatureState::Flying);

	Harvester.AbilitySystem->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("Status.Death.Dying")));
	TestTrue(TEXT("The summoner's death ends the swarm at once"), Swarm->IsFinished());
	TestTrue(
		TEXT("The creature that had struck finishes as harvested"),
		Swarm->GetCreatures()[0].State == ERpgHarvestSwarmCreatureState::Harvested);
	TestTrue(
		TEXT("Creatures that had not struck dissipate"),
		Swarm->GetCreatures()[1].State == ERpgHarvestSwarmCreatureState::Dissipated &&
			Swarm->GetCreatures()[2].State == ERpgHarvestSwarmCreatureState::Dissipated);
	TestEqual(TEXT("What was harvested before the death is still delivered"), CountMaterial(Harvester.PlayerState), 2 * YieldPerSection);

	Advance(TestWorld, 3.0);
	TestEqual(TEXT("Nothing more is taken from the nearest tree"), Trees->GetRemainingSections(0), 2);
	TestEqual(TEXT("Nothing is harvested after the death"), Trees->GetRemainingSections(1), 4);
	TestEqual(TEXT("No late rewards arrive"), CountMaterial(Harvester.PlayerState), 2 * YieldPerSection);
	return true;
}

namespace RpgHarvestTuningTests
{
	using namespace RpgHarvestSwarmTests;

	/** Restores the class-default fragment of the tree tool when a test ends. */
	struct FScopedTreeTool
	{
		explicit FScopedTreeTool(const URpgSkillTreeDefinition* Tree)
		{
			URpgHarvestAutomationTestTreeToolDefinition::SetTestSkillTree(Tree);
		}

		~FScopedTreeTool()
		{
			URpgHarvestAutomationTestTreeToolDefinition::SetTestSkillTree(nullptr);
		}
	};

	/** Restores the class-default cooldown tag of the test cooldown effect when a test ends. */
	struct FScopedCooldownTag
	{
		explicit FScopedCooldownTag(const FGameplayTag CooldownTag)
		{
			URpgHarvestAutomationTestCooldownEffect::SetCooldownTag(CooldownTag);
		}

		~FScopedCooldownTag()
		{
			URpgHarvestAutomationTestCooldownEffect::SetCooldownTag(FGameplayTag());
		}
	};

	FRpgSkillTreeAbilityTuning MakeTuning(
		const FGameplayTag TuningTag,
		const ERpgSkillTreeTuningOperation Operation,
		const float Value)
	{
		FRpgSkillTreeAbilityTuning Tuning;
		Tuning.TuningTag = TuningTag;
		Tuning.Operation = Operation;
		Tuning.Value = Value;
		return Tuning;
	}

	/**
	 * One-point nodes on Mining, each tuning every ability of the tool: Wide (+120 cm area, +2 targets, +1 section,
	 * -1 s cooldown), Reach (+150 cm), Brood (6 creatures with one section each) and Blast (250 cm strike radius,
	 * 0.2 s rest).
	 */
	URpgSkillTreeDefinition* MakePowerTree()
	{
		using namespace RpgHarvestingMagicGameplayTags;

		URpgSkillTreeDefinition* Tree = NewObject<URpgSkillTreeDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		Tree->TreeTag = TAG_HarvestTuningTest_Tree;
		Tree->MasterySkillTag = RpgTradeSkillGameplayTags::Skill_Gathering_Mining;
		Tree->MaxPoints = 10;

		auto AddNode = [Tree](const FGameplayTag NodeTag, const int32 Column) -> FRpgSkillTreeNode&
		{
			FRpgSkillTreeNode& Node = Tree->Nodes.AddDefaulted_GetRef();
			Node.NodeTag = NodeTag;
			Node.Column = Column;
			return Node;
		};

		AddNode(TAG_HarvestTuningTest_Wide, 0).AbilityTunings = {
			MakeTuning(Ability_Tuning_Harvest_AreaRadius, ERpgSkillTreeTuningOperation::Add, 120.0f),
			MakeTuning(Ability_Tuning_Harvest_MaxTargets, ERpgSkillTreeTuningOperation::Add, 2.0f),
			MakeTuning(Ability_Tuning_Harvest_Sections, ERpgSkillTreeTuningOperation::Add, 1.0f),
			MakeTuning(Ability_Tuning_Harvest_Cooldown, ERpgSkillTreeTuningOperation::Add, -1.0f)};
		AddNode(TAG_HarvestTuningTest_Reach, 1).AbilityTunings = {
			MakeTuning(Ability_Tuning_Harvest_Reach, ERpgSkillTreeTuningOperation::Add, 150.0f)};
		AddNode(TAG_HarvestTuningTest_Brood, 2).AbilityTunings = {
			MakeTuning(Ability_Tuning_Harvest_Creatures, ERpgSkillTreeTuningOperation::Set, 6.0f),
			MakeTuning(Ability_Tuning_Harvest_Sections, ERpgSkillTreeTuningOperation::Set, 1.0f)};
		AddNode(TAG_HarvestTuningTest_Blast, 3).AbilityTunings = {
			MakeTuning(Ability_Tuning_Harvest_StrikeRadius, ERpgSkillTreeTuningOperation::Set, 250.0f),
			MakeTuning(Ability_Tuning_Harvest_StrikeInterval, ERpgSkillTreeTuningOperation::Set, 0.2f)};
		return Tree;
	}

	/** Gives the harvester's player state the tree and enough Mining levels for every node. */
	URpgSkillTreeComponent* PrepareSkillTree(const FHarvesterFixture& Harvester, const URpgSkillTreeDefinition* Tree)
	{
		URpgSkillTreeComponent* SkillTrees = Harvester.PlayerState->GetSkillTreeComponent();
		URpgTradeSkillProgressionComponent* TradeSkills = Harvester.PlayerState->GetTradeSkillProgressionComponent();
		if (!SkillTrees || !TradeSkills)
		{
			return nullptr;
		}
		SkillTrees->RegisterSkillTree(Tree);
		const FGameplayTag Mining = RpgTradeSkillGameplayTags::Skill_Gathering_Mining;
		for (int32 Guard = 0; Guard < 200 && TradeSkills->GetSkillLevelByTag(Mining) < 6; ++Guard)
		{
			const float MissingXP = TradeSkills->GetXPToNextLevelByTag(Mining) - TradeSkills->GetSkillXPByTag(Mining);
			TradeSkills->AddSkillXPByTag(Mining, FMath::Max(1.0f, MissingXP));
		}
		return SkillTrees;
	}

	/** Equipment instance whose source item is the tree tool, as the equipment manager grants tool abilities. */
	URpgEquipmentInstance* MakeTreeToolEquipment(const FHarvesterFixture& Harvester)
	{
		URpgInventoryItemInstance* ToolItem = Harvester.PlayerState->GetInventoryManagerComponent()->GrantItemDefinition(
			URpgHarvestAutomationTestTreeToolDefinition::StaticClass());
		URpgEquipmentInstance* Equipment = ToolItem ? NewObject<URpgEquipmentInstance>(Harvester.Pawn) : nullptr;
		if (Equipment)
		{
			Equipment->SetInstigator(ToolItem);
		}
		return Equipment;
	}

	/** Grants an area ability sourced from Equipment: 200 cm area, one target, one section, reach Reach. */
	FGrantedAbility GrantAreaAbility(URpgAbilitySystemComponent* AbilitySystem, UObject* Equipment, const float Reach)
	{
		FGrantedAbility Granted = GrantAbility(AbilitySystem, Equipment);
		if (Granted.Instance)
		{
			FRpgHarvestTargetingParams AreaTargeting;
			AreaTargeting.Shape = ERpgHarvestTargetShape::AreaAtAimPoint;
			AreaTargeting.MaxAimDistance = 1000.0f;
			AreaTargeting.MaxReachFromAvatar = Reach;
			AreaTargeting.AreaRadius = 200.0f;
			AreaTargeting.MaxTargets = 1;
			Granted.Instance->ConfigureTargeting(AreaTargeting);
			Granted.Instance->ConfigureSections(1);
		}
		return Granted;
	}

	/** Remaining sections of every tree of Grove, in instance order. */
	TArray<int32> RemainingSections(const ARpgHarvestAutomationInstancesActor* Grove, const int32 TreeCount)
	{
		TArray<int32> Remaining;
		for (int32 TreeIndex = 0; TreeIndex < TreeCount; ++TreeIndex)
		{
			Remaining.Add(Grove->Instances->GetRemainingSections(TreeIndex));
		}
		return Remaining;
	}

	/** Sections each target of Preview would take, in preview order; zero for targets that would not be harvested. */
	TArray<int32> PreviewSections(const FRpgHarvestPreview& Preview)
	{
		TArray<int32> Sections;
		for (const FRpgHarvestTargetEvaluation& Target : Preview.Targets)
		{
			Sections.Add(Target.WouldHarvest() ? Target.Result.SectionsTaken : 0);
		}
		return Sections;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestTuningPreviewAndCommitTest,
	"SurvivalRpg.Harvesting.Tuning.PreviewAndCommitAgree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestTuningPreviewAndCommitTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestTuningTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	RpgHarvestAutomation::AddInstanceStock(World);
	ARpgHarvestAutomationInstancesActor* Grove = SpawnGrove(World, Harvester.Pawn->BaseEyeHeight);
	URpgSkillTreeDefinition* Tree = MakePowerTree();
	FScopedTreeTool TreeTool(Tree);
	URpgSkillTreeComponent* SkillTrees = PrepareSkillTree(Harvester, Tree);
	URpgEquipmentInstance* Equipment = MakeTreeToolEquipment(Harvester);
	// The aimed tree face lies about 258 cm away, beyond the authored reach of 200 cm.
	const FGrantedAbility Power = GrantAreaAbility(Harvester.AbilitySystem, Equipment, 200.0f);
	if (!TestNotNull(TEXT("Grove exists"), Grove) ||
		!TestNotNull(TEXT("Skill trees are prepared"), SkillTrees) ||
		!TestNotNull(TEXT("Tree tool equipment exists"), Equipment) ||
		!TestNotNull(TEXT("Area ability exists"), Power.Instance))
	{
		return false;
	}
	FScopedCooldownTag CooldownTag(TAG_HarvestTuningTest_Cooldown);
	Power.Instance->ConfigureCooldown(URpgHarvestAutomationTestCooldownEffect::StaticClass());
	TestWorld.PrimeTimerManager();

	const FVector AvatarLocation = Harvester.Pawn->GetActorLocation();
	FRpgHarvestPreview Preview = Evaluate(Harvester.AbilitySystem, Power);
	TestTrue(
		TEXT("Without upgrades the aim point stops at the authored reach"),
		FVector::Dist(AvatarLocation, Preview.AimPoint) <= 201.0);
	TestTrue(TEXT("The area at the reach still takes the nearest tree"), PreviewSections(Preview) == TArray<int32>({1}));
	TestEqual(TEXT("The authored area radius is previewed"), Preview.AreaRadius, 200.0f);

	TestEqual(TEXT("Reach is learned"),
		SkillTrees->UnlockNode(TAG_HarvestTuningTest_Tree, TAG_HarvestTuningTest_Reach), ERpgSkillTreeUnlockResult::Unlockable);
	const FGameplayAbilitySpec* Spec = Harvester.AbilitySystem->FindAbilitySpecFromHandle(Power.Handle);
	FRpgHarvestTunedValues Values;
	if (Spec)
	{
		Power.Instance->ResolveTunedValues(*Spec, *Harvester.AbilitySystem->AbilityActorInfo, Values);
	}
	TestEqual(TEXT("A reach upgrade extends the reach"), Values.Targeting.MaxReachFromAvatar, 350.0f);
	TestEqual(TEXT("The aim ray grows with the reach"), Values.Targeting.MaxAimDistance, 1150.0f);
	Preview = Evaluate(Harvester.AbilitySystem, Power);
	TestTrue(
		TEXT("The longer reach lets the aim point stay on the aimed tree"),
		FVector::Dist(AvatarLocation, Preview.AimPoint) > 250.0 && FVector::Dist(AvatarLocation, Preview.AimPoint) <= 350.0);
	TestTrue(TEXT("The longer reach takes the nearest tree"), PreviewSections(Preview) == TArray<int32>({1}));

	TestEqual(TEXT("Wide is learned"),
		SkillTrees->UnlockNode(TAG_HarvestTuningTest_Tree, TAG_HarvestTuningTest_Wide), ERpgSkillTreeUnlockResult::Unlockable);
	Preview = Evaluate(Harvester.AbilitySystem, Power);
	TestEqual(TEXT("The preview ring follows the tuned radius"), Preview.AreaRadius, 320.0f);
	TestTrue(
		TEXT("The tuned area, target count and sections reach all three trees"),
		PreviewSections(Preview) == TArray<int32>({2, 2, 2}));

	TestTrue(TEXT("The power executes"), Harvester.AbilitySystem->TryActivateAbility(Power.Handle));
	TestTrue(
		TEXT("The commit takes exactly what the preview showed"),
		RemainingSections(Grove, 3) == TArray<int32>({2, 2, 2}));
	TestEqual(TEXT("The rewards arrive"), CountMaterial(Harvester.PlayerState), 6 * YieldPerSection);

	FGameplayEffectQuery CooldownQuery;
	CooldownQuery.EffectDefinition = URpgHarvestAutomationTestCooldownEffect::StaticClass();
	const TArray<float> CooldownDurations = Harvester.AbilitySystem->GetActiveEffectsDuration(CooldownQuery);
	TestTrue(
		TEXT("The cooldown is shortened by one second"),
		CooldownDurations.Num() == 1 &&
			FMath::IsNearlyEqual(CooldownDurations[0], URpgHarvestAutomationTestCooldownEffect::BaseDurationSeconds - 1.0f));
	TestTrue(TEXT("The cooldown blocks the power"), Harvester.AbilitySystem->HasMatchingGameplayTag(TAG_HarvestTuningTest_Cooldown));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestTuningCaptureTest,
	"SurvivalRpg.Harvesting.Tuning.ValuesCapturedAtExecutionStart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestTuningCaptureTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestTuningTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	RpgHarvestAutomation::AddInstanceStock(World);
	// The grove of SpawnGrove with eight sections per tree, so no tree falls and the aim stays on the nearest one.
	ARpgHarvestAutomationInstancesActor* Grove = RpgHarvestAutomation::SpawnInstances(
		World,
		MakeProfile(World, 8),
		{FVector(0.0, 0.0, 0.0), FVector(0.0, 300.0, 0.0), FVector(300.0, 0.0, 0.0)},
		FVector(300.0, 0.0, Harvester.Pawn->BaseEyeHeight));
	URpgSkillTreeDefinition* Tree = MakePowerTree();
	FScopedTreeTool TreeTool(Tree);
	URpgSkillTreeComponent* SkillTrees = PrepareSkillTree(Harvester, Tree);
	URpgEquipmentInstance* Equipment = MakeTreeToolEquipment(Harvester);
	const FGrantedAbility Power = GrantAreaAbility(Harvester.AbilitySystem, Equipment, 700.0f);
	if (!TestNotNull(TEXT("Grove exists"), Grove) ||
		!TestNotNull(TEXT("Skill trees are prepared"), SkillTrees) ||
		!TestNotNull(TEXT("Area ability exists"), Power.Instance))
	{
		return false;
	}
	Power.Instance->ConfigureCommitDelay(0.5f);
	TestWorld.PrimeTimerManager();

	// Learning an upgrade during the swing does not change the swing.
	TestTrue(TEXT("The first swing starts"), Harvester.AbilitySystem->TryActivateAbility(Power.Handle));
	SkillTrees->UnlockNode(TAG_HarvestTuningTest_Tree, TAG_HarvestTuningTest_Wide);
	TestWorld.AdvanceTimers(0.6f);
	TestTrue(
		TEXT("The swing commits with the values from its start"),
		RemainingSections(Grove, 3) == TArray<int32>({7, 8, 8}));

	// Resetting the tree during the next swing does not take the upgrade away from it.
	TestTrue(TEXT("The second swing starts"), Harvester.AbilitySystem->TryActivateAbility(Power.Handle));
	SkillTrees->ResetTree(TAG_HarvestTuningTest_Tree);
	TestWorld.AdvanceTimers(0.6f);
	TestTrue(
		TEXT("The upgraded swing keeps its area, targets and sections"),
		RemainingSections(Grove, 3) == TArray<int32>({5, 6, 6}));

	// A spell effect with the same tunings as Wide, applied during a swing, waits for the next swing.
	const FRpgSkillTreeNode* WideNode = Tree->FindNode(TAG_HarvestTuningTest_Wide);
	const UGameplayEffect* Spell = WideNode
		? URpgAbilityTuningEffectComponent::MakeTransientEffectForTests(WideNode->AbilityTunings, 0.8f)
		: nullptr;
	if (!TestNotNull(TEXT("The spell effect carries Wide's tunings"), Spell))
	{
		return false;
	}
	TestTrue(TEXT("The third swing starts"), Harvester.AbilitySystem->TryActivateAbility(Power.Handle));
	TestTrue(TEXT("The spell effect applies during the swing"),
		Harvester.AbilitySystem->ApplyGameplayEffectToSelf(Spell, 1.0f, Harvester.AbilitySystem->MakeEffectContext()).IsValid());
	TestWorld.AdvanceTimers(0.6f);
	TestTrue(
		TEXT("The swing the spell started in commits without it"),
		RemainingSections(Grove, 3) == TArray<int32>({4, 6, 6}));

	// The spell expiring during the next swing does not take the upgrade away from it.
	TestTrue(TEXT("The fourth swing starts"), Harvester.AbilitySystem->TryActivateAbility(Power.Handle));
	TestWorld.AdvanceTimers(0.3f);
	const FGameplayAbilitySpec* PowerSpec = Harvester.AbilitySystem->FindAbilitySpecFromHandle(Power.Handle);
	FRpgHarvestTunedValues Expired;
	if (PowerSpec)
	{
		Power.Instance->ResolveTunedValues(*PowerSpec, *Harvester.AbilitySystem->AbilityActorInfo, Expired);
	}
	TestEqual(TEXT("The spell has expired before the commit"), Expired.Targeting.AreaRadius, 200.0f);
	TestWorld.AdvanceTimers(0.3f);
	TestTrue(
		TEXT("The swing that started inside the spell keeps its area, targets and sections"),
		RemainingSections(Grove, 3) == TArray<int32>({2, 4, 4}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestSwarmStrikeRadiusTest,
	"SurvivalRpg.Harvesting.Swarm.StrikeRadiusTakesEachTargetOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestSwarmStrikeRadiusTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestTuningTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	FHarvesterFixture Other = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixtures exist"), Harvester.IsValid() && Other.IsValid()))
	{
		return false;
	}
	RpgHarvestAutomation::AddInstanceStock(World);
	// The nearest tree, two trees 300 cm from it and one 400 cm from it.
	ARpgHarvestAutomationInstancesActor* Grove = RpgHarvestAutomation::SpawnInstances(
		World,
		MakeProfile(World, 4),
		{FVector(0.0, 0.0, 0.0), FVector(0.0, 300.0, 0.0), FVector(300.0, 0.0, 0.0), FVector(0.0, -400.0, 0.0)},
		FVector(300.0, 0.0, Harvester.Pawn->BaseEyeHeight));
	const FGrantedAbility Summon = GrantSwarmAbility(Harvester.AbilitySystem, 3);
	if (!TestNotNull(TEXT("Grove exists"), Grove) || !TestNotNull(TEXT("Swarm ability exists"), Summon.Instance))
	{
		return false;
	}
	FRpgHarvestTargetingParams AreaTargeting;
	AreaTargeting.Shape = ERpgHarvestTargetShape::AreaAtAimPoint;
	AreaTargeting.MaxAimDistance = 1000.0f;
	AreaTargeting.MaxReachFromAvatar = 700.0f;
	AreaTargeting.AreaRadius = 450.0f;
	AreaTargeting.MaxTargets = 4;
	Summon.Instance->ConfigureTargeting(AreaTargeting);
	FRpgHarvestSwarmParams SwarmParams;
	SwarmParams.CreatureCount = 3;
	SwarmParams.FlightSpeed = 1000.0f;
	SwarmParams.EmergeSeconds = 0.5f;
	SwarmParams.LaunchIntervalSeconds = 0.1f;
	SwarmParams.StrikeIntervalSeconds = 0.3f;
	SwarmParams.MaxLifetimeSeconds = 8.0f;
	SwarmParams.StrikeRadius = 350.0f;
	Summon.Instance->ConfigureSwarm(ARpgHarvestSwarm::StaticClass(), SwarmParams);
	TestWorld.PrimeTimerManager();

	// Another player leaves only two sections on the first side tree.
	TestTrue(
		TEXT("Another player harvests the first side tree"),
		IRpgHarvestableTarget::Execute_CommitHarvest(
			Grove->Instances,
			RpgHarvestAutomation::MakeInstanceRequest(Grove->Instances, 1, Other.Pawn, 2)).IsSuccess());

	// Two creatures share the nearest tree; the third reserves the side tree's last two sections.
	TestTrue(TEXT("The swarm is summoned"), Harvester.AbilitySystem->TryActivateAbility(Summon.Handle));
	ARpgHarvestSwarm* Swarm = FindActiveSwarm(World);
	if (!TestNotNull(TEXT("The commit summons a swarm"), Swarm))
	{
		return false;
	}
	TestEqual(TEXT("The strike radius reaches every machine"), Swarm->GetStrikeRadius(), 350.0f);

	Advance(TestWorld, Swarm->GetCreatures()[0].ArrivalServerTime - Swarm->GetServerWorldTimeSeconds() + 0.01);
	TestTrue(
		TEXT("The strike also takes the free stock within its radius, once and without the reserved sections"),
		RemainingSections(Grove, 4) == TArray<int32>({2, 2, 2, 4}));
	TestEqual(TEXT("The striking creature counts every section of its strike"), Swarm->GetCreatures()[0].SectionsTaken, 4);

	const double ThirdArrival = Swarm->GetCreatures()[2].ArrivalServerTime;
	Advance(TestWorld, ThirdArrival - Swarm->GetServerWorldTimeSeconds() + 0.01);
	TestTrue(
		TEXT("The creature that reserved the side tree takes it with its first flight"),
		Swarm->GetCreatures()[2].Strikes == 1 && Swarm->GetCreatures()[2].SectionsTaken == 2);

	Advance(TestWorld, 5.0);
	TestTrue(TEXT("Every creature finished"), Swarm->IsFinished());
	TestTrue(TEXT("The swarm empties every tree"), RemainingSections(Grove, 4) == TArray<int32>({0, 0, 0, 0}));
	TestEqual(TEXT("The swarm counts every section exactly once"), Swarm->GetHarvestedSections(), 14);
	TestEqual(TEXT("The summoner receives every section"), CountMaterial(Harvester.PlayerState), 14 * YieldPerSection);
	TestTrue(TEXT("The rewards arrive as one delivery"), Swarm->GetDelivery() == ERpgHarvestDelivery::Inventory);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestTuningSwarmFormsTest,
	"SurvivalRpg.Harvesting.Tuning.SwarmFormsFollowTree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestTuningSwarmFormsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestTuningTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	RpgHarvestAutomation::AddInstanceStock(World);
	ARpgHarvestAutomationInstancesActor* Grove = SpawnGrove(World, Harvester.Pawn->BaseEyeHeight);
	URpgSkillTreeDefinition* Tree = MakePowerTree();
	FScopedTreeTool TreeTool(Tree);
	URpgSkillTreeComponent* SkillTrees = PrepareSkillTree(Harvester, Tree);
	URpgEquipmentInstance* Equipment = MakeTreeToolEquipment(Harvester);
	const FGrantedAbility Summon = GrantSwarmAbility(Harvester.AbilitySystem, 3, Equipment);
	if (!TestNotNull(TEXT("Grove exists"), Grove) ||
		!TestNotNull(TEXT("Skill trees are prepared"), SkillTrees) ||
		!TestNotNull(TEXT("Swarm ability exists"), Summon.Instance))
	{
		return false;
	}
	TestWorld.PrimeTimerManager();

	// The brood form: six creatures that take one section per strike, resting 0.2 s.
	SkillTrees->UnlockNode(TAG_HarvestTuningTest_Tree, TAG_HarvestTuningTest_Brood);
	SkillTrees->UnlockNode(TAG_HarvestTuningTest_Tree, TAG_HarvestTuningTest_Blast);
	TestTrue(TEXT("The swarm is summoned"), Harvester.AbilitySystem->TryActivateAbility(Summon.Handle));
	ARpgHarvestSwarm* Swarm = FindActiveSwarm(World);
	if (!TestNotNull(TEXT("The commit summons a swarm"), Swarm))
	{
		return false;
	}
	const TArray<FRpgHarvestSwarmCreature> Creatures = Swarm->GetCreatures();
	TestEqual(TEXT("The tuned creature count is summoned"), Creatures.Num(), 6);
	TestEqual(TEXT("The tuned strike radius is summoned"), Swarm->GetStrikeRadius(), 250.0f);

	const float StrikeHeight = Swarm->GetStrikeHeight();
	const FVector EyeLift(0.0, 0.0, Harvester.Pawn->BaseEyeHeight + StrikeHeight);
	const FVector NearestTree = FVector(300.0, 0.0, 0.0) + EyeLift;
	const FVector SideTree = FVector(300.0, 300.0, 0.0) + EyeLift;
	bool bPlanned = Creatures.Num() == 6;
	for (int32 CreatureIndex = 0; bPlanned && CreatureIndex < Creatures.Num(); ++CreatureIndex)
	{
		const FVector Expected = CreatureIndex < 4 ? NearestTree : SideTree;
		bPlanned = Creatures[CreatureIndex].State == ERpgHarvestSwarmCreatureState::Flying &&
			FVector::Dist(Creatures[CreatureIndex].To, Expected) < 1.0;
	}
	TestTrue(TEXT("Four creatures share the nearest tree's four sections, two start on the side tree"), bPlanned);

	const double FirstArrival = Creatures[0].ArrivalServerTime;
	Advance(TestWorld, FirstArrival - Swarm->GetServerWorldTimeSeconds() + 0.01);
	TestEqual(TEXT("A brood creature takes one section per strike"), Grove->Instances->GetRemainingSections(0), 3);
	TestTrue(
		TEXT("The tuned rest delays the creature's next flight"),
		FMath::IsNearlyEqual(Swarm->GetCreatures()[0].LaunchServerTime - FirstArrival, 0.2, 0.03));

	Advance(TestWorld, 6.0);
	TestTrue(TEXT("Every creature finished"), Swarm->IsFinished());
	TestTrue(TEXT("The brood empties every tree"), RemainingSections(Grove, 3) == TArray<int32>({0, 0, 0}));
	TestEqual(TEXT("The brood counts every section once"), Swarm->GetHarvestedSections(), 12);
	return true;
}

namespace RpgHarvestYieldTests
{
	using namespace RpgHarvestTuningTests;

	using FInputMaterial = URpgHarvestAutomationTestStackItemDefinition;
	using FOutputMaterial = URpgHarvestAutomationTestSecondMaterialDefinition;

	FRpgHarvestYieldConversion MakeConversion(const int32 InputPerOutput, const FGameplayTag RequiredOwnerTag = FGameplayTag())
	{
		FRpgHarvestYieldConversion Conversion;
		Conversion.RequiredOwnerTag = RequiredOwnerTag;
		Conversion.InputItem = FInputMaterial::StaticClass();
		Conversion.OutputItem = FOutputMaterial::StaticClass();
		Conversion.InputPerOutput = InputPerOutput;
		return Conversion;
	}

	int32 CountStack(const FInventoryPickup& Reward, const TSubclassOf<URpgInventoryItemDefinition> Definition)
	{
		int32 Count = 0;
		for (const FPickupTemplate& Template : Reward.Templates)
		{
			Count += Template.ItemDef == Definition ? Template.StackCount : 0;
		}
		return Count;
	}

	int32 CountOutput(const ARpgHarvestAutomationTestPlayerState* PlayerState)
	{
		return PlayerState && PlayerState->GetInventoryManagerComponent()
			? PlayerState->GetInventoryManagerComponent()->GetTotalItemCountByDefinition(FOutputMaterial::StaticClass())
			: 0;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestYieldConversionRulesTest,
	"SurvivalRpg.Harvesting.Yield.ConversionKeepsRemainder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestYieldConversionRulesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestYieldTests;

	auto MakeReward = [](const TArray<TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>>& Stacks)
	{
		FInventoryPickup Reward;
		for (const TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>& Stack : Stacks)
		{
			FPickupTemplate& Template = Reward.Templates.AddDefaulted_GetRef();
			Template.ItemDef = Stack.Key;
			Template.StackCount = Stack.Value;
		}
		return Reward;
	};
	const TSubclassOf<URpgInventoryItemDefinition> Other = URpgHarvestAutomationTestLowToolDefinition::StaticClass();

	FInventoryPickup Reward = MakeReward({{FInputMaterial::StaticClass(), 7}, {Other, 3}});
	TestEqual(TEXT("Seven inputs at two per output make three outputs"),
		FRpgHarvestRewardService::ApplyYieldConversions(Reward, {MakeConversion(2)}), 3);
	TestEqual(TEXT("The odd input stays the input material"), CountStack(Reward, FInputMaterial::StaticClass()), 1);
	TestEqual(TEXT("The outputs arrive as one stack"), CountStack(Reward, FOutputMaterial::StaticClass()), 3);
	TestEqual(TEXT("Other loot is untouched"), CountStack(Reward, Other), 3);

	Reward = MakeReward({{FInputMaterial::StaticClass(), 5}, {FOutputMaterial::StaticClass(), 1}, {FInputMaterial::StaticClass(), 4}});
	TestEqual(TEXT("Split input stacks count together"),
		FRpgHarvestRewardService::ApplyYieldConversions(Reward, {MakeConversion(3)}), 3);
	TestEqual(TEXT("Nothing of the input is left"), CountStack(Reward, FInputMaterial::StaticClass()), 0);
	TestEqual(TEXT("Outputs join an existing output stack"), CountStack(Reward, FOutputMaterial::StaticClass()), 4);
	TestEqual(TEXT("Empty stacks are removed"), Reward.Templates.Num(), 1);

	Reward = MakeReward({{FInputMaterial::StaticClass(), 4}});
	TestEqual(TEXT("Too few inputs make no output"),
		FRpgHarvestRewardService::ApplyYieldConversions(Reward, {MakeConversion(5)}), 0);
	TestEqual(TEXT("The inputs stay unchanged"), CountStack(Reward, FInputMaterial::StaticClass()), 4);

	FRpgHarvestYieldConversion SameItem = MakeConversion(1);
	SameItem.OutputItem = SameItem.InputItem;
	TestEqual(TEXT("An invalid conversion is skipped"),
		FRpgHarvestRewardService::ApplyYieldConversions(Reward, {SameItem}), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestYieldAbilityTest,
	"SurvivalRpg.Harvesting.Yield.AbilityConvertsWhileFormIsActive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestYieldAbilityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestYieldTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	RpgHarvestAutomation::AddInstanceStock(World);
	ARpgHarvestAutomationInstancesActor* Grove = SpawnGrove(World, Harvester.Pawn->BaseEyeHeight);
	// One section of each of the three trees, two items per section: six inputs per execution.
	const FGrantedAbility Power = GrantAreaAbility(Harvester.AbilitySystem, nullptr, 700.0f);
	const FGrantedAbility PlainPower = GrantAreaAbility(Harvester.AbilitySystem, nullptr, 700.0f);
	if (!TestNotNull(TEXT("Grove exists"), Grove) ||
		!TestNotNull(TEXT("Converting power exists"), Power.Instance) ||
		!TestNotNull(TEXT("Plain power exists"), PlainPower.Instance))
	{
		return false;
	}
	for (const FGrantedAbility& Granted : {Power, PlainPower})
	{
		FRpgHarvestTargetingParams AreaTargeting;
		AreaTargeting.Shape = ERpgHarvestTargetShape::AreaAtAimPoint;
		AreaTargeting.MaxAimDistance = 1000.0f;
		AreaTargeting.MaxReachFromAvatar = 700.0f;
		AreaTargeting.AreaRadius = 450.0f;
		AreaTargeting.MaxTargets = 3;
		Granted.Instance->ConfigureTargeting(AreaTargeting);
	}
	Power.Instance->ConfigureYieldConversions({MakeConversion(4, TAG_HarvestYieldTest_Form)});
	TestWorld.PrimeTimerManager();

	TestTrue(TEXT("Without the form the preview converts nothing"), Evaluate(Harvester.AbilitySystem, Power).YieldConversions.IsEmpty());
	TestTrue(TEXT("Without the form the power executes"), Harvester.AbilitySystem->TryActivateAbility(Power.Handle));
	TestEqual(TEXT("Without the form the harvest yields its material"), CountMaterial(Harvester.PlayerState), 6);
	TestEqual(TEXT("Without the form nothing is converted"), CountOutput(Harvester.PlayerState), 0);

	Harvester.AbilitySystem->AddLooseGameplayTag(TAG_HarvestYieldTest_Form);
	const FRpgHarvestPreview Preview = Evaluate(Harvester.AbilitySystem, Power);
	TestTrue(TEXT("The preview shows the active conversion"),
		Preview.YieldConversions.Num() == 1 && Preview.YieldConversions[0].InputPerOutput == 4);
	TestTrue(TEXT("With the form the power executes"), Harvester.AbilitySystem->TryActivateAbility(Power.Handle));
	TestEqual(TEXT("The three trees' rewards convert together: six inputs make one output"), CountOutput(Harvester.PlayerState), 1);
	TestEqual(TEXT("Two inputs remain as material, no hidden loss"), CountMaterial(Harvester.PlayerState), 6 + 2);

	TestTrue(TEXT("A power without conversions executes"), Harvester.AbilitySystem->TryActivateAbility(PlainPower.Handle));
	TestEqual(TEXT("Another ability keeps its material despite the form"), CountMaterial(Harvester.PlayerState), 8 + 6);
	TestEqual(TEXT("Another ability converts nothing"), CountOutput(Harvester.PlayerState), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestYieldSwarmTest,
	"SurvivalRpg.Harvesting.Yield.SwarmKeepsConversionFromSummon",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestYieldSwarmTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestYieldTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	RpgHarvestAutomation::AddInstanceStock(World);
	ARpgHarvestAutomationInstancesActor* Grove = SpawnGrove(World, Harvester.Pawn->BaseEyeHeight);
	const FGrantedAbility Summon = GrantSwarmAbility(Harvester.AbilitySystem, 4);
	if (!TestNotNull(TEXT("Grove exists"), Grove) || !TestNotNull(TEXT("Swarm ability exists"), Summon.Instance))
	{
		return false;
	}
	Summon.Instance->ConfigureYieldConversions({MakeConversion(5, TAG_HarvestYieldTest_Form)});
	TestWorld.PrimeTimerManager();

	Harvester.AbilitySystem->AddLooseGameplayTag(TAG_HarvestYieldTest_Form);
	TestTrue(TEXT("The swarm is summoned with the form"), Harvester.AbilitySystem->TryActivateAbility(Summon.Handle));
	ARpgHarvestSwarm* Swarm = FindActiveSwarm(World);
	if (!TestNotNull(TEXT("The commit summons a swarm"), Swarm))
	{
		return false;
	}
	// Unlearning the form after the summon does not change the swarm.
	Harvester.AbilitySystem->RemoveLooseGameplayTag(TAG_HarvestYieldTest_Form);
	Advance(TestWorld, 5.0);
	TestTrue(TEXT("Every creature finished"), Swarm->IsFinished());
	TestEqual(TEXT("The swarm harvests all twelve sections"), Swarm->GetHarvestedSections(), 12);
	TestEqual(TEXT("Its 24 inputs convert at the summon's ratio into four outputs"), CountOutput(Harvester.PlayerState), 4);
	TestEqual(TEXT("Four inputs remain as material"), CountMaterial(Harvester.PlayerState), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestFormCueTest,
	"SurvivalRpg.Harvesting.Feedback.FormSelectsSuccessCueAtAreaCenter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestFormCueTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestTuningTests;

	struct FRoutedCue
	{
		FGameplayTag Tag;
		FVector Location = FVector::ZeroVector;
		float RawMagnitude = 0.0f;
	};

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()) || !TestNotNull(TEXT("Cue manager exists"), CueManager))
	{
		return false;
	}
	RpgHarvestAutomation::AddInstanceStock(World);
	ARpgHarvestAutomationInstancesActor* Grove = SpawnGrove(World, Harvester.Pawn->BaseEyeHeight);
	const FGrantedAbility Power = GrantAreaAbility(Harvester.AbilitySystem, nullptr, 700.0f);
	if (!TestNotNull(TEXT("Grove exists"), Grove) || !TestNotNull(TEXT("Area power exists"), Power.Instance))
	{
		return false;
	}
	FRpgHarvestTargetingParams AreaTargeting;
	AreaTargeting.Shape = ERpgHarvestTargetShape::AreaAtAimPoint;
	AreaTargeting.MaxAimDistance = 1000.0f;
	AreaTargeting.MaxReachFromAvatar = 700.0f;
	AreaTargeting.AreaRadius = 450.0f;
	AreaTargeting.MaxTargets = 3;
	Power.Instance->ConfigureTargeting(AreaTargeting);
	FRpgHarvestFormGameplayCue FormCue;
	FormCue.RequiredOwnerTag = TAG_HarvestYieldTest_Form;
	FormCue.GameplayCue = TAG_HarvestFormCueTest_Form;
	Power.Instance->ConfigureSuccessCues(TAG_HarvestFormCueTest_Plain, {FormCue});
	TestWorld.PrimeTimerManager();

	TArray<FRoutedCue> RoutedCues;
	const FDelegateHandle RoutedHandle = CueManager->OnGameplayCueRouted().AddLambda(
		[&RoutedCues](AActor*, const FGameplayTag Tag, const EGameplayCueEvent::Type Event,
			const FGameplayCueParameters& CueParameters, EGameplayCueExecutionOptions)
		{
			if (Event == EGameplayCueEvent::Executed &&
				(Tag == TAG_HarvestFormCueTest_Plain.GetTag() || Tag == TAG_HarvestFormCueTest_Form.GetTag()))
			{
				RoutedCues.Add({Tag, FVector(CueParameters.Location), CueParameters.RawMagnitude});
			}
		});
	ON_SCOPE_EXIT
	{
		CueManager->OnGameplayCueRouted().Remove(RoutedHandle);
	};

	const FVector AreaCenter = Evaluate(Harvester.AbilitySystem, Power).AimPoint;
	TestTrue(TEXT("Without the form the power executes"), Harvester.AbilitySystem->TryActivateAbility(Power.Handle));
	if (!TestEqual(TEXT("The commit plays one success cue"), RoutedCues.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("Without the form the plain success cue plays"), RoutedCues[0].Tag, TAG_HarvestFormCueTest_Plain.GetTag());
	TestTrue(TEXT("An area cue plays at the area's center"), RoutedCues[0].Location.Equals(AreaCenter, 1.0));
	TestEqual(TEXT("An area cue carries the area radius"), RoutedCues[0].RawMagnitude, 450.0f);

	Harvester.AbilitySystem->AddLooseGameplayTag(TAG_HarvestYieldTest_Form);
	TestTrue(TEXT("With the form the power executes"), Harvester.AbilitySystem->TryActivateAbility(Power.Handle));
	if (!TestEqual(TEXT("The second commit plays one success cue"), RoutedCues.Num(), 2))
	{
		return false;
	}
	TestEqual(TEXT("The form's cue replaces the plain cue"), RoutedCues[1].Tag, TAG_HarvestFormCueTest_Form.GetTag());
	TestEqual(TEXT("The form's cue carries the area radius too"), RoutedCues[1].RawMagnitude, 450.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestTargetCueTest,
	"SurvivalRpg.Harvesting.Feedback.TargetCuePlaysOnEveryHarvestedResource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestTargetCueTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestTuningTests;

	struct FRoutedCue
	{
		FVector Location = FVector::ZeroVector;
		float RawMagnitude = 0.0f;
	};

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()) || !TestNotNull(TEXT("Cue manager exists"), CueManager))
	{
		return false;
	}
	RpgHarvestAutomation::AddInstanceStock(World);
	ARpgHarvestAutomationInstancesActor* Grove = SpawnGrove(World, Harvester.Pawn->BaseEyeHeight);
	const FGrantedAbility Power = GrantAreaAbility(Harvester.AbilitySystem, nullptr, 700.0f);
	if (!TestNotNull(TEXT("Grove exists"), Grove) || !TestNotNull(TEXT("Area power exists"), Power.Instance))
	{
		return false;
	}
	FRpgHarvestTargetingParams AreaTargeting;
	AreaTargeting.Shape = ERpgHarvestTargetShape::AreaAtAimPoint;
	AreaTargeting.MaxAimDistance = 1000.0f;
	AreaTargeting.MaxReachFromAvatar = 700.0f;
	AreaTargeting.AreaRadius = 450.0f;
	AreaTargeting.MaxTargets = 3;
	Power.Instance->ConfigureTargeting(AreaTargeting);
	// Every harvested cube is depleted, and depletion scales its instance to zero.
	Power.Instance->ConfigureSections(4);
	Power.Instance->ConfigureTargetCue(TAG_HarvestTargetCueTest);
	TestWorld.PrimeTimerManager();

	// The cubes are 100 cm wide: each cue should sit 50 cm above its instance's center and carry a 50 cm radius.
	TArray<int32> HarvestedInstances;
	TArray<FVector> ExpectedTops;
	for (const FRpgHarvestTargetEvaluation& Target : Evaluate(Harvester.AbilitySystem, Power).Targets)
	{
		FTransform InstanceTransform;
		if (Target.WouldHarvest() && Grove->Instances->GetInstanceTransform(Target.Hit.Item, InstanceTransform, true))
		{
			HarvestedInstances.Add(Target.Hit.Item);
			ExpectedTops.Add(InstanceTransform.GetLocation() + FVector(0.0, 0.0, 50.0));
		}
	}
	if (!TestTrue(TEXT("The area covers several cubes"), ExpectedTops.Num() >= 2))
	{
		return false;
	}

	TArray<FRoutedCue> RoutedCues;
	const FDelegateHandle RoutedHandle = CueManager->OnGameplayCueRouted().AddLambda(
		[&RoutedCues](AActor*, const FGameplayTag Tag, const EGameplayCueEvent::Type Event,
			const FGameplayCueParameters& CueParameters, EGameplayCueExecutionOptions)
		{
			if (Event == EGameplayCueEvent::Executed && Tag == TAG_HarvestTargetCueTest.GetTag())
			{
				RoutedCues.Add({FVector(CueParameters.Location), CueParameters.RawMagnitude});
			}
		});
	ON_SCOPE_EXIT
	{
		CueManager->OnGameplayCueRouted().Remove(RoutedHandle);
	};

	TestTrue(TEXT("The power executes"), Harvester.AbilitySystem->TryActivateAbility(Power.Handle));
	if (!TestEqual(TEXT("Every harvested cube plays one target cue"), RoutedCues.Num(), ExpectedTops.Num()))
	{
		return false;
	}
	for (int32 CueIndex = 0; CueIndex < RoutedCues.Num(); ++CueIndex)
	{
		TestTrue(
			FString::Printf(TEXT("Target cue %d plays on top of its cube, in selection order"), CueIndex),
			RoutedCues[CueIndex].Location.Equals(ExpectedTops[CueIndex], 1.0));
		TestEqual(
			FString::Printf(TEXT("Target cue %d carries the cube's size from before the commit"), CueIndex),
			RoutedCues[CueIndex].RawMagnitude,
			50.0f,
			0.5f);
	}
	for (const int32 InstanceIndex : HarvestedInstances)
	{
		FTransform InstanceTransform;
		TestTrue(
			FString::Printf(TEXT("Harvested cube %d is depleted and hidden after the commit"), InstanceIndex),
			Grove->Instances->GetRemainingSections(InstanceIndex) == 0 &&
				Grove->Instances->GetInstanceTransform(InstanceIndex, InstanceTransform, true) &&
				InstanceTransform.GetScale3D().IsNearlyZero());
	}
	return true;
}

namespace RpgHarvestStrideTests
{
	using namespace RpgHarvestTuningTests;

	/** Gives the harvester's plain pawn a root at the origin, so a test can walk it. */
	bool MakeWalkable(const FHarvesterFixture& Harvester)
	{
		USceneComponent* Root = NewObject<USceneComponent>(Harvester.Pawn, TEXT("StrideTestRoot"));
		Harvester.Pawn->SetRootComponent(Root);
		Root->RegisterComponent();
		return Harvester.Pawn->SetActorLocation(FVector::ZeroVector);
	}

	/**
	 * Grants a stride ability that harvests 300 cm around the harvester for 2 s, one pulse every 0.5 s, with up to
	 * MaxTargets targets per pulse and Sections sections per target.
	 */
	FGrantedAbility GrantStrideAbility(
		URpgAbilitySystemComponent* AbilitySystem,
		const int32 MaxTargets,
		const int32 Sections,
		const FGameplayTag CueTag = FGameplayTag())
	{
		FGrantedAbility Granted = GrantAbility(AbilitySystem);
		if (Granted.Instance)
		{
			FRpgHarvestTargetingParams Around;
			Around.Shape = ERpgHarvestTargetShape::AreaAroundHarvester;
			Around.AreaRadius = 300.0f;
			Around.MaxTargets = MaxTargets;
			Granted.Instance->ConfigureTargeting(Around);
			Granted.Instance->ConfigureSections(Sections);
			FRpgHarvestStrideParams Stride;
			Stride.DurationSeconds = 2.0f;
			Stride.PulseIntervalSeconds = 0.5f;
			Granted.Instance->ConfigureStride(Stride, CueTag);
		}
		return Granted;
	}

	bool IsAbilityActive(const URpgAbilitySystemComponent* AbilitySystem, const FGrantedAbility& Granted)
	{
		const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromHandle(Granted.Handle);
		return Spec && Spec->IsActive();
	}

	bool IsFull(const ARpgHarvestAutomationCollidableNodeActor* Node, const int32 SectionCount)
	{
		return Node && Node->HarvestableNode->GetRemainingSections() == SectionCount;
	}

	bool IsEmpty(const ARpgHarvestAutomationCollidableNodeActor* Node)
	{
		return Node && !Node->HarvestableNode->IsHarvestable();
	}

	/** Reads the prediction key of the active cue Tag of AbilitySystem; false when the cue is not active. */
	bool FindActiveCuePredictionKey(const UAbilitySystemComponent* AbilitySystem, const FGameplayTag Tag, FPredictionKey& OutKey)
	{
		const FStructProperty* Property =
			FindFProperty<FStructProperty>(UAbilitySystemComponent::StaticClass(), TEXT("ActiveGameplayCues"));
		const FActiveGameplayCueContainer* Cues =
			Property ? Property->ContainerPtrToValuePtr<FActiveGameplayCueContainer>(AbilitySystem) : nullptr;
		for (const FActiveGameplayCue& Cue : Cues ? Cues->GameplayCues : TArray<FActiveGameplayCue>())
		{
			if (Cue.GameplayCueTag == Tag)
			{
				OutKey = Cue.PredictionKey;
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestStrideWalkTest,
	"SurvivalRpg.Harvesting.Stride.HarvestsAroundTheWalkingHarvester",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestStrideWalkTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestStrideTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()) ||
		!TestTrue(TEXT("The harvester can walk"), MakeWalkable(Harvester)))
	{
		return false;
	}

	// Resources along the harvester's path, one far beside it, and one it reaches only after the stride.
	ARpgHarvestAutomationCollidableNodeActor* Start = SpawnNode(World, FVector(200.0, 0.0, 0.0), MakeProfile(World, 4));
	ARpgHarvestAutomationCollidableNodeActor* Middle = SpawnNode(World, FVector(900.0, 0.0, 0.0), MakeProfile(World, 4));
	ARpgHarvestAutomationCollidableNodeActor* End = SpawnNode(World, FVector(1600.0, 0.0, 0.0), MakeProfile(World, 4));
	ARpgHarvestAutomationCollidableNodeActor* Later = SpawnNode(World, FVector(2600.0, 0.0, 0.0), MakeProfile(World, 4));
	ARpgHarvestAutomationCollidableNodeActor* Beside = SpawnNode(World, FVector(900.0, 900.0, 0.0), MakeProfile(World, 4));
	const FGrantedAbility Stride =
		GrantStrideAbility(Harvester.AbilitySystem, 4, URpgHarvestProfile::MaxSectionCount, TAG_HarvestStrideTest_Cue);
	if (!TestTrue(TEXT("Every resource exists"), Start && Middle && End && Later && Beside) ||
		!TestNotNull(TEXT("Stride ability exists"), Stride.Instance))
	{
		return false;
	}
	FScopedCooldownTag CooldownTag(TAG_HarvestTuningTest_Cooldown);
	Stride.Instance->ConfigureCooldown(URpgHarvestAutomationTestCooldownEffect::StaticClass());
	TestWorld.PrimeTimerManager();

	// As on a server, the stride starts inside the owning client's predicted activation.
	const FPredictionKey ClientKey = FPredictionKey::CreateNewPredictionKey(Harvester.AbilitySystem);
	TestTrue(
		TEXT("The stride activates without aiming"),
		Harvester.AbilitySystem->InternalTryActivateAbility(Stride.Handle, ClientKey));
	TestTrue(TEXT("The stride runs"), Stride.Instance->IsStriding());
	TestTrue(TEXT("The ability stays active while the stride runs"), IsAbilityActive(Harvester.AbilitySystem, Stride));
	TestTrue(TEXT("The cooldown starts with the stride"), Harvester.AbilitySystem->HasMatchingGameplayTag(TAG_HarvestTuningTest_Cooldown));
	TestTrue(TEXT("The stride's cue is on the harvester"), Harvester.AbilitySystem->HasMatchingGameplayTag(TAG_HarvestStrideTest_Cue));
	FPredictionKey CueKey;
	TestTrue(
		TEXT("The owning client plays the cue from replication, because it carries no client prediction key"),
		FindActiveCuePredictionKey(Harvester.AbilitySystem, TAG_HarvestStrideTest_Cue, CueKey) && !CueKey.IsValidKey());
	TestTrue(TEXT("The first pulse harvests around the harvester at once"), IsEmpty(Start));
	TestTrue(TEXT("Resources ahead wait for the harvester"), IsFull(Middle, 4) && IsFull(End, 4));
	TestEqual(TEXT("The rewards wait for the end of the stride"), CountMaterial(Harvester.PlayerState), 0);

	// Timers fire once the time passes their deadline, so every step lands just after a pulse.
	Harvester.Pawn->SetActorLocation(FVector(900.0, 0.0, 0.0));
	TestWorld.AdvanceTimers(0.55f);
	TestTrue(TEXT("A pulse harvests where the harvester walked"), IsEmpty(Middle));
	TestTrue(TEXT("The resource ahead still waits"), IsFull(End, 4));
	TestEqual(TEXT("The rewards still wait"), CountMaterial(Harvester.PlayerState), 0);

	Harvester.Pawn->SetActorLocation(FVector(1600.0, 0.0, 0.0));
	TestWorld.AdvanceTimers(0.5f);
	TestTrue(TEXT("The next pulse harvests further along the path"), IsEmpty(End));

	TestWorld.AdvanceTimers(0.5f);
	TestTrue(TEXT("The stride runs until its duration is over"), Stride.Instance->IsStriding());
	TestWorld.AdvanceTimers(0.5f);
	TestFalse(TEXT("The stride ends after its duration"), Stride.Instance->IsStriding());
	TestFalse(TEXT("The ability ends with the stride"), IsAbilityActive(Harvester.AbilitySystem, Stride));
	TestFalse(TEXT("The stride's cue is removed"), Harvester.AbilitySystem->HasMatchingGameplayTag(TAG_HarvestStrideTest_Cue));
	TestEqual(
		TEXT("The rewards of every pulse arrive when the stride ends"),
		CountMaterial(Harvester.PlayerState),
		3 * 4 * YieldPerSection);
	TestEqual(TEXT("A fitting stride reward spawns no drop"), GetWorldDrops(World).Num(), 0);

	Harvester.Pawn->SetActorLocation(FVector(2600.0, 0.0, 0.0));
	TestWorld.AdvanceTimers(0.5f);
	TestTrue(TEXT("Nothing is harvested after the stride"), IsFull(Later, 4));
	TestTrue(TEXT("A resource beside the path stays untouched"), IsFull(Beside, 4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestStridePulseTest,
	"SurvivalRpg.Harvesting.Stride.EachPulseTakesUpToMaxTargets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestStridePulseTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestStrideTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()) ||
		!TestTrue(TEXT("The harvester can walk"), MakeWalkable(Harvester)))
	{
		return false;
	}

	// Three resources around a standing harvester, two pulses of stock each, and two targets per pulse.
	ARpgHarvestAutomationCollidableNodeActor* Nearest = SpawnNode(World, FVector(100.0, 0.0, 0.0), MakeProfile(World, 2));
	ARpgHarvestAutomationCollidableNodeActor* Second = SpawnNode(World, FVector(0.0, 150.0, 0.0), MakeProfile(World, 2));
	ARpgHarvestAutomationCollidableNodeActor* Third = SpawnNode(World, FVector(-200.0, 0.0, 0.0), MakeProfile(World, 2));
	const FGrantedAbility Stride = GrantStrideAbility(Harvester.AbilitySystem, 2, 1);
	if (!TestTrue(TEXT("Every resource exists"), Nearest && Second && Third) ||
		!TestNotNull(TEXT("Stride ability exists"), Stride.Instance))
	{
		return false;
	}
	TestWorld.PrimeTimerManager();

	const FRpgHarvestPreview Preview = Evaluate(Harvester.AbilitySystem, Stride);
	TestTrue(TEXT("The preview shows an area"), Preview.bHasArea && FMath::IsNearlyEqual(Preview.AreaRadius, 300.0f));
	TestTrue(TEXT("The area is centered on the harvester"), Preview.AimPoint.Equals(Harvester.Pawn->GetActorLocation(), 1.0));
	TestTrue(
		TEXT("The preview selects the two nearest resources"),
		Preview.Targets.Num() == 2 && Preview.Targets[0].WouldHarvest() && Preview.Targets[1].WouldHarvest() &&
			Preview.Targets[0].Receiver.Get() == Nearest->HarvestableNode &&
			Preview.Targets[1].Receiver.Get() == Second->HarvestableNode);

	TestTrue(TEXT("The stride activates"), Harvester.AbilitySystem->TryActivateAbility(Stride.Handle));
	TestTrue(
		TEXT("A pulse takes one strike from each of the nearest resources"),
		IsFull(Nearest, 1) && IsFull(Second, 1) && IsFull(Third, 2));
	TestWorld.AdvanceTimers(0.55f);
	TestTrue(TEXT("The next pulse strikes them again"), IsEmpty(Nearest) && IsEmpty(Second) && IsFull(Third, 2));
	TestWorld.AdvanceTimers(0.5f);
	TestTrue(TEXT("Emptied resources no longer count against the limit"), IsFull(Third, 1));
	TestWorld.AdvanceTimers(0.5f);
	TestTrue(TEXT("The last pulse empties the third resource"), IsEmpty(Third));
	TestWorld.AdvanceTimers(0.5f);
	TestFalse(TEXT("The stride is over"), Stride.Instance->IsStriding());
	TestEqual(TEXT("Every section arrives once"), CountMaterial(Harvester.PlayerState), 3 * 2 * YieldPerSection);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestStrideEarlyEndTest,
	"SurvivalRpg.Harvesting.Stride.EndingEarlyDeliversWhatWasHarvested",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestStrideEarlyEndTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestStrideTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture FullHarvester = SpawnHarvester(World);
	FHarvesterFixture DyingHarvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixtures exist"), FullHarvester.IsValid() && DyingHarvester.IsValid()) ||
		!TestTrue(TEXT("The harvesters can walk"), MakeWalkable(FullHarvester) && MakeWalkable(DyingHarvester)))
	{
		return false;
	}
	DyingHarvester.Pawn->SetActorLocation(FVector(5000.0, 0.0, 0.0));
	TestWorld.PrimeTimerManager();

	// A tool switch removes the ability mid-stride: the harvest so far arrives as one drop from a full inventory.
	URpgInventoryManagerComponent* FullInventory = FullHarvester.PlayerState->GetInventoryManagerComponent();
	FullInventory->SetFixedMaxEntries(0);
	FullInventory->SetCapacityMode(ERpgInventoryCapacityMode::FixedEntries);
	ARpgHarvestAutomationCollidableNodeActor* First = SpawnNode(World, FVector(150.0, 0.0, 0.0), MakeProfile(World, 4));
	const FGrantedAbility Switched =
		GrantStrideAbility(FullHarvester.AbilitySystem, 4, URpgHarvestProfile::MaxSectionCount, TAG_HarvestStrideTest_Cue);
	if (!TestNotNull(TEXT("First resource exists"), First) || !TestNotNull(TEXT("Stride ability exists"), Switched.Instance))
	{
		return false;
	}
	TestTrue(TEXT("The stride activates"), FullHarvester.AbilitySystem->TryActivateAbility(Switched.Handle));
	TestTrue(TEXT("The first pulse harvests"), IsEmpty(First));
	TestEqual(TEXT("Nothing is dropped while the stride runs"), GetWorldDrops(World).Num(), 0);

	FullHarvester.AbilitySystem->ClearAbility(Switched.Handle);
	TArray<ARpgDroppedInventoryActor*> Drops = GetWorldDrops(World);
	if (!TestEqual(TEXT("Removing the ability delivers the harvest in exactly one drop"), Drops.Num(), 1))
	{
		return false;
	}
	const URpgInventoryManagerComponent* DropInventory = Drops[0]->GetLootInventoryManager();
	TestTrue(
		TEXT("The drop holds the whole harvest"),
		DropInventory &&
			DropInventory->GetTotalItemCountByDefinition(URpgHarvestAutomationTestStackItemDefinition::StaticClass()) ==
				4 * YieldPerSection);
	TestTrue(
		TEXT("The drop lands at the harvester"),
		FVector::Dist2D(Drops[0]->GetActorLocation(), FullHarvester.Pawn->GetActorLocation()) < 100.0);
	TestFalse(TEXT("The cue ends with the stride"), FullHarvester.AbilitySystem->HasMatchingGameplayTag(TAG_HarvestStrideTest_Cue));
	ARpgHarvestAutomationCollidableNodeActor* AfterSwitch = SpawnNode(World, FVector(0.0, 150.0, 0.0), MakeProfile(World, 4));
	TestWorld.AdvanceTimers(0.5f);
	TestTrue(TEXT("Nothing is harvested after the tool switch"), IsFull(AfterSwitch, 4));
	TestEqual(TEXT("No further drop appears"), GetWorldDrops(World).Num(), 1);

	// A harvester that dies mid-stride harvests nothing more, and receives what it harvested before.
	ARpgHarvestAutomationCollidableNodeActor* BeforeDeath =
		SpawnNode(World, FVector(5150.0, 0.0, 0.0), MakeProfile(World, 4));
	const FGrantedAbility Dying = GrantStrideAbility(DyingHarvester.AbilitySystem, 4, URpgHarvestProfile::MaxSectionCount);
	if (!TestNotNull(TEXT("Resource before death exists"), BeforeDeath) || !TestNotNull(TEXT("Stride ability exists"), Dying.Instance))
	{
		return false;
	}
	TestTrue(TEXT("The second stride activates"), DyingHarvester.AbilitySystem->TryActivateAbility(Dying.Handle));
	TestTrue(TEXT("The second stride harvests"), IsEmpty(BeforeDeath));
	DyingHarvester.AbilitySystem->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("Status.Death")));
	ARpgHarvestAutomationCollidableNodeActor* AfterDeath = SpawnNode(World, FVector(5000.0, 150.0, 0.0), MakeProfile(World, 4));
	TestWorld.AdvanceTimers(0.55f);
	TestTrue(TEXT("A dead harvester harvests nothing more"), IsFull(AfterDeath, 4));
	TestFalse(TEXT("Death ends the stride"), Dying.Instance->IsStriding() || IsAbilityActive(DyingHarvester.AbilitySystem, Dying));
	TestEqual(
		TEXT("What the stride harvested before the death arrives"),
		CountMaterial(DyingHarvester.PlayerState),
		4 * YieldPerSection);
	return true;
}

namespace RpgHarvestChainTests
{
	using namespace RpgHarvestAbilityTests;

	/** Spawns a chain box of Extent at Location that takes up to MaxChainedTargets resources at ChainSpeed. */
	ARpgHarvestAutomationChainActor* SpawnChain(
		UWorld* World,
		const FVector& Location,
		const FVector& Extent,
		const int32 MaxChainedTargets,
		const float ChainSpeed)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ARpgHarvestAutomationChainActor* Actor = World
			? World->SpawnActor<ARpgHarvestAutomationChainActor>(
				ARpgHarvestAutomationChainActor::StaticClass(),
				FTransform(Location),
				SpawnParameters)
			: nullptr;
		if (!Actor || !Actor->Chain)
		{
			return nullptr;
		}
		Actor->Chain->SetBoxExtent(Extent);
		Actor->Chain->ConfigureChain(MaxChainedTargets, ChainSpeed);
		if (!Actor->HasActorBegunPlay())
		{
			Actor->DispatchBeginPlay();
		}
		return Actor;
	}

	/** Builds the committed result of Request on its instance, the way a harvest passes its trigger to the chain. */
	FRpgHarvestTargetEvaluation MakeTrigger(
		URpgHarvestableInstancesComponent* Instances,
		const FRpgHarvestRequest& Request,
		const FRpgHarvestResult& Result)
	{
		FRpgHarvestTargetEvaluation Trigger;
		Trigger.Receiver = Instances;
		Trigger.Hit = Request.Hit;
		Trigger.Result = Result;
		Trigger.bInReach = true;
		return Trigger;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestChainSwingTest,
	"SurvivalRpg.Harvesting.Chain.FellingOneTreeFellsTheGrove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestChainSwingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestChainTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	const float EyeHeight = Harvester.Pawn->BaseEyeHeight;
	URpgHarvestInstanceStockComponent* Stock = RpgHarvestAutomation::AddInstanceStock(World);
	// The aimed tree, three grove trees 250, 300 and 350 cm from it, and one tree outside the grove.
	ARpgHarvestAutomationInstancesActor* Field = RpgHarvestAutomation::SpawnInstances(
		World,
		MakeProfile(World, 2),
		{FVector(0.0, 0.0, 0.0), FVector(0.0, 250.0, 0.0), FVector(0.0, -300.0, 0.0), FVector(350.0, 0.0, 0.0),
			FVector(0.0, 700.0, 0.0)},
		FVector(400.0, 0.0, EyeHeight));
	ARpgHarvestAutomationChainActor* Grove =
		SpawnChain(World, FVector(500.0, 0.0, EyeHeight), FVector(350.0, 400.0, 200.0), 8, 500.0f);
	const FGrantedAbility Swing = GrantAbility(Harvester.AbilitySystem);
	if (!TestNotNull(TEXT("Instance stock exists"), Stock) ||
		!TestNotNull(TEXT("Trees exist"), Field) ||
		!TestNotNull(TEXT("Grove exists"), Grove) ||
		!TestNotNull(TEXT("Swing exists"), Swing.Instance))
	{
		return false;
	}
	FRpgHarvestTargetingParams Single;
	Single.MaxAimDistance = 1000.0f;
	Single.MaxReachFromAvatar = 500.0f;
	Swing.Instance->ConfigureTargeting(Single);
	TestWorld.PrimeTimerManager();
	URpgHarvestAutomationInstancesComponent* Trees = Field->Instances;

	FRpgHarvestPreview Preview = Evaluate(Harvester.AbilitySystem, Swing);
	TestEqual(TEXT("A swing that leaves stock previews no chain"), Preview.Targets.Num(), 1);
	TestTrue(TEXT("The first swing activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TestEqual(TEXT("The first swing takes one section"), Trees->GetRemainingSections(0), 1);
	for (int32 TreeIndex = 1; TreeIndex < 5; ++TreeIndex)
	{
		TestEqual(TEXT("A partial harvest leaves the grove standing"), Trees->GetRemainingSections(TreeIndex), 2);
	}

	Preview = Evaluate(Harvester.AbilitySystem, Swing);
	int32 NumChained = 0;
	bool bChainedTakeWholeStock = true;
	for (const FRpgHarvestTargetEvaluation& Target : Preview.Targets)
	{
		if (Target.bChained)
		{
			++NumChained;
			bChainedTakeWholeStock &=
				Target.WouldHarvest() && Target.Result.SectionsTaken == 2 && Target.Result.RemainingSections == 0;
		}
	}
	TestEqual(TEXT("The felling swing previews the three other grove trees"), NumChained, 3);
	TestTrue(TEXT("Each chained tree previews its whole stock"), bChainedTakeWholeStock);
	TestEqual(TEXT("The preview leaves out the tree outside the grove"), Preview.Targets.Num(), 4);
	TestTrue(TEXT("The aimed tree is the swing's own target"), !Preview.Targets.IsEmpty() && !Preview.Targets[0].bChained);

	TestTrue(TEXT("The felling swing activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	for (int32 TreeIndex = 0; TreeIndex < 4; ++TreeIndex)
	{
		TestEqual(TEXT("The grove falls with the felled tree"), Trees->GetRemainingSections(TreeIndex), 0);
	}
	TestEqual(TEXT("The tree outside the grove stands"), Trees->GetRemainingSections(4), 2);
	TestEqual(
		TEXT("The harvester receives the wood of the whole grove"),
		CountMaterial(Harvester.PlayerState),
		4 * 2 * YieldPerSection);

	FIntVector NearKey;
	FIntVector FarKey;
	Trees->GetInstanceKey(1, NearKey);
	Trees->GetInstanceKey(3, FarKey);
	TestTrue(
		TEXT("The chain reaches a tree after its distance at the chain speed"),
		FMath::IsNearlyEqual(Stock->GetRemainingPresentationDelay(NearKey), 0.5f, 0.02f));
	TestTrue(
		TEXT("Farther trees fall later"),
		FMath::IsNearlyEqual(Stock->GetRemainingPresentationDelay(FarKey), 0.7f, 0.02f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestChainRulesTest,
	"SurvivalRpg.Harvesting.Chain.ChainFollowsAreaHarvestRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestChainRulesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestChainTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	const FVector FieldLocation(0.0, 3000.0, 0.0);
	URpgHarvestInstanceStockComponent* Stock = RpgHarvestAutomation::AddInstanceStock(World);
	// Six one-section trees in a row, 200 cm apart, and a vein that needs another tool beside them.
	ARpgHarvestAutomationInstancesActor* Field = RpgHarvestAutomation::SpawnInstances(
		World,
		MakeProfile(World, 1),
		{FVector(0.0, 0.0, 0.0), FVector(200.0, 0.0, 0.0), FVector(400.0, 0.0, 0.0), FVector(600.0, 0.0, 0.0),
			FVector(800.0, 0.0, 0.0), FVector(1000.0, 0.0, 0.0)},
		FieldLocation);
	URpgHarvestProfile* VeinProfile = MakeProfile(World, 1);
	VeinProfile->RequiredToolTag = RpgHarvestingMagicGameplayTags::Tool_Harvesting_Skinning;
	ARpgHarvestAutomationInstancesActor* Vein =
		RpgHarvestAutomation::SpawnInstances(World, VeinProfile, {FVector(300.0, 100.0, 0.0)}, FieldLocation);
	ARpgHarvestAutomationChainActor* Row =
		SpawnChain(World, FieldLocation + FVector(500.0, 0.0, 0.0), FVector(600.0, 300.0, 200.0), 3, 0.0f);

	// A camp protects the third tree.
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgHarvestAutomationProtectionActor* Camp = World->SpawnActor<ARpgHarvestAutomationProtectionActor>(
		ARpgHarvestAutomationProtectionActor::StaticClass(),
		FTransform(FieldLocation + FVector(400.0, 0.0, 0.0)),
		SpawnParameters);
	if (!TestNotNull(TEXT("Instance stock exists"), Stock) ||
		!TestNotNull(TEXT("Trees exist"), Field) ||
		!TestNotNull(TEXT("Vein exists"), Vein) ||
		!TestNotNull(TEXT("Chain exists"), Row) ||
		!TestNotNull(TEXT("Camp exists"), Camp))
	{
		return false;
	}
	Camp->Protection->SetBoxExtent(FVector(50.0, 50.0, 100.0));
	if (!Camp->HasActorBegunPlay())
	{
		Camp->DispatchBeginPlay();
	}
	TestWorld.PrimeTimerManager();
	URpgHarvestAutomationInstancesComponent* Trees = Field->Instances;

	TSet<TObjectKey<URpgHarvestChainComponent>> ChainedBoxes;
	TArray<FRpgHarvestTargetEvaluation> Chained;
	FRpgHarvestRequest Request = RpgHarvestAutomation::MakeInstanceRequest(Trees, 0, Harvester.Pawn);
	Request.Provenance.RootActionId = 42;
	FRpgHarvestResult Partial;
	Partial.Outcome = ERpgHarvestOutcome::Harvested;
	Partial.SectionsTaken = 1;
	Partial.RemainingSections = 1;
	TestEqual(
		TEXT("A harvest that leaves stock does not chain"),
		FRpgHarvestChains::Commit(*World, Request, MakeTrigger(Trees, Request, Partial), ChainedBoxes, Chained),
		0);
	TestEqual(TEXT("Nothing is chained without a depletion"), Chained.Num(), 0);

	const FRpgHarvestResult Felled = Trees->CommitHarvest_Implementation(Request);
	TestTrue(TEXT("The first tree is felled"), Felled.bDepleted);
	const FRpgHarvestTargetEvaluation Trigger = MakeTrigger(Trees, Request, Felled);
	TestEqual(
		TEXT("The chain takes three sections"),
		FRpgHarvestChains::Commit(*World, Request, Trigger, ChainedBoxes, Chained),
		3);
	TestEqual(TEXT("The chain stops at its limit"), Chained.Num(), 3);
	TestFalse(
		TEXT("Every chained result is marked"),
		Chained.ContainsByPredicate([](const FRpgHarvestTargetEvaluation& Target) { return !Target.bChained; }));
	TestFalse(
		TEXT("Chained work is follow-up work of the trigger's action, never a direct hit"),
		Chained.ContainsByPredicate([](const FRpgHarvestTargetEvaluation& Target)
		{
			return Target.Result.Provenance.IsDirect() || Target.Result.Provenance.RootActionId != 42 ||
				Target.Result.IsAcceptedDirectHit();
		}));
	TestFalse(
		TEXT("Finishing a chained tree reports no overkill"),
		Chained.ContainsByPredicate([](const FRpgHarvestTargetEvaluation& Target) { return Target.Result.OverkillWork != 0; }));
	TestEqual(TEXT("The nearest tree falls"), Trees->GetRemainingSections(1), 0);
	TestEqual(TEXT("A protected tree is skipped"), Trees->GetRemainingSections(2), 1);
	TestEqual(TEXT("The chain continues past it"), Trees->GetRemainingSections(3), 0);
	TestEqual(TEXT("The chain takes the next tree"), Trees->GetRemainingSections(4), 0);
	TestEqual(TEXT("A tree beyond the limit stands"), Trees->GetRemainingSections(5), 1);
	TestEqual(TEXT("A resource that needs another tool is skipped"), Vein->Instances->GetRemainingSections(0), 1);
	TestEqual(TEXT("The harvester receives the felled and chained wood"), CountMaterial(Harvester.PlayerState), 4 * YieldPerSection);

	TestEqual(
		TEXT("One harvest runs a chain box once"),
		FRpgHarvestChains::Commit(*World, Request, Trigger, ChainedBoxes, Chained),
		0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestChainSwarmTest,
	"SurvivalRpg.Harvesting.Chain.SwarmStrikeFellsTheGrove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestChainSwarmTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestChainTests;
	using namespace RpgHarvestSwarmTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	const float EyeHeight = Harvester.Pawn->BaseEyeHeight;
	URpgHarvestInstanceStockComponent* Stock = RpgHarvestAutomation::AddInstanceStock(World);
	ARpgHarvestAutomationInstancesActor* Grove = SpawnGrove(World, EyeHeight);
	ARpgHarvestAutomationChainActor* Roots =
		SpawnChain(World, FVector(450.0, 150.0, EyeHeight), FVector(300.0, 300.0, 200.0), 8, 0.0f);
	// One creature works on the nearest tree only; the roots take the other two.
	const FGrantedAbility Summon = GrantSwarmAbility(Harvester.AbilitySystem, 1);
	if (!TestNotNull(TEXT("Instance stock exists"), Stock) ||
		!TestNotNull(TEXT("Grove exists"), Grove) ||
		!TestNotNull(TEXT("Roots exist"), Roots) ||
		!TestNotNull(TEXT("Swarm ability exists"), Summon.Instance))
	{
		return false;
	}
	FRpgHarvestTargetingParams OneTree;
	OneTree.Shape = ERpgHarvestTargetShape::AreaAtAimPoint;
	OneTree.MaxAimDistance = 1000.0f;
	OneTree.MaxReachFromAvatar = 700.0f;
	OneTree.AreaRadius = 450.0f;
	OneTree.MaxTargets = 1;
	Summon.Instance->ConfigureTargeting(OneTree);
	TestWorld.PrimeTimerManager();
	URpgHarvestAutomationInstancesComponent* Trees = Grove->Instances;

	const FRpgHarvestPreview Preview = Evaluate(Harvester.AbilitySystem, Summon);
	int32 NumChained = 0;
	for (const FRpgHarvestTargetEvaluation& Target : Preview.Targets)
	{
		NumChained += Target.bChained && Target.WouldHarvest() ? 1 : 0;
	}
	TestEqual(TEXT("The swarm preview marks the trees the roots take"), NumChained, 2);

	TestTrue(TEXT("The swarm is summoned"), Harvester.AbilitySystem->TryActivateAbility(Summon.Handle));
	ARpgHarvestSwarm* Swarm = FindActiveSwarm(World);
	if (!TestNotNull(TEXT("The commit summons a swarm"), Swarm))
	{
		return false;
	}
	for (int32 Frame = 0; Frame < 250 && Trees->GetRemainingSections(0) == 4; ++Frame)
	{
		Advance(TestWorld, 0.02);
	}
	TestEqual(TEXT("The first strike takes two sections"), Trees->GetRemainingSections(0), 2);
	TestEqual(TEXT("A strike that leaves stock leaves the grove standing"), Trees->GetRemainingSections(1), 4);
	Advance(TestWorld, 4.0);
	for (int32 TreeIndex = 0; TreeIndex < 3; ++TreeIndex)
	{
		TestEqual(TEXT("The strike that fells the tree fells the grove"), Trees->GetRemainingSections(TreeIndex), 0);
	}
	TestTrue(TEXT("The swarm has finished"), Swarm->IsFinished());
	TestEqual(
		TEXT("The summoner receives the wood of the whole grove"),
		CountMaterial(Harvester.PlayerState),
		3 * 4 * YieldPerSection);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestWorkTalentTest,
	"SurvivalRpg.Harvesting.Tuning.WorkTalentsShapeOwnHits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestWorkTalentTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestTuningTests;
	using namespace RpgHarvestingMagicGameplayTags;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}

	// Talents of the Impact branch, as one-node tunings of every ability of the tool.
	URpgSkillTreeDefinition* Tree = NewObject<URpgSkillTreeDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
	Tree->TreeTag = TAG_HarvestWorkTest_Tree;
	Tree->MasterySkillTag = RpgTradeSkillGameplayTags::Skill_Gathering_Mining;
	Tree->MaxPoints = 10;
	FRpgSkillTreeNode& Cleave = Tree->Nodes.AddDefaulted_GetRef();
	Cleave.NodeTag = TAG_HarvestWorkTest_Cleave;
	Cleave.AbilityTunings = {MakeTuning(Ability_Tuning_Harvest_Work, ERpgSkillTreeTuningOperation::Multiply, 1.25f)};
	FRpgSkillTreeNode& Shatter = Tree->Nodes.AddDefaulted_GetRef();
	Shatter.NodeTag = TAG_HarvestWorkTest_Shatter;
	Shatter.Column = 1;
	Shatter.Cost = 3;
	Shatter.ExclusiveGroup = TEXT("Choice");
	Shatter.AbilityTunings = {MakeTuning(Ability_Tuning_Harvest_WeakPointWork, ERpgSkillTreeTuningOperation::Multiply, 2.0f)};
	FRpgSkillTreeNode& FollowUp = Tree->Nodes.AddDefaulted_GetRef();
	FollowUp.NodeTag = TAG_HarvestWorkTest_FollowUp;
	FollowUp.Column = 2;
	FollowUp.Cost = 3;
	FollowUp.ExclusiveGroup = TEXT("Choice");
	FollowUp.AbilityTunings = {
		MakeTuning(Ability_Tuning_Harvest_FollowUpWork, ERpgSkillTreeTuningOperation::Multiply, 1.5f),
		MakeTuning(Ability_Tuning_Harvest_FollowUpWindow, ERpgSkillTreeTuningOperation::Add, 3.0f)};

	FScopedTreeTool TreeTool(Tree);
	URpgSkillTreeComponent* SkillTrees = PrepareSkillTree(Harvester, Tree);
	URpgEquipmentInstance* Equipment = MakeTreeToolEquipment(Harvester);
	URpgHarvestProfile* Profile = MakeProfile(World, 16);
	Profile->WeakPointBonusSections = 1;
	ARpgHarvestAutomationCollidableNodeActor* Node =
		SpawnNode(World, FVector(150.0, 0.0, Harvester.Pawn->BaseEyeHeight), Profile);
	const FGrantedAbility Swing = GrantAbility(Harvester.AbilitySystem, Equipment);
	URpgHarvestAutomationHarvestedListener* Harvested = NewObject<URpgHarvestAutomationHarvestedListener>();
	// The view ray strikes the front face at its center; the weak point there is active at 0, 3, 6, ... sections.
	if (!TestNotNull(TEXT("Skill trees are prepared"), SkillTrees) ||
		!TestNotNull(TEXT("Tree tool equipment exists"), Equipment) ||
		!TestNotNull(TEXT("Node exists"), Node) ||
		!TestNotNull(TEXT("Swing exists"), Swing.Instance) ||
		!TestTrue(
			TEXT("Weak points are configured"),
			RpgHarvestAutomation::ConfigureWeakPoints(
				Node->HarvestableNode,
				{FVector(-40.0, 0.0, 0.0), FVector(-40.0, 0.0, 60.0), FVector(-40.0, 0.0, -60.0)},
				20.0f)))
	{
		return false;
	}
	FRpgHarvestTargetingParams Single;
	Single.MaxAimDistance = 1000.0f;
	Single.MaxReachFromAvatar = 250.0f;
	Swing.Instance->ConfigureTargeting(Single);
	Swing.Instance->ConfigureWeakPointHits(true);
	Node->HarvestableNode->OnHarvested.AddDynamic(Harvested, &URpgHarvestAutomationHarvestedListener::HandleHarvested);
	TestWorld.PrimeTimerManager();

	auto SwingOnce = [this, &Harvester, &Swing, Harvested](const TCHAR* What) -> FRpgHarvestResult
	{
		const int32 Before = Harvested->Results.Num();
		TestTrue(*FString::Printf(TEXT("%s: the swing activates"), What), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
		const bool bCommitted = Harvested->Results.Num() == Before + 1;
		TestTrue(*FString::Printf(TEXT("%s: the swing commits once"), What), bCommitted);
		return bCommitted ? Harvested->Results.Last() : FRpgHarvestResult();
	};

	TestEqual(
		TEXT("Cleave is learned"),
		SkillTrees->UnlockNode(TAG_HarvestWorkTest_Tree, TAG_HarvestWorkTest_Cleave),
		ERpgSkillTreeUnlockResult::Unlockable);
	const FRpgHarvestPreview Preview = Evaluate(Harvester.AbilitySystem, Swing);
	TestTrue(
		TEXT("The preview shows the tuned work"),
		Preview.Targets.Num() == 1 && Preview.Targets[0].Result.WorkApplied == 225 && Preview.Targets[0].Result.bWeakPointHit);
	FRpgHarvestResult Result = SwingOnce(TEXT("Weak point with Cleave"));
	TestTrue(TEXT("Cleave adds a quarter to the swing, and the weak point a section"), Result.WorkApplied == 225 && Result.bWeakPointHit);
	Result = SwingOnce(TEXT("Plain swing with Cleave"));
	TestTrue(TEXT("A plain swing does a quarter more work"), Result.WorkApplied == 125 && !Result.bWeakPointHit);

	TestEqual(
		TEXT("Shatter is learned"),
		SkillTrees->UnlockNode(TAG_HarvestWorkTest_Tree, TAG_HarvestWorkTest_Shatter),
		ERpgSkillTreeUnlockResult::Unlockable);
	Result = SwingOnce(TEXT("Weak point with Shatter"));
	TestTrue(TEXT("Shatter doubles the weak point's bonus work"), Result.WorkApplied == 125 + 200 && Result.bWeakPointHit);

	TestEqual(
		TEXT("Shatter is refunded"),
		SkillTrees->RefundNode(TAG_HarvestWorkTest_Tree, TAG_HarvestWorkTest_Shatter),
		ERpgSkillTreeRefundResult::Refundable);
	TestEqual(
		TEXT("Follow Up is learned"),
		SkillTrees->UnlockNode(TAG_HarvestWorkTest_Tree, TAG_HarvestWorkTest_FollowUp),
		ERpgSkillTreeUnlockResult::Unlockable);
	Result = SwingOnce(TEXT("Weak point with Follow Up"));
	TestTrue(TEXT("Without Shatter the weak point adds one section"), Result.WorkApplied == 225 && Result.bWeakPointHit);
	Swing.Instance->ConfigureWeakPointHits(false);
	Result = SwingOnce(TEXT("Follow-up swing"));
	TestEqual(TEXT("The next hit on the same target does half again as much work"), Result.WorkApplied, 188);
	Result = SwingOnce(TEXT("Swing after the follow-up"));
	TestEqual(TEXT("The follow-up is used up"), Result.WorkApplied, 125);

	Swing.Instance->ConfigureWeakPointHits(true);
	Result = SwingOnce(TEXT("Weak point before a pause"));
	TestTrue(TEXT("Another weak-point hit arms the follow-up again"), Result.bWeakPointHit);
	TestWorld.AdvanceTimers(4.0f);
	Swing.Instance->ConfigureWeakPointHits(false);
	Result = SwingOnce(TEXT("Swing after the window"));
	TestEqual(TEXT("A follow-up expires after its window"), Result.WorkApplied, 125);
	TestFalse(
		TEXT("Every swing is the harvester's own accepted hit"),
		Harvested->Results.ContainsByPredicate([](const FRpgHarvestResult& Committed) { return !Committed.IsAcceptedDirectHit(); }));
	return true;
}

namespace RpgHarvestRuneFlowTests
{
	using namespace RpgHarvestAbilityTests;
	using namespace RpgHarvestTuningTests;

	/**
	 * One-point nodes on Mining that tune every ability of the tool like the Runenfluss runes and talents: Echo (half
	 * a hit's work), EveryHit (an echo on every own hit), Transfer (all of the overkill) and Split (two strikes of 60 %).
	 */
	URpgSkillTreeDefinition* MakeRuneFlowTree()
	{
		using namespace RpgHarvestingMagicGameplayTags;

		URpgSkillTreeDefinition* Tree = NewObject<URpgSkillTreeDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		Tree->TreeTag = TAG_HarvestTuningTest_Tree;
		Tree->MasterySkillTag = RpgTradeSkillGameplayTags::Skill_Gathering_Mining;
		Tree->MaxPoints = 10;
		auto AddNode = [Tree](const FGameplayTag NodeTag, const int32 Column, const FRpgSkillTreeAbilityTuning& Tuning)
		{
			FRpgSkillTreeNode& Node = Tree->Nodes.AddDefaulted_GetRef();
			Node.NodeTag = NodeTag;
			Node.Column = Column;
			Node.AbilityTunings = {Tuning};
		};
		AddNode(TAG_HarvestRuneFlowTest_Echo, 0, MakeTuning(Ability_Tuning_Harvest_EchoWork, ERpgSkillTreeTuningOperation::Set, 0.5f));
		AddNode(TAG_HarvestRuneFlowTest_EveryHit, 1, MakeTuning(Ability_Tuning_Harvest_EchoHits, ERpgSkillTreeTuningOperation::Set, 1.0f));
		AddNode(TAG_HarvestRuneFlowTest_Transfer, 2, MakeTuning(Ability_Tuning_Harvest_TransferWork, ERpgSkillTreeTuningOperation::Set, 1.0f));
		AddNode(TAG_HarvestRuneFlowTest_Split, 3, MakeTuning(Ability_Tuning_Harvest_RuneFlowSplit, ERpgSkillTreeTuningOperation::Set, 0.6f));
		return Tree;
	}

	/** Residual work on the partly worked section of Node, in hundredths of a section. */
	int32 SectionWork(const ARpgHarvestAutomationCollidableNodeActor* Node)
	{
		return Node ? static_cast<int32>(Node->HarvestableNode->GetHarvestState().SectionWork) : INDEX_NONE;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestEchoAbilityTest,
	"SurvivalRpg.Harvesting.Ability.EchoRepeatsEveryThirdOwnHit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestEchoAbilityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestRuneFlowTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	const double Eye = Harvester.Pawn->BaseEyeHeight;
	ARpgHarvestAutomationCollidableNodeActor* Struck = SpawnNode(World, FVector(150.0, 0.0, Eye), MakeProfile(World, 6));
	ARpgHarvestAutomationCollidableNodeActor* Neighbor = SpawnNode(World, FVector(150.0, 300.0, Eye), MakeProfile(World, 4));
	URpgSkillTreeDefinition* Tree = MakeRuneFlowTree();
	FScopedTreeTool TreeTool(Tree);
	URpgSkillTreeComponent* SkillTrees = PrepareSkillTree(Harvester, Tree);
	URpgEquipmentInstance* Equipment = MakeTreeToolEquipment(Harvester);
	const FGrantedAbility Swing = GrantAbility(Harvester.AbilitySystem, Equipment);
	if (!TestNotNull(TEXT("Struck node exists"), Struck) || !TestNotNull(TEXT("Neighbor node exists"), Neighbor) ||
		!TestNotNull(TEXT("Skill trees are prepared"), SkillTrees) || !TestNotNull(TEXT("Swing exists"), Swing.Instance))
	{
		return false;
	}
	Swing.Instance->ConfigureRuneFlow(ARpgHarvestRuneStrike::StaticClass(), ARpgHarvestRuneStrike::StaticClass(), 800.0f);
	TestWorld.PrimeTimerManager();

	TestTrue(TEXT("A swing without echo work activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TestEqual(TEXT("Echo is learned"),
		SkillTrees->UnlockNode(TAG_HarvestTuningTest_Tree, TAG_HarvestRuneFlowTest_Echo), ERpgSkillTreeUnlockResult::Unlockable);
	TestTrue(TEXT("The first own hit activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TestTrue(TEXT("The second own hit activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TestWorld.AdvanceTimers(0.35f);
	TestEqual(TEXT("Two own hits do not echo"), SectionWork(Struck), 0);
	TestTrue(TEXT("The third own hit activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TestEqual(TEXT("The hits took four sections"), Struck->HarvestableNode->GetRemainingSections(), 2);
	TestEqual(TEXT("The echo waits for its delay"), SectionWork(Struck), 0);
	TestWorld.AdvanceTimers(0.35f);
	TestEqual(TEXT("The third own hit echoes with half its work"), SectionWork(Struck), 50);
	TestEqual(TEXT("The echo stays on the struck resource"), SectionWork(Neighbor), 0);
	TestEqual(TEXT("The echo adds no yield"), CountMaterial(Harvester.PlayerState), 4 * YieldPerSection);

	// Every own hit echoes, and split, the echo also strikes the nearest other resource; each strike does 60 %.
	TestEqual(TEXT("EveryHit is learned"),
		SkillTrees->UnlockNode(TAG_HarvestTuningTest_Tree, TAG_HarvestRuneFlowTest_EveryHit), ERpgSkillTreeUnlockResult::Unlockable);
	TestEqual(TEXT("Split is learned"),
		SkillTrees->UnlockNode(TAG_HarvestTuningTest_Tree, TAG_HarvestRuneFlowTest_Split), ERpgSkillTreeUnlockResult::Unlockable);
	TestTrue(TEXT("A further own hit activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TestWorld.AdvanceTimers(0.35f);
	TestEqual(TEXT("The split echo does 30 units on the struck resource"), SectionWork(Struck), 80);
	TestEqual(TEXT("The split echo does 30 units on the nearest other resource"), SectionWork(Neighbor), 30);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestTransferAbilityTest,
	"SurvivalRpg.Harvesting.Ability.TransferCarriesOverkillToTheNextResource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestTransferAbilityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestRuneFlowTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	const double Eye = Harvester.Pawn->BaseEyeHeight;
	// Three groups 20 m apart on the harvester's view ray: each has a struck resource on the ray and neighbors 3 m and
	// 5 m beside it. A group's struck resource is removed once its case is done, so the next swing reaches the next one.
	auto SpawnGroup = [World, Eye](const double X, const int32 StruckSections, ARpgHarvestAutomationCollidableNodeActor*& OutNear,
		ARpgHarvestAutomationCollidableNodeActor*& OutFar)
	{
		OutNear = SpawnNode(World, FVector(X, 300.0, Eye), MakeProfile(World, 4));
		OutFar = SpawnNode(World, FVector(X, -500.0, Eye), MakeProfile(World, 4));
		return SpawnNode(World, FVector(X, 0.0, Eye), MakeProfile(World, StruckSections));
	};
	ARpgHarvestAutomationCollidableNodeActor* ExactNear = nullptr;
	ARpgHarvestAutomationCollidableNodeActor* ExactFar = nullptr;
	ARpgHarvestAutomationCollidableNodeActor* Exact = SpawnGroup(150.0, 2, ExactNear, ExactFar);
	ARpgHarvestAutomationCollidableNodeActor* SingleNear = nullptr;
	ARpgHarvestAutomationCollidableNodeActor* SingleFar = nullptr;
	ARpgHarvestAutomationCollidableNodeActor* Single = SpawnGroup(2150.0, 1, SingleNear, SingleFar);
	ARpgHarvestAutomationCollidableNodeActor* SplitNear = nullptr;
	ARpgHarvestAutomationCollidableNodeActor* SplitFar = nullptr;
	ARpgHarvestAutomationCollidableNodeActor* Split = SpawnGroup(4150.0, 1, SplitNear, SplitFar);
	URpgSkillTreeDefinition* Tree = MakeRuneFlowTree();
	FScopedTreeTool TreeTool(Tree);
	URpgSkillTreeComponent* SkillTrees = PrepareSkillTree(Harvester, Tree);
	URpgEquipmentInstance* Equipment = MakeTreeToolEquipment(Harvester);
	const FGrantedAbility Swing = GrantAbility(Harvester.AbilitySystem, Equipment);
	if (!Exact || !Single || !Split || !SplitNear || !SplitFar || !TestNotNull(TEXT("Skill trees are prepared"), SkillTrees) ||
		!TestNotNull(TEXT("Swing exists"), Swing.Instance))
	{
		return false;
	}
	Swing.Instance->ConfigureRuneFlow(ARpgHarvestRuneStrike::StaticClass(), ARpgHarvestRuneStrike::StaticClass(), 800.0f);
	FRpgHarvestTargetingParams FarReach;
	FarReach.MaxAimDistance = 6000.0f;
	FarReach.MaxReachFromAvatar = 6000.0f;
	Swing.Instance->ConfigureTargeting(FarReach);
	TestEqual(TEXT("Transfer is learned"),
		SkillTrees->UnlockNode(TAG_HarvestTuningTest_Tree, TAG_HarvestRuneFlowTest_Transfer), ERpgSkillTreeUnlockResult::Unlockable);
	URpgHarvestAutomationHarvestedListener* NearResults = NewObject<URpgHarvestAutomationHarvestedListener>();
	SingleNear->HarvestableNode->OnHarvested.AddDynamic(NearResults, &URpgHarvestAutomationHarvestedListener::HandleHarvested);
	TestWorld.PrimeTimerManager();

	// A hit that does not empty its resource, and one that empties it exactly, carry nothing.
	TestTrue(TEXT("A partial hit activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TestTrue(TEXT("An exactly emptying hit activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TestWorld.AdvanceTimers(0.35f);
	TestFalse(TEXT("The exact hit empties its resource"), Exact->HarvestableNode->IsHarvestable());
	TestTrue(
		TEXT("Without overkill no other resource is worked"),
		ExactNear->HarvestableNode->GetRemainingSections() == 4 && ExactFar->HarvestableNode->GetRemainingSections() == 4);

	// Three sections on a one-section resource: two sections of overkill carry to the nearest other resource.
	Exact->Destroy();
	Swing.Instance->ConfigureSections(3);
	TestTrue(TEXT("An overkilling hit activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TestFalse(TEXT("The overkilling hit empties its resource"), Single->HarvestableNode->IsHarvestable());
	TestWorld.AdvanceTimers(0.35f);
	TestEqual(TEXT("The overkill works the nearest other resource"), SingleNear->HarvestableNode->GetRemainingSections(), 2);
	TestEqual(TEXT("A farther resource is not worked"), SingleFar->HarvestableNode->GetRemainingSections(), 4);
	TestEqual(TEXT("The carried sections reward the harvester"), CountMaterial(Harvester.PlayerState), 5 * YieldPerSection);
	if (TestEqual(TEXT("The neighbor reports the carried work"), NearResults->Results.Num(), 1))
	{
		TestTrue(
			TEXT("Carried work is follow-up work, not an own hit"),
			!NearResults->Results[0].Provenance.IsDirect() && NearResults->Results[0].Provenance.RootActionId > 0);
	}

	// Split: the overkill goes to the two nearest other resources, 60 % each.
	Single->Destroy();
	TestEqual(TEXT("Split is learned"),
		SkillTrees->UnlockNode(TAG_HarvestTuningTest_Tree, TAG_HarvestRuneFlowTest_Split), ERpgSkillTreeUnlockResult::Unlockable);
	TestTrue(TEXT("A split overkilling hit activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TestWorld.AdvanceTimers(0.35f);
	TestTrue(
		TEXT("Each of the two nearest other resources takes 1.2 sections"),
		SplitNear->HarvestableNode->GetRemainingSections() == 3 && SectionWork(SplitNear) == 20 &&
			SplitFar->HarvestableNode->GetRemainingSections() == 3 && SectionWork(SplitFar) == 20);
	return true;
}

namespace RpgHarvestSpreadTests
{
	using namespace RpgHarvestAbilityTests;
	using namespace RpgHarvestTuningTests;
	using RpgHarvestRuneFlowTests::SectionWork;

	/**
	 * One-point nodes on Mining that tune every ability of the tool like the Ausbreitung runes and talents: Leap (half
	 * a hit's work per leap), Pierce (all of it), Fork (two strikes of 60 % per step), Burn (half a hit's work per
	 * second for 4 s, like Brand) and Wander (the burn moves on when its resource runs empty).
	 */
	URpgSkillTreeDefinition* MakeSpreadTree()
	{
		using namespace RpgHarvestingMagicGameplayTags;

		URpgSkillTreeDefinition* Tree = NewObject<URpgSkillTreeDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		Tree->TreeTag = TAG_HarvestTuningTest_Tree;
		Tree->MasterySkillTag = RpgTradeSkillGameplayTags::Skill_Gathering_Mining;
		Tree->MaxPoints = 10;
		auto AddNode = [Tree](const FGameplayTag NodeTag, const int32 Column, const TArray<FRpgSkillTreeAbilityTuning>& Tunings)
		{
			FRpgSkillTreeNode& Node = Tree->Nodes.AddDefaulted_GetRef();
			Node.NodeTag = NodeTag;
			Node.Column = Column;
			Node.AbilityTunings = Tunings;
		};
		AddNode(TAG_HarvestSpreadTest_Leap, 0, {MakeTuning(Ability_Tuning_Harvest_LeapWork, ERpgSkillTreeTuningOperation::Set, 0.5f)});
		AddNode(TAG_HarvestSpreadTest_Pierce, 1, {MakeTuning(Ability_Tuning_Harvest_PierceWork, ERpgSkillTreeTuningOperation::Set, 1.0f)});
		AddNode(TAG_HarvestSpreadTest_Fork, 2, {MakeTuning(Ability_Tuning_Harvest_SpreadSplit, ERpgSkillTreeTuningOperation::Set, 0.6f)});
		AddNode(
			TAG_HarvestSpreadTest_Burn,
			3,
			{MakeTuning(Ability_Tuning_Harvest_BurnWork, ERpgSkillTreeTuningOperation::Set, 0.5f),
				MakeTuning(Ability_Tuning_Harvest_BurnDuration, ERpgSkillTreeTuningOperation::Set, 4.0f)});
		AddNode(TAG_HarvestSpreadTest_Wander, 4, {MakeTuning(Ability_Tuning_Harvest_BurnWander, ERpgSkillTreeTuningOperation::Set, 1.0f)});
		return Tree;
	}

	/** A burn tick of one second; timers fire once their time is exceeded, so a step lies just past it. */
	constexpr float BurnStep = 1.01f;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestLeapAbilityTest,
	"SurvivalRpg.Harvesting.Ability.LeapStrikesEachResourceOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestLeapAbilityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestSpreadTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	// The struck resource on the view ray, A 3 m and B 4.5 m beside it, C 4.5 m beyond A (7.5 m from the struck one)
	// and Far 15 m away. Leaps reach 6 m from the resource they left.
	const double Eye = Harvester.Pawn->BaseEyeHeight;
	ARpgHarvestAutomationCollidableNodeActor* Struck = SpawnNode(World, FVector(150.0, 0.0, Eye), MakeProfile(World, 6));
	ARpgHarvestAutomationCollidableNodeActor* A = SpawnNode(World, FVector(150.0, 300.0, Eye), MakeProfile(World, 4));
	ARpgHarvestAutomationCollidableNodeActor* B = SpawnNode(World, FVector(150.0, -450.0, Eye), MakeProfile(World, 4));
	ARpgHarvestAutomationCollidableNodeActor* C = SpawnNode(World, FVector(150.0, 750.0, Eye), MakeProfile(World, 4));
	ARpgHarvestAutomationCollidableNodeActor* Far = SpawnNode(World, FVector(150.0, -1500.0, Eye), MakeProfile(World, 4));
	URpgSkillTreeDefinition* Tree = MakeSpreadTree();
	FScopedTreeTool TreeTool(Tree);
	URpgSkillTreeComponent* SkillTrees = PrepareSkillTree(Harvester, Tree);
	URpgEquipmentInstance* Equipment = MakeTreeToolEquipment(Harvester);
	const FGrantedAbility Swing = GrantAbility(Harvester.AbilitySystem, Equipment);
	if (!Struck || !A || !B || !C || !Far || !TestNotNull(TEXT("Skill trees are prepared"), SkillTrees) ||
		!TestNotNull(TEXT("Swing exists"), Swing.Instance))
	{
		return false;
	}
	Swing.Instance->ConfigureSpread(ARpgHarvestRuneStrike::StaticClass(), ARpgHarvestRuneStrike::StaticClass(), 600.0f, 400.0f);
	TestEqual(TEXT("Leap is learned"),
		SkillTrees->UnlockNode(TAG_HarvestTuningTest_Tree, TAG_HarvestSpreadTest_Leap), ERpgSkillTreeUnlockResult::Unlockable);
	TestWorld.PrimeTimerManager();

	// Two leaps: to the nearest other resource, then on to the nearest one from there that this hit did not strike.
	TestTrue(TEXT("An own hit activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TestEqual(TEXT("The hit takes its section"), Struck->HarvestableNode->GetRemainingSections(), 5);
	TestEqual(TEXT("The leap waits for its delay"), SectionWork(A), 0);
	TestWorld.AdvanceTimers(0.35f);
	TestEqual(TEXT("The first leap does half the hit's work on the nearest other resource"), SectionWork(A), 50);
	TestEqual(TEXT("The second leap lands one step later"), SectionWork(C), 0);
	TestWorld.AdvanceTimers(0.2f);
	TestEqual(TEXT("The second leap goes on from the first resource"), SectionWork(C), 50);
	TestTrue(
		TEXT("The leaps never come back to the struck resource and skip resources beyond their reach"),
		SectionWork(Struck) == 0 && SectionWork(B) == 0 && SectionWork(Far) == 0);
	TestEqual(TEXT("Leaps add no yield of their own"), CountMaterial(Harvester.PlayerState), YieldPerSection);

	// Fork: the first step strikes the two nearest resources and the next goes on from the nearer one, 60 % each.
	TestEqual(TEXT("Fork is learned"),
		SkillTrees->UnlockNode(TAG_HarvestTuningTest_Tree, TAG_HarvestSpreadTest_Fork), ERpgSkillTreeUnlockResult::Unlockable);
	TestTrue(TEXT("A forking hit activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TestWorld.AdvanceTimers(0.6f);
	TestTrue(
		TEXT("A forked leap does 30 units on A and B, then 30 on C"),
		SectionWork(A) == 80 && SectionWork(B) == 30 && SectionWork(C) == 80 && SectionWork(Far) == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestPierceAbilityTest,
	"SurvivalRpg.Harvesting.Ability.PierceStrikesTheResourceBehind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestPierceAbilityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestSpreadTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	// The struck resource on the view ray; Behind 3 m further along the ray, Aside 3.4 m behind and 1.4 m off it, Beside
	// level with it, Back behind the harvester, and Deep 8 m along the ray, beyond the pierce's 4 m.
	const double Eye = Harvester.Pawn->BaseEyeHeight;
	ARpgHarvestAutomationCollidableNodeActor* Struck = SpawnNode(World, FVector(150.0, 0.0, Eye), MakeProfile(World, 6));
	ARpgHarvestAutomationCollidableNodeActor* Behind = SpawnNode(World, FVector(450.0, 0.0, Eye), MakeProfile(World, 4));
	ARpgHarvestAutomationCollidableNodeActor* Aside = SpawnNode(World, FVector(450.0, 140.0, Eye), MakeProfile(World, 4));
	ARpgHarvestAutomationCollidableNodeActor* Beside = SpawnNode(World, FVector(150.0, 300.0, Eye), MakeProfile(World, 4));
	ARpgHarvestAutomationCollidableNodeActor* Back = SpawnNode(World, FVector(-300.0, 0.0, Eye), MakeProfile(World, 4));
	ARpgHarvestAutomationCollidableNodeActor* Deep = SpawnNode(World, FVector(950.0, 0.0, Eye), MakeProfile(World, 4));
	URpgSkillTreeDefinition* Tree = MakeSpreadTree();
	FScopedTreeTool TreeTool(Tree);
	URpgSkillTreeComponent* SkillTrees = PrepareSkillTree(Harvester, Tree);
	URpgEquipmentInstance* Equipment = MakeTreeToolEquipment(Harvester);
	const FGrantedAbility Swing = GrantAbility(Harvester.AbilitySystem, Equipment);
	if (!Struck || !Behind || !Aside || !Beside || !Back || !Deep || !TestNotNull(TEXT("Skill trees are prepared"), SkillTrees) ||
		!TestNotNull(TEXT("Swing exists"), Swing.Instance))
	{
		return false;
	}
	Swing.Instance->ConfigureSpread(ARpgHarvestRuneStrike::StaticClass(), ARpgHarvestRuneStrike::StaticClass(), 600.0f, 400.0f);
	TestEqual(TEXT("Pierce is learned"),
		SkillTrees->UnlockNode(TAG_HarvestTuningTest_Tree, TAG_HarvestSpreadTest_Pierce), ERpgSkillTreeUnlockResult::Unlockable);
	TestWorld.PrimeTimerManager();

	TestTrue(TEXT("An own hit activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TestEqual(TEXT("The hit takes its section"), Struck->HarvestableNode->GetRemainingSections(), 5);
	TestWorld.AdvanceTimers(0.35f);
	TestEqual(TEXT("The pierce does the hit's whole work on the resource behind"), Behind->HarvestableNode->GetRemainingSections(), 3);
	TestTrue(
		TEXT("Only the nearest resource behind is struck"),
		Aside->HarvestableNode->GetRemainingSections() == 4 && SectionWork(Aside) == 0 &&
			Beside->HarvestableNode->GetRemainingSections() == 4 && Back->HarvestableNode->GetRemainingSections() == 4 &&
			Deep->HarvestableNode->GetRemainingSections() == 4);
	TestEqual(TEXT("The pierced section rewards the harvester"), CountMaterial(Harvester.PlayerState), 2 * YieldPerSection);

	// Fork: the two nearest resources behind take 60 % each; nothing beside, behind the harvester or too deep.
	TestEqual(TEXT("Fork is learned"),
		SkillTrees->UnlockNode(TAG_HarvestTuningTest_Tree, TAG_HarvestSpreadTest_Fork), ERpgSkillTreeUnlockResult::Unlockable);
	TestTrue(TEXT("A forking hit activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TestWorld.AdvanceTimers(0.35f);
	TestTrue(
		TEXT("A forked pierce does 60 units on the two resources behind"),
		SectionWork(Behind) == 60 && SectionWork(Aside) == 60 && SectionWork(Beside) == 0 && SectionWork(Back) == 0 &&
			SectionWork(Deep) == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestBurnWanderAbilityTest,
	"SurvivalRpg.Harvesting.Ability.BurnWandersOnWhenItsResourceRunsEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestBurnWanderAbilityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestSpreadTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	// Two groups 20 m apart on the view ray: a two-section resource on the ray, a neighbor 3 m beside it, and in the
	// second group one more 7 m away, beyond the 6 m a burn wanders.
	const double Eye = Harvester.Pawn->BaseEyeHeight;
	ARpgHarvestAutomationCollidableNodeActor* Plain = SpawnNode(World, FVector(150.0, 0.0, Eye), MakeProfile(World, 2));
	ARpgHarvestAutomationCollidableNodeActor* PlainNeighbor = SpawnNode(World, FVector(150.0, 300.0, Eye), MakeProfile(World, 4));
	ARpgHarvestAutomationCollidableNodeActor* Wandering = SpawnNode(World, FVector(2150.0, 0.0, Eye), MakeProfile(World, 2));
	ARpgHarvestAutomationCollidableNodeActor* Neighbor = SpawnNode(World, FVector(2150.0, 300.0, Eye), MakeProfile(World, 4));
	ARpgHarvestAutomationCollidableNodeActor* Distant = SpawnNode(World, FVector(2150.0, -700.0, Eye), MakeProfile(World, 4));
	URpgSkillTreeDefinition* Tree = MakeSpreadTree();
	FScopedTreeTool TreeTool(Tree);
	URpgSkillTreeComponent* SkillTrees = PrepareSkillTree(Harvester, Tree);
	URpgEquipmentInstance* Equipment = MakeTreeToolEquipment(Harvester);
	const FGrantedAbility Swing = GrantAbility(Harvester.AbilitySystem, Equipment);
	if (!Plain || !PlainNeighbor || !Wandering || !Neighbor || !Distant ||
		!TestNotNull(TEXT("Skill trees are prepared"), SkillTrees) || !TestNotNull(TEXT("Swing exists"), Swing.Instance))
	{
		return false;
	}
	Swing.Instance->ConfigureBurn(ARpgHarvestBurn::StaticClass());
	Swing.Instance->ConfigureSpread(nullptr, nullptr, 600.0f, 400.0f);
	FRpgHarvestTargetingParams FarReach;
	FarReach.MaxAimDistance = 6000.0f;
	FarReach.MaxReachFromAvatar = 6000.0f;
	Swing.Instance->ConfigureTargeting(FarReach);
	TestEqual(TEXT("Burn is learned"),
		SkillTrees->UnlockNode(TAG_HarvestTuningTest_Tree, TAG_HarvestSpreadTest_Burn), ERpgSkillTreeUnlockResult::Unlockable);
	AActor* Beneficiary = Harvester.PlayerState;
	TestWorld.PrimeTimerManager();

	// Without Wander, the burn goes out with its resource although it has time left.
	TestTrue(TEXT("A hit lights a burn"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TestNotNull(TEXT("The struck resource burns"), ARpgHarvestBurn::FindBurn(Plain->HarvestableNode, INDEX_NONE, Beneficiary));
	TestWorld.AdvanceTimers(BurnStep);
	TestWorld.AdvanceTimers(BurnStep);
	TestFalse(TEXT("Two burn ticks empty the resource"), Plain->HarvestableNode->IsHarvestable());
	TestWorld.AdvanceTimers(BurnStep);
	TestTrue(
		TEXT("The burn goes out with its resource"),
		!ARpgHarvestBurn::FindBurn(PlainNeighbor->HarvestableNode, INDEX_NONE, Beneficiary) &&
			PlainNeighbor->HarvestableNode->GetRemainingSections() == 4 && SectionWork(PlainNeighbor) == 0);
	Plain->Destroy();

	// With Wander, the burn moves on to the nearest other resource and burns there for its two remaining ticks only.
	TestEqual(TEXT("Wander is learned"),
		SkillTrees->UnlockNode(TAG_HarvestTuningTest_Tree, TAG_HarvestSpreadTest_Wander), ERpgSkillTreeUnlockResult::Unlockable);
	TestTrue(TEXT("A wandering hit lights a burn"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TestWorld.AdvanceTimers(BurnStep);
	TestWorld.AdvanceTimers(BurnStep);
	TestFalse(TEXT("Two burn ticks empty the struck resource"), Wandering->HarvestableNode->IsHarvestable());
	ARpgHarvestBurn* Wandered = ARpgHarvestBurn::FindBurn(Neighbor->HarvestableNode, INDEX_NONE, Beneficiary);
	if (!TestNotNull(TEXT("The burn moved on to the neighbor"), Wandered))
	{
		return false;
	}
	TestEqual(TEXT("The wandered burn keeps only the remaining ticks"), Wandered->GetRemainingTicks(), 2);
	TestTrue(TEXT("The wandered burn works for the same player"), Wandered->GetBeneficiary() == Beneficiary);
	TestWorld.AdvanceTimers(BurnStep);
	TestWorld.AdvanceTimers(BurnStep);
	TestEqual(TEXT("Its two ticks complete one section of the neighbor"), Neighbor->HarvestableNode->GetRemainingSections(), 3);
	TestEqual(TEXT("No residual work is left"), SectionWork(Neighbor), 0);
	TestNull(TEXT("The burn is out after its time"), ARpgHarvestBurn::FindBurn(Neighbor->HarvestableNode, INDEX_NONE, Beneficiary));
	TestEqual(TEXT("A resource beyond the wander reach is untouched"), Distant->HarvestableNode->GetRemainingSections(), 4);
	TestEqual(
		TEXT("Every completed section rewarded the igniting player once"),
		CountMaterial(Harvester.PlayerState),
		5 * YieldPerSection);
	return true;
}

namespace RpgHarvestWorkerTests
{
	using namespace RpgHarvestAbilityTests;
	using namespace RpgHarvestTuningTests;

	/**
	 * One-point nodes on Mining that tune every ability of the tool like the Helper runes and talents: Worker (strikes of
	 * half a hit's work, raised every second own hit) and Pack (one more worker).
	 */
	URpgSkillTreeDefinition* MakeWorkerTree()
	{
		using namespace RpgHarvestingMagicGameplayTags;

		URpgSkillTreeDefinition* Tree = NewObject<URpgSkillTreeDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		Tree->TreeTag = TAG_HarvestTuningTest_Tree;
		Tree->MasterySkillTag = RpgTradeSkillGameplayTags::Skill_Gathering_Mining;
		Tree->MaxPoints = 10;
		FRpgSkillTreeNode& Worker = Tree->Nodes.AddDefaulted_GetRef();
		Worker.NodeTag = TAG_HarvestWorkerTest_Worker;
		Worker.Column = 0;
		Worker.AbilityTunings = {
			MakeTuning(Ability_Tuning_Harvest_WorkerWork, ERpgSkillTreeTuningOperation::Set, 0.5f),
			MakeTuning(Ability_Tuning_Harvest_WorkerHits, ERpgSkillTreeTuningOperation::Set, 2.0f)};
		FRpgSkillTreeNode& Pack = Tree->Nodes.AddDefaulted_GetRef();
		Pack.NodeTag = TAG_HarvestWorkerTest_Pack;
		Pack.Column = 1;
		Pack.AbilityTunings = {MakeTuning(Ability_Tuning_Harvest_WorkerCount, ERpgSkillTreeTuningOperation::Add, 1.0f)};
		return Tree;
	}

	/** Swarms in World that have not finished. */
	int32 CountActiveSwarms(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<ARpgHarvestSwarm> It(World); It; ++It)
		{
			Count += IsValid(*It) && !It->IsActorBeingDestroyed() && !It->IsFinished() ? 1 : 0;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestWorkerAbilityTest,
	"SurvivalRpg.Harvesting.Ability.WorkersRiseEveryFewHitsForTheirPlayer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestWorkerAbilityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestWorkerTests;
	using RpgHarvestSwarmTests::Advance;
	using RpgHarvestSwarmTests::FindActiveSwarm;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	// A large resource on the view ray, so the workers keep working it, and one 15 m away, beyond their 6 m.
	const double Eye = Harvester.Pawn->BaseEyeHeight;
	ARpgHarvestAutomationCollidableNodeActor* Struck = SpawnNode(World, FVector(150.0, 0.0, Eye), MakeProfile(World, 20));
	ARpgHarvestAutomationCollidableNodeActor* Far = SpawnNode(World, FVector(150.0, -1500.0, Eye), MakeProfile(World, 4));
	URpgSkillTreeDefinition* Tree = MakeWorkerTree();
	FScopedTreeTool TreeTool(Tree);
	URpgSkillTreeComponent* SkillTrees = PrepareSkillTree(Harvester, Tree);
	URpgEquipmentInstance* Equipment = MakeTreeToolEquipment(Harvester);
	const FGrantedAbility Swing = GrantAbility(Harvester.AbilitySystem, Equipment);
	if (!Struck || !Far || !TestNotNull(TEXT("Skill trees are prepared"), SkillTrees) || !TestNotNull(TEXT("Swing exists"), Swing.Instance))
	{
		return false;
	}
	FRpgHarvestSwarmParams Workers;
	Workers.CreatureCount = 1;
	Workers.FlightSpeed = 1000.0f;
	Workers.EmergeSeconds = 0.2f;
	Workers.LaunchIntervalSeconds = 0.1f;
	Workers.StrikeIntervalSeconds = 0.5f;
	Workers.MaxReassignments = 4;
	Workers.MaxLifetimeSeconds = 3.0f;
	Workers.bRequireLineOfSight = false;
	Swing.Instance->ConfigureWorkers(ARpgHarvestSwarm::StaticClass(), Workers, 600.0f);
	URpgHarvestAutomationHarvestedListener* Results = NewObject<URpgHarvestAutomationHarvestedListener>();
	Struck->HarvestableNode->OnHarvested.AddDynamic(Results, &URpgHarvestAutomationHarvestedListener::HandleHarvested);
	TestWorld.PrimeTimerManager();

	TestTrue(TEXT("A hit without worker work activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TestNull(TEXT("Without worker work no worker rises"), FindActiveSwarm(World));
	TestEqual(TEXT("Worker is learned"),
		SkillTrees->UnlockNode(TAG_HarvestTuningTest_Tree, TAG_HarvestWorkerTest_Worker), ERpgSkillTreeUnlockResult::Unlockable);
	TestTrue(TEXT("The first counted hit activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TestNull(TEXT("One counted hit raises no worker"), FindActiveSwarm(World));
	TestTrue(TEXT("The second counted hit activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	ARpgHarvestSwarm* Crew = FindActiveSwarm(World);
	if (!TestNotNull(TEXT("The second counted hit raises a worker"), Crew))
	{
		return false;
	}
	TestTrue(TEXT("The worker works for the hitting player"), Crew->GetBeneficiary() == Harvester.PlayerState);
	TestEqual(TEXT("One worker rises"), Crew->GetCreatures().Num(), 1);

	// A hit while the worker works raises no second crew and does not count towards the next one.
	TestTrue(TEXT("A hit while the worker works activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TestEqual(TEXT("The player keeps one crew"), CountActiveSwarms(World), 1);
	const int32 HitSections = 4;
	Advance(TestWorld, 1.5);
	const int32 MidSections = static_cast<int32>(Struck->HarvestableNode->GetHarvestState().HarvestedSections);
	TestTrue(TEXT("The worker works the struck resource"), MidSections > HitSections || Struck->HarvestableNode->GetHarvestState().SectionWork > 0);
	TestEqual(TEXT("Its rewards wait for the crew to finish"), CountMaterial(Harvester.PlayerState), HitSections * YieldPerSection);
	Advance(TestWorld, 2.0);
	TestNull(TEXT("The worker is gone after its lifetime"), FindActiveSwarm(World));
	const int32 Harvested = static_cast<int32>(Struck->HarvestableNode->GetHarvestState().HarvestedSections);
	TestTrue(TEXT("The worker completed sections of its own"), Harvested >= HitSections + 2);
	TestEqual(TEXT("Every completed section rewarded the player once"), CountMaterial(Harvester.PlayerState), Harvested * YieldPerSection);
	TestEqual(TEXT("A resource beyond the workers' reach is untouched"), Far->HarvestableNode->GetRemainingSections(), 4);
	TestTrue(
		TEXT("Worker strikes are follow-up work, never the player's own hits"),
		Results->Results.ContainsByPredicate([](const FRpgHarvestResult& Result)
		{
			return Result.IsSuccess() && !Result.Provenance.IsDirect() && Result.Provenance.RootActionId > 0;
		}));

	// The hit during the work did not count: one more hit raises nothing, the second one raises the next crew. Pack
	// raises two workers at once.
	TestEqual(TEXT("Pack is learned"),
		SkillTrees->UnlockNode(TAG_HarvestTuningTest_Tree, TAG_HarvestWorkerTest_Pack), ERpgSkillTreeUnlockResult::Unlockable);
	TestTrue(TEXT("A first hit after the crew activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TestNull(TEXT("The hit during the work did not count"), FindActiveSwarm(World));
	TestTrue(TEXT("A second hit after the crew activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	ARpgHarvestSwarm* Pack = FindActiveSwarm(World);
	TestTrue(TEXT("Pack raises two workers"), Pack && Pack->GetCreatures().Num() == 2);
	Advance(TestWorld, 3.5);
	TestEqual(
		TEXT("The pack's sections reward the player once as well"),
		CountMaterial(Harvester.PlayerState),
		static_cast<int32>(Struck->HarvestableNode->GetHarvestState().HarvestedSections) * YieldPerSection);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestAllRunesAbilityTest,
	"SurvivalRpg.Harvesting.Ability.AllRunesWorkTogether",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestAllRunesAbilityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestAbilityTests;
	using namespace RpgHarvestTuningTests;
	using namespace RpgHarvestingMagicGameplayTags;
	using RpgHarvestSwarmTests::Advance;
	using RpgHarvestSwarmTests::FindActiveSwarm;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}

	// One node tunes the tool like all six runes at once, with the talents that widen them: Brand with Wandering
	// Effect, Echo on every hit, Kraftuebertrag, Erntesprung and Durchschlag forked, and workers on every hit.
	URpgSkillTreeDefinition* Tree = NewObject<URpgSkillTreeDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
	Tree->TreeTag = TAG_HarvestTuningTest_Tree;
	Tree->MasterySkillTag = RpgTradeSkillGameplayTags::Skill_Gathering_Mining;
	Tree->MaxPoints = 10;
	FRpgSkillTreeNode& AllRunes = Tree->Nodes.AddDefaulted_GetRef();
	AllRunes.NodeTag = TAG_HarvestCatalogTest_AllRunes;
	AllRunes.AbilityTunings = {
		MakeTuning(Ability_Tuning_Harvest_BurnWork, ERpgSkillTreeTuningOperation::Set, 0.3f),
		MakeTuning(Ability_Tuning_Harvest_BurnDuration, ERpgSkillTreeTuningOperation::Set, 3.0f),
		MakeTuning(Ability_Tuning_Harvest_BurnWander, ERpgSkillTreeTuningOperation::Set, 1.0f),
		MakeTuning(Ability_Tuning_Harvest_EchoWork, ERpgSkillTreeTuningOperation::Set, 0.5f),
		MakeTuning(Ability_Tuning_Harvest_EchoHits, ERpgSkillTreeTuningOperation::Set, 1.0f),
		MakeTuning(Ability_Tuning_Harvest_TransferWork, ERpgSkillTreeTuningOperation::Set, 1.0f),
		MakeTuning(Ability_Tuning_Harvest_RuneFlowSplit, ERpgSkillTreeTuningOperation::Set, 0.6f),
		MakeTuning(Ability_Tuning_Harvest_LeapWork, ERpgSkillTreeTuningOperation::Set, 0.5f),
		MakeTuning(Ability_Tuning_Harvest_PierceWork, ERpgSkillTreeTuningOperation::Set, 0.5f),
		MakeTuning(Ability_Tuning_Harvest_SpreadSplit, ERpgSkillTreeTuningOperation::Set, 0.6f),
		MakeTuning(Ability_Tuning_Harvest_WorkerWork, ERpgSkillTreeTuningOperation::Set, 0.5f),
		MakeTuning(Ability_Tuning_Harvest_WorkerHits, ERpgSkillTreeTuningOperation::Set, 1.0f)};

	// The struck resource on the view ray; two behind it, three beside it within a leap, and one far away.
	const double Eye = Harvester.Pawn->BaseEyeHeight;
	TArray<ARpgHarvestAutomationCollidableNodeActor*> Nodes = {
		SpawnNode(World, FVector(150.0, 0.0, Eye), MakeProfile(World, 5)),
		SpawnNode(World, FVector(450.0, 0.0, Eye), MakeProfile(World, 4)),
		SpawnNode(World, FVector(450.0, 140.0, Eye), MakeProfile(World, 4)),
		SpawnNode(World, FVector(150.0, 300.0, Eye), MakeProfile(World, 4)),
		SpawnNode(World, FVector(150.0, -450.0, Eye), MakeProfile(World, 4)),
		SpawnNode(World, FVector(150.0, 750.0, Eye), MakeProfile(World, 4))};
	ARpgHarvestAutomationCollidableNodeActor* Far = SpawnNode(World, FVector(150.0, -1500.0, Eye), MakeProfile(World, 4));
	FScopedTreeTool TreeTool(Tree);
	URpgSkillTreeComponent* SkillTrees = PrepareSkillTree(Harvester, Tree);
	URpgEquipmentInstance* Equipment = MakeTreeToolEquipment(Harvester);
	const FGrantedAbility Swing = GrantAbility(Harvester.AbilitySystem, Equipment);
	if (Nodes.Contains(nullptr) || !Far || !TestNotNull(TEXT("Skill trees are prepared"), SkillTrees) ||
		!TestNotNull(TEXT("Swing exists"), Swing.Instance))
	{
		return false;
	}
	FRpgHarvestSwarmParams Workers;
	Workers.CreatureCount = 2;
	Workers.FlightSpeed = 1000.0f;
	Workers.EmergeSeconds = 0.2f;
	Workers.StrikeIntervalSeconds = 0.5f;
	Workers.MaxReassignments = 4;
	Workers.MaxLifetimeSeconds = 3.0f;
	Workers.bRequireLineOfSight = false;
	Swing.Instance->ConfigureBurn(ARpgHarvestBurn::StaticClass());
	Swing.Instance->ConfigureRuneFlow(ARpgHarvestRuneStrike::StaticClass(), ARpgHarvestRuneStrike::StaticClass(), 800.0f);
	Swing.Instance->ConfigureSpread(ARpgHarvestRuneStrike::StaticClass(), ARpgHarvestRuneStrike::StaticClass(), 600.0f, 400.0f);
	Swing.Instance->ConfigureWorkers(ARpgHarvestSwarm::StaticClass(), Workers, 600.0f);
	Swing.Instance->ConfigureSections(3);
	TestEqual(TEXT("All runes are learned"),
		SkillTrees->UnlockNode(TAG_HarvestTuningTest_Tree, TAG_HarvestCatalogTest_AllRunes), ERpgSkillTreeUnlockResult::Unlockable);

	// Every result of every resource, and every follow-up actor spawned.
	URpgHarvestAutomationHarvestedListener* Results = NewObject<URpgHarvestAutomationHarvestedListener>();
	for (ARpgHarvestAutomationCollidableNodeActor* Node : Nodes)
	{
		Node->HarvestableNode->OnHarvested.AddDynamic(Results, &URpgHarvestAutomationHarvestedListener::HandleHarvested);
	}
	int32 Strikes = 0;
	int32 Burns = 0;
	int32 Crews = 0;
	const FDelegateHandle SpawnHandle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda(
		[&Strikes, &Burns, &Crews](AActor* Actor)
		{
			Strikes += Actor && Actor->IsA<ARpgHarvestRuneStrike>() ? 1 : 0;
			Burns += Actor && Actor->IsA<ARpgHarvestBurn>() ? 1 : 0;
			Crews += Actor && Actor->IsA<ARpgHarvestSwarm>() ? 1 : 0;
		}));
	ON_SCOPE_EXIT
	{
		World->RemoveOnActorSpawnedHandler(SpawnHandle);
	};
	TestWorld.PrimeTimerManager();

	// Two own hits: the first works the struck resource, the second empties it with overkill.
	TestTrue(TEXT("The first own hit activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	Advance(TestWorld, 0.1);
	TestTrue(TEXT("The second own hit activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	Advance(TestWorld, 12.0);

	// Everything that followed the two hits has ended.
	TestNull(TEXT("No worker is left"), FindActiveSwarm(World));
	int32 LiveFollowUps = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		LiveFollowUps += (It->IsA<ARpgHarvestRuneStrike>() || It->IsA<ARpgHarvestBurn>()) && !It->IsActorBeingDestroyed() ? 1 : 0;
	}
	TestEqual(TEXT("No strike or burn is left"), LiveFollowUps, 0);
	TestEqual(TEXT("One crew of workers rose for both hits"), Crews, 1);
	// Per hit at most: an echo split in two, a transfer split in two, two forked leap steps and a forked pierce.
	TestTrue(*FString::Printf(TEXT("Follow-up strikes are bounded (%d)"), Strikes), Strikes > 0 && Strikes <= 2 * 10);
	TestTrue(*FString::Printf(TEXT("Burns are bounded by their ticks (%d)"), Burns), Burns > 0 && Burns <= 2 * 4);

	// Only the two hits are own hits; everything else is follow-up work of one of them.
	TSet<int32> Roots;
	int32 Direct = 0;
	for (const FRpgHarvestResult& Result : Results->Results)
	{
		if (Result.IsSuccess() && Result.Provenance.IsDirect())
		{
			++Direct;
			Roots.Add(Result.Provenance.RootActionId);
		}
	}
	TestEqual(TEXT("Exactly the two hits are own hits"), Direct, 2);
	TestTrue(
		TEXT("Every follow-up belongs to one of the two hits"),
		!Results->Results.ContainsByPredicate([&Roots](const FRpgHarvestResult& Result)
		{
			return Result.IsSuccess() && !Result.Provenance.IsDirect() && !Roots.Contains(Result.Provenance.RootActionId);
		}));

	// No section is rewarded twice and none is lost: the yield is exactly the completed sections.
	int32 Completed = 0;
	for (const ARpgHarvestAutomationCollidableNodeActor* Node : Nodes)
	{
		Completed += static_cast<int32>(Node->HarvestableNode->GetHarvestState().HarvestedSections);
	}
	TestTrue(TEXT("The follow-ups completed sections beyond the hits"), Completed > 5);
	TestEqual(TEXT("Every completed section rewarded the player exactly once"), CountMaterial(Harvester.PlayerState), Completed * YieldPerSection);
	TestEqual(TEXT("A resource out of every reach is untouched"), Far->HarvestableNode->GetRemainingSections(), 4);
	return true;
}

namespace RpgHarvestSwingTests
{
	using namespace RpgHarvestSpreadTests;

	/** Records every executed cue of the swing tests with its location. */
	struct FSwingCueRecorder
	{
		struct FCue
		{
			FGameplayTag Tag;
			FVector Location = FVector::ZeroVector;
		};

		FSwingCueRecorder()
		{
			if (UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager())
			{
				Handle = CueManager->OnGameplayCueRouted().AddLambda(
					[this](AActor*, const FGameplayTag Tag, const EGameplayCueEvent::Type Event,
						const FGameplayCueParameters& CueParameters, EGameplayCueExecutionOptions)
					{
						if (Event == EGameplayCueEvent::Executed &&
							(Tag == TAG_HarvestSwingTest_Hit.GetTag() || Tag == TAG_HarvestSwingTest_NoYield.GetTag()))
						{
							Cues.Add({Tag, FVector(CueParameters.Location)});
						}
					});
			}
		}

		~FSwingCueRecorder()
		{
			if (UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager())
			{
				CueManager->OnGameplayCueRouted().Remove(Handle);
			}
		}

		int32 Count(const FGameplayTag Tag) const
		{
			return Cues.FilterByPredicate([Tag](const FCue& Cue) { return Cue.Tag == Tag; }).Num();
		}

		TArray<FCue> Cues;
		FDelegateHandle Handle;
	};

	/** A 12 cm sphere swept from the tool's grip over its head to the tip on the weapon channel, as the axe and pickaxe swings trace. */
	FRpgSwingTraceParams MakeSwingTrace()
	{
		FRpgSwingTraceParams Trace;
		Trace.TraceMode = ERpgWeaponAttackTraceMode::SphereSweep;
		Trace.TraceRadius = 12.0f;
		Trace.bTraceBetweenSockets = true;
		Trace.TraceInterpolationDistance = 12.0f;
		Trace.TracePointSockets = {TEXT("TracePoint_Grip"), TEXT("TracePoint_Base"), TEXT("TracePoint_Tip")};
		Trace.TraceChannel = Rpg_TraceChannel_Weapon;
		return Trace;
	}

	/**
	 * Grants a tool swing from Source with the swing cues, one section per contact and Reach from the harvester, bound to
	 * the primary input when bBindPrimaryInput is set (the input needs an equipment instance as Source).
	 */
	FGrantedAbility GrantSwing(
		URpgAbilitySystemComponent* AbilitySystem,
		UObject* Source,
		const float Reach = 250.0f,
		const bool bBindPrimaryInput = false)
	{
		FGrantedAbility Granted = GrantAbility(AbilitySystem, Source, bBindPrimaryInput);
		if (Granted.Instance)
		{
			FRpgHarvestTargetingParams Single;
			Single.MaxReachFromAvatar = Reach;
			Granted.Instance->ConfigureTargeting(Single);
			Granted.Instance->ConfigureSections(1);
			Granted.Instance->ConfigureToolSwing(MakeSwingTrace(), TAG_HarvestSwingTest_NoYield);
			Granted.Instance->ConfigureSuccessCues(TAG_HarvestSwingTest_Hit, {});
		}
		return Granted;
	}

	/** A contact of the tool's sweep with Node at ImpactPoint, the tool coming from TraceStart. */
	FHitResult MakeContact(ARpgHarvestAutomationCollidableNodeActor* Node, const FVector& ImpactPoint, const FVector& TraceStart)
	{
		FHitResult Hit(Node, Node ? Node->Collision.Get() : nullptr, ImpactPoint, (TraceStart - ImpactPoint).GetSafeNormal());
		// A sweep against a single-body component reports no instance; the constructor leaves Item at 0.
		Hit.Item = INDEX_NONE;
		Hit.TraceStart = TraceStart;
		Hit.TraceEnd = ImpactPoint + (ImpactPoint - TraceStart);
		Hit.bBlockingHit = false;
		return Hit;
	}

	/** Center of the face of a 40 cm node that looks at the harvester at the origin. */
	FVector FrontFace(const ARpgHarvestAutomationCollidableNodeActor* Node)
	{
		return Node ? Node->GetActorLocation() - FVector(40.0, 0.0, 0.0) : FVector::ZeroVector;
	}

	/** Tool pose with its trace points from x = BaseX to x = TipX, pointing away from the harvester, at Y and Height. */
	RpgWeaponTraceInterpolation::FBladeSample MakeToolPose(const double BaseX, const double TipX, const double Y, const double Height)
	{
		RpgWeaponTraceInterpolation::FBladeSample Sample;
		Sample.TracePointTransforms = {FTransform(FVector(BaseX, Y, Height)), FTransform(FVector(TipX, Y, Height))};
		return Sample;
	}

	/**
	 * Tool pose of a flat swing around the harvester at the origin: at Yaw and Height, one trace point at each of
	 * Distances out from the harvester, each turned with the tool, so sub-steps follow the swing's arc.
	 */
	RpgWeaponTraceInterpolation::FBladeSample MakeRadialToolPose(const double Yaw, const double Height, const TArray<double>& Distances)
	{
		RpgWeaponTraceInterpolation::FBladeSample Sample;
		const FRotator Rotation(0.0, Yaw, 0.0);
		for (const double Distance : Distances)
		{
			Sample.TracePointTransforms.Add(FTransform(Rotation, Rotation.Vector() * Distance + FVector(0.0, 0.0, Height)));
		}
		return Sample;
	}

	/** Sweeps the running swing's trace from Previous to Current in World and feeds every query to the ability. */
	void TraceSwing(
		UWorld& World,
		URpgHarvestAutomationTestAbility& Ability,
		const RpgWeaponTraceInterpolation::FBladeSample& Previous,
		const RpgWeaponTraceInterpolation::FBladeSample& Current)
	{
		FRpgSwingTraceSink Sink;
		Sink.OnHits.BindLambda([&Ability](const TArray<FHitResult>& HitResults, const ERpgSwingTraceRole Role)
		{
			Ability.HandleToolSwingHitsForTests(HitResults, Role);
		});
		FRpgSwingTraceWindow Window;
		Window.TraceSampleMotionForTests(World, Ability.GetPlayedSwingTrace(), Previous, Current, MoveTemp(Sink));
	}

	/** One-point Mining node like the Wide Swing spell: +150 cm swing reach and one more target per swing. */
	URpgSkillTreeDefinition* MakeWideSwingTree()
	{
		using namespace RpgHarvestingMagicGameplayTags;

		URpgSkillTreeDefinition* Tree = NewObject<URpgSkillTreeDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		Tree->TreeTag = TAG_HarvestTuningTest_Tree;
		Tree->MasterySkillTag = RpgTradeSkillGameplayTags::Skill_Gathering_Mining;
		Tree->MaxPoints = 10;
		FRpgSkillTreeNode& Node = Tree->Nodes.AddDefaulted_GetRef();
		Node.NodeTag = TAG_HarvestSwingTest_Wide;
		Node.AbilityTunings = {
			MakeTuning(Ability_Tuning_Harvest_SwingReach, ERpgSkillTreeTuningOperation::Add, 150.0f),
			MakeTuning(Ability_Tuning_Harvest_SwingTargets, ERpgSkillTreeTuningOperation::Add, 1.0f)};
		return Tree;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestSwingFirstContactTest,
	"SurvivalRpg.Harvesting.Swing.FirstContactCommitsOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestSwingFirstContactTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestSwingTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	const double Eye = Harvester.Pawn->BaseEyeHeight;
	ARpgHarvestAutomationCollidableNodeActor* First = SpawnNode(World, FVector(110.0, 0.0, Eye), MakeProfile(World, 4));
	ARpgHarvestAutomationCollidableNodeActor* Second = SpawnNode(World, FVector(110.0, 100.0, Eye), MakeProfile(World, 4));
	const FGrantedAbility Swing = GrantSwing(Harvester.AbilitySystem, nullptr);
	if (!First || !Second || !TestNotNull(TEXT("Swing exists"), Swing.Instance))
	{
		return false;
	}
	URpgHarvestAutomationHarvestedListener* Harvested = NewObject<URpgHarvestAutomationHarvestedListener>();
	First->HarvestableNode->OnHarvested.AddDynamic(Harvested, &URpgHarvestAutomationHarvestedListener::HandleHarvested);
	FSwingCueRecorder Cues;
	TestWorld.PrimeTimerManager();

	TestTrue(TEXT("The swing activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TestTrue(TEXT("The swing waits for its contacts"), Swing.Instance->IsActive());
	TestEqual(TEXT("The swing starts its trace once"), Swing.Instance->GetPlayedSwingCount(), 1);
	TestEqual(TEXT("A plain swing traces the authored tool"), Swing.Instance->GetPlayedSwingTrace().TipExtension, 0.0f);

	// One query touches the ground first, then both resources; a later query touches the first resource again.
	const FVector FirstImpact = FrontFace(First);
	const FHitResult Ground(nullptr, nullptr, FVector(60.0, 0.0, 0.0), FVector::UpVector);
	Swing.Instance->HandleToolSwingHitsForTests(
		{Ground,
			MakeContact(First, FirstImpact, FirstImpact - FVector(0.0, 40.0, 0.0)),
			MakeContact(Second, FrontFace(Second), FVector(70.0, 60.0, Eye))},
		ERpgSwingTraceRole::Authority);
	Swing.Instance->HandleToolSwingHitsForTests(
		{MakeContact(First, FirstImpact + FVector(0.0, 10.0, 0.0), FirstImpact)},
		ERpgSwingTraceRole::Authority);

	TestEqual(TEXT("The first contact takes one section"), First->HarvestableNode->GetRemainingSections(), 3);
	TestEqual(
		TEXT("The swing's one target is used up, so the second resource stays whole"),
		Second->HarvestableNode->GetRemainingSections(),
		4);
	TestEqual(TEXT("The section is rewarded once"), CountMaterial(Harvester.PlayerState), YieldPerSection);
	TestEqual(TEXT("Ground and repeated touches never count"), Swing.Instance->GetToolSwingContactCountForTests(), 1);
	if (TestEqual(TEXT("The contact plays the hit cue once"), Cues.Count(TAG_HarvestSwingTest_Hit), 1))
	{
		TestTrue(TEXT("The hit cue plays at the impact"), Cues.Cues[0].Location.Equals(FirstImpact, 0.1));
	}
	TestEqual(TEXT("A harvesting contact plays no no-yield cue"), Cues.Count(TAG_HarvestSwingTest_NoYield), 0);
	TestTrue(TEXT("The contact is the harvester's own hit"), Harvested->Results.Num() == 1 && Harvested->Results[0].IsAcceptedDirectHit());

	// The swinging client's own trace only predicts; the server's contacts are the gameplay result.
	Harvester.AbilitySystem->CancelAbilityHandle(Swing.Handle);
	TestTrue(TEXT("A swing traced for feedback activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	Swing.Instance->HandleToolSwingHitsForTests(
		{MakeContact(Second, FrontFace(Second), FVector(70.0, 60.0, Eye))},
		ERpgSwingTraceRole::OwnerFeedback);
	TestEqual(TEXT("The feedback trace counts its contact"), Swing.Instance->GetToolSwingContactCountForTests(), 1);
	TestEqual(TEXT("Feedback contacts never commit"), Second->HarvestableNode->GetRemainingSections(), 4);

	Harvester.AbilitySystem->CancelAbilityHandle(Swing.Handle);
	TestTrue(TEXT("The next swing activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	Swing.Instance->HandleToolSwingHitsForTests(
		{MakeContact(First, FirstImpact, FVector(70.0, -40.0, Eye))},
		ERpgSwingTraceRole::Authority);
	TestEqual(TEXT("Every swing strikes the resource anew"), First->HarvestableNode->GetRemainingSections(), 2);
	TestTrue(
		TEXT("Every swing is its own root action"),
		Harvested->Results.Num() == 2 && Harvested->Results[0].Provenance.RootActionId > 0 &&
			Harvested->Results[0].Provenance.RootActionId != Harvested->Results[1].Provenance.RootActionId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestSwingRejectedContactTest,
	"SurvivalRpg.Harvesting.Swing.RejectedContactConsumesSwing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestSwingRejectedContactTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestSwingTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	const double Eye = Harvester.Pawn->BaseEyeHeight;
	// The swing carries no tool, so the skinning-only resource rejects it; the plain resources accept any swing.
	URpgHarvestProfile* SkinningOnly = MakeProfile(World, 4);
	SkinningOnly->RequiredToolTag = RpgHarvestingMagicGameplayTags::Tool_Harvesting_Skinning;
	ARpgHarvestAutomationCollidableNodeActor* WrongTool = SpawnNode(World, FVector(110.0, 0.0, Eye), SkinningOnly);
	ARpgHarvestAutomationCollidableNodeActor* Single = SpawnNode(World, FVector(110.0, -100.0, Eye), MakeProfile(World, 1));
	ARpgHarvestAutomationCollidableNodeActor* Behind = SpawnNode(World, FVector(110.0, 100.0, Eye), MakeProfile(World, 4));
	const FGrantedAbility Swing = GrantSwing(Harvester.AbilitySystem, nullptr);
	if (!WrongTool || !Single || !Behind || !TestNotNull(TEXT("Swing exists"), Swing.Instance))
	{
		return false;
	}
	FSwingCueRecorder Cues;
	TestWorld.PrimeTimerManager();

	TestTrue(TEXT("The swing activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	Swing.Instance->HandleToolSwingHitsForTests(
		{MakeContact(WrongTool, FrontFace(WrongTool), FVector(70.0, -40.0, Eye)),
			MakeContact(Behind, FrontFace(Behind), FVector(70.0, 60.0, Eye))},
		ERpgSwingTraceRole::Authority);
	TestEqual(TEXT("The wrong tool takes nothing"), WrongTool->HarvestableNode->GetRemainingSections(), 4);
	TestEqual(TEXT("The rejected contact used up the swing"), Behind->HarvestableNode->GetRemainingSections(), 4);
	TestEqual(TEXT("Nothing is rewarded"), CountMaterial(Harvester.PlayerState), 0);
	if (TestEqual(TEXT("The rejected contact plays the no-yield cue once"), Cues.Count(TAG_HarvestSwingTest_NoYield), 1))
	{
		TestTrue(TEXT("The no-yield cue plays at the impact"), Cues.Cues[0].Location.Equals(FrontFace(WrongTool), 0.1));
	}
	TestEqual(TEXT("A rejected contact plays no hit cue"), Cues.Count(TAG_HarvestSwingTest_Hit), 0);

	// An empty resource consumes the swing the same way.
	Harvester.AbilitySystem->CancelAbilityHandle(Swing.Handle);
	TestTrue(TEXT("A swing at the single-section resource activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	Swing.Instance->HandleToolSwingHitsForTests(
		{MakeContact(Single, FrontFace(Single), FVector(70.0, -140.0, Eye))},
		ERpgSwingTraceRole::Authority);
	TestFalse(TEXT("The swing empties the single-section resource"), Single->HarvestableNode->IsHarvestable());
	Harvester.AbilitySystem->CancelAbilityHandle(Swing.Handle);
	TestTrue(TEXT("A swing at the empty resource activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	Swing.Instance->HandleToolSwingHitsForTests(
		{MakeContact(Single, FrontFace(Single), FVector(70.0, -140.0, Eye)),
			MakeContact(Behind, FrontFace(Behind), FVector(70.0, 60.0, Eye))},
		ERpgSwingTraceRole::Authority);
	TestEqual(TEXT("The empty resource used up the swing"), Behind->HarvestableNode->GetRemainingSections(), 4);
	TestEqual(TEXT("The empty resource plays the no-yield cue"), Cues.Count(TAG_HarvestSwingTest_NoYield), 2);
	TestEqual(TEXT("Only the emptying swing was rewarded"), CountMaterial(Harvester.PlayerState), YieldPerSection);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestSwingReachGuardTest,
	"SurvivalRpg.Harvesting.Swing.ReachGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestSwingReachGuardTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestSwingTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	const double Eye = Harvester.Pawn->BaseEyeHeight;
	ARpgHarvestAutomationCollidableNodeActor* Far = SpawnNode(World, FVector(300.0, 0.0, Eye), MakeProfile(World, 4));
	ARpgHarvestAutomationCollidableNodeActor* Near = SpawnNode(World, FVector(110.0, 0.0, Eye), MakeProfile(World, 4));
	const FGrantedAbility Swing = GrantSwing(Harvester.AbilitySystem, nullptr, 150.0f);
	if (!Far || !Near || !TestNotNull(TEXT("Swing exists"), Swing.Instance))
	{
		return false;
	}
	FSwingCueRecorder Cues;
	TestWorld.PrimeTimerManager();

	TestTrue(TEXT("The swing activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	// The far contact lies about 270 cm from the harvester, beyond its 150 cm reach, as a stale or forged pose would.
	Swing.Instance->HandleToolSwingHitsForTests(
		{MakeContact(Far, FrontFace(Far), FVector(250.0, -40.0, Eye)),
			MakeContact(Near, FrontFace(Near), FVector(70.0, -40.0, Eye))},
		ERpgSwingTraceRole::Authority);
	TestEqual(TEXT("A contact beyond the reach takes nothing"), Far->HarvestableNode->GetRemainingSections(), 4);
	TestEqual(TEXT("A contact beyond the reach does not use up the swing"), Near->HarvestableNode->GetRemainingSections(), 3);
	TestEqual(TEXT("Only the reachable contact counts"), Swing.Instance->GetToolSwingContactCountForTests(), 1);
	TestEqual(TEXT("A contact beyond the reach plays no cue"), Cues.Count(TAG_HarvestSwingTest_NoYield), 0);
	TestEqual(TEXT("The reachable contact plays its hit cue"), Cues.Count(TAG_HarvestSwingTest_Hit), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestSwingCancelTest,
	"SurvivalRpg.Harvesting.Swing.CancelYieldsNothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestSwingCancelTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestSwingTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	const double Eye = Harvester.Pawn->BaseEyeHeight;
	ARpgHarvestAutomationCollidableNodeActor* Node = SpawnNode(World, FVector(110.0, 0.0, Eye), MakeProfile(World, 4));
	const FGrantedAbility Swing = GrantSwing(Harvester.AbilitySystem, nullptr);
	if (!Node || !TestNotNull(TEXT("Swing exists"), Swing.Instance))
	{
		return false;
	}
	FSwingCueRecorder Cues;
	TestWorld.PrimeTimerManager();
	const FHitResult Contact = MakeContact(Node, FrontFace(Node), FVector(70.0, -40.0, Eye));

	TestTrue(TEXT("The swing activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	Harvester.AbilitySystem->CancelAbilityHandle(Swing.Handle);
	TestFalse(TEXT("Cancelling, for example by a dodge or a tool switch, ends the swing"), Swing.Instance->IsActive());
	TestFalse(TEXT("The cancelled swing keeps no window or timers"), Swing.Instance->HasToolSwingResidualStateForTests());
	// A sample that arrives after the cancel, such as the window's final one, finds the swing closed.
	Swing.Instance->HandleToolSwingHitsForTests({Contact}, ERpgSwingTraceRole::Authority);
	TestWorld.AdvanceTimers(2.0f);
	TestEqual(TEXT("A cancelled swing takes nothing"), Node->HarvestableNode->GetRemainingSections(), 4);
	TestEqual(TEXT("A cancelled swing rewards nothing"), CountMaterial(Harvester.PlayerState), 0);
	TestEqual(TEXT("A cancelled swing plays no cue"), Cues.Cues.Num(), 0);

	TestTrue(TEXT("A new swing activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	Swing.Instance->HandleToolSwingHitsForTests({Contact}, ERpgSwingTraceRole::Authority);
	TestEqual(TEXT("A swing that is not cancelled takes its section"), Node->HarvestableNode->GetRemainingSections(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestSwingWeakPointTest,
	"SurvivalRpg.Harvesting.Swing.WeakPointByImpact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestSwingWeakPointTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestSwingTests;

	// The rule itself: an aim ray through the weak point counts, a swing's sweep past it does not.
	FRpgHarvestWeakPoint WeakPoint;
	WeakPoint.Location = FVector(100.0, 0.0, 0.0);
	WeakPoint.Radius = 20.0f;
	FHitResult Passing;
	Passing.TraceStart = FVector(100.0, -60.0, 0.0);
	Passing.Location = FVector(100.0, 40.0, 0.0);
	Passing.ImpactPoint = Passing.Location;
	TestTrue(TEXT("An aim ray through the weak point strikes it"), WeakPoint.IsStruckBy(Passing));
	TestFalse(TEXT("A swing's sweep past the weak point does not"), WeakPoint.IsStruckBy(Passing, true));
	FHitResult OnIt;
	OnIt.TraceStart = FVector(100.0, -60.0, 0.0);
	OnIt.ImpactPoint = FVector(95.0, 12.0, 0.0);
	OnIt.Location = OnIt.ImpactPoint;
	TestTrue(TEXT("A swing whose impact lies within the radius strikes it"), WeakPoint.IsStruckBy(OnIt, true));

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	URpgHarvestProfile* Profile = MakeProfile(World, 4);
	Profile->WeakPointBonusSections = 1;
	ARpgHarvestAutomationCollidableNodeActor* Node =
		SpawnNode(World, FVector(150.0, 0.0, Harvester.Pawn->BaseEyeHeight), Profile);
	const FGrantedAbility Swing = GrantSwing(Harvester.AbilitySystem, nullptr);
	if (!Node || !TestNotNull(TEXT("Swing exists"), Swing.Instance) ||
		!TestTrue(
			TEXT("Weak points are configured"),
			RpgHarvestAutomation::ConfigureWeakPoints(
				Node->HarvestableNode,
				{FVector(-40.0, 0.0, 0.0), FVector(-40.0, 30.0, 60.0), FVector(-40.0, -30.0, -60.0)},
				20.0f)))
	{
		return false;
	}
	Swing.Instance->ConfigureWeakPointHits(true);
	URpgHarvestAutomationHarvestedListener* Harvested = NewObject<URpgHarvestAutomationHarvestedListener>();
	Node->HarvestableNode->OnHarvested.AddDynamic(Harvested, &URpgHarvestAutomationHarvestedListener::HandleHarvested);
	TestWorld.PrimeTimerManager();

	FVector Active = FVector::ZeroVector;
	float Radius = 0.0f;
	TestTrue(TEXT("The node shows an active weak point"), Node->HarvestableNode->GetActiveWeakPoint(Active, Radius));
	TestTrue(TEXT("The swing activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	Swing.Instance->HandleToolSwingHitsForTests(
		{MakeContact(Node, Active + FVector(0.0, 12.0, 0.0), Active - FVector(0.0, 50.0, 0.0))},
		ERpgSwingTraceRole::Authority);
	TestEqual(TEXT("An impact on the weak point takes the bonus section"), Node->HarvestableNode->GetRemainingSections(), 2);

	Harvester.AbilitySystem->CancelAbilityHandle(Swing.Handle);
	TestTrue(TEXT("The moved weak point is active"), Node->HarvestableNode->GetActiveWeakPoint(Active, Radius));
	TestTrue(TEXT("The next swing activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	// The tool sweeps through the weak point but meets the resource 40 cm beside it.
	Swing.Instance->HandleToolSwingHitsForTests(
		{MakeContact(Node, Active + FVector(0.0, 40.0, 0.0), Active - FVector(0.0, 40.0, 0.0))},
		ERpgSwingTraceRole::Authority);
	TestEqual(TEXT("An impact beside the weak point takes one section"), Node->HarvestableNode->GetRemainingSections(), 1);
	TestTrue(
		TEXT("Only the first swing reports a weak-point hit"),
		Harvested->Results.Num() == 2 && Harvested->Results[0].bWeakPointHit && !Harvested->Results[1].bWeakPointHit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestSwingPierceTest,
	"SurvivalRpg.Harvesting.Swing.PierceFollowsHarvesterToImpact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestSwingPierceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestSwingTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	// The harvester faces the struck resource; Behind lies 3 m further on, Beside 3 m to the side, where the tool's
	// sideways sweep points.
	const double Eye = Harvester.Pawn->BaseEyeHeight;
	ARpgHarvestAutomationCollidableNodeActor* Struck = SpawnNode(World, FVector(150.0, 0.0, Eye), MakeProfile(World, 6));
	ARpgHarvestAutomationCollidableNodeActor* Behind = SpawnNode(World, FVector(450.0, 0.0, Eye), MakeProfile(World, 4));
	ARpgHarvestAutomationCollidableNodeActor* Beside = SpawnNode(World, FVector(150.0, 300.0, Eye), MakeProfile(World, 4));
	URpgSkillTreeDefinition* Tree = MakeSpreadTree();
	FScopedTreeTool TreeTool(Tree);
	URpgSkillTreeComponent* SkillTrees = PrepareSkillTree(Harvester, Tree);
	URpgEquipmentInstance* Equipment = MakeTreeToolEquipment(Harvester);
	const FGrantedAbility Swing = GrantSwing(Harvester.AbilitySystem, Equipment);
	if (!Struck || !Behind || !Beside || !TestNotNull(TEXT("Skill trees are prepared"), SkillTrees) ||
		!TestNotNull(TEXT("Swing exists"), Swing.Instance))
	{
		return false;
	}
	Swing.Instance->ConfigureSpread(ARpgHarvestRuneStrike::StaticClass(), ARpgHarvestRuneStrike::StaticClass(), 600.0f, 400.0f);
	TestEqual(TEXT("Pierce is learned"),
		SkillTrees->UnlockNode(TAG_HarvestTuningTest_Tree, TAG_HarvestSpreadTest_Pierce), ERpgSkillTreeUnlockResult::Unlockable);
	TestWorld.PrimeTimerManager();

	TestTrue(TEXT("The swing activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	// The blade meets the front face while it sweeps from left to right, along +Y.
	const FVector Impact = FrontFace(Struck);
	Swing.Instance->HandleToolSwingHitsForTests(
		{MakeContact(Struck, Impact, Impact - FVector(0.0, 60.0, 0.0))},
		ERpgSwingTraceRole::Authority);
	TestEqual(TEXT("The swing takes its section"), Struck->HarvestableNode->GetRemainingSections(), 5);
	TestWorld.AdvanceTimers(0.35f);
	TestEqual(TEXT("The pierce drives on from the harvester through the impact"), Behind->HarvestableNode->GetRemainingSections(), 3);
	TestTrue(
		TEXT("The pierce does not follow the tool's sideways sweep"),
		Beside->HarvestableNode->GetRemainingSections() == 4 && SectionWork(Beside) == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestSwingReachTuningTest,
	"SurvivalRpg.Harvesting.Swing.ReachTuningHitsTargetBehind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestSwingReachTuningTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestSwingTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	// Front stands where the tool head swings past, Behind 1.6 m further on, beyond the authored 200 cm reach.
	const double Eye = Harvester.Pawn->BaseEyeHeight;
	ARpgHarvestAutomationCollidableNodeActor* Front = SpawnNode(World, FVector(100.0, 0.0, Eye), MakeProfile(World, 4));
	ARpgHarvestAutomationCollidableNodeActor* Behind = SpawnNode(World, FVector(260.0, 0.0, Eye), MakeProfile(World, 4));
	URpgSkillTreeDefinition* Tree = MakeWideSwingTree();
	FScopedTreeTool TreeTool(Tree);
	URpgSkillTreeComponent* SkillTrees = PrepareSkillTree(Harvester, Tree);
	URpgEquipmentInstance* Equipment = MakeTreeToolEquipment(Harvester);
	const FGrantedAbility Swing = GrantSwing(Harvester.AbilitySystem, Equipment, 200.0f);
	if (!Front || !Behind || !TestNotNull(TEXT("Skill trees are prepared"), SkillTrees) ||
		!TestNotNull(TEXT("Swing exists"), Swing.Instance))
	{
		return false;
	}
	TestWorld.PrimeTimerManager();
	// The tool reaches from 40 to 110 cm in front of the harvester and sweeps from its left to its right.
	const RpgWeaponTraceInterpolation::FBladeSample Left = MakeToolPose(40.0, 110.0, -100.0, Eye);
	const RpgWeaponTraceInterpolation::FBladeSample Right = MakeToolPose(40.0, 110.0, 100.0, Eye);

	TestTrue(TEXT("A plain swing activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TraceSwing(*World, *Swing.Instance, Left, Right);
	TestEqual(TEXT("The tool's own sweep takes the front resource"), Front->HarvestableNode->GetRemainingSections(), 3);
	TestEqual(TEXT("The tool's own sweep never reaches the resource behind"), Behind->HarvestableNode->GetRemainingSections(), 4);

	Harvester.AbilitySystem->CancelAbilityHandle(Swing.Handle);
	TestEqual(TEXT("Wide Swing is learned"),
		SkillTrees->UnlockNode(TAG_HarvestTuningTest_Tree, TAG_HarvestSwingTest_Wide), ERpgSkillTreeUnlockResult::Unlockable);
	const FGameplayAbilitySpec* Spec = Harvester.AbilitySystem->FindAbilitySpecFromHandle(Swing.Handle);
	FRpgHarvestTunedValues Values;
	if (Spec)
	{
		Swing.Instance->ResolveTunedValues(*Spec, *Harvester.AbilitySystem->AbilityActorInfo, Values);
	}
	TestEqual(TEXT("The tuning extends the swing's reach"), Values.SwingReach, 150.0f);
	TestEqual(TEXT("The tuning adds a target per swing"), Values.SwingTargets, 2);

	TestTrue(TEXT("A wide swing activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TestEqual(TEXT("The wide swing traces 150 cm past the tool tip"), Swing.Instance->GetPlayedSwingTrace().TipExtension, 150.0f);
	TraceSwing(*World, *Swing.Instance, Left, Right);
	TestEqual(TEXT("The wide swing takes the front resource"), Front->HarvestableNode->GetRemainingSections(), 2);
	TestEqual(
		TEXT("The wide swing also takes the resource behind, within its grown reach"),
		Behind->HarvestableNode->GetRemainingSections(),
		3);
	TestEqual(TEXT("Both contacts count towards the two targets"), Swing.Instance->GetToolSwingContactCountForTests(), 2);
	TestEqual(TEXT("Every taken section is rewarded"), CountMaterial(Harvester.PlayerState), 3 * YieldPerSection);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestSwingEffectReachTest,
	"SurvivalRpg.Harvesting.Swing.EffectReachHitsTargetBehind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestSwingEffectReachTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestSwingTests;
	using namespace RpgHarvestingMagicGameplayTags;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	// The layout of ReachTuningHitsTargetBehind; a timed effect like the Wide Swing spell replaces the tree node, and
	// the swing has no source item, so no tree or rune could tune it.
	const double Eye = Harvester.Pawn->BaseEyeHeight;
	ARpgHarvestAutomationCollidableNodeActor* Front = SpawnNode(World, FVector(100.0, 0.0, Eye), MakeProfile(World, 4));
	ARpgHarvestAutomationCollidableNodeActor* Behind = SpawnNode(World, FVector(260.0, 0.0, Eye), MakeProfile(World, 4));
	const FGrantedAbility Swing = GrantSwing(Harvester.AbilitySystem, nullptr, 200.0f);
	FGameplayAbilitySpec* SwingSpec = Harvester.AbilitySystem->FindAbilitySpecFromHandle(Swing.Handle);
	if (!Front || !Behind || !TestNotNull(TEXT("Swing exists"), Swing.Instance) || !SwingSpec)
	{
		return false;
	}
	const FGameplayTag AxeChop = FGameplayTag::RequestGameplayTag(TEXT("Ability.Harvesting.AxeChop"));
	const FGameplayTag PickaxeStrike = FGameplayTag::RequestGameplayTag(TEXT("Ability.Harvesting.PickaxeStrike"));
	SwingSpec->GetDynamicSpecSourceTags().AddTag(AxeChop);
	auto MakeSwingTuning = [](const FGameplayTag AbilityId, const FGameplayTag TuningTag, const float Value)
	{
		FRpgSkillTreeAbilityTuning Tuning = MakeTuning(TuningTag, ERpgSkillTreeTuningOperation::Add, Value);
		Tuning.AbilityIdTag = AbilityId;
		return Tuning;
	};
	const UGameplayEffect* WideSwing = URpgAbilityTuningEffectComponent::MakeTransientEffectForTests({
		MakeSwingTuning(AxeChop, Ability_Tuning_Harvest_SwingReach, 150.0f),
		MakeSwingTuning(AxeChop, Ability_Tuning_Harvest_SwingTargets, 2.0f),
		MakeSwingTuning(PickaxeStrike, Ability_Tuning_Harvest_SwingReach, 150.0f),
		MakeSwingTuning(PickaxeStrike, Ability_Tuning_Harvest_SwingTargets, 2.0f)});
	TestWorld.PrimeTimerManager();
	const RpgWeaponTraceInterpolation::FBladeSample Left = MakeToolPose(40.0, 110.0, -100.0, Eye);
	const RpgWeaponTraceInterpolation::FBladeSample Right = MakeToolPose(40.0, 110.0, 100.0, Eye);

	TestTrue(TEXT("A plain swing activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TraceSwing(*World, *Swing.Instance, Left, Right);
	TestEqual(TEXT("The plain swing takes the front resource"), Front->HarvestableNode->GetRemainingSections(), 3);
	TestEqual(TEXT("The plain swing never reaches the resource behind"), Behind->HarvestableNode->GetRemainingSections(), 4);

	Harvester.AbilitySystem->CancelAbilityHandle(Swing.Handle);
	const FActiveGameplayEffectHandle SpellHandle =
		Harvester.AbilitySystem->ApplyGameplayEffectToSelf(WideSwing, 1.0f, Harvester.AbilitySystem->MakeEffectContext());
	TestTrue(TEXT("The Wide Swing effect applies"), SpellHandle.IsValid());
	FRpgHarvestTunedValues Values;
	Swing.Instance->ResolveTunedValues(*SwingSpec, *Harvester.AbilitySystem->AbilityActorInfo, Values);
	TestEqual(TEXT("The effect extends the swing's reach"), Values.SwingReach, 150.0f);
	TestEqual(TEXT("The effect adds two targets per swing"), Values.SwingTargets, 3);

	TestTrue(TEXT("A wide swing activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TestEqual(TEXT("The wide swing traces 150 cm past the tool tip"), Swing.Instance->GetPlayedSwingTrace().TipExtension, 150.0f);
	TraceSwing(*World, *Swing.Instance, Left, Right);
	TestEqual(TEXT("The wide swing takes the front resource"), Front->HarvestableNode->GetRemainingSections(), 2);
	TestEqual(TEXT("The wide swing also takes the resource behind"), Behind->HarvestableNode->GetRemainingSections(), 3);
	TestEqual(TEXT("Both contacts count"), Swing.Instance->GetToolSwingContactCountForTests(), 2);

	Harvester.AbilitySystem->CancelAbilityHandle(Swing.Handle);
	TestTrue(TEXT("The Wide Swing effect ends"), Harvester.AbilitySystem->RemoveActiveGameplayEffect(SpellHandle));
	TestTrue(TEXT("A swing after the spell activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TestEqual(TEXT("After the spell the swing traces to the tool tip"), Swing.Instance->GetPlayedSwingTrace().TipExtension, 0.0f);
	TraceSwing(*World, *Swing.Instance, Left, Right);
	TestEqual(TEXT("After the spell the swing takes the front resource"), Front->HarvestableNode->GetRemainingSections(), 1);
	TestEqual(TEXT("After the spell the resource behind is out of reach again"), Behind->HarvestableNode->GetRemainingSections(), 3);
	TestEqual(TEXT("Every taken section is rewarded"), CountMaterial(Harvester.PlayerState), 4 * YieldPerSection);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestSwingCloseContactTest,
	"SurvivalRpg.Harvesting.Swing.CloseContactHitsWithTheHandle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestSwingCloseContactTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestSwingTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	// A 40 cm trunk whose face is 50 cm in front of the harvester, as when the player stands close to a tree.
	const double Height = Harvester.Pawn->BaseEyeHeight - 40.0;
	ARpgHarvestAutomationCollidableNodeActor* Trunk = SpawnNode(World, FVector(70.0, 0.0, Height), MakeProfile(World, 4));
	const FGrantedAbility Swing = GrantSwing(Harvester.AbilitySystem, nullptr);
	if (!Trunk || !TestNotNull(TEXT("Swing exists"), Swing.Instance))
	{
		return false;
	}
	Trunk->Collision->SetBoxExtent(FVector(20.0, 20.0, 200.0));
	TestWorld.PrimeTimerManager();

	// The tool sweeps flat from the harvester's right to its left. The grip is 40 cm out and the head reaches from 110 to
	// 125 cm, so the head passes beyond the trunk's far corners (92 cm) while the handle crosses the trunk.
	const TArray<double> HeadOnly = {110.0, 125.0};
	const TArray<double> WholeTool = {40.0, 110.0, 125.0};

	TestTrue(TEXT("The swing activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	TraceSwing(*World, *Swing.Instance, MakeRadialToolPose(-70.0, Height, HeadOnly), MakeRadialToolPose(70.0, Height, HeadOnly));
	TestEqual(TEXT("Up close, a trace of the head alone passes beyond the trunk"), Trunk->HarvestableNode->GetRemainingSections(), 4);
	TestEqual(TEXT("The head alone touches nothing"), Swing.Instance->GetToolSwingContactCountForTests(), 0);

	TraceSwing(*World, *Swing.Instance, MakeRadialToolPose(-70.0, Height, WholeTool), MakeRadialToolPose(70.0, Height, WholeTool));
	TestEqual(TEXT("The trace over the whole tool meets the trunk with its handle"), Trunk->HarvestableNode->GetRemainingSections(), 3);
	TestEqual(TEXT("The handle's contact counts once"), Swing.Instance->GetToolSwingContactCountForTests(), 1);
	TestEqual(TEXT("The section is rewarded once"), CountMaterial(Harvester.PlayerState), YieldPerSection);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestSwingHitstopTest,
	"SurvivalRpg.Harvesting.Swing.LocalHitstopKeepsTheCadence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestSwingHitstopTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestSwingTests;

	// A 1.5 s montage that blends out over its last 0.25 s: the server times a 1.25 s cycle.
	UAnimMontage* Montage = MakeMontage({}, 1.5f);
	Montage->BlendOut.SetBlendTime(0.25f);

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	const double Eye = Harvester.Pawn->BaseEyeHeight;
	ARpgHarvestAutomationCollidableNodeActor* Front = SpawnNode(World, FVector(110.0, 0.0, Eye), MakeProfile(World, 8));
	ARpgHarvestAutomationCollidableNodeActor* Beside = SpawnNode(World, FVector(110.0, 100.0, Eye), MakeProfile(World, 8));
	// Wide Swing lets one swing take two contacts.
	URpgSkillTreeDefinition* Tree = MakeWideSwingTree();
	FScopedTreeTool TreeTool(Tree);
	URpgSkillTreeComponent* SkillTrees = PrepareSkillTree(Harvester, Tree);
	URpgEquipmentInstance* Equipment = MakeTreeToolEquipment(Harvester);
	const FGrantedAbility Swing = GrantSwing(Harvester.AbilitySystem, Equipment);
	if (!Front || !Beside || !TestNotNull(TEXT("Skill trees are prepared"), SkillTrees) ||
		!TestNotNull(TEXT("Swing exists"), Swing.Instance))
	{
		return false;
	}
	TestEqual(TEXT("Wide Swing is learned"),
		SkillTrees->UnlockNode(TAG_HarvestTuningTest_Tree, TAG_HarvestSwingTest_Wide), ERpgSkillTreeUnlockResult::Unlockable);
	Swing.Instance->ConfigureMontage(Montage);
	Swing.Instance->ConfigureHitstop(60.0f);
	TestWorld.PrimeTimerManager();
	URpgHarvestAutomationTestAbility& Ability = *Swing.Instance;
	const FHitResult FrontContact = MakeContact(Front, FrontFace(Front), FVector(70.0, -40.0, Eye));
	const FHitResult BesideContact = MakeContact(Beside, FrontFace(Beside), FVector(70.0, 60.0, Eye));

	// The test harvester is the server's own, locally controlled player, as a listen-server host is.
	TestTrue(TEXT("The swing activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	const double SwingStart = World->GetTimeSeconds();
	TestWorld.AdvanceTimers(0.3f);
	TestTrue(TEXT("Nothing freezes before a contact"), Ability.GetMontagePauseTimes().IsEmpty());
	Ability.HandleToolSwingHitsForTests({FrontContact}, ERpgSwingTraceRole::Authority);
	const double ContactTime = World->GetTimeSeconds();
	if (TestEqual(TEXT("The host's contact freezes its own montage once"), Ability.GetMontagePauseTimes().Num(), 1))
	{
		TestTrue(TEXT("The montage freezes at the contact"), FMath::IsNearlyEqual(Ability.GetMontagePauseTimes()[0], ContactTime, 0.001));
	}
	TestTrue(TEXT("The hitstop holds the montage"), Ability.IsToolSwingHitstopActiveForTests());
	TestEqual(TEXT("The contact takes its section during the freeze"), Front->HarvestableNode->GetRemainingSections(), 7);
	Ability.HandleToolSwingHitsForTests({BesideContact}, ERpgSwingTraceRole::Authority);
	TestEqual(TEXT("The swing's second contact counts"), Ability.GetToolSwingContactCountForTests(), 2);
	TestEqual(TEXT("A further contact of the same swing freezes nothing more"), Ability.GetMontagePauseTimes().Num(), 1);

	TestWorld.AdvanceTimers(0.05f);
	TestTrue(TEXT("50 ms after the contact the montage is still frozen"), Ability.IsToolSwingHitstopActiveForTests());
	TestWorld.AdvanceTimers(0.011f);
	TestFalse(TEXT("After the tuned 60 ms the montage plays on"), Ability.IsToolSwingHitstopActiveForTests());
	if (TestEqual(TEXT("The montage resumes once"), Ability.GetMontageResumeTimes().Num(), 1))
	{
		TestTrue(
			TEXT("The montage froze for the tuned 60 ms"),
			FMath::IsNearlyEqual(Ability.GetMontageResumeTimes()[0] - ContactTime, 0.061, 0.002));
	}

	// The server times the swing from its montage alone; the freeze neither shortens nor stretches the cadence.
	TestTrue(TEXT("The cycle is the montage's"), FMath::IsNearlyEqual(Ability.GetToolSwingCycleSecondsForTests(), 1.25f, 0.001f));
	Ability.SetRemoteInstanceHasEnded();
	TestTrue(
		TEXT("A swing cut short holds the next one back by the montage's cycle, as without a hitstop"),
		FMath::IsNearlyEqual(
			Ability.GetToolSwingEarliestNextStartForTests(),
			SwingStart + 1.25 - URpgGameplayAbility_Harvest::ToolSwingCadenceToleranceSeconds,
			0.001));
	Harvester.AbilitySystem->CancelAbilityHandle(Swing.Handle);
	TestWorld.AdvanceTimers(1.0f);

	// A remote owning client freezes at its predicted contact; a cancel during the freeze resumes the montage at once.
	TestTrue(TEXT("The next swing activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	Ability.HandleToolSwingHitsForTests({FrontContact}, ERpgSwingTraceRole::OwnerFeedback);
	TestEqual(TEXT("The owner's predicted contact freezes its montage"), Ability.GetMontagePauseTimes().Num(), 2);
	TestEqual(TEXT("The predicted contact takes nothing"), Front->HarvestableNode->GetRemainingSections(), 7);
	TestWorld.AdvanceTimers(0.02f);
	Harvester.AbilitySystem->CancelAbilityHandle(Swing.Handle);
	TestFalse(TEXT("A cancel during the freeze ends it"), Ability.IsToolSwingHitstopActiveForTests());
	TestEqual(TEXT("The cancel resumes the montage at once, so it can blend out"), Ability.GetMontageResumeTimes().Num(), 2);
	TestWorld.AdvanceTimers(0.1f);
	TestEqual(TEXT("No stale timer resumes it again"), Ability.GetMontageResumeTimes().Num(), 2);

	// Without a tuned hitstop nothing freezes.
	Ability.ConfigureHitstop(0.0f);
	TestTrue(TEXT("A swing without hitstop activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	Ability.HandleToolSwingHitsForTests({FrontContact}, ERpgSwingTraceRole::Authority);
	TestEqual(TEXT("The contact still takes its section"), Front->HarvestableNode->GetRemainingSections(), 6);
	TestEqual(TEXT("A swing without hitstop freezes nothing"), Ability.GetMontagePauseTimes().Num(), 2);
	Harvester.AbilitySystem->CancelAbilityHandle(Swing.Handle);
	return true;
}

namespace RpgHarvestHoldSwingTests
{
	using namespace RpgHarvestSwingTests;

	const FGameplayTag& PrimaryInputTag()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Weapon.Primary"));
		return Tag;
	}

	/** One frame of the player controller's input processing. */
	void ProcessInput(URpgAbilitySystemComponent& AbilitySystem)
	{
		AbilitySystem.ProcessAbilityInput(1.0f / 60.0f, false);
	}

	/** Makes the swing's class default a hold-to-repeat swing, as the tool swing assets are, until the test ends. */
	struct FScopedHoldToRepeat
	{
		FScopedHoldToRepeat() { GetMutableDefault<URpgHarvestAutomationTestAbility>()->ConfigureHoldToRepeat(true); }
		~FScopedHoldToRepeat() { GetMutableDefault<URpgHarvestAutomationTestAbility>()->ConfigureHoldToRepeat(false); }
	};

	/** Grants a hold-to-repeat tool swing from an axe on the primary input. */
	FGrantedAbility GrantHeldSwing(URpgAbilitySystemComponent* AbilitySystem, APawn* Pawn)
	{
		FGrantedAbility Granted = GrantSwing(AbilitySystem, NewObject<URpgEquipmentInstance>(Pawn), 250.0f, true);
		if (Granted.Instance)
		{
			Granted.Instance->ConfigureHoldToRepeat(true);
		}
		return Granted;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestSwingHoldTest,
	"SurvivalRpg.Harvesting.Swing.HoldKeepsSwingingUntilRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestSwingHoldTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestHoldSwingTests;

	const FScopedHoldToRepeat HoldToRepeat;
	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	const double Eye = Harvester.Pawn->BaseEyeHeight;
	ARpgHarvestAutomationCollidableNodeActor* Node = SpawnNode(World, FVector(110.0, 0.0, Eye), MakeProfile(World, 8));
	const FGrantedAbility Swing = GrantHeldSwing(Harvester.AbilitySystem, Harvester.Pawn);
	if (!Node || !TestNotNull(TEXT("Swing exists"), Swing.Instance))
	{
		return false;
	}
	Swing.Instance->ConfigureHitstop(60.0f);
	TestWorld.PrimeTimerManager();
	URpgAbilitySystemComponent& AbilitySystem = *Harvester.AbilitySystem;
	const FHitResult Contact = MakeContact(Node, FrontFace(Node), FVector(70.0, -40.0, Eye));

	AbilitySystem.AbilityInputTagPressed(PrimaryInputTag());
	ProcessInput(AbilitySystem);
	TestTrue(TEXT("Pressing the input starts a swing"), Swing.Instance->IsActive());
	Swing.Instance->HandleToolSwingHitsForTests({Contact}, ERpgSwingTraceRole::Authority);
	ProcessInput(AbilitySystem);
	ProcessInput(AbilitySystem);
	TestEqual(TEXT("A held input never restarts a running swing"), Swing.Instance->GetPlayedSwingCount(), 1);
	TestEqual(TEXT("The swing takes its section once"), Node->HarvestableNode->GetRemainingSections(), 7);
	TestEqual(TEXT("The swing's contact freezes its montage once"), Swing.Instance->GetMontagePauseTimes().Num(), 1);
	TestWorld.AdvanceTimers(0.1f);
	TestEqual(TEXT("The hitstop ends before the swing does"), Swing.Instance->GetMontageResumeTimes().Num(), 1);

	// The swing ends where its montage starts to blend out; the held input starts the next one in the next frame.
	Swing.Instance->HandleToolSwingBlendOutForTests();
	TestFalse(TEXT("The swing ends at its blend-out"), Swing.Instance->IsActive());
	ProcessInput(AbilitySystem);
	TestTrue(TEXT("The held input starts the next swing"), Swing.Instance->IsActive());
	TestEqual(TEXT("The next swing is a swing of its own"), Swing.Instance->GetPlayedSwingCount(), 2);
	TestEqual(TEXT("The next swing has counted no contact yet"), Swing.Instance->GetToolSwingContactCountForTests(), 0);
	Swing.Instance->HandleToolSwingHitsForTests({Contact}, ERpgSwingTraceRole::Authority);
	Swing.Instance->HandleToolSwingHitsForTests({Contact}, ERpgSwingTraceRole::Authority);
	TestEqual(TEXT("Each held swing strikes the resource once"), Node->HarvestableNode->GetRemainingSections(), 6);
	TestEqual(TEXT("Each held swing freezes once at its contact"), Swing.Instance->GetMontagePauseTimes().Num(), 2);

	// Released mid-swing: the running swing finishes, and no further one starts.
	AbilitySystem.AbilityInputTagReleased(PrimaryInputTag());
	ProcessInput(AbilitySystem);
	TestTrue(TEXT("Releasing the input lets the running swing finish"), Swing.Instance->IsActive());
	Swing.Instance->HandleToolSwingBlendOutForTests();
	ProcessInput(AbilitySystem);
	ProcessInput(AbilitySystem);
	TestFalse(TEXT("Without the input no further swing starts"), Swing.Instance->IsActive());
	TestEqual(TEXT("Two held swings ran"), Swing.Instance->GetPlayedSwingCount(), 2);
	TestEqual(TEXT("Both swings were rewarded once each"), CountMaterial(Harvester.PlayerState), 2 * YieldPerSection);
	TestEqual(TEXT("The server timed both swings"), Swing.Instance->GetToolSwingStartCountForTests(), 2u);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestSwingCancelSuppressionTest,
	"SurvivalRpg.Harvesting.Swing.CancelStopsHoldUntilPressedAgain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestSwingCancelSuppressionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestHoldSwingTests;

	const FScopedHoldToRepeat HoldToRepeat;
	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	const FGrantedAbility Swing = GrantHeldSwing(Harvester.AbilitySystem, Harvester.Pawn);
	if (!TestNotNull(TEXT("Swing exists"), Swing.Instance))
	{
		return false;
	}
	TestWorld.PrimeTimerManager();
	URpgAbilitySystemComponent& AbilitySystem = *Harvester.AbilitySystem;

	AbilitySystem.AbilityInputTagPressed(PrimaryInputTag());
	ProcessInput(AbilitySystem);
	TestTrue(TEXT("Pressing the input starts a swing"), Swing.Instance->IsActive());

	// A dodge, a block or an attack replaces the swing by cancelling it, as a tool switch does.
	AbilitySystem.CancelAbilityHandle(Swing.Handle);
	ProcessInput(AbilitySystem);
	ProcessInput(AbilitySystem);
	TestFalse(TEXT("A cancelled hold starts no new swing while the input stays held"), Swing.Instance->IsActive());
	AbilitySystem.AbilityInputTagReleased(PrimaryInputTag());
	ProcessInput(AbilitySystem);
	TestFalse(TEXT("The release itself starts nothing"), Swing.Instance->IsActive());
	AbilitySystem.AbilityInputTagPressed(PrimaryInputTag());
	ProcessInput(AbilitySystem);
	TestTrue(TEXT("A fresh press swings again"), Swing.Instance->IsActive());
	TestEqual(TEXT("Two presses started two swings"), Swing.Instance->GetPlayedSwingCount(), 2);

	// Another montage in the swing's slot group, such as a stagger, interrupts it the same way.
	Swing.Instance->HandleToolSwingInterruptedForTests();
	ProcessInput(AbilitySystem);
	ProcessInput(AbilitySystem);
	TestFalse(TEXT("An interrupted hold starts no new swing while the input stays held"), Swing.Instance->IsActive());
	AbilitySystem.AbilityInputTagReleased(PrimaryInputTag());
	ProcessInput(AbilitySystem);
	AbilitySystem.AbilityInputTagPressed(PrimaryInputTag());
	ProcessInput(AbilitySystem);
	TestTrue(TEXT("After the release a press swings again"), Swing.Instance->IsActive());

	// A swing that ends at its blend-out keeps the hold going.
	Swing.Instance->HandleToolSwingBlendOutForTests();
	ProcessInput(AbilitySystem);
	TestTrue(TEXT("A swing that ended normally keeps the hold going"), Swing.Instance->IsActive());
	TestEqual(TEXT("Every press and the held repeat started one swing each"), Swing.Instance->GetPlayedSwingCount(), 4);
	AbilitySystem.AbilityInputTagReleased(PrimaryInputTag());
	AbilitySystem.CancelAbilityHandle(Swing.Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestSwingCadenceTest,
	"SurvivalRpg.Harvesting.Swing.ClientCannotOutpaceTheMontage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestSwingCadenceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestHoldSwingTests;

	// A 1.5 s montage blends out over its last 0.25 s, so one swing lasts 1.25 s.
	UAnimMontage* Montage = MakeMontage({}, 1.5f);
	Montage->BlendOut.SetBlendTime(0.25f);
	TestTrue(
		TEXT("A swing cycle lasts until the montage starts to blend out"),
		FMath::IsNearlyEqual(URpgGameplayAbility_Harvest::ResolveSwingCycleSeconds(Montage, 1.0f), 1.25f, 0.001f));
	TestTrue(
		TEXT("A faster play rate shortens the cycle"),
		FMath::IsNearlyEqual(URpgGameplayAbility_Harvest::ResolveSwingCycleSeconds(Montage, 2.0f), 0.5f, 0.001f));
	TestEqual(TEXT("Without a montage there is no cycle"), URpgGameplayAbility_Harvest::ResolveSwingCycleSeconds(nullptr, 1.0f), 0.0f);

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	const FGrantedAbility Swing = GrantSwing(Harvester.AbilitySystem, nullptr);
	if (!TestNotNull(TEXT("Swing exists"), Swing.Instance))
	{
		return false;
	}
	Swing.Instance->ConfigureMontage(Montage);
	TestWorld.PrimeTimerManager();
	URpgAbilitySystemComponent& AbilitySystem = *Harvester.AbilitySystem;
	const double CycleSeconds = 1.25 - URpgGameplayAbility_Harvest::ToolSwingCadenceToleranceSeconds;

	// The owning client ends its swing itself 0.3 s in, long before the montage would, and asks for the next one.
	TestTrue(TEXT("The swing activates"), AbilitySystem.TryActivateAbility(Swing.Handle));
	const double FirstStart = World->GetTimeSeconds();
	TestWorld.AdvanceTimers(0.3f);
	Swing.Instance->SetRemoteInstanceHasEnded();
	AbilitySystem.CancelAbilityHandle(Swing.Handle);
	TestTrue(
		TEXT("The server holds the next swing back until the cut swing's cycle is over"),
		FMath::IsNearlyEqual(Swing.Instance->GetToolSwingEarliestNextStartForTests(), FirstStart + CycleSeconds, 0.001));
	TestFalse(TEXT("The client cannot swing again at once"), AbilitySystem.TryActivateAbility(Swing.Handle));
	TestWorld.AdvanceTimers(0.5f);
	TestFalse(TEXT("Nor before the montage's cycle is over"), AbilitySystem.TryActivateAbility(Swing.Handle));
	TestWorld.AdvanceTimers(0.3f);
	TestTrue(TEXT("Once the cycle is over the next swing starts"), AbilitySystem.TryActivateAbility(Swing.Handle));
	TestEqual(TEXT("Refused swings never started"), Swing.Instance->GetToolSwingStartCountForTests(), 2u);
	TestTrue(TEXT("A started swing holds nothing back"), Swing.Instance->GetToolSwingEarliestNextStartForTests() < 0.0);

	// A swing the server ends itself, at its blend-out or through its own dodge, keeps the hold seamless.
	Swing.Instance->HandleToolSwingBlendOutForTests();
	TestTrue(TEXT("A swing ended at its blend-out is followed at once"), AbilitySystem.TryActivateAbility(Swing.Handle));
	AbilitySystem.CancelAbilityHandle(Swing.Handle);
	TestTrue(TEXT("A swing the server cancelled is followed at once"), AbilitySystem.TryActivateAbility(Swing.Handle));
	AbilitySystem.CancelAbilityHandle(Swing.Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestSwingStruckTargetIndicatorTest,
	"SurvivalRpg.Harvesting.Swing.StruckTargetIndicatorLingers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestSwingStruckTargetIndicatorTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestHoldSwingTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()))
	{
		return false;
	}
	// The resource sits where the view's centre would aim, at eye height straight ahead.
	const double Eye = Harvester.Pawn->BaseEyeHeight;
	ARpgHarvestAutomationCollidableNodeActor* Node = SpawnNode(World, FVector(150.0, 0.0, Eye), MakeProfile(World, 4));
	URpgEquipmentInstance* Axe = NewObject<URpgEquipmentInstance>(Harvester.Pawn);
	const FGrantedAbility Swing = GrantSwing(Harvester.AbilitySystem, Axe, 250.0f, true);
	if (!Node || !TestNotNull(TEXT("Swing exists"), Swing.Instance))
	{
		return false;
	}

	// The local player's controller owns the harvester's state, so the swing finds the player's targeting component.
	FActorSpawnParameters ControllerParameters;
	ControllerParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APlayerController* Controller = World->SpawnActor<APlayerController>(
		APlayerController::StaticClass(),
		FTransform::Identity,
		ControllerParameters);
	if (!TestNotNull(TEXT("Local controller exists"), Controller))
	{
		return false;
	}
	Controller->SetAsLocalPlayerController();
	Controller->PlayerState = Harvester.PlayerState;
	Harvester.PlayerState->SetOwner(Controller);
	Harvester.AbilitySystem->InitAbilityActorInfo(Harvester.PlayerState, Harvester.Pawn);
	URpgIndicatorManagerComponent* IndicatorManager = NewObject<URpgIndicatorManagerComponent>(Controller);
	IndicatorManager->RegisterComponent();
	URpgHarvestAutomationTargetingComponent* Targeting = NewObject<URpgHarvestAutomationTargetingComponent>(Controller);
	Targeting->ConfigureIndicator(TSoftClassPtr<UUserWidget>(UUserWidget::StaticClass()));
	Targeting->ConfigureStruckTargetLinger(3.0f);
	Targeting->RegisterComponent();
	TestWorld.PrimeTimerManager();

	auto ExpectIndicatedStock = [this, IndicatorManager, Targeting, Node](const TCHAR* What, const int32 ExpectedRemaining)
	{
		if (!TestEqual(*FString::Printf(TEXT("%s: one indicator marks the struck resource"), What), IndicatorManager->GetIndicators().Num(), 1))
		{
			return;
		}
		const UIndicatorDescriptor* Indicator = IndicatorManager->GetIndicators()[0];
		TestTrue(*FString::Printf(TEXT("%s: the indicator anchors to the struck resource"), What), Indicator->GetSceneComponent() == Node->Collision);
		ERpgHarvestTargetStatus Status = ERpgHarvestTargetStatus::None;
		int32 Remaining = 0;
		int32 SectionCount = 0;
		int32 ToTake = 0;
		TestTrue(
			*FString::Printf(TEXT("%s: the struck resource is summarized"), What),
			Targeting->GetIndicatedTargetStatus(Indicator, Status, Remaining, SectionCount, ToTake));
		TestTrue(*FString::Printf(TEXT("%s: the struck resource is harvestable"), What), Status == ERpgHarvestTargetStatus::Harvestable);
		TestEqual(*FString::Printf(TEXT("%s: the indicator shows the stock"), What), Remaining, ExpectedRemaining);
		TestEqual(*FString::Printf(TEXT("%s: the indicator shows the section count"), What), SectionCount, 4);
		TestEqual(*FString::Printf(TEXT("%s: the next swing there takes one section"), What), ToTake, 1);
		TestTrue(*FString::Printf(TEXT("%s: the preview is the struck resource"), What), Targeting->IsPreviewStruckTarget());
	};

	Targeting->RefreshPreview();
	TestTrue(TEXT("The tool swing is previewed"), Targeting->GetCurrentPreview().HasAbility());
	TestEqual(TEXT("A tool swing runs no aim query, so the resource ahead is not marked"), Targeting->GetCurrentPreview().Targets.Num(), 0);
	TestEqual(TEXT("Nothing is indicated before a hit"), IndicatorManager->GetIndicators().Num(), 0);
	TestFalse(TEXT("No struck resource is previewed before a hit"), Targeting->IsPreviewStruckTarget());

	// The listen server's own swing: the stock shows at once, after the commit.
	const FHitResult Contact = MakeContact(Node, FrontFace(Node), FVector(70.0, -40.0, Eye));
	TestTrue(TEXT("The swing activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	Swing.Instance->HandleToolSwingHitsForTests({Contact}, ERpgSwingTraceRole::Authority);
	TestEqual(TEXT("The hit takes a section"), Node->HarvestableNode->GetRemainingSections(), 3);
	ExpectIndicatedStock(TEXT("After the server's own hit"), 3);

	// It stays for the linger time, then hides; no aim query brings it back.
	TestWorld.AdvanceTimers(2.5f);
	Targeting->RefreshPreview();
	ExpectIndicatedStock(TEXT("Within the linger time"), 3);
	TestWorld.AdvanceTimers(0.6f);
	TestEqual(TEXT("After the linger time the indicator hides"), IndicatorManager->GetIndicators().Num(), 0);
	TestFalse(TEXT("After the linger time no struck resource is previewed"), Targeting->IsPreviewStruckTarget());
	Targeting->RefreshPreview();
	TestEqual(TEXT("A refresh does not bring the indicator back"), IndicatorManager->GetIndicators().Num(), 0);

	// A remote owner's predicted contact shows the resource before the server's result, which corrects the stock.
	Harvester.AbilitySystem->CancelAbilityHandle(Swing.Handle);
	TestTrue(TEXT("A swing traced for feedback activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	Swing.Instance->HandleToolSwingHitsForTests({Contact}, ERpgSwingTraceRole::OwnerFeedback);
	ExpectIndicatedStock(TEXT("After the predicted hit"), 3);
	FRpgHarvestRequest ServerCommit;
	ServerCommit.Harvester = Harvester.Pawn;
	ServerCommit.AbilityId = RpgHarvestingMagicGameplayTags::Ability_Harvesting_Manual;
	ServerCommit.Hit = Contact;
	ServerCommit.ExpectedRevision = IRpgHarvestableTarget::Execute_GetHarvestRevision(Node->HarvestableNode, Contact);
	TestTrue(TEXT("The server commits the contact"), Node->HarvestableNode->CommitHarvest_Implementation(ServerCommit).IsSuccess());
	Targeting->RefreshPreview();
	ExpectIndicatedStock(TEXT("After the server's result replicated"), 2);

	// A further hit restarts the time.
	TestWorld.AdvanceTimers(2.0f);
	Harvester.AbilitySystem->CancelAbilityHandle(Swing.Handle);
	TestTrue(TEXT("The next swing activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	Swing.Instance->HandleToolSwingHitsForTests({Contact}, ERpgSwingTraceRole::OwnerFeedback);
	TestWorld.AdvanceTimers(2.0f);
	ExpectIndicatedStock(TEXT("A further hit restarts the linger time"), 2);
	TestWorld.AdvanceTimers(1.1f);
	TestEqual(TEXT("The restarted time runs out as well"), IndicatorManager->GetIndicators().Num(), 0);
	Harvester.AbilitySystem->CancelAbilityHandle(Swing.Handle);
	return true;
}

namespace RpgHarvestFocusTests
{
	using namespace RpgHarvestSwingTests;

	/** Focus a test swing adds per landed swing, as the axe and pickaxe swings do. */
	constexpr float GainPerSwing = 4.0f;

	/** Gives the harvester's ability system the focus set, as GF_Harvesting_Magic grants it to the player state. */
	bool GrantFocus(const FHarvesterFixture& Harvester)
	{
		Harvester.AbilitySystem->AddAttributeSetSubobject(NewObject<URpgFocusSet>(Harvester.PlayerState, NAME_None, RF_Transient));
		return Harvester.AbilitySystem->HasAttributeSetForAttribute(URpgFocusSet::GetFocusAttribute());
	}

	float GetFocus(const FHarvesterFixture& Harvester)
	{
		return Harvester.AbilitySystem->GetNumericAttribute(URpgFocusSet::GetFocusAttribute());
	}

	/** Grants a tool swing that fills GainPerSwing focus through the test gain effect. */
	FGrantedAbility GrantFillingSwing(const FHarvesterFixture& Harvester)
	{
		FGrantedAbility Swing = GrantSwing(Harvester.AbilitySystem, nullptr);
		if (Swing.Instance)
		{
			Swing.Instance->ConfigureFocusGain(URpgHarvestAutomationTestFocusGainEffect::StaticClass(), GainPerSwing);
		}
		return Swing;
	}

	/** Applies a timed effect with Tunings for every ability of the harvester, as a spell does. */
	FActiveGameplayEffectHandle ApplyTunings(const FHarvesterFixture& Harvester, const TArray<FRpgSkillTreeAbilityTuning>& Tunings)
	{
		const UGameplayEffect* Effect = URpgAbilityTuningEffectComponent::MakeTransientEffectForTests(Tunings);
		return Harvester.AbilitySystem->ApplyGameplayEffectToSelf(Effect, 1.0f, Harvester.AbilitySystem->MakeEffectContext());
	}

	/** Restores the class-default ignored tag of the test gain effect when a test ends. */
	struct FScopedIgnoredFocusTag
	{
		explicit FScopedIgnoredFocusTag(const FGameplayTag Tag)
		{
			URpgHarvestAutomationTestFocusGainEffect::SetIgnoredTargetTag(Tag);
		}

		~FScopedIgnoredFocusTag()
		{
			URpgHarvestAutomationTestFocusGainEffect::SetIgnoredTargetTag(FGameplayTag());
		}
	};

	FRpgHarvestTargetEvaluation MakeResult(const ERpgHarvestOutcome Outcome, const int32 WorkApplied, const ERpgHarvestWorkSource Source)
	{
		FRpgHarvestTargetEvaluation Target;
		Target.Result.Outcome = Outcome;
		Target.Result.WorkApplied = WorkApplied;
		Target.Result.Provenance.Source = Source;
		return Target;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestFocusOncePerSwingTest,
	"SurvivalRpg.Harvesting.Focus.SwingFillsOncePerSwing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestFocusOncePerSwingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestFocusTests;
	using namespace RpgHarvestingMagicGameplayTags;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()) ||
		!TestTrue(TEXT("The harvester has focus"), GrantFocus(Harvester)))
	{
		return false;
	}
	const double Eye = Harvester.Pawn->BaseEyeHeight;
	ARpgHarvestAutomationCollidableNodeActor* Left = SpawnNode(World, FVector(110.0, -100.0, Eye), MakeProfile(World, 4));
	ARpgHarvestAutomationCollidableNodeActor* Middle = SpawnNode(World, FVector(110.0, 0.0, Eye), MakeProfile(World, 4));
	ARpgHarvestAutomationCollidableNodeActor* Right = SpawnNode(World, FVector(110.0, 100.0, Eye), MakeProfile(World, 4));
	const FGrantedAbility Swing = GrantFillingSwing(Harvester);
	if (!Left || !Middle || !Right || !TestNotNull(TEXT("Swing exists"), Swing.Instance))
	{
		return false;
	}
	auto Contact = [](ARpgHarvestAutomationCollidableNodeActor* Node)
	{
		return MakeContact(Node, FrontFace(Node), FrontFace(Node) - FVector(40.0, 0.0, 0.0));
	};
	// Four targets per swing, as a spell such as Wide Swing gives.
	TestTrue(
		TEXT("The wide swing effect applies"),
		ApplyTunings(Harvester, {MakeTuning(Ability_Tuning_Harvest_SwingTargets, ERpgSkillTreeTuningOperation::Add, 3.0f)}).IsValid());
	TestWorld.PrimeTimerManager();

	TestEqual(TEXT("The meter starts empty"), GetFocus(Harvester), 0.0f);
	TestTrue(TEXT("The swing activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	Swing.Instance->HandleToolSwingHitsForTests({Contact(Left), Contact(Middle)}, ERpgSwingTraceRole::Authority);
	Swing.Instance->HandleToolSwingHitsForTests({Contact(Right)}, ERpgSwingTraceRole::Authority);
	TestTrue(
		TEXT("The swing takes a section from every resource it meets"),
		Left->HarvestableNode->GetRemainingSections() == 3 && Middle->HarvestableNode->GetRemainingSections() == 3 &&
			Right->HarvestableNode->GetRemainingSections() == 3);
	TestEqual(TEXT("Three accepted contacts fill the meter once"), GetFocus(Harvester), GainPerSwing);
	TestEqual(TEXT("The swing applied its gain once"), Swing.Instance->GetAuthorityFocusFillCountForTests(), 1u);

	// The swinging client's own trace only predicts; it never fills.
	Harvester.AbilitySystem->CancelAbilityHandle(Swing.Handle);
	TestTrue(TEXT("A swing traced for feedback activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	Swing.Instance->HandleToolSwingHitsForTests({Contact(Middle)}, ERpgSwingTraceRole::OwnerFeedback);
	TestEqual(TEXT("A feedback contact fills nothing"), GetFocus(Harvester), GainPerSwing);

	Harvester.AbilitySystem->CancelAbilityHandle(Swing.Handle);
	TestTrue(TEXT("The next swing activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	Swing.Instance->HandleToolSwingHitsForTests({Contact(Middle)}, ERpgSwingTraceRole::Authority);
	TestEqual(TEXT("Every landed swing fills anew"), GetFocus(Harvester), 2.0f * GainPerSwing);

	// A talent such as a faster meter tunes the gain; the swing captures it when it starts.
	Harvester.AbilitySystem->CancelAbilityHandle(Swing.Handle);
	TestTrue(
		TEXT("The faster meter effect applies"),
		ApplyTunings(Harvester, {MakeTuning(Ability_Tuning_Harvest_FocusGain, ERpgSkillTreeTuningOperation::Add, 2.0f)}).IsValid());
	TestTrue(TEXT("A tuned swing activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	Swing.Instance->HandleToolSwingHitsForTests({Contact(Middle)}, ERpgSwingTraceRole::Authority);
	TestEqual(TEXT("The tuned gain fills"), GetFocus(Harvester), 3.0f * GainPerSwing + 2.0f);
	TestEqual(TEXT("Each landed swing applied its gain once"), Swing.Instance->GetAuthorityFocusFillCountForTests(), 3u);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestFocusRejectedTest,
	"SurvivalRpg.Harvesting.Focus.RejectedContactsNeverFill",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestFocusRejectedTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestFocusTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()) ||
		!TestTrue(TEXT("The harvester has focus"), GrantFocus(Harvester)))
	{
		return false;
	}
	const double Eye = Harvester.Pawn->BaseEyeHeight;
	// The swing carries no tool, so the skinning-only resource rejects it.
	URpgHarvestProfile* SkinningOnly = MakeProfile(World, 4);
	SkinningOnly->RequiredToolTag = RpgHarvestingMagicGameplayTags::Tool_Harvesting_Skinning;
	ARpgHarvestAutomationCollidableNodeActor* WrongTool = SpawnNode(World, FVector(110.0, 0.0, Eye), SkinningOnly);
	ARpgHarvestAutomationCollidableNodeActor* Single = SpawnNode(World, FVector(110.0, -100.0, Eye), MakeProfile(World, 1));
	const FGrantedAbility Swing = GrantFillingSwing(Harvester);
	if (!WrongTool || !Single || !TestNotNull(TEXT("Swing exists"), Swing.Instance))
	{
		return false;
	}
	TestWorld.PrimeTimerManager();

	TestTrue(TEXT("A swing at the wrong resource activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	Swing.Instance->HandleToolSwingHitsForTests(
		{MakeContact(WrongTool, FrontFace(WrongTool), FVector(70.0, -40.0, Eye))},
		ERpgSwingTraceRole::Authority);
	TestEqual(TEXT("The wrong tool takes nothing"), WrongTool->HarvestableNode->GetRemainingSections(), 4);
	TestEqual(TEXT("A rejected contact fills nothing"), GetFocus(Harvester), 0.0f);

	Harvester.AbilitySystem->CancelAbilityHandle(Swing.Handle);
	TestTrue(TEXT("A swing at the single-section resource activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	Swing.Instance->HandleToolSwingHitsForTests(
		{MakeContact(Single, FrontFace(Single), FVector(70.0, -140.0, Eye))},
		ERpgSwingTraceRole::Authority);
	TestFalse(TEXT("The swing empties the single-section resource"), Single->HarvestableNode->IsHarvestable());
	TestEqual(TEXT("The emptying swing fills"), GetFocus(Harvester), GainPerSwing);

	Harvester.AbilitySystem->CancelAbilityHandle(Swing.Handle);
	TestTrue(TEXT("A swing at the empty resource activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	Swing.Instance->HandleToolSwingHitsForTests(
		{MakeContact(Single, FrontFace(Single), FVector(70.0, -140.0, Eye))},
		ERpgSwingTraceRole::Authority);
	TestEqual(TEXT("A contact with an empty resource fills nothing"), GetFocus(Harvester), GainPerSwing);
	TestEqual(TEXT("Only the landed swing applied its gain"), Swing.Instance->GetAuthorityFocusFillCountForTests(), 1u);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestFocusSecondaryWorkTest,
	"SurvivalRpg.Harvesting.Focus.SecondaryWorkNeverFills",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestFocusSecondaryWorkTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestFocusTests;
	using namespace RpgHarvestingMagicGameplayTags;

	// Only an accepted own hit counts: work a rejection or a follow-up did is never the player's swing.
	const ERpgHarvestWorkSource Direct = ERpgHarvestWorkSource::Direct;
	const ERpgHarvestWorkSource Secondary = ERpgHarvestWorkSource::Secondary;
	TestTrue(
		TEXT("An accepted own hit fills"),
		URpgGameplayAbility_Harvest::HasAcceptedDirectHit({MakeResult(ERpgHarvestOutcome::Harvested, 100, Direct)}));
	TestFalse(
		TEXT("Accepted secondary work never fills"),
		URpgGameplayAbility_Harvest::HasAcceptedDirectHit({MakeResult(ERpgHarvestOutcome::Harvested, 100, Secondary)}));
	TestFalse(
		TEXT("A rejected own hit never fills"),
		URpgGameplayAbility_Harvest::HasAcceptedDirectHit({MakeResult(ERpgHarvestOutcome::WrongTool, 0, Direct)}));
	TestFalse(
		TEXT("An own hit without accepted work never fills"),
		URpgGameplayAbility_Harvest::HasAcceptedDirectHit({MakeResult(ERpgHarvestOutcome::Harvested, 0, Direct)}));
	TestTrue(
		TEXT("One accepted own hit among chained resources fills"),
		URpgGameplayAbility_Harvest::HasAcceptedDirectHit(
			{MakeResult(ERpgHarvestOutcome::Harvested, 100, Secondary), MakeResult(ERpgHarvestOutcome::Harvested, 100, Direct)}));

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()) ||
		!TestTrue(TEXT("The harvester has focus"), GrantFocus(Harvester)))
	{
		return false;
	}
	// An echo on every own hit, as a socketed Echo rune with its talent does; the echo strikes later as secondary work.
	const double Eye = Harvester.Pawn->BaseEyeHeight;
	ARpgHarvestAutomationCollidableNodeActor* Struck = SpawnNode(World, FVector(150.0, 0.0, Eye), MakeProfile(World, 6));
	const FGrantedAbility Power = GrantAbility(Harvester.AbilitySystem);
	if (!Struck || !TestNotNull(TEXT("Power exists"), Power.Instance))
	{
		return false;
	}
	Power.Instance->ConfigureFocusGain(URpgHarvestAutomationTestFocusGainEffect::StaticClass(), GainPerSwing);
	Power.Instance->ConfigureRuneFlow(ARpgHarvestRuneStrike::StaticClass(), ARpgHarvestRuneStrike::StaticClass(), 800.0f);
	TestTrue(
		TEXT("The echo effect applies"),
		ApplyTunings(
			Harvester,
			{MakeTuning(Ability_Tuning_Harvest_EchoWork, ERpgSkillTreeTuningOperation::Set, 0.5f),
				MakeTuning(Ability_Tuning_Harvest_EchoHits, ERpgSkillTreeTuningOperation::Set, 1.0f)})
			.IsValid());
	TestWorld.PrimeTimerManager();

	TestTrue(TEXT("The own hit activates"), Harvester.AbilitySystem->TryActivateAbility(Power.Handle));
	TestEqual(TEXT("The own hit takes its section"), Struck->HarvestableNode->GetRemainingSections(), 5);
	TestEqual(TEXT("The own hit fills once"), GetFocus(Harvester), GainPerSwing);
	TestWorld.AdvanceTimers(0.35f);
	TestEqual(
		TEXT("The echo strikes the resource with half the work"),
		static_cast<int32>(Struck->HarvestableNode->GetHarvestState().SectionWork),
		50);
	TestEqual(TEXT("The echo's secondary work fills nothing"), GetFocus(Harvester), GainPerSwing);
	TestEqual(TEXT("The execution applied its gain once"), Power.Instance->GetAuthorityFocusFillCountForTests(), 1u);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestFocusStrideTest,
	"SurvivalRpg.Harvesting.Focus.StridePulsesNeverFill",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestFocusStrideTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestFocusTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()) ||
		!TestTrue(TEXT("The harvester can walk"), RpgHarvestStrideTests::MakeWalkable(Harvester)) ||
		!TestTrue(TEXT("The harvester has focus"), GrantFocus(Harvester)))
	{
		return false;
	}
	ARpgHarvestAutomationCollidableNodeActor* Near = SpawnNode(World, FVector(200.0, 0.0, 0.0), MakeProfile(World, 4));
	const FGrantedAbility Stride = RpgHarvestStrideTests::GrantStrideAbility(Harvester.AbilitySystem, 4, 1);
	if (!Near || !TestNotNull(TEXT("Stride ability exists"), Stride.Instance))
	{
		return false;
	}
	Stride.Instance->ConfigureFocusGain(URpgHarvestAutomationTestFocusGainEffect::StaticClass(), GainPerSwing);
	TestWorld.PrimeTimerManager();

	TestTrue(TEXT("The stride activates"), Harvester.AbilitySystem->TryActivateAbility(Stride.Handle));
	TestEqual(TEXT("The first pulse harvests at once"), Near->HarvestableNode->GetRemainingSections(), 3);
	TestWorld.AdvanceTimers(0.55f);
	TestEqual(TEXT("A later pulse harvests again"), Near->HarvestableNode->GetRemainingSections(), 2);
	TestWorld.AdvanceTimers(2.0f);
	TestFalse(TEXT("The stride ends"), Stride.Instance->IsStriding());
	TestEqual(TEXT("Stride pulses fill nothing"), GetFocus(Harvester), 0.0f);
	TestEqual(TEXT("The stride applied no gain"), Stride.Instance->GetAuthorityFocusFillCountForTests(), 0u);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestFocusUnconfiguredTest,
	"SurvivalRpg.Harvesting.Focus.ZeroGainNeverFills",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestFocusUnconfiguredTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestFocusTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()) ||
		!TestTrue(TEXT("The harvester has focus"), GrantFocus(Harvester)))
	{
		return false;
	}
	const double Eye = Harvester.Pawn->BaseEyeHeight;
	ARpgHarvestAutomationCollidableNodeActor* Node = SpawnNode(World, FVector(110.0, 0.0, Eye), MakeProfile(World, 4));
	const FGrantedAbility Swing = GrantSwing(Harvester.AbilitySystem, nullptr);
	if (!Node || !TestNotNull(TEXT("Swing exists"), Swing.Instance))
	{
		return false;
	}
	TestEqual(TEXT("Harvest abilities fill nothing by default"), GetDefault<URpgHarvestAutomationTestAbility>()->GetFocusGainPerSwing(), 0.0f);
	TestWorld.PrimeTimerManager();
	const FHitResult Contact = MakeContact(Node, FrontFace(Node), FVector(70.0, -40.0, Eye));

	// The gain effect alone, with the default gain of zero.
	Swing.Instance->ConfigureFocusGain(URpgHarvestAutomationTestFocusGainEffect::StaticClass(), 0.0f);
	TestTrue(TEXT("A swing without gain activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	Swing.Instance->HandleToolSwingHitsForTests({Contact}, ERpgSwingTraceRole::Authority);
	TestEqual(TEXT("The swing without gain harvests"), Node->HarvestableNode->GetRemainingSections(), 3);
	TestEqual(TEXT("A gain of zero fills nothing"), GetFocus(Harvester), 0.0f);

	// A gain without an effect to apply it.
	Harvester.AbilitySystem->CancelAbilityHandle(Swing.Handle);
	Swing.Instance->ConfigureFocusGain(nullptr, GainPerSwing);
	TestTrue(TEXT("A swing without gain effect activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	Swing.Instance->HandleToolSwingHitsForTests({Contact}, ERpgSwingTraceRole::Authority);
	TestEqual(TEXT("The swing without gain effect harvests"), Node->HarvestableNode->GetRemainingSections(), 2);
	TestEqual(TEXT("Without a gain effect nothing fills"), GetFocus(Harvester), 0.0f);
	TestEqual(TEXT("Neither swing applied a gain"), Swing.Instance->GetAuthorityFocusFillCountForTests(), 0u);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestFocusIgnoredTagTest,
	"SurvivalRpg.Harvesting.Focus.GainWaitsWhileItsIgnoredTagIsOwned",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestFocusIgnoredTagTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestFocusTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FHarvesterFixture Harvester = SpawnHarvester(World);
	if (!TestTrue(TEXT("Harvester fixture exists"), Harvester.IsValid()) ||
		!TestTrue(TEXT("The harvester has focus"), GrantFocus(Harvester)))
	{
		return false;
	}
	const double Eye = Harvester.Pawn->BaseEyeHeight;
	ARpgHarvestAutomationCollidableNodeActor* Node = SpawnNode(World, FVector(110.0, 0.0, Eye), MakeProfile(World, 4));
	const FGrantedAbility Swing = GrantFillingSwing(Harvester);
	if (!Node || !TestNotNull(TEXT("Swing exists"), Swing.Instance))
	{
		return false;
	}
	// The gain ignores a running spell, as GE_HarvestFocus_Gain ignores Harvest.Spell.Active.
	FScopedIgnoredFocusTag IgnoredTag(TAG_HarvestFocusTest_Spell);
	TestWorld.PrimeTimerManager();
	const FHitResult Contact = MakeContact(Node, FrontFace(Node), FVector(70.0, -40.0, Eye));

	Harvester.AbilitySystem->AddLooseGameplayTag(TAG_HarvestFocusTest_Spell);
	TestTrue(TEXT("A swing during the spell activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	Swing.Instance->HandleToolSwingHitsForTests({Contact}, ERpgSwingTraceRole::Authority);
	TestEqual(TEXT("The swing during the spell harvests"), Node->HarvestableNode->GetRemainingSections(), 3);
	TestEqual(TEXT("The swing during the spell fills nothing"), GetFocus(Harvester), 0.0f);
	TestEqual(TEXT("The refused gain is not counted as applied"), Swing.Instance->GetAuthorityFocusFillCountForTests(), 0u);

	Harvester.AbilitySystem->CancelAbilityHandle(Swing.Handle);
	Harvester.AbilitySystem->RemoveLooseGameplayTag(TAG_HarvestFocusTest_Spell);
	TestTrue(TEXT("A swing after the spell activates"), Harvester.AbilitySystem->TryActivateAbility(Swing.Handle));
	Swing.Instance->HandleToolSwingHitsForTests({Contact}, ERpgSwingTraceRole::Authority);
	TestEqual(TEXT("A swing after the spell fills again"), GetFocus(Harvester), GainPerSwing);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
