#include "RpgInventoryUseRequestAutomationTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "RpgInventoryAutomationTestTypes.h"
#include "RpgInventoryContainerActor.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"
#include "RpgInventoryUiActionComponent.h"
#include "RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"

namespace RpgInventoryUseRequestAutomationTests
{
	class FScopedUseRequestWorld
	{
	public:
		FScopedUseRequestWorld()
		{
			GameInstance = NewObject<UGameInstance>(
				GEngine,
				NAME_None,
				RF_Transient);
			if (!GameInstance)
			{
				return;
			}

			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}

		~FScopedUseRequestWorld()
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

		bool IsValid() const
		{
			return GameInstance != nullptr && World != nullptr;
		}

		UWorld* GetWorld() const
		{
			return World;
		}

	private:
		TObjectPtr<UGameInstance> GameInstance;
		TObjectPtr<UWorld> World;
	};

	struct FScopedAbilityStateReset
	{
		FScopedAbilityStateReset()
		{
			URpgInventoryUseRequestAutomationAbility::ResetTestState();
		}

		~FScopedAbilityStateReset()
		{
			URpgInventoryUseRequestAutomationAbility::ResetTestState();
		}
	};

	FRpgInventoryGridPlacement MakePocketPlacement(int32 X, int32 Y)
	{
		FRpgInventoryGridPlacement Placement;
		Placement.SetContainerHandle(
			FRpgInventoryContainerHandle::MakeRoot(
				URpgPlayerInventoryLayoutComponent::PocketsGroupId));
		Placement.X = X;
		Placement.Y = Y;
		return Placement;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryUseRequestExactlyOnceEndToEndTest,
	"SurvivalRpg.Inventory.UseRequest.ExactlyOnceEndToEnd",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryUseRequestExactlyOnceEndToEndTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgInventoryUseRequestAutomationTests;

	FScopedAbilityStateReset AbilityStateReset;
	FScopedUseRequestWorld TestWorld;
	if (!TestTrue(
			TEXT("An isolated item-use integration world exists"),
			TestWorld.IsValid()))
	{
		return false;
	}

	UWorld* World = TestWorld.GetWorld();
	FActorSpawnParameters ControllerSpawnParameters;
	ControllerSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerController::StaticClass(),
		TEXT("UseExactlyOnceController"));
	ControllerSpawnParameters.ObjectFlags = RF_Transient;
	ControllerSpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgInventoryAutomationTestPlayerController* Controller =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(
			ControllerSpawnParameters);

