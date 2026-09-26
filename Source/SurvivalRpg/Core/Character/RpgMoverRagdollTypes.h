#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "MoverDataModelTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "RpgMoverRagdollTypes.generated.h"

class UAnimMontage;
class UGameplayAbility;
class UPrimitiveComponent;

/** Reversible living-ragdoll phases. Health/Death owns the separate, terminal death lifecycle. */
UENUM(BlueprintType)
enum class ERpgMoverRagdollPhase : uint8
{
	Inactive,
	Ragdoll,
	GettingUp
};

/** Server-approved ragdoll command and getup selection; client bone transforms are never movement input. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgMoverRagdollState
{
	GENERATED_BODY()

	/** Monotonically increasing command revision on this pawn, including stop commands. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Ragdoll") int32 Revision = 0;
	/** Entry revision identifying this complete ragdoll/getup episode. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Ragdoll") int32 Episode = 0;
	/** Gameplay-owned phase; presentation must not change it. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Ragdoll") ERpgMoverRagdollPhase Phase = ERpgMoverRagdollPhase::Inactive;
	/** Validated capsule anchor, in world space, for the stationary supported-ground pilot. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Ragdoll") FTransform Anchor = FTransform::Identity;
	/** Authority-selected source montage. Only the configured whitelist is accepted. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Ragdoll") TObjectPtr<UAnimMontage> GetUpMontage = nullptr;
	/** Source PoseSearch/Chooser start position, in montage seconds. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Ragdoll") float GetUpStartTime = 0.f;
	/** Playback speed shared by server and owner; source clips currently use 1. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Ragdoll") float GetUpPlayRate = 1.f;
	/** Distinguishes an interrupted stop from a completed getup; neither may restart old presentation. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Ragdoll") bool bCancelled = false;
	/** Correlates ordinary GAS proxy playback with this selection, including successive uses of one asset. */
	UPROPERTY() uint8 GetUpPlayId = 0;
	/** Replicated separately because zero is a valid wrapping GAS play token. */
	UPROPERTY() bool bHasGetUpPlayId = false;
	/** Exact GAS activation correlation, never permission to activate from a network payload. */
	UPROPERTY() FGameplayAbilitySpecHandle AbilityHandle;
	/** Server-issued activation key; interpreted together with its origin flag and ability handle. */
	UPROPERTY() int16 ActivationPredictionKey = 0;
	/** Preserves GAS key origin when correlating the replicated command with an activation. */
	UPROPERTY() bool bServerInitiatedKey = false;
	/** Ground support is observed weakly so NP history cannot retain a destroyed world. */
	UPROPERTY() TWeakObjectPtr<UPrimitiveComponent> Support;
	/** Entry-time world transform used to reject getup if the approved static support moved. */
	UPROPERTY() FTransform SupportTransform = FTransform::Identity;

	bool IsActive() const { return Phase != ERpgMoverRagdollPhase::Inactive; }
	bool MatchesAbility(const UGameplayAbility* Ability) const;
	bool Equals(const FRpgMoverRagdollState& Other) const;
	void Serialize(FArchive& Ar);
	void RetainObjectsForHistory();
	void AddReferencedObjects(FReferenceCollector& Collector);

private:
	// NP frame buffers are not traversed by UObject GC. Retain assets, but never world components.
	TStrongObjectPtr<UAnimMontage> MontageLifetime;
};

/** Ragdoll lifecycle and getup extraction position corrected with the existing Mover NP state. */
USTRUCT()
struct SURVIVALRPG_API FRpgMoverRagdollSyncState : public FMoverDataStructBase
{
	GENERATED_BODY()
	/** Historical authority command restored with this Mover simulation frame. */
	UPROPERTY() FRpgMoverRagdollState State;
	/** Last transition applied in simulation, preventing duplicate movement effects after replay. */
	UPROPERTY() int32 AppliedRevision = 0;
	/** Next authoritative montage extraction position in seconds. */
	UPROPERTY() float MontagePosition = 0.f;
	virtual FMoverDataStructBase* Clone() const override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
	virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
};

template<> struct TStructOpsTypeTraits<FRpgMoverRagdollState> : TStructOpsTypeTraitsBase2<FRpgMoverRagdollState>
{ enum { WithCopy = true }; };
template<> struct TStructOpsTypeTraits<FRpgMoverRagdollSyncState> : TStructOpsTypeTraitsBase2<FRpgMoverRagdollSyncState>
{ enum { WithNetSerializer = true, WithCopy = true }; };
