// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgPlayerController.h"

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerInput.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/Core/Player/RpgBasePlayerState.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Equipment/RpgQuickBarComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Progression/Player/RpgPlayerProgressionComponent.h"
#include "SurvivalRpg/SurvivalRpg.h"

ARpgPlayerController::ARpgPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	QuickBarComponent = CreateDefaultSubobject<URpgQuickBarComponent>(TEXT("QuickBarComponent"));
}

ARpgPlayerState* ARpgPlayerController::GetRpgPlayerState() const
{
	return CastChecked<ARpgPlayerState>(PlayerState, ECastCheckedType::NullAllowed);
}

URpgAbilitySystemComponent* ARpgPlayerController::GetRpgAbilitySystemComponent() const
{
	const ARpgPlayerState* RpgPS = GetRpgPlayerState();
	return (RpgPS ? RpgPS->GetRpgAbilitySystemComponent() : nullptr);
}

void ARpgPlayerController::RequestRespawn()
{
	if (!HasAuthority())
	{
		ServerRequestRespawn();
		return;
	}

	if (ARpgGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARpgGameModeBase>() : nullptr)
	{
		GameMode->RequestPlayerRespawn(this);
	}
}

void ARpgPlayerController::ClientRestoreGameplayInputFocus_Implementation()
{
	RestoreGameplayInputFocus();
}

void ARpgPlayerController::SetActiveQuickBarSlot(int32 SlotIndex)
{
	if (QuickBarComponent != nullptr)
	{
		QuickBarComponent->SetActiveSlotIndex(SlotIndex);
	}
}

void ARpgPlayerController::RpgPrintProgression() const
{
	const ARpgPlayerState* RpgPS = GetRpgPlayerState();
	const URpgPlayerProgressionComponent* ProgressionComponent = RpgPS ? RpgPS->GetPlayerProgressionComponent() : nullptr;
	if (ProgressionComponent == nullptr)
	{
		UE_LOG(LogRpgProgression, Warning, TEXT("No player progression component found for %s."), *GetNameSafe(this));
		return;
	}

	const FString Message = FString::Printf(
		TEXT("Progression: Level %d | XP %.0f / %.0f | SkillPoints %d"),
		ProgressionComponent->GetLevel(),
		ProgressionComponent->GetXP(),
		ProgressionComponent->GetXPToNextLevelForCurrentLevel(),
		ProgressionComponent->GetUnspentSkillPoints());

	UE_LOG(LogRpgProgression, Display, TEXT("%s"), *Message);
	if (GEngine && IsLocalController())
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 5.0f, FColor::Green, Message);
	}
}

void ARpgPlayerController::BeginPlayingState()
{
	Super::BeginPlayingState();
	RefreshPlayerStateBindings();
	BindToGameModeRespawnEvent();
}

void ARpgPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (!IsLocalController())
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		for (const UInputMappingContext* MappingContext : DefaultMappingContexts)
		{
			if (MappingContext && !InputSubsystem->HasMappingContext(MappingContext))
			{
				InputSubsystem->AddMappingContext(MappingContext, 0);
			}
		}
	}
}

void ARpgPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromPawnExtensionForLoadout();
	UnbindFromGameModeRespawnEvent();
	Super::EndPlay(EndPlayReason);
}

void ARpgPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (GetIsAutoRunning())
	{
		if (APawn* CurrentPawn = GetPawn())
		{
			const FRotator MovementRotation(0.0f, GetControlRotation().Yaw, 0.0f);
			const FVector MovementDirection = MovementRotation.RotateVector(FVector::ForwardVector);
			CurrentPawn->AddMovementInput(MovementDirection, 1.0f);
		}
	}
}

void ARpgPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	SetIsAutoRunning(false);
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
	ClientRestoreGameplayInputFocus();
	BindToGameModeRespawnEvent();
	BindToPawnExtensionForLoadout(InPawn);
}

void ARpgPlayerController::OnUnPossess()
{
	if (QuickBarComponent)
	{
		QuickBarComponent->UnequipActiveLoadoutFromCurrentPawn();
	}

	UnbindFromPawnExtensionForLoadout();
	Super::OnUnPossess();
}

