#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"

#include "RpgSkillTreeComponent.generated.h"

class APawn;
class URpgSkillTreeDefinition;
struct FGameplayAbilityActorInfo;
struct FGameplayAbilitySpec;
struct FRpgSkillTreeNode;

/** Saved and owner-replicated progress of one skill tree. Pointer-free, so the host save stores it directly. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgSkillTreeState
{
	GENERATED_BODY()

	/** Tree the progress belongs to; stable across content changes. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Rpg|Skill Tree", meta = (Categories = "SkillTree.Tree"))
	FGameplayTag TreeTag;

	/** Learned nodes in purchase order. Server-authored. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Rpg|Skill Tree", meta = (Categories = "SkillTree.Node"))
	TArray<FGameplayTag> UnlockedNodes;

	/**
	 * Ability ids placed on Q/E/R while a weapon of this tree is in use; index 0 is Q. Empty entries leave the slot to
	 * ability set defaults. Server-authored.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Rpg|Skill Tree", meta = (Categories = "Ability"))
	TArray<FGameplayTag> SlotAbilityIds;

	/** Structural validation for saves and replication: valid unique tags and at most three slots. */
	bool IsValid() const;
};

/** Why a node can or cannot be learned right now. */
UENUM(BlueprintType)
enum class ERpgSkillTreeUnlockResult : uint8
{
	/** Every requirement is met. */
	Unlockable,

	/** The node is already learned. */
	AlreadyUnlocked,

	/** No registered tree has the requested tag. */
	UnknownTree,

	/** The tree has no node with the requested tag. */
	UnknownNode,

	/** Another node of the same exclusive group is learned. */
	ExclusiveConflict,

	/** A prerequisite node is not learned yet. */
	MissingPrerequisite,

	/** Too few points are spent in the tree for the node's row. */
	TreePointsRequired,

	/** The node costs more than the available points. */
	NotEnoughPoints
};

/** Why a learned node can or cannot be refunded right now. */
UENUM(BlueprintType)
enum class ERpgSkillTreeRefundResult : uint8
{
	/** The node can be refunded. */
	Refundable,

	/** The node is not learned. */
	NotUnlocked,

	/** No registered tree has the requested tag. */
	UnknownTree,

	/** The tree has no node with the requested tag. */
	UnknownNode,

	/** Another learned node needs this one as a prerequisite; refund that node first. */
	RequiredByNode,

	/** Another learned node needs the points spent on this one to stay learnable. */
	PointsStillNeeded
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgSkillTreeChangedSignature, FGameplayTag, TreeTag);

