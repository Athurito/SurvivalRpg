#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "RpgEnemyCombatLoadout.generated.h"

class URpgEquipmentInstance;
class URpgEquipmentManagerComponent;
class URpgInventoryItemDefinition;
class URpgPawnExtensionComponent;

UCLASS(BlueprintType, Blueprintable, ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgEnemyCombatArchetypeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URpgEnemyCombatArchetypeComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintPure, Category = "Enemy Combat")
	static URpgEnemyCombatArchetypeComponent* FindEnemyCombatArchetypeComponent(const AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "Enemy Combat")
	FGameplayTag GetEnemyCombatArchetypeTag() const { return EnemyCombatArchetypeTag; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy Combat")
	void SetEnemyCombatArchetypeTag(FGameplayTag NewArchetypeTag);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(EditAnywhere, Replicated, Category = "Enemy Combat")
	FGameplayTag EnemyCombatArchetypeTag;
};

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgEnemyCombatLoadoutItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Combat")
	ERpgEquipmentSlot EquipmentSlot = ERpgEquipmentSlot::MainHand;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Combat", meta = (AssetBundles = "Server"))
	TSoftClassPtr<URpgInventoryItemDefinition> ItemDefinition;
};

UCLASS(BlueprintType, Const)
class SURVIVALRPG_API URpgEnemyCombatLoadoutDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Enemy Combat")
	FGameplayTag GetArchetypeTag() const { return ArchetypeTag; }

	const TArray<FRpgEnemyCombatLoadoutItem>& GetEquipmentItems() const { return EquipmentItems; }

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Combat", meta = (AllowPrivateAccess = "true", Categories = "Enemy.Archetype"))
	FGameplayTag ArchetypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Combat", meta = (AllowPrivateAccess = "true", TitleProperty = "EquipmentSlot"))
	TArray<FRpgEnemyCombatLoadoutItem> EquipmentItems;
};

UCLASS(BlueprintType, Blueprintable, ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgEnemyCombatLoadoutComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URpgEnemyCombatLoadoutComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy Combat")
	void ApplyCombatLoadout();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy Combat")
	void ClearAppliedCombatLoadout();

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BindToPawnExtension();
	void UnbindFromPawnExtension();
	void HandleAbilitySystemInitialized();
	void HandleAbilitySystemUninitialized();
	void ScheduleApplyRetry();

	FGameplayTag ResolveArchetypeTag() const;
	const URpgEnemyCombatLoadoutDefinition* ResolveLoadoutDefinition(FGameplayTag ArchetypeTag) const;
	TSubclassOf<URpgEquipmentDefinition> ResolveEquipmentDefinition(const FRpgEnemyCombatLoadoutItem& LoadoutItem) const;
	URpgEquipmentManagerComponent* FindEquipmentManager() const;

private:
	UPROPERTY(EditAnywhere, Category = "Enemy Combat", meta = (Categories = "Enemy.Archetype"))
	FGameplayTag DefaultArchetypeTag;

	UPROPERTY(EditAnywhere, Category = "Enemy Combat", meta = (AssetBundles = "Server"))
	TArray<TSoftObjectPtr<const URpgEnemyCombatLoadoutDefinition>> LoadoutDefinitions;

	UPROPERTY(EditAnywhere, Category = "Enemy Combat")
	bool bApplyOnAbilitySystemInitialized = true;

	UPROPERTY(Transient)
	TObjectPtr<URpgPawnExtensionComponent> BoundPawnExtension;

	UPROPERTY(Transient)
	FGameplayTag AppliedArchetypeTag;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<URpgEquipmentInstance>> AppliedEquipmentInstances;

	FTimerHandle ApplyRetryTimerHandle;
};
