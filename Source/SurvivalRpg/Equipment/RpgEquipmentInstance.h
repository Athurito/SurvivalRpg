#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RpgEquipmentDefinition.h"
#include "RpgEquipmentInstance.generated.h"

class AActor;
class APawn;
struct FRpgEquipmentActorToSpawn;

UCLASS(BlueprintType, Blueprintable)
class SURVIVALRPG_API URpgEquipmentInstance : public UObject
{
	GENERATED_BODY()

public:
	URpgEquipmentInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsSupportedForNetworking() const override { return true; }
	virtual UWorld* GetWorld() const override final;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	UObject* GetInstigator() const { return Instigator; }

	void SetInstigator(UObject* InInstigator) { Instigator = InInstigator; }

	UFUNCTION(BlueprintPure, Category = "Equipment")
	APawn* GetPawn() const;

	UFUNCTION(BlueprintPure, Category = "Equipment", meta = (DeterminesOutputType = "PawnType"))
	APawn* GetTypedPawn(TSubclassOf<APawn> PawnType) const;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	TArray<AActor*> GetSpawnedActors() const { return SpawnedActors; }

	UFUNCTION(BlueprintPure, Category = "Equipment")
	ERpgEquipmentSlot GetEquippedSlot() const { return EquippedSlot; }

	void SetEquippedSlot(ERpgEquipmentSlot InEquippedSlot) { EquippedSlot = InEquippedSlot; }

	virtual void SpawnEquipmentActors(const TArray<FRpgEquipmentActorToSpawn>& ActorsToSpawn);
	virtual void DestroyEquipmentActors();

	virtual void OnEquipped();
	virtual void OnUnequipped();

protected:
	virtual void RegisterReplicationFragments(UE::Net::FFragmentRegistrationContext& Context, UE::Net::EFragmentRegistrationFlags RegistrationFlags) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Equipment", meta = (DisplayName = "OnEquipped"))
	void K2_OnEquipped();

	UFUNCTION(BlueprintImplementableEvent, Category = "Equipment", meta = (DisplayName = "OnUnequipped"))
	void K2_OnUnequipped();

private:
	UFUNCTION()
	void OnRep_Instigator();

	UPROPERTY(ReplicatedUsing = OnRep_Instigator)
	TObjectPtr<UObject> Instigator = nullptr;

	UPROPERTY(Replicated)
	ERpgEquipmentSlot EquippedSlot = ERpgEquipmentSlot::None;

	UPROPERTY(Replicated)
	TArray<TObjectPtr<AActor>> SpawnedActors;
};
