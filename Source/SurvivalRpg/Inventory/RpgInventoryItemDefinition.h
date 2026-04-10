// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "RpgInventoryItemDefinition.generated.h"

template <typename T> class TSubclassOf;

class URpgInventoryItemInstance;
struct FFrame;

//////////////////////////////////////////////////////////////////////

// Represents a fragment of an item definition
UCLASS(MinimalAPI, DefaultToInstanced, EditInlineNew, Abstract)
class URpgInventoryItemFragment : public UObject
{
	GENERATED_BODY()

public:
	virtual void OnInstanceCreated(URpgInventoryItemInstance* Instance) const {}
};

//////////////////////////////////////////////////////////////////////

/**
 * URpgInventoryItemDefinition
 */
UCLASS(Blueprintable, Const, Abstract)
class URpgInventoryItemDefinition : public UObject
{
	GENERATED_BODY()

public:
	explicit URpgInventoryItemDefinition(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Display)
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Display, Instanced)
	TArray<TObjectPtr<URpgInventoryItemFragment>> Fragments;

public:
	const URpgInventoryItemFragment* FindFragmentByClass(TSubclassOf<URpgInventoryItemFragment> FragmentClass) const;
};

//@TODO: Make into a subsystem instead?
UCLASS()
class URpgInventoryFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, meta=(DeterminesOutputType=FragmentClass))
	static const URpgInventoryItemFragment* FindItemDefinitionFragment(TSubclassOf<URpgInventoryItemDefinition> ItemDef, TSubclassOf<URpgInventoryItemFragment> FragmentClass);
};
