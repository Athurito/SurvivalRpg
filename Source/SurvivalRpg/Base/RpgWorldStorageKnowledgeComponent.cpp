#include "RpgWorldStorageKnowledgeComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgWorldStorageKnowledgeComponent)

URpgWorldStorageKnowledgeComponent::URpgWorldStorageKnowledgeComponent(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void URpgWorldStorageKnowledgeComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, KnowledgeTags);
}

bool URpgWorldStorageKnowledgeComponent::GrantKnowledgeTag(FGameplayTag KnowledgeTag)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !IsConcreteStorageKnowledgeTag(KnowledgeTag) ||
		KnowledgeTags.HasTagExact(KnowledgeTag))
	{
		return false;
	}

	const FGameplayTagContainer PreviousTags = KnowledgeTags;
	KnowledgeTags.AddTag(KnowledgeTag);
	NotifyAuthorityMutation(PreviousTags);
	return true;
}

int32 URpgWorldStorageKnowledgeComponent::GrantKnowledgeTags(
	const FGameplayTagContainer& InKnowledgeTags)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return 0;
	}

	const FGameplayTagContainer PreviousTags = KnowledgeTags;
	for (const FGameplayTag& KnowledgeTag : InKnowledgeTags)
	{
		if (IsConcreteStorageKnowledgeTag(KnowledgeTag))
		{
			KnowledgeTags.AddTag(KnowledgeTag);
		}
	}

	FGameplayTagContainer AddedTags = KnowledgeTags;
	AddedTags.RemoveTags(PreviousTags);
	if (!AddedTags.IsEmpty())
	{
		NotifyAuthorityMutation(PreviousTags);
	}
	return AddedTags.Num();
}

bool URpgWorldStorageKnowledgeComponent::HasKnowledgeTag(FGameplayTag KnowledgeTag) const
{
	return KnowledgeTag.IsValid() && KnowledgeTags.HasTagExact(KnowledgeTag);
}

bool URpgWorldStorageKnowledgeComponent::HasAllKnowledgeTags(
	const FGameplayTagContainer& RequiredKnowledgeTags) const
{
	return KnowledgeTags.HasAllExact(RequiredKnowledgeTags);
}

FRpgWorldStorageKnowledgeSaveData URpgWorldStorageKnowledgeComponent::ExportSaveData() const
{
	FRpgWorldStorageKnowledgeSaveData SaveData;
	SaveData.KnowledgeTags = KnowledgeTags;
	return SaveData;
}

bool URpgWorldStorageKnowledgeComponent::ImportSaveData(
	const FRpgWorldStorageKnowledgeSaveData& SaveData)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !ValidateSaveData(SaveData))
	{
		return false;
	}

	if (KnowledgeTags == SaveData.KnowledgeTags)
	{
		return true;
	}

	const FGameplayTagContainer PreviousTags = KnowledgeTags;
	KnowledgeTags = SaveData.KnowledgeTags;
	NotifyAuthorityMutation(PreviousTags);
	return true;
}

bool URpgWorldStorageKnowledgeComponent::ValidateSaveData(
	const FRpgWorldStorageKnowledgeSaveData& SaveData,
	FString* OutError)
{
	for (const FGameplayTag& KnowledgeTag : SaveData.KnowledgeTags)
	{
		if (!IsConcreteStorageKnowledgeTag(KnowledgeTag))
		{
			if (OutError)
			{
				*OutError = FString::Printf(
					TEXT("Storage knowledge snapshot contains invalid or non-concrete tag '%s'."),
					*KnowledgeTag.ToString());
			}
			return false;
		}
	}

	if (OutError)
	{
		OutError->Reset();
	}
	return true;
}

void URpgWorldStorageKnowledgeComponent::OnRep_KnowledgeTags(
	FGameplayTagContainer PreviousTags)
{
	BroadcastDifferences(PreviousTags, KnowledgeTags);
}

bool URpgWorldStorageKnowledgeComponent::IsConcreteStorageKnowledgeTag(
	FGameplayTag KnowledgeTag)
{
	return KnowledgeTag.IsValid() &&
		KnowledgeTag != RpgGameplayTags::Storage_Knowledge &&
		KnowledgeTag.MatchesTag(RpgGameplayTags::Storage_Knowledge);
}

void URpgWorldStorageKnowledgeComponent::BroadcastDifferences(
	const FGameplayTagContainer& PreviousTags,
	const FGameplayTagContainer& NewTags)
{
	FGameplayTagContainer RemovedTags = PreviousTags;
	RemovedTags.RemoveTags(NewTags);
	for (const FGameplayTag& RemovedTag : RemovedTags)
	{
		OnKnowledgeChanged.Broadcast(RemovedTag, false);
	}

	FGameplayTagContainer AddedTags = NewTags;
	AddedTags.RemoveTags(PreviousTags);
	for (const FGameplayTag& AddedTag : AddedTags)
	{
		OnKnowledgeChanged.Broadcast(AddedTag, true);
	}
}

void URpgWorldStorageKnowledgeComponent::NotifyAuthorityMutation(
	const FGameplayTagContainer& PreviousTags)
{
	BroadcastDifferences(PreviousTags, KnowledgeTags);
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
		if (UWorld* World = OwnerActor->GetWorld())
		{
			if (ARpgGameModeBase* GameMode =
					World->GetAuthGameMode<ARpgGameModeBase>())
			{
				GameMode->MarkStorageKnowledgeSaveDirty();
			}
		}
	}
}
