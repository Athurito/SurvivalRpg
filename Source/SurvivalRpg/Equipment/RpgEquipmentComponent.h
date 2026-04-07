#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/Items/RpgItemGrantTypes.h"
#include "SurvivalRpg/Items/RpgItemSourceHandle.h"
#include "RpgEquipmentComponent.generated.h"

class FOutBunch;
struct FReplicationFlags;
class UActorChannel;
class URpgAbilitySystemComponent;
class URpgEquipmentRuleset;
class URpgItemDefinition;
class URpgItemInstance;

UENUM()
enum class ERpgEquipmentHandSlot : uint8
{
	MainHand,
	OffHand
};

UENUM(BlueprintType)
enum class ERpgWeaponToolPresentationNotifyAction : uint8
{
	ApplyCurrentState,
	HolsterVisuals,
	DrawActiveSet
};

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgEquippedWeaponSet
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	TObjectPtr<URpgItemInstance> MainHandItem = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	TObjectPtr<URpgItemInstance> OffHandItem = nullptr;

	bool operator==(const FRpgEquippedWeaponSet& Other) const
	{
		return MainHandItem == Other.MainHandItem && OffHandItem == Other.OffHandItem;
	}

	bool operator!=(const FRpgEquippedWeaponSet& Other) const
	{
		return !(*this == Other);
	}
};

struct FRpgEquipmentStateChangedEvent
{
	int32 PreviousActiveWeaponSetIndex = INDEX_NONE;
	int32 NewActiveWeaponSetIndex = INDEX_NONE;
	bool bWeaponSlotsChanged = false;

	bool HasActiveWeaponSetChanged() const
	{
		return PreviousActiveWeaponSetIndex != NewActiveWeaponSetIndex;
	}
};

struct FRpgItemGameplayEffectGrantKey
{
	TSubclassOf<UGameplayEffect> GameplayEffect = nullptr;
	float EffectLevel = 1.0f;

	bool operator==(const FRpgItemGameplayEffectGrantKey& Other) const
	{
		return GameplayEffect == Other.GameplayEffect && FMath::IsNearlyEqual(EffectLevel, Other.EffectLevel);
	}
};

FORCEINLINE uint32 GetTypeHash(const FRpgItemGameplayEffectGrantKey& Key)
{
	return HashCombine(GetTypeHash(Key.GameplayEffect), GetTypeHash(Key.EffectLevel));
}

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRpgEquipmentChangedSignature);
DECLARE_MULTICAST_DELEGATE_OneParam(FRpgEquipmentStateChangedNativeSignature, const FRpgEquipmentStateChangedEvent&);

UCLASS(ClassGroup = (Custom), BlueprintType, meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URpgEquipmentComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) override;

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void SetEquipmentRuleset(const URpgEquipmentRuleset* InRuleset);

	UFUNCTION(BlueprintPure, Category = "Equipment")
	const URpgEquipmentRuleset* GetEquipmentRuleset() const { return EquipmentRuleset; }

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	URpgItemInstance* CreateItemInstance(URpgItemDefinition* ItemDefinition, const FRpgItemSourceHandle& SourceHandle);

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	URpgItemInstance* RegisterExistingItemInstance(URpgItemInstance* ItemInstance);

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool CanEquipItem(const URpgItemInstance* ItemInstance, FGameplayTag SlotTag) const;

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool TryEquipItem(URpgItemInstance* ItemInstance, FGameplayTag SlotTag);

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool TryAutoEquipItem(URpgItemInstance* ItemInstance);

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool TryUnequipItem(FGameplayTag SlotTag);

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool SetActiveWeaponSet(int32 WeaponSetIndex);

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool ClearActiveWeaponSet();

	UFUNCTION(BlueprintPure, Category = "Equipment")
	FRpgEquippedWeaponSet GetActiveWeaponSet() const;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	FRpgEquippedWeaponSet GetWeaponSet(int32 WeaponSetIndex) const;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	int32 GetActiveWeaponSetIndex() const { return ActiveWeaponSetIndex; }

	UFUNCTION(BlueprintPure, Category = "Equipment")
	int32 GetWeaponSetCount() const { return GetDesiredWeaponSetCount(); }

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void GetEquippedItems(TArray<URpgItemInstance*>& OutItems) const;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	URpgItemInstance* GetItemInSlot(FGameplayTag SlotTag) const;

	UPROPERTY(BlueprintAssignable, Category = "Equipment")
	FRpgEquipmentChangedSignature OnEquipmentChanged;

	FRpgEquipmentStateChangedNativeSignature& OnEquipmentStateChangedNative() { return EquipmentStateChangedNative; }
	const FRpgEquipmentStateChangedNativeSignature& OnEquipmentStateChangedNative() const { return EquipmentStateChangedNative; }

