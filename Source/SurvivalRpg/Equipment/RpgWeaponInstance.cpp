#include "RpgWeaponInstance.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

#if WITH_EDITOR
#include "Animation/AnimClassInterface.h"
#include "Misc/DataValidation.h"
#include "SurvivalRpg/Animation/AnimNotifyState_RpgComboWindow.h"
#include "SurvivalRpg/Animation/AnimNotify_RpgWeaponAttackWindow.h"
#endif

#define LOCTEXT_NAMESPACE "RpgWeaponInstance"

FRpgWeaponBlockDefinition FRpgWeaponBlockDefinition::ResolveForSkeleton(const USkeleton* Skeleton) const
{
	FRpgWeaponBlockDefinition Resolved = *this;
	const FRpgWeaponBlockPresentation* Presentation = Skeleton ? SkeletonPresentations.FindByPredicate(
		[Skeleton](const FRpgWeaponBlockPresentation& Candidate) { return Candidate.Skeleton == Skeleton; }) : nullptr;
	if (Presentation)
	{
		Resolved.BlockLocomotionLayer = Presentation->BlockLocomotionLayer;
		Resolved.BlockStartMontage = Presentation->BlockStartMontage;
		Resolved.BlockLoopMontage = Presentation->BlockLoopMontage;
		Resolved.BlockEndMontage = Presentation->BlockEndMontage;
		Resolved.BlockHitMontage = Presentation->BlockHitMontage;
		Resolved.PerfectBlockMontage = Presentation->PerfectBlockMontage;
		Resolved.GuardBreakMontage = Presentation->GuardBreakMontage;
	}
	return Resolved;
}

FRpgWeaponAttackDefinition::FRpgWeaponAttackDefinition()
{
	TracePointSockets.Add(TEXT("blade_base"));
	TracePointSockets.Add(TEXT("blade_mid"));
	TracePointSockets.Add(TEXT("blade_tip"));
}

bool FRpgWeaponAttackDefinition::HasValidTraceData() const
{
	if (TracePointSockets.Num() < 2 || TraceSampleInterval <= 0.0f)
	{
		return false;
	}

	switch (TraceMode)
	{
	case ERpgWeaponAttackTraceMode::LineTrace:
		return !bTraceBetweenSockets || TraceInterpolationDistance > 0.0f;
	case ERpgWeaponAttackTraceMode::SphereSweep:
		return TraceRadius > 0.0f;
	case ERpgWeaponAttackTraceMode::CapsuleSweep:
		return TraceRadius > 0.0f && TraceCapsuleHalfHeight > 0.0f;
	case ERpgWeaponAttackTraceMode::BoxSweep:
		return TraceBoxExtent.GetAbsMin() > 0.0f;
	default:
		return false;
	}
}

bool FRpgConditionalAttackModifier::MatchesTargetTags(const FGameplayTagContainer& TargetTags) const
{
	if (!TargetTags.HasAll(RequiredTargetTags))
	{
		return false;
	}

	if (BlockedTargetTags.Num() > 0 && TargetTags.HasAny(BlockedTargetTags))
	{
		return false;
	}

	return true;
}

URpgWeaponInstance::URpgWeaponInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WeaponTypeTag = RpgGameplayTags::Weapon_Type_Melee;
	WeaponFamilyTag = RpgGameplayTags::Weapon_Family_Sword;

	FRpgWeaponAttackDefinition PrimaryAttack;
	PrimaryAttack.DamageTypeTags.AddTag(RpgGameplayTags::Damage_Type_Melee);
	PrimaryAttack.StaggerDamage = 20.0f;
	PrimaryAttack.HitReactionEventTag = RpgGameplayTags::GameplayEvent_HitReaction;
	AttackDefinitions.Add(RpgGameplayTags::Weapon_Attack_Primary, PrimaryAttack);

	BlockDefinition.BlockableDamageTypeTags.AddTag(RpgGameplayTags::Damage_Type_Melee);
}

