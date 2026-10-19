#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "RpgTaggedActor.h"
#include "RpgCharacterPartsAutomationTestTypes.generated.h"

class URpgEquipmentManagerComponent;
class URpgPawnComponent_CharacterParts;
class USkeletalMeshComponent;

/** Character fixture with a character parts component and an equipment manager, used by Character Parts automation tests. */
UCLASS(NotBlueprintable, Transient)
class ARpgCharacterPartsTestCharacter final : public ACharacter
{
	GENERATED_BODY()

public:
	explicit ARpgCharacterPartsTestCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	URpgPawnComponent_CharacterParts* GetCharacterParts() const { return CharacterParts; }
	URpgEquipmentManagerComponent* GetEquipmentManager() const { return EquipmentManager; }

private:
	UPROPERTY()
	TObjectPtr<URpgPawnComponent_CharacterParts> CharacterParts;

	UPROPERTY()
	TObjectPtr<URpgEquipmentManagerComponent> EquipmentManager;
};

/**
 * Body part fixture owning Cosmetic.Body. Tags are requested at call time because ini tags load after CDOs.
 * Like the MetaHuman body part, its mesh sits below a scene root instead of being the root.
 */
UCLASS(NotBlueprintable, Transient)
class ARpgCharacterPartsTestBodyPart final : public ARpgTaggedActor
{
	GENERATED_BODY()

public:
	explicit ARpgCharacterPartsTestBodyPart(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;

	USkeletalMeshComponent* GetBodyMesh() const { return BodyMesh; }

private:
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> BodyMesh;
};

/** Armor part fixture owning Cosmetic.Cover.UpperBody. */
UCLASS(NotBlueprintable, Transient)
class ARpgCharacterPartsTestArmorPart final : public ARpgTaggedActor
{
	GENERATED_BODY()

public:
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
};

/** Part fixture that wrongly replicates; equipment data validation must reject it. */
UCLASS(NotBlueprintable, Transient)
class ARpgCharacterPartsTestReplicatedPart final : public ARpgTaggedActor
{
	GENERATED_BODY()

public:
	explicit ARpgCharacterPartsTestReplicatedPart(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** Chest equipment fixture that wears the armor part fixture. */
UCLASS(NotBlueprintable, Transient)
class URpgCharacterPartsTestChestDefinition final : public URpgEquipmentDefinition
{
	GENERATED_BODY()

public:
	explicit URpgCharacterPartsTestChestDefinition(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
