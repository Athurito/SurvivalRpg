#include "Portals/RpgPortalTravelComponent.h"

#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Portals/RpgPortalActor.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPortalTravelComponent)

DEFINE_LOG_CATEGORY_STATIC(LogRpgPortalTravel, Log, All);

namespace
{
	constexpr float PortalTravelVisibilityRetryDelay = 0.1f;
	constexpr float PortalTravelVisibilityTimeout = 8.0f;
	constexpr float PortalTravelDeferredUnloadRetryDelay = 0.05f;
	constexpr float PortalTravelDeferredUnloadTimeout = 3.0f;
	constexpr float PortalTravelResumeRetryDelay = 0.25f;
	constexpr int32 PortalTravelResumeMaxAttempts = 24;

	FString PortalTravelStateToString(ERpgPortalTravelState State)
	{
		const UEnum* Enum = StaticEnum<ERpgPortalTravelState>();
		return Enum ? Enum->GetNameStringByValue(static_cast<int64>(State)) : FString::Printf(TEXT("State_%d"), static_cast<int32>(State));
	}
}

URpgPortalTravelComponent::URpgPortalTravelComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoRegister = true;
	bAutoActivate = true;
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

URpgPortalTravelComponent* URpgPortalTravelComponent::FindPortalTravelComponent(AController* Controller)
{
	return Controller ? Controller->FindComponentByClass<URpgPortalTravelComponent>() : nullptr;
}

void URpgPortalTravelComponent::BeginPlay()
{
	Super::BeginPlay();
	ScheduleServerResumeCheck();
}

void URpgPortalTravelComponent::ReceivedPlayer()
{
	Super::ReceivedPlayer();
	ScheduleServerResumeCheck();
}

void URpgPortalTravelComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NetVisibilityRetryTimerHandle);
		World->GetTimerManager().ClearTimer(ClientDeferredUnloadTimerHandle);
		World->GetTimerManager().ClearTimer(ServerResumeCheckTimerHandle);
	}

	UnloadClientDungeonLevelInstance();
	ResetRequestData();
	Super::EndPlay(EndPlayReason);
}

bool URpgPortalTravelComponent::BeginPortalTravel(
	ARpgPortalActor* Portal,
	AActor* TravelActor,
	const FSoftObjectPath& DungeonLevelPath,
	const FTransform& LevelInstanceTransform,
	const FString& LevelInstanceName,
	FName ExpectedPackageName,
	int32 RequestId)
{
	APlayerController* PlayerController = GetOwningPlayerController();
	if (!PlayerController || !PlayerController->HasAuthority() || !Portal || !TravelActor || DungeonLevelPath.IsNull() || LevelInstanceName.IsEmpty() || ExpectedPackageName.IsNone() || RequestId <= 0)
	{
		LogInvalidTransition(TEXT("BeginPortalTravel received invalid request data."), Portal, RequestId);
		return false;
	}

	if (TravelState != ERpgPortalTravelState::Idle
		&& TravelState != ERpgPortalTravelState::InsideDungeon
		&& TravelState != ERpgPortalTravelState::Cancelled
		&& TravelState != ERpgPortalTravelState::Failed)
	{
		CancelPortalTravel(ActivePortal, ActiveRequestId, TEXT("Replaced by a newer portal travel request."));
	}

	ActivePortal = Portal;
	ActiveTravelActor = TravelActor;
	ActiveDungeonLevelPath = DungeonLevelPath;
	ActiveLevelInstanceTransform = LevelInstanceTransform;
	ActiveLevelInstanceName = LevelInstanceName;
	ActiveExpectedPackageName = ExpectedPackageName;
	ActiveRequestId = RequestId;

	SetTravelState(ERpgPortalTravelState::ServerRequestTravel);

	if (!ShouldUseClientLoadHandshake())
	{
		SetTravelState(ERpgPortalTravelState::ReadyToTeleport);
		return ActivePortal && ActivePortal->CompletePortalTravel(this, ActiveRequestId);
	}

	ClientLoadPortalDungeon(Portal, RequestId, DungeonLevelPath, LevelInstanceTransform, LevelInstanceName, ExpectedPackageName);
	return true;
}

