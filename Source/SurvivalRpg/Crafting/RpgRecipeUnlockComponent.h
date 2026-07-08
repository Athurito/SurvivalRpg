#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"

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

/**
 * Replicated global recipe unlock store.
 *
 * This component is intended to live on the GameState so a recipe unlock becomes available to every
 * player in the current session. Disk persistence is deliberately left to a later SaveGame layer.
 */
UCLASS(BlueprintType, ClassGroup = (Crafting), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgRecipeUnlockComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	explicit URpgRecipeUnlockComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Adds one global recipe unlock tag. Server-authoritative; returns false if the tag is invalid or already unlocked. */
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

private:
	/** Session-replicated global recipe unlocks. Mutated by the server; clients treat it as read-only UI state. */
	UPROPERTY(ReplicatedUsing = OnRep_UnlockedRecipeTags, BlueprintReadOnly, Category = "Crafting|Unlocks", meta = (AllowPrivateAccess = "true"))
	FGameplayTagContainer UnlockedRecipeTags;
};
