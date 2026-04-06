#include "RpgWeaponPresentationComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Items/Fragments/RpgItemFragment_Visual.h"
#include "SurvivalRpg/Items/RpgItemInstance.h"

URpgWeaponPresentationComponent::URpgWeaponPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	PrimaryComponentTick.EndTickGroup = TG_PostUpdateWork;
}

FRpgWeaponToolCameraSettings URpgWeaponPresentationComponent::GetActiveCameraSettings() const
{
	if (const URpgItemFragment_Visual* VisualFragment = GetPrimaryPresentationVisualFragmentForWeaponSet(VisibleWeaponSetIndex))
	{
		return VisualFragment->GetWeaponToolCameraSettings();
	}

	return FRpgWeaponToolCameraSettings();
}

FRpgWeaponToolCharacterSettings URpgWeaponPresentationComponent::GetActiveWeaponToolCharacterSettings() const
{
	if (EquipmentComponent == nullptr)
	{
		return FRpgWeaponToolCharacterSettings();
	}

	if (const URpgItemFragment_Visual* VisualFragment = GetPrimaryPresentationVisualFragmentForWeaponSet(EquipmentComponent->GetActiveWeaponSetIndex()))
	{
		return VisualFragment->GetWeaponToolCharacterSettings();
	}

	return FRpgWeaponToolCharacterSettings();
}

void URpgWeaponPresentationComponent::ApplyWeaponToolPresentationNotifyAction(ERpgWeaponToolPresentationNotifyAction Action)
{
	const int32 ActiveWeaponSetIndex = EquipmentComponent ? EquipmentComponent->GetActiveWeaponSetIndex() : INDEX_NONE;
	switch (Action)
	{
	case ERpgWeaponToolPresentationNotifyAction::ApplyCurrentState:
	case ERpgWeaponToolPresentationNotifyAction::DrawActiveSet:
		if (VisibleWeaponSetIndex == ActiveWeaponSetIndex)
		{
			RefreshVisiblePresentationState();
		}
		else
		{
			SetVisibleWeaponSetIndex(ActiveWeaponSetIndex);
		}
		break;

	case ERpgWeaponToolPresentationNotifyAction::HolsterVisuals:
		if (VisibleWeaponSetIndex == INDEX_NONE)
		{
			RefreshVisiblePresentationState();
		}
		else
		{
			SetVisibleWeaponSetIndex(INDEX_NONE);
		}
		break;

	default:
		break;
	}
}

void URpgWeaponPresentationComponent::HandlePawnContextChanged()
{
	BindEquipmentComponent(ResolveEquipmentComponent());
	RefreshPresentationBindings();

	if (EquipmentComponent == nullptr)
	{
		VisibleWeaponSetIndex = INDEX_NONE;
		ApplyActiveWeaponToolCharacterSettings();
		RefreshVisiblePresentationState();
		return;
	}

	SyncFromEquipmentState(false, EquipmentComponent->GetActiveWeaponSetIndex());
}

void URpgWeaponPresentationComponent::HandleAbilitySystemInitialized()
{
	HandlePawnContextChanged();
}

void URpgWeaponPresentationComponent::HandleAbilitySystemUninitialized()
{
	UnbindEquipmentComponent();
	VisibleWeaponSetIndex = INDEX_NONE;
	DestroyAllVisualActors();
	RestorePresentationDefaults();
	ResetPresentationBindings();
	BroadcastActiveCameraSettingsIfChanged();
	UpdateTickEnabledState();
}

#if WITH_DEV_AUTOMATION_TESTS
void URpgWeaponPresentationComponent::SetPendingAnimSwitchForTests(bool bPending)
{
	PendingAnimState.bPending = bPending;
	UpdateTickEnabledState();
}

void URpgWeaponPresentationComponent::SetCameraBlendActiveForTests(bool bActive)
{
	CameraBlendState.bActive = bActive;
	UpdateTickEnabledState();
}
#endif

