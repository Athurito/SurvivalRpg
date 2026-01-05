// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgPawnGameplayComponent.h"

#include "EnhancedInputSubsystems.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "RpgCharacter.h"
#include "RpgPawnData.h"
#include "RpgPawnExtensionComponent.h"
#include "SurvivalRpg/GameplayTags/GameplayTags.h"
#include "SurvivalRpg/Input/RpgInputComponent.h"
#include "SurvivalRpg/Input/RpgInputConfig.h"
#include "UserSettings/EnhancedInputUserSettings.h"

namespace RpgCharacter
{
	static constexpr float LookYawRate = 300.0f;
	static constexpr float LookPitchRate = 165.0f;
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
	if (!Pawn)
	{
		return;
	}

	const APlayerController* PC = GetController<APlayerController>();
	check(PC);

	const ULocalPlayer* LP = Cast<ULocalPlayer>(PC->GetLocalPlayer());
	check(LP);

	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	check(Subsystem);

	Subsystem->ClearAllMappings();

	if (const URpgPawnExtensionComponent* PawnExtComp = URpgPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
	{
		if (const URpgPawnData* PawnData = PawnExtComp->GetPawnData<URpgPawnData>())
		{
			if (const URpgInputConfig* InputConfig = PawnData->InputConfig)
			{
				// for (const FInputMappingContextAndPriority& Mapping : DefaultInputMappings)
				// {
				// 	if (UInputMappingContext* IMC = Mapping.InputMapping.LoadSynchronous())
				// 	{
				// 		if (Mapping.bRegisterWithSettings)
				// 		{
				// 			if (UEnhancedInputUserSettings* Settings = Subsystem->GetUserSettings())
				// 			{
				// 				Settings->RegisterInputMappingContext(IMC);
				// 			}
				// 			
				// 			FModifyContextOptions Options = {};
				// 			Options.bIgnoreAllPressedKeysUntilRelease = false;
				// 			// Actually add the config to the local player							
				// 			Subsystem->AddMappingContext(IMC, Mapping.Priority, Options);
				// 		}
				// 	}
				//}

				// The Lyra Input Component has some additional functions to map Gameplay Tags to an Input Action.
				// If you want this functionality but still want to change your input component class, make it a subclass
				// of the ULyraInputComponent or modify this component accordingly.
				URpgInputComponent* LyraIC = Cast<URpgInputComponent>(PlayerInputComponent);
				if (ensureMsgf(LyraIC, TEXT("Unexpected Input Component class! The Gameplay Abilities will not be bound to their inputs. Change the input component to ULyraInputComponent or a subclass of it.")))
				{
					// Add the key mappings that may have been set by the player
					LyraIC->AddInputMappings(InputConfig, Subsystem);

					// This is where we actually bind and input action to a gameplay tag, which means that Gameplay Ability Blueprints will
					// be triggered directly by these input actions Triggered events. 
					TArray<uint32> BindHandles;
					LyraIC->BindAbilityActions(InputConfig, this, &ThisClass::Input_AbilityInputTagPressed, &ThisClass::Input_AbilityInputTagReleased, /*out*/ BindHandles);

					LyraIC->BindNativeAction(InputConfig, RpgGameplayTags::InputTag_Move, ETriggerEvent::Triggered, this, &ThisClass::Input_Move, /*bLogIfNotFound=*/ false);
					LyraIC->BindNativeAction(InputConfig, RpgGameplayTags::InputTag_Look_Mouse, ETriggerEvent::Triggered, this, &ThisClass::Input_LookMouse, /*bLogIfNotFound=*/ false);
					LyraIC->BindNativeAction(InputConfig, RpgGameplayTags::InputTag_Look_Stick, ETriggerEvent::Triggered, this, &ThisClass::Input_LookStick, /*bLogIfNotFound=*/ false);
					LyraIC->BindNativeAction(InputConfig, RpgGameplayTags::InputTag_Crouch, ETriggerEvent::Triggered, this, &ThisClass::Input_Crouch, /*bLogIfNotFound=*/ false);
					LyraIC->BindNativeAction(InputConfig, RpgGameplayTags::InputTag_AutoRun, ETriggerEvent::Triggered, this, &ThisClass::Input_AutoRun, /*bLogIfNotFound=*/ false);
				}
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


// Called when the game starts
void URpgPawnGameplayComponent::BeginPlay()
{
	Super::BeginPlay();
}

