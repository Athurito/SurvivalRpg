// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Templates/SubclassOf.h"
#include "UObject/Interface.h"

#include "UObject/ObjectPtr.h"
#include "IPickupable.generated.h"

class URpgInventoryManagerComponent;
template <typename InterfaceType> class TScriptInterface;

class AActor;
class URpgInventoryItemDefinition;
class URpgInventoryItemInstance;
class UObject;
struct FFrame;

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FPickupTemplate
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	int32 StackCount = 1;

	UPROPERTY(EditAnywhere)
	TSubclassOf<URpgInventoryItemDefinition> ItemDef;
};

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FPickupInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<URpgInventoryItemInstance> Item = nullptr;
};

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FInventoryPickup
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FPickupInstance> Instances;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FPickupTemplate> Templates;
};

/**  */
UINTERFACE(MinimalAPI, BlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class UPickupable : public UInterface
{
	GENERATED_BODY()
};

/**  */
class IPickupable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	virtual FInventoryPickup GetPickupInventory() const = 0;
};

/**  */
UCLASS()
class UPickupableStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UPickupableStatics();

public:
	UFUNCTION(BlueprintPure)
	static TScriptInterface<IPickupable> GetFirstPickupableFromActor(AActor* Actor);

	/**
	 * Attempts to add every detached pickup entry as one server-authoritative operation.
	 * Canonical dropped-inventory actors fail closed here and must use their source-owned transfer path.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, meta = (WorldContext = "Ability"))
	static bool AddPickupToInventory(URpgInventoryManagerComponent* InventoryComponent, TScriptInterface<IPickupable> Pickup);
};