/**
 * Character-owned progress of every skill tree, on the PlayerState next to the trade skills.
 *
 * The server owns the learned nodes and the Q/E/R assignment per tree; they replicate only to the owning player and are
 * saved by the host. Points are derived from the level of each tree's mastery skill and never stored. Weapons carry
 * their tree through URpgInventoryFragment_SkillTree; while one is in use, the equipment manager grants its learned
 * nodes. Clients request changes through the Request* RPCs, which the server validates like its own calls.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgSkillTreeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Number of weapon ability slots a tree can assign (Q/E/R). */
	static constexpr int32 SlotCount = 3;

	URpgSkillTreeComponent();

	//~ UActorComponent interface
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End UActorComponent interface

	/** Makes Tree known to requests, restores and the UI. Trees of the asset manager and of equipped weapons register automatically. */
	void RegisterSkillTree(const URpgSkillTreeDefinition* Tree);

	/**
	 * Registers every tree the asset manager knows. Runs at BeginPlay; the progression UI calls it again so trees of
	 * Game Features registered later appear too. Loads the tree assets synchronously.
	 */
	void RegisterSkillTreesFromAssetManager();

	/** Returns the registered tree with TreeTag, or null. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Skill Tree")
	URpgSkillTreeDefinition* FindSkillTree(FGameplayTag TreeTag) const;

	/** Returns every registered tree, for example for the progression overview. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Skill Tree")
	TArray<URpgSkillTreeDefinition*> GetKnownSkillTrees() const;

	/** Returns whether NodeTag of TreeTag is learned. Valid on the server and the owning client. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Skill Tree")
	bool IsNodeUnlocked(FGameplayTag TreeTag, FGameplayTag NodeTag) const;

	/** Returns the learned nodes of TreeTag in purchase order. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Skill Tree")
	TArray<FGameplayTag> GetUnlockedNodes(FGameplayTag TreeTag) const;

	/** Returns the points TreeTag has earned from its mastery skill level. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Skill Tree")
	int32 GetEarnedPoints(FGameplayTag TreeTag) const;

	/** Returns the points spent on learned nodes of TreeTag. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Skill Tree")
	int32 GetSpentPoints(FGameplayTag TreeTag) const;

	/** Returns the earned points of TreeTag not spent yet. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Skill Tree")
	int32 GetAvailablePoints(FGameplayTag TreeTag) const;

	/** Returns whether NodeTag of TreeTag can be learned now, or the first reason it cannot. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Skill Tree")
	ERpgSkillTreeUnlockResult EvaluateUnlock(FGameplayTag TreeTag, FGameplayTag NodeTag) const;

	/**
	 * Returns whether the learned NodeTag of TreeTag can be refunded now, or the first reason it cannot. A node can be
	 * refunded when every other learned node stays learnable without it: no learned node lists it as a prerequisite, and
	 * every point gate is still met.
	 */
	UFUNCTION(BlueprintPure, Category = "Rpg|Skill Tree")
	ERpgSkillTreeRefundResult EvaluateRefund(FGameplayTag TreeTag, FGameplayTag NodeTag) const;

	/** Returns the ability id TreeTag places on weapon ability slot SlotIndex (0 = Q), or an empty tag. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Skill Tree")
	FGameplayTag GetSlotAbilityId(FGameplayTag TreeTag, int32 SlotIndex) const;

	/** Returns the replicated progress of every tree with any state. UI read-only. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Skill Tree")
	const TArray<FRpgSkillTreeState>& GetTreeStates() const { return TreeStates; }

	/** Learns NodeTag of TreeTag. The server validates it like UnlockNode; rejected requests change nothing. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Rpg|Skill Tree")
	void RequestUnlockNode(FGameplayTag TreeTag, FGameplayTag NodeTag);

	/**
	 * Refunds the point of the learned NodeTag of TreeTag for free and takes its abilities off Q/E/R. The server
	 * validates it like RefundNode; rejected requests change nothing.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Rpg|Skill Tree")
	void RequestRefundNode(FGameplayTag TreeTag, FGameplayTag NodeTag);

	/** Refunds every point of TreeTag for free and clears its Q/E/R assignment. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Rpg|Skill Tree")
	void RequestResetTree(FGameplayTag TreeTag);

	/**
	 * Places AbilityIdTag on slot SlotIndex (0 = Q) of TreeTag; an empty tag clears the slot. The ability must come from a
	 * learned node of the tree. An ability already on another slot swaps places with the slot's previous occupant.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Rpg|Skill Tree")
	void RequestAssignSlot(FGameplayTag TreeTag, int32 SlotIndex, FGameplayTag AbilityIdTag);

	/** Authority: learns a node when EvaluateUnlock allows it and places its abilities on free slots. */
	ERpgSkillTreeUnlockResult UnlockNode(FGameplayTag TreeTag, FGameplayTag NodeTag);

	/** Authority: forgets one node when EvaluateRefund allows it, refunds its cost and clears its slots. */
	ERpgSkillTreeRefundResult RefundNode(FGameplayTag TreeTag, FGameplayTag NodeTag);

	/** Authority: refunds every point of a tree. Returns false when nothing changed. */
	bool ResetTree(FGameplayTag TreeTag);

	/** Authority: changes one slot assignment as described for RequestAssignSlot. Returns false when rejected. */
	bool AssignSlot(FGameplayTag TreeTag, int32 SlotIndex, FGameplayTag AbilityIdTag);

	/**
	 * Applies the learned tunings of Tree to BaseValue: Set entries in node order replace it, then Add entries are added,
	 * then Multiply entries are multiplied. Only entries for TuningTag whose ability id is empty or in AbilityIds count.
	 */
	float ResolveAbilityTuning(
		const URpgSkillTreeDefinition& Tree,
		const FGameplayTagContainer& AbilityIds,
		FGameplayTag TuningTag,
		float BaseValue) const;

	/**
	 * Resolves TuningTag for the ability of Spec: the tree comes from the item of the spec's source equipment, the
	 * progress from the owner of ActorInfo. Returns BaseValue for abilities without a tree.
	 */
	static float ResolveAbilityTuningForSpec(
		const FGameplayAbilitySpec& Spec,
		const FGameplayAbilityActorInfo& ActorInfo,
		FGameplayTag TuningTag,
		float BaseValue);

	/** Finds the component of Actor's player: on a PlayerState, a pawn's PlayerState, a controller's PlayerState or Actor itself. */
	static URpgSkillTreeComponent* FindForActor(const AActor* Actor);

	/** Returns the tree whose Q/E/R assignment applies to Pawn: the main-hand weapon's, then the off-hand weapon's. */
	static const URpgSkillTreeDefinition* FindActiveWeaponSkillTree(const APawn* Pawn);

	/** Pointer-free snapshot consumed by host persistence. */
	TArray<FRpgSkillTreeState> ExportSkillTreeStates() const { return TreeStates; }

	/**
	 * Restores a saved snapshot on the server. Registered trees drop unknown nodes and replay the purchases; a tree whose
	 * purchases no longer fit is reset so its points are refunded. States of unregistered trees are kept unchanged.
	 */
	bool RestoreSkillTreeStates(const TArray<FRpgSkillTreeState>& InStates);

	/** Clears every tree, as for profiles from saves before skill trees existed. */
	void ResetSkillTreesToDefaults();

	/** Validates pointer-free save state without mutating runtime progress. */
	static bool ValidateSkillTreeStates(const TArray<FRpgSkillTreeState>& InStates, FString* OutError = nullptr);

	/** Fired on the server and the owning client whenever a tree's learned nodes or slots change. UI read-only. */
	UPROPERTY(BlueprintAssignable, Category = "Rpg|Skill Tree")
	FRpgSkillTreeChangedSignature OnSkillTreeChanged;

