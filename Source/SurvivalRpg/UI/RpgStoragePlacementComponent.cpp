#include "RpgStoragePlacementComponent.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "SurvivalRpg/Base/RpgBaseBuildableDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryUiActionComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgStoragePlacementComponent)

#define LOCTEXT_NAMESPACE "StoragePlacement"

namespace
{
	URpgInventoryContainerComponent* ResolvePlacementChest(const UWorld* World, FName ContainerId)
	{
		if (!World || ContainerId.IsNone()) { return nullptr; }
		URpgInventoryContainerComponent* Result = nullptr;
		for (TActorIterator<ARpgInventoryContainerActor> It(World); It; ++It)
		{
			URpgInventoryContainerComponent* Container = It->GetContainerComponent();
			if (Container && Container->GetPersistentContainerId() == ContainerId)
			{
				if (Result) { return nullptr; }
				Result = Container;
			}
		}
		return Result;
	}
}

URpgStoragePlacementComponent::URpgStoragePlacementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(false);
}

APlayerController* URpgStoragePlacementComponent::GetLocalController() const
{
	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	return Controller && Controller->IsLocalController() ? Controller : nullptr;
}

bool URpgStoragePlacementComponent::BeginPlacement(URpgBaseBuildableDefinition* Definition, FName RelocatingContainerId)
{
	APlayerController* Controller = GetLocalController();
	if (!Controller || !Controller->GetPawn() || !Definition || !Definition->BuildActorClass ||
		!Definition->BuildActorClass->IsChildOf(ARpgInventoryContainerActor::StaticClass()) || Preview.bAwaitingServer)
	{
		return false;
	}
	URpgInventoryUiActionComponent* Actions = Controller->FindComponentByClass<URpgInventoryUiActionComponent>();
	if (!Actions) { return false; }
	URpgInventoryContainerComponent* Moving = nullptr;
	if (!RelocatingContainerId.IsNone())
	{
		Moving = ResolvePlacementChest(GetWorld(), RelocatingContainerId);
		if (!Moving || Moving->GetBuildableDefinition() != Definition || !Moving->CanActorAccess(Controller->GetPawn())) { return false; }
	}
	ResetSession();
	ActionComponent = Actions;
	ActionComponent->OnPhysicalStorageCommandCompleted.AddUniqueDynamic(this, &ThisClass::HandleCommandCompleted);
	SessionPawn = Controller->GetPawn();
	Preview.Definition = Definition;
	Preview.RelocatingContainerId = RelocatingContainerId;
	Preview.bActive = true;
	ExpectedSettingsRevision = Moving ? Moving->GetSettingsRevision() : INDEX_NONE;
	PlacementYaw = Moving ? Moving->GetOwner()->GetActorRotation().Yaw : Controller->GetPawn()->GetActorRotation().Yaw;
	bInputEnabled = true;
	SetComponentTickEnabled(true);
	CreateCosmeticPreview();
	if (!Preview.bActive || ActionComponent != Actions || Preview.RelocatingContainerId != RelocatingContainerId) { return false; }
	if (Moving)
	{
		FRpgPhysicalStorageRequest Request;
		Request.RequestId = FGuid::NewGuid();
		Request.Command = ERpgPhysicalStorageCommand::BeginRelocate;
		Request.ContainerId = RelocatingContainerId;
		Request.ExpectedSettingsRevision = ExpectedSettingsRevision;
		RelocationSessionId = Request.RequestId;
		PendingRequestId = Request.RequestId;
		bAwaitingRelocationStart = true;
		Preview.bAwaitingServer = true;
		Preview.Reason = LOCTEXT("StartingMove", "Versetzen wird gestartet …");
		PublishPreview();
		if (!IsValid(ActionComponent) || !Preview.bActive || !bAwaitingRelocationStart || PendingRequestId != Request.RequestId) { return false; }
		Actions->RequestPhysicalStorageCommand(Request);
	}
	UpdateFromView(bLastUseMouseCursor);
	return Preview.bActive;
}