void URpgWeaponPresentationComponent::BeginPlay()
{
	Super::BeginPlay();
	BindPawnExtension();
	HandlePawnContextChanged();
}

void URpgWeaponPresentationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindEquipmentComponent();
	DestroyAllVisualActors();
	RestorePresentationDefaults();
	ResetPresentationBindings();
	Super::EndPlay(EndPlayReason);
}

void URpgWeaponPresentationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdatePendingAnimClassSwitch();
	UpdateCameraBlend(DeltaTime);
}

void URpgWeaponPresentationComponent::BindPawnExtension()
{
	if (const APawn* VisualPawn = Cast<APawn>(GetOwner()))
	{
		if (URpgPawnExtensionComponent* PawnExtension = URpgPawnExtensionComponent::FindPawnExtensionComponent(VisualPawn))
		{
			PawnExtension->OnAbilitySystemInitialized_RegisterAndCall(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemInitialized));
			PawnExtension->OnAbilitySystemUninitialized_Register(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemUninitialized));
		}
	}
}

void URpgWeaponPresentationComponent::UnbindEquipmentComponent()
{
	if (EquipmentComponent != nullptr)
	{
		EquipmentComponent->OnEquipmentStateChangedNative().RemoveAll(this);
		EquipmentComponent = nullptr;
	}
}

void URpgWeaponPresentationComponent::BindEquipmentComponent(URpgEquipmentComponent* InEquipmentComponent)
{
	if (EquipmentComponent == InEquipmentComponent)
	{
		return;
	}

	UnbindEquipmentComponent();
	EquipmentComponent = InEquipmentComponent;

	if (EquipmentComponent != nullptr)
	{
		EquipmentComponent->OnEquipmentStateChangedNative().AddUObject(this, &ThisClass::HandleEquipmentStateChanged);
	}
}

void URpgWeaponPresentationComponent::HandleEquipmentStateChanged(const FRpgEquipmentStateChangedEvent& Event)
{
	if (EquipmentComponent == nullptr)
	{
		return;
	}

	if (Event.HasActiveWeaponSetChanged())
	{
		SyncFromEquipmentState(true, Event.PreviousActiveWeaponSetIndex);
		return;
	}

	ApplyActiveWeaponToolCharacterSettings();
	RefreshVisiblePresentationState();
}

void URpgWeaponPresentationComponent::SyncFromEquipmentState(bool bAllowMontage, int32 PreviousActiveWeaponSetIndex)
{
	ApplyActiveWeaponToolCharacterSettings();

	const int32 ActiveWeaponSetIndex = EquipmentComponent ? EquipmentComponent->GetActiveWeaponSetIndex() : INDEX_NONE;
	if (EquipmentComponent != nullptr && bAllowMontage && PreviousActiveWeaponSetIndex != ActiveWeaponSetIndex)
	{
		const bool bUseEquipMontage = ActiveWeaponSetIndex != INDEX_NONE;
		const int32 MontageWeaponSetIndex = bUseEquipMontage ? ActiveWeaponSetIndex : PreviousActiveWeaponSetIndex;
		const bool bUsesNotifyDrivenPresentation = WeaponSetUsesPresentationNotify(MontageWeaponSetIndex, bUseEquipMontage);
		const bool bPlayedMontage = PlayPresentationMontageForWeaponSet(MontageWeaponSetIndex, bUseEquipMontage);

		if (!bUsesNotifyDrivenPresentation || !bPlayedMontage)
		{
			SetVisibleWeaponSetIndex(ActiveWeaponSetIndex);
		}

		return;
	}

	VisibleWeaponSetIndex = ActiveWeaponSetIndex;
	RefreshVisiblePresentationState();
}