void URpgPortalTravelComponent::CancelPortalTravel(ARpgPortalActor* Portal, int32 RequestId, const TCHAR* Reason)
{
	if (!IsActiveRequest(Portal, RequestId))
	{
		LogInvalidTransition(Reason ? Reason : TEXT("CancelPortalTravel stale request."), Portal, RequestId);
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NetVisibilityRetryTimerHandle);
		World->GetTimerManager().ClearTimer(ClientDeferredUnloadTimerHandle);
	}

	UE_LOG(LogRpgPortalTravel, Log, TEXT("Portal travel cancelled. RequestId=%d Controller=%s Pawn=%s Portal=%s InstanceName=%s ExpectedPackageName=%s Reason=%s"),
		ActiveRequestId,
		*GetNameSafe(GetOwner()),
		*GetNameSafe(ActiveTravelActor),
		*GetNameSafe(ActivePortal),
		*ActiveLevelInstanceName,
		*ActiveExpectedPackageName.ToString(),
		Reason ? Reason : TEXT("None"));

	SetTravelState(ERpgPortalTravelState::Cancelled);
	ClientUnloadPortalDungeon(ActivePortal, ActiveLevelInstanceName, ActiveRequestId, ERpgPortalTravelState::Cancelled);
	ResetRequestData();
}

void URpgPortalTravelComponent::FailPortalTravel(ARpgPortalActor* Portal, int32 RequestId, const TCHAR* Reason)
{
	if (!IsActiveRequest(Portal, RequestId))
	{
		LogInvalidTransition(Reason ? Reason : TEXT("FailPortalTravel stale request."), Portal, RequestId);
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NetVisibilityRetryTimerHandle);
		World->GetTimerManager().ClearTimer(ClientDeferredUnloadTimerHandle);
	}

	UE_LOG(LogRpgPortalTravel, Warning, TEXT("Portal travel failed. RequestId=%d Controller=%s Pawn=%s Portal=%s InstanceName=%s ExpectedPackageName=%s Reason=%s"),
		ActiveRequestId,
		*GetNameSafe(GetOwner()),
		*GetNameSafe(ActiveTravelActor),
		*GetNameSafe(ActivePortal),
		*ActiveLevelInstanceName,
		*ActiveExpectedPackageName.ToString(),
		Reason ? Reason : TEXT("None"));

	SetTravelState(ERpgPortalTravelState::Failed);
	ClientUnloadPortalDungeon(ActivePortal, ActiveLevelInstanceName, ActiveRequestId, ERpgPortalTravelState::Failed);
	ResetRequestData();
}

bool URpgPortalTravelComponent::MarkTeleporting(ARpgPortalActor* Portal, int32 RequestId)
{
	if (!IsActiveRequest(Portal, RequestId) || TravelState != ERpgPortalTravelState::ReadyToTeleport)
	{
		LogInvalidTransition(TEXT("MarkTeleporting requires ReadyToTeleport."), Portal, RequestId);
		return false;
	}

	SetTravelState(ERpgPortalTravelState::Teleporting);
	return true;
}

bool URpgPortalTravelComponent::MarkInsideDungeon(ARpgPortalActor* Portal, int32 RequestId)
{
	if (!IsActiveRequest(Portal, RequestId) || TravelState != ERpgPortalTravelState::Teleporting)
	{
		LogInvalidTransition(TEXT("MarkInsideDungeon requires Teleporting."), Portal, RequestId);
		return false;
	}

	SetTravelState(ERpgPortalTravelState::InsideDungeon);
	return true;
}

void URpgPortalTravelComponent::BeginPortalExit(ARpgPortalActor* Portal)
{
	if (Portal && ActivePortal && Portal != ActivePortal)
	{
		LogInvalidTransition(TEXT("BeginPortalExit received a different portal."), Portal, ActiveRequestId);
		return;
	}

	if (TravelState != ERpgPortalTravelState::InsideDungeon && TravelState != ERpgPortalTravelState::Teleporting)
	{
		LogInvalidTransition(TEXT("BeginPortalExit requires an inside dungeon request."), Portal, ActiveRequestId);
	}

	const FString InstanceNameToUnload = ActiveLevelInstanceName;
	const int32 RequestIdToUnload = ActiveRequestId;
	ARpgPortalActor* PortalToUnload = ActivePortal ? ActivePortal.Get() : Portal;

	SetTravelState(ERpgPortalTravelState::Exiting);
	ClientUnloadPortalDungeon(PortalToUnload, InstanceNameToUnload, RequestIdToUnload, ERpgPortalTravelState::Idle);
	ResetRequestData();
	SetTravelState(ERpgPortalTravelState::Idle);
}