#if WITH_EDITOR
EDataValidationResult URpgWeaponInstance::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
	const FText WeaponPath = FText::FromString(GetPathName());
	if (!FMath::IsFinite(BlockDefinition.MovementSpeedLimit) || BlockDefinition.MovementSpeedLimit < 0.f)
	{
		Context.AddError(FText::Format(LOCTEXT("InvalidBlockMovementSpeedLimit",
			"Weapon '{0}' has an invalid BlockDefinition.MovementSpeedLimit. Use a finite non-negative speed in cm/s; zero preserves the ordinary speed limit."), WeaponPath));
		Result = EDataValidationResult::Invalid;
	}

	const auto ValidateLayer = [&Context, &WeaponPath, &Result](UClass* LayerClass, const USkeleton* ExpectedSkeleton, const FText& Field)
	{
		const IAnimClassInterface* AnimClass = IAnimClassInterface::GetFromClass(LayerClass);
		// LinkAnimClassLayers binds only implemented animation functions. A main AnimGraph alone,
		// an interface stub, or a native AnimInstance cannot replace a linked layer.
		const bool bHasImplementedLayer = AnimClass && AnimClass->GetAnimBlueprintFunctions().ContainsByPredicate(
			[](const FAnimBlueprintFunction& Function)
			{
				return Function.bImplemented && !Function.Name.IsNone() && Function.Name != TEXT("AnimGraph");
			});
		if (LayerClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_Interface)
			|| !bHasImplementedLayer)
		{
			Context.AddError(FText::Format(LOCTEXT("InvalidBlockLocomotionLayer",
				"Weapon '{0}' uses {2} '{1}', which is not a concrete compiled Anim Blueprint with an implemented animation layer. Assign a valid linked-layer class or clear it to retain the legacy block loop."),
				WeaponPath, FText::FromString(LayerClass->GetPathName()), Field));
			Result = EDataValidationResult::Invalid;
			return;
		}
		if (ExpectedSkeleton && AnimClass->GetTargetSkeleton() != ExpectedSkeleton)
		{
			Context.AddError(FText::Format(LOCTEXT("BlockLayerSkeletonMismatch",
				"Weapon '{0}' assigns {4} '{1}' to skeleton '{2}', but the layer targets '{3}'. Linked layers only bind to their exact target skeleton."),
				WeaponPath, FText::FromString(LayerClass->GetPathName()), FText::FromString(ExpectedSkeleton->GetPathName()),
				FText::FromString(GetPathNameSafe(AnimClass->GetTargetSkeleton())), Field));
			Result = EDataValidationResult::Invalid;
		}
		// Let the layer's existing asset-validation seam report its designer-owned selection checks.
		Result = CombineDataValidationResults(Result, LayerClass->GetDefaultObject<UAnimInstance>()->IsDataValid(Context));
	};

	if (UClass* LayerClass = BlockDefinition.BlockLocomotionLayer.Get())
	{
		ValidateLayer(LayerClass, nullptr, LOCTEXT("DefaultBlockLayerField", "BlockDefinition.BlockLocomotionLayer"));
	}

	TSet<const USkeleton*> PresentationSkeletons;
	for (const FRpgWeaponBlockPresentation& Presentation : BlockDefinition.SkeletonPresentations)
	{
		bool bDuplicate = false;
		PresentationSkeletons.Add(Presentation.Skeleton, &bDuplicate);
		if (!Presentation.Skeleton || bDuplicate)
		{
			Context.AddError(FText::Format(LOCTEXT("InvalidBlockPresentationSkeleton",
				"Weapon '{0}' has a block presentation without a skeleton or with a skeleton listed twice ('{1}'). Each presentation needs its own gameplay skeleton."),
				WeaponPath, FText::FromString(GetPathNameSafe(Presentation.Skeleton))));
			Result = EDataValidationResult::Invalid;
			continue;
		}
		if (UClass* LayerClass = Presentation.BlockLocomotionLayer.Get())
		{
			ValidateLayer(LayerClass, Presentation.Skeleton, LOCTEXT("PresentationBlockLayerField", "BlockDefinition.SkeletonPresentations.BlockLocomotionLayer"));
		}
		for (const UAnimMontage* Montage : { Presentation.BlockStartMontage.Get(), Presentation.BlockLoopMontage.Get(),
			Presentation.BlockEndMontage.Get(), Presentation.BlockHitMontage.Get(), Presentation.PerfectBlockMontage.Get(),
			Presentation.GuardBreakMontage.Get() })
		{
			if (Montage && Montage->GetSkeleton() != Presentation.Skeleton)
			{
				Context.AddError(FText::Format(LOCTEXT("BlockMontageSkeletonMismatch",
					"Weapon '{0}' assigns the block montage '{1}' to skeleton '{2}', but the montage uses '{3}'."),
					WeaponPath, FText::FromString(Montage->GetPathName()), FText::FromString(Presentation.Skeleton->GetPathName()),
					FText::FromString(GetPathNameSafe(Montage->GetSkeleton()))));
				Result = EDataValidationResult::Invalid;
			}
		}
	}

	for (const TPair<FGameplayTag, FRpgWeaponAttackDefinition>& Entry : AttackDefinitions)
	{
		const FRpgWeaponAttackDefinition& Attack = Entry.Value;
		if (!Attack.ComboNextAttack.IsValid())
		{
			continue;
		}
		const FText AttackName = FText::FromName(Entry.Key.GetTagName());
		auto AddComboError = [&Context, &WeaponPath, &AttackName, &Result](const FText& Reason)
		{
			Context.AddError(FText::Format(LOCTEXT("InvalidComboAttack", "Weapon '{0}' attack '{1}' cannot chain: {2}"),
				WeaponPath, AttackName, Reason));
			Result = EDataValidationResult::Invalid;
		};

		// Walk the chain once from every entry; a revisited attack means the combo never ends.
		TSet<FGameplayTag> Visited{ Entry.Key };
		for (FGameplayTag Next = Attack.ComboNextAttack; Next.IsValid();)
		{
			const FRpgWeaponAttackDefinition* NextAttack = AttackDefinitions.Find(Next);
			if (!NextAttack)
			{
				AddComboError(FText::Format(LOCTEXT("MissingComboNext", "the chain reaches '{0}', which is not an attack of this weapon."),
					FText::FromName(Next.GetTagName())));
				break;
			}
			bool bRevisited = false;
			Visited.Add(Next, &bRevisited);
			if (bRevisited)
			{
				AddComboError(FText::Format(LOCTEXT("CyclicCombo", "the chain returns to '{0}' and never ends."), FText::FromName(Next.GetTagName())));
				break;
			}
			Next = NextAttack->ComboNextAttack;
		}

		float WindowStart = 0.0f;
		float WindowEnd = 0.0f;
		FString Failure;
		if (!UAnimNotifyState_RpgComboWindow::FindComboWindow(Attack.Montage, WindowStart, WindowEnd, Failure))
		{
			AddComboError(FText::FromString(Failure));
			continue;
		}
		for (const FAnimNotifyEvent& NotifyEvent : Attack.Montage->Notifies)
		{
			// The next hit replaces this montage, so it may not start before this hit's damage window has closed.
			if (NotifyEvent.Notify && NotifyEvent.Notify->IsA<UAnimNotify_RpgWeaponAttackWindowEnd>()
				&& NotifyEvent.GetTriggerTime() > WindowStart)
			{
				AddComboError(FText::Format(LOCTEXT("ComboBeforeAttackEnd", "its Combo Window starts at {0} s, before the attack window ends at {1} s."),
					FText::AsNumber(WindowStart), FText::AsNumber(NotifyEvent.GetTriggerTime())));
			}
		}
		const float EffectivePlayRate = Attack.MontagePlayRate * Attack.Montage->RateScale;
		if (Attack.Montage->bEnableAutoBlendOut && EffectivePlayRate > UE_SMALL_NUMBER)
		{
			const float BlendOutTrigger = Attack.Montage->BlendOutTriggerTime >= 0.0f
				? Attack.Montage->BlendOutTriggerTime : Attack.Montage->BlendOut.GetBlendTime();
			if ((Attack.Montage->GetPlayLength() - WindowEnd) / EffectivePlayRate <= BlendOutTrigger)
			{
				AddComboError(FText::Format(LOCTEXT("ComboAfterBlendOut", "its Combo Window ends at {0} s, inside the montage's auto blend-out, when the attack already finishes."),
					FText::AsNumber(WindowEnd)));
			}
		}
	}
	return Result;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
