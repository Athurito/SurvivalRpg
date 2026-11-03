#include "RpgAbilityTask_RangedAim.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Equipment/RpgRangedWeaponInstance.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/SurvivalRpg.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgAbilityTask_RangedAim)

namespace RpgRangedAimTask
{
	const FName Airborne(TEXT("Airborne"));
	const FName Sprint(TEXT("Sprint"));
	const FName ExclusiveAbility(TEXT("ExclusiveAbility"));
	const FName AvatarChanged(TEXT("AvatarChanged"));
}

URpgAbilityTask_RangedAim::URpgAbilityTask_RangedAim(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

URpgAbilityTask_RangedAim* URpgAbilityTask_RangedAim::HoldRangedAim(UGameplayAbility* OwningAbility, const FRpgRangedAimDefinition& AimDefinition)
{
	URpgAbilityTask_RangedAim* Task = NewAbilityTask<URpgAbilityTask_RangedAim>(OwningAbility);
	Task->Definition = AimDefinition;
	return Task;
}

URpgAbilityTask_RangedAim* URpgAbilityTask_RangedAim::HoldRangedAimFromWeapon(UGameplayAbility* OwningAbility)
{
	URpgAbilityTask_RangedAim* Task = NewAbilityTask<URpgAbilityTask_RangedAim>(OwningAbility);
	Task->bDefinitionFromWeapon = true;
	return Task;
}

void URpgAbilityTask_RangedAim::Activate()
{
	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	URpgAbilitySystemComponent* ASC = Cast<URpgAbilitySystemComponent>(AbilitySystemComponent.Get());
	const FGameplayAbilitySpec* Spec = ASC && Ability ? ASC->FindAbilitySpecFromHandle(Ability->GetCurrentAbilitySpecHandle()) : nullptr;
	if (!ActorInfo || !ASC || !Spec)
	{
		EndTask();
		return;
	}
	if (bDefinitionFromWeapon)
	{
		const URpgRangedWeaponInstance* Weapon = Cast<URpgRangedWeaponInstance>(Ability->GetCurrentSourceObject());
		if (!Weapon)
		{
			EndTask();
			return;
		}
		Definition = Weapon->GetAimDefinition();
	}

	CapturedASC = ASC;
	CapturedAvatar = ASC->GetAvatarActor();
	CapturedSpec = Spec->Handle;
	// The cap is fixed for this activation; a passive gained while aiming applies to the next stance.
	const float Tuned = URpgGameplayAbility::GetTunedValueForSpec(*Spec, *ActorInfo, RpgGameplayTags::Ability_Tuning_Ranged_AimMovement,
		Definition.MovementSpeedLimit);
	MovementSpeedLimit = FMath::IsFinite(Tuned) ? FMath::Max(0.0f, Tuned) : FMath::Max(0.0f, Definition.MovementSpeedLimit);
	// A sprint already held when the stance starts does not interrupt it; once it ends, the next sprint does.
	bSprintAtStart = ASC->IsSprintMovementActive();
	CancelledHandle = Ability->OnGameplayAbilityCancelled.AddUObject(this, &ThisClass::HandleAbilityCancelled);
	// A press during a dodge, jump or stagger is kept: the stance begins when that state ends instead of cancelling.
	bWaitingToStart = !FindInterruption().IsNone();
	if (!bWaitingToStart)
	{
		BeginStance();
	}
}

void URpgAbilityTask_RangedAim::BeginStance()
{
	URpgAbilitySystemComponent* ASC = CapturedASC.Get();
	Token = ASC ? ASC->BeginRangedAim(Ability, MovementSpeedLimit, Definition.CameraMode) : 0;
	// A shot that starts drawing on this stance may end the activation before the token is stored here.
	if (Token && IsFinished())
	{
		ReleaseToken();
		return;
	}
	if (!Token)
	{
		EndTask();
	}
}

void URpgAbilityTask_RangedAim::TickTask(const float DeltaTime)
{
	Super::TickTask(DeltaTime);
	if (bWaitingToStart && !bInterrupting)
	{
		const FName Reason = FindInterruption();
		if (Reason.IsNone())
		{
			bWaitingToStart = false;
			const URpgAbilitySystemComponent* ASC = CapturedASC.Get();
			bSprintAtStart = ASC && ASC->IsSprintMovementActive();
			BeginStance();
		}
		else if (Reason == RpgRangedAimTask::AvatarChanged)
		{
			bInterrupting = true;
			if (URpgAbilitySystemComponent* ASC = CapturedASC.Get())
			{
				ASC->CancelAbilityHandle(CapturedSpec);
			}
		}
		return;
	}
	if (!Token || bInterrupting)
	{
		return;
	}
	const URpgAbilitySystemComponent* SprintSource = CapturedASC.Get();
	if (bSprintAtStart && SprintSource && !SprintSource->IsSprintMovementActive())
	{
		bSprintAtStart = false;
	}
	const FName Reason = FindInterruption();
	if (Reason.IsNone())
	{
		return;
	}
	bInterrupting = true;
	UE_LOG(LogRpgAbilitySystem, Verbose, TEXT("Ranged aim of %s interrupted: %s."), *GetNameSafe(Ability), *Reason.ToString());
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnInterrupted.Broadcast(Reason);
	}
	if (URpgAbilitySystemComponent* ASC = CapturedASC.Get())
	{
		ASC->CancelAbilityHandle(CapturedSpec);
	}
}

