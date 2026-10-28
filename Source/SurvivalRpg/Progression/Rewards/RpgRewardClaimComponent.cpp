#include "RpgRewardClaimComponent.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Misc/Crc.h"
#include "Net/UnrealNetwork.h"
#include "RpgRewardClaimDefinition.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Inventory/IPickupable.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootResolver.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgRewardClaimComponent)

URpgRewardClaimComponent::URpgRewardClaimComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void URpgRewardClaimComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(URpgRewardClaimComponent, PendingClaims, COND_OwnerOnly, REPNOTIFY_Always);
}

bool URpgRewardClaimComponent::GrantClaim(
	const FName ClaimId,
	const URpgRewardClaimDefinition* Reward,
	const FGameplayTag SourceTag,
	const int32 SourceLevel)
{
	if (!HasAuthority() || ClaimId.IsNone() || !Reward || !Reward->HasValidConfiguration() ||
		FindPendingClaim(ClaimId) || HasResolvedClaim(ClaimId))
	{
		return false;
	}

	FRpgRewardClaim& Claim = PendingClaims.AddDefaulted_GetRef();
	Claim.ClaimId = ClaimId;
	Claim.Reward = Reward;
	Claim.SourceTag = SourceTag;
	Claim.SourceLevel = SourceLevel;
	HandleClaimsChanged();
	MarkOwnerSaveDirty();
	return true;
}

ERpgRewardClaimResult URpgRewardClaimComponent::ClaimReward(const FName ClaimId, const int32 OptionIndex)
{
	if (!HasAuthority())
	{
		return ERpgRewardClaimResult::Rejected;
	}

	const int32 ClaimIndex = PendingClaims.IndexOfByPredicate([ClaimId](const FRpgRewardClaim& Claim)
	{
		return Claim.ClaimId == ClaimId;
	});
	if (ClaimIndex == INDEX_NONE)
	{
		return ERpgRewardClaimResult::UnknownClaim;
	}

	const FRpgRewardClaim Claim = PendingClaims[ClaimIndex];
	const URpgRewardClaimDefinition* Reward = Claim.Reward;
	if (!Reward || OptionIndex < 0 || OptionIndex >= Reward->GetClaimOptionCount())
	{
		return ERpgRewardClaimResult::InvalidOption;
	}

	URpgInventoryManagerComponent* Inventory = FindOwnerInventory();
	if (!Inventory)
	{
		return ERpgRewardClaimResult::Rejected;
	}

	FInventoryPickup Pickup;
	if (Reward->IsChoice())
	{
		const FRpgRewardClaimOption& Option = Reward->Options[OptionIndex];
		FPickupTemplate& Template = Pickup.Templates.AddDefaulted_GetRef();
		Template.ItemDef = Option.ItemDefinition;
		Template.StackCount = Option.Count;
	}
	else
	{
		// The claim id seeds the roll, so a retry after a full inventory rolls the same items.
		FRpgLootRollContext Context;
		Context.SourceActor = GetOwner();
		Context.RecipientActor = GetOwner();
		if (Claim.SourceTag.IsValid())
		{
			Context.SourceTags.AddTag(Claim.SourceTag);
		}
		Context.SourceLevel = FMath::Max(1, Claim.SourceLevel);
		Context.Seed = static_cast<int32>(FCrc::StrCrc32(*ClaimId.ToString()));
		if (!FRpgLootResolver::RollAndMaterialize(Reward->LootTable, Context, GetOwner(), Pickup))
		{
			return ERpgRewardClaimResult::Rejected;
		}
	}

	// A loot pool that rolled nothing still resolves the claim, so it cannot stay open forever.
	if (!Pickup.Templates.IsEmpty() || !Pickup.Instances.IsEmpty())
	{
		TArray<FRpgInventoryItemId> AffectedItemIds;
		if (!Inventory->CanAddPickupBatch(Pickup) || !Inventory->AddPickupBatch(Pickup, AffectedItemIds).IsSuccess())
		{
			return ERpgRewardClaimResult::NoRoom;
		}
	}

	PendingClaims.RemoveAt(ClaimIndex);
	ResolvedClaimIds.AddUnique(ClaimId);
	HandleClaimsChanged();
	MarkOwnerSaveDirty();
	return ERpgRewardClaimResult::Claimed;
}

void URpgRewardClaimComponent::RequestClaimReward_Implementation(const FName ClaimId, const int32 OptionIndex)
{
	const ERpgRewardClaimResult Result = ClaimReward(ClaimId, OptionIndex);
	OnClaimResult.Broadcast(ClaimId, Result);
	ClientReportClaimResult(ClaimId, Result);
}

void URpgRewardClaimComponent::ClientReportClaimResult_Implementation(const FName ClaimId, const ERpgRewardClaimResult Result)
{
	// A listen server's own player already heard the result from the server side.
	if (!HasAuthority())
	{
		OnClaimResult.Broadcast(ClaimId, Result);
	}
}