void ARpgPlayerController::SetGenericTeamId(const FGenericTeamId& NewTeamID)
{
	UE_LOG(LogTemp, Verbose, TEXT("ARpgPlayerController ignores SetGenericTeamId because team is driven by PawnData on the PlayerState."));
}

FGenericTeamId ARpgPlayerController::GetGenericTeamId() const
{
	const ARpgBasePlayerState* RpgPS = GetPlayerState<ARpgBasePlayerState>();
	return RpgPS ? RpgPS->GetGenericTeamId() : FGenericTeamId::NoTeam;
}

ETeamAttitude::Type ARpgPlayerController::GetTeamAttitudeTowards(const AActor& Other) const
{
	return ARpgBasePlayerState::GetTeamAttitudeTowardsActor(GetGenericTeamId(), Other);
}

void ARpgPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	RefreshPlayerStateBindings();

	if (GetWorld()->IsNetMode(NM_Client))
	{
		if (ARpgPlayerState* RpgPS = GetPlayerState<ARpgPlayerState>())
		{
			if (URpgAbilitySystemComponent* RpgASC = RpgPS->GetRpgAbilitySystemComponent())
			{
				RpgASC->RefreshAbilityActorInfo();
				RpgASC->TryActivateAbilitiesOnSpawn();
			}
		}
	}
}

void ARpgPlayerController::PostProcessInput(const float DeltaTime, const bool bGamePaused)
{
	if (URpgAbilitySystemComponent* RpgASC = GetRpgAbilitySystemComponent())
	{
		RpgASC->ProcessAbilityInput(DeltaTime, bGamePaused);
	}
	Super::PostProcessInput(DeltaTime, bGamePaused);
}

void ARpgPlayerController::SetIsAutoRunning(bool bEnabled)
{
	const bool bIsAutoRunning = GetIsAutoRunning();
	if (bEnabled == bIsAutoRunning)
	{
		return;
	}

	if (bEnabled)
	{
		OnStartAutoRun();
	}
	else
	{
		OnEndAutoRun();
	}
}

bool ARpgPlayerController::GetIsAutoRunning() const
{
	if (const URpgAbilitySystemComponent* RpgASC = GetRpgAbilitySystemComponent())
	{
		return RpgASC->GetTagCount(RpgGameplayTags::Status_AutoRunning) > 0;
	}

	return false;
}

void ARpgPlayerController::ServerRequestRespawn_Implementation()
{
	RequestRespawn();
}

void ARpgPlayerController::HandleRespawnStateChanged(bool bIsWaitingForRespawn, float RespawnAvailableServerTime)
{
	K2_OnRespawnStateChanged(bIsWaitingForRespawn, RespawnAvailableServerTime);

	if (!bIsWaitingForRespawn && IsLocalController() && GetPawn())
	{
		RestoreGameplayInputFocus();
	}
}

void ARpgPlayerController::HandleCheckpointChanged(bool bHasCheckpoint, FTransform CheckpointTransform)
{
	K2_OnCheckpointChanged(bHasCheckpoint, CheckpointTransform);
}

void ARpgPlayerController::RefreshPlayerStateBindings()
{
	ARpgPlayerState* CurrentPlayerState = GetPlayerState<ARpgPlayerState>();
	if (BoundPlayerState == CurrentPlayerState)
	{
		return;
	}

	UnbindFromPlayerState();
	BindToPlayerState(CurrentPlayerState);
}

void ARpgPlayerController::BindToPlayerState(ARpgPlayerState* NewPlayerState)
{
	if (!NewPlayerState)
	{
		return;
	}

	BoundPlayerState = NewPlayerState;
	BoundPlayerState->OnRespawnStateChanged.AddDynamic(this, &ThisClass::HandleRespawnStateChanged);
	BoundPlayerState->OnCheckpointChanged.AddDynamic(this, &ThisClass::HandleCheckpointChanged);

	HandleRespawnStateChanged(
		BoundPlayerState->IsWaitingForRespawn(),
		BoundPlayerState->GetRespawnAvailableServerTime());

	HandleCheckpointChanged(
		BoundPlayerState->HasCheckpoint(),
		BoundPlayerState->GetCheckpointTransform());
}

