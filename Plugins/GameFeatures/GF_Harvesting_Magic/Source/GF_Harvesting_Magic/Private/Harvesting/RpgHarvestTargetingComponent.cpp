#include "Harvesting/RpgHarvestTargetingComponent.h"

#include "AbilitySystem/Abilities/RpgGameplayAbility_Harvest.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "SurvivalRpg/Equipment/RpgEquipmentInstance.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/UI/IndicatorSystem/RpgIndicatorManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgHarvestTargetingComponent)

namespace RpgHarvestTargetingComponent
{
	/** Core input tag of the main-hand primary action; resolved by name because core native tags are not exported. */
	const FGameplayTag& GetPrimaryInputTag()
	{
		static const FGameplayTag PrimaryInputTag = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Weapon.Primary"));
		return PrimaryInputTag;
	}
}

URpgHarvestTargetingComponent::URpgHarvestTargetingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

URpgHarvestTargetingComponent* URpgHarvestTargetingComponent::FindForController(const AController* Controller)
{
	return Controller ? Controller->FindComponentByClass<URpgHarvestTargetingComponent>() : nullptr;
}

void URpgHarvestTargetingComponent::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (World && IsLocallyControlled())
	{
		World->GetTimerManager().SetTimer(
			RefreshTimerHandle,
			this,
			&ThisClass::RefreshPreview,
			1.0f / FMath::Clamp(UpdateRateHz, 1.0f, 60.0f),
			true);
	}
}

void URpgHarvestTargetingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}
	AimingSpecHandle = FGameplayAbilitySpecHandle();
	RemoveTargetIndicator();
	Super::EndPlay(EndPlayReason);
}

void URpgHarvestTargetingComponent::RefreshPreview()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	FRpgHarvestPreview NewPreview;
	if (UAbilitySystemComponent* AbilitySystem = FindAbilitySystem())
	{
		bool bAiming = false;
		const FGameplayAbilitySpec* Spec = FindPreviewSpec(*AbilitySystem, bAiming);
		const URpgGameplayAbility_Harvest* Ability = Spec
			? Cast<URpgGameplayAbility_Harvest>(Spec->GetPrimaryInstance())
			: nullptr;
		if (Spec && !Ability)
		{
			Ability = Cast<URpgGameplayAbility_Harvest>(Spec->Ability);
		}
		if (Ability && AbilitySystem->AbilityActorInfo.IsValid())
		{
			Ability->EvaluateTargets(*Spec, *AbilitySystem->AbilityActorInfo, NewPreview);
			NewPreview.bIsAiming = bAiming;
		}
	}

	const bool bChanged = !NewPreview.IsEquivalent(CurrentPreview);
	CurrentPreview = MoveTemp(NewPreview);
	UpdateTargetIndicator();
	if (bChanged)
	{
		OnPreviewChanged.Broadcast(CurrentPreview);
	}
}

bool URpgHarvestTargetingComponent::GetPrimaryTargetStatus(
	ERpgHarvestTargetStatus& OutStatus,
	int32& OutRemainingSections,
	int32& OutSectionCount,
	int32& OutSectionsToTake) const
{
	OutStatus = ERpgHarvestTargetStatus::None;
	OutRemainingSections = 0;
	OutSectionCount = 0;
	OutSectionsToTake = 0;
	if (CurrentPreview.Targets.IsEmpty())
	{
		return false;
	}

	const FRpgHarvestTargetEvaluation& Target = CurrentPreview.Targets[0];
	const FRpgHarvestResult& Result = Target.Result;
	OutSectionCount = Result.SectionCount;
	OutRemainingSections = Result.IsSuccess()
		? Result.RemainingSections + Result.SectionsTaken
		: Result.RemainingSections;
	OutSectionsToTake = Target.WouldHarvest() ? Result.SectionsTaken : 0;
	switch (Result.Outcome)
	{
	case ERpgHarvestOutcome::Harvested:
		OutStatus = Target.bInReach ? ERpgHarvestTargetStatus::Harvestable : ERpgHarvestTargetStatus::OutOfReach;
		break;
	case ERpgHarvestOutcome::Depleted:
		OutStatus = ERpgHarvestTargetStatus::Depleted;
		break;
	case ERpgHarvestOutcome::WrongTool:
		OutStatus = ERpgHarvestTargetStatus::WrongTool;
		break;
	case ERpgHarvestOutcome::SkillGate:
		OutStatus = ERpgHarvestTargetStatus::SkillLocked;
		break;
	default:
		OutStatus = ERpgHarvestTargetStatus::Unavailable;
		break;
	}
	return true;
}