AActor* URpgPortalTravelComponent::GetTravelActorForRequest(ARpgPortalActor* Portal, int32 RequestId) const
{
	return IsActiveRequest(Portal, RequestId) ? ActiveTravelActor.Get() : nullptr;
}

bool URpgPortalTravelComponent::IsActiveRequest(ARpgPortalActor* Portal, int32 RequestId) const
{
	return Portal != nullptr && ActivePortal == Portal && ActiveRequestId == RequestId && RequestId > 0;
}

void URpgPortalTravelComponent::ClientLoadPortalDungeon_Implementation(
	ARpgPortalActor* Portal,
	int32 RequestId,
	FSoftObjectPath DungeonLevelPath,
	FTransform LevelInstanceTransform,
	const FString& LevelInstanceName,
	FName ExpectedPackageName)
{
	if (!Portal || DungeonLevelPath.IsNull() || LevelInstanceName.IsEmpty() || RequestId <= 0)
	{
		UE_LOG(LogRpgPortalTravel, Warning, TEXT("ClientLoadPortalDungeon rejected invalid data. RequestId=%d Controller=%s Portal=%s InstanceName=%s ExpectedPackageName=%s"),
			RequestId,
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Portal),
			*LevelInstanceName,
			*ExpectedPackageName.ToString());
		ServerNotifyPortalDungeonTravelFailed(Portal, RequestId, LevelInstanceName);
		return;
	}

	if (TravelState != ERpgPortalTravelState::Idle
		&& TravelState != ERpgPortalTravelState::Cancelled
		&& TravelState != ERpgPortalTravelState::Failed)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ClientDeferredUnloadTimerHandle);
		}
		UnloadClientDungeonLevelInstance();
	}

	ActivePortal = Portal;
	ActiveTravelActor = nullptr;
	ActiveDungeonLevelPath = DungeonLevelPath;
	ActiveLevelInstanceTransform = LevelInstanceTransform;
	ActiveLevelInstanceName = LevelInstanceName;
	ActiveExpectedPackageName = ExpectedPackageName;
	ActiveRequestId = RequestId;

	SetTravelState(ERpgPortalTravelState::ClientLoadingLevel);

	if (!LoadClientDungeonLevelInstance())
	{
		SetTravelState(ERpgPortalTravelState::Failed);
		ServerNotifyPortalDungeonTravelFailed(Portal, RequestId, LevelInstanceName);
		ResetRequestData();
	}
}

void URpgPortalTravelComponent::ClientUnloadPortalDungeon_Implementation(ARpgPortalActor* Portal, const FString& LevelInstanceName, int32 RequestId, ERpgPortalTravelState TerminalState)
{
	if (Portal && ActivePortal && Portal != ActivePortal)
	{
		LogInvalidTransition(TEXT("ClientUnloadPortalDungeon received a different portal."), Portal, RequestId);
		return;
	}

	if (!LevelInstanceName.IsEmpty() && !ActiveLevelInstanceName.IsEmpty() && LevelInstanceName != ActiveLevelInstanceName)
	{
		LogInvalidTransition(TEXT("ClientUnloadPortalDungeon received a different instance name."), Portal, RequestId);
		return;
	}

	if (TerminalState == ERpgPortalTravelState::Idle && LocalDungeonLevelStreaming && !IsClientSafeToUnloadDungeonLevelInstance())
	{
		StartClientDeferredUnloadAfterExit(Portal, LevelInstanceName, RequestId);
		return;
	}

	UnloadClientDungeonLevelInstance();

	if (TerminalState == ERpgPortalTravelState::Cancelled || TerminalState == ERpgPortalTravelState::Failed)
	{
		SetTravelState(TerminalState);
	}

	ResetRequestData();

	if (TerminalState == ERpgPortalTravelState::Idle)
	{
		SetTravelState(ERpgPortalTravelState::Idle);
	}
}

