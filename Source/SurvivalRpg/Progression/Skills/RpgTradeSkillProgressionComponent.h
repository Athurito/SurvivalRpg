#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/RpgTradeSkillConfigData.h"
#include "Data/RpgTradeSkillState.h"
#include "GameplayTagContainer.h"
#include "RpgTradeSkillProgressionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnTradeSkillTagChanged,
	FGameplayTag, SkillTag,
	const FTradeSkillState&, NewState);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnTradeSkillChanged,
	ETradeSkill, Skill,
	const FTradeSkillState&, NewState);

/**
 * Server-authoritative, owner-only replicated progression for tag-addressed trade skills.
 *
 * Skill identity is stable across save versions because runtime state is keyed by Skill.* gameplay tags. Legacy
 * enum APIs remain as migration adapters for existing Blueprints and route through the same authoritative state.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgTradeSkillProgressionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	static constexpr int32 SupportedMaxSkillLevel = 100;

	URpgTradeSkillProgressionComponent();

	/** Optional designer overrides. Skills without an override use the documented level 1-100 defaults. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Progression|Trade Skills")
	TObjectPtr<URpgTradeSkillConfigData> ConfigData = nullptr;

	/** Server-authored runtime states replicated only to the owning player and persisted by the host save. */
	UPROPERTY(ReplicatedUsing = OnRep_SkillStates, VisibleInstanceOnly, BlueprintReadOnly, Category = "Rpg|Progression|Trade Skills")
	TArray<FTradeSkillState> SkillStates;

	/** Fired for the changed tag on the server and owning client; UI must treat the supplied state as read-only. */
	UPROPERTY(BlueprintAssignable, Category = "Rpg|Progression|Trade Skills")
	FOnTradeSkillTagChanged OnTradeSkillTagChanged;

	/** Legacy enum event retained while existing UI Blueprints migrate to OnTradeSkillTagChanged. */
	UPROPERTY(BlueprintAssignable, Category = "Rpg|Progression|Trade Skills", meta = (DeprecatedProperty, DeprecationMessage = "Use OnTradeSkillTagChanged."))
	FOnTradeSkillChanged OnTradeSkillChanged;

	/** Returns the current level for SkillTag, or level 1 when the tag has no state yet. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Progression|Trade Skills")
	int32 GetSkillLevelByTag(FGameplayTag SkillTag) const;

	/** Returns unspent experience toward SkillTag's next level, or zero for an unknown tag. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Progression|Trade Skills")
	float GetSkillXPByTag(FGameplayTag SkillTag) const;

	/** Returns the XP cost for SkillTag's current level using an authored curve or the default formula. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Progression|Trade Skills")
	float GetXPToNextLevelByTag(FGameplayTag SkillTag) const;

	/** Returns SkillTag's base quantity multiplier; defaults linearly from 1.0 at level 1 to 1.5 at level 100. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Progression|Trade Skills")
	float GetSkillYieldMultiplier(FGameplayTag SkillTag) const;

	/** Returns SkillTag's multiplicative rare-find modifier; defaults linearly from 1.0 at level 1 to 2.0 at level 100. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Progression|Trade Skills")
	float GetSkillRareFindMultiplier(FGameplayTag SkillTag) const;

	/** Adds use-based XP on the server. Returns false for invalid input, clients, or an already capped skill. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rpg|Progression|Trade Skills")
	bool AddSkillXPByTag(FGameplayTag SkillTag, float Amount);

	/** Converts an existing enum id to its stable Skill.* gameplay tag. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Progression|Trade Skills")
	static FGameplayTag GetSkillTagForLegacySkill(ETradeSkill Skill);

	/** Legacy query adapter. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Progression|Trade Skills", meta = (DeprecatedFunction, DeprecationMessage = "Use GetSkillLevelByTag."))
	int32 GetSkillLevel(ETradeSkill Skill) const;

	/** Legacy query adapter. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Progression|Trade Skills", meta = (DeprecatedFunction, DeprecationMessage = "Use GetSkillXPByTag."))
	float GetSkillXP(ETradeSkill Skill) const;

	/** Legacy server mutation adapter. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rpg|Progression|Trade Skills", meta = (DeprecatedFunction, DeprecationMessage = "Use AddSkillXPByTag."))
	void AddSkillXP(ETradeSkill Skill, float Amount);

	/** Pointer-free snapshot consumed by host persistence. */
	TArray<FTradeSkillState> ExportSkillStates() const;

	/** Restores a prevalidated server snapshot and supplies defaults for omitted core skills. */
	bool RestoreSkillStates(const TArray<FTradeSkillState>& InSkillStates);

	/** Resets every configured and core skill to level 1 with zero XP. */
	void ResetSkillStatesToDefaults();

	/** Validates pointer-free save state without mutating runtime progression. */
	static bool ValidateSkillStates(const TArray<FTradeSkillState>& InSkillStates, FString* OutError = nullptr);

	/** Default XP cost used when no curve is authored. */
	static float CalculateDefaultXPToNextLevel(int32 Level);

	/** Default quantity multiplier used when no curve is authored. */
	static float CalculateDefaultYieldMultiplier(int32 Level);

	/** Default rare-find multiplier used when no curve is authored. */
	static float CalculateDefaultRareFindMultiplier(int32 Level);

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_SkillStates();

private:
	void EnsureDefaultSkillStates();
	void BroadcastSkillChanged(const FTradeSkillState& State);
	void TryLevelUp(FTradeSkillState& State);
	void HandleSkillLevelUp(FGameplayTag SkillTag, int32 NewLevel);
	void MarkOwnerSaveDirty() const;

	FTradeSkillState* FindMutableSkillState(FGameplayTag SkillTag);
	const FTradeSkillState* FindSkillState(FGameplayTag SkillTag) const;
	const FTradeSkillConfig* GetConfig(FGameplayTag SkillTag) const;
	int32 GetMaxLevel(FGameplayTag SkillTag) const;
	float GetXPToNextLevel(FGameplayTag SkillTag, int32 Level) const;
	static bool TryGetLegacySkillForTag(FGameplayTag SkillTag, ETradeSkill& OutSkill);
};
