#include "Harvesting/RpgHarvestTargetingComponent.h"

#include "AbilitySystem/Abilities/RpgGameplayAbility_Harvest.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
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

	/** Returns the instance Target addresses within an instanced component, or INDEX_NONE for other components. */
	int32 GetTargetInstanceIndex(const FRpgHarvestTargetEvaluation& Target)
	{
		return Cast<UInstancedStaticMeshComponent>(Target.Hit.GetComponent()) ? Target.Hit.Item : INDEX_NONE;
	}

	/** Presentation summary of one evaluated target; false and empty outputs for no target. */
	bool SummarizeTarget(
		const FRpgHarvestTargetEvaluation* Target,
		ERpgHarvestTargetStatus& OutStatus,
		int32& OutRemainingSections,
		int32& OutSectionCount,
		int32& OutSectionsToTake)
	{
		OutStatus = ERpgHarvestTargetStatus::None;
		OutRemainingSections = 0;
		OutSectionCount = 0;
		OutSectionsToTake = 0;
		if (!Target)
		{
			return false;
		}

		const FRpgHarvestResult& Result = Target->Result;
		OutSectionCount = Result.SectionCount;
		OutRemainingSections = Result.IsSuccess()
			? Result.RemainingSections + Result.SectionsTaken
			: Result.RemainingSections;
		OutSectionsToTake = Target->WouldHarvest() ? Result.SectionsTaken : 0;
		switch (Result.Outcome)
		{
		case ERpgHarvestOutcome::Harvested:
			OutStatus = Target->bInReach ? ERpgHarvestTargetStatus::Harvestable : ERpgHarvestTargetStatus::OutOfReach;
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
		case ERpgHarvestOutcome::Protected:
			OutStatus = ERpgHarvestTargetStatus::Protected;
			break;
		default:
			OutStatus = ERpgHarvestTargetStatus::Unavailable;
			break;
		}
		return true;
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
	RemoveTargetIndicators();
	DestroyAreaMarker();
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
	UpdateTargetIndicators();
	UpdateAreaMarker();
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
	return RpgHarvestTargetingComponent::SummarizeTarget(
		CurrentPreview.Targets.IsEmpty() ? nullptr : &CurrentPreview.Targets[0],
		OutStatus,
		OutRemainingSections,
		OutSectionCount,
		OutSectionsToTake);
}

bool URpgHarvestTargetingComponent::GetTargetStatus(
	const USceneComponent* TargetComponent,
	ERpgHarvestTargetStatus& OutStatus,
	int32& OutRemainingSections,
	int32& OutSectionCount,
	int32& OutSectionsToTake) const
{
	return RpgHarvestTargetingComponent::SummarizeTarget(
		FindTargetEvaluation(TargetComponent),
		OutStatus,
		OutRemainingSections,
		OutSectionCount,
		OutSectionsToTake);
}

const FRpgHarvestTargetEvaluation* URpgHarvestTargetingComponent::FindTargetEvaluation(
	const USceneComponent* TargetComponent) const
{
	return TargetComponent
		? CurrentPreview.Targets.FindByPredicate([TargetComponent](const FRpgHarvestTargetEvaluation& Candidate)
		{
			return Candidate.Hit.GetComponent() == TargetComponent;
		})
		: nullptr;
}

const FRpgHarvestTargetEvaluation* URpgHarvestTargetingComponent::FindIndicatedTarget(
	const UIndicatorDescriptor* Indicator) const
{
	const FRpgHarvestTargetIndicator* Indicated = Indicator
		? TargetIndicators.FindByPredicate([Indicator](const FRpgHarvestTargetIndicator& Candidate)
		{
			return Candidate.Indicator == Indicator;
		})
		: nullptr;
	if (!Indicated)
	{
		return nullptr;
	}

	const USceneComponent* TargetComponent = Indicator->GetSceneComponent();
	return CurrentPreview.Targets.FindByPredicate([TargetComponent, Indicated](const FRpgHarvestTargetEvaluation& Candidate)
	{
		return Candidate.Hit.GetComponent() == TargetComponent &&
			RpgHarvestTargetingComponent::GetTargetInstanceIndex(Candidate) == Indicated->InstanceIndex;
	});
}

bool URpgHarvestTargetingComponent::GetIndicatedTargetStatus(
	const UIndicatorDescriptor* Indicator,
	ERpgHarvestTargetStatus& OutStatus,
	int32& OutRemainingSections,
	int32& OutSectionCount,
	int32& OutSectionsToTake) const
{
	return RpgHarvestTargetingComponent::SummarizeTarget(
		FindIndicatedTarget(Indicator),
		OutStatus,
		OutRemainingSections,
		OutSectionCount,
		OutSectionsToTake);
}