void URpgPortalTravelComponent::ServerNotifyPortalDungeonLevelShown_Implementation(ARpgPortalActor* Portal, int32 RequestId, const FString& LevelInstanceName, FName ExpectedPackageName)
{
	if (!IsActiveRequest(Portal, RequestId) || LevelInstanceName != ActiveLevelInstanceName || ExpectedPackageName != ActiveExpectedPackageName)
	{
		LogInvalidTransition(TEXT("ServerNotifyPortalDungeonLevelShown stale or mismatched request."), Portal, RequestId);
		return;
	}

	SetTravelState(ERpgPortalTravelState::ClientLevelShown);
	StartServerVisibilityWait();
}

void URpgPortalTravelComponent::ServerNotifyPortalDungeonTravelFailed_Implementation(ARpgPortalActor* Portal, int32 RequestId, const FString& LevelInstanceName)
{
	if (!IsActiveRequest(Portal, RequestId) || LevelInstanceName != ActiveLevelInstanceName)
	{
		LogInvalidTransition(TEXT("ServerNotifyPortalDungeonTravelFailed stale or mismatched request."), Portal, RequestId);
		return;
	}

	if (ActivePortal)
	{
		ActivePortal->HandlePortalTravelFailed(this, RequestId);
	}
	FailPortalTravel(Portal, RequestId, TEXT("Owning client failed to load/show the dungeon level."));
}

void URpgPortalTravelComponent::HandleClientDungeonLevelShown()
{
	if (TravelState != ERpgPortalTravelState::ClientLoadingLevel || !ActivePortal || ActiveRequestId <= 0)
	{
		LogInvalidTransition(TEXT("HandleClientDungeonLevelShown without an active client loading request."), ActivePortal, ActiveRequestId);
		return;
	}

	SetTravelState(ERpgPortalTravelState::ClientLevelShown);
	ServerNotifyPortalDungeonLevelShown(ActivePortal, ActiveRequestId, ActiveLevelInstanceName, ActiveExpectedPackageName);
}

void URpgPortalTravelComponent::StartServerVisibilityWait()
{
	if (TravelState != ERpgPortalTravelState::ClientLevelShown)
	{
		LogInvalidTransition(TEXT("StartServerVisibilityWait requires ClientLevelShown."), ActivePortal, ActiveRequestId);
		return;
	}

	SetTravelState(ERpgPortalTravelState::ServerWaitingForNetVisibility);
	ServerVisibilityWaitStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	TryCompleteServerVisibilityWait();
}

void URpgPortalTravelComponent::TryCompleteServerVisibilityWait()
{
	if (TravelState != ERpgPortalTravelState::ServerWaitingForNetVisibility)
	{
		LogInvalidTransition(TEXT("TryCompleteServerVisibilityWait requires ServerWaitingForNetVisibility."), ActivePortal, ActiveRequestId);
		return;
	}

	if (!IsExpectedPackageVisibleToOwningClient())
	{
		if (UWorld* World = GetWorld())
		{
			if (ServerVisibilityWaitStartTime > 0.0 && World->GetTimeSeconds() - ServerVisibilityWaitStartTime >= PortalTravelVisibilityTimeout)
			{
				FailPortalTravel(ActivePortal, ActiveRequestId, TEXT("Timed out waiting for NetConnection ClientVisibleLevelNames."));
				return;
			}

			World->GetTimerManager().SetTimer(NetVisibilityRetryTimerHandle, this, &ThisClass::TryCompleteServerVisibilityWait, PortalTravelVisibilityRetryDelay, false);
		}
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NetVisibilityRetryTimerHandle);
	}

	SetTravelState(ERpgPortalTravelState::ReadyToTeleport);
	if (!ActivePortal || !ActivePortal->CompletePortalTravel(this, ActiveRequestId))
	{
		FailPortalTravel(ActivePortal, ActiveRequestId, TEXT("Portal rejected ReadyToTeleport request."));
	}
}

bool URpgPortalTravelComponent::IsExpectedPackageVisibleToOwningClient() const
{
	APlayerController* PlayerController = GetOwningPlayerController();
	if (!PlayerController)
	{
		return false;
	}

	UNetConnection* NetConnection = PlayerController->GetNetConnection();
	if (!NetConnection)
	{
		return PlayerController->HasAuthority() && PlayerController->IsLocalController();
	}

	return NetConnection->ClientVisibleLevelNames.Contains(ActiveExpectedPackageName);
}

