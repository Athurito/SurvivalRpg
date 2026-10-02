#pragma once

#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "RpgStoragePlacementComponent.generated.h"

class APlayerController;
class APawn;
class URpgBaseBuildableDefinition;
class URpgInventoryUiActionComponent;

/** Owning-client read model for an authored chest placement screen and cosmetic actor. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgStoragePlacementPreview
{
	GENERATED_BODY()

	/** Selected designer-owned construction definition; null while no placement session is active. */
	UPROPERTY(BlueprintReadOnly, Category = "Storage|Placement")
	TObjectPtr<URpgBaseBuildableDefinition> Definition;

	/** Stable chest identity for a move; None means construction. */
	UPROPERTY(BlueprintReadOnly, Category = "Storage|Placement")
	FName RelocatingContainerId;

	/** Latest local ground position and yaw, with unit scale. Authority checks it again on confirmation. */
	UPROPERTY(BlueprintReadOnly, Category = "Storage|Placement")
	FTransform Transform = FTransform::Identity;

	/** True while preview or server confirmation is active. */
	UPROPERTY(BlueprintReadOnly, Category = "Storage|Placement")
	bool bActive = false;

	/** True if the latest cursor/view trace found a supported surface. */
	UPROPERTY(BlueprintReadOnly, Category = "Storage|Placement")
	bool bHasSurface = false;

	/** Read-only local preflight result; costs and world state are rechecked by authority. */
	UPROPERTY(BlueprintReadOnly, Category = "Storage|Placement")
	bool bValid = false;

	/** True after one immutable request was sent; repeated confirms cannot submit another request. */
	UPROPERTY(BlueprintReadOnly, Category = "Storage|Placement")
	bool bAwaitingServer = false;

	/** Explanation suitable for the authored CommonUI screen; cosmetic preview uses bValid for styling. */
	UPROPERTY(BlueprintReadOnly, Category = "Storage|Placement")
	FText Reason;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgStoragePlacementUpdated, const FRpgStoragePlacementPreview&, Preview);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FRpgStoragePlacementFinished, bool, bSucceeded, bool, bCancelled, const FText&, Message);

/**
 * Local placement session on the owning PlayerController. CommonUI/Blueprint owns selection, input bindings,
 * focus and preview styling. This component only traces, preflights and submits the existing authority request.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup = (UI), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgStoragePlacementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URpgStoragePlacementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Starts local preview; moving first authorizes direct chest interaction on the server, then permits walking within its base. */
	UFUNCTION(BlueprintCallable, Category = "Storage|Placement")
	bool BeginPlacement(URpgBaseBuildableDefinition* Definition, FName RelocatingContainerId = NAME_None);

	/** Traces the mouse ray, or the camera center for gamepad. It never changes CommonUI focus or input mode. */
	UFUNCTION(BlueprintCallable, Category = "Storage|Placement")
	bool UpdateFromView(bool bUseMouseCursor = true);

	/** Adds yaw in degrees, then reruns placement validation. Wire this to authored rotate actions. */
	UFUNCTION(BlueprintCallable, Category = "Storage|Placement")
	void RotatePlacement(float Degrees);

	/** Sends one immutable build/move request after a fresh local preflight; true means submitted, not accepted. */
	UFUNCTION(BlueprintCallable, Category = "Storage|Placement")
	bool ConfirmPlacement();

	/** Cancels an unsent preview. A submitted request cannot be cancelled and returns false until the server replies. */
	UFUNCTION(BlueprintCallable, Category = "Storage|Placement")
	bool CancelPlacement();

	/** CommonUI should disable this while another modal owns input, and enable it when the placement screen regains focus. */
	UFUNCTION(BlueprintCallable, Category = "Storage|Placement")
	void SetPlacementInputEnabled(bool bEnabled);

	/** Current local presentation snapshot. It contains no authoritative item or construction state. */
	UFUNCTION(BlueprintPure, Category = "Storage|Placement")
	FRpgStoragePlacementPreview GetPlacementPreview() const { return Preview; }

	/** Optional local cosmetic actor. Blueprint owns all materials, meshes, visibility details and styling. */
	UFUNCTION(BlueprintPure, Category = "Storage|Placement")
	AActor* GetPreviewActor() const { return PreviewActor; }

	/** Latest local preview or request state; bind an authored CommonUI screen or cosmetic Blueprint. */
	UPROPERTY(BlueprintAssignable, Category = "Storage|Placement")
	FRpgStoragePlacementUpdated OnPlacementUpdated;

	/** Successful confirmation, rejected request or local cancellation. Rejection leaves the preview available for correction. */
	UPROPERTY(BlueprintAssignable, Category = "Storage|Placement")
	FRpgStoragePlacementFinished OnPlacementFinished;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Cosmetic-only, nonreplicated Blueprint class. It must not own inventory or other gameplay authority. Optional. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Storage|Placement")
	TSubclassOf<AActor> PreviewActorClass;

	/** Candidate build definitions displayed by the authored picker; this list grants no server construction permission. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Storage|Placement")
	TArray<TObjectPtr<URpgBaseBuildableDefinition>> AvailableBuildables;

	/** Maximum cursor/view trace distance in centimeters. Placement range remains authoritative definition/base data. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Storage|Placement", meta = (ClampMin = "1", Units = "cm"))
	float ViewTraceDistance = 3000.0f;

	/** Channel used only to locate the preview surface; authority performs its own ground/collision checks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Storage|Placement")
	TEnumAsByte<ECollisionChannel> PreviewTraceChannel = ECC_Visibility;

	/** Updates the last selected mouse/camera ray while the placement session owns its UI input gate. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Storage|Placement")
	bool bAutoUpdateFromView = true;

private:
	UFUNCTION()
	void HandleCommandCompleted(FGuid RequestId, bool bSucceeded, FText Message);

	APlayerController* GetLocalController() const;
	bool ValidateCurrentPlacement();
	void PublishPreview();
	void ResetSession();
	void CreateCosmeticPreview();

	UPROPERTY(Transient)
	FRpgStoragePlacementPreview Preview;

	UPROPERTY(Transient)
	TObjectPtr<AActor> PreviewActor;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryUiActionComponent> ActionComponent;

	TWeakObjectPtr<APawn> SessionPawn;
	FGuid PendingRequestId;
	FGuid RelocationSessionId;
	bool bAwaitingRelocationStart = false;
	bool bRelocationAuthorized = false;
	int32 ExpectedSettingsRevision = INDEX_NONE;
	float PlacementYaw = 0.0f;
	bool bInputEnabled = false;
	bool bLastUseMouseCursor = true;
};
