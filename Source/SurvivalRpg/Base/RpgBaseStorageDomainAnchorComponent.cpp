#include "RpgBaseStorageDomainAnchorComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgBaseStorageDomainAnchorComponent)

bool FRpgBaseStorageDomainAnchorVisualState::IsValid() const
{
	return FMath::IsFinite(FillRatio) &&
		FMath::IsFinite(StrainRatio) &&
		FillRatio >= 0.0f && FillRatio <= 1.0f &&
		StrainRatio >= 0.0f && StrainRatio <= 1.0f;
}

bool FRpgBaseStorageDomainAnchorVisualState::operator==(
	const FRpgBaseStorageDomainAnchorVisualState& Other) const
{
	return Status == Other.Status &&
		FMath::IsNearlyEqual(FillRatio, Other.FillRatio) &&
		FMath::IsNearlyEqual(StrainRatio, Other.StrainRatio);
}

URpgBaseStorageDomainAnchorComponent::URpgBaseStorageDomainAnchorComponent(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void URpgBaseStorageDomainAnchorComponent::BeginPlay()
{
	Super::BeginPlay();
	BroadcastVisualState();
}

void URpgBaseStorageDomainAnchorComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, AnchorId);
	DOREPLIFETIME(ThisClass, DomainTag);
	DOREPLIFETIME(ThisClass, VisualState);
}

bool URpgBaseStorageDomainAnchorComponent::ConfigureAnchor(
	FName NewAnchorId,
	FGameplayTag NewDomainTag)
{
	AActor* OwnerActor = GetOwner();
	UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;
	const bool bRuntime = World && World->IsGameWorld() && IsRegistered();
	if (NewAnchorId.IsNone() || !NewDomainTag.IsValid() ||
		NewDomainTag == RpgGameplayTags::Storage_Domain ||
		!NewDomainTag.MatchesTag(RpgGameplayTags::Storage_Domain) ||
		(bRuntime && (!OwnerActor || !OwnerActor->HasAuthority())))
	{
		return false;
	}

	AnchorId = NewAnchorId;
	DomainTag = NewDomainTag;
	if (bRuntime)
	{
		OwnerActor->ForceNetUpdate();
	}
	return true;
}

bool URpgBaseStorageDomainAnchorComponent::SetVisualState(
	const FRpgBaseStorageDomainAnchorVisualState& NewVisualState)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() ||
		!NewVisualState.IsValid() || VisualState == NewVisualState)
	{
		return false;
	}

	VisualState = NewVisualState;
	BroadcastVisualState();
	OwnerActor->ForceNetUpdate();
	return true;
}

void URpgBaseStorageDomainAnchorComponent::OnRep_VisualState()
{
	BroadcastVisualState();
}

void URpgBaseStorageDomainAnchorComponent::BroadcastVisualState()
{
	OnVisualStateChanged.Broadcast(this, VisualState);
}

#if WITH_EDITOR
EDataValidationResult URpgBaseStorageDomainAnchorComponent::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(
		Super::IsDataValid(Context),
		EDataValidationResult::Valid);
	if (AnchorId.IsNone())
	{
		Context.AddError(NSLOCTEXT(
			"RpgBaseStorageDomainAnchor",
			"MissingAnchorId",
			"Base storage domain anchors require a non-empty stable AnchorId."));
		Result = EDataValidationResult::Invalid;
	}

	if (!DomainTag.IsValid() ||
		DomainTag == RpgGameplayTags::Storage_Domain ||
		!DomainTag.MatchesTag(RpgGameplayTags::Storage_Domain))
	{
		Context.AddError(NSLOCTEXT(
			"RpgBaseStorageDomainAnchor",
			"InvalidDomainTag",
			"Base storage domain anchors require a DomainTag that is a strict child of Storage.Domain."));
		Result = EDataValidationResult::Invalid;
	}

	if (!VisualState.IsValid())
	{
		Context.AddError(NSLOCTEXT(
			"RpgBaseStorageDomainAnchor",
			"InvalidVisualState",
			"Base storage domain anchor FillRatio and StrainRatio must remain finite values in the inclusive range 0..1."));
		Result = EDataValidationResult::Invalid;
	}

	if (const AActor* OwnerActor = GetOwner())
	{
		TInlineComponentArray<URpgBaseStorageDomainAnchorComponent*> Anchors(OwnerActor);
		bool bReportedDuplicateId = false;
		bool bReportedDuplicateDomain = false;
		for (const URpgBaseStorageDomainAnchorComponent* Other : Anchors)
		{
			if (!IsValid(Other) || Other == this)
			{
				continue;
			}

			if (!bReportedDuplicateId && Other->AnchorId == AnchorId)
			{
				Context.AddError(FText::Format(
					NSLOCTEXT(
						"RpgBaseStorageDomainAnchor",
						"DuplicateAnchorId",
						"Storage domain anchor id '{0}' is also used by component '{1}'. Anchor ids must be unique per actor."),
					FText::FromName(AnchorId),
					FText::FromName(Other->GetFName())));
				Result = EDataValidationResult::Invalid;
				bReportedDuplicateId = true;
			}

			if (!bReportedDuplicateDomain && DomainTag.IsValid() &&
				Other->DomainTag == DomainTag)
			{
				Context.AddError(FText::Format(
					NSLOCTEXT(
						"RpgBaseStorageDomainAnchor",
						"DuplicateDomainTag",
						"Storage domain '{0}' is also represented by component '{1}'. One actor may expose at most one anchor per domain."),
					FText::FromString(DomainTag.ToString()),
					FText::FromName(Other->GetFName())));
				Result = EDataValidationResult::Invalid;
				bReportedDuplicateDomain = true;
			}

			if (bReportedDuplicateId && bReportedDuplicateDomain)
			{
				break;
			}
		}
	}

	return Result;
}
#endif
