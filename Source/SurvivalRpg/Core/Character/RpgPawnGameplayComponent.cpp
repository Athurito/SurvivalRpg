// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgPawnGameplayComponent.h"

#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "BasePawnData.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "RpgCharacter.h"
#include "RpgPawnExtensionComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
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
		if (!Manager->HasFeatureReachedInitState(Pawn, URpgPawnExtensionComponent::Name_ActorFeatureName, RpgGameplayTags::InitState_DataAvailable))
		{
			return false;
		}

		const ARpgPlayerState* PlayerState = GetPlayerState<ARpgPlayerState>();
		if (!PlayerState)
		{
			return false;
		}

		return (PlayerState->GetRpgAbilitySystemComponent() != nullptr);
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
		ARpgPlayerState* PS = GetPlayerState<ARpgPlayerState>();

		if (!Pawn || !PS) return;
		
		if (URpgPawnExtensionComponent* PawnExt = URpgPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
		{
			URpgAbilitySystemComponent* AbilitySystemComponent = PS->GetRpgAbilitySystemComponent();
			if (!AbilitySystemComponent)
			{
				return;
			}

			PawnExt->InitializeAbilitySystemComponent(AbilitySystemComponent, PS);
			GrantPawnDataAbilitySets(AbilitySystemComponent, PawnExt->GetPawnData<UBasePawnData>(), Pawn);
			ResetCurrentHealthToMaxHealth(AbilitySystemComponent);
		}
		
		if (APlayerController* PC = GetController<APlayerController>())
		{
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
				URpgInputComponent* RpgIC = Cast<URpgInputComponent>(PlayerInputComponent);
				if (ensureMsgf(RpgIC, TEXT("Unexpected Input Component class! The Gameplay Abilities will not be bound to their inputs. Change the input component to ULyraInputComponent or a subclass of it.")))
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
				}
			}
		}
	}
}

void URpgPawnGameplayComponent::Input_AbilityInputTagPressed(FGameplayTag InputTag)
{
	ARpgPlayerState* PlayerState = GetPlayerState<ARpgPlayerState>();
	if (PlayerState == nullptr)
	{
		return;
	}

	if (URpgAbilitySystemComponent* AbilitySystemComponent = PlayerState->GetRpgAbilitySystemComponent())
	{
		AbilitySystemComponent->ActivateAbilitiesByInputTag(InputTag, true);
	}
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
	RemovePawnDataAbilitySets();
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

void URpgPawnGameplayComponent::GrantPawnDataAbilitySets(URpgAbilitySystemComponent* AbilitySystemComponent, const UBasePawnData* PawnData, APawn* Pawn)
{
	if (!AbilitySystemComponent || !PawnData || !Pawn)
	{
		return;
	}

	if (!AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return;
	}

	if (GrantedAbilitySystemComponent == AbilitySystemComponent && GrantedPawnAbilitySets.Num() > 0)
	{
		return;
	}

	if (GrantedAbilitySystemComponent && GrantedAbilitySystemComponent != AbilitySystemComponent)
	{
		RemovePawnDataAbilitySets();
	}

	for (const TObjectPtr<const URpgAbilitySet>& AbilitySet : PawnData->AbilitySets)
	{
		if (!AbilitySet)
		{
			continue;
		}

		FRpgPawnGameplayAbilitySetGrant& GrantedSet = GrantedPawnAbilitySets.AddDefaulted_GetRef();
		GrantedSet.AbilitySet = AbilitySet;
		AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, &GrantedSet.GrantedHandles, Pawn);
	}

	GrantedAbilitySystemComponent = AbilitySystemComponent;
}

void URpgPawnGameplayComponent::ResetCurrentHealthToMaxHealth(URpgAbilitySystemComponent* AbilitySystemComponent) const
{
	if (!AbilitySystemComponent || !AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return;
	}

	const URpgHealthSet* HealthSet = AbilitySystemComponent->GetSet<URpgHealthSet>();
	if (!HealthSet)
	{
		return;
	}

	// Startup runtime health should be derived after all currently-known stat sources have been applied.
	AbilitySystemComponent->SetNumericAttributeBase(URpgHealthSet::GetHealthAttribute(), HealthSet->GetMaxHealth());
}

void URpgPawnGameplayComponent::RemovePawnDataAbilitySets()
{
	if (GrantedAbilitySystemComponent && GrantedAbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		for (FRpgPawnGameplayAbilitySetGrant& GrantedSet : GrantedPawnAbilitySets)
		{
			GrantedSet.GrantedHandles.TakeFromAbilitySystem(GrantedAbilitySystemComponent);
		}
	}

	GrantedPawnAbilitySets.Reset();
	GrantedAbilitySystemComponent = nullptr;
}

void URpgPawnGameplayComponent::HandleAbilitySystemUninitialized()
{
	RemovePawnDataAbilitySets();
}

