// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgPawnGameplayComponent.h"

#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "RpgCharacter.h"
#include "RpgPawnData.h"
#include "RpgPawnExtensionComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "GameFramework/Controller.h"
#include "SurvivalRpg/Camera/RpgCameraComponent.h"
#include "SurvivalRpg/Camera/RpgCameraMode.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Input/RpgInputComponent.h"

namespace RpgCharacter
{
	static constexpr float LookYawRate = 300.0f;
	static constexpr float LookPitchRate = 165.0f;
}
const FName URpgPawnGameplayComponent::Name_ActorFeatureName = FName("RpgPawnGameplayComponent");



// Sets default values for this component's properties
URpgPawnGameplayComponent::URpgPawnGameplayComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
}

void URpgPawnGameplayComponent::BeginPlay()
{
	Super::BeginPlay();

	if (APawn* Pawn = GetPawn<APawn>())
	{
		if (URpgPawnExtensionComponent* PawnExt = URpgPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
		{
			PawnExt->OnAbilitySystemUninitialized_Register(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemUninitialized));
		}
	}

	BindOnActorInitStateChanged(URpgPawnExtensionComponent::Name_ActorFeatureName, FGameplayTag(), false);
	TryToChangeInitState(RpgGameplayTags::InitState_Spawned);
	CheckDefaultInitialization();
}

void URpgPawnGameplayComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APawn* Pawn = GetPawn<APawn>())
	{
		if (URpgCameraComponent* CameraComponent = URpgCameraComponent::FindCameraComponent(Pawn))
		{
			CameraComponent->DetermineCameraModeDelegate.Unbind();
		}
	}
	UnregisterInitStateFeature();
	Super::EndPlay(EndPlayReason);
}

void URpgPawnGameplayComponent::OnRegister()
{
	Super::OnRegister();
	if (GetPawn<APawn>())
	{
		RegisterInitStateFeature();
	}
}


bool URpgPawnGameplayComponent::CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) const
{
	check(Manager);
	APawn* Pawn = GetPawn<APawn>();
	if (!Pawn) return false;
	
	// -------------- Spawned --------------
	if (!CurrentState.IsValid() && DesiredState == RpgGameplayTags::InitState_Spawned)
	{
		return true;
	}

	// -------------- DataAvailable --------------
	if (CurrentState == RpgGameplayTags::InitState_Spawned && DesiredState == RpgGameplayTags::InitState_DataAvailable)
	{
		if (!GetPlayerState<ARpgPlayerState>())
		{
			return false;
		}

		if (Pawn->GetLocalRole() != ROLE_SimulatedProxy)
		{
			AController* Controller = GetController<AController>();
			const bool bHasControllerPairedWithPS = (Controller != nullptr)
				&& (Controller->PlayerState != nullptr)
				&& (Controller->PlayerState->GetOwner() == Controller);

			if (!bHasControllerPairedWithPS)
			{
				return false;
			}
		}

		const bool bIsLocallyControlled = Pawn->IsLocallyControlled();
		const bool bIsBot = Pawn->IsBotControlled();
		if (bIsLocallyControlled && !bIsBot)
		{
			APlayerController* PC = GetController<APlayerController>();
			if (!Pawn->InputComponent || !PC || !PC->GetLocalPlayer())
			{
				return false;
			}
		}
		return true;
	}
	
	// -------------- DataInitialized --------------
	if (CurrentState == RpgGameplayTags::InitState_DataAvailable && DesiredState == RpgGameplayTags::InitState_DataInitialized)
	{
		ARpgPlayerState* PlayerState = GetPlayerState<ARpgPlayerState>();
		return PlayerState
			&& Manager->HasFeatureReachedInitState(Pawn, URpgPawnExtensionComponent::Name_ActorFeatureName, RpgGameplayTags::InitState_DataInitialized);
	}
	
	// -------------- GameplayReady --------------
	if (CurrentState == RpgGameplayTags::InitState_DataInitialized && DesiredState == RpgGameplayTags::InitState_GameplayReady)
	{
		return true;
	}
	return false;
}

