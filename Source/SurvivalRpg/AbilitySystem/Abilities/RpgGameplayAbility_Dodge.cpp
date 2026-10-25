#include "RpgGameplayAbility_Dodge.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "TimerManager.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGameplayAbility_Dodge)

#define LOCTEXT_NAMESPACE "RpgGameplayAbility_Dodge"

bool FRpgDodgeDirectionTargetData::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	// Eight directions fit in three bits; every value read back is a valid direction.
	uint8 Value = static_cast<uint8>(Direction) & 7;
	Ar.SerializeBits(&Value, 3);
	if (Ar.IsLoading())
	{
		Direction = static_cast<ERpgDodgeDirection>(Value & 7);
	}
	bOutSuccess = !Ar.IsError();
	return true;
}

URpgGameplayAbility_Dodge::URpgGameplayAbility_Dodge(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationGroup = ERpgAbilityActivationGroup::Exclusive_Replaceable;
}

FRpgDodgeRootMotionTuning URpgGameplayAbility_Dodge::ResolveRootMotionTuning(
	FName ProfileName,
	TConstArrayView<FRpgDodgeRootMotionTuning> Tunings,
	float DefaultPlayRate,
	float DefaultTranslationScale)
{
	if (!ProfileName.IsNone())
	{
		for (const FRpgDodgeRootMotionTuning& Tuning : Tunings)
		{
			if (Tuning.ProfileName == ProfileName)
			{
				FRpgDodgeRootMotionTuning Sanitized = Tuning;
				Sanitized.MontagePlayRate = FMath::Max(0.01f, Sanitized.MontagePlayRate);
				Sanitized.TranslationScale = FMath::Max(0.0f, Sanitized.TranslationScale);
				return Sanitized;
			}
		}
	}

	FRpgDodgeRootMotionTuning Fallback;
	Fallback.ProfileName = ProfileName;
	Fallback.MontagePlayRate = FMath::Max(0.01f, DefaultPlayRate);
	Fallback.TranslationScale = FMath::Max(0.0f, DefaultTranslationScale);
	return Fallback;
}

FName URpgGameplayAbility_Dodge::GetDodgeDirectionSectionName(ERpgDodgeDirection Direction)
{
	static const FName SectionNames[] = { TEXT("F"), TEXT("FR"), TEXT("R"), TEXT("BR"), TEXT("B"), TEXT("BL"), TEXT("L"), TEXT("FL") };
	const uint8 Index = static_cast<uint8>(Direction);
	return Index < UE_ARRAY_COUNT(SectionNames) ? SectionNames[Index] : NAME_None;
}

ERpgDodgeDirection URpgGameplayAbility_Dodge::ResolveDodgeDirection(const FVector& MovementInput, const FRotator& Facing)
{
	const FVector Input2D(MovementInput.X, MovementInput.Y, 0.0);
	if (Input2D.SizeSquared() < FMath::Square(0.1))
	{
		// Like a Souls backstep: no input dodges away from where the avatar faces.
		return ERpgDodgeDirection::Backward;
	}
	// Yaw grows clockwise seen from above, so a positive delta lies to the avatar's right.
	const double Delta = FRotator::NormalizeAxis(Input2D.Rotation().Yaw - Facing.Yaw);
	const int32 Sector = (FMath::RoundToInt(Delta / 45.0) % 8 + 8) % 8;
	return static_cast<ERpgDodgeDirection>(Sector);
}

bool URpgGameplayAbility_Dodge::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const APawn* Pawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UPawnMovementComponent* Movement = Pawn ? Pawn->GetMovementComponent() : nullptr;
	if (bRequiresGround && Movement && !Movement->IsMovingOnGround())
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(RpgGameplayTags::Ability_ActivateFail_TagsBlocked);
		}
		return false;
	}
	return true;
}

