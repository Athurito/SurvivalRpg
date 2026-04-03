#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SurvivalRpg/Items/RpgItemSourceHandle.h"
#include "RpgItemPickup.generated.h"

class URpgEquipmentComponent;
class URpgItemDefinition;
class URpgItemInstance;
class USceneComponent;
class FOutBunch;
struct FReplicationFlags;
class UActorChannel;

UCLASS(BlueprintType)
class SURVIVALRPG_API ARpgItemPickup : public AActor
{
	GENERATED_BODY()

public:
	ARpgItemPickup();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) override;

	UFUNCTION(BlueprintCallable, Category = "Loot")
	void InitializeFromDefinition(URpgItemDefinition* ItemDefinition, const FRpgItemSourceHandle& SourceHandle);

	UFUNCTION(BlueprintCallable, Category = "Loot")
	void SetItemInstance(URpgItemInstance* NewItemInstance);

	UFUNCTION(BlueprintCallable, Category = "Loot")
	URpgItemInstance* ClaimPickupToEquipment(URpgEquipmentComponent* EquipmentComponent, bool bAutoEquip = false);

	URpgItemInstance* GetItemInstance() const { return ItemInstance; }

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(Replicated)
	TObjectPtr<URpgItemInstance> ItemInstance;
};
