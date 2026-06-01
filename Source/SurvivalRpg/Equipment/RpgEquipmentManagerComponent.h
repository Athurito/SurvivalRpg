#pragma once

#include "CoreMinimal.h"
#include "Components/PawnComponent.h"
#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "RpgEquipmentDefinition.h"
#include "RpgEquipmentManagerComponent.generated.h"

class URpgAbilitySystemComponent;
class URpgEquipmentDefinition;
class URpgEquipmentInstance;
struct FNetDeltaSerializeInfo;

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgAppliedEquipmentEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FString GetDebugString() const;

private:
	friend struct FRpgEquipmentList;
	friend class URpgEquipmentManagerComponent;

	UPROPERTY()
	TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition;

	UPROPERTY()
	ERpgEquipmentSlot EquippedSlot = ERpgEquipmentSlot::None;

	UPROPERTY()
	TObjectPtr<URpgEquipmentInstance> Instance = nullptr;

	UPROPERTY(NotReplicated)
	FRpgAbilitySet_GrantedHandles GrantedHandles;
};

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgEquipmentList : public FFastArraySerializer
{
	GENERATED_BODY()

	FRpgEquipmentList() = default;
	explicit FRpgEquipmentList(UActorComponent* InOwnerComponent) : OwnerComponent(InOwnerComponent) {}

	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FRpgAppliedEquipmentEntry, FRpgEquipmentList>(Entries, DeltaParms, *this);
	}

	URpgEquipmentInstance* AddEntry(TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition, ERpgEquipmentSlot EquippedSlot);
	void RemoveEntry(URpgEquipmentInstance* Instance);

private:
	URpgAbilitySystemComponent* GetAbilitySystemComponent() const;

	friend class URpgEquipmentManagerComponent;

	UPROPERTY()
	TArray<FRpgAppliedEquipmentEntry> Entries;

	UPROPERTY(NotReplicated)
	TObjectPtr<UActorComponent> OwnerComponent = nullptr;
};

template<>
struct TStructOpsTypeTraits<FRpgEquipmentList> : public TStructOpsTypeTraitsBase2<FRpgEquipmentList>
{
	enum { WithNetDeltaSerializer = true };
};

UCLASS(BlueprintType, Const, meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgEquipmentManagerComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	URpgEquipmentManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Equipment")
	URpgEquipmentInstance* EquipItem(TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Equipment")
	URpgEquipmentInstance* EquipItemInSlot(TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition, ERpgEquipmentSlot Slot);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Equipment")
	void UnequipItem(URpgEquipmentInstance* ItemInstance);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Equipment")
	void UnequipItemInSlot(ERpgEquipmentSlot Slot);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
	URpgEquipmentInstance* GetFirstInstanceOfType(TSubclassOf<URpgEquipmentInstance> InstanceType) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
	TArray<URpgEquipmentInstance*> GetEquipmentInstancesOfType(TSubclassOf<URpgEquipmentInstance> InstanceType) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
	URpgEquipmentInstance* GetEquipmentInstanceInSlot(ERpgEquipmentSlot Slot) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
	bool IsEquipmentSlotBlocked(ERpgEquipmentSlot Slot) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
	bool IsEquipmentInstanceActiveForInputTag(const URpgEquipmentInstance* EquipmentInstance, FGameplayTag InputTag) const;

	template <typename T>
	T* GetFirstInstanceOfType() const
	{
		return Cast<T>(GetFirstInstanceOfType(T::StaticClass()));
	}

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
	virtual void InitializeComponent() override;
	virtual void UninitializeComponent() override;
	virtual void ReadyForReplication() override;

private:
	bool CanEquipItemInSlot(TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition, ERpgEquipmentSlot Slot) const;
	void UnequipConflictingItems(TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition, ERpgEquipmentSlot Slot);
	bool DoesEquipmentOccupySlot(const FRpgAppliedEquipmentEntry& Entry, ERpgEquipmentSlot Slot) const;
	bool CanEquipmentBlock(const URpgEquipmentInstance* EquipmentInstance) const;

	UPROPERTY(Replicated)
	FRpgEquipmentList EquipmentList;
};