FName URpgAbilityTask_RangedAim::FindInterruption() const
{
	const URpgAbilitySystemComponent* ASC = CapturedASC.Get();
	const AActor* Avatar = CapturedAvatar.Get();
	// The ability system drops every token when its avatar changes; an ability that survives that must not keep aiming.
	if (!ASC || !Avatar || ASC->GetAvatarActor() != Avatar)
	{
		return RpgRangedAimTask::AvatarChanged;
	}
	if (const ACharacter* Character = Cast<ACharacter>(Avatar))
	{
		const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		if (Movement && Movement->IsFalling())
		{
			return RpgRangedAimTask::Airborne;
		}
	}
	if (!bSprintAtStart && ASC->IsSprintMovementActive())
	{
		return RpgRangedAimTask::Sprint;
	}
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.IsActive() || Spec.Handle == CapturedSpec)
		{
			continue;
		}
		// Instances report their live group: a stagger or death switches itself to blocking after it starts.
		const URpgGameplayAbility* Other = Cast<URpgGameplayAbility>(Spec.GetPrimaryInstance());
		Other = Other ? Other : Cast<URpgGameplayAbility>(Spec.Ability);
		if (Other && Other->GetActivationGroup() != ERpgAbilityActivationGroup::Independent
			&& !Other->GetAssetTags().HasTag(RpgGameplayTags::Ability_Type_RangedAim))
		{
			return RpgRangedAimTask::ExclusiveAbility;
		}
	}
	return NAME_None;
}

void URpgAbilityTask_RangedAim::HandleAbilityCancelled()
{
	// A dodge, stagger or traversal ended the stance while its button is still down: the button must be pressed again.
	URpgAbilitySystemComponent* ASC = CapturedASC.Get();
	const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(CapturedSpec) : nullptr;
	if (Spec && Spec->InputPressed && ASC->AbilityActorInfo.IsValid() && ASC->AbilityActorInfo->IsLocallyControlled())
	{
		ASC->SuppressAbilityInputUntilRelease(CapturedSpec);
	}
	ReleaseToken();
}

void URpgAbilityTask_RangedAim::ReleaseToken()
{
	if (URpgAbilitySystemComponent* ASC = CapturedASC.Get())
	{
		if (Token)
		{
			ASC->EndRangedAim(Ability, Token);
		}
	}
	Token = 0;
}

void URpgAbilityTask_RangedAim::OnDestroy(const bool bInOwnerFinished)
{
	if (Ability && CancelledHandle.IsValid())
	{
		Ability->OnGameplayAbilityCancelled.Remove(CancelledHandle);
	}
	CancelledHandle.Reset();
	ReleaseToken();
	Super::OnDestroy(bInOwnerFinished);
}
