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

void URpgAIBlockDriverComponent::SetBlockOnDamageEnabled(const bool bEnabled)
{
	bBlockOnDamageEnabled = bEnabled;
}

bool URpgAIBlockDriverComponent::HoldBlock(const float Duration)
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return false;
	}
	if (PressBlock(Duration))
	{
		return true;
	}
	ReleaseBlock();
	return false;
}

void URpgAIBlockDriverComponent::HandleHealthChanged(URpgHealthComponent* HealthComponent, float OldValue, float NewValue, AActor* Instigator)
{
	if (!bBlockOnDamageEnabled || !HealthComponent || HealthComponent->IsDeadOrDying() || NewValue >= OldValue)
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
	const UWorld* World = GetWorld();
	if (!World || bBlockInputHeld)
	{
		return;
	}

	if ((World->GetTimeSeconds() - LastBlockStartTime) < BlockCooldown)
	{
		return;
	}

	if (FMath::FRand() > BlockChanceOnDamage)
	{
		return;
	}

	PressBlock(BlockHoldDuration);
}

bool URpgAIBlockDriverComponent::PressBlock(const float Duration)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	URpgAbilitySystemComponent* ASC = Owner
		? Cast<URpgAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
		: nullptr;
	if (!World || !ASC || bBlockInputHeld)
	{
		return false;
	}

	LastBlockStartTime = World->GetTimeSeconds();
	bBlockInputHeld = true;

	ASC->AbilityInputTagPressed(RpgGameplayTags::InputTag_Weapon_Block);
	ASC->ProcessAbilityInput(0.0f, false);

	World->GetTimerManager().SetTimer(
		ReleaseBlockTimerHandle,
		this,
		&ThisClass::ReleaseBlock,
		FMath::Max(UE_KINDA_SMALL_NUMBER, Duration),
		false);
	return ASC->HasMatchingGameplayTag(RpgGameplayTags::State_Blocking);
}

void URpgAIBlockDriverComponent::ReleaseBlock()
{
	if (!bBlockInputHeld)
	{
		return;
	}

	bBlockInputHeld = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReleaseBlockTimerHandle);
	}

	if (AActor* Owner = GetOwner())
	{
		if (URpgAbilitySystemComponent* ASC = Cast<URpgAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner)))
		{
			ASC->AbilityInputTagReleased(RpgGameplayTags::InputTag_Weapon_Block);
			ASC->ProcessAbilityInput(0.0f, false);
		}
	}
}