bool URpgWeaponInstance::ConfigureBlockLocomotionForTests(TSubclassOf<UAnimInstance> LayerClass, float SpeedLimit)
{
	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) || !GetPawn()
		|| !FMath::IsFinite(SpeedLimit) || SpeedLimit < 0.f) { return false; }
	// Configure the presentation the test pawn resolves, so restoring its previously resolved layer is symmetric.
	const USkeletalMeshComponent* Mesh = URpgPawnExtensionComponent::FindGameplayMesh(GetPawn());
	const USkeleton* Skeleton = Mesh && Mesh->GetSkeletalMeshAsset() ? Mesh->GetSkeletalMeshAsset()->GetSkeleton() : nullptr;
	FRpgWeaponBlockPresentation* Presentation = Skeleton ? BlockDefinition.SkeletonPresentations.FindByPredicate(
		[Skeleton](const FRpgWeaponBlockPresentation& Candidate) { return Candidate.Skeleton == Skeleton; }) : nullptr;
	(Presentation ? Presentation->BlockLocomotionLayer : BlockDefinition.BlockLocomotionLayer) = LayerClass;
	BlockDefinition.MovementSpeedLimit = SpeedLimit;
	return true;
}
#endif

const FRpgWeaponAttackDefinition* URpgWeaponInstance::FindAttackDefinition(FGameplayTag AttackDefinitionTag) const
{
	return AttackDefinitionTag.IsValid() ? AttackDefinitions.Find(AttackDefinitionTag) : nullptr;
}