void URpgWeaponPresentationComponent::RefreshPresentationBindings()
{
	RestorePresentationDefaults();
	ResetPresentationBindings();

	Bindings.Pawn = Cast<APawn>(GetOwner());
	if (Bindings.Pawn == nullptr || Bindings.Pawn->GetNetMode() == NM_DedicatedServer)
	{
		DestroyAllVisualActors();
		UpdateTickEnabledState();
		return;
	}

	Bindings.Mesh = ResolvePresentationMesh(Bindings.Pawn);
	Bindings.MovementComponent = ResolvePresentationMovementComponent(Bindings.Pawn);
	Bindings.CameraComponent = ResolvePresentationCameraComponent(Bindings.Pawn);
	Bindings.SpringArmComponent = ResolvePresentationSpringArmComponent(Bindings.Pawn);

	if (Bindings.Mesh != nullptr)
	{
		Defaults.AnimClass = Bindings.Mesh->GetAnimClass();
	}

	if (Bindings.MovementComponent != nullptr)
	{
		Defaults.MaxWalkSpeed = Bindings.MovementComponent->MaxWalkSpeed;
		Defaults.bOrientRotationToMovement = Bindings.MovementComponent->bOrientRotationToMovement;
		Defaults.bUseControllerDesiredRotation = Bindings.MovementComponent->bUseControllerDesiredRotation;
	}

	if (Bindings.CameraComponent != nullptr)
	{
		Defaults.CameraFOV = Bindings.CameraComponent->FieldOfView;
	}

	if (Bindings.SpringArmComponent != nullptr)
	{
		Defaults.SpringArmSocketOffset = Bindings.SpringArmComponent->SocketOffset;
	}

	CameraBlendState.AppliedFOV = Defaults.CameraFOV;
	CameraBlendState.AppliedSocketOffset = Defaults.SpringArmSocketOffset;
}

void URpgWeaponPresentationComponent::ResetPresentationBindings()
{
	Bindings = FRpgPresentationBindings();
	Defaults = FRpgPresentationDefaults();
	PendingAnimState = FRpgPendingAnimState();
	CameraBlendState = FRpgCameraBlendState();
	CameraBlendState.AppliedFOV = Defaults.CameraFOV;
	CameraBlendState.AppliedSocketOffset = Defaults.SpringArmSocketOffset;
}