bool URpgPortalTravelComponent::ShouldUseClientLoadHandshake() const
{
	APlayerController* PlayerController = GetOwningPlayerController();
	if (!PlayerController || !PlayerController->HasAuthority())
	{
		return false;
	}

	return !PlayerController->IsLocalController() || PlayerController->GetNetConnection() != nullptr;
}

bool URpgPortalTravelComponent::LoadClientDungeonLevelInstance()
{
	UWorld* World = GetWorld();
	if (!World || ActiveDungeonLevelPath.IsNull())
	{
		return false;
	}

	if (LocalDungeonLevelStreaming)
	{
		if (UWorld* TimerWorld = GetWorld())
		{
			TimerWorld->GetTimerManager().ClearTimer(ClientDeferredUnloadTimerHandle);
		}
		ResetPendingClientUnloadData();
		LocalDungeonLevelStreaming->SetShouldBeLoaded(true);
		LocalDungeonLevelStreaming->SetShouldBeVisible(true);
		LocalDungeonLevelStreaming->SetIsRequestingUnloadAndRemoval(false);
		LocalDungeonLevelStreaming->OnLevelShown.RemoveDynamic(this, &ThisClass::HandleClientDungeonLevelShown);
		LocalDungeonLevelStreaming->OnLevelShown.AddDynamic(this, &ThisClass::HandleClientDungeonLevelShown);

		if (const ULevel* LoadedLevel = LocalDungeonLevelStreaming->GetLoadedLevel())
		{
			if (LoadedLevel->bIsVisible)
			{
				HandleClientDungeonLevelShown();
			}
		}

		return true;
	}

	const TSoftObjectPtr<UWorld> DungeonLevel(ActiveDungeonLevelPath);
	bool bLevelLoaded = false;
	ULevelStreamingDynamic::FLoadLevelInstanceParams LoadParams(World, DungeonLevel.GetLongPackageName(), ActiveLevelInstanceTransform);
	LoadParams.OptionalLevelNameOverride = &ActiveLevelInstanceName;
	LoadParams.bAllowReuseExitingLevelStreaming = true;
	LoadParams.bInitiallyVisible = true;

	LocalDungeonLevelStreaming = ULevelStreamingDynamic::LoadLevelInstance(LoadParams, bLevelLoaded);

	if (!bLevelLoaded || !LocalDungeonLevelStreaming)
	{
		UE_LOG(LogRpgPortalTravel, Warning, TEXT("Client failed to stream portal dungeon. RequestId=%d Controller=%s Portal=%s InstanceName=%s Level=%s"),
			ActiveRequestId,
			*GetNameSafe(GetOwner()),
			*GetNameSafe(ActivePortal),
			*ActiveLevelInstanceName,
			*ActiveDungeonLevelPath.ToString());
		LocalDungeonLevelStreaming = nullptr;
		return false;
	}

	LocalDungeonLevelStreaming->SetIsRequestingUnloadAndRemoval(false);
	LocalDungeonLevelStreaming->OnLevelShown.RemoveDynamic(this, &ThisClass::HandleClientDungeonLevelShown);
	LocalDungeonLevelStreaming->OnLevelShown.AddDynamic(this, &ThisClass::HandleClientDungeonLevelShown);

	if (const ULevel* LoadedLevel = LocalDungeonLevelStreaming->GetLoadedLevel())
	{
		if (LoadedLevel->bIsVisible)
		{
			HandleClientDungeonLevelShown();
		}
	}

	return true;
}

void URpgPortalTravelComponent::UnloadClientDungeonLevelInstance()
{
	if (!LocalDungeonLevelStreaming)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ClientDeferredUnloadTimerHandle);
	}
	LocalDungeonLevelStreaming->OnLevelShown.RemoveDynamic(this, &ThisClass::HandleClientDungeonLevelShown);
	LocalDungeonLevelStreaming->SetShouldBeVisible(false);
	LocalDungeonLevelStreaming->SetShouldBeLoaded(false);
	LocalDungeonLevelStreaming->SetIsRequestingUnloadAndRemoval(true);
	LocalDungeonLevelStreaming = nullptr;
	ResetPendingClientUnloadData();
}

