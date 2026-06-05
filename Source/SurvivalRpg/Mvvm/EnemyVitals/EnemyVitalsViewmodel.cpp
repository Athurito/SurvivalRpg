#include "EnemyVitalsViewmodel.h"

#include "AbilitySystemInterface.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"

void UEnemyVitalsViewmodel::BindToActor(AActor* InObservedActor)
{
	if (ObservedActor.Get() == InObservedActor && BoundPawnExtension)
	{
		HandleAbilitySystemInitialized();
		return;
	}

	UnbindFromActor();
	ObservedActor = InObservedActor;

	if (!InObservedActor)
	{
		return;
	}

	BoundPawnExtension = URpgPawnExtensionComponent::FindPawnExtensionComponent(InObservedActor);
	if (BoundPawnExtension)
	{
		BoundPawnExtension->OnAbilitySystemInitialized_RegisterAndCall(
			FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemInitialized));
		BoundPawnExtension->OnAbilitySystemUninitialized_Register(
			FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemUninitialized));
		return;
	}

	if (const IAbilitySystemInterface* AbilitySystemActor = Cast<IAbilitySystemInterface>(InObservedActor))
	{
		BindASC(AbilitySystemActor->GetAbilitySystemComponent());
	}
}

void UEnemyVitalsViewmodel::UnbindFromActor()
{
	if (BoundPawnExtension)
	{
		BoundPawnExtension->OnAbilitySystemInitialized.RemoveAll(this);
		BoundPawnExtension->OnAbilitySystemUninitialized.RemoveAll(this);
		BoundPawnExtension = nullptr;
	}

	BindASC(nullptr);
	ObservedActor.Reset();
}

void UEnemyVitalsViewmodel::BeginDestroy()
{
	UnbindFromActor();

	Super::BeginDestroy();
}

void UEnemyVitalsViewmodel::HandleAbilitySystemInitialized()
{
	if (BoundPawnExtension)
	{
		BindASC(BoundPawnExtension->GetRpgAbilitySystemComponent());
	}
}

void UEnemyVitalsViewmodel::HandleAbilitySystemUninitialized()
{
	BindASC(nullptr);
}