void URpgHarvestTargetingComponent::UpdateTargetIndicators()
{
	using namespace RpgHarvestTargetingComponent;

	if (IndicatorWidgetClass.IsNull())
	{
		return;
	}

	// The primary swing marks its target; a held aim ability marks every target it would hit.
	TArray<const FRpgHarvestTargetEvaluation*, TInlineAllocator<8>> IndicatedTargets;
	for (const FRpgHarvestTargetEvaluation& Target : CurrentPreview.Targets)
	{
		if (Target.Hit.GetComponent())
		{
			IndicatedTargets.Add(&Target);
		}
		if (!CurrentPreview.bIsAiming)
		{
			break;
		}
	}

	URpgIndicatorManagerComponent* IndicatorManager =
		URpgIndicatorManagerComponent::GetComponent(Cast<AController>(GetOwner()));
	for (int32 Index = TargetIndicators.Num() - 1; Index >= 0; --Index)
	{
		const FRpgHarvestTargetIndicator& Indicated = TargetIndicators[Index];
		UIndicatorDescriptor* Indicator = Indicated.Indicator;
		const USceneComponent* IndicatedComponent = Indicator ? Indicator->GetSceneComponent() : nullptr;
		const int32 TargetIndex = IndicatedComponent
			? IndicatedTargets.IndexOfByPredicate([IndicatedComponent, &Indicated](const FRpgHarvestTargetEvaluation* Target)
			{
				return Target->Hit.GetComponent() == IndicatedComponent &&
					GetTargetInstanceIndex(*Target) == Indicated.InstanceIndex;
			})
			: INDEX_NONE;
		if (TargetIndex != INDEX_NONE)
		{
			// Keep the placement current, for example while a resource shrinks with its sections.
			PlaceTargetIndicator(*Indicator, *IndicatedTargets[TargetIndex]);
			IndicatedTargets.RemoveAt(TargetIndex);
			continue;
		}
		if (Indicator && IndicatorManager)
		{
			IndicatorManager->RemoveIndicator(Indicator);
		}
		TargetIndicators.RemoveAt(Index);
	}

	if (!IndicatorManager)
	{
		return;
	}
	for (const FRpgHarvestTargetEvaluation* Target : IndicatedTargets)
	{
		UIndicatorDescriptor* Indicator = NewObject<UIndicatorDescriptor>(this);
		Indicator->SetDataObject(this);
		Indicator->SetIndicatorClass(IndicatorWidgetClass);
		Indicator->SetBoundingBoxAnchor(BoundingBoxAnchor);
		Indicator->SetScreenSpaceOffset(ScreenSpaceOffset);
		Indicator->SetPriority(IndicatorPriority);
		Indicator->SetAutoRemoveWhenIndicatorComponentIsNull(true);
		PlaceTargetIndicator(*Indicator, *Target);

		FRpgHarvestTargetIndicator& Indicated = TargetIndicators.AddDefaulted_GetRef();
		Indicated.Indicator = Indicator;
		Indicated.InstanceIndex = GetTargetInstanceIndex(*Target);
		IndicatorManager->AddIndicator(Indicator);
	}
}

void URpgHarvestTargetingComponent::PlaceTargetIndicator(
	UIndicatorDescriptor& Indicator,
	const FRpgHarvestTargetEvaluation& Target) const
{
	USceneComponent* TargetComponent = Target.Hit.GetComponent();
	Indicator.SetSceneComponent(TargetComponent);

	const UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(TargetComponent);
	FTransform InstanceTransform;
	if (Instances && Instances->GetInstanceTransform(Target.Hit.Item, InstanceTransform, true))
	{
		// The component's bounds span every instance, so the indicator marks the anchor of the targeted instance.
		const UStaticMesh* Mesh = Instances->GetStaticMesh();
		const FBox InstanceBox = Mesh
			? Mesh->GetBounds().GetBox().TransformBy(InstanceTransform)
			: FBox(InstanceTransform.GetLocation(), InstanceTransform.GetLocation());
		Indicator.SetProjectionMode(EActorCanvasProjectionMode::ComponentPoint);
		Indicator.SetWorldPositionOverride(
			InstanceBox.GetCenter() + InstanceBox.GetSize() * (BoundingBoxAnchor - FVector(0.5)));
		return;
	}

	Indicator.ClearWorldPositionOverride();
	Indicator.SetProjectionMode(ProjectionMode);
}

void URpgHarvestTargetingComponent::RemoveTargetIndicators()
{
	URpgIndicatorManagerComponent* IndicatorManager =
		URpgIndicatorManagerComponent::GetComponent(Cast<AController>(GetOwner()));
	for (const FRpgHarvestTargetIndicator& Indicated : TargetIndicators)
	{
		if (Indicated.Indicator && IndicatorManager)
		{
			IndicatorManager->RemoveIndicator(Indicated.Indicator);
		}
	}
	TargetIndicators.Reset();
}

void URpgHarvestTargetingComponent::UpdateAreaMarker()
{
	if (AreaMarkerClass.IsNull())
	{
		return;
	}

	const bool bShowArea = CurrentPreview.bIsAiming && CurrentPreview.bHasArea && CurrentPreview.AreaRadius > 0.0f;
	if (!bShowArea)
	{
		if (AreaMarker)
		{
			AreaMarker->SetActorHiddenInGame(true);
		}
		return;
	}

	UWorld* World = GetWorld();
	if (!AreaMarker && World)
	{
		// One small cosmetic class, loaded on the first held aim and reused afterwards.
		if (UClass* MarkerClass = AreaMarkerClass.LoadSynchronous())
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Owner = GetOwner();
			SpawnParameters.ObjectFlags |= RF_Transient;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AreaMarker = World->SpawnActor<AActor>(MarkerClass, FTransform(CurrentPreview.AimPoint), SpawnParameters);
		}
	}
	if (!AreaMarker)
	{
		return;
	}

	const double RadiusScale = CurrentPreview.AreaRadius / 100.0;
	AreaMarker->SetActorLocation(CurrentPreview.AimPoint);
	AreaMarker->SetActorScale3D(FVector(RadiusScale));
	AreaMarker->SetActorHiddenInGame(false);
}

void URpgHarvestTargetingComponent::DestroyAreaMarker()
{
	if (AreaMarker)
	{
		AreaMarker->Destroy();
		AreaMarker = nullptr;
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