void URpgPortalTravelComponent::StartClientDeferredUnloadAfterExit(ARpgPortalActor* Portal, const FString& LevelInstanceName, int32 RequestId)
{
	PendingUnloadPortal = Portal;
	PendingUnloadLevelInstanceName = LevelInstanceName;
	PendingUnloadRequestId = RequestId;
	ClientDeferredUnloadStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	SetTravelState(ERpgPortalTravelState::Exiting);
	TryClientDeferredUnloadAfterExit();
}

void URpgPortalTravelComponent::TryClientDeferredUnloadAfterExit()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const bool bTimedOut = ClientDeferredUnloadStartTime > 0.0
		&& World->GetTimeSeconds() - ClientDeferredUnloadStartTime >= PortalTravelDeferredUnloadTimeout;

	if (!LocalDungeonLevelStreaming || IsClientSafeToUnloadDungeonLevelInstance() || bTimedOut)
	{
		if (bTimedOut && LocalDungeonLevelStreaming)
		{
			UE_LOG(LogRpgPortalTravel, Warning, TEXT("Portal travel deferred client unload timed out. RequestId=%d Controller=%s Portal=%s InstanceName=%s"),
				PendingUnloadRequestId,
				*GetNameSafe(GetOwner()),
				*GetNameSafe(PendingUnloadPortal),
				*PendingUnloadLevelInstanceName);
		}

		World->GetTimerManager().ClearTimer(ClientDeferredUnloadTimerHandle);
		UnloadClientDungeonLevelInstance();
		ResetRequestData();
		SetTravelState(ERpgPortalTravelState::Idle);
		return;
	}

	World->GetTimerManager().SetTimer(ClientDeferredUnloadTimerHandle, this, &ThisClass::TryClientDeferredUnloadAfterExit, PortalTravelDeferredUnloadRetryDelay, false);
}

bool URpgPortalTravelComponent::IsClientSafeToUnloadDungeonLevelInstance() const
{
	if (!LocalDungeonLevelStreaming)
	{
		return true;
	}

	const ULevel* LoadedDungeonLevel = LocalDungeonLevelStreaming->GetLoadedLevel();
	if (!LoadedDungeonLevel)
	{
		return true;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	for (TActorIterator<ACharacter> CharacterIt(World); CharacterIt; ++CharacterIt)
	{
		const ACharacter* Character = *CharacterIt;
		if (!Character)
		{
			continue;
		}

		if (IsObjectInLocalDungeonLevel(Character->GetMovementBase())
			|| IsObjectInLocalDungeonLevel(Character->GetBasedMovement().MovementBase)
			|| IsObjectInLocalDungeonLevel(Character->GetReplicatedBasedMovement().MovementBase))
		{
			return false;
		}

		if (const UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement())
		{
			if (IsObjectInLocalDungeonLevel(MovementComponent->CurrentFloor.HitResult.GetComponent()))
			{
				return false;
			}
		}
	}

	return true;
}

bool URpgPortalTravelComponent::IsObjectInLocalDungeonLevel(const UObject* Object) const
{
	if (!Object || !LocalDungeonLevelStreaming)
	{
		return false;
	}

	const ULevel* LoadedDungeonLevel = LocalDungeonLevelStreaming->GetLoadedLevel();
	if (!LoadedDungeonLevel)
	{
		return false;
	}

	if (const AActor* Actor = Cast<AActor>(Object))
	{
		return Actor->GetLevel() == LoadedDungeonLevel;
	}

	if (const UActorComponent* ActorComponent = Cast<UActorComponent>(Object))
	{
		const AActor* Owner = ActorComponent->GetOwner();
		return Owner && Owner->GetLevel() == LoadedDungeonLevel;
	}

	return false;
}

void URpgPortalTravelComponent::ScheduleServerResumeCheck()
{
	APlayerController* PlayerController = GetOwningPlayerController();
	if (!PlayerController || !PlayerController->HasAuthority())
	{
		return;
	}

	if (TravelState != ERpgPortalTravelState::Idle
		&& TravelState != ERpgPortalTravelState::Cancelled
		&& TravelState != ERpgPortalTravelState::Failed)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || World->GetTimerManager().IsTimerActive(ServerResumeCheckTimerHandle))
	{
		return;
	}

	ServerResumeCheckAttempts = 0;
	World->GetTimerManager().SetTimer(ServerResumeCheckTimerHandle, this, &ThisClass::TryRestorePortalResumeAfterLogin, PortalTravelResumeRetryDelay, false);
}