void URpgGameplayAbility_Dodge::BeginDodge()
{
	UAbilitySystemComponent* AbilitySystem = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ResolveDodgeProfileForActivation() || !AbilitySystem)
	{
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
		return;
	}
	const FPredictionKey ActivationKey = CurrentActivationInfo.GetActivationPredictionKey();
	if (CurrentActorInfo->IsLocallyControlled())
	{
		const APawn* Pawn = Cast<APawn>(CurrentActorInfo->AvatarActor.Get());
		const ERpgDodgeDirection Direction = ResolveDodgeDirection(
			Pawn ? Pawn->GetLastMovementInputVector() : FVector::ZeroVector,
			CurrentActorInfo->AvatarActor->GetActorRotation());
		// The commit, effects and montage below share the key the server applies its copy under.
		FScopedPredictionWindow ScopedPrediction(AbilitySystem, !CurrentActorInfo->IsNetAuthority());
		if (!CurrentActorInfo->IsNetAuthority())
		{
			FRpgDodgeDirectionTargetData* Data = new FRpgDodgeDirectionTargetData();
			Data->Direction = Direction;
			AbilitySystem->CallServerSetReplicatedTargetData(CurrentSpecHandle, ActivationKey,
				FGameplayAbilityTargetDataHandle(Data), FGameplayTag(), AbilitySystem->ScopedPredictionKey);
		}
		FinishDodgeDirection(Direction);
		return;
	}

	// A remote owner's direction travels right behind its activation; the server starts the dodge when it arrives.
	DirectionTargetDataHandle = AbilitySystem->AbilityTargetDataSetDelegate(CurrentSpecHandle, ActivationKey)
		.AddUObject(this, &ThisClass::HandleDirectionTargetData);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(DirectionTimeoutHandle, this, &ThisClass::OnDirectionTimedOut, DirectionTimeout, false);
	}
	AbilitySystem->CallReplicatedTargetDataDelegatesIfSet(CurrentSpecHandle, ActivationKey);
}

void URpgGameplayAbility_Dodge::HandleDirectionTargetData(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag)
{
	if (!IsActive() || bDirectionResolved)
	{
		return;
	}
	// Hold our own handle while consuming the ASC's cached entry.
	const FGameplayAbilityTargetDataHandle LocalData = Data;
	if (UAbilitySystemComponent* AbilitySystem = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		AbilitySystem->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
	}
	if (LocalData.Num() != 1 || !LocalData.Get(0) || LocalData.Get(0)->GetScriptStruct() != FRpgDodgeDirectionTargetData::StaticStruct())
	{
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
		return;
	}
	FinishDodgeDirection(static_cast<const FRpgDodgeDirectionTargetData*>(LocalData.Get(0))->Direction);
}

void URpgGameplayAbility_Dodge::FinishDodgeDirection(ERpgDodgeDirection Direction)
{
	bDirectionResolved = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DirectionTimeoutHandle);
	}
	ResolvedDodgeProfile.Direction = Direction;
	ResolvedDodgeProfile.StartSection = GetDodgeDirectionSectionName(Direction);
	// Pay first: a failed commit must never leave an invulnerability window behind.
	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
		return;
	}
	const float PlayRate = FMath::Max(0.01f, ResolvedDodgeProfile.MontagePlayRate);
	if (EvadeDuration > 0.0f && CurrentActorInfo && CurrentActorInfo->IsNetAuthority())
	{
		if (URpgAbilitySystemComponent* AbilitySystem = GetRpgAbilitySystemComponentFromActorInfo())
		{
			AbilitySystem->AddTimedLooseGameplayTag(RpgGameplayTags::State_Evading, EvadeDuration / PlayRate);
		}
	}
	if (RecoveryCancelTime > 0.0f && GetActivationGroup() == ERpgAbilityActivationGroup::Exclusive_Blocking)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(RecoveryCancelHandle, this, &ThisClass::OnRecoveryCancelOpened,
				RecoveryCancelTime / PlayRate, false);
		}
	}
	OnDodgeReady();
}

void URpgGameplayAbility_Dodge::OnDirectionTimedOut()
{
	if (IsActive() && !bDirectionResolved)
	{
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
	}
}

void URpgGameplayAbility_Dodge::OnRecoveryCancelOpened()
{
	// From here an attack, a block or another exclusive ability may replace the recovery.
	if (IsActive() && CanChangeActivationGroup(ERpgAbilityActivationGroup::Exclusive_Replaceable))
	{
		ChangeActivationGroup(ERpgAbilityActivationGroup::Exclusive_Replaceable);
	}
}

void URpgGameplayAbility_Dodge::ClearDodgeRuntimeState()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DirectionTimeoutHandle);
		World->GetTimerManager().ClearTimer(RecoveryCancelHandle);
	}
	UAbilitySystemComponent* AbilitySystem = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (AbilitySystem && DirectionTargetDataHandle.IsValid())
	{
		const FPredictionKey ActivationKey = CurrentActivationInfo.GetActivationPredictionKey();
		AbilitySystem->AbilityTargetDataSetDelegate(CurrentSpecHandle, ActivationKey).Remove(DirectionTargetDataHandle);
		AbilitySystem->ConsumeClientReplicatedTargetData(CurrentSpecHandle, ActivationKey);
	}
	DirectionTargetDataHandle.Reset();
	bDirectionResolved = false;
}

