#include "RpgAbilityTask_ProjectileShot.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgStaminaSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Combat/RpgCombatProjectile.h"
#include "SurvivalRpg/Equipment/RpgRangedWeaponInstance.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/SurvivalRpg.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgAbilityTask_ProjectileShot)

namespace RpgProjectileShotTask
{
	/** Shot identities this process hands out; they only need to be distinct per shooter. */
	int32 NextShotId = 0;
}

URpgAbilityTask_ProjectileShot::URpgAbilityTask_ProjectileShot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

URpgAbilityTask_ProjectileShot* URpgAbilityTask_ProjectileShot::StartProjectileShot(
	UGameplayAbility* OwningAbility,
	const FRpgProjectileShotDefinition& ShotDefinition,
	const ERpgProjectileAmmunitionPolicy InAmmunitionPolicy)
{
	URpgAbilityTask_ProjectileShot* Task = NewAbilityTask<URpgAbilityTask_ProjectileShot>(OwningAbility);
	Task->Definition = ShotDefinition;
	Task->AmmunitionPolicy = InAmmunitionPolicy;
	return Task;
}

URpgAbilityTask_ProjectileShot* URpgAbilityTask_ProjectileShot::StartProjectileShotFromWeapon(
	UGameplayAbility* OwningAbility,
	const ERpgProjectileAmmunitionPolicy InAmmunitionPolicy)
{
	URpgAbilityTask_ProjectileShot* Task = NewAbilityTask<URpgAbilityTask_ProjectileShot>(OwningAbility);
	Task->AmmunitionPolicy = InAmmunitionPolicy;
	Task->bDefinitionFromWeapon = true;
	return Task;
}

void URpgAbilityTask_ProjectileShot::Activate()
{
	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	const FGameplayAbilitySpec* Spec = ASC && Ability ? ASC->FindAbilitySpecFromHandle(Ability->GetCurrentAbilitySpecHandle()) : nullptr;
	if (!ActorInfo || !ASC || !Spec || !Cast<APawn>(GetAvatarActor()))
	{
		FinishCancelled(ERpgProjectileShotResult::InvalidShot);
		return;
	}

	if (bDefinitionFromWeapon)
	{
		const URpgRangedWeaponInstance* Weapon = Cast<URpgRangedWeaponInstance>(Ability->GetCurrentSourceObject());
		if (!Weapon)
		{
			FinishCancelled(ERpgProjectileShotResult::InvalidShot);
			return;
		}
		Definition = Weapon->GetShotDefinition();
	}
	TArray<FText> DefinitionErrors;
	RpgProjectileShot::ValidateDefinition(Definition, DefinitionErrors);
	if (!DefinitionErrors.IsEmpty())
	{
		FinishCancelled(ERpgProjectileShotResult::InvalidShot);
		return;
	}

	CapturedASC = ASC;
	CapturedSpec = Spec->Handle;
	CapturedActivationKey = Ability->GetCurrentActivationInfo().GetActivationPredictionKey();
	bAuthority = ActorInfo->IsNetAuthority();
	bLocallyControlled = ActorInfo->IsLocallyControlled();
	StartSeconds = GetWorld()->GetTimeSeconds();
	// Every value this activation uses is fixed here; a passive gained while drawing applies to the next shot.
	Tuning = RpgProjectileShot::ResolveTuning(Definition, *Spec, *ActorInfo);

	// Nothing to draw with: refuse before any presentation starts. The release checks again.
	const ERpgProjectileShotResult Resources = RpgProjectileShot::CheckResources(Definition, AmmunitionPolicy, Tuning, ASC);
	if (Resources == ERpgProjectileShotResult::NoAmmunition)
	{
		FinishCancelled(Resources);
		return;
	}

	// Bind before anything can end the shot: an On Nocked handler or an early release may resolve it synchronously, and
	// the end releases these bindings.
	if (bLocallyControlled)
	{
		InputReleasedHandle = ASC->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::InputReleased, CapturedSpec, CapturedActivationKey)
			.AddUObject(this, &ThisClass::HandleInputReleased);
	}
	else if (bAuthority)
	{
		// A remote owner's release travels behind its activation as target data.
		TargetDataHandle = ASC->AbilityTargetDataSetDelegate(CapturedSpec, CapturedActivationKey)
			.AddUObject(this, &ThisClass::HandleReleaseTargetData);
		if (Definition.MaxHoldSeconds > 0.0f)
		{
			GetWorld()->GetTimerManager().SetTimer(ServerTimeoutHandle, this, &ThisClass::HandleServerTimeout, Definition.MaxHoldSeconds, false);
		}
	}

	if (Tuning.MinimumNockSeconds > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(NockTimerHandle, this, &ThisClass::HandleNockTimer, Tuning.MinimumNockSeconds, false);
	}
	else
	{
		HandleNockTimer();
	}

	// A release that reached the server before this activation did resolves now.
	if (!bResolved && !IsFinished() && TargetDataHandle.IsValid())
	{
		ASC->CallReplicatedTargetDataDelegatesIfSet(CapturedSpec, CapturedActivationKey);
	}
}

