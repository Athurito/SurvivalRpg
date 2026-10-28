#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"

#include "RpgRewardClaimComponent.generated.h"

class URpgInventoryManagerComponent;
class URpgRewardClaimDefinition;

/** One guaranteed reward a player has earned but not claimed yet. Owner-replicated runtime state. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgRewardClaim
{
	GENERATED_BODY()

	/**
	 * Stable identity of the grant, for example "SkillTree.Tree.Harvesting/Mastery/3". A player holds or resolves each
	 * id at most once, so the same source can never grant it twice.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	FName ClaimId;

	/** What the claim gives. */
	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	TObjectPtr<const URpgRewardClaimDefinition> Reward = nullptr;

	/** What granted it, for example the mastery skill Skill.Mastery.Harvesting; UI context only. */
	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	FGameplayTag SourceTag;

	/** Level of the source that granted it, for example the mastery level of a milestone, or 0; UI context only. */
	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	int32 SourceLevel = 0;
};

/** Pointer-free saved form of one pending claim. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgRewardClaimSaveEntry
{
	GENERATED_BODY()

	/** Stable identity of the grant. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|Rewards")
	FName ClaimId;

	/** Reward asset of the claim. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|Rewards")
	TSoftObjectPtr<const URpgRewardClaimDefinition> Reward;

	/** Source of the grant; UI context only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|Rewards")
	FGameplayTag SourceTag;

	/** Source level of the grant; UI context only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|Rewards")
	int32 SourceLevel = 0;
};

/** Saved reward claims of one player: what is still open and which grants were already claimed. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgRewardClaimSaveData
{
	GENERATED_BODY()

	/** Claims the player has not claimed yet, in grant order. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|Rewards")
	TArray<FRpgRewardClaimSaveEntry> PendingClaims;

	/** Ids of claims the player already claimed; their sources can never grant them again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|Rewards")
	TArray<FName> ResolvedClaimIds;
};

/** Outcome of claiming a reward. */
UENUM(BlueprintType)
enum class ERpgRewardClaimResult : uint8
{
	/** The items are in the player's inventory and the claim is resolved. */
	Claimed,

	/** The player holds no open claim with that id. */
	UnknownClaim,

	/** The chosen option does not exist. */
	InvalidOption,

	/** The player's inventory has no room for the items; the claim stays open. */
	NoRoom,

	/** The reward could not be delivered, for example on a client or without an inventory; the claim stays open. */
	Rejected
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRpgRewardClaimsChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRpgRewardClaimResultSignature, FName, ClaimId, ERpgRewardClaimResult, Result);

/**
 * Guaranteed rewards of one player, on the PlayerState next to the progression components.
 *
 * Sources such as mastery milestones grant a claim with a stable id. The server owns the claims; the open ones
 * replicate only to the owning player, and the host saves open and resolved ids with the player. Claiming puts the
 * reward into the player's inventory in one all-or-nothing commit. Without room the claim stays open, so a full
 * inventory never loses a reward, and a resolved id can never be granted or claimed again, also after a reconnect.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgRewardClaimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URpgRewardClaimComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Grants Reward under ClaimId. Server only; returns false for an invalid reward or an id the player already holds
	 * or has claimed, so repeated grants from one source are harmless.
	 */
	bool GrantClaim(FName ClaimId, const URpgRewardClaimDefinition* Reward, FGameplayTag SourceTag, int32 SourceLevel);

	/**
	 * Claims ClaimId with the option at OptionIndex (0 for a loot pool) into the owning player's inventory. Server
	 * only. Without room nothing changes and the claim stays open.
	 */
	ERpgRewardClaimResult ClaimReward(FName ClaimId, int32 OptionIndex);

	/** Asks the server to claim ClaimId with OptionIndex; the result arrives through OnClaimResult. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Rpg|Rewards")
	void RequestClaimReward(FName ClaimId, int32 OptionIndex);

	/** Open claims in grant order. Read-only for UI. */
	const TArray<FRpgRewardClaim>& GetPendingClaims() const { return PendingClaims; }

	/** The open claim ClaimId, or null. */
	const FRpgRewardClaim* FindPendingClaim(FName ClaimId) const;

	/** True when the player already claimed ClaimId. Server only, since resolved ids are not replicated. */
	bool HasResolvedClaim(FName ClaimId) const { return ResolvedClaimIds.Contains(ClaimId); }

	FRpgRewardClaimSaveData ExportSaveData() const;

	/** Replaces every claim with SaveData. Server only; invalid data changes nothing and returns false. */
	bool ImportSaveData(const FRpgRewardClaimSaveData& SaveData);

	/** Forgets every claim, for a player without saved claims. Server only. */
	void ResetToDefaults();

	/** True when every entry has an id and a reward, and no id is open twice or both open and resolved. */
	static bool ValidateSaveData(const FRpgRewardClaimSaveData& SaveData, FString* OutError = nullptr);

	/** The reward claims of the player state that owns Actor, or of Actor itself. */
	static URpgRewardClaimComponent* FindForActor(const AActor* Actor);

	/** Fired on the server and the owning client whenever the open claims change. */
	UPROPERTY(BlueprintAssignable, Category = "Rpg|Rewards")
	FRpgRewardClaimsChangedSignature OnClaimsChanged;

	/** Fired on the requesting client, and on the server, with the outcome of each claim request. */
	UPROPERTY(BlueprintAssignable, Category = "Rpg|Rewards")
	FRpgRewardClaimResultSignature OnClaimResult;

private:
	UFUNCTION()
	void OnRep_PendingClaims();

	UFUNCTION(Client, Reliable)
	void ClientReportClaimResult(FName ClaimId, ERpgRewardClaimResult Result);

	URpgInventoryManagerComponent* FindOwnerInventory() const;
	bool HasAuthority() const;
	void HandleClaimsChanged();
	void MarkOwnerSaveDirty() const;

	/** Open claims. Server-mutated, replicated only to the owning player and saved. */
	UPROPERTY(ReplicatedUsing = OnRep_PendingClaims)
	TArray<FRpgRewardClaim> PendingClaims;

	/** Ids already claimed. Server only and saved; never replicated. */
	TArray<FName> ResolvedClaimIds;
};