void URpgHarvestTargetingComponent::UpdateTargetIndicator()
{
	if (IndicatorWidgetClass.IsNull())
	{
		return;
	}

	USceneComponent* TargetComponent = CurrentPreview.Targets.IsEmpty()
		? nullptr
		: CurrentPreview.Targets[0].Hit.GetComponent();
	if (!TargetComponent)
	{
		RemoveTargetIndicator();
		return;
	}
	if (TargetIndicator && TargetIndicator->GetSceneComponent() == TargetComponent)
	{
		return;
	}

	RemoveTargetIndicator();
	URpgIndicatorManagerComponent* IndicatorManager =
		URpgIndicatorManagerComponent::GetComponent(Cast<AController>(GetOwner()));
	if (!IndicatorManager)
	{
		return;
	}

	TargetIndicator = NewObject<UIndicatorDescriptor>(this);
	TargetIndicator->SetDataObject(this);
	TargetIndicator->SetSceneComponent(TargetComponent);
	TargetIndicator->SetIndicatorClass(IndicatorWidgetClass);
	TargetIndicator->SetProjectionMode(ProjectionMode);
	TargetIndicator->SetBoundingBoxAnchor(BoundingBoxAnchor);
	TargetIndicator->SetScreenSpaceOffset(ScreenSpaceOffset);
	TargetIndicator->SetPriority(IndicatorPriority);
	TargetIndicator->SetAutoRemoveWhenIndicatorComponentIsNull(true);
	IndicatorManager->AddIndicator(TargetIndicator);
}

void URpgHarvestTargetingComponent::RemoveTargetIndicator()
{
	if (TargetIndicator)
	{
		if (URpgIndicatorManagerComponent* IndicatorManager =
				URpgIndicatorManagerComponent::GetComponent(Cast<AController>(GetOwner())))
		{
			IndicatorManager->RemoveIndicator(TargetIndicator);
		}
		TargetIndicator = nullptr;
	}
}

void URpgHarvestTargetingComponent::SetAimingAbility(const FGameplayAbilitySpecHandle Handle, const bool bAiming)
{
	if (bAiming)
	{
		AimingSpecHandle = Handle;
	}
	else if (AimingSpecHandle == Handle)
	{
		AimingSpecHandle = FGameplayAbilitySpecHandle();
	}
	RefreshPreview();
}

bool URpgHarvestTargetingComponent::IsLocallyControlled() const
{
	const AController* Controller = Cast<AController>(GetOwner());
	return Controller && Controller->IsLocalController();
}

UAbilitySystemComponent* URpgHarvestTargetingComponent::FindAbilitySystem() const
{
	const AController* Controller = Cast<AController>(GetOwner());
	if (!Controller)
	{
		return nullptr;
	}
	if (UAbilitySystemComponent* AbilitySystem =
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Controller->PlayerState))
	{
		return AbilitySystem;
	}
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Controller->GetPawn());
}

const FGameplayAbilitySpec* URpgHarvestTargetingComponent::FindPreviewSpec(
	UAbilitySystemComponent& AbilitySystem,
	bool& bOutAiming) const
{
	bOutAiming = false;
	if (AimingSpecHandle.IsValid())
	{
		const FGameplayAbilitySpec* AimingSpec = AbilitySystem.FindAbilitySpecFromHandle(AimingSpecHandle);
		if (AimingSpec && Cast<URpgGameplayAbility_Harvest>(AimingSpec->Ability))
		{
			bOutAiming = true;
			return AimingSpec;
		}
	}

	// Always-on preview: the swing bound to the primary input of the active main-hand tool.
	for (const FGameplayAbilitySpec& Spec : AbilitySystem.GetActivatableAbilities())
	{
		const URpgGameplayAbility_Harvest* Ability = Cast<URpgGameplayAbility_Harvest>(Spec.Ability);
		if (!Ability || Ability->IsAimWhileInputHeld() ||
			!Spec.GetDynamicSpecSourceTags().HasTagExact(RpgHarvestTargetingComponent::GetPrimaryInputTag()))
		{
			continue;
		}

		const URpgEquipmentInstance* Equipment = Cast<URpgEquipmentInstance>(Spec.SourceObject.Get());
		const APawn* EquipmentPawn = Equipment ? Equipment->GetPawn() : nullptr;
		if (!EquipmentPawn)
		{
			continue;
		}
		const URpgEquipmentManagerComponent* EquipmentManager =
			EquipmentPawn->FindComponentByClass<URpgEquipmentManagerComponent>();
		if (EquipmentManager &&
			!EquipmentManager->IsEquipmentInstanceActiveForInputTag(Equipment, RpgHarvestTargetingComponent::GetPrimaryInputTag()))
		{
			continue;
		}
		return &Spec;
	}
	return nullptr;
}
