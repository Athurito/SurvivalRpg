#include "RpgCharacterStatsViewModel.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgDefenseSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgManaSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgStaminaSet.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Equipment/RpgEquipmentInstance.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Progression/Player/Data/RpgPlayerProgressionData.h"
#include "SurvivalRpg/Progression/Player/RpgPlayerProgressionComponent.h"
#include "SurvivalRpg/UI/RpgUISettings.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgCharacterStatsViewModel)

namespace
{
	constexpr ETextIdenticalModeFlags CharacterStatsTextIdentityFlags =
		ETextIdenticalModeFlags::DeepCompare |
		ETextIdenticalModeFlags::LexicalCompareInvariants;

	float GetRatio(const float Value, const float Max)
	{
		return Max > 0.0f ? FMath::Clamp(Value / Max, 0.0f, 1.0f) : 0.0f;
	}

	FText MakeValueOfMaxText(const float Value, const float Max)
	{
		// Current values round up, so a living character never reads 0.
		return FText::Format(
			NSLOCTEXT("RpgCharacterStats", "ValueOfMax", "{0} / {1}"),
			FText::AsNumber(FMath::CeilToInt(Value - UE_KINDA_SMALL_NUMBER)),
			FText::AsNumber(FMath::RoundToInt(Max)));
	}

	FText MakeLoadText(const float Load, const float HeavyThreshold)
	{
		FNumberFormattingOptions LoadOptions;
		LoadOptions.MinimumFractionalDigits = 1;
		LoadOptions.MaximumFractionalDigits = 1;
		FNumberFormattingOptions ThresholdOptions;
		ThresholdOptions.MaximumFractionalDigits = 1;
		return FText::Format(
			NSLOCTEXT("RpgCharacterStats", "LoadOfHeavy", "{0} / {1} kg"),
			FText::AsNumber(Load, &LoadOptions),
			FText::AsNumber(HeavyThreshold, &ThresholdOptions));
	}

	FText MakeLoadTierText(const ERpgEquipmentLoadTier Tier)
	{
		switch (Tier)
		{
		case ERpgEquipmentLoadTier::Medium:
			return NSLOCTEXT("RpgCharacterStats", "LoadMedium", "Medium");
		case ERpgEquipmentLoadTier::Heavy:
			return NSLOCTEXT("RpgCharacterStats", "LoadHeavy", "Heavy");
		case ERpgEquipmentLoadTier::Light:
		default:
			return NSLOCTEXT("RpgCharacterStats", "LoadLight", "Light");
		}
	}

	const URpgEquipmentLoadoutComponent* FindEquipmentLoadout(const APlayerController* PlayerController)
	{
		const ARpgPlayerController* RpgPlayerController = Cast<ARpgPlayerController>(PlayerController);
		return RpgPlayerController ? RpgPlayerController->GetEquipmentLoadoutComponent() : nullptr;
	}

	float ResolveHeavyLoadThreshold(const URpgEquipmentLoadoutComponent* EquipmentLoadout)
	{
		return EquipmentLoadout
			? EquipmentLoadout->GetHeavyLoadThreshold()
			: GetDefault<URpgEquipmentLoadoutComponent>()->GetHeavyLoadThreshold();
	}
}

void URpgCharacterStatsViewModel::BeginDestroy()
{
	// Only release the observers here; no view should receive field changes from an object being destroyed.
	if (UWorld* World = GetObservedWorld())
	{
		World->GetTimerManager().ClearTimer(CombatTimerHandle);
	}
	ReleaseProgression();
	ReleaseAbilitySystem();
	if (LoadoutChangedHandle.IsValid())
	{
		LoadoutChangedHandle.Unregister();
	}
	Super::BeginDestroy();
}

void URpgCharacterStatsViewModel::BindPlayerController(APlayerController* InPlayerController)
{
	if (ObservedPlayerController.Get() != InPlayerController)
	{
		if (LoadoutChangedHandle.IsValid())
		{
			LoadoutChangedHandle.Unregister();
		}
		ObservedPlayerController = InPlayerController;
		if (UWorld* World = InPlayerController ? InPlayerController->GetWorld() : nullptr)
		{
			LoadoutChangedHandle = UGameplayMessageSubsystem::Get(World).RegisterListener<FRpgEquipmentLoadoutSlotsChangedMessage>(
				RpgGameplayTags::Rpg_EquipmentLoadout_Message_SlotsChanged,
				this,
				&ThisClass::HandleEquipmentLoadoutChanged);
		}
	}

	const URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout(InPlayerController);
	ApplyEquipmentLoad(
		EquipmentLoadout ? EquipmentLoadout->GetEquipmentLoadWeight() : 0.0f,
		ResolveHeavyLoadThreshold(EquipmentLoadout),
		EquipmentLoadout ? EquipmentLoadout->GetEquipmentLoadTier() : ERpgEquipmentLoadTier::Light);
	BindProgressionOfPlayerState(InPlayerController, ObservedAbilitySystem.Get());
}

