#include "RpgCharacterPartsAutomationTestTypes.h"

#include "RpgPawnComponent_CharacterParts.h"
#include "Components/SkeletalMeshComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgCharacterPartsAutomationTestTypes)

ARpgCharacterPartsTestCharacter::ARpgCharacterPartsTestCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CharacterParts = CreateDefaultSubobject<URpgPawnComponent_CharacterParts>(TEXT("CharacterParts"));
	EquipmentManager = CreateDefaultSubobject<URpgEquipmentManagerComponent>(TEXT("EquipmentManager"));
}

ARpgCharacterPartsTestBodyPart::ARpgCharacterPartsTestBodyPart(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	BodyMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	BodyMesh->SetupAttachment(RootComponent);
}

void ARpgCharacterPartsTestBodyPart::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	Super::GetOwnedGameplayTags(TagContainer);
	TagContainer.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Cosmetic.Body")));
}

void ARpgCharacterPartsTestArmorPart::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	Super::GetOwnedGameplayTags(TagContainer);
	TagContainer.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Cosmetic.Cover.UpperBody")));
}

ARpgCharacterPartsTestReplicatedPart::ARpgCharacterPartsTestReplicatedPart(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
}

URpgCharacterPartsTestChestDefinition::URpgCharacterPartsTestChestDefinition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AllowedSlots = { ERpgEquipmentSlot::Chest };
	FRpgCharacterPart& Part = CharacterParts.AddDefaulted_GetRef();
	Part.PartClass = ARpgCharacterPartsTestArmorPart::StaticClass();
}