protected:
	UFUNCTION()
	void OnRep_TreeStates();

private:
	const FRpgSkillTreeState* FindState(FGameplayTag TreeTag) const;
	FRpgSkillTreeState* FindMutableState(FGameplayTag TreeTag);
	FRpgSkillTreeState& FindOrAddState(FGameplayTag TreeTag);
	int32 GetMasterySkillLevel(const URpgSkillTreeDefinition& Tree) const;
	int32 CalculateSpentPoints(const URpgSkillTreeDefinition& Tree, const FRpgSkillTreeState* State) const;
	ERpgSkillTreeUnlockResult EvaluateUnlockForState(
		const URpgSkillTreeDefinition& Tree,
		const FRpgSkillTreeState* State,
		FGameplayTag NodeTag,
		int32 EarnedPoints) const;
	void AssignFreeSlots(const FRpgSkillTreeNode& Node, FRpgSkillTreeState& State) const;
	void SanitizeSlots(const URpgSkillTreeDefinition& Tree, FRpgSkillTreeState& State) const;
	bool ReplayRestoredState(const URpgSkillTreeDefinition& Tree, FRpgSkillTreeState& State) const;
	void HandleSkillTreeChanged(FGameplayTag TreeTag);
	void RefreshOwnerGrants() const;
	void MarkOwnerSaveDirty() const;

	/** Server-authored progress per tree, replicated only to the owning player. */
	UPROPERTY(ReplicatedUsing = OnRep_TreeStates)
	TArray<FRpgSkillTreeState> TreeStates;

	/** Trees known to this machine; not replicated, every machine registers the same assets. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<const URpgSkillTreeDefinition>> KnownTrees;
};