void URpgPawnGameplayComponent::HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState)
{
	if (CurrentState == RpgGameplayTags::InitState_DataAvailable && DesiredState == RpgGameplayTags::InitState_DataInitialized)
	{
		APawn* Pawn = GetPawn<APawn>();

		if (!Pawn) return;

		const URpgPawnData* PawnData = nullptr;
		
		if (URpgPawnExtensionComponent* PawnExt = URpgPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
		{
			PawnData = PawnExt->GetPawnData<URpgPawnData>();
		}
		
		if (APlayerController* PC = GetController<APlayerController>())
		{
			if (Pawn && Pawn->InputComponent)
			{
				InitializePlayerInput(Pawn->InputComponent);
			}
		}

		if (PawnData)
		{
			if (URpgCameraComponent* CameraComponent = URpgCameraComponent::FindCameraComponent(Pawn))
			{
				CameraComponent->DetermineCameraModeDelegate.BindUObject(this, &ThisClass::DetermineCameraMode);
			}
		}
	}
}

void URpgPawnGameplayComponent::OnActorInitStateChanged(const FActorInitStateChangedParams& Params)
{
	if (Params.FeatureName == URpgPawnExtensionComponent::Name_ActorFeatureName)
	{
		if (Params.FeatureState == RpgGameplayTags::InitState_DataInitialized)
		{
			CheckDefaultInitialization();
		}
	}
}

void URpgPawnGameplayComponent::CheckDefaultInitialization()
{
	const TArray<FGameplayTag> StateChain = {
		RpgGameplayTags::InitState_Spawned,
		RpgGameplayTags::InitState_DataAvailable,
		RpgGameplayTags::InitState_DataInitialized,
		RpgGameplayTags::InitState_GameplayReady
	};
	
	ContinueInitStateChain(StateChain);
}



void URpgPawnGameplayComponent::SetAbilityCameraMode(TSubclassOf<URpgCameraMode> CameraMode,
	const FGameplayAbilitySpecHandle& OwningSpecHandle)
{
	if (CameraMode)
	{
		AbilityCameraMode = CameraMode;
		AbilityCameraModeOwningSpecHandle = OwningSpecHandle;
	}
}

void URpgPawnGameplayComponent::ClearAbilityCameraMode(const FGameplayAbilitySpecHandle& OwningSpecHandle)
{
	if (AbilityCameraModeOwningSpecHandle == OwningSpecHandle)
	{
		AbilityCameraMode = nullptr;
		AbilityCameraModeOwningSpecHandle = FGameplayAbilitySpecHandle();
	}
}

void URpgPawnGameplayComponent::InitializePlayerInput(UInputComponent* PlayerInputComponent)
{
	check(PlayerInputComponent);

	const APawn* Pawn = GetPawn<APawn>();
	if (!Pawn) return;
	
	if (URpgPawnExtensionComponent* PawnExt = URpgPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
	{
		if (const URpgPawnData* PawnData = PawnExt->GetPawnData<URpgPawnData>())
		{
			if (const URpgInputConfig* InputConfig = PawnData->InputConfig)
			{
				URpgInputComponent* RpgIC = Cast<URpgInputComponent>(PlayerInputComponent);
				if (ensureMsgf(RpgIC, TEXT("Unexpected Input Component class! The Gameplay Abilities will not be bound to their inputs. Change the input component to URpgInputComponent or a subclass of it.")))
				{
					// Add the key mappings that may have been set by the player
				
					// This is where we actually bind and input action to a gameplay tag, which means that Gameplay Ability Blueprints will
					// be triggered directly by these input actions Triggered events. 
					TArray<uint32> BindHandles;
					RpgIC->BindAbilityActions(InputConfig, this, &ThisClass::Input_AbilityInputTagPressed, &ThisClass::Input_AbilityInputTagReleased, /*out*/ BindHandles);
				
					RpgIC->BindNativeAction(InputConfig, RpgGameplayTags::InputTag_Move, ETriggerEvent::Triggered, this, &ThisClass::Input_Move, /*bLogIfNotFound=*/ false);
					RpgIC->BindNativeAction(InputConfig, RpgGameplayTags::InputTag_Look_Mouse, ETriggerEvent::Triggered, this, &ThisClass::Input_LookMouse, /*bLogIfNotFound=*/ false);
					RpgIC->BindNativeAction(InputConfig, RpgGameplayTags::InputTag_Look_Stick, ETriggerEvent::Triggered, this, &ThisClass::Input_LookStick, /*bLogIfNotFound=*/ false);
					RpgIC->BindNativeAction(InputConfig, RpgGameplayTags::InputTag_Crouch, ETriggerEvent::Triggered, this, &ThisClass::Input_Crouch, /*bLogIfNotFound=*/ false);
					RpgIC->BindNativeAction(InputConfig, RpgGameplayTags::InputTag_AutoRun, ETriggerEvent::Triggered, this, &ThisClass::Input_AutoRun, /*bLogIfNotFound=*/ false);
					RpgIC->BindNativeAction(InputConfig, RpgGameplayTags::InputTag_Jump, ETriggerEvent::Started, this, &ThisClass::Input_Jump, /*bLogIfNotFound=*/ false);
					RpgIC->BindNativeAction(InputConfig, RpgGameplayTags::InputTag_StopJump, ETriggerEvent::Completed, this, &ThisClass::Input_StopJump, /*bLogIfNotFound=*/ false);
					RpgIC->BindNativeAction(InputConfig, RpgGameplayTags::InputTag_QuickBar_Slot_1, ETriggerEvent::Started, this, &ThisClass::Input_QuickBarSlot1, /*bLogIfNotFound=*/ false);
					RpgIC->BindNativeAction(InputConfig, RpgGameplayTags::InputTag_QuickBar_Slot_2, ETriggerEvent::Started, this, &ThisClass::Input_QuickBarSlot2, /*bLogIfNotFound=*/ false);
					RpgIC->BindNativeAction(InputConfig, RpgGameplayTags::InputTag_QuickBar_Slot_3, ETriggerEvent::Started, this, &ThisClass::Input_QuickBarSlot3, /*bLogIfNotFound=*/ false);
					RpgIC->BindNativeAction(InputConfig, RpgGameplayTags::InputTag_QuickBar_Slot_4, ETriggerEvent::Started, this, &ThisClass::Input_QuickBarSlot4, /*bLogIfNotFound=*/ false);
					RpgIC->BindNativeAction(InputConfig, RpgGameplayTags::InputTag_QuickBar_Slot_5, ETriggerEvent::Started, this, &ThisClass::Input_QuickBarSlot5, /*bLogIfNotFound=*/ false);
					RpgIC->BindNativeAction(InputConfig, RpgGameplayTags::InputTag_QuickBar_Slot_6, ETriggerEvent::Started, this, &ThisClass::Input_QuickBarSlot6, /*bLogIfNotFound=*/ false);
					RpgIC->BindNativeAction(InputConfig, RpgGameplayTags::InputTag_QuickBar_Slot_7, ETriggerEvent::Started, this, &ThisClass::Input_QuickBarSlot7, /*bLogIfNotFound=*/ false);
					RpgIC->BindNativeAction(InputConfig, RpgGameplayTags::InputTag_QuickBar_Slot_8, ETriggerEvent::Started, this, &ThisClass::Input_QuickBarSlot8, /*bLogIfNotFound=*/ false);
				}
			}
		}
	}
}

void URpgPawnGameplayComponent::Input_AbilityInputTagPressed(FGameplayTag InputTag)
{
	if (const APawn* Pawn = GetPawn<APawn>())
	{
		if (const URpgPawnExtensionComponent* PawnExtComp = URpgPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
		{
			if (URpgAbilitySystemComponent* RpgAsc = PawnExtComp->GetRpgAbilitySystemComponent())
			{
				RpgAsc->AbilityInputTagPressed(InputTag);
			}
		}
	}
}

void URpgPawnGameplayComponent::Input_AbilityInputTagReleased(FGameplayTag InputTag)
{
	if (const APawn* Pawn = GetPawn<APawn>())
	{
		if (const URpgPawnExtensionComponent* PawnExtComp = URpgPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
		{
			if (URpgAbilitySystemComponent* RpgAsc = PawnExtComp->GetRpgAbilitySystemComponent())
			{
				RpgAsc->AbilityInputTagReleased(InputTag);
			}
		}
	}
}

void URpgPawnGameplayComponent::Input_Move(const FInputActionValue& InputActionValue)
{
	APawn* Pawn = GetPawn<APawn>();
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;

	// If the player has attempted to move again then cancel auto running
	if (ARpgPlayerController* RpgController = Cast<ARpgPlayerController>(Controller))
	{
		RpgController->SetIsAutoRunning(false);
	}
	
	if (Controller)
	{
		const FVector2D Value = InputActionValue.Get<FVector2D>();
		const FRotator MovementRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);

		if (Value.X != 0.0f)
		{
			const FVector MovementDirection = MovementRotation.RotateVector(FVector::RightVector);
			Pawn->AddMovementInput(MovementDirection, Value.X);
		}

		if (Value.Y != 0.0f)
		{
			const FVector MovementDirection = MovementRotation.RotateVector(FVector::ForwardVector);
			Pawn->AddMovementInput(MovementDirection, Value.Y);
		}
	}
}

void URpgPawnGameplayComponent::Input_LookMouse(const FInputActionValue& InputActionValue)
{
	APawn* Pawn = GetPawn<APawn>();

	if (!Pawn)
	{
		return;
	}
	
	const FVector2D Value = InputActionValue.Get<FVector2D>();

	if (Value.X != 0.0f)
	{
		Pawn->AddControllerYawInput(Value.X);
	}

	if (Value.Y != 0.0f)
	{
		Pawn->AddControllerPitchInput(Value.Y);
	}
}

void URpgPawnGameplayComponent::Input_LookStick(const FInputActionValue& InputActionValue)
{
	APawn* Pawn = GetPawn<APawn>();

	if (!Pawn)
	{
		return;
	}
	
	const FVector2D Value = InputActionValue.Get<FVector2D>();

	const UWorld* World = GetWorld();
	check(World);

	if (Value.X != 0.0f)
	{
		Pawn->AddControllerYawInput(Value.X * RpgCharacter::LookYawRate * World->GetDeltaSeconds());
	}

	if (Value.Y != 0.0f)
	{
		Pawn->AddControllerPitchInput(Value.Y * RpgCharacter::LookPitchRate * World->GetDeltaSeconds());
	}
}

void URpgPawnGameplayComponent::Input_Crouch(const FInputActionValue& InputActionValue)
{
	if (ARpgCharacter* Character = GetPawn<ARpgCharacter>())
	{
		Character->ToggleCrouch();
	}
}

void URpgPawnGameplayComponent::Input_AutoRun(const FInputActionValue& InputActionValue)
{
	if (APawn* Pawn = GetPawn<APawn>())
	{
		if (ARpgPlayerController* Controller = Cast<ARpgPlayerController>(Pawn->GetController()))
		{
			Controller->SetIsAutoRunning(!Controller->GetIsAutoRunning());
		}
	}
}

void URpgPawnGameplayComponent::Input_Jump(const FInputActionValue& InputActionValue)
{
	if (ARpgCharacter* Character = GetPawn<ARpgCharacter>())
	{
		Character->UnCrouch();
		Character->Jump();
	}
}

void URpgPawnGameplayComponent::Input_StopJump(const FInputActionValue& InputActionValue)
{
	if (ARpgCharacter* Character = GetPawn<ARpgCharacter>())
	{
		Character->StopJumping();
	}
}

void URpgPawnGameplayComponent::Input_QuickBarSlot1(const FInputActionValue& InputActionValue)
{
	Input_QuickBarSlot(0);
}

void URpgPawnGameplayComponent::Input_QuickBarSlot2(const FInputActionValue& InputActionValue)
{
	Input_QuickBarSlot(1);
}

void URpgPawnGameplayComponent::Input_QuickBarSlot3(const FInputActionValue& InputActionValue)
{
	Input_QuickBarSlot(2);
}

void URpgPawnGameplayComponent::Input_QuickBarSlot4(const FInputActionValue& InputActionValue)
{
	Input_QuickBarSlot(3);
}

void URpgPawnGameplayComponent::Input_QuickBarSlot5(const FInputActionValue& InputActionValue)
{
	Input_QuickBarSlot(4);
}

void URpgPawnGameplayComponent::Input_QuickBarSlot6(const FInputActionValue& InputActionValue)
{
	Input_QuickBarSlot(5);
}

void URpgPawnGameplayComponent::Input_QuickBarSlot7(const FInputActionValue& InputActionValue)
{
	Input_QuickBarSlot(6);
}

void URpgPawnGameplayComponent::Input_QuickBarSlot8(const FInputActionValue& InputActionValue)
{
	Input_QuickBarSlot(7);
}

void URpgPawnGameplayComponent::Input_QuickBarSlot(int32 SlotIndex)
{
	if (APawn* Pawn = GetPawn<APawn>())
	{
		if (ARpgPlayerController* Controller = Cast<ARpgPlayerController>(Pawn->GetController()))
		{
			Controller->SetActiveQuickBarSlot(SlotIndex);
		}
	}
}

TSubclassOf<URpgCameraMode> URpgPawnGameplayComponent::DetermineCameraMode() const
{
	if (AbilityCameraMode)
	{
		return AbilityCameraMode;
	}

	const APawn* Pawn = GetPawn<APawn>();
	if (!Pawn)
	{
		return nullptr;
	}

	if (URpgPawnExtensionComponent* PawnExtComp = URpgPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
	{
		if (const URpgPawnData* PawnData = PawnExtComp->GetPawnData<URpgPawnData>())
		{
			return PawnData->DefaultCameraMode;
		}
	}

	return nullptr;
}

void URpgPawnGameplayComponent::HandleAbilitySystemUninitialized()
{
	if (APawn* Pawn = GetPawn<APawn>())
	{
		if (URpgCameraComponent* CameraComponent = URpgCameraComponent::FindCameraComponent(Pawn))
		{
			CameraComponent->DetermineCameraModeDelegate.Unbind();
		}
	}
}

