#include "RpgWeaponInstance.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Physics/RpgCollisionChannels.h"
#include "SurvivalRpg/Runes/RpgInventoryFragment_WeaponRunes.h"
#include "SurvivalRpg/Runes/RpgWeaponRuneDefinition.h"
#include "SurvivalRpg/SurvivalRpg.h"

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
	: HitCueTag(RpgGameplayTags::GameplayCue_Combat_Hit)
{
	TracePointSockets.Add(TEXT("blade_base"));
	TracePointSockets.Add(TEXT("blade_mid"));
	TracePointSockets.Add(TEXT("blade_tip"));
}

bool FRpgWeaponAttackDefinition::HasValidTraceData() const
{
	return MakeSwingTraceParams().HasValidTraceData();
}

FRpgSwingTraceParams FRpgWeaponAttackDefinition::MakeSwingTraceParams() const
{
	FRpgSwingTraceParams Params;
	Params.TraceMode = TraceMode;
	Params.bTraceBetweenSockets = bTraceBetweenSockets;
	Params.TraceInterpolationDistance = TraceInterpolationDistance;
	Params.TraceRadius = TraceRadius;
	Params.TraceCapsuleHalfHeight = TraceCapsuleHalfHeight;
	Params.TraceBoxExtent = TraceBoxExtent;
	Params.TracePointSockets = TracePointSockets;
	Params.TraceSampleInterval = TraceSampleInterval;
	Params.TraceChannel = Rpg_TraceChannel_Weapon;
	return Params;
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

bool URpgWeaponInstance::SetEchoRuneForTests(const URpgWeaponRuneDefinition* Rune)
{
	const APawn* Pawn = GetPawn();
	if (!Pawn || !Pawn->HasAuthority())
	{
		return false;
	}

	StopEcho();
	EchoRune = Rune && Rune->EchoStrike.bEnabled ? Rune : nullptr;
	return EchoRune == Rune;
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

void URpgWeaponInstance::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, EchoCharge);
}

void URpgWeaponInstance::OnEquipped()
{
	Super::OnEquipped();

	const APawn* Pawn = GetPawn();
	if (!Pawn || !Pawn->HasAuthority())
	{
		return;
	}

	// The runes come with the item instance this weapon was equipped from.
	const URpgInventoryItemInstance* Item = Cast<URpgInventoryItemInstance>(GetInstigator());
	const URpgInventoryFragment_WeaponRunes* Runes = Item ? Item->FindFragmentByClass<URpgInventoryFragment_WeaponRunes>() : nullptr;
	EchoRune = Runes ? Runes->FindEchoRune() : nullptr;
	EchoChargeRule.Reset();
	SetEchoCharge(0);
}

void URpgWeaponInstance::OnUnequipped()
{
	StopEcho();
	Super::OnUnequipped();
}