bool URpgWeaponInstance::GetAttackDefinitionByTag(FGameplayTag AttackDefinitionTag, FRpgWeaponAttackDefinition& OutAttackDefinition) const
{
	if (const FRpgWeaponAttackDefinition* AttackDefinition = FindAttackDefinition(AttackDefinitionTag))
	{
		OutAttackDefinition = *AttackDefinition;
		return true;
	}

	return false;
}

TArray<FGameplayTag> URpgWeaponInstance::GetAttackDefinitionTags() const
{
	TArray<FGameplayTag> AttackDefinitionTags;
	AttackDefinitions.GetKeys(AttackDefinitionTags);
	return AttackDefinitionTags;
}

TArray<FName> URpgWeaponInstance::GetAttackDefinitionTagNames() const
{
	TArray<FName> AttackDefinitionTagNames;
	AttackDefinitionTagNames.Reserve(AttackDefinitions.Num());
	for (const TPair<FGameplayTag, FRpgWeaponAttackDefinition>& Entry : AttackDefinitions)
	{
		AttackDefinitionTagNames.Add(Entry.Key.GetTagName());
	}
	return AttackDefinitionTagNames;
}

bool URpgWeaponInstance::HasAttackDefinitionByTagName(FName AttackDefinitionTagName) const
{
	const FGameplayTag AttackDefinitionTag = AttackDefinitionTagName.IsNone() ? FGameplayTag() : FGameplayTag::RequestGameplayTag(AttackDefinitionTagName);
	return FindAttackDefinition(AttackDefinitionTag) != nullptr;
}

#if WITH_DEV_AUTOMATION_TESTS
bool URpgWeaponInstance::SetAttackMontagePlayRateForTests(
	const FGameplayTag AttackDefinitionTag,
	const float NewPlayRate,
	float& OutPreviousPlayRate)
{
	if (!FMath::IsFinite(NewPlayRate) || NewPlayRate <= UE_SMALL_NUMBER)
	{
		return false;
	}

	FRpgWeaponAttackDefinition* AttackDefinition = AttackDefinitions.Find(AttackDefinitionTag);
	if (!AttackDefinition)
	{
		return false;
	}

	OutPreviousPlayRate = AttackDefinition->MontagePlayRate;
	AttackDefinition->MontagePlayRate = NewPlayRate;
	return true;
}
#endif

void URpgWeaponInstance::SetWeaponTagsByName(FName WeaponTypeTagName, FName WeaponFamilyTagName)
{
	WeaponTypeTag = WeaponTypeTagName.IsNone() ? FGameplayTag() : FGameplayTag::RequestGameplayTag(WeaponTypeTagName);
	WeaponFamilyTag = WeaponFamilyTagName.IsNone() ? FGameplayTag() : FGameplayTag::RequestGameplayTag(WeaponFamilyTagName);
}

void URpgWeaponInstance::ClearAttackDefinitions()
{
	AttackDefinitions.Reset();
}