	FActorSpawnParameters PlayerStateSpawnParameters;
	PlayerStateSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerState::StaticClass(),
		TEXT("UseExactlyOncePlayerState"));
	PlayerStateSpawnParameters.ObjectFlags = RF_Transient;
	PlayerStateSpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerState>(
			PlayerStateSpawnParameters);

	FActorSpawnParameters PawnSpawnParameters;
	PawnSpawnParameters.Name = MakeUniqueObjectName(
		World,
		APawn::StaticClass(),
		TEXT("UseExactlyOncePawn"));
	PawnSpawnParameters.ObjectFlags = RF_Transient;
	PawnSpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APawn* Pawn = World->SpawnActor<APawn>(PawnSpawnParameters);
	if (!TestNotNull(TEXT("The authority controller exists"), Controller) ||
		!TestNotNull(TEXT("The authority player state exists"), PlayerState) ||
		!TestNotNull(TEXT("The authority avatar exists"), Pawn))
	{
		return false;
	}

	Controller->SetPlayerState(PlayerState);
	PlayerState->SetOwner(Controller);
	Controller->Possess(Pawn);

	URpgInventoryUiActionComponent* ActionComponent =
		Controller->GetInventoryUiActionComponent();
	URpgInventoryManagerComponent* Inventory =
		PlayerState->GetInventoryManagerComponent();
	URpgAbilitySystemComponent* AbilitySystem =
		PlayerState->GetRpgAbilitySystemComponent();
	if (!TestTrue(
			TEXT("The request fixture executes on authority"),
			Controller->HasAuthority() &&
				PlayerState->HasAuthority() && Pawn->HasAuthority()) ||
		!TestNotNull(
			TEXT("The controller owns the real UI action component"),
			ActionComponent) ||
		!TestNotNull(
			TEXT("The player state owns the real inventory"),
			Inventory) ||
		!TestNotNull(
			TEXT("The player state owns the real ASC"),
			AbilitySystem))
	{
		return false;
	}

	AbilitySystem->InitAbilityActorInfo(PlayerState, Pawn);
	TestEqual(
		TEXT("The action gateway remains controller-owned"),
		ActionComponent->GetOwner(),
		static_cast<AActor*>(Controller));
	TestEqual(
		TEXT("The authoritative inventory remains player-state-owned"),
		Inventory->GetOwner(),
		static_cast<AActor*>(PlayerState));
	TestEqual(
		TEXT("The ASC owner actor remains the player state"),
		AbilitySystem->GetOwnerActor(),
		static_cast<AActor*>(PlayerState));
	TestEqual(
		TEXT("The ASC avatar actor is the possessed pawn"),
		AbilitySystem->GetAvatarActor(),
		static_cast<AActor*>(Pawn));

	constexpr int32 InitialStackCount = 4;
	constexpr int32 RequestedUseCount = 2;
	URpgInventoryItemInstance* Item =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryUseRequestAutomationItemDefinition::
				StaticClass(),
			InitialStackCount,
			MakePocketPlacement(0, 0));
	if (!TestNotNull(
			TEXT("A stackable usable item exists in the player inventory"),
			Item))
	{
		return false;
	}

	FRpgInventoryUseRequest Request;
	Request.RequestId = FGuid::NewGuid();
	Request.ItemId = Item->GetItemId();
	Request.UseCount = RequestedUseCount;
	URpgInventoryUseRequestAutomationAbility::ConfigureReentrantRequest(
		ActionComponent,
		Inventory,
		Request);

	const int32 InitialInventoryRevision =
		Inventory->GetInventoryRevision();
	const uint64 InitialMutationEpoch = Inventory->GetMutationEpoch();
	TArray<FRpgInventoryActionFeedbackMessage> FeedbackMessages;
	bool bIssuedCompletedRetryFromFirstFeedback = false;
	bool bCompletedRetryReturnedSynchronously = false;
	int32 FeedbackCountWhenCompletedRetryReturned = INDEX_NONE;
	int32 ActivationCountBeforeCompletedRetry = INDEX_NONE;
	int32 ActivationCountAfterCompletedRetry = INDEX_NONE;
	int32 StackCountBeforeCompletedRetry = INDEX_NONE;
	int32 StackCountAfterCompletedRetry = INDEX_NONE;
	int32 InventoryRevisionBeforeCompletedRetry = INDEX_NONE;
	int32 InventoryRevisionAfterCompletedRetry = INDEX_NONE;
	uint64 MutationEpochBeforeCompletedRetry = MAX_uint64;
	uint64 MutationEpochAfterCompletedRetry = MAX_uint64;

	UGameplayMessageSubsystem& MessageSubsystem =
		UGameplayMessageSubsystem::Get(World);
	FGameplayMessageListenerHandle FeedbackHandle =
		MessageSubsystem.RegisterListener<
			FRpgInventoryActionFeedbackMessage>(
			RpgGameplayTags::Rpg_Inventory_Message_ActionFeedback,
			[&](
				FGameplayTag Channel,
				const FRpgInventoryActionFeedbackMessage& Message)
			{
				if (Channel !=
						RpgGameplayTags::
							Rpg_Inventory_Message_ActionFeedback ||
					Message.ActionTag !=
						RpgGameplayTags::Rpg_Inventory_Action_Use ||
					Message.RequestId != Request.RequestId)
				{
					return;
				}

				FeedbackMessages.Add(Message);
				if (bIssuedCompletedRetryFromFirstFeedback)
				{
					return;
				}

				bIssuedCompletedRetryFromFirstFeedback = true;
				ActivationCountBeforeCompletedRetry =
					URpgInventoryUseRequestAutomationAbility::
						GetActivationCount();
				URpgInventoryItemInstance* CurrentItem =
					Inventory->FindItemById(Request.ItemId);
				StackCountBeforeCompletedRetry = CurrentItem
					? Inventory->GetItemStackCount(CurrentItem)
					: INDEX_NONE;
				InventoryRevisionBeforeCompletedRetry =
					Inventory->GetInventoryRevision();
				MutationEpochBeforeCompletedRetry =
					Inventory->GetMutationEpoch();

				ActionComponent->RequestUseInventoryItemById(
					Inventory,
					Request);
				bCompletedRetryReturnedSynchronously = true;
				FeedbackCountWhenCompletedRetryReturned =
					FeedbackMessages.Num();

				ActivationCountAfterCompletedRetry =
					URpgInventoryUseRequestAutomationAbility::
						GetActivationCount();
				CurrentItem = Inventory->FindItemById(Request.ItemId);
				StackCountAfterCompletedRetry = CurrentItem
					? Inventory->GetItemStackCount(CurrentItem)
					: INDEX_NONE;
				InventoryRevisionAfterCompletedRetry =
					Inventory->GetInventoryRevision();
				MutationEpochAfterCompletedRetry =
					Inventory->GetMutationEpoch();
			});

	ActionComponent->RequestUseInventoryItemById(Inventory, Request);
	FeedbackHandle.Unregister();

	TestTrue(
		TEXT("The GAS ability issued an identical retry while activation was in flight"),
		URpgInventoryUseRequestAutomationAbility::
			DidIssueReentrantRetry());
	TestEqual(
		TEXT("The in-flight retry cannot activate GAS a second time"),
		URpgInventoryUseRequestAutomationAbility::GetActivationCount(),
		1);
	TestEqual(
		TEXT("GAS receives the requested use count as event magnitude"),
		URpgInventoryUseRequestAutomationAbility::
			GetObservedEventMagnitude(),
		static_cast<float>(RequestedUseCount));
	TestEqual(
		TEXT("Consumption has not started before the reentrant in-flight retry"),
		URpgInventoryUseRequestAutomationAbility::
			GetStackCountBeforeReentrantRetry(),
		InitialStackCount);
	TestEqual(
		TEXT("The reentrant in-flight retry performs no consume side effect"),
		URpgInventoryUseRequestAutomationAbility::
			GetStackCountAfterReentrantRetry(),
		InitialStackCount);

	URpgInventoryItemInstance* RemainingItem =
		Inventory->FindItemById(Request.ItemId);
	if (!TestNotNull(
			TEXT("The partially consumed item keeps its stable identity"),
			RemainingItem))
	{
		return false;
	}
	TestEqual(
		TEXT("The accepted request consumes its authored quantity exactly once"),
		Inventory->GetItemStackCount(RemainingItem),
		InitialStackCount - RequestedUseCount);
	TestEqual(
		TEXT("Exactly one consume commit advances the inventory revision once"),
		Inventory->GetInventoryRevision(),
		InitialInventoryRevision + 1);
	TestEqual(
		TEXT("A normal consume does not change the restore mutation epoch"),
		Inventory->GetMutationEpoch(),
		InitialMutationEpoch);

	TestTrue(
		TEXT("The first feedback callback issued the completed retry"),
		bIssuedCompletedRetryFromFirstFeedback);
	TestTrue(
		TEXT("The completed retry replayed synchronously from the first feedback callback"),
		bCompletedRetryReturnedSynchronously);
	TestEqual(
		TEXT("The replay feedback arrives before the first feedback callback resumes"),
		FeedbackCountWhenCompletedRetryReturned,
		2);
	TestEqual(
		TEXT("Finalize-before-feedback produces the original result plus one nested replay"),
		FeedbackMessages.Num(),
		2);
	if (FeedbackMessages.Num() == 2)
	{
		const FRpgInventoryActionFeedbackMessage& OriginalFeedback =
			FeedbackMessages[0];
		const FRpgInventoryActionFeedbackMessage& ReplayFeedback =
			FeedbackMessages[1];
		TestEqual(
			TEXT("The original item-use request succeeds"),
			OriginalFeedback.Result,
			ERpgInventoryActionFeedbackResult::Success);
		TestEqual(
			TEXT("The completed retry replays the exact result"),
			ReplayFeedback.Result,
			OriginalFeedback.Result);
		TestEqual(
			TEXT("The completed retry replays the exact affected count"),
			ReplayFeedback.StackCount,
			OriginalFeedback.StackCount);
		TestEqual(
			TEXT("The successful feedback reports the derived consume quantity"),
			OriginalFeedback.StackCount,
			RequestedUseCount);
		TestEqual(
			TEXT("The replay keeps the caller correlation id"),
			ReplayFeedback.RequestId,
			OriginalFeedback.RequestId);
		TestTrue(
			TEXT("The replay keeps the stable item identity"),
			ReplayFeedback.ItemId == OriginalFeedback.ItemId &&
				ReplayFeedback.ItemId == Request.ItemId);
		TestEqual(
			TEXT("The replay keeps the original authorized item context"),
			ReplayFeedback.Item.Get(),
			OriginalFeedback.Item.Get());
		TestEqual(
			TEXT("The original feedback exposes the still-owned item context"),
			OriginalFeedback.Item.Get(),
			RemainingItem);
		TestEqual(
			TEXT("Both feedback messages remain addressed to the owning controller"),
			ReplayFeedback.Recipient.Get(),
			OriginalFeedback.Recipient.Get());
		TestEqual(
			TEXT("The feedback recipient is the authority-owned controller fixture"),
			OriginalFeedback.Recipient.Get(),
			static_cast<APlayerController*>(Controller));
	}

	TestEqual(
		TEXT("The completed replay performs no additional GAS activation"),
		ActivationCountAfterCompletedRetry,
		ActivationCountBeforeCompletedRetry);
	TestEqual(
		TEXT("The completed replay performs no additional consume"),
		StackCountAfterCompletedRetry,
		StackCountBeforeCompletedRetry);
	TestEqual(
		TEXT("The completed replay performs no normal inventory revision"),
		InventoryRevisionAfterCompletedRetry,
		InventoryRevisionBeforeCompletedRetry);
	TestEqual(
		TEXT("The completed replay performs no inventory mutation"),
		MutationEpochAfterCompletedRetry,
		MutationEpochBeforeCompletedRetry);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryUseRequestAuthorizationReplayRedactionTest,
	"SurvivalRpg.Inventory.UseRequest.AuthorizationReplayRedaction",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryUseRequestAuthorizationReplayRedactionTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgInventoryUseRequestAutomationTests;

	FScopedUseRequestWorld TestWorld;
	if (!TestTrue(
			TEXT("An isolated use-authorization world exists"),
			TestWorld.IsValid()))
	{
		return false;
	}

	UWorld* World = TestWorld.GetWorld();
	FActorSpawnParameters ControllerParameters;
	ControllerParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerController::StaticClass(),
		TEXT("UseAuthorizationController"));
	ControllerParameters.ObjectFlags = RF_Transient;
	ControllerParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgInventoryAutomationTestPlayerController* Controller =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(
			ControllerParameters);

	FActorSpawnParameters PlayerStateParameters;
	PlayerStateParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerState::StaticClass(),
		TEXT("UseAuthorizationPlayerState"));
	PlayerStateParameters.ObjectFlags = RF_Transient;
	PlayerStateParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerState>(
			PlayerStateParameters);

	FActorSpawnParameters PawnParameters;
	PawnParameters.Name = MakeUniqueObjectName(
		World,
		APawn::StaticClass(),
		TEXT("UseAuthorizationPawn"));
	PawnParameters.ObjectFlags = RF_Transient;
	PawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APawn* Pawn = World->SpawnActor<APawn>(PawnParameters);

	FActorSpawnParameters ContainerParameters;
	ContainerParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryContainerActor::StaticClass(),
		TEXT("UseAuthorizationContainer"));
	ContainerParameters.ObjectFlags = RF_Transient;
	ContainerParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgInventoryContainerActor* Container =
		World->SpawnActor<ARpgInventoryContainerActor>(
			ContainerParameters);
	if (!TestNotNull(TEXT("The authorization controller exists"), Controller) ||
		!TestNotNull(TEXT("The authorization player state exists"), PlayerState) ||
		!TestNotNull(TEXT("The authorization pawn exists"), Pawn) ||
		!TestNotNull(TEXT("The authorization container exists"), Container))
	{
		return false;
	}

	Controller->SetPlayerState(PlayerState);
	PlayerState->SetOwner(Controller);
	Controller->Possess(Pawn);
	Pawn->SetActorLocation(FVector::ZeroVector);
	Container->SetActorLocation(FVector::ZeroVector);

	URpgInventoryUiActionComponent* ActionComponent =
		Controller->GetInventoryUiActionComponent();
	URpgInventoryManagerComponent* Inventory =
		Container->GetInventoryManager();
	if (!TestNotNull(TEXT("The action gateway exists"), ActionComponent) ||
		!TestNotNull(TEXT("The foreign inventory exists"), Inventory) ||
		!TestTrue(
			TEXT("The nearby foreign inventory is initially accessible"),
			ActionComponent->CanAccessInventory(Inventory)))
	{
		return false;
	}

	FRpgInventoryGridPlacement Placement;
	Placement.SetContainerHandle(
		FRpgInventoryContainerHandle::MakeRoot(
			Inventory->GetDefaultContainerId()));
	Placement.X = 0;
	Placement.Y = 0;
	URpgInventoryItemInstance* Item =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			2,
			Placement);
	if (!TestNotNull(TEXT("The foreign use-test item exists"), Item))
	{
		return false;
	}

	FRpgInventoryUseRequest Request;
	Request.RequestId = FGuid::NewGuid();
	Request.ItemId = Item->GetItemId();
	Request.UseCount = 1;
	const int32 RevisionBeforeRequests = Inventory->GetInventoryRevision();

	TArray<FRpgInventoryActionFeedbackMessage> FeedbackMessages;
	UGameplayMessageSubsystem& MessageSubsystem =
		UGameplayMessageSubsystem::Get(World);
	FGameplayMessageListenerHandle FeedbackHandle =
		MessageSubsystem.RegisterListener<
			FRpgInventoryActionFeedbackMessage>(
			RpgGameplayTags::Rpg_Inventory_Message_ActionFeedback,
			[&FeedbackMessages, &Request](
				FGameplayTag,
				const FRpgInventoryActionFeedbackMessage& Message)
			{
				if (Message.RequestId == Request.RequestId)
				{
					FeedbackMessages.Add(Message);
				}
			});

	ActionComponent->RequestUseInventoryItemById(Inventory, Request);
	TestEqual(
		TEXT("The authorized request emits one result"),
		FeedbackMessages.Num(),
		1);
	if (FeedbackMessages.IsValidIndex(0))
	{
		TestEqual(
			TEXT("The configured item is not usable"),
			FeedbackMessages[0].Result,
			ERpgInventoryActionFeedbackResult::CannotUse);
		TestEqual(
			TEXT("Authorized feedback may include its owned item context"),
			FeedbackMessages[0].Item.Get(),
			Item);
	}

	Container->SetActorLocation(FVector(10000.0, 0.0, 0.0));
	TestFalse(
		TEXT("Moving the foreign inventory away revokes access"),
		ActionComponent->CanAccessInventory(Inventory));
	ActionComponent->RequestUseInventoryItemById(Inventory, Request);
	TestEqual(
		TEXT("The revoked retry emits one replay denial"),
		FeedbackMessages.Num(),
		2);
	if (FeedbackMessages.IsValidIndex(1))
	{
		TestEqual(
			TEXT("Use replay revalidates current access"),
			FeedbackMessages[1].Result,
			ERpgInventoryActionFeedbackResult::NoAccess);
		TestNull(
			TEXT("The revoked replay cannot expose its old item pointer"),
			FeedbackMessages[1].Item.Get());
		TestEqual(
			TEXT("The revoked replay retains stable ItemId correlation"),
			FeedbackMessages[1].ItemId,
			Request.ItemId);
	}

	Container->SetActorLocation(FVector::ZeroVector);
	TestTrue(
		TEXT("Returning the foreign inventory restores access"),
		ActionComponent->CanAccessInventory(Inventory));
	ActionComponent->RequestUseInventoryItemById(Inventory, Request);
	FeedbackHandle.Unregister();
	TestEqual(
		TEXT("The restored retry replays the cached denial once"),
		FeedbackMessages.Num(),
		3);
	if (FeedbackMessages.IsValidIndex(2))
	{
		TestEqual(
			TEXT("Restored access cannot revive the original use result"),
			FeedbackMessages[2].Result,
			ERpgInventoryActionFeedbackResult::NoAccess);
		TestNull(
			TEXT("The cached use denial remains pointer-redacted"),
			FeedbackMessages[2].Item.Get());
		TestEqual(
			TEXT("The cached use denial retains stable ItemId correlation"),
			FeedbackMessages[2].ItemId,
			Request.ItemId);
	}

	TestEqual(
		TEXT("Use replays do not mutate the foreign inventory"),
		Inventory->GetInventoryRevision(),
		RevisionBeforeRequests);
	TestEqual(
		TEXT("Use replays preserve the foreign stack"),
		Inventory->GetItemStackCount(Item),
		2);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