void URpgAbilityTask_ProjectileShot::TickTask(const float DeltaTime)
{
	Super::TickTask(DeltaTime);
	if (bResolved || !bAuthority || !bNocked || Tuning.HoldStaminaPerSecond <= 0.0f || !FMath::IsFinite(DeltaTime) || DeltaTime <= 0.0f)
	{
		return;
	}

	UAbilitySystemComponent* ASC = CapturedASC.Get();
	if (!ASC || !MatchesActivation())
	{
		FinishCancelled(ERpgProjectileShotResult::Interrupted);
		return;
	}
	bool bFound = false;
	const float Stamina = ASC->GetGameplayAttributeValue(URpgStaminaSet::GetStaminaAttribute(), bFound);
	if (!bFound || !FMath::IsFinite(Stamina))
	{
		FinishCancelled(ERpgProjectileShotResult::InvalidShot);
		return;
	}

	const float Drain = FMath::Min(Stamina, Tuning.HoldStaminaPerSecond * DeltaTime);
	if (Drain > 0.0f)
	{
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(Definition.StaminaChangeEffect, Ability->GetAbilityLevel(), ASC->MakeEffectContext());
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(RpgGameplayTags::Data_StaminaDelta, -Drain);
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}
	// Exhaustion lowers the bow; it never fires on its own, and the held input must be released before the next draw.
	if (!bResolved && ASC->GetNumericAttribute(URpgStaminaSet::GetStaminaAttribute()) <= 0.0f)
	{
		if (URpgAbilitySystemComponent* RpgASC = Cast<URpgAbilitySystemComponent>(ASC))
		{
			RpgASC->SuppressAbilityInputUntilRelease(CapturedSpec);
		}
		FinishCancelled(ERpgProjectileShotResult::Exhausted);
	}
}

void URpgAbilityTask_ProjectileShot::ReleaseShot()
{
	if (bLocallyControlled)
	{
		HandleInputReleased();
	}
}

float URpgAbilityTask_ProjectileShot::GetDrawFraction() const
{
	return bResolved ? 0.0f : RpgProjectileShot::GetDrawFraction(static_cast<float>(GetElapsedSeconds()), Tuning);
}

bool URpgAbilityTask_ProjectileShot::IsNocked() const
{
	return bNocked;
}

void URpgAbilityTask_ProjectileShot::HandleInputReleased()
{
	if (bResolved || !MatchesActivation())
	{
		return;
	}
	if (UAbilitySystemComponent* ASC = CapturedASC.Get())
	{
		ASC->ConsumeGenericReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, CapturedSpec, CapturedActivationKey);
	}

	const FRpgProjectileShotRelease Release = MakeLocalRelease();
	if (bAuthority)
	{
		ResolveRelease(Release);
		return;
	}

	// The release shares the activation's key, so the server reads it with the activation it belongs to.
	UAbilitySystemComponent* ASC = CapturedASC.Get();
	FScopedPredictionWindow ScopedPrediction(ASC, true);
	FRpgProjectileShotTargetData* Data = new FRpgProjectileShotTargetData();
	Data->Release = Release;
	ASC->CallServerSetReplicatedTargetData(CapturedSpec, CapturedActivationKey,
		FGameplayAbilityTargetDataHandle(Data), FGameplayTag(), ASC->ScopedPredictionKey);
	PredictRelease(Release);
}

