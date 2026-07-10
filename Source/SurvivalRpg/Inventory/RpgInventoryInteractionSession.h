#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "RpgInventoryDragDrop.h"
#include "UObject/Object.h"

#include "RpgInventoryInteractionSession.generated.h"

class APlayerController;
class UActorComponent;
class URpgInventoryManagerComponent;
struct FRpgActionBarSlotsChangedMessage;
struct FRpgEquipmentLoadoutSlotsChangedMessage;
struct FRpgInventoryActionFeedbackMessage;
struct FRpgInventoryChangeMessage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FRpgInventoryInteractionStateChanged,
	ERpgInventoryInteractionPreviewState, PreviewState,
	bool, bHasPayload,
	bool, bPendingRequest);

/**
 * Screen-local state shared by mouse drag/drop and controller pick/place inventory interaction.
 *
 * The session owns only transient UI intent: payload, rotation, grab offsets, target, preview, and pending request
 * correlation. Gameplay state remains server-authoritative in inventory, equipment, and actionbar components.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryInteractionSession : public UObject
{
	GENERATED_BODY()

public:
	/** Initializes message acknowledgement for the local player that owns this inventory screen. */
	void Initialize(UObject* InWorldContextObject, APlayerController* InPlayerController);

	/** Starts or replaces the transient payload used by both pointer and controller interaction. */
	bool BeginInteraction(const FRpgInventoryDragPayload& InPayload, ERpgInventoryInteractionInputMode InInputMode);

	/** Clears all transient interaction state without sending a gameplay mutation. */
	void CancelInteraction();

	/** Updates the current target and its locally resolved semantic preview. */
	void SetPreviewTarget(const FRpgInventoryDropTarget& InTarget, ERpgInventoryInteractionPreviewState InPreviewState);

	/** Clears only the current hover/focus target while retaining the held payload and its rotation. */
	void ClearPreviewTarget();

	/** Rotates the UI payload in place, including cell and pointer grab offsets. */
	bool ToggleTargetRotation();

	/** Marks a dispatched server request pending while retaining the payload until acknowledgement. */
	void MarkRequestPending(const FRpgInventoryDropTarget& InTarget, FGameplayTag InActionTag);

	/** Marks a locally dispatched request as rejected when no server request could be sent. */
	void RejectRequestLocally();

	/** True while this screen owns a mouse or controller payload. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Interaction")
	bool HasPayload() const { return bHasPayload; }

	/** True after request dispatch and before authoritative acknowledgement. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Interaction")
	bool IsRequestPending() const { return bPendingRequest; }

	/** Current shared mouse/controller payload, including transformed grab offsets. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Interaction")
	FRpgInventoryDragPayload GetPayload() const { return Payload; }

	/** Current target rotation applied when building spatial target placements. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Interaction")
	bool IsTargetRotated() const { return bTargetRotated; }

	/** Last target previewed or committed by this screen-local interaction. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Interaction")
	FRpgInventoryDropTarget GetTarget() const { return Target; }

	/** Semantic presentation state for target indicators and contextual action text. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Interaction")
	ERpgInventoryInteractionPreviewState GetPreviewState() const { return PreviewState; }

	/** Local request correlation id. It is UI-only until the authoritative action API gains request ids. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Interaction")
	FGuid GetRequestId() const { return RequestId; }

	/** Input path that began the current interaction. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Interaction")
	ERpgInventoryInteractionInputMode GetInputMode() const { return InputMode; }

	/** Fired when payload, rotation, target, preview, or pending state changes. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Interaction")
	FRpgInventoryInteractionStateChanged OnInteractionStateChanged;

	virtual void BeginDestroy() override;

private:
	void RegisterMessageListeners();
	void UnregisterMessageListeners();
	void ResolvePendingRequest(bool bSucceeded);
	void BroadcastStateChanged();
	bool IsPendingMessageRelevant(UActorComponent* InventoryOwner, const UObject* Item) const;
	void HandleActionFeedback(FGameplayTag Channel, const FRpgInventoryActionFeedbackMessage& Message);
	void HandleInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message);
	void HandleEquipmentChanged(FGameplayTag Channel, const FRpgEquipmentLoadoutSlotsChangedMessage& Message);
	void HandleActionBarChanged(FGameplayTag Channel, const FRpgActionBarSlotsChangedMessage& Message);

	UPROPERTY(Transient)
	TObjectPtr<UObject> WorldContextObject = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<APlayerController> PlayerController = nullptr;

	UPROPERTY(Transient)
	FRpgInventoryDragPayload Payload;

	UPROPERTY(Transient)
	FRpgInventoryDropTarget Target;

	UPROPERTY(Transient)
	ERpgInventoryInteractionPreviewState PreviewState = ERpgInventoryInteractionPreviewState::None;

	UPROPERTY(Transient)
	ERpgInventoryInteractionInputMode InputMode = ERpgInventoryInteractionInputMode::None;

	UPROPERTY(Transient)
	FGuid RequestId;

	UPROPERTY(Transient)
	FGameplayTag PendingActionTag;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryManagerComponent> PendingSourceInventory = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryManagerComponent> PendingTargetInventory = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UObject> PendingItem = nullptr;

	FGuid PendingEntryId;
	bool bHasPayload = false;
	bool bTargetRotated = false;
	bool bPendingRequest = false;

	FGameplayMessageListenerHandle ActionFeedbackHandle;
	FGameplayMessageListenerHandle InventoryChangedHandle;
	FGameplayMessageListenerHandle EquipmentChangedHandle;
	FGameplayMessageListenerHandle ActionBarChangedHandle;
};