void ARpgPlayerController::UnbindFromPlayerState()
{
	if (!BoundPlayerState)
	{
		return;
	}

	BoundPlayerState->OnRespawnStateChanged.RemoveDynamic(this, &ThisClass::HandleRespawnStateChanged);
	BoundPlayerState->OnCheckpointChanged.RemoveDynamic(this, &ThisClass::HandleCheckpointChanged);
	BoundPlayerState = nullptr;
}

void ARpgPlayerController::OnStartAutoRun()
{
	if (URpgAbilitySystemComponent* RpgASC = GetRpgAbilitySystemComponent())
	{
		RpgASC->SetLooseGameplayTagCount(RpgGameplayTags::Status_AutoRunning, 1);
		K2_OnStartAutoRun();
	}
}

void ARpgPlayerController::OnEndAutoRun()
{
	if (URpgAbilitySystemComponent* RpgASC = GetRpgAbilitySystemComponent())
	{
		RpgASC->SetLooseGameplayTagCount(RpgGameplayTags::Status_AutoRunning, 0);
		K2_OnEndAutoRun();
	}
}

void ARpgPlayerController::RestoreGameplayInputFocus()
{
	if (!IsLocalController())
	{
		return;
	}

	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
	SetShowMouseCursor(false);

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);

	if (PlayerInput)
	{
		PlayerInput->FlushPressedKeys();
	}

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
}

void ARpgPlayerController::BindToPawnExtensionForLoadout(APawn* InPawn)
{
	UnbindFromPawnExtensionForLoadout();

	if (!HasAuthority() || !InPawn)
	{
		return;
	}

	URpgPawnExtensionComponent* PawnExtension = URpgPawnExtensionComponent::FindPawnExtensionComponent(InPawn);
	if (!PawnExtension)
	{
		return;
	}

	BoundLoadoutPawnExtension = PawnExtension;
	BoundLoadoutPawnExtension->OnAbilitySystemInitialized_RegisterAndCall(
		FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandlePossessedPawnAbilitySystemInitialized));
	BoundLoadoutPawnExtension->OnAbilitySystemUninitialized_Register(
		FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandlePossessedPawnAbilitySystemUninitialized));
}

void ARpgPlayerController::UnbindFromPawnExtensionForLoadout()
{
	if (!BoundLoadoutPawnExtension)
	{
		return;
	}

	BoundLoadoutPawnExtension->OnAbilitySystemInitialized.RemoveAll(this);
	BoundLoadoutPawnExtension->OnAbilitySystemUninitialized.RemoveAll(this);
	BoundLoadoutPawnExtension = nullptr;
}

void ARpgPlayerController::HandlePossessedPawnAbilitySystemInitialized()
{
	if (HasAuthority() && QuickBarComponent)
	{
		QuickBarComponent->RefreshActiveLoadoutOnCurrentPawn();
	}
}

void ARpgPlayerController::HandlePossessedPawnAbilitySystemUninitialized()
{
	if (QuickBarComponent)
	{
		QuickBarComponent->UnequipActiveLoadoutFromCurrentPawn();
	}
}

void ARpgPlayerController::BindToGameModeRespawnEvent()
{
	if (!HasAuthority())
	{
		return;
	}

	ARpgGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARpgGameModeBase>() : nullptr;
	if (BoundRespawnGameMode == GameMode)
	{
		return;
	}

	UnbindFromGameModeRespawnEvent();

	if (GameMode)
	{
		BoundRespawnGameMode = GameMode;
		BoundRespawnGameMode->OnPlayerRespawned.AddUniqueDynamic(this, &ThisClass::HandleGameModePlayerRespawned);
	}
}

void ARpgPlayerController::UnbindFromGameModeRespawnEvent()
{
	if (!BoundRespawnGameMode)
	{
		return;
	}

	BoundRespawnGameMode->OnPlayerRespawned.RemoveDynamic(this, &ThisClass::HandleGameModePlayerRespawned);
	BoundRespawnGameMode = nullptr;
}

void ARpgPlayerController::HandleGameModePlayerRespawned(APlayerController* RespawnedPlayerController, FTransform RespawnTransform)
{
	if (RespawnedPlayerController != this)
	{
		return;
	}

	if (QuickBarComponent)
	{
		QuickBarComponent->RefreshActiveLoadoutOnCurrentPawn();
	}

	ClientRestoreGameplayInputFocus();
}
