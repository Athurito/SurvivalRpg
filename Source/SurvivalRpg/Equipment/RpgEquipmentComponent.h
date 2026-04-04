#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/Items/RpgItemGrantTypes.h"
#include "SurvivalRpg/Items/RpgItemSourceHandle.h"
#include "RpgEquipmentComponent.generated.h"

class AActor;
class APawn;
class FOutBunch;
struct FReplicationFlags;
class UActorChannel;
class UDataAsset;
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
enum class ERpgEquipmentPresentationNotifyAction : uint8
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
};

USTRUCT()
struct FRpgEquipmentVisualEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<URpgItemInstance> ItemInstance = nullptr;

	UPROPERTY()
	TObjectPtr<AActor> VisualActor = nullptr;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRpgEquipmentChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgActiveCameraSettingsChangedSignature, UDataAsset*, CameraSettings);

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
	bool TryActivateWeaponSet(int32 WeaponSetIndex);

	UFUNCTION(BlueprintPure, Category = "Equipment")
	FRpgEquippedWeaponSet GetActiveWeaponSet() const;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	int32 GetActiveWeaponSetIndex() const { return ActiveWeaponSetIndex; }

	UFUNCTION(BlueprintPure, Category = "Equipment|Presentation")
	UDataAsset* GetActiveCameraSettings() const { return ActiveCameraSettings; }

	UFUNCTION(BlueprintCallable, Category = "Equipment|Presentation")
	void ApplyPresentationNotifyAction(ERpgEquipmentPresentationNotifyAction Action);

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void GetEquippedItems(TArray<URpgItemInstance*>& OutItems) const;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	URpgItemInstance* GetItemInSlot(FGameplayTag SlotTag) const;

	UPROPERTY(BlueprintAssignable, Category = "Equipment")
	FRpgEquipmentChangedSignature OnEquipmentChanged;

	UPROPERTY(BlueprintAssignable, Category = "Equipment|Presentation")
	FRpgActiveCameraSettingsChangedSignature OnActiveCameraSettingsChanged;

#if WITH_DEV_AUTOMATION_TESTS
	void SetAbilitySystemOverrideForTests(URpgAbilitySystemComponent* InAbilitySystemComponent) { AbilitySystemOverrideForTests = InAbilitySystemComponent; }
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION()
	void OnRep_WeaponSets();

	UFUNCTION()
	void OnRep_ActiveWeaponSetIndex();

	UFUNCTION(Server, Reliable)
	void ServerTryEquipItem(URpgItemInstance* ItemInstance, FGameplayTag SlotTag);

	UFUNCTION(Server, Reliable)
	void ServerTryUnequipItem(FGameplayTag SlotTag);

	UFUNCTION(Server, Reliable)
	void ServerTryActivateWeaponSet(int32 WeaponSetIndex);

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
	bool IsItemInPresentationVisibleWeaponSet(const URpgItemInstance* ItemInstance) const;
	int32 CountTwoHandedItems(const TArray<FRpgEquippedWeaponSet>& WeaponSetStates) const;
	URpgItemInstance* FindKnownItemById(const FGuid& InstanceId) const;
	void HandleEquipmentStateChanged();
	void RefreshPresentationState(bool bAllowMontage);
	void SetPresentationVisibleWeaponSetIndex(int32 InPresentationVisibleWeaponSetIndex);
	void RemoveAppliedGrants();
	void ApplyCurrentGrants();
	void ApplyAbilitySets(URpgAbilitySystemComponent* AbilitySystemComponent, const TArray<TObjectPtr<const URpgAbilitySet>>& AbilitySets, UObject* SourceObject);
	void ApplyGameplayEffects(URpgAbilitySystemComponent* AbilitySystemComponent, const TArray<FRpgItemGameplayEffectGrant>& GameplayEffects);
	void ApplyLooseTags(URpgAbilitySystemComponent* AbilitySystemComponent, const FGameplayTagContainer& LooseTags);
	URpgAbilitySystemComponent* ResolveAbilitySystemComponent() const;
	void QueueVisualRefresh();
	void RefreshVisuals();
	void RefreshActiveCameraSettings();
	APawn* ResolveVisualPawn() const;
	URpgItemInstance* GetPrimaryPresentationItemForWeaponSet(int32 WeaponSetIndex) const;
	const class URpgItemFragment_Visual* GetPrimaryPresentationVisualFragmentForWeaponSet(int32 WeaponSetIndex) const;
	bool MontageUsesPresentationNotify(int32 WeaponSetIndex, bool bUseEquipMontage) const;
	bool PlayPresentationMontageForWeaponSet(int32 WeaponSetIndex, bool bUseEquipMontage) const;
	AActor* FindVisualActorForItem(const URpgItemInstance* ItemInstance) const;
	AActor* FindOrSpawnVisualActor(URpgItemInstance* ItemInstance, APawn* VisualPawn);
	void DestroyVisualActorForItem(const URpgItemInstance* ItemInstance);
	void DestroyAllVisualActors();
	void ForceOwnerNetUpdate() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<const URpgEquipmentRuleset> EquipmentRuleset = nullptr;

	UPROPERTY(Replicated)
	TArray<TObjectPtr<URpgItemInstance>> KnownItemInstances;

	UPROPERTY(ReplicatedUsing = OnRep_WeaponSets)
	TArray<FRpgEquippedWeaponSet> WeaponSets;

	UPROPERTY(ReplicatedUsing = OnRep_ActiveWeaponSetIndex)
	int32 ActiveWeaponSetIndex = INDEX_NONE;

	UPROPERTY(Transient)
	TArray<FRpgEquipmentVisualEntry> VisualEntries;

	UPROPERTY(Transient)
	TObjectPtr<APawn> CachedVisualPawn = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDataAsset> ActiveCameraSettings = nullptr;

	UPROPERTY(Transient)
	int32 PresentationVisibleWeaponSetIndex = INDEX_NONE;

	TArray<FRpgAbilitySet_GrantedHandles> AppliedAbilitySetHandles;
	TArray<FActiveGameplayEffectHandle> AppliedGameplayEffectHandles;
	TMap<FGameplayTag, int32> AppliedLooseTagCounts;
	int32 ObservedActiveWeaponSetIndex = INDEX_NONE;
	bool bVisualRefreshQueued = true;

#if WITH_DEV_AUTOMATION_TESTS
	URpgAbilitySystemComponent* AbilitySystemOverrideForTests = nullptr;
#endif
};