bool URpgStoragePlacementComponent::UpdateFromView(bool bUseMouseCursor)
{
	// Authored UI may choose camera mode while the begin-relocation reply is still in flight.
	bLastUseMouseCursor = bUseMouseCursor;
	APlayerController* Controller = GetLocalController();
	if (!Preview.bActive || Preview.bAwaitingServer || !bInputEnabled || !Controller ||
		(!SessionPawn.IsValid() || Controller->GetPawn() != SessionPawn.Get())) { return false; }
	FVector Origin;
	FVector Direction;
	bool bHasRay = false;
	if (bUseMouseCursor) { bHasRay = Controller->DeprojectMousePositionToWorld(Origin, Direction); }
	else
	{
		FRotator ViewRotation;
		Controller->GetPlayerViewPoint(Origin, ViewRotation);
		Direction = ViewRotation.Vector();
		bHasRay = true;
	}
	FCollisionQueryParams Query(SCENE_QUERY_STAT(StoragePlacementPreview), false);
	Query.AddIgnoredActor(Controller->GetPawn());
	if (PreviewActor) { Query.AddIgnoredActor(PreviewActor); }
	if (URpgInventoryContainerComponent* Moving = ResolvePlacementChest(GetWorld(), Preview.RelocatingContainerId)) { Query.AddIgnoredActor(Moving->GetOwner()); }
	FHitResult Hit;
	Preview.bHasSurface = bHasRay && FMath::IsFinite(ViewTraceDistance) && ViewTraceDistance > 0.0f &&
		GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * ViewTraceDistance, PreviewTraceChannel, Query);
	if (Preview.bHasSurface)
	{
		Preview.Transform = FTransform(FRotator(0.0f, PlacementYaw, 0.0f), Hit.ImpactPoint, FVector::OneVector);
		ValidateCurrentPlacement();
	}
	else
	{
		Preview.bValid = false;
		Preview.Reason = LOCTEXT("NoSurface", "Keine geeignete Bodenfläche im Blick.");
	}
	PublishPreview();
	return Preview.bValid;
}

bool URpgStoragePlacementComponent::ValidateCurrentPlacement()
{
	Preview.bValid = false;
	APlayerController* Controller = GetLocalController();
	if (!Controller || !SessionPawn.IsValid() || Controller->GetPawn() != SessionPawn.Get() || !ActionComponent || !Preview.bHasSurface)
	{
		Preview.Reason = LOCTEXT("Unavailable", "Platzierung nicht verfügbar.");
		return false;
	}
	if (!Preview.RelocatingContainerId.IsNone())
	{
		if (!bRelocationAuthorized) { return false; }
		const URpgInventoryContainerComponent* Moving = ResolvePlacementChest(GetWorld(), Preview.RelocatingContainerId);
		if (!Moving || !Moving->IsContainerAccessible())
		{
			Preview.Reason = LOCTEXT("ChestUnavailable", "Kiste nicht mehr verfügbar. Versetzen erneut starten.");
			return false;
		}
		if (Moving->GetSettingsRevision() != ExpectedSettingsRevision)
		{
			Preview.Reason = LOCTEXT("ChangedChest", "Die Kiste hat sich geändert. Versetzen erneut starten.");
			return false;
		}
	}
	Preview.bValid = ActionComponent->CanPlacePhysicalStorage(Preview.Definition, Preview.Transform, Preview.RelocatingContainerId, Preview.Reason);
	return Preview.bValid;
}

void URpgStoragePlacementComponent::RotatePlacement(float Degrees)
{
	if (!Preview.bActive || Preview.bAwaitingServer || !bInputEnabled || !FMath::IsFinite(Degrees)) { return; }
	PlacementYaw = FRotator::NormalizeAxis(PlacementYaw + FMath::Fmod(Degrees, 360.0f));
	Preview.Transform.SetRotation(FRotator(0.0f, PlacementYaw, 0.0f).Quaternion());
	ValidateCurrentPlacement();
	PublishPreview();
}

bool URpgStoragePlacementComponent::ConfirmPlacement()
{
	if (!Preview.bActive || Preview.bAwaitingServer || !bInputEnabled || !UpdateFromView(bLastUseMouseCursor) || !ActionComponent) { return false; }
	FRpgPhysicalStorageRequest Request;
	Request.RequestId = FGuid::NewGuid();
	Request.Command = Preview.RelocatingContainerId.IsNone() ? ERpgPhysicalStorageCommand::Build : ERpgPhysicalStorageCommand::Relocate;
	Request.ContainerId = Preview.RelocatingContainerId;
	Request.ExpectedSettingsRevision = ExpectedSettingsRevision;
	Request.RelocationSessionId = RelocationSessionId;
	Request.Transform = Preview.Transform;
	Request.BuildableDefinition = Preview.Definition;
	PendingRequestId = Request.RequestId;
	Preview.bAwaitingServer = true;
	Preview.bValid = false;
	Preview.Reason = LOCTEXT("Waiting", "Platzierung wird bestätigt …");
	PublishPreview();
	// Set the pending state before the RPC: a listen-server result can arrive synchronously.
	if (!IsValid(ActionComponent) || PendingRequestId != Request.RequestId || !Preview.bAwaitingServer) { return false; }
	ActionComponent->RequestPhysicalStorageCommand(Request);
	return true;
}

bool URpgStoragePlacementComponent::CancelPlacement()
{
	if (!Preview.bActive || (Preview.bAwaitingServer && !bAwaitingRelocationStart)) { return false; }
	ResetSession();
	PublishPreview();
	OnPlacementFinished.Broadcast(false, true, FText::GetEmpty());
	return true;
}