void URpgCharacterStatsViewModel::BindAbilitySystem(UAbilitySystemComponent* InAbilitySystem)
{
	if (ObservedAbilitySystem.Get() != InAbilitySystem)
	{
		ReleaseAbilitySystem();
		ObservedAbilitySystem = InAbilitySystem;
		if (InAbilitySystem)
		{
			const FGameplayAttribute ObservedAttributes[] =
			{
				URpgHealthSet::GetHealthAttribute(),
				URpgHealthSet::GetMaxHealthAttribute(),
				URpgStaminaSet::GetStaminaAttribute(),
				URpgStaminaSet::GetMaxStaminaAttribute(),
				URpgManaSet::GetManaAttribute(),
				URpgManaSet::GetMaxManaAttribute(),
				URpgDefenseSet::GetArmorAttribute(),
			};
			for (const FGameplayAttribute& Attribute : ObservedAttributes)
			{
				AttributeHandles.Emplace(
					Attribute,
					InAbilitySystem->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this, &ThisClass::HandleAttributeChanged));
			}
			AbilityActivatedHandle = InAbilitySystem->AbilityActivatedCallbacks.AddUObject(this, &ThisClass::HandleAbilityActivated);
		}
	}

	RefreshAttributes();
	BindProgressionOfPlayerState(ObservedPlayerController.Get(), InAbilitySystem);
}

void URpgCharacterStatsViewModel::UnbindAbilitySystem()
{
	ReleaseAbilitySystem();
	ClearCombat();
	RefreshAttributes();
}

void URpgCharacterStatsViewModel::BindProgression(URpgPlayerProgressionComponent* InProgression)
{
	if (ObservedProgression.Get() != InProgression)
	{
		ReleaseProgression();
		ObservedProgression = InProgression;
		if (InProgression)
		{
			InProgression->OnLevelChanged.AddUniqueDynamic(this, &ThisClass::HandleCharacterLevelChanged);
			InProgression->OnXPChanged.AddUniqueDynamic(this, &ThisClass::HandleCharacterXPChanged);
		}
	}

	RefreshProgression();
}

void URpgCharacterStatsViewModel::Unbind()
{
	BindPlayerController(nullptr);
	BindProgression(nullptr);
	UnbindAbilitySystem();
}

void URpgCharacterStatsViewModel::ReleaseAbilitySystem()
{
	if (UAbilitySystemComponent* AbilitySystem = ObservedAbilitySystem.Get())
	{
		for (const TPair<FGameplayAttribute, FDelegateHandle>& Handle : AttributeHandles)
		{
			AbilitySystem->GetGameplayAttributeValueChangeDelegate(Handle.Key).Remove(Handle.Value);
		}
		AbilitySystem->AbilityActivatedCallbacks.Remove(AbilityActivatedHandle);
	}
	AttributeHandles.Reset();
	AbilityActivatedHandle.Reset();
	ObservedAbilitySystem.Reset();
}

void URpgCharacterStatsViewModel::ReleaseProgression()
{
	if (URpgPlayerProgressionComponent* Progression = ObservedProgression.Get())
	{
		Progression->OnLevelChanged.RemoveDynamic(this, &ThisClass::HandleCharacterLevelChanged);
		Progression->OnXPChanged.RemoveDynamic(this, &ThisClass::HandleCharacterXPChanged);
	}
	ObservedProgression.Reset();
}

void URpgCharacterStatsViewModel::HandleCharacterLevelChanged(int32 NewLevel)
{
	RefreshProgression();
}

void URpgCharacterStatsViewModel::HandleCharacterXPChanged(float CurrentXP, float XPToNextLevel)
{
	RefreshProgression();
}

void URpgCharacterStatsViewModel::HandleAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshAttributes();
	if (ChangeData.Attribute == URpgHealthSet::GetHealthAttribute() && ChangeData.NewValue < ChangeData.OldValue)
	{
		EnterCombat();
	}
}

void URpgCharacterStatsViewModel::HandleAbilityActivated(UGameplayAbility* Ability)
{
	// Equipment grants attacks, block and weapon or tool abilities. Movement, interaction and reactions do not count.
	UAbilitySystemComponent* AbilitySystem = ObservedAbilitySystem.Get();
	if (!Ability || !AbilitySystem || !Ability->IsInstantiated())
	{
		return;
	}
	const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromHandle(Ability->GetCurrentAbilitySpecHandle());
	if (Spec && Cast<URpgEquipmentInstance>(Spec->SourceObject.Get()))
	{
		EnterCombat();
	}
}

