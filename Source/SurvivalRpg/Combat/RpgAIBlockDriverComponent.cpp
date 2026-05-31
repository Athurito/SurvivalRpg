#include "RpgAIBlockDriverComponent.h"

#include "AbilitySystemGlobals.h"
#include "TimerManager.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

URpgAIBlockDriverComponent::URpgAIBlockDriverComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void URpgAIBlockDriverComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	CachedHealthComponent = URpgHealthComponent::FindHealthComponent(Owner);
	if (CachedHealthComponent)
	{
		CachedHealthComponent->OnHealthChanged.AddDynamic(this, &ThisClass::HandleHealthChanged);
	}
}

void URpgAIBlockDriverComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReleaseBlockTimerHandle);
	}

	ReleaseBlock();

	if (CachedHealthComponent)
	{
		CachedHealthComponent->OnHealthChanged.RemoveDynamic(this, &ThisClass::HandleHealthChanged);
		CachedHealthComponent = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void URpgAIBlockDriverComponent::HandleHealthChanged(URpgHealthComponent* HealthComponent, float OldValue, float NewValue, AActor* Instigator)
{
	if (!HealthComponent || HealthComponent->IsDeadOrDying() || NewValue >= OldValue)
	{
		return;
	}

	if (Instigator == GetOwner())
	{
		return;
	}

	TryStartBlock();
}

void URpgAIBlockDriverComponent::TryStartBlock()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || bBlockInputHeld)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	if ((Now - LastBlockStartTime) < BlockCooldown)
	{
		return;
	}

	if (FMath::FRand() > BlockChanceOnDamage)
	{
		return;
	}

	URpgAbilitySystemComponent* ASC = Cast<URpgAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner));
	if (!ASC)
	{
		return;
	}

	LastBlockStartTime = Now;
	bBlockInputHeld = true;

	ASC->AbilityInputTagPressed(RpgGameplayTags::InputTag_Weapon_Block);
	ASC->ProcessAbilityInput(0.0f, false);

	World->GetTimerManager().SetTimer(
		ReleaseBlockTimerHandle,
		this,
		&ThisClass::ReleaseBlock,
		FMath::Max(0.0f, BlockHoldDuration),
		false);
}

void URpgAIBlockDriverComponent::ReleaseBlock()
{
	if (!bBlockInputHeld)
	{
		return;
	}

	bBlockInputHeld = false;

	if (AActor* Owner = GetOwner())
	{
		if (URpgAbilitySystemComponent* ASC = Cast<URpgAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner)))
		{
			ASC->AbilityInputTagReleased(RpgGameplayTags::InputTag_Weapon_Block);
			ASC->ProcessAbilityInput(0.0f, false);
		}
	}
}
