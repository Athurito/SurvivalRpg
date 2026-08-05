#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"

#include "RpgWorldStorageKnowledgeComponent.generated.h"

/** Pointer-free world-storage knowledge snapshot consumed by the host save layer. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgWorldStorageKnowledgeSaveData
{
	GENERATED_BODY()

	/** Concrete Storage.Knowledge.* discoveries shared by the world; runtime objects are never retained. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|Storage Knowledge")
	FGameplayTagContainer KnowledgeTags;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FRpgWorldStorageKnowledgeChanged,
	FGameplayTag, KnowledgeTag,
	bool, bIsKnown);

/**
 * Server-authoritative world knowledge used by storage upgrade prerequisites.
 *
 * The component is owned by the GameState, replicates to every relevant client,
 * and exposes pointer-free import/export hooks without owning the SaveGame schema.
 */
UCLASS(BlueprintType, ClassGroup = (Storage), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgWorldStorageKnowledgeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	explicit URpgWorldStorageKnowledgeComponent(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Adds one concrete Storage.Knowledge.* tag on the server; returns true only for a new discovery. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rpg|Storage|Knowledge")
	bool GrantKnowledgeTag(FGameplayTag KnowledgeTag);

	/** Adds all valid concrete knowledge tags and returns how many were newly discovered. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rpg|Storage|Knowledge")
	int32 GrantKnowledgeTags(const FGameplayTagContainer& InKnowledgeTags);

	/** Returns whether the replicated world snapshot contains this exact knowledge tag. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rpg|Storage|Knowledge")
	bool HasKnowledgeTag(FGameplayTag KnowledgeTag) const;

	/** Returns whether every requested tag is present exactly; an empty requirement succeeds. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rpg|Storage|Knowledge")
	bool HasAllKnowledgeTags(const FGameplayTagContainer& RequiredKnowledgeTags) const;

	/** Returns the replicated read-only knowledge snapshot for prerequisite checks and presentation. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rpg|Storage|Knowledge")
	FGameplayTagContainer GetKnowledgeTags() const { return KnowledgeTags; }

	/** Creates a pointer-free snapshot for the host-owned world save coordinator. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rpg|Storage|Knowledge|Save")
	FRpgWorldStorageKnowledgeSaveData ExportSaveData() const;

	/** Replaces the complete server snapshot from validated save data; unchanged imports still succeed. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rpg|Storage|Knowledge|Save")
	bool ImportSaveData(const FRpgWorldStorageKnowledgeSaveData& SaveData);

	/** Validates that a pointer-free snapshot contains only concrete Storage.Knowledge.* tags. */
	static bool ValidateSaveData(
		const FRpgWorldStorageKnowledgeSaveData& SaveData,
		FString* OutError = nullptr);

	/** Fired locally on the server and clients whenever one concrete discovery changes. */
	UPROPERTY(BlueprintAssignable, Category = "Rpg|Storage|Knowledge")
	FRpgWorldStorageKnowledgeChanged OnKnowledgeChanged;

private:
	UFUNCTION()
	void OnRep_KnowledgeTags(FGameplayTagContainer PreviousTags);

	static bool IsConcreteStorageKnowledgeTag(FGameplayTag KnowledgeTag);
	void BroadcastDifferences(
		const FGameplayTagContainer& PreviousTags,
		const FGameplayTagContainer& NewTags);
	void NotifyAuthorityMutation(const FGameplayTagContainer& PreviousTags);

private:
	/** World-shared discoveries mutated by the server, replicated to all clients, and exported for host persistence. */
	UPROPERTY(ReplicatedUsing = OnRep_KnowledgeTags, BlueprintReadOnly, Category = "Rpg|Storage|Knowledge", meta = (AllowPrivateAccess = "true"))
	FGameplayTagContainer KnowledgeTags;
};