void URpgCharacterStatsViewModel::HandleEquipmentLoadoutChanged(
	FGameplayTag Channel,
	const FRpgEquipmentLoadoutSlotsChangedMessage& Message)
{
	const APlayerController* PlayerController = ObservedPlayerController.Get();
	if (!PlayerController || Message.Owner != PlayerController)
	{
		return;
	}

	ApplyEquipmentLoad(
		Message.EquipmentLoadWeight,
		ResolveHeavyLoadThreshold(FindEquipmentLoadout(PlayerController)),
		Message.EquipmentLoadTier);
}

void URpgCharacterStatsViewModel::RefreshAttributes()
{
	const UAbilitySystemComponent* AbilitySystem = ObservedAbilitySystem.Get();
	auto ReadAttribute = [AbilitySystem](const FGameplayAttribute& Attribute)
	{
		return AbilitySystem && AbilitySystem->HasAttributeSetForAttribute(Attribute)
			? AbilitySystem->GetNumericAttribute(Attribute)
			: 0.0f;
	};

	const float NewHealth = ReadAttribute(URpgHealthSet::GetHealthAttribute());
	const float NewMaxHealth = ReadAttribute(URpgHealthSet::GetMaxHealthAttribute());
	const float NewStamina = ReadAttribute(URpgStaminaSet::GetStaminaAttribute());
	const float NewMaxStamina = ReadAttribute(URpgStaminaSet::GetMaxStaminaAttribute());
	const bool bNewHasMana = AbilitySystem && AbilitySystem->HasAttributeSetForAttribute(URpgManaSet::GetManaAttribute());
	const float NewMana = ReadAttribute(URpgManaSet::GetManaAttribute());
	const float NewMaxMana = ReadAttribute(URpgManaSet::GetMaxManaAttribute());
	const float NewArmor = ReadAttribute(URpgDefenseSet::GetArmorAttribute());

	UE_MVVM_SET_PROPERTY_VALUE(Health, NewHealth);
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, NewMaxHealth);
	UE_MVVM_SET_PROPERTY_VALUE(HealthProgress, GetRatio(NewHealth, NewMaxHealth));
	SetText(HealthText, MakeValueOfMaxText(NewHealth, NewMaxHealth), FFieldNotificationClassDescriptor::HealthText);
	UE_MVVM_SET_PROPERTY_VALUE(Stamina, NewStamina);
	UE_MVVM_SET_PROPERTY_VALUE(MaxStamina, NewMaxStamina);
	UE_MVVM_SET_PROPERTY_VALUE(StaminaProgress, GetRatio(NewStamina, NewMaxStamina));
	SetText(StaminaText, MakeValueOfMaxText(NewStamina, NewMaxStamina), FFieldNotificationClassDescriptor::StaminaText);
	UE_MVVM_SET_PROPERTY_VALUE(bHasMana, bNewHasMana);
	UE_MVVM_SET_PROPERTY_VALUE(Mana, NewMana);
	UE_MVVM_SET_PROPERTY_VALUE(MaxMana, NewMaxMana);
	UE_MVVM_SET_PROPERTY_VALUE(ManaProgress, GetRatio(NewMana, NewMaxMana));
	SetText(ManaText, bNewHasMana ? MakeValueOfMaxText(NewMana, NewMaxMana) : FText::GetEmpty(), FFieldNotificationClassDescriptor::ManaText);
	UE_MVVM_SET_PROPERTY_VALUE(Armor, NewArmor);
	SetText(ArmorText, FText::AsNumber(FMath::RoundToInt(NewArmor)), FFieldNotificationClassDescriptor::ArmorText);

	const bool bNewVitalsFull = AbilitySystem && NewMaxHealth > 0.0f
		&& NewHealth >= NewMaxHealth && NewStamina >= NewMaxStamina && (!bNewHasMana || NewMana >= NewMaxMana);
	UE_MVVM_SET_PROPERTY_VALUE(bVitalsFull, bNewVitalsFull);
	RefreshHudContext();
}

