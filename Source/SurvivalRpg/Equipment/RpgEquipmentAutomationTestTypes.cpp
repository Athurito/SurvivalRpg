#include "RpgEquipmentAutomationTestTypes.h"

#include "SurvivalRpg/AbilitySystem/Attributes/RpgCombatSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgPrimarySet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgStaminaSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Inventory/Itemization/RpgInventoryFragment_Itemization.h"
#include "SurvivalRpg/Inventory/Itemization/RpgItemizationGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgEquipmentAutomationTestTypes)

URpgEquipmentAutomationTestItemizationProfile::URpgEquipmentAutomationTestItemizationProfile(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	for (FRpgItemRarityWeight& Row : RarityWeights)
	{
		Row.Weight = Row.Rarity == ERpgItemRarity::Common ? 1.0f : 0.0f;
	}

	FRpgItemStatRollDefinition& WeaponDamage = BaseStats.AddDefaulted_GetRef();
	WeaponDamage.StatTag = RpgItemizationGameplayTags::Item_Stat_WeaponDamage;
	WeaponDamage.MinimumValue = FScalableFloat(41.0f);
	WeaponDamage.MaximumValue = FScalableFloat(41.0f);

	FRpgItemStatRollDefinition& MaxStamina = BaseStats.AddDefaulted_GetRef();
	MaxStamina.StatTag = RpgItemizationGameplayTags::Item_Stat_MaxStamina;
	MaxStamina.MinimumValue = FScalableFloat(25.0f);
	MaxStamina.MaximumValue = FScalableFloat(75.0f);
}

URpgEquipmentAutomationTestItemizedWeaponDefinition::URpgEquipmentAutomationTestItemizedWeaponDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Automation Itemized Weapon"));

	URpgInventoryFragment_SpatialItem* Spatial =
		CreateDefaultSubobject<URpgInventoryFragment_SpatialItem>(TEXT("Spatial"));
	Spatial->Footprint.Width = 1;
	Spatial->Footprint.Height = 1;
	Fragments.Add(Spatial);

	URpgInventoryFragment_ItemTraits* Traits =
		CreateDefaultSubobject<URpgInventoryFragment_ItemTraits>(TEXT("Traits"));
	Traits->ItemCategory = ERpgInventoryItemCategory::Weapon;
	Traits->bCanStack = false;
	Traits->MaxStackSize = 1;
	Fragments.Add(Traits);

	URpgInventoryFragment_Itemization* Itemization =
		CreateDefaultSubobject<URpgInventoryFragment_Itemization>(TEXT("Itemization"));
	Itemization->ItemizationProfile =
		GetMutableDefault<URpgEquipmentAutomationTestItemizationProfile>();
	Fragments.Add(Itemization);
}

URpgEquipmentAutomationTestMaxHealthEffect::URpgEquipmentAutomationTestMaxHealthEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	FGameplayModifierInfo& MaxHealthModifier = Modifiers.AddDefaulted_GetRef();
	MaxHealthModifier.Attribute = URpgHealthSet::GetMaxHealthAttribute();
	MaxHealthModifier.ModifierOp = EGameplayModOp::Additive;
	MaxHealthModifier.ModifierMagnitude = FScalableFloat(500.0f);
}

URpgEquipmentAutomationTestHealthAbilitySet::URpgEquipmentAutomationTestHealthAbilitySet()
{
	FRpgAbilitySet_GameplayEffect& MaxHealthGrant = GrantedGameplayEffects.AddDefaulted_GetRef();
	MaxHealthGrant.GameplayEffect = URpgEquipmentAutomationTestMaxHealthEffect::StaticClass();
	MaxHealthGrant.EffectLevel = 1.0f;
}

URpgEquipmentAutomationTestHelmetDefinition::URpgEquipmentAutomationTestHelmetDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AllowedSlots = { ERpgEquipmentSlot::Head };
	AbilitySetsToGrant.Add(
		CreateDefaultSubobject<URpgEquipmentAutomationTestHealthAbilitySet>(TEXT("PersistentHealthAbilitySet")));
}

URpgEquipmentAutomationTestSwordDefinition::URpgEquipmentAutomationTestSwordDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AllowedSlots = { ERpgEquipmentSlot::MainHand };
}

URpgEquipmentAutomationTestOffHandDefinition::URpgEquipmentAutomationTestOffHandDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AllowedSlots = { ERpgEquipmentSlot::OffHand };
}

ARpgEquipmentAutomationTestPawn::ARpgEquipmentAutomationTestPawn(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilitySystemComponent = CreateDefaultSubobject<URpgAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	HealthSet = CreateDefaultSubobject<URpgHealthSet>(TEXT("HealthSet"));
	PrimarySet = CreateDefaultSubobject<URpgPrimarySet>(TEXT("PrimarySet"));
	CombatSet = CreateDefaultSubobject<URpgCombatSet>(TEXT("CombatSet"));
	StaminaSet = CreateDefaultSubobject<URpgStaminaSet>(TEXT("StaminaSet"));
	EquipmentManagerComponent = CreateDefaultSubobject<URpgEquipmentManagerComponent>(TEXT("EquipmentManagerComponent"));
}

UAbilitySystemComponent* ARpgEquipmentAutomationTestPawn::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ARpgEquipmentAutomationTestPawn::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AddAttributeSetSubobject(HealthSet.Get());
		AbilitySystemComponent->AddAttributeSetSubobject(PrimarySet.Get());
		AbilitySystemComponent->AddAttributeSetSubobject(CombatSet.Get());
		AbilitySystemComponent->AddAttributeSetSubobject(StaminaSet.Get());
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}
