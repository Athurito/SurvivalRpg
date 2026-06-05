#include "RpgEnemyHealthIndicatorSourceComponent.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/UI/IndicatorSystem/RpgIndicatorManagerComponent.h"

URpgEnemyHealthIndicatorSourceComponent::URpgEnemyHealthIndicatorSourceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void URpgEnemyHealthIndicatorSourceComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	BoundHealthComponent = URpgHealthComponent::FindHealthComponent(GetOwner());
	if (BoundHealthComponent)
	{
		BoundHealthComponent->OnHealthChanged.AddDynamic(this, &ThisClass::HandleHealthChanged);
		BoundHealthComponent->OnDeathFinished.AddDynamic(this, &ThisClass::HandleDeathFinished);

		if (BoundHealthComponent->GetMaxHealth() > 0.0f
			&& BoundHealthComponent->GetHealth() < BoundHealthComponent->GetMaxHealth()
			&& !BoundHealthComponent->IsDeadOrDying())
		{
			ShowIndicator();
		}
	}
}

void URpgEnemyHealthIndicatorSourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BoundHealthComponent)
	{
		BoundHealthComponent->OnHealthChanged.RemoveDynamic(this, &ThisClass::HandleHealthChanged);
		BoundHealthComponent->OnDeathFinished.RemoveDynamic(this, &ThisClass::HandleDeathFinished);
		BoundHealthComponent = nullptr;
	}

	RemoveIndicators();
	Super::EndPlay(EndPlayReason);
}

void URpgEnemyHealthIndicatorSourceComponent::HandleHealthChanged(
	URpgHealthComponent* HealthComponent,
	float OldValue,
	float NewValue,
	AActor* Instigator)
{
	const bool bLostHealth = NewValue < OldValue;
	const bool bDiscoveredAlreadyInjured = FMath::IsNearlyEqual(NewValue, OldValue)
		&& HealthComponent
		&& HealthComponent->GetMaxHealth() > 0.0f
		&& NewValue < HealthComponent->GetMaxHealth();

	if ((bLostHealth || bDiscoveredAlreadyInjured) && ActiveIndicators.IsEmpty())
	{
		ShowIndicator();
	}
}

void URpgEnemyHealthIndicatorSourceComponent::HandleDeathFinished(AActor* OwningActor)
{
	RemoveIndicators();
}

void URpgEnemyHealthIndicatorSourceComponent::ShowIndicator()
{
	if (IndicatorWidgetClass.IsNull() || !GetOwner() || !GetOwner()->GetRootComponent())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		if (!PlayerController || !PlayerController->IsLocalController())
		{
			continue;
		}

		URpgIndicatorManagerComponent* Manager = URpgIndicatorManagerComponent::GetComponent(PlayerController);
		if (!Manager)
		{
			continue;
		}

		UIndicatorDescriptor* Descriptor = NewObject<UIndicatorDescriptor>(this);
		Descriptor->SetDataObject(GetOwner());
		Descriptor->SetSceneComponent(GetOwner()->GetRootComponent());
		Descriptor->SetIndicatorClass(IndicatorWidgetClass);
		Descriptor->SetProjectionMode(ProjectionMode);
		Descriptor->SetBoundingBoxAnchor(BoundingBoxAnchor);
		Descriptor->SetScreenSpaceOffset(ScreenSpaceOffset);
		Descriptor->SetPriority(Priority);
		Descriptor->SetAutoRemoveWhenIndicatorComponentIsNull(true);

		Manager->AddIndicator(Descriptor);
		ActiveIndicators.Add(Descriptor);
	}
}

void URpgEnemyHealthIndicatorSourceComponent::RemoveIndicators()
{
	for (UIndicatorDescriptor* Descriptor : ActiveIndicators)
	{
		if (Descriptor)
		{
			Descriptor->UnregisterIndicator();
		}
	}
	ActiveIndicators.Reset();
}