void URpgCharacterStatsViewModel::RefreshProgression()
{
	const URpgPlayerProgressionComponent* Progression = ObservedProgression.Get();
	const int32 NewLevel = Progression ? FMath::Max(1, Progression->GetLevel()) : 1;
	const float NewXP = Progression ? Progression->GetXP() : 0.0f;
	const float NewXPToNextLevel = Progression ? Progression->GetXPToNextLevelForCurrentLevel() : 0.0f;
	const bool bMaxLevel = Progression && Progression->ConfigData && NewLevel >= Progression->ConfigData->MaxLevel;

	UE_MVVM_SET_PROPERTY_VALUE(CharacterLevel, NewLevel);
	SetText(CharacterLevelText, FText::AsNumber(NewLevel), FFieldNotificationClassDescriptor::CharacterLevelText);
	UE_MVVM_SET_PROPERTY_VALUE(CharacterXP, NewXP);
	UE_MVVM_SET_PROPERTY_VALUE(CharacterXPToNextLevel, NewXPToNextLevel);
	UE_MVVM_SET_PROPERTY_VALUE(CharacterLevelProgress, bMaxLevel ? 1.0f : GetRatio(NewXP, NewXPToNextLevel));
	FText NewXPText;
	if (bMaxLevel)
	{
		NewXPText = NSLOCTEXT("RpgCharacterStats", "MaxLevel", "Max level");
	}
	else if (NewXPToNextLevel > 0.0f)
	{
		NewXPText = FText::Format(
			NSLOCTEXT("RpgCharacterStats", "XPOfNextLevel", "{0} / {1}"),
			FText::AsNumber(FMath::FloorToInt(NewXP)),
			FText::AsNumber(FMath::RoundToInt(NewXPToNextLevel)));
	}
	else
	{
		// Without an XP curve there is no next level to measure against.
		NewXPText = FText::Format(NSLOCTEXT("RpgCharacterStats", "XPOnly", "{0} XP"), FText::AsNumber(FMath::FloorToInt(NewXP)));
	}
	SetText(CharacterXPText, NewXPText, FFieldNotificationClassDescriptor::CharacterXPText);
}

void URpgCharacterStatsViewModel::ApplyEquipmentLoad(
	const float NewLoad,
	const float NewHeavyThreshold,
	const ERpgEquipmentLoadTier NewTier)
{
	UE_MVVM_SET_PROPERTY_VALUE(EquipmentLoad, NewLoad);
	UE_MVVM_SET_PROPERTY_VALUE(HeavyLoadThreshold, NewHeavyThreshold);
	UE_MVVM_SET_PROPERTY_VALUE(EquipmentLoadProgress, GetRatio(NewLoad, NewHeavyThreshold));
	UE_MVVM_SET_PROPERTY_VALUE(EquipmentLoadTier, NewTier);
	SetText(EquipmentLoadText, MakeLoadText(NewLoad, NewHeavyThreshold), FFieldNotificationClassDescriptor::EquipmentLoadText);
	SetText(EquipmentLoadTierText, MakeLoadTierText(NewTier), FFieldNotificationClassDescriptor::EquipmentLoadTierText);
}

void URpgCharacterStatsViewModel::BindProgressionOfPlayerState(
	const APlayerController* PlayerController,
	const UAbilitySystemComponent* AbilitySystem)
{
	// The player state can replicate after the controller. The ability system lives on it, so either may supply it.
	const APlayerState* PlayerState = PlayerController ? PlayerController->PlayerState.Get() : nullptr;
	if (!PlayerState && AbilitySystem)
	{
		PlayerState = Cast<APlayerState>(AbilitySystem->GetOwnerActor());
	}
	if (PlayerState)
	{
		BindProgression(PlayerState->FindComponentByClass<URpgPlayerProgressionComponent>());
	}
}

void URpgCharacterStatsViewModel::EnterCombat()
{
	UWorld* World = GetObservedWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		CombatTimerHandle,
		FTimerDelegate::CreateUObject(this, &ThisClass::ExitCombat),
		FMath::Max(0.5f, GetDefault<URpgUISettings>()->HudCombatHoldSeconds),
		false);
	UE_MVVM_SET_PROPERTY_VALUE(bInCombat, true);
	RefreshHudContext();
}

void URpgCharacterStatsViewModel::ExitCombat()
{
	CombatTimerHandle.Invalidate();
	UE_MVVM_SET_PROPERTY_VALUE(bInCombat, false);
	RefreshHudContext();
}

void URpgCharacterStatsViewModel::ClearCombat()
{
	if (UWorld* World = GetObservedWorld())
	{
		World->GetTimerManager().ClearTimer(CombatTimerHandle);
	}
	ExitCombat();
}

void URpgCharacterStatsViewModel::RefreshHudContext()
{
	UE_MVVM_SET_PROPERTY_VALUE(bShowVitals, bInCombat || !bVitalsFull);
}

UWorld* URpgCharacterStatsViewModel::GetObservedWorld() const
{
	if (const UAbilitySystemComponent* AbilitySystem = ObservedAbilitySystem.Get())
	{
		return AbilitySystem->GetWorld();
	}
	const APlayerController* PlayerController = ObservedPlayerController.Get();
	return PlayerController ? PlayerController->GetWorld() : nullptr;
}

void URpgCharacterStatsViewModel::SetText(FText& Field, const FText& NewValue, const UE::FieldNotification::FFieldId FieldId)
{
	if (!Field.IdenticalTo(NewValue, CharacterStatsTextIdentityFlags))
	{
		Field = NewValue;
		BroadcastFieldValueChanged(FieldId);
	}
}