#if WITH_DEV_AUTOMATION_TESTS
	void SetAbilitySystemOverrideForTests(URpgAbilitySystemComponent* InAbilitySystemComponent) { AbilitySystemOverrideForTests = InAbilitySystemComponent; }
	int32 GetKnownItemInstanceCountForTests() const { return KnownItemInstances.Num(); }
#endif

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UFUNCTION()
	void OnRep_WeaponSets();

	UFUNCTION()
	void OnRep_ActiveWeaponSetIndex();

	UFUNCTION(Server, Reliable)
	void ServerTryEquipItem(URpgItemInstance* ItemInstance, FGameplayTag SlotTag);

	UFUNCTION(Server, Reliable)
	void ServerTryUnequipItem(FGameplayTag SlotTag);

	UFUNCTION(Server, Reliable)
	void ServerSetActiveWeaponSet(int32 WeaponSetIndex);

	UFUNCTION(Server, Reliable)
	void ServerClearActiveWeaponSet();

private:
	bool HasAuthorityForEquipment() const;
	int32 GetDesiredWeaponSetCount() const;
	void EnsureWeaponSetCount();
	void EnsureWeaponSetCount(TArray<FRpgEquippedWeaponSet>& InOutWeaponSets) const;
	bool ResolveSlotTag(const FGameplayTag& SlotTag, int32& OutWeaponSetIndex, ERpgEquipmentHandSlot& OutHandSlot) const;
	FGameplayTag MakeSlotTag(int32 WeaponSetIndex, ERpgEquipmentHandSlot HandSlot) const;
	bool BuildProposedEquipState(URpgItemInstance* ItemInstance, const FGameplayTag& SlotTag, TArray<FRpgEquippedWeaponSet>& InOutWeaponSets) const;
	bool ValidateWeaponSets(const TArray<FRpgEquippedWeaponSet>& WeaponSetStates) const;
	void StripItemFromWeaponSets(URpgItemInstance* ItemInstance, TArray<FRpgEquippedWeaponSet>& InOutWeaponSets) const;
	bool IsItemInActiveWeaponSet(const URpgItemInstance* ItemInstance) const;
	int32 CountTwoHandedItems(const TArray<FRpgEquippedWeaponSet>& WeaponSetStates) const;
	URpgItemInstance* FindKnownItemById(const FGuid& InstanceId) const;
	void HandleEquipmentStateChanged(const TArray<FRpgEquippedWeaponSet>& PreviousWeaponSets, int32 PreviousActiveWeaponSetIndex);
	void RemoveAllAppliedGrants();
	void ReconcileAppliedGrants();
	void CompactKnownItemInstances();
	URpgAbilitySystemComponent* ResolveAbilitySystemComponent() const;
	void ForceOwnerNetUpdate() const;
	void BroadcastStateChangedNative(const FRpgEquipmentStateChangedEvent& Event);
	bool AreWeaponSetsEqual(const TArray<FRpgEquippedWeaponSet>& Left, const TArray<FRpgEquippedWeaponSet>& Right) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<const URpgEquipmentRuleset> EquipmentRuleset = nullptr;

	UPROPERTY(Replicated)
	TArray<TObjectPtr<URpgItemInstance>> KnownItemInstances;

	UPROPERTY(ReplicatedUsing = OnRep_WeaponSets)
	TArray<FRpgEquippedWeaponSet> WeaponSets;

	UPROPERTY(ReplicatedUsing = OnRep_ActiveWeaponSetIndex)
	int32 ActiveWeaponSetIndex = INDEX_NONE;

	TMap<const URpgAbilitySet*, FRpgAbilitySet_GrantedHandles> AppliedAbilitySetHandles;
	TMap<const URpgAbilitySet*, TWeakObjectPtr<UObject>> AppliedAbilitySetSourceObjects;
	TMap<FRpgItemGameplayEffectGrantKey, TArray<FActiveGameplayEffectHandle>> AppliedGameplayEffectHandles;
	TMap<FGameplayTag, int32> AppliedLooseTagCounts;
	FRpgEquipmentStateChangedNativeSignature EquipmentStateChangedNative;
	int32 LastNotifiedActiveWeaponSetIndex = INDEX_NONE;

#if WITH_DEV_AUTOMATION_TESTS
	URpgAbilitySystemComponent* AbilitySystemOverrideForTests = nullptr;
#endif
};