void URpgStoragePlacementComponent::SetPlacementInputEnabled(bool bEnabled)
{
	if (!GetLocalController()) { return; }
	bInputEnabled = bEnabled;
	if (bEnabled && Preview.bActive && !Preview.bAwaitingServer) { UpdateFromView(bLastUseMouseCursor); }
	else if (!bEnabled && Preview.bActive && !Preview.bAwaitingServer)
	{
		Preview.bValid = false;
		Preview.Reason = LOCTEXT("InputSuspended", "Platzierung pausiert.");
		PublishPreview();
	}
}

void URpgStoragePlacementComponent::HandleCommandCompleted(FGuid RequestId, bool bSucceeded, FText Message)
{
	if (!Preview.bAwaitingServer || RequestId != PendingRequestId) { return; }
	if (bAwaitingRelocationStart)
	{
		bAwaitingRelocationStart = false;
		PendingRequestId.Invalidate();
		Preview.bAwaitingServer = false;
		bRelocationAuthorized = bSucceeded;
		if (bSucceeded) { UpdateFromView(bLastUseMouseCursor); }
		else
		{
			Preview.Reason = Message.IsEmpty() ? LOCTEXT("StartFailed", "Versetzen konnte nicht gestartet werden.") : Message;
			PublishPreview();
			OnPlacementFinished.Broadcast(false, false, Preview.Reason);
		}
		return;
	}
	if (bSucceeded) { RelocationSessionId.Invalidate(); ResetSession(); }
	else
	{
		PendingRequestId.Invalidate();
		Preview.bAwaitingServer = false;
		Preview.bValid = false;
		Preview.Reason = Message;
	}
	PublishPreview();
	OnPlacementFinished.Broadcast(bSucceeded, false, Message);
}

void URpgStoragePlacementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	APlayerController* Controller = GetLocalController();
	if (!Controller || !SessionPawn.IsValid() || Controller->GetPawn() != SessionPawn.Get())
	{
		ResetSession();
		PublishPreview();
		OnPlacementFinished.Broadcast(false, true, LOCTEXT("PawnChanged", "Platzierung beendet."));
		return;
	}
	if (bAutoUpdateFromView && bInputEnabled && !Preview.bAwaitingServer) { UpdateFromView(bLastUseMouseCursor); }
}

void URpgStoragePlacementComponent::CreateCosmeticPreview()
{
	const AActor* DefaultActor = PreviewActorClass ? PreviewActorClass->GetDefaultObject<AActor>() : nullptr;
	if (!DefaultActor || DefaultActor->GetIsReplicated() || DefaultActor->IsA<ARpgInventoryContainerActor>()) { return; }
	PreviewActor = GetWorld()->SpawnActorDeferred<AActor>(PreviewActorClass, Preview.Transform, GetOwner(), SessionPawn.Get(), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!PreviewActor) { return; }
	PreviewActor->SetActorEnableCollision(false);
	PreviewActor->SetActorHiddenInGame(true);
	PreviewActor->FinishSpawning(Preview.Transform);
	if (IsValid(PreviewActor))
	{
		PreviewActor->SetReplicates(false);
		PreviewActor->SetActorEnableCollision(false);
	}
}

void URpgStoragePlacementComponent::PublishPreview()
{
	if (IsValid(PreviewActor))
	{
		PreviewActor->SetActorTransform(Preview.Transform);
		PreviewActor->SetActorHiddenInGame(!Preview.bHasSurface);
	}
	const FRpgStoragePlacementPreview Snapshot = Preview;
	OnPlacementUpdated.Broadcast(Snapshot);
}

void URpgStoragePlacementComponent::ResetSession()
{
	SetComponentTickEnabled(false);
	if (ActionComponent) { ActionComponent->OnPhysicalStorageCommandCompleted.RemoveDynamic(this, &ThisClass::HandleCommandCompleted); }
	URpgInventoryUiActionComponent* OldActions = ActionComponent;
	FRpgPhysicalStorageRequest Cancel;
	Cancel.RequestId = FGuid::NewGuid();
	Cancel.Command = ERpgPhysicalStorageCommand::CancelRelocate;
	Cancel.ContainerId = Preview.RelocatingContainerId;
	Cancel.RelocationSessionId = RelocationSessionId;
	ActionComponent = nullptr;
	AActor* OldPreviewActor = PreviewActor;
	PreviewActor = nullptr;
	Preview = FRpgStoragePlacementPreview();
	SessionPawn.Reset();
	PendingRequestId.Invalidate();
	RelocationSessionId.Invalidate();
	bAwaitingRelocationStart = false;
	bRelocationAuthorized = false;
	ExpectedSettingsRevision = INDEX_NONE;
	bInputEnabled = false;
	if (IsValid(OldPreviewActor)) { OldPreviewActor->Destroy(); }
	// Clear local state before callbacks from a synchronous listen-server response can start another session.
	if (IsValid(OldActions) && Cancel.RelocationSessionId.IsValid()) { OldActions->RequestPhysicalStorageCommand(Cancel); }
}

void URpgStoragePlacementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetSession();
	Super::EndPlay(EndPlayReason);
}

#undef LOCTEXT_NAMESPACE
