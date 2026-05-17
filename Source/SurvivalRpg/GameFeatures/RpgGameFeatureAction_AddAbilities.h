// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "RpgGameFeatureAction_WorldActionBase.h"
#include "Abilities/GameplayAbility.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "RpgGameFeatureAction_AddAbilities.generated.h"

class UAttributeSet;
class UDataTable;
class URpgAbilitySet;
class URpgGameplayAbility;
struct FComponentRequestHandle;

USTRUCT(BlueprintType)
struct FRpgGameFeatureAbilityGrant
{
	GENERATED_BODY()

	/** Gameplay ability type to grant while this GameFeature is active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AssetBundles = "Client,Server"))
	TSoftClassPtr<URpgGameplayAbility> AbilityType;
};

USTRUCT(BlueprintType)
struct FRpgGameFeatureAttributeSetGrant
{
	GENERATED_BODY()

	/** Attribute set type to add while this GameFeature is active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AssetBundles = "Client,Server"))
	TSoftClassPtr<UAttributeSet> AttributeSetType;

	/** Optional table used to initialize attributes on the granted set. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AssetBundles = "Client,Server"))
	TSoftObjectPtr<UDataTable> InitializationData;
};

USTRUCT()
struct FRpgGameFeatureAbilitiesEntry
{
	GENERATED_BODY()

	/** Actor class this grant entry applies to. */
	UPROPERTY(EditAnywhere, Category = "Abilities")
	TSoftClassPtr<AActor> ActorClass;

	/** Individual abilities to grant to matching actors. */
	UPROPERTY(EditAnywhere, Category = "Abilities")
	TArray<FRpgGameFeatureAbilityGrant> GrantedAbilities;

	/** Attribute sets to grant to matching actors. */
	UPROPERTY(EditAnywhere, Category = "Attributes")
	TArray<FRpgGameFeatureAttributeSetGrant> GrantedAttributes;

	/** Ability sets to grant to matching actors. Prefer this path for startup grants. */
	UPROPERTY(EditAnywhere, Category = "Ability Sets", meta = (AssetBundles = "Client,Server"))
	TArray<TSoftObjectPtr<const URpgAbilitySet>> GrantedAbilitySets;
};

/**
 * GameFeatureAction that grants abilities, attributes, and ability sets to matching actors.
 *
 * Grants are tied to the GameFeature activation context and are removed when the feature deactivates.
 */
UCLASS(meta = (DisplayName = "Add Rpg Abilities"))
class SURVIVALRPG_API URpgGameFeatureAction_AddAbilities final : public URpgGameFeatureAction_WorldActionBase
{
	GENERATED_BODY()

public:
	//~ UGameFeatureAction interface
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
	//~ End UGameFeatureAction interface

	//~ UObject interface
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
	//~ End UObject interface

	/** Grant entries keyed by target actor class. */
	UPROPERTY(EditAnywhere, Category = "Abilities", meta = (TitleProperty = "ActorClass", ShowOnlyInnerProperties))
	TArray<FRpgGameFeatureAbilitiesEntry> AbilitiesList;

private:
	struct FActorExtensions
	{
		TArray<FGameplayAbilitySpecHandle> Abilities;
		TArray<UAttributeSet*> Attributes;
		TArray<FRpgAbilitySet_GrantedHandles> AbilitySetHandles;
	};

	struct FPerContextData
	{
		TMap<AActor*, FActorExtensions> ActiveExtensions;
		TArray<TSharedPtr<FComponentRequestHandle>> ComponentRequests;
	};

	TMap<FGameFeatureStateChangeContext, FPerContextData> ContextData;

	//~ URpgGameFeatureAction_WorldActionBase interface
	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) override;
	//~ End URpgGameFeatureAction_WorldActionBase interface

	void Reset(FPerContextData& ActiveData);
	void HandleActorExtension(AActor* Actor, FName EventName, int32 EntryIndex, FGameFeatureStateChangeContext ChangeContext);
	void AddActorAbilities(AActor* Actor, const FRpgGameFeatureAbilitiesEntry& AbilitiesEntry, FPerContextData& ActiveData);
	void RemoveActorAbilities(AActor* Actor, FPerContextData& ActiveData);

	UActorComponent* FindOrAddComponentForActor(UClass* ComponentType, AActor* Actor, const FRpgGameFeatureAbilitiesEntry& AbilitiesEntry, FPerContextData& ActiveData);
};