const FRpgRewardClaim* URpgRewardClaimComponent::FindPendingClaim(const FName ClaimId) const
{
	return PendingClaims.FindByPredicate([ClaimId](const FRpgRewardClaim& Claim)
	{
		return Claim.ClaimId == ClaimId;
	});
}

FRpgRewardClaimSaveData URpgRewardClaimComponent::ExportSaveData() const
{
	FRpgRewardClaimSaveData SaveData;
	for (const FRpgRewardClaim& Claim : PendingClaims)
	{
		FRpgRewardClaimSaveEntry& Entry = SaveData.PendingClaims.AddDefaulted_GetRef();
		Entry.ClaimId = Claim.ClaimId;
		Entry.Reward = Claim.Reward.Get();
		Entry.SourceTag = Claim.SourceTag;
		Entry.SourceLevel = Claim.SourceLevel;
	}
	SaveData.ResolvedClaimIds = ResolvedClaimIds;
	return SaveData;
}

bool URpgRewardClaimComponent::ImportSaveData(const FRpgRewardClaimSaveData& SaveData)
{
	if (!HasAuthority() || !ValidateSaveData(SaveData))
	{
		return false;
	}

	TArray<FRpgRewardClaim> RestoredClaims;
	for (const FRpgRewardClaimSaveEntry& Entry : SaveData.PendingClaims)
	{
		// A reward asset removed from the game drops its open claim; dev saves need no migration.
		const URpgRewardClaimDefinition* Reward = Entry.Reward.LoadSynchronous();
		if (!Reward)
		{
			continue;
		}
		FRpgRewardClaim& Claim = RestoredClaims.AddDefaulted_GetRef();
		Claim.ClaimId = Entry.ClaimId;
		Claim.Reward = Reward;
		Claim.SourceTag = Entry.SourceTag;
		Claim.SourceLevel = Entry.SourceLevel;
	}

	PendingClaims = MoveTemp(RestoredClaims);
	ResolvedClaimIds = SaveData.ResolvedClaimIds;
	HandleClaimsChanged();
	return true;
}

void URpgRewardClaimComponent::ResetToDefaults()
{
	if (!HasAuthority())
	{
		return;
	}
	PendingClaims.Reset();
	ResolvedClaimIds.Reset();
	HandleClaimsChanged();
}

bool URpgRewardClaimComponent::ValidateSaveData(const FRpgRewardClaimSaveData& SaveData, FString* OutError)
{
	auto Fail = [OutError](const FString& Message)
	{
		if (OutError)
		{
			*OutError = Message;
		}
		return false;
	};

	TSet<FName> SeenIds;
	for (const FName ResolvedId : SaveData.ResolvedClaimIds)
	{
		if (ResolvedId.IsNone() || SeenIds.Contains(ResolvedId))
		{
			return Fail(FString::Printf(TEXT("Resolved reward claim id %s is empty or repeated."), *ResolvedId.ToString()));
		}
		SeenIds.Add(ResolvedId);
	}
	for (const FRpgRewardClaimSaveEntry& Entry : SaveData.PendingClaims)
	{
		if (Entry.ClaimId.IsNone() || Entry.Reward.IsNull() || SeenIds.Contains(Entry.ClaimId))
		{
			return Fail(FString::Printf(
				TEXT("Open reward claim %s has no id or reward, or repeats another claim."),
				*Entry.ClaimId.ToString()));
		}
		SeenIds.Add(Entry.ClaimId);
	}
	return true;
}

URpgRewardClaimComponent* URpgRewardClaimComponent::FindForActor(const AActor* Actor)
{
	const APlayerState* PlayerState = Cast<APlayerState>(Actor);
	if (!PlayerState)
	{
		if (const APawn* Pawn = Cast<APawn>(Actor))
		{
			PlayerState = Pawn->GetPlayerState();
		}
		else if (const AController* Controller = Cast<AController>(Actor))
		{
			PlayerState = Controller->PlayerState;
		}
	}
	return PlayerState ? PlayerState->FindComponentByClass<URpgRewardClaimComponent>() : nullptr;
}

void URpgRewardClaimComponent::OnRep_PendingClaims()
{
	OnClaimsChanged.Broadcast();
}

URpgInventoryManagerComponent* URpgRewardClaimComponent::FindOwnerInventory() const
{
	const ARpgPlayerState* PlayerState = Cast<ARpgPlayerState>(GetOwner());
	return PlayerState ? PlayerState->GetInventoryManagerComponent() : nullptr;
}

bool URpgRewardClaimComponent::HasAuthority() const
{
	const AActor* Owner = GetOwner();
	return Owner && Owner->HasAuthority();
}

void URpgRewardClaimComponent::HandleClaimsChanged()
{
	OnClaimsChanged.Broadcast();
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
}

void URpgRewardClaimComponent::MarkOwnerSaveDirty() const
{
	const AActor* Owner = GetOwner();
	APlayerController* PlayerController = Owner ? Cast<APlayerController>(Owner->GetOwner()) : nullptr;
	if (ARpgGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARpgGameModeBase>() : nullptr)
	{
		GameMode->MarkPlayerSaveDirty(PlayerController);
	}
}