void URpgWeaponInstance::ConfigureAttackByTagName(
	FName AttackDefinitionTagName,
	UAnimMontage* Montage,
	TSubclassOf<UGameplayEffect> DamageEffect,
	float Damage,
	float TraceRadius,
	const TArray<FName>& TracePointSockets,
	float TraceSampleInterval,
	TSubclassOf<URpgCameraMode> CameraMode,
	FName HitReactionEventTagName)
{
	const FGameplayTag AttackDefinitionTag = AttackDefinitionTagName.IsNone() ? FGameplayTag() : FGameplayTag::RequestGameplayTag(AttackDefinitionTagName);
	if (!AttackDefinitionTag.IsValid())
	{
		return;
	}

	FRpgWeaponAttackDefinition& AttackDefinition = AttackDefinitions.FindOrAdd(AttackDefinitionTag);
	AttackDefinition.Montage = Montage;
	AttackDefinition.DamageEffect = DamageEffect;
	AttackDefinition.Damage = Damage;
	AttackDefinition.TraceRadius = TraceRadius;
	AttackDefinition.TracePointSockets = TracePointSockets;
	AttackDefinition.TraceSampleInterval = TraceSampleInterval;
	AttackDefinition.CameraMode = CameraMode;
	AttackDefinition.HitReactionEventTag = HitReactionEventTagName.IsNone() ? FGameplayTag() : FGameplayTag::RequestGameplayTag(HitReactionEventTagName);
}

void URpgWeaponInstance::ConfigureMeleeAttackByTagName(
	FName AttackDefinitionTagName,
	UAnimMontage* Montage,
	TSubclassOf<UGameplayEffect> DamageEffect,
	float Damage,
	float StaggerDamage,
	float TraceRadius,
	const TArray<FName>& TracePointSockets,
	float TraceSampleInterval,
	TSubclassOf<URpgCameraMode> CameraMode,
	FName HitReactionEventTagName)
{
	ConfigureAttackByTagName(
		AttackDefinitionTagName,
		Montage,
		DamageEffect,
		Damage,
		TraceRadius,
		TracePointSockets,
		TraceSampleInterval,
		CameraMode,
		HitReactionEventTagName);

	const FGameplayTag AttackDefinitionTag = AttackDefinitionTagName.IsNone() ? FGameplayTag() : FGameplayTag::RequestGameplayTag(AttackDefinitionTagName);
	if (FRpgWeaponAttackDefinition* AttackDefinition = AttackDefinitions.Find(AttackDefinitionTag))
	{
		AttackDefinition->DamageTypeTags.Reset();
		AttackDefinition->DamageTypeTags.AddTag(RpgGameplayTags::Damage_Type_Melee);
		AttackDefinition->StaggerDamage = FMath::Max(0.0f, StaggerDamage);
	}
}

void URpgWeaponInstance::ConfigureMeleeBlock(
	bool bCanBlock,
	bool bAllowPerfectBlock,
	float BlockAngleDegrees,
	float PerfectBlockWindow,
	float StaminaCost,
	float DamageReduction,
	float BlockStaggerDamageMultiplier,
	float PerfectBlockStaminaRestore,
	float PerfectBlockStaggerDamage,
	UAnimMontage* BlockLoopMontage)
{
	BlockDefinition.bCanBlock = bCanBlock;
	BlockDefinition.bAllowPerfectBlock = bAllowPerfectBlock;
	BlockDefinition.BlockableDamageTypeTags.Reset();
	BlockDefinition.BlockableDamageTypeTags.AddTag(RpgGameplayTags::Damage_Type_Melee);
	BlockDefinition.BlockAngleDegrees = FMath::Clamp(BlockAngleDegrees, 0.0f, 360.0f);
	BlockDefinition.PerfectBlockWindow = FMath::Max(0.0f, PerfectBlockWindow);
	BlockDefinition.StaminaCost = FMath::Max(0.0f, StaminaCost);
	BlockDefinition.DamageReduction = FMath::Clamp(DamageReduction, 0.0f, 1.0f);
	BlockDefinition.BlockStaggerDamageMultiplier = FMath::Max(0.0f, BlockStaggerDamageMultiplier);
	BlockDefinition.PerfectBlockStaminaRestore = FMath::Max(0.0f, PerfectBlockStaminaRestore);
	BlockDefinition.PerfectBlockStaggerDamage = FMath::Max(0.0f, PerfectBlockStaggerDamage);
	BlockDefinition.BlockLoopMontage = BlockLoopMontage;
}

#undef LOCTEXT_NAMESPACE