void URpgWeaponInstance::HandleAttackSwingResolved(const FRpgWeaponSwingRecord& Swing, const bool bLandedHit)
{
	UWorld* World = GetWorld();
	const APawn* Pawn = GetPawn();
	if (!EchoRune || !World || !Pawn || !Pawn->HasAuthority())
	{
		return;
	}

	const FRpgEchoStrikeSettings& Settings = EchoRune->EchoStrike;
	const float Now = World->GetTimeSeconds();
	const bool bTriggered = EchoChargeRule.RegisterSwing(bLandedHit, Settings.HitsToTrigger, Settings.ChargeTimeout, Now);
	SetEchoCharge(EchoChargeRule.Charge);

	FTimerManager& TimerManager = World->GetTimerManager();
	if (EchoChargeRule.Charge > 0 && Settings.ChargeTimeout > 0.0f)
	{
		TimerManager.SetTimer(EchoChargeTimeoutHandle, this, &ThisClass::ExpireEchoCharge, Settings.ChargeTimeout, false);
	}
	else
	{
		TimerManager.ClearTimer(EchoChargeTimeoutHandle);
	}

	if (!bTriggered || Swing.Traces.IsEmpty())
	{
		return;
	}

	// The ghost strike outlives the attack ability: a dodge may cancel the attack during the delay.
	GhostStrikeSwing = Swing;
	GhostStrikeHitActors.Reset();
	NextGhostStrikeTrace = 0;
	GhostStrikeStartTime = FMath::Max(Now, Swing.WindowStartTime + Settings.StrikeDelay);
	UE_LOG(LogRpgWeapons, Verbose, TEXT("Echo[%s] strike triggered: %d traces replay from %.3f, %.3f s after the swing."),
		*GetNameSafe(this), Swing.Traces.Num(), GhostStrikeStartTime, GhostStrikeStartTime - Swing.WindowStartTime);
	TimerManager.SetTimer(
		GhostStrikeTimerHandle,
		this,
		&ThisClass::TickGhostStrike,
		1.0f / 60.0f,
		true,
		FMath::Max(UE_KINDA_SMALL_NUMBER, GhostStrikeStartTime - Now));

	UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
	if (AbilitySystem && Settings.StrikeCueTag.IsValid())
	{
		FGameplayCueParameters Parameters;
		Parameters.Instigator = const_cast<APawn*>(Pawn);
		Parameters.EffectCauser = const_cast<APawn*>(Pawn);
		const TArray<AActor*> WeaponActors = GetSpawnedActors();
		Parameters.SourceObject = WeaponActors.IsEmpty() ? nullptr : WeaponActors[0];
		Parameters.Location = Pawn->GetActorLocation();
		// How far the ghost runs behind the swing; at least the delay, more when the window outlasted it.
		Parameters.RawMagnitude = GhostStrikeStartTime - Swing.WindowStartTime;
		// Until the replayed swing has played out on a machine that receives the cue now.
		Parameters.NormalizedMagnitude = FMath::Max(0.1f, GhostStrikeStartTime + Swing.GetDuration() - Now);
		AbilitySystem->ExecuteGameplayCue(Settings.StrikeCueTag, Parameters);
	}
}

void URpgWeaponInstance::SetEchoCharge(const int32 NewCharge)
{
	const uint8 ClampedCharge = static_cast<uint8>(FMath::Clamp(NewCharge, 0, 255));
	if (EchoCharge == ClampedCharge)
	{
		return;
	}
	EchoCharge = ClampedCharge;
	K2_OnEchoChargeChanged(EchoCharge);
}

void URpgWeaponInstance::OnRep_EchoCharge()
{
	K2_OnEchoChargeChanged(EchoCharge);
}

void URpgWeaponInstance::ExpireEchoCharge()
{
	EchoChargeRule.Reset();
	SetEchoCharge(0);
}

void URpgWeaponInstance::StopEcho()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EchoChargeTimeoutHandle);
		World->GetTimerManager().ClearTimer(GhostStrikeTimerHandle);
	}
	EchoChargeRule.Reset();
	GhostStrikeSwing = FRpgWeaponSwingRecord();
	GhostStrikeHitActors.Reset();
	NextGhostStrikeTrace = 0;
	const APawn* Pawn = GetPawn();
	if (Pawn && Pawn->HasAuthority())
	{
		SetEchoCharge(0);
	}
	EchoRune = nullptr;
}

void URpgWeaponInstance::TickGhostStrike()
{
	UWorld* World = GetWorld();
	if (!World || !EchoRune)
	{
		StopEcho();
		return;
	}

	// Each recorded query runs at its original offset, so the ghost sweeps the swing at its own pace.
	const int32 DueEnd = GhostStrikeSwing.FindDueEnd(NextGhostStrikeTrace, World->GetTimeSeconds() - GhostStrikeStartTime);
	for (; NextGhostStrikeTrace < DueEnd; ++NextGhostStrikeTrace)
	{
		ApplyGhostStrikeTrace(GhostStrikeSwing.Traces[NextGhostStrikeTrace]);
	}

	if (NextGhostStrikeTrace >= GhostStrikeSwing.Traces.Num())
	{
		World->GetTimerManager().ClearTimer(GhostStrikeTimerHandle);
		GhostStrikeSwing = FRpgWeaponSwingRecord();
		GhostStrikeHitActors.Reset();
		NextGhostStrikeTrace = 0;
	}
}