bool URpgGameplayAbility_Dodge::ResolveDodgeProfileForActivation()
{
	if (!IsActive() || bIsAbilityEnding || !CurrentActorInfo || !CurrentActorInfo->AvatarActor.IsValid())
	{
		return false;
	}
	if (!bHasResolvedDodgeProfile)
	{
		ResolvedDodgeProfile = ResolveDodgeProfile(*CurrentActorInfo);
		bHasResolvedDodgeProfile = true;
		// The roll keeps the facing it started with; rotation modes would otherwise steer its root motion.
		BeginFacingCommitment(0.0f);
	}
	return true;
}

void URpgGameplayAbility_Dodge::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (!IsEndAbilityValid(Handle, ActorInfo))
	{
		return;
	}
	if (ScopeLockCount > 0)
	{
		WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &ThisClass::EndAbility,
			Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled));
		return;
	}
	ClearDodgeRuntimeState();
	ResolvedDodgeProfile = FRpgResolvedDodgeProfile();
	bHasResolvedDodgeProfile = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	// The ASC has released the group this activation ended in; the reused instance starts in its authored group again.
	ActivationGroup = GetClass()->GetDefaultObject<ThisClass>()->GetActivationGroup();
}

FRpgResolvedDodgeProfile URpgGameplayAbility_Dodge::ResolveDodgeProfile(const FGameplayAbilityActorInfo& ActorInfo) const
{
	FRpgResolvedDodgeProfile Result;
	FRpgEquipmentDodgeProfile EquipmentProfile = DefaultDodgeProfile;
	AController* Controller = ActorInfo.PlayerController.Get();
	if (!Controller)
	{
		Controller = GetControllerFromActorInfo();
	}

	if (Controller)
	{
		if (const URpgEquipmentLoadoutComponent* Loadout = Controller->FindComponentByClass<URpgEquipmentLoadoutComponent>())
		{
			Result.LoadTier = Loadout->GetEquipmentLoadTier();
			const FRpgEquipmentDodgeProfile TierProfile = Loadout->GetDodgeProfileForCurrentLoad();
			if (!TierProfile.Montage.IsNull())
			{
				EquipmentProfile.Montage = TierProfile.Montage;
			}
			EquipmentProfile.RootMotionProfile = TierProfile.RootMotionProfile;
		}
	}

	const FRpgDodgeRootMotionTuning Tuning = ResolveRootMotionTuning(
		EquipmentProfile.RootMotionProfile,
		RootMotionTunings,
		DefaultMontagePlayRate,
		DefaultRootMotionTranslationScale);
	Result.Montage = Tuning.Montage.IsNull() ? EquipmentProfile.Montage : Tuning.Montage;
	Result.RootMotionProfile = EquipmentProfile.RootMotionProfile;
	Result.MontagePlayRate = Tuning.MontagePlayRate;
	Result.TranslationScale = Tuning.TranslationScale;
	Result.StartSection = Tuning.StartSection;
	return Result;
}

#if WITH_EDITOR
EDataValidationResult URpgGameplayAbility_Dodge::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
	const auto ValidateMontage = [&Context, &Result, this](const TSoftObjectPtr<UAnimMontage>& Montage, const FText& Field)
	{
		if (Montage.IsNull())
		{
			return;
		}
		const UAnimMontage* Loaded = Montage.LoadSynchronous();
		for (uint8 Index = 0; Index < 8; ++Index)
		{
			const FName Section = GetDodgeDirectionSectionName(static_cast<ERpgDodgeDirection>(Index));
			if (!Loaded || !Loaded->IsValidSectionName(Section))
			{
				Context.AddError(FText::Format(LOCTEXT("MissingDodgeSection",
					"Dodge ability '{0}' uses {1} '{2}', which has no section '{3}'. A dodge montage needs one section per direction: F, FR, R, BR, B, BL, L, FL."),
					FText::FromString(GetPathName()), Field, FText::FromString(Montage.ToString()), FText::FromName(Section)));
				Result = EDataValidationResult::Invalid;
				return;
			}
		}
	};
	ValidateMontage(DefaultDodgeProfile.Montage, LOCTEXT("DefaultProfileField", "DefaultDodgeProfile.Montage"));
	for (const FRpgDodgeRootMotionTuning& Tuning : RootMotionTunings)
	{
		ValidateMontage(Tuning.Montage, FText::Format(LOCTEXT("TuningField", "the tuning for '{0}'"), FText::FromName(Tuning.ProfileName)));
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
