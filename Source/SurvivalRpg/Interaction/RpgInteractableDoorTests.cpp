// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Interaction/Abilities/RpgGameplayAbility_ExecuteInteraction.h"
#include "SurvivalRpg/Interaction/Components/RpgInteractableDoorComponent.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"
#include "SurvivalRpg/Interaction/InteractionStatics.h"

#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

namespace RpgInteractableDoorTests
{
	class FScopedTestWorld
	{
	public:
		FScopedTestWorld()
		{
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			if (!GameInstance)
			{
				return;
			}

			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}

		~FScopedTestWorld()
		{
			UWorld* WorldToDestroy = World;
			if (GameInstance)
			{
				GameInstance->Shutdown();
			}

			if (WorldToDestroy)
			{
				GEngine->DestroyWorldContext(WorldToDestroy);
				WorldToDestroy->DestroyWorld(false);
			}

			if (GameInstance)
			{
				GameInstance->RemoveFromRoot();
			}
		}

		UWorld* GetWorld() const
		{
			return World;
		}

	private:
		TObjectPtr<UGameInstance> GameInstance = nullptr;
		TObjectPtr<UWorld> World = nullptr;
	};

	URpgInteractableDoorComponent* SpawnDoor(UWorld* World, AActor*& OutOwner)
	{
		OutOwner = nullptr;
		if (!World)
		{
			return nullptr;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = MakeUniqueObjectName(World, AActor::StaticClass(), TEXT("DoorTestOwner"));
		SpawnParameters.ObjectFlags = RF_Transient;
		OutOwner = World->SpawnActor<AActor>(SpawnParameters);
		if (!OutOwner)
		{
			return nullptr;
		}

		USceneComponent* RootComponent = NewObject<USceneComponent>(
			OutOwner,
			MakeUniqueObjectName(OutOwner, USceneComponent::StaticClass(), TEXT("Root")),
			RF_Transient);
		OutOwner->AddInstanceComponent(RootComponent);
		OutOwner->SetRootComponent(RootComponent);
		RootComponent->RegisterComponent();

		URpgInteractableDoorComponent* Door = NewObject<URpgInteractableDoorComponent>(
			OutOwner,
			MakeUniqueObjectName(OutOwner, URpgInteractableDoorComponent::StaticClass(), TEXT("Door")),
			RF_Transient);
		OutOwner->AddInstanceComponent(Door);
		Door->RegisterComponent();
		return Door;
	}

	bool GatherSingleOption(
		URpgInteractableDoorComponent* Door,
		AActor* Requester,
		FInteractionOption& OutOption)
	{
		if (!Door)
		{
			return false;
		}

		TScriptInterface<IInteractableTarget> Target;
		Target.SetObject(Door);
		Target.SetInterface(Door);

		FInteractionQuery Query;
		Query.RequestingAvatar = Requester;
		Query.QueryMode = ERpgInteractionQueryMode::AuthorityValidation;
		Query.QueryOrigin = Requester ? Requester->GetActorLocation() : FVector::ZeroVector;

		TArray<FInteractionOption> Options;
		FInteractionOptionBuilder Builder(Target, Options);
		Door->GatherInteractionOptions(Query, Builder);
		if (Options.Num() != 1)
		{
			return false;
		}

		OutOption = Options[0];
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInteractableDoorStateRevisionTest,
	"SurvivalRpg.Interaction.Door.StateAndRevision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInteractableDoorStateRevisionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgInteractableDoorTests;

	FScopedTestWorld TestWorld;
	AActor* DoorOwner = nullptr;
	URpgInteractableDoorComponent* Door = SpawnDoor(TestWorld.GetWorld(), DoorOwner);
	if (!TestNotNull(TEXT("Standalone door test world exists"), TestWorld.GetWorld()) ||
		!TestNotNull(TEXT("Door owner is spawned"), DoorOwner) ||
		!TestNotNull(TEXT("Door component is registered"), Door))
	{
		return false;
	}

	TestTrue(TEXT("Standalone door owner has server authority"), DoorOwner->HasAuthority());
	TestFalse(TEXT("Door starts closed"), Door->IsDoorOpen());
	TestFalse(TEXT("Door starts unlocked"), Door->IsDoorLocked());

	FInteractionOption Option;
	if (!TestTrue(TEXT("Closed door exposes exactly one interaction option"), GatherSingleOption(Door, DoorOwner, Option)))
	{
		return false;
	}
	TestTrue(TEXT("Closed unlocked door exposes Open"), Option.InteractionTag == RpgGameplayTags::Rpg_Interaction_Action_Door_Open);
	TestEqual(TEXT("Authored door state starts at revision zero"), Option.TargetRef.Revision, 0);

	TestTrue(TEXT("Authority can open an unlocked door"), Door->SetDoorOpen(true));
	TestTrue(TEXT("Open state is applied immediately on authority"), Door->IsDoorOpen());
	TestTrue(TEXT("Open door exposes one Close option"), GatherSingleOption(Door, DoorOwner, Option));
	TestTrue(TEXT("Open door exposes Close"), Option.InteractionTag == RpgGameplayTags::Rpg_Interaction_Action_Door_Close);
	TestEqual(TEXT("Opening increments the state revision once"), Option.TargetRef.Revision, 1);

	TestTrue(TEXT("Setting the existing open state is an idempotent success"), Door->SetDoorOpen(true));
	TestTrue(TEXT("Idempotent open keeps one option"), GatherSingleOption(Door, DoorOwner, Option));
	TestEqual(TEXT("Idempotent open does not increment the revision"), Option.TargetRef.Revision, 1);

	TestTrue(TEXT("Authority can lock the open door"), Door->SetDoorLocked(true));
	TestTrue(TEXT("Door becomes locked"), Door->IsDoorLocked());
	TestFalse(TEXT("Locking also closes the door"), Door->IsDoorOpen());
	TestTrue(TEXT("Locked door still exposes a visible option"), GatherSingleOption(Door, DoorOwner, Option));
	TestTrue(TEXT("Locked door keeps Open as the blocked semantic action"), Option.InteractionTag == RpgGameplayTags::Rpg_Interaction_Action_Door_Open);
	TestEqual(TEXT("Locked door remains visible as Blocked"), Option.Availability, ERpgInteractionAvailability::Blocked);
	TestFalse(TEXT("Locked door supplies a player-facing reason"), Option.Prompt.BlockedReason.IsEmpty());
	TestEqual(TEXT("Lock-and-close is one atomic revision"), Option.TargetRef.Revision, 2);

	TestFalse(TEXT("A locked door cannot be opened"), Door->SetDoorOpen(true));
	TestTrue(TEXT("Rejected open keeps the locked prompt"), GatherSingleOption(Door, DoorOwner, Option));
	TestEqual(TEXT("Rejected open does not increment the revision"), Option.TargetRef.Revision, 2);

	TestTrue(TEXT("Authority can unlock the door directly"), Door->SetDoorLocked(false));
	TestTrue(TEXT("Unlocked door exposes one option"), GatherSingleOption(Door, DoorOwner, Option));
	TestEqual(TEXT("Unlocking increments the revision once"), Option.TargetRef.Revision, 3);

	const FProperty* OpenProperty = FindFProperty<FProperty>(URpgInteractableDoorComponent::StaticClass(), TEXT("bIsOpen"));
	const FProperty* LockedProperty = FindFProperty<FProperty>(URpgInteractableDoorComponent::StaticClass(), TEXT("bIsLocked"));
	const FProperty* RevisionProperty = FindFProperty<FProperty>(URpgInteractableDoorComponent::StaticClass(), TEXT("DoorStateRevision"));
	TestTrue(TEXT("Open state participates in replication"), OpenProperty && OpenProperty->HasAnyPropertyFlags(CPF_Net));
	TestTrue(TEXT("Open state has an OnRep presentation hook"), OpenProperty && OpenProperty->HasAnyPropertyFlags(CPF_RepNotify));
	TestTrue(TEXT("Locked state participates in replication"), LockedProperty && LockedProperty->HasAnyPropertyFlags(CPF_Net));
	TestTrue(TEXT("Locked state has an OnRep presentation hook"), LockedProperty && LockedProperty->HasAnyPropertyFlags(CPF_RepNotify));
	TestTrue(TEXT("Door revision participates in replication"), RevisionProperty && RevisionProperty->HasAnyPropertyFlags(CPF_Net));
	TestTrue(TEXT("Door revision has an OnRep presentation hook"), RevisionProperty && RevisionProperty->HasAnyPropertyFlags(CPF_RepNotify));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInteractableDoorAuthorityCommitTest,
	"SurvivalRpg.Interaction.Door.AuthorityAndStaleCommit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInteractableDoorAuthorityCommitTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgInteractableDoorTests;

	FScopedTestWorld TestWorld;
	AActor* DoorOwner = nullptr;
	URpgInteractableDoorComponent* Door = SpawnDoor(TestWorld.GetWorld(), DoorOwner);
	FActorSpawnParameters RequesterSpawnParameters;
	RequesterSpawnParameters.Name = MakeUniqueObjectName(TestWorld.GetWorld(), AActor::StaticClass(), TEXT("DoorRequester"));
	RequesterSpawnParameters.ObjectFlags = RF_Transient;
	AActor* Requester = TestWorld.GetWorld()
		? TestWorld.GetWorld()->SpawnActor<AActor>(RequesterSpawnParameters)
		: nullptr;
	if (!TestNotNull(TEXT("Door component exists"), Door) ||
		!TestNotNull(TEXT("Door requester exists"), Requester))
	{
		return false;
	}

	FInteractionOption InitialOpenOption;
	if (!TestTrue(TEXT("Initial Open option can be gathered"), GatherSingleOption(Door, Requester, InitialOpenOption)))
	{
		return false;
	}

	FInteractionQuery AuthorityQuery;
	AuthorityQuery.RequestingAvatar = Requester;
	AuthorityQuery.QueryMode = ERpgInteractionQueryMode::AuthorityValidation;
	AuthorityQuery.QueryOrigin = Requester->GetActorLocation();

	DoorOwner->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("A simulated proxy cannot mutate open state"), Door->SetDoorOpen(true));
	TestFalse(TEXT("A simulated proxy cannot mutate lock state"), Door->SetDoorLocked(true));
	TestFalse(TEXT("A simulated proxy cannot commit an otherwise valid option"), Door->CommitInteraction(AuthorityQuery, InitialOpenOption));
	TestFalse(TEXT("Rejected proxy mutations leave the door closed"), Door->IsDoorOpen());
	TestFalse(TEXT("Rejected proxy mutations leave the door unlocked"), Door->IsDoorLocked());

	DoorOwner->SetRole(ROLE_Authority);
	TestTrue(TEXT("Authority can commit the exact current Open option"), Door->CommitInteraction(AuthorityQuery, InitialOpenOption));
	TestTrue(TEXT("Committed Open changes authoritative state"), Door->IsDoorOpen());
	TestFalse(TEXT("The same now-stale option cannot be replayed"), Door->CommitInteraction(AuthorityQuery, InitialOpenOption));

	FInteractionOption CloseOption;
	if (!TestTrue(TEXT("Current Close option can be gathered"), GatherSingleOption(Door, Requester, CloseOption)))
	{
		return false;
	}
	TestEqual(TEXT("Close option observes the incremented revision"), CloseOption.TargetRef.Revision, 1);
	TestTrue(TEXT("Authority can commit the current Close option"), Door->CommitInteraction(AuthorityQuery, CloseOption));
	TestFalse(TEXT("Committed Close changes authoritative state"), Door->IsDoorOpen());

	FInteractionOption ReopenedOption;
	TestTrue(TEXT("Closed state remains gatherable after commit"), GatherSingleOption(Door, Requester, ReopenedOption));
	TestEqual(TEXT("Open and Close commits each advance revision once"), ReopenedOption.TargetRef.Revision, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInteractionCommonAuthorityValidationTest,
	"SurvivalRpg.Interaction.AuthorityValidation.CommonRejectionMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInteractionCommonAuthorityValidationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgInteractableDoorTests;

	FScopedTestWorld TestWorld;
	AActor* DoorOwner = nullptr;
	URpgInteractableDoorComponent* Door = SpawnDoor(TestWorld.GetWorld(), DoorOwner);
	FActorSpawnParameters RequesterSpawnParameters;
	RequesterSpawnParameters.Name = MakeUniqueObjectName(TestWorld.GetWorld(), AActor::StaticClass(), TEXT("ValidationRequester"));
	RequesterSpawnParameters.ObjectFlags = RF_Transient;
	AActor* Requester = TestWorld.GetWorld()
		? TestWorld.GetWorld()->SpawnActor<AActor>(RequesterSpawnParameters)
		: nullptr;
	if (!TestNotNull(TEXT("Authority-validation door exists"), Door) ||
		!TestNotNull(TEXT("Authority-validation requester exists"), Requester))
	{
		return false;
	}

	URpgAbilitySystemComponent* AbilitySystem = NewObject<URpgAbilitySystemComponent>(
		Requester,
		MakeUniqueObjectName(Requester, URpgAbilitySystemComponent::StaticClass(), TEXT("AbilitySystem")),
		RF_Transient);
	if (!TestNotNull(TEXT("Requester owns an RPG ability system"), AbilitySystem))
	{
		return false;
	}
	Requester->AddInstanceComponent(AbilitySystem);
	AbilitySystem->RegisterComponent();
	AbilitySystem->InitAbilityActorInfo(Requester, Requester);
	if (!TestTrue(TEXT("Ability actor info is initialized"), AbilitySystem->AbilityActorInfo.IsValid()))
	{
		return false;
	}

	FGameplayAbilitySpec ExecuteSpec(URpgGameplayAbility_ExecuteInteraction::StaticClass(), 1);
	const FGameplayAbilitySpecHandle ExecuteHandle = AbilitySystem->GiveAbility(ExecuteSpec);
	if (!TestTrue(TEXT("Generic execution ability is granted"), ExecuteHandle.IsValid()))
	{
		return false;
	}

	DoorOwner->SetActorLocation(FVector(100.0, 0.0, 0.0));
	FInteractionOption CurrentOption;
	if (!TestTrue(TEXT("Current door option can be gathered"), GatherSingleOption(Door, Requester, CurrentOption)))
	{
		return false;
	}

	FGameplayEventData ValidPayload;
	if (!TestTrue(
		TEXT("Current option produces a complete GAS event payload"),
		UInteractionStatics::BuildInteractionEventData(CurrentOption, Requester, AbilitySystem, ValidPayload)))
	{
		return false;
	}

	auto Validate = [this, AbilitySystem](const TCHAR* Label, const FGameplayEventData& Payload, const bool bExpected)
	{
		FInteractionOption ValidatedOption;
		FInteractionQuery AuthoritativeQuery;
		FText FailureReason;
		const bool bActual = UInteractionStatics::ValidateInteractionEventData(
			*AbilitySystem->AbilityActorInfo,
			&Payload,
			ValidatedOption,
			AuthoritativeQuery,
			FailureReason);
		TestEqual(Label, bActual, bExpected);
		if (!bExpected)
		{
			TestFalse(*FString::Printf(TEXT("%s supplies a rejection reason"), Label), FailureReason.IsEmpty());
		}
		return bActual;
	};

	Validate(TEXT("Exact current tag, revision, target and ability spec validate"), ValidPayload, true);

	FGameplayEventData WrongTagPayload = ValidPayload;
	WrongTagPayload.TargetTags.Reset();
	WrongTagPayload.TargetTags.AddTag(RpgGameplayTags::Rpg_Interaction_Action_Collect);
	Validate(TEXT("A different interaction action tag is rejected"), WrongTagPayload, false);

	FGameplayEventData StaleRevisionPayload = ValidPayload;
	StaleRevisionPayload.EventMagnitude = static_cast<float>(CurrentOption.TargetRef.Revision + 1);
	Validate(TEXT("A stale or fabricated target revision is rejected"), StaleRevisionPayload, false);

	FGameplayEventData WrongInstancePayload = ValidPayload;
	FGameplayEffectContextHandle WrongInstanceContext = AbilitySystem->MakeEffectContext();
	FHitResult WrongInstanceHit(DoorOwner, nullptr, DoorOwner->GetActorLocation(), FVector::UpVector);
	WrongInstanceHit.Item = 7;
	WrongInstanceContext.AddHitResult(WrongInstanceHit, true);
	WrongInstancePayload.ContextHandle = MoveTemp(WrongInstanceContext);
	Validate(TEXT("A fabricated instance index is rejected"), WrongInstancePayload, false);

	AbilitySystem->ClearAbility(ExecuteHandle);
	Validate(TEXT("An option whose execution ability is no longer granted is rejected"), ValidPayload, false);
	const FGameplayAbilitySpecHandle ReplacementHandle = AbilitySystem->GiveAbility(
		FGameplayAbilitySpec(URpgGameplayAbility_ExecuteInteraction::StaticClass(), 1));
	if (!TestTrue(TEXT("Execution ability can be restored for spatial validation"), ReplacementHandle.IsValid()))
	{
		return false;
	}

	DoorOwner->SetActorLocation(FVector(300.0, 0.0, 0.0));
	FInteractionOption BoundaryOption;
	FGameplayEventData BoundaryPayload;
	if (!TestTrue(TEXT("Interaction-range boundary option gathers"), GatherSingleOption(Door, Requester, BoundaryOption)) ||
		!TestTrue(
			TEXT("Interaction-range boundary payload builds"),
			UInteractionStatics::BuildInteractionEventData(BoundaryOption, Requester, AbilitySystem, BoundaryPayload)))
	{
		return false;
	}
	Validate(TEXT("Exact interaction-range boundary validates"), BoundaryPayload, true);

	DoorOwner->SetActorLocation(FVector(300.01, 0.0, 0.0));
	FInteractionOption OutOfRangeOption;
	FGameplayEventData OutOfRangePayload;
	if (!TestTrue(TEXT("Out-of-range option remains gatherable for a blocked prompt"), GatherSingleOption(Door, Requester, OutOfRangeOption)) ||
		!TestTrue(
			TEXT("Out-of-range payload builds for server rejection"),
			UInteractionStatics::BuildInteractionEventData(OutOfRangeOption, Requester, AbilitySystem, OutOfRangePayload)))
	{
		return false;
	}
	Validate(TEXT("Beyond the interaction range is rejected"), OutOfRangePayload, false);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
