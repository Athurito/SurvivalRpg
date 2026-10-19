// Ported from Lyra's LyraPawnComponent_CharacterParts.h (Epic Games).

#pragma once

#include "CoreMinimal.h"
#include "Components/PawnComponent.h"
#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "RpgCharacterPartTypes.h"
#include "RpgCosmeticAnimationTypes.h"
#include "RpgPawnComponent_CharacterParts.generated.h"

class UChildActorComponent;
class URpgPawnComponent_CharacterParts;
class USceneComponent;
class USkeletalMeshComponent;
struct FNetDeltaSerializeInfo;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgSpawnedCharacterPartsChanged, URpgPawnComponent_CharacterParts*, ComponentWithChangedParts);

/** One applied character part. The part description replicates; the handle and spawned actor stay local. */
USTRUCT()
struct FRpgAppliedCharacterPartEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FString GetDebugString() const;

private:
	friend FRpgCharacterPartList;
	friend URpgPawnComponent_CharacterParts;

	/** The part being represented. Replicated. */
	UPROPERTY()
	FRpgCharacterPart Part;

	/** Handle returned to the caller that added the part (authority only). */
	UPROPERTY(NotReplicated)
	int32 PartHandle = INDEX_NONE;

	/** Locally spawned child actor component (never on a dedicated server). */
	UPROPERTY(NotReplicated)
	TObjectPtr<UChildActorComponent> SpawnedComponent = nullptr;
};

/** Replicated list of a pawn's applied character parts. Only the authority adds or removes entries. */
USTRUCT(BlueprintType)
struct FRpgCharacterPartList : public FFastArraySerializer
{
	GENERATED_BODY()

	FRpgCharacterPartList()
		: OwnerComponent(nullptr)
	{
	}

	//~FFastArraySerializer contract
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);
	//~End of FFastArraySerializer contract

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FRpgAppliedCharacterPartEntry, FRpgCharacterPartList>(Entries, DeltaParms, *this);
	}

	FRpgCharacterPartHandle AddEntry(const FRpgCharacterPart& NewPart);
	void RemoveEntry(FRpgCharacterPartHandle Handle);
	void ClearAllEntries(bool bBroadcastChangeDelegate);

	FGameplayTagContainer CollectCombinedTags() const;

	void SetOwnerComponent(URpgPawnComponent_CharacterParts* InOwnerComponent)
	{
		OwnerComponent = InOwnerComponent;
	}

private:
	friend URpgPawnComponent_CharacterParts;

	bool SpawnActorForEntry(FRpgAppliedCharacterPartEntry& Entry);
	bool DestroyActorForEntry(FRpgAppliedCharacterPartEntry& Entry);

	/** Replicated list of applied parts. */
	UPROPERTY()
	TArray<FRpgAppliedCharacterPartEntry> Entries;

	/** Component that owns this list. */
	UPROPERTY(NotReplicated)
	TObjectPtr<URpgPawnComponent_CharacterParts> OwnerComponent;

	/** Authority-side counter for handles. */
	int32 PartHandleCounter = 0;
};

template<>
struct TStructOpsTypeTraits<FRpgCharacterPartList> : public TStructOpsTypeTraitsBase2<FRpgCharacterPartList>
{
	enum { WithNetDeltaSerializer = true };
};

/**
 * Spawns cosmetic part actors attached to the owning pawn's gameplay mesh on every non-dedicated machine.
 *
 * The authority adds and removes parts (equipment adds armor parts, the controller parts component adds the body);
 * the list replicates as a FastArray, and each client spawns the parts locally as child actors. Parts are
 * presentation only: gameplay never reads them. The body-style rules may swap the gameplay mesh, for example to an
 * invisible copy while a visible body part follows it.
 */
UCLASS(ClassGroup = (Cosmetics), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgPawnComponent_CharacterParts : public UPawnComponent
{
	GENERATED_BODY()

public:
	URpgPawnComponent_CharacterParts(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~UActorComponent interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnRegister() override;
	//~End of UActorComponent interface

	/** Adds a part to the owning pawn. Authority only; returns a handle for RemoveCharacterPart. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Cosmetics")
	FRpgCharacterPartHandle AddCharacterPart(const FRpgCharacterPart& NewPart);

	/** Removes a part previously added with AddCharacterPart. Authority only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Cosmetics")
	void RemoveCharacterPart(FRpgCharacterPartHandle Handle);

	/** Removes every part. Authority only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Cosmetics")
	void RemoveAllCharacterParts();

	/** Returns the part actors spawned on this machine (empty on a dedicated server). */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, BlueprintCosmetic, Category = "Cosmetics")
	TArray<AActor*> GetCharacterPartActors() const;

	/** If the owner is an ACharacter, returns its gameplay mesh, otherwise nullptr. */
	USkeletalMeshComponent* GetParentMeshComponent() const;

	/** Scene component the parts attach to: the gameplay mesh, else the owner's root component. */
	USceneComponent* GetSceneComponentToAttachTo() const;

	/**
	 * Returns the combined tags of the spawned parts, optionally filtered to tags under RequiredPrefix.
	 * Cosmetic and local: a dedicated server spawns no parts and therefore returns no tags.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, BlueprintCosmetic, Category = "Cosmetics")
	FGameplayTagContainer GetCombinedTags(FGameplayTag RequiredPrefix) const;

	/** The rules used to choose the gameplay mesh from the parts' cosmetic tags. */
	const FRpgAnimBodyStyleSelectionSet& GetBodyMeshes() const { return BodyMeshes; }

	void BroadcastChanged();

	/** Called on each machine after the set of spawned parts changed there. */
	UPROPERTY(BlueprintAssignable, Category = "Cosmetics", BlueprintCallable)
	FRpgSpawnedCharacterPartsChanged OnCharacterPartsChanged;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Designer-tuned rules choosing the gameplay mesh from the parts' cosmetic tags. Leave DefaultMesh empty and the rule
	 * list empty to never touch the gameplay mesh. The swap keeps the running AnimBP and its linked layers.
	 */
	UPROPERTY(EditAnywhere, Category = "Cosmetics")
	FRpgAnimBodyStyleSelectionSet BodyMeshes;

private:
	/** Replicated list of parts. */
	UPROPERTY(Replicated, Transient)
	FRpgCharacterPartList CharacterPartList;
};
