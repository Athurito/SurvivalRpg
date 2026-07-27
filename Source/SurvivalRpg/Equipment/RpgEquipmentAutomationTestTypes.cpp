#include "RpgEquipmentAutomationTestTypes.h"

#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgEquipmentAutomationTestTypes)

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

ARpgEquipmentAutomationTestPawn::ARpgEquipmentAutomationTestPawn(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilitySystemComponent = CreateDefaultSubobject<URpgAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	HealthSet = CreateDefaultSubobject<URpgHealthSet>(TEXT("HealthSet"));
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
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}
