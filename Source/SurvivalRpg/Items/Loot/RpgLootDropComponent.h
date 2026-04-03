#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RpgLootDropComponent.generated.h"

class ARpgItemPickup;
class URpgHealthComponent;
class URpgLootTable;

UCLASS(ClassGroup = (Custom), BlueprintType, meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgLootDropComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URpgLootDropComponent();

	UFUNCTION(BlueprintCallable, Category = "Loot")
	TArray<ARpgItemPickup*> DropLootAtLocation(const FVector& DropLocation, const FRotator& DropRotation = FRotator::ZeroRotator);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleOwnerDeath(AActor* OwningActor);

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgLootTable> LootTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<ARpgItemPickup> DefaultPickupActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (AllowPrivateAccess = "true"))
	bool bDropOnOwnerDeath = true;
};