void URpgWeaponInstance::ApplyGhostStrikeTrace(const FRpgRecordedWeaponTrace& Trace)
{
	UWorld* World = GetWorld();
	APawn* Pawn = GetPawn();
	if (!World || !Pawn)
	{
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RpgEchoGhostStrike), false, Pawn);
	for (AActor* SpawnedActor : GetSpawnedActors())
	{
		if (SpawnedActor)
		{
			QueryParams.AddIgnoredActor(SpawnedActor);
		}
	}

	TArray<FHitResult> HitResults;
	switch (Trace.Shape)
	{
	case ERpgRecordedTraceShape::Line:
		World->LineTraceMultiByChannel(HitResults, Trace.Start, Trace.End, Rpg_TraceChannel_Weapon, QueryParams);
		break;
	case ERpgRecordedTraceShape::Sphere:
		World->SweepMultiByChannel(HitResults, Trace.Start, Trace.End, FQuat::Identity, Rpg_TraceChannel_Weapon,
			FCollisionShape::MakeSphere(Trace.ShapeExtent.X), QueryParams);
		break;
	case ERpgRecordedTraceShape::Capsule:
		World->SweepMultiByChannel(HitResults, Trace.Start, Trace.End, FQuat::Identity, Rpg_TraceChannel_Weapon,
			FCollisionShape::MakeCapsule(Trace.ShapeExtent.X, Trace.ShapeExtent.Y), QueryParams);
		break;
	case ERpgRecordedTraceShape::Box:
		World->SweepMultiByChannel(HitResults, Trace.Start, Trace.End, FQuat::Identity, Rpg_TraceChannel_Weapon,
			FCollisionShape::MakeBox(Trace.ShapeExtent), QueryParams);
		break;
	}

	for (const FHitResult& HitResult : HitResults)
	{
		AActor* TargetActor = HitResult.GetActor();
		if (!TargetActor || TargetActor == Pawn || GhostStrikeHitActors.Contains(TargetActor))
		{
			continue;
		}
		GhostStrikeHitActors.Add(TargetActor);
		ApplyGhostStrikeHit(TargetActor, HitResult);
	}
}

void URpgWeaponInstance::ApplyGhostStrikeHit(AActor* TargetActor, const FHitResult& HitResult)
{
	const URpgHealthComponent* HealthComponent = URpgHealthComponent::FindHealthComponent(TargetActor);
	APawn* Pawn = GetPawn();
	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
	if (!EchoRune || !HealthComponent || HealthComponent->IsDeadOrDying() || !SourceASC || !TargetASC ||
		!EchoRune->EchoStrike.DamageEffect)
	{
		return;
	}

	const FRpgEchoStrikeSettings& Settings = EchoRune->EchoStrike;
	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddSourceObject(this);
	Context.AddHitResult(HitResult, true);
	const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(Settings.DamageEffect, 1.0f, Context);
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (!Spec)
	{
		return;
	}

	Spec->SetSetByCallerMagnitude(RpgGameplayTags::SetByCaller_Damage, FMath::Max(0.0f, Settings.Damage));
	Spec->SetSetByCallerMagnitude(RpgGameplayTags::SetByCaller_StaggerDamage, FMath::Max(0.0f, Settings.StaggerDamage));
	Spec->AppendDynamicAssetTags(Settings.DamageTypeTags);
	if (Settings.HitCueTag.IsValid())
	{
		Spec->AddDynamicAssetTag(Settings.HitCueTag);
	}
	UE_LOG(LogRpgWeapons, Verbose, TEXT("Echo[%s] ghost hit %s."), *GetNameSafe(this), *GetNameSafe(TargetActor));
	SourceASC->ApplyGameplayEffectSpecToTarget(*Spec, TargetASC);
}

#undef LOCTEXT_NAMESPACE