void URpgWeaponPresentationComponent::RestorePresentationDefaults()
{
	APawn* VisualPawn = Bindings.Pawn.Get();
	if (VisualPawn == nullptr || VisualPawn->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (ShouldApplyVisibleWeaponToolAnimClassToPawn(VisualPawn) && Bindings.Mesh != nullptr && Bindings.Mesh->GetAnimClass() != Defaults.AnimClass)
	{
		Bindings.Mesh->SetAnimInstanceClass(Defaults.AnimClass);
	}

	if (ShouldApplyActiveWeaponToolCharacterSettingsToPawn(VisualPawn) && Bindings.MovementComponent != nullptr)
	{
		Bindings.MovementComponent->MaxWalkSpeed = Defaults.MaxWalkSpeed;
		Bindings.MovementComponent->bOrientRotationToMovement = Defaults.bOrientRotationToMovement;
		Bindings.MovementComponent->bUseControllerDesiredRotation = Defaults.bUseControllerDesiredRotation;
	}

	if (ShouldApplyVisibleWeaponToolCameraSettingsToPawn(VisualPawn))
	{
		if (Bindings.CameraComponent != nullptr)
		{
			Bindings.CameraComponent->SetFieldOfView(Defaults.CameraFOV);
		}

		if (Bindings.SpringArmComponent != nullptr)
		{
			Bindings.SpringArmComponent->SocketOffset = Defaults.SpringArmSocketOffset;
		}
	}
}

void URpgWeaponPresentationComponent::ApplyActiveWeaponToolCharacterSettings()
{
	APawn* VisualPawn = Bindings.Pawn.Get();
	if (!ShouldApplyActiveWeaponToolCharacterSettingsToPawn(VisualPawn) || Bindings.MovementComponent == nullptr)
	{
		return;
	}

	const FRpgWeaponToolCharacterSettings CharacterSettings = GetActiveWeaponToolCharacterSettings();
	const bool bUseOverride = CharacterSettings.bEnabled;
	Bindings.MovementComponent->MaxWalkSpeed = bUseOverride ? CharacterSettings.MaxWalkSpeed : Defaults.MaxWalkSpeed;
	Bindings.MovementComponent->bOrientRotationToMovement = bUseOverride ? CharacterSettings.bOrientRotationToMovement : Defaults.bOrientRotationToMovement;
	Bindings.MovementComponent->bUseControllerDesiredRotation = bUseOverride ? CharacterSettings.bUseControllerDesiredRotation : Defaults.bUseControllerDesiredRotation;
}

void URpgWeaponPresentationComponent::RefreshVisiblePresentationState()
{
	const URpgItemFragment_Visual* VisualFragment = GetPrimaryPresentationVisualFragmentForWeaponSet(VisibleWeaponSetIndex);
	const FRpgWeaponToolCharacterSettings VisibleCharacterSettings = VisualFragment ? VisualFragment->GetWeaponToolCharacterSettings() : FRpgWeaponToolCharacterSettings();
	const TSubclassOf<UAnimInstance> DesiredAnimClass = (VisibleCharacterSettings.bEnabled && VisibleCharacterSettings.AnimClass != nullptr)
		? VisibleCharacterSettings.AnimClass
		: Defaults.AnimClass;

	QueuePendingAnimClassSwitch(DesiredAnimClass);
	BroadcastActiveCameraSettingsIfChanged();
	StartOrUpdateCameraBlend();
	RefreshVisuals();
}

void URpgWeaponPresentationComponent::SetVisibleWeaponSetIndex(int32 InVisibleWeaponSetIndex)
{
	int32 NewVisibleWeaponSetIndex = INDEX_NONE;
	if (EquipmentComponent != nullptr
		&& InVisibleWeaponSetIndex >= 0
		&& InVisibleWeaponSetIndex < EquipmentComponent->GetDesiredWeaponSetCount())
	{
		NewVisibleWeaponSetIndex = InVisibleWeaponSetIndex;
	}

	if (VisibleWeaponSetIndex == NewVisibleWeaponSetIndex)
	{
		return;
	}

	VisibleWeaponSetIndex = NewVisibleWeaponSetIndex;
	RefreshVisiblePresentationState();
}

void URpgWeaponPresentationComponent::BroadcastActiveCameraSettingsIfChanged()
{
	const FRpgWeaponToolCameraSettings NewCameraSettings = GetActiveCameraSettings();
	if (LastBroadcastCameraSettings == NewCameraSettings)
	{
		return;
	}

	LastBroadcastCameraSettings = NewCameraSettings;
	OnActiveCameraSettingsChanged.Broadcast(NewCameraSettings);
	if (EquipmentComponent != nullptr)
	{
		EquipmentComponent->BroadcastForwardedActiveCameraSettingsChanged(NewCameraSettings);
	}
}

void URpgWeaponPresentationComponent::QueuePendingAnimClassSwitch(TSubclassOf<UAnimInstance> DesiredAnimClass)
{
	PendingAnimState.DesiredAnimClass = DesiredAnimClass;
	PendingAnimState.bPending = Bindings.Mesh != nullptr && Bindings.Mesh->GetAnimClass() != DesiredAnimClass;
	UpdateTickEnabledState();
}

void URpgWeaponPresentationComponent::StartOrUpdateCameraBlend()
{
	const FRpgWeaponToolCameraSettings CameraSettings = GetActiveCameraSettings();
	const float DesiredFOV = CameraSettings.bEnabled ? CameraSettings.FOV : Defaults.CameraFOV;
	const FVector DesiredSocketOffset = CameraSettings.bEnabled ? CameraSettings.SpringArmSocketOffset : Defaults.SpringArmSocketOffset;

	float DesiredBlendTime = CameraSettings.BlendTime;
	if (!CameraSettings.bEnabled && FMath::IsNearlyZero(DesiredBlendTime))
	{
		DesiredBlendTime = CameraBlendState.LastBlendTime;
	}
	CameraBlendState.LastBlendTime = DesiredBlendTime;
	CameraBlendState.TargetFOV = DesiredFOV;
	CameraBlendState.TargetSocketOffset = DesiredSocketOffset;

	APawn* VisualPawn = Bindings.Pawn.Get();
	if (!ShouldApplyVisibleWeaponToolCameraSettingsToPawn(VisualPawn))
	{
		CameraBlendState.bActive = false;
		CameraBlendState.AppliedFOV = DesiredFOV;
		CameraBlendState.AppliedSocketOffset = DesiredSocketOffset;
		UpdateTickEnabledState();
		return;
	}

	const float CurrentFOV = Bindings.CameraComponent != nullptr ? Bindings.CameraComponent->FieldOfView : CameraBlendState.AppliedFOV;
	const FVector CurrentSocketOffset = Bindings.SpringArmComponent != nullptr ? Bindings.SpringArmComponent->SocketOffset : CameraBlendState.AppliedSocketOffset;
	CameraBlendState.AppliedFOV = CurrentFOV;
	CameraBlendState.AppliedSocketOffset = CurrentSocketOffset;

	if (DesiredBlendTime <= 0.0f
		|| (FMath::IsNearlyEqual(CurrentFOV, DesiredFOV) && CurrentSocketOffset.Equals(DesiredSocketOffset)))
	{
		CameraBlendState.Duration = 0.0f;
		CameraBlendState.Elapsed = 0.0f;
		CameraBlendState.bActive = false;
		CameraBlendState.StartFOV = DesiredFOV;
		CameraBlendState.StartSocketOffset = DesiredSocketOffset;
		ApplyCameraBlendAlpha(1.0f);
		UpdateTickEnabledState();
		return;
	}

	CameraBlendState.StartFOV = CurrentFOV;
	CameraBlendState.StartSocketOffset = CurrentSocketOffset;
	CameraBlendState.Duration = DesiredBlendTime;
	CameraBlendState.Elapsed = 0.0f;
	CameraBlendState.bActive = true;
	UpdateTickEnabledState();
}

void URpgWeaponPresentationComponent::UpdatePendingAnimClassSwitch()
{
	if (!PendingAnimState.bPending)
	{
		return;
	}

	APawn* VisualPawn = Bindings.Pawn.Get();
	if (!ShouldApplyVisibleWeaponToolAnimClassToPawn(VisualPawn) || Bindings.Mesh == nullptr)
	{
		return;
	}

	if (Bindings.Mesh->GetAnimClass() == PendingAnimState.DesiredAnimClass)
	{
		PendingAnimState.bPending = false;
		UpdateTickEnabledState();
		return;
	}

	UAnimInstance* CurrentAnimInstance = Bindings.Mesh->GetAnimInstance();
	if (Bindings.Mesh->IsRunningParallelEvaluation()
		|| (CurrentAnimInstance != nullptr
			&& (CurrentAnimInstance->IsRunningParallelEvaluation()
				|| CurrentAnimInstance->IsUpdatingAnimation()
				|| CurrentAnimInstance->IsPostUpdatingAnimation())))
	{
		return;
	}

	Bindings.Mesh->SetAnimInstanceClass(PendingAnimState.DesiredAnimClass);
	PendingAnimState.bPending = false;
	UpdateTickEnabledState();
}

void URpgWeaponPresentationComponent::UpdateCameraBlend(float DeltaTime)
{
	if (!CameraBlendState.bActive)
	{
		return;
	}

	APawn* VisualPawn = Bindings.Pawn.Get();
	if (!ShouldApplyVisibleWeaponToolCameraSettingsToPawn(VisualPawn))
	{
		CameraBlendState.bActive = false;
		UpdateTickEnabledState();
		return;
	}

	if (CameraBlendState.Duration <= 0.0f)
	{
		ApplyCameraBlendAlpha(1.0f);
		CameraBlendState.bActive = false;
		UpdateTickEnabledState();
		return;
	}

	CameraBlendState.Elapsed = FMath::Min(CameraBlendState.Elapsed + DeltaTime, CameraBlendState.Duration);
	const float LinearAlpha = FMath::Clamp(CameraBlendState.Elapsed / CameraBlendState.Duration, 0.0f, 1.0f);
	const float EasedAlpha = LinearAlpha * LinearAlpha * (3.0f - (2.0f * LinearAlpha));
	ApplyCameraBlendAlpha(EasedAlpha);

	if (LinearAlpha >= 1.0f)
	{
		CameraBlendState.bActive = false;
		UpdateTickEnabledState();
	}
}

void URpgWeaponPresentationComponent::ApplyCameraBlendAlpha(float BlendAlpha)
{
	const float NewFOV = FMath::Lerp(CameraBlendState.StartFOV, CameraBlendState.TargetFOV, BlendAlpha);
	const FVector NewSocketOffset = FMath::Lerp(CameraBlendState.StartSocketOffset, CameraBlendState.TargetSocketOffset, BlendAlpha);
	CameraBlendState.AppliedFOV = NewFOV;
	CameraBlendState.AppliedSocketOffset = NewSocketOffset;

	if (Bindings.CameraComponent != nullptr && !FMath::IsNearlyEqual(Bindings.CameraComponent->FieldOfView, NewFOV))
	{
		Bindings.CameraComponent->SetFieldOfView(NewFOV);
	}

	if (Bindings.SpringArmComponent != nullptr && !Bindings.SpringArmComponent->SocketOffset.Equals(NewSocketOffset))
	{
		Bindings.SpringArmComponent->SocketOffset = NewSocketOffset;
	}
}

void URpgWeaponPresentationComponent::UpdateTickEnabledState()
{
	SetComponentTickEnabled(PendingAnimState.bPending || CameraBlendState.bActive);
}

bool URpgWeaponPresentationComponent::ShouldApplyActiveWeaponToolCharacterSettingsToPawn(const APawn* VisualPawn) const
{
	if (VisualPawn == nullptr || VisualPawn->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	return VisualPawn->HasAuthority() || VisualPawn->IsLocallyControlled();
}

bool URpgWeaponPresentationComponent::ShouldApplyVisibleWeaponToolAnimClassToPawn(const APawn* VisualPawn) const
{
	return VisualPawn != nullptr && VisualPawn->GetNetMode() != NM_DedicatedServer;
}

bool URpgWeaponPresentationComponent::ShouldApplyVisibleWeaponToolCameraSettingsToPawn(const APawn* VisualPawn) const
{
	return VisualPawn != nullptr && VisualPawn->GetNetMode() != NM_DedicatedServer && VisualPawn->IsLocallyControlled();
}

URpgEquipmentComponent* URpgWeaponPresentationComponent::ResolveEquipmentComponent() const
{
	const APawn* VisualPawn = Cast<APawn>(GetOwner());
	const ARpgPlayerState* PlayerState = VisualPawn ? VisualPawn->GetPlayerState<ARpgPlayerState>() : nullptr;
	return PlayerState ? PlayerState->GetEquipmentComponent() : nullptr;
}

USkeletalMeshComponent* URpgWeaponPresentationComponent::ResolvePresentationMesh(APawn* VisualPawn) const
{
	return VisualPawn ? VisualPawn->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

UCharacterMovementComponent* URpgWeaponPresentationComponent::ResolvePresentationMovementComponent(APawn* VisualPawn) const
{
	return VisualPawn ? VisualPawn->FindComponentByClass<UCharacterMovementComponent>() : nullptr;
}

UCameraComponent* URpgWeaponPresentationComponent::ResolvePresentationCameraComponent(APawn* VisualPawn) const
{
	return VisualPawn ? VisualPawn->FindComponentByClass<UCameraComponent>() : nullptr;
}

USpringArmComponent* URpgWeaponPresentationComponent::ResolvePresentationSpringArmComponent(APawn* VisualPawn) const
{
	return VisualPawn ? VisualPawn->FindComponentByClass<USpringArmComponent>() : nullptr;
}

URpgItemInstance* URpgWeaponPresentationComponent::GetPrimaryPresentationItemForWeaponSet(int32 WeaponSetIndex) const
{
	if (EquipmentComponent == nullptr)
	{
		return nullptr;
	}

	const FRpgEquippedWeaponSet WeaponSet = EquipmentComponent->GetWeaponSet(WeaponSetIndex);
	return WeaponSet.MainHandItem != nullptr ? WeaponSet.MainHandItem : WeaponSet.OffHandItem;
}

const URpgItemFragment_Visual* URpgWeaponPresentationComponent::GetPrimaryPresentationVisualFragmentForWeaponSet(int32 WeaponSetIndex) const
{
	if (URpgItemInstance* PresentationItem = GetPrimaryPresentationItemForWeaponSet(WeaponSetIndex))
	{
		return PresentationItem->FindFragmentByClass<URpgItemFragment_Visual>();
	}

	return nullptr;
}

bool URpgWeaponPresentationComponent::WeaponSetUsesPresentationNotify(int32 WeaponSetIndex, bool bUseEquipMontage) const
{
	return EquipmentComponent != nullptr && EquipmentComponent->WeaponSetUsesPresentationNotify(WeaponSetIndex, bUseEquipMontage);
}

bool URpgWeaponPresentationComponent::PlayPresentationMontageForWeaponSet(int32 WeaponSetIndex, bool bUseEquipMontage) const
{
	const URpgItemFragment_Visual* VisualFragment = GetPrimaryPresentationVisualFragmentForWeaponSet(WeaponSetIndex);
	if (VisualFragment == nullptr || Bindings.Mesh == nullptr)
	{
		return false;
	}

	UAnimMontage* MontageToPlay = bUseEquipMontage ? VisualFragment->GetEquipMontage() : VisualFragment->GetUnequipMontage();
	UAnimInstance* AnimInstance = Bindings.Mesh->GetAnimInstance();
	return MontageToPlay != nullptr && AnimInstance != nullptr && AnimInstance->Montage_Play(MontageToPlay) > 0.0f;
}

void URpgWeaponPresentationComponent::RefreshVisuals()
{
	if (Bindings.Pawn == nullptr || Bindings.Pawn->GetNetMode() == NM_DedicatedServer || EquipmentComponent == nullptr)
	{
		DestroyAllVisualActors();
		return;
	}

	TArray<URpgItemInstance*> EquippedItems;
	EquipmentComponent->GetEquippedItems(EquippedItems);

	for (int32 EntryIndex = VisualEntries.Num() - 1; EntryIndex >= 0; --EntryIndex)
	{
		const FRpgWeaponPresentationVisualEntry& Entry = VisualEntries[EntryIndex];
		if (Entry.ItemInstance == nullptr || !EquippedItems.Contains(Entry.ItemInstance))
		{
			if (Entry.VisualActor != nullptr)
			{
				Entry.VisualActor->Destroy();
			}

			VisualEntries.RemoveAtSwap(EntryIndex);
		}
	}

	if (Bindings.Mesh == nullptr)
	{
		DestroyAllVisualActors();
		return;
	}

	for (URpgItemInstance* ItemInstance : EquippedItems)
	{
		const URpgItemFragment_Visual* VisualFragment = ItemInstance ? ItemInstance->FindFragmentByClass<URpgItemFragment_Visual>() : nullptr;
		if (VisualFragment == nullptr || VisualFragment->GetEquippedActorClass() == nullptr)
		{
			DestroyVisualActorForItem(ItemInstance);
			continue;
		}

		AActor* VisualActor = FindOrSpawnVisualActor(ItemInstance);
		if (VisualActor == nullptr)
		{
			continue;
		}

		const bool bIsVisibleEquippedItem = EquipmentComponent->GetWeaponSet(VisibleWeaponSetIndex).MainHandItem == ItemInstance
			|| EquipmentComponent->GetWeaponSet(VisibleWeaponSetIndex).OffHandItem == ItemInstance;
		const FName DesiredSocket = bIsVisibleEquippedItem ? VisualFragment->GetEquippedSocketName() : VisualFragment->GetStowedSocketName();
		const bool bShouldHide = !bIsVisibleEquippedItem && DesiredSocket.IsNone() && VisualFragment->ShouldHideWhenInactiveWithoutStowedSocket();

		VisualActor->SetActorHiddenInGame(bShouldHide);
		if (bShouldHide)
		{
			continue;
		}

		VisualActor->AttachToComponent(Bindings.Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, DesiredSocket);
		VisualActor->SetActorRelativeTransform(bIsVisibleEquippedItem ? VisualFragment->GetEquippedRelativeTransform() : VisualFragment->GetStowedRelativeTransform());
	}
}

AActor* URpgWeaponPresentationComponent::FindVisualActorForItem(const URpgItemInstance* ItemInstance) const
{
	for (const FRpgWeaponPresentationVisualEntry& Entry : VisualEntries)
	{
		if (Entry.ItemInstance == ItemInstance)
		{
			return Entry.VisualActor;
		}
	}

	return nullptr;
}

AActor* URpgWeaponPresentationComponent::FindOrSpawnVisualActor(URpgItemInstance* ItemInstance)
{
	APawn* VisualPawn = Bindings.Pawn.Get();
	if (ItemInstance == nullptr || VisualPawn == nullptr)
	{
		return nullptr;
	}

	if (AActor* ExistingActor = FindVisualActorForItem(ItemInstance))
	{
		return ExistingActor;
	}

	const URpgItemFragment_Visual* VisualFragment = ItemInstance->FindFragmentByClass<URpgItemFragment_Visual>();
	if (VisualFragment == nullptr || VisualFragment->GetEquippedActorClass() == nullptr)
	{
		return nullptr;
	}

	UWorld* World = VisualPawn->GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = VisualPawn;
	SpawnParameters.Instigator = VisualPawn;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* VisualActor = World->SpawnActor<AActor>(VisualFragment->GetEquippedActorClass(), VisualPawn->GetActorLocation(), VisualPawn->GetActorRotation(), SpawnParameters);
	if (VisualActor == nullptr)
	{
		return nullptr;
	}

	VisualActor->SetReplicates(false);
	VisualActor->SetActorEnableCollision(false);

	FRpgWeaponPresentationVisualEntry& NewEntry = VisualEntries.AddDefaulted_GetRef();
	NewEntry.ItemInstance = ItemInstance;
	NewEntry.VisualActor = VisualActor;
	return VisualActor;
}

void URpgWeaponPresentationComponent::DestroyVisualActorForItem(const URpgItemInstance* ItemInstance)
{
	for (int32 EntryIndex = 0; EntryIndex < VisualEntries.Num(); ++EntryIndex)
	{
		if (VisualEntries[EntryIndex].ItemInstance != ItemInstance)
		{
			continue;
		}

		if (VisualEntries[EntryIndex].VisualActor != nullptr)
		{
			VisualEntries[EntryIndex].VisualActor->Destroy();
		}

		VisualEntries.RemoveAtSwap(EntryIndex);
		return;
	}
}

void URpgWeaponPresentationComponent::DestroyAllVisualActors()
{
	for (FRpgWeaponPresentationVisualEntry& Entry : VisualEntries)
	{
		if (Entry.VisualActor != nullptr)
		{
			Entry.VisualActor->Destroy();
		}
	}

	VisualEntries.Reset();
}
