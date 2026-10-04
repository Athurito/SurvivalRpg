#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystem/Abilities/RpgGameplayAbility_Harvest.h"
#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
#include "Harvesting/RpgHarvestableComponent.h"
#include "Harvesting/RpgHarvestAutomationTestTypes.h"
#include "Harvesting/RpgHarvestAutomationTestWorld.h"
#include "Harvesting/RpgHarvestProfile.h"
#include "Harvesting/RpgHarvestTargetingComponent.h"
#include "SurvivalRpg/Animation/AnimNotify_RpgGameplayEvent.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentInstance.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootTable.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillGameplayTags.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillProgressionComponent.h"
#include "SurvivalRpg/UI/IndicatorSystem/RpgIndicatorManagerComponent.h"
#include "Blueprint/UserWidget.h"

#include "Animation/AnimMontage.h"
#include "Components/BoxComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/DecalActor.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"

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

#endif // WITH_DEV_AUTOMATION_TESTS