void URpgAbilityTask_ProjectileShot::HandleReleaseTargetData(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag)
{
	// Hold our own handle while consuming the ASC's cached entry.
	const FGameplayAbilityTargetDataHandle LocalData = Data;
	if (UAbilitySystemComponent* ASC = CapturedASC.Get())
	{
		ASC->ConsumeClientReplicatedTargetData(CapturedSpec, CapturedActivationKey);
	}
	if (bResolved)
	{
		// A duplicate release: the first one already decided this shot.
		return;
	}
	if (LocalData.Num() != 1 || !LocalData.Get(0) || LocalData.Get(0)->GetScriptStruct() != FRpgProjectileShotTargetData::StaticStruct())
	{
		FinishCancelled(ERpgProjectileShotResult::InvalidShot);
		return;
	}
	ResolveRelease(static_cast<const FRpgProjectileShotTargetData*>(LocalData.Get(0))->Release);
}

void URpgAbilityTask_ProjectileShot::HandleNockTimer()
{
	if (bResolved || bNocked)
	{
		return;
	}
	bNocked = true;
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnNocked.Broadcast();
	}
}

void URpgAbilityTask_ProjectileShot::HandleServerTimeout()
{
	if (!bResolved)
	{
		FinishCancelled(ERpgProjectileShotResult::TimedOut);
	}
}

FRpgProjectileShotRelease URpgAbilityTask_ProjectileShot::MakeLocalRelease() const
{
	FRpgProjectileShotRelease Release;
	Release.DrawSeconds = static_cast<float>(GetElapsedSeconds());
	Release.ShotId = ++RpgProjectileShotTask::NextShotId;

	// The camera gives the aim; an NPC or a pawn without a player camera aims with its controller or its eyes.
	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	const APawn* Pawn = Cast<APawn>(GetAvatarActor());
	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	if (APlayerController* PlayerController = ActorInfo ? ActorInfo->PlayerController.Get() : nullptr)
	{
		PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	else if (const AController* Controller = Pawn ? Pawn->GetController() : nullptr)
	{
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	else if (Pawn)
	{
		Pawn->GetActorEyesViewPoint(ViewLocation, ViewRotation);
	}
	Release.AimOrigin = ViewLocation;
	Release.AimDirection = ViewRotation.Vector();
	return Release;
}

void URpgAbilityTask_ProjectileShot::ResolveRelease(const FRpgProjectileShotRelease& Release)
{
	if (bResolved || !bAuthority)
	{
		return;
	}
	bResolved = true;
	ReleaseBindings();
	if (!MatchesActivation() || !FMath::IsFinite(Release.DrawSeconds) || Release.DrawSeconds < 0.0f)
	{
		FinishCancelled(ERpgProjectileShotResult::InvalidShot);
		return;
	}

	// The owner's draw counts only as long as the server saw the shot drawn, plus a little jitter.
	const float DrawSeconds = FMath::Min(Release.DrawSeconds, static_cast<float>(GetElapsedSeconds()) + RpgProjectileShot::DrawTimeTolerance);
	if (DrawSeconds < Tuning.MinimumNockSeconds)
	{
		FinishCancelled(ERpgProjectileShotResult::EarlyRelease);
		return;
	}

	FRpgProjectileShotGeometry Geometry;
	if (!RpgProjectileShot::ResolveGeometry(Definition, Cast<APawn>(GetAvatarActor()), Release.AimOrigin, Release.AimDirection, Geometry))
	{
		FinishCancelled(ERpgProjectileShotResult::InvalidShot);
		return;
	}

	const float DrawFraction = RpgProjectileShot::GetDrawFraction(DrawSeconds, Tuning);
	ARpgCombatProjectile* Projectile = nullptr;
	const ERpgProjectileShotResult CommitResult = RpgProjectileShot::CommitShot(
		Ability, Definition, AmmunitionPolicy, Tuning, DrawFraction, Geometry, Release.ShotId, Projectile);
	if (CommitResult == ERpgProjectileShotResult::Released)
	{
		FinishReleased(DrawFraction, Projectile);
	}
	else
	{
		FinishCancelled(CommitResult);
	}
}

void URpgAbilityTask_ProjectileShot::PredictRelease(const FRpgProjectileShotRelease& Release)
{
	// Presentation only: the owner shows what the server will most likely decide from the same rules and its replicated
	// inventory and stamina. The server's result alone launches and pays.
	bResolved = true;
	ReleaseBindings();
	if (Release.DrawSeconds < Tuning.MinimumNockSeconds)
	{
		FinishCancelled(ERpgProjectileShotResult::EarlyRelease);
		return;
	}
	const ERpgProjectileShotResult Resources = RpgProjectileShot::CheckResources(Definition, AmmunitionPolicy, Tuning, CapturedASC.Get());
	if (Resources != ERpgProjectileShotResult::Released)
	{
		FinishCancelled(Resources);
		return;
	}
	FinishReleased(RpgProjectileShot::GetDrawFraction(Release.DrawSeconds, Tuning), nullptr);
}

void URpgAbilityTask_ProjectileShot::FinishReleased(const float DrawFraction, ARpgCombatProjectile* Projectile)
{
	bResolved = true;
	Result = ERpgProjectileShotResult::Released;
	ReleaseBindings();
	UE_LOG(LogRpgAbilitySystem, Verbose, TEXT("Projectile shot of %s released at draw %.2f (%s)."),
		*GetNameSafe(Ability), DrawFraction, Projectile ? *Projectile->GetName() : TEXT("predicted"));
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnReleased.Broadcast(DrawFraction, Projectile);
	}
	EndTask();
}

void URpgAbilityTask_ProjectileShot::FinishCancelled(const ERpgProjectileShotResult Reason)
{
	bResolved = true;
	Result = Reason;
	ReleaseBindings();
	UE_LOG(LogRpgAbilitySystem, Verbose, TEXT("Projectile shot of %s ended without a projectile: %s."),
		*GetNameSafe(Ability), *UEnum::GetValueAsString(Reason));
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnCancelled.Broadcast(Reason);
	}
	EndTask();
}

