#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SurvivalRpg/Core/Game/RpgWorldSaveParticipant.h"

#include "RpgRecipeUnlockComponent.generated.h"

class URpgCraftingRecipeDefinition;

/** GameplayMessage payload for global crafting recipe unlock changes. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgRecipeUnlockChangeMessage
{
	GENERATED_BODY()

	/** Component that owns the replicated unlock list, usually the game state component. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|Unlocks")
	TObjectPtr<UActorComponent> UnlockOwner = nullptr;

	/** Recipe unlock tag that was added or refreshed. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|Unlocks")
	FGameplayTag RecipeUnlockTag;

	/** True when the tag was newly added on the server. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|Unlocks")
	bool bNewlyUnlocked = false;
};

/** Pointer-free recipe knowledge of one world, the payload of the recipe unlock store's world save entry. */
USTRUCT()
struct SURVIVALRPG_API FRpgRecipeUnlockSaveData
{
	GENERATED_BODY()

	/** Concrete Recipe.* unlock tags the world has learned. Host-authoritative save data; never replicated as is. */
	UPROPERTY(SaveGame)
	FGameplayTagContainer UnlockedRecipeTags;
};

/**
 * Replicated recipe knowledge of the current world.
 *
 * This component lives on the GameState, so a recipe unlock becomes available to every player in the world, for
 * example the base recipe of a rune someone found. The server owns it, and it is saved with the host's world snapshot
 * as a world save participant, so a learned recipe stays known after the world is loaded again.
 */
UCLASS(BlueprintType, ClassGroup = (Crafting), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgRecipeUnlockComponent : public UActorComponent, public IRpgWorldSaveParticipant
{
	GENERATED_BODY()

public:
	explicit URpgRecipeUnlockComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//~ IRpgWorldSaveParticipant interface
	virtual FName GetWorldSaveFeatureId() const override;
	virtual bool CaptureWorldSaveData(FRpgWorldFeatureSaveData& OutData) override;
	virtual bool RestoreWorldSaveData(const FRpgWorldFeatureSaveData* SavedData) override;
	//~ End IRpgWorldSaveParticipant interface

	/** Schema of this store's world save payload. */
	static constexpr int32 CurrentSaveSchemaVersion = 1;

	/**
	 * Adds one recipe unlock tag for the whole world. Server-authoritative; returns false if the tag is not a concrete
	 * Recipe.* tag or is already unlocked. A new unlock replicates to every player and marks the world save dirty.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting|Unlocks")
	bool UnlockRecipeTag(FGameplayTag RecipeUnlockTag);

	/** Adds the unlock tag from a recipe definition, if the recipe has one. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting|Unlocks")
	bool UnlockRecipe(const URpgCraftingRecipeDefinition* RecipeDefinition);

	/** Returns true when this global store contains the exact recipe unlock tag. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting|Unlocks")
	bool IsRecipeTagUnlocked(FGameplayTag RecipeUnlockTag) const;

	/** Returns true when a recipe is unlocked by default or by the replicated global unlock list. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting|Unlocks")
	bool IsRecipeUnlocked(const URpgCraftingRecipeDefinition* RecipeDefinition) const;

	/** Returns the current replicated unlock tags for UI/debug display. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting|Unlocks")
	FGameplayTagContainer GetUnlockedRecipeTags() const { return UnlockedRecipeTags; }

private:
	UFUNCTION()
	void OnRep_UnlockedRecipeTags(FGameplayTagContainer PreviousTags);

	void BroadcastUnlockChanged(FGameplayTag RecipeUnlockTag, bool bNewlyUnlocked) const;

	/** True for a valid tag below the Recipe root, the only tags this store keeps and saves. */
	static bool IsConcreteRecipeTag(FGameplayTag RecipeUnlockTag);

private:
	/** Recipe unlocks of this world. Mutated by the server and saved with the world; clients only read them. */
	UPROPERTY(ReplicatedUsing = OnRep_UnlockedRecipeTags, BlueprintReadOnly, Category = "Crafting|Unlocks", meta = (AllowPrivateAccess = "true"))
	FGameplayTagContainer UnlockedRecipeTags;
};
