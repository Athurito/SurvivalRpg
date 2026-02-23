// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgPawnGameplayComponent.h"

#include "BasePawnData.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "RpgCharacter.h"
#include "RpgPawnExtensionComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "SurvivalRpg/GameplayTags/GameplayTags.h"
#include "SurvivalRpg/Input/RpgInputComponent.h"

namespace RpgCharacter
{
	static constexpr float LookYawRate = 300.0f;
	static constexpr float LookPitchRate = 165.0f;
}
const FName URpgPawnGameplayComponent::Name_ActorFeatureName = FName("RpgPawnGameplayComponent");

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
		if (Pawn->IsLocallyControlled() && !Pawn->IsBotControlled())
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
		Manager->HasFeatureReachedInitState(Pawn, URpgPawnExtensionComponent::Name_ActorFeatureName, RpgGameplayTags::InitState_DataAvailable);
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
		if (APlayerController* PC = GetController<APlayerController>())
		{
			APawn* Pawn = GetPawn<APawn>();
			if (Pawn && Pawn->InputComponent)
			{
				InitializePlayerInput(Pawn->InputComponent);
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

// Sets default values for this component's properties
URpgPawnGameplayComponent::URpgPawnGameplayComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
}

void URpgPawnGameplayComponent::InitializePlayerInput(UInputComponent* PlayerInputComponent)
{
	check(PlayerInputComponent);

	const APawn* Pawn = GetPawn<APawn>();
	if (!Pawn) return;
	
	if (URpgPawnExtensionComponent* PawnExt = URpgPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
	{
		if (const UBasePawnData* PawnData = PawnExt->GetPawnData<UBasePawnData>())
		{
			if (const URpgInputConfig* InputConfig = PawnData->InputConfig)
			{
				// URpgInputComponent* LyraIC = Cast<URpgInputComponent>(PlayerInputComponent);
				// if (ensureMsgf(LyraIC, TEXT("Unexpected Input Component class! The Gameplay Abilities will not be bound to their inputs. Change the input component to ULyraInputComponent or a subclass of it.")))
				// {
				// 	// Add the key mappings that may have been set by the player
				//
				// 	// This is where we actually bind and input action to a gameplay tag, which means that Gameplay Ability Blueprints will
				// 	// be triggered directly by these input actions Triggered events. 
				// 	TArray<uint32> BindHandles;
				// 	LyraIC->BindAbilityActions(InputConfig, this, &ThisClass::Input_AbilityInputTagPressed, &ThisClass::Input_AbilityInputTagReleased, /*out*/ BindHandles);
				//
				// 	LyraIC->BindNativeAction(InputConfig, RpgGameplayTags::InputTag_Move, ETriggerEvent::Triggered, this, &ThisClass::Input_Move, /*bLogIfNotFound=*/ false);
				// 	LyraIC->BindNativeAction(InputConfig, RpgGameplayTags::InputTag_Look_Mouse, ETriggerEvent::Triggered, this, &ThisClass::Input_LookMouse, /*bLogIfNotFound=*/ false);
				// 	LyraIC->BindNativeAction(InputConfig, RpgGameplayTags::InputTag_Look_Stick, ETriggerEvent::Triggered, this, &ThisClass::Input_LookStick, /*bLogIfNotFound=*/ false);
				// 	LyraIC->BindNativeAction(InputConfig, RpgGameplayTags::InputTag_Crouch, ETriggerEvent::Triggered, this, &ThisClass::Input_Crouch, /*bLogIfNotFound=*/ false);
				// 	LyraIC->BindNativeAction(InputConfig, RpgGameplayTags::InputTag_AutoRun, ETriggerEvent::Triggered, this, &ThisClass::Input_AutoRun, /*bLogIfNotFound=*/ false);
				// 	LyraIC->BindNativeAction(InputConfig, RpgGameplayTags::InputTag_Jump, ETriggerEvent::Started, this, &ThisClass::Input_Jump, /*bLogIfNotFound=*/ false);
				// 	LyraIC->BindNativeAction(InputConfig, RpgGameplayTags::InputTag_StopJump, ETriggerEvent::Completed, this, &ThisClass::Input_StopJump, /*bLogIfNotFound=*/ false);
				// }
			}
		}
	}
}

void URpgPawnGameplayComponent::Input_AbilityInputTagPressed(FGameplayTag InputTag)
{
}

void URpgPawnGameplayComponent::Input_AbilityInputTagReleased(FGameplayTag InputTag)
{
}

void URpgPawnGameplayComponent::Input_Move(const FInputActionValue& InputActionValue)
{
	APawn* Pawn = GetPawn<APawn>();
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;

	// If the player has attempted to move again then cancel auto running
	// if (ARPlayerController* LyraController = Cast<ALyraPlayerController>(Controller))
	// {
	// 	LyraController->SetIsAutoRunning(false);
	// }
	
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
	//TODO
	if (ARpgCharacter* Character = GetPawn<ARpgCharacter>())
	{
		Character->ToggleCrouch();
	}
}

void URpgPawnGameplayComponent::Input_AutoRun(const FInputActionValue& InputActionValue)
{
}

void URpgPawnGameplayComponent::Input_Jump(const FInputActionValue& InputActionValue)
{
	if (ARpgCharacter* Character = GetPawn<ARpgCharacter>())
	{
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

void URpgPawnGameplayComponent::BeginPlay()
{
	Super::BeginPlay();
	BindOnActorInitStateChanged(URpgPawnExtensionComponent::Name_ActorFeatureName, FGameplayTag(), false);
	TryToChangeInitState(RpgGameplayTags::InitState_Spawned);
	CheckDefaultInitialization();
}

void URpgPawnGameplayComponent::OnRegister()
{
	Super::OnRegister();
	if (GetPawn<APawn>())
	{
		RegisterInitStateFeature();
	}
}