double URpgAbilityTask_ProjectileShot::GetElapsedSeconds() const
{
	const UWorld* World = GetWorld();
	return World ? FMath::Max(0.0, World->GetTimeSeconds() - StartSeconds) : 0.0;
}

bool URpgAbilityTask_ProjectileShot::MatchesActivation() const
{
	const UAbilitySystemComponent* ASC = CapturedASC.Get();
	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	if (!ASC || !Ability || !Ability->IsActive() || !ActorInfo || ActorInfo->AbilitySystemComponent.Get() != ASC
		|| Ability->GetCurrentAbilitySpecHandle() != CapturedSpec
		|| Ability->GetCurrentActivationInfo().GetActivationPredictionKey() != CapturedActivationKey)
	{
		return false;
	}
	const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(CapturedSpec);
	return Spec && Spec->IsActive();
}

void URpgAbilityTask_ProjectileShot::ReleaseBindings()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NockTimerHandle);
		World->GetTimerManager().ClearTimer(ServerTimeoutHandle);
	}
	if (UAbilitySystemComponent* ASC = CapturedASC.Get())
	{
		if (InputReleasedHandle.IsValid())
		{
			ASC->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::InputReleased, CapturedSpec, CapturedActivationKey)
				.Remove(InputReleasedHandle);
		}
		if (TargetDataHandle.IsValid())
		{
			ASC->AbilityTargetDataSetDelegate(CapturedSpec, CapturedActivationKey).Remove(TargetDataHandle);
			ASC->ConsumeClientReplicatedTargetData(CapturedSpec, CapturedActivationKey);
		}
	}
	InputReleasedHandle.Reset();
	TargetDataHandle.Reset();
}

void URpgAbilityTask_ProjectileShot::OnDestroy(const bool bInOwnerFinished)
{
	// An activation that ends before its release, through a dodge, a stagger or the weapon's removal, pays nothing.
	bResolved = true;
	ReleaseBindings();
	Super::OnDestroy(bInOwnerFinished);
}