void URpgPortalTravelComponent::TryRestorePortalResumeAfterLogin()
{
	APlayerController* PlayerController = GetOwningPlayerController();
	UWorld* World = GetWorld();
	if (!PlayerController || !PlayerController->HasAuthority() || !World)
	{
		return;
	}

	if (TravelState != ERpgPortalTravelState::Idle
		&& TravelState != ERpgPortalTravelState::Cancelled
		&& TravelState != ERpgPortalTravelState::Failed)
	{
		return;
	}

	++ServerResumeCheckAttempts;

	if (!PlayerController->GetPawn())
	{
		if (ServerResumeCheckAttempts < PortalTravelResumeMaxAttempts)
		{
			World->GetTimerManager().SetTimer(ServerResumeCheckTimerHandle, this, &ThisClass::TryRestorePortalResumeAfterLogin, PortalTravelResumeRetryDelay, false);
		}
		return;
	}

	for (TActorIterator<ARpgPortalActor> PortalIt(World); PortalIt; ++PortalIt)
	{
		ARpgPortalActor* Portal = *PortalIt;
		if (Portal && Portal->TryRestoreReconnectController(PlayerController))
		{
			World->GetTimerManager().ClearTimer(ServerResumeCheckTimerHandle);
			return;
		}
	}

	if (ServerResumeCheckAttempts < PortalTravelResumeMaxAttempts)
	{
		World->GetTimerManager().SetTimer(ServerResumeCheckTimerHandle, this, &ThisClass::TryRestorePortalResumeAfterLogin, PortalTravelResumeRetryDelay, false);
	}
}

void URpgPortalTravelComponent::SetTravelState(ERpgPortalTravelState NewState)
{
	if (TravelState == NewState)
	{
		return;
	}

	TravelState = NewState;
	UE_LOG(LogRpgPortalTravel, Log, TEXT("Portal travel state changed. RequestId=%d Controller=%s Pawn=%s Portal=%s InstanceName=%s ExpectedPackageName=%s State=%s"),
		ActiveRequestId,
		*GetNameSafe(GetOwner()),
		*GetNameSafe(ActiveTravelActor),
		*GetNameSafe(ActivePortal),
		*ActiveLevelInstanceName,
		*ActiveExpectedPackageName.ToString(),
		*PortalTravelStateToString(TravelState));
}

void URpgPortalTravelComponent::ResetRequestData()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NetVisibilityRetryTimerHandle);
		World->GetTimerManager().ClearTimer(ClientDeferredUnloadTimerHandle);
	}

	ActivePortal = nullptr;
	ActiveTravelActor = nullptr;
	ActiveDungeonLevelPath.Reset();
	ActiveLevelInstanceTransform = FTransform::Identity;
	ActiveLevelInstanceName.Reset();
	ActiveExpectedPackageName = NAME_None;
	ActiveRequestId = 0;
	ServerVisibilityWaitStartTime = 0.0;
	ResetPendingClientUnloadData();
}

void URpgPortalTravelComponent::ResetPendingClientUnloadData()
{
	PendingUnloadPortal = nullptr;
	PendingUnloadLevelInstanceName.Reset();
	PendingUnloadRequestId = 0;
	ClientDeferredUnloadStartTime = 0.0;
}

APlayerController* URpgPortalTravelComponent::GetOwningPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

void URpgPortalTravelComponent::LogInvalidTransition(const TCHAR* Reason, ARpgPortalActor* Portal, int32 RequestId) const
{
	UE_LOG(LogRpgPortalTravel, Warning, TEXT("Portal travel ignored transition. RequestId=%d Controller=%s Pawn=%s Portal=%s InstanceName=%s ExpectedPackageName=%s State=%s Reason=%s RequestedPortal=%s RequestedRequestId=%d"),
		ActiveRequestId,
		*GetNameSafe(GetOwner()),
		*GetNameSafe(ActiveTravelActor),
		*GetNameSafe(ActivePortal),
		*ActiveLevelInstanceName,
		*ActiveExpectedPackageName.ToString(),
		*PortalTravelStateToString(TravelState),
		Reason ? Reason : TEXT("None"),
		*GetNameSafe(Portal),
		RequestId);
}
