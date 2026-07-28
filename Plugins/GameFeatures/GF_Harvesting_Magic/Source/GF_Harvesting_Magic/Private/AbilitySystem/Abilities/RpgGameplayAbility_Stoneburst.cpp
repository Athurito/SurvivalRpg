#include "AbilitySystem/Abilities/RpgGameplayAbility_Stoneburst.h"

#include "AbilitySystemComponent.h"
#include "CollisionShape.h"
#include "Components/ActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameplayEffectTypes.h"
#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
#include "Harvesting/RpgHarvestableInstancedMeshComponent.h"
#include "Harvesting/RpgHarvestableTarget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGameplayAbility_Stoneburst)

URpgGameplayAbility_Stoneburst::URpgGameplayAbility_Stoneburst(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActivationGroup = ERpgAbilityActivationGroup::Exclusive_Replaceable;
	AbilityDisplayName = NSLOCTEXT("RpgHarvestingMagic", "StoneburstName", "Stoneburst");
	AbilityDescription = NSLOCTEXT(
		"RpgHarvestingMagic",
		"StoneburstDescription",
		"Shatters a targeted mineral or stone resource with harvesting magic.");

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RpgHarvestingMagicGameplayTags::Ability_Harvesting_Stoneburst);
	SetAssetTags(AssetTags);
	const FGameplayTag DeathTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Death"), false);
	if (DeathTag.IsValid())
	{
		ActivationBlockedTags.AddTag(DeathTag);
	}
}

FGameplayTag URpgGameplayAbility_Stoneburst::GetStoneburstAbilityId()
{
	return RpgHarvestingMagicGameplayTags::Ability_Harvesting_Stoneburst;
}

void URpgGameplayAbility_Stoneburst::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!ActorInfo || !ActorInfo->IsNetAuthority() || !ActorInfo->AvatarActor.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FHitResult TargetHit;
	UObject* HarvestReceiver = nullptr;
	FRpgHarvestRequest Request;
	if (!FindHarvestTarget(*ActorInfo, TargetHit, HarvestReceiver, Request) || !HarvestReceiver)
	{
		K2_OnStoneburstResolved(TargetHit, false);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		K2_OnStoneburstResolved(TargetHit, false);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bHarvestSucceeded = IRpgHarvestableTarget::Execute_CommitHarvest(HarvestReceiver, Request);
	if (bHarvestSucceeded && SuccessGameplayCue.IsValid())
	{
		FGameplayCueParameters CueParameters;
		CueParameters.Instigator = ActorInfo->AvatarActor.Get();
		CueParameters.EffectCauser = ActorInfo->AvatarActor.Get();
		CueParameters.Location = TargetHit.ImpactPoint;
		CueParameters.Normal = TargetHit.ImpactNormal;
		ActorInfo->AbilitySystemComponent->ExecuteGameplayCue(SuccessGameplayCue, CueParameters);
	}

	K2_OnStoneburstResolved(TargetHit, bHarvestSucceeded);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, !bHarvestSucceeded);
}

bool URpgGameplayAbility_Stoneburst::FindHarvestTarget(
	const FGameplayAbilityActorInfo& ActorInfo,
	FHitResult& OutHit,
	UObject*& OutReceiver,
	FRpgHarvestRequest& OutRequest) const
{
	OutHit = FHitResult();
	OutReceiver = nullptr;
	OutRequest = FRpgHarvestRequest();

	AActor* AvatarActor = ActorInfo.AvatarActor.Get();
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}

	FVector ViewLocation = AvatarActor->GetActorLocation();
	FRotator ViewRotation = AvatarActor->GetActorRotation();
	if (AController* Controller = GetControllerFromActorInfo())
	{
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	else
	{
		AvatarActor->GetActorEyesViewPoint(ViewLocation, ViewRotation);
	}

	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * FMath::Max(0.0f, MaxRange);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(StoneburstHarvestTrace), false, AvatarActor);
	QueryParams.AddIgnoredActor(ActorInfo.OwnerActor.Get());

	TArray<FHitResult> Hits;
	const bool bHitAnything = TargetRadius > KINDA_SMALL_NUMBER
		? World->SweepMultiByChannel(
			Hits,
			ViewLocation,
			TraceEnd,
			FQuat::Identity,
			TraceChannel,
			FCollisionShape::MakeSphere(TargetRadius),
			QueryParams)
		: World->LineTraceMultiByChannel(Hits, ViewLocation, TraceEnd, TraceChannel, QueryParams);
	if (!bHitAnything)
	{
		return false;
	}

	Hits.StableSort([](const FHitResult& A, const FHitResult& B)
	{
		return A.Distance < B.Distance;
	});

	for (const FHitResult& Hit : Hits)
	{
		UObject* CandidateReceiver = FindHarvestReceiver(Hit);
		if (!CandidateReceiver)
		{
			continue;
		}

		FRpgHarvestRequest CandidateRequest;
		CandidateRequest.Harvester = AvatarActor;
		CandidateRequest.AbilityId = GetStoneburstAbilityId();
		CandidateRequest.TraceOrigin = ViewLocation;
		CandidateRequest.Hit = Hit;
		CandidateRequest.HarvestPower = FMath::Max(0.0f, HarvestPower);
		if (const URpgHarvestableInstancedMeshComponent* ResourceInstances =
			Cast<URpgHarvestableInstancedMeshComponent>(CandidateReceiver))
		{
			CandidateRequest.ExpectedRevision =
				ResourceInstances->GetResourceInstanceRevision(Hit.Item);
		}
		if (!IRpgHarvestableTarget::Execute_CanAcceptHarvest(CandidateReceiver, CandidateRequest))
		{
			continue;
		}

		OutHit = Hit;
		OutReceiver = CandidateReceiver;
		OutRequest = MoveTemp(CandidateRequest);
		return true;
	}

	return false;
}

UObject* URpgGameplayAbility_Stoneburst::FindHarvestReceiver(const FHitResult& Hit)
{
	// Instance identity belongs to the hit component. Prefer it before an actor-level
	// implementation or unrelated components on an actor with multiple resource meshes.
	if (UPrimitiveComponent* HitComponent = Hit.GetComponent();
		HitComponent && HitComponent->GetClass()->ImplementsInterface(URpgHarvestableTarget::StaticClass()))
	{
		return HitComponent;
	}

	AActor* HitActor = Hit.GetActor();
	if (!HitActor)
	{
		return nullptr;
	}

	if (HitActor->GetClass()->ImplementsInterface(URpgHarvestableTarget::StaticClass()))
	{
		return HitActor;
	}

	TInlineComponentArray<UActorComponent*> Components(HitActor);
	for (UActorComponent* Component : Components)
	{
		if (Component && Component != Hit.GetComponent() && Component->GetClass()->ImplementsInterface(URpgHarvestableTarget::StaticClass()))
		{
			return Component;
		}
	}

	return nullptr;
}
