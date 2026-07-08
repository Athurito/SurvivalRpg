// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgPawnGameplayComponent.h"

#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "RpgCharacter.h"
#include "RpgPawnData.h"
#include "RpgPawnExtensionComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Controller.h"
#include "SurvivalRpg/Camera/RpgCameraComponent.h"
#include "SurvivalRpg/Camera/RpgCameraMode.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerGameplayInputRouterComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Input/RpgInputComponent.h"
#include "UserSettings/EnhancedInputUserSettings.h"

namespace RpgCharacter
{
	static constexpr float LookYawRate = 300.0f;
	static constexpr float LookPitchRate = 165.0f;
}
const FName URpgPawnGameplayComponent::NAME_BindInputsNow = FName("BindInputsNow");
const FName URpgPawnGameplayComponent::Name_ActorFeatureName = FName("RpgPawnGameplayComponent");



// Sets default values for this component's properties
URpgPawnGameplayComponent::URpgPawnGameplayComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;
	AbilityCameraMode = nullptr;
	bReadyToBindInputs = false;
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

		if (URpgInputComponent* RpgIC = Cast<URpgInputComponent>(Pawn->InputComponent))
		{
			for (auto& Entry : AdditionalInputConfigBindHandles)
			{
				RpgIC->RemoveBinds(Entry.Value);
			}
		}
	}

	AdditionalInputConfigBindHandles.Reset();
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
		ARpgPlayerState* PlayerState = GetPlayerState<ARpgPlayerState>();

		if (!Pawn || !PlayerState) return;

		const URpgPawnData* PawnData = nullptr;
		
		if (URpgPawnExtensionComponent* PawnExt = URpgPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
		{
			PawnData = PawnExt->GetPawnData<URpgPawnData>();
			PawnExt->InitializeAbilitySystemComponent(PlayerState->GetRpgAbilitySystemComponent(), PlayerState);
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

	if (bReadyToBindInputs)
	{
		return;
	}

	const APlayerController* PC = GetController<APlayerController>();
	if (!PC)
	{
		return;
	}

	ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!InputSubsystem)
	{
		return;
	}
	
	if (URpgPawnExtensionComponent* PawnExt = URpgPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
	{
		if (const URpgPawnData* PawnData = PawnExt->GetPawnData<URpgPawnData>())
		{
			for (const FRpgInputMappingContextAndPriority& Mapping : DefaultInputMappings)
			{
				if (UInputMappingContext* IMC = Mapping.InputMapping.LoadSynchronous())
				{
					if (Mapping.bRegisterWithSettings)
					{
						if (UEnhancedInputUserSettings* Settings = InputSubsystem->GetUserSettings())
						{
							Settings->RegisterInputMappingContext(IMC);
						}
					}

					FModifyContextOptions Options;
					Options.bIgnoreAllPressedKeysUntilRelease = false;
					InputSubsystem->AddMappingContext(IMC, Mapping.Priority, Options);
				}
			}

			if (const URpgInputConfig* InputConfig = PawnData->InputConfig)
			{
				URpgInputComponent* RpgIC = Cast<URpgInputComponent>(PlayerInputComponent);
				if (ensureMsgf(RpgIC, TEXT("Unexpected Input Component class! The Gameplay Abilities will not be bound to their inputs. Change the input component to URpgInputComponent or a subclass of it.")))
				{
					RpgIC->AddInputMappings(InputConfig, InputSubsystem);
				
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

					BindRoutedGameplayHotkeys(InputConfig, RpgIC);
				}
			}
		}
	}

	bReadyToBindInputs = true;
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(const_cast<APlayerController*>(PC), NAME_BindInputsNow);
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(const_cast<APawn*>(Pawn), NAME_BindInputsNow);
}

void URpgPawnGameplayComponent::AddAdditionalInputConfig(const URpgInputConfig* InputConfig)
{
	if (!InputConfig)
	{
		return;
	}

	if (AdditionalInputConfigBindHandles.Contains(InputConfig))
	{
		return;
	}

	APawn* Pawn = GetPawn<APawn>();
	const APlayerController* PC = GetController<APlayerController>();
	if (!Pawn || !PC || !PC->GetLocalPlayer())
	{
		return;
	}

	URpgInputComponent* RpgIC = Cast<URpgInputComponent>(Pawn->InputComponent);
	if (ensureMsgf(RpgIC, TEXT("Unexpected Input Component class! Ability inputs from the additional config will not be bound.")))
	{
		TArray<uint32> BindHandles;
		RpgIC->BindAbilityActions(InputConfig, this, &ThisClass::Input_AbilityInputTagPressed, &ThisClass::Input_AbilityInputTagReleased, BindHandles);
		BindRoutedGameplayHotkeys(InputConfig, RpgIC, &BindHandles);
		AdditionalInputConfigBindHandles.Add(InputConfig, MoveTemp(BindHandles));
	}
}

void URpgPawnGameplayComponent::RemoveAdditionalInputConfig(const URpgInputConfig* InputConfig)
{
	if (!InputConfig)
	{
		return;
	}

	TArray<uint32>* BindHandles = AdditionalInputConfigBindHandles.Find(InputConfig);
	if (!BindHandles)
	{
		return;
	}

	if (APawn* Pawn = GetPawn<APawn>())
	{
		if (URpgInputComponent* RpgIC = Cast<URpgInputComponent>(Pawn->InputComponent))
		{
			RpgIC->RemoveBinds(*BindHandles);
		}
	}

	AdditionalInputConfigBindHandles.Remove(InputConfig);
}

bool URpgPawnGameplayComponent::IsReadyToBindInputs() const
{
	return bReadyToBindInputs;
}

void URpgPawnGameplayComponent::Input_AbilityInputTagPressed(FGameplayTag InputTag)
{
	if (IsRoutedGameplayHotkeyTag(InputTag))
	{
		Input_GameplayHotkeyPressed(InputTag);
		return;
	}

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
	if (IsRoutedGameplayHotkeyTag(InputTag))
	{
		Input_GameplayHotkeyReleased(InputTag);
		return;
	}

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

void URpgPawnGameplayComponent::Input_GameplayHotkeyPressed(FGameplayTag InputTag)
{
	if (const APawn* Pawn = GetPawn<APawn>())
	{
		if (const ARpgPlayerController* Controller = Cast<ARpgPlayerController>(Pawn->GetController()))
		{
			if (URpgPlayerGameplayInputRouterComponent* InputRouter = Controller->FindComponentByClass<URpgPlayerGameplayInputRouterComponent>())
			{
				InputRouter->HandleGameplayInputPressed(InputTag);
			}
		}
	}
}

void URpgPawnGameplayComponent::Input_GameplayHotkeyReleased(FGameplayTag InputTag)
{
	if (const APawn* Pawn = GetPawn<APawn>())
	{
		if (const ARpgPlayerController* Controller = Cast<ARpgPlayerController>(Pawn->GetController()))
		{
			if (URpgPlayerGameplayInputRouterComponent* InputRouter = Controller->FindComponentByClass<URpgPlayerGameplayInputRouterComponent>())
			{
				InputRouter->HandleGameplayInputReleased(InputTag);
			}
		}
	}
}

void URpgPawnGameplayComponent::BindRoutedGameplayHotkeys(const URpgInputConfig* InputConfig, URpgInputComponent* RpgIC, TArray<uint32>* BindHandles)
{
	if (!InputConfig || !RpgIC)
	{
		return;
	}

	const FGameplayTag RoutedHotkeys[] =
	{
		RpgGameplayTags::InputTag_ActionBar_Slot_1,
		RpgGameplayTags::InputTag_ActionBar_Slot_2,
		RpgGameplayTags::InputTag_ActionBar_Slot_3,
		RpgGameplayTags::InputTag_ActionBar_Slot_4,
		RpgGameplayTags::InputTag_ActionBar_Slot_5,
		RpgGameplayTags::InputTag_ActionBar_Slot_6,
		RpgGameplayTags::InputTag_ActionBar_Slot_7,
		RpgGameplayTags::InputTag_ActionBar_Slot_8,
		RpgGameplayTags::InputTag_Weapon_Ability_1,
		RpgGameplayTags::InputTag_Weapon_Ability_2,
		RpgGameplayTags::InputTag_Weapon_Ability_3
	};

	for (const FGameplayTag& InputTag : RoutedHotkeys)
	{
		if (BindHandles)
		{
			RpgIC->BindNativeActionWithTag(InputConfig, InputTag, ETriggerEvent::Started, this, &ThisClass::Input_GameplayHotkeyPressed, *BindHandles, /*bLogIfNotFound=*/ false);
			RpgIC->BindNativeActionWithTag(InputConfig, InputTag, ETriggerEvent::Completed, this, &ThisClass::Input_GameplayHotkeyReleased, *BindHandles, /*bLogIfNotFound=*/ false);
			RpgIC->BindNativeActionWithTag(InputConfig, InputTag, ETriggerEvent::Canceled, this, &ThisClass::Input_GameplayHotkeyReleased, *BindHandles, /*bLogIfNotFound=*/ false);
		}
		else
		{
			RpgIC->BindNativeActionWithTag(InputConfig, InputTag, ETriggerEvent::Started, this, &ThisClass::Input_GameplayHotkeyPressed, /*bLogIfNotFound=*/ false);
			RpgIC->BindNativeActionWithTag(InputConfig, InputTag, ETriggerEvent::Completed, this, &ThisClass::Input_GameplayHotkeyReleased, /*bLogIfNotFound=*/ false);
			RpgIC->BindNativeActionWithTag(InputConfig, InputTag, ETriggerEvent::Canceled, this, &ThisClass::Input_GameplayHotkeyReleased, /*bLogIfNotFound=*/ false);
		}
	}
}

bool URpgPawnGameplayComponent::IsRoutedGameplayHotkeyTag(FGameplayTag InputTag)
{
	return InputTag == RpgGameplayTags::InputTag_ActionBar_Slot_1
		|| InputTag == RpgGameplayTags::InputTag_ActionBar_Slot_2
		|| InputTag == RpgGameplayTags::InputTag_ActionBar_Slot_3
		|| InputTag == RpgGameplayTags::InputTag_ActionBar_Slot_4
		|| InputTag == RpgGameplayTags::InputTag_ActionBar_Slot_5
		|| InputTag == RpgGameplayTags::InputTag_ActionBar_Slot_6
		|| InputTag == RpgGameplayTags::InputTag_ActionBar_Slot_7
		|| InputTag == RpgGameplayTags::InputTag_ActionBar_Slot_8
		|| InputTag == RpgGameplayTags::InputTag_Weapon_Ability_1
		|| InputTag == RpgGameplayTags::InputTag_Weapon_Ability_2
		|| InputTag == RpgGameplayTags::InputTag_Weapon_Ability_3;
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

