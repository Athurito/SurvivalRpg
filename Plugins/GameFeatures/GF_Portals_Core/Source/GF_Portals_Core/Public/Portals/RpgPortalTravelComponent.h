#pragma once

#include "CoreMinimal.h"
#include "Components/ControllerComponent.h"
#include "UObject/SoftObjectPath.h"
#include "RpgPortalTravelComponent.generated.h"

class ARpgPortalActor;
class ULevelStreamingDynamic;

/**
 * Debuggable lifecycle for one player controller's portal-realm travel request.
 *
 * The server owns the authoritative request. The owning client loads the same
 * dynamic realm level instance locally, reports OnLevelShown, then the server
 * waits until Unreal's level-visibility RPC has reached the NetConnection before
 * the portal teleports the pawn into streamed-level collision.
 */
UENUM(BlueprintType)
enum class ERpgPortalTravelState : uint8
{
	Idle,
	ServerRequestTravel,
	ClientLoadingLevel,
	ClientLevelShown,
	ServerWaitingForNetVisibility,
	ReadyToTeleport,
	Teleporting,
	InsideRealm,
	Exiting,
	Cancelled,
	Failed
};

/**
 * GameFeature-owned portal travel handshaker added to RpgPlayerController.
 *
 * This component deliberately lives in GF_Portals_Core instead of the core
 * player controller so portal-specific streaming/RPC behavior can activate and
 * deactivate with the portal feature. It is replicated so Client/Server RPCs run
 * over the owning PlayerController channel.
 */
UCLASS(Blueprintable, ClassGroup = (Portals), meta = (BlueprintSpawnableComponent))
class GF_PORTALS_CORE_API URpgPortalTravelComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	URpgPortalTravelComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	static URpgPortalTravelComponent* FindPortalTravelComponent(AController* Controller);

	virtual void BeginPlay() override;
	virtual void ReceivedPlayer() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Starts a server-authoritative realm travel request for the owning controller.
	 *
	 * Remote clients receive a Client RPC and must load/show the matching dynamic
	 * level instance before the server will teleport the pawn. Local authority
	 * controllers can become ready immediately because they already share the
	 * server's streamed world.
	 */
	bool BeginPortalTravel(
		ARpgPortalActor* Portal,
		AActor* TravelActor,
		const FSoftObjectPath& RealmLevelPath,
		const FTransform& LevelInstanceTransform,
		const FString& LevelInstanceName,
		FName ExpectedPackageName,
		int32 RequestId);

	/** Cancels the active request and tells the owning client to unload its local realm instance. */
	void CancelPortalTravel(ARpgPortalActor* Portal, int32 RequestId, const TCHAR* Reason);

	/** Marks the active request as failed and unloads any client-side local realm instance. */
	void FailPortalTravel(ARpgPortalActor* Portal, int32 RequestId, const TCHAR* Reason);

	/** Called by the portal when the server is about to move the pawn. */
	bool MarkTeleporting(ARpgPortalActor* Portal, int32 RequestId);

	/** Called by the portal after a successful server teleport into the realm. */
	bool MarkInsideRealm(ARpgPortalActor* Portal, int32 RequestId);

	/** Called by the portal when this controller exits the realm back to the overworld. */
	void BeginPortalExit(ARpgPortalActor* Portal);

	UFUNCTION(BlueprintPure, Category = "Portal|Travel")
	ERpgPortalTravelState GetTravelState() const { return TravelState; }

	UFUNCTION(BlueprintPure, Category = "Portal|Travel")
	int32 GetActiveRequestId() const { return ActiveRequestId; }

	UFUNCTION(BlueprintPure, Category = "Portal|Travel")
	ARpgPortalActor* GetActivePortal() const { return ActivePortal; }

	AActor* GetTravelActorForRequest(ARpgPortalActor* Portal, int32 RequestId) const;
	bool IsActiveRequest(ARpgPortalActor* Portal, int32 RequestId) const;

protected:
	UFUNCTION(Client, Reliable)
	void ClientLoadPortalRealm(
		ARpgPortalActor* Portal,
		int32 RequestId,
		FSoftObjectPath RealmLevelPath,
		FTransform LevelInstanceTransform,
		const FString& LevelInstanceName,
		FName ExpectedPackageName);

	UFUNCTION(Client, Reliable)
	void ClientUnloadPortalRealm(ARpgPortalActor* Portal, const FString& LevelInstanceName, int32 RequestId, ERpgPortalTravelState TerminalState);

	UFUNCTION(Server, Reliable)
	void ServerNotifyPortalRealmLevelShown(ARpgPortalActor* Portal, int32 RequestId, const FString& LevelInstanceName, FName ExpectedPackageName);

	UFUNCTION(Server, Reliable)
	void ServerNotifyPortalRealmTravelFailed(ARpgPortalActor* Portal, int32 RequestId, const FString& LevelInstanceName);

	UFUNCTION()
	void HandleClientRealmLevelShown();

	void StartServerVisibilityWait();
	void TryCompleteServerVisibilityWait();
	bool IsExpectedPackageVisibleToOwningClient() const;
	bool ShouldUseClientLoadHandshake() const;
	bool LoadClientRealmLevelInstance();
	void UnloadClientRealmLevelInstance();
	void StartClientDeferredUnloadAfterExit(ARpgPortalActor* Portal, const FString& LevelInstanceName, int32 RequestId);
	void TryClientDeferredUnloadAfterExit();
	bool IsClientSafeToUnloadRealmLevelInstance() const;
	bool IsObjectInLocalRealmLevel(const UObject* Object) const;
	void ScheduleServerResumeCheck();
	void TryRestorePortalResumeAfterLogin();
	void SetTravelState(ERpgPortalTravelState NewState);
	void ResetRequestData();
	void ResetPendingClientUnloadData();
	APlayerController* GetOwningPlayerController() const;
	void LogInvalidTransition(const TCHAR* Reason, ARpgPortalActor* Portal, int32 RequestId) const;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Portal|Travel")
	ERpgPortalTravelState TravelState = ERpgPortalTravelState::Idle;

	UPROPERTY(Transient)
	TObjectPtr<ARpgPortalActor> ActivePortal;

	UPROPERTY(Transient)
	TObjectPtr<AActor> ActiveTravelActor;

	UPROPERTY(Transient)
	TObjectPtr<ULevelStreamingDynamic> LocalRealmLevelStreaming;

	FSoftObjectPath ActiveRealmLevelPath;
	FTransform ActiveLevelInstanceTransform = FTransform::Identity;
	FString ActiveLevelInstanceName;
	FName ActiveExpectedPackageName;
	int32 ActiveRequestId = 0;
	double ServerVisibilityWaitStartTime = 0.0;

	FTimerHandle NetVisibilityRetryTimerHandle;
	FTimerHandle ClientDeferredUnloadTimerHandle;
	FTimerHandle ServerResumeCheckTimerHandle;
	TObjectPtr<ARpgPortalActor> PendingUnloadPortal;
	FString PendingUnloadLevelInstanceName;
	int32 PendingUnloadRequestId = 0;
	double ClientDeferredUnloadStartTime = 0.0;
	int32 ServerResumeCheckAttempts = 0;
};
