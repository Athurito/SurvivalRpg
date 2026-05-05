// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SurvivalRpg/Systems/GameplayTagStack.h"
#include "Templates/SubclassOf.h"

#include "RpgInventoryItemInstance.generated.h"

class FLifetimeProperty;

class URpgInventoryItemDefinition;
class URpgInventoryItemFragment;
struct FFrame;
struct FGameplayTag;

/**
 * URpgInventoryItemInstance
 */
UCLASS(BlueprintType)
class URpgInventoryItemInstance : public UObject
{
	GENERATED_BODY()

public:
	explicit URpgInventoryItemInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~UObject interface
	virtual bool IsSupportedForNetworking() const override { return true; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End of UObject interface

	// Adds a specified number of stacks to the tag (does nothing if StackCount is below 1)
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	void AddStatTagStack(FGameplayTag Tag, int32 StackCount);

	// Removes a specified number of stacks from the tag (does nothing if StackCount is below 1)
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category= Inventory)
	void RemoveStatTagStack(FGameplayTag Tag, int32 StackCount);

	// Returns the stack count of the specified tag (or 0 if the tag is not present)
	UFUNCTION(BlueprintCallable, Category=Inventory)
	int32 GetStatTagStackCount(FGameplayTag Tag) const;

	// Returns true if there is at least one stack of the specified tag
	UFUNCTION(BlueprintCallable, Category=Inventory)
	bool HasStatTag(FGameplayTag Tag) const;

	TSubclassOf<URpgInventoryItemDefinition> GetItemDef() const
	{
		return ItemDef;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure=false, meta=(DeterminesOutputType=FragmentClass))
	const URpgInventoryItemFragment* FindFragmentByClass(TSubclassOf<URpgInventoryItemFragment> FragmentClass) const;

	template <typename ResultClass>
	const ResultClass* FindFragmentByClass() const
	{
		return static_cast<const ResultClass*>(FindFragmentByClass(ResultClass::StaticClass()));
	}

	/** Register all replication fragments */
	virtual void RegisterReplicationFragments(UE::Net::FFragmentRegistrationContext& Context, UE::Net::EFragmentRegistrationFlags RegistrationFlags) override;
private:

	void SetItemDef(TSubclassOf<URpgInventoryItemDefinition> InDef);

	friend struct FRpgInventoryList;

private:
	UPROPERTY(Replicated)
	FGameplayTagStackContainer StatTags;

	// The item definition
	UPROPERTY(Replicated)
	TSubclassOf<URpgInventoryItemDefinition> ItemDef;
};
