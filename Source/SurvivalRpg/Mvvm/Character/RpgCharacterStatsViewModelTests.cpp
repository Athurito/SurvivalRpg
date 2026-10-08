#if WITH_DEV_AUTOMATION_TESTS

#include "RpgCharacterStatsViewModel.h"

#include "Curves/CurveFloat.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgDefenseSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgStaminaSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Progression/Player/Data/RpgPlayerProgressionData.h"
#include "SurvivalRpg/Progression/Player/RpgPlayerProgressionComponent.h"
#include "UObject/StrongObjectPtr.h"

namespace RpgCharacterStatsViewModelTests
{
	class FScopedWorld
	{
	public:
		FScopedWorld()
		{
			GameInstance.Reset(NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient));
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}

		~FScopedWorld()
		{
			GameInstance->Shutdown();
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}

		TStrongObjectPtr<UGameInstance> GameInstance;
		UWorld* World = nullptr;
	};

	/** Reads a view model field by the name its bindings use. */
	template <typename TValue>
	const TValue* FindField(const UObject* ViewModel, const TCHAR* FieldName)
	{
		const FProperty* Property = ViewModel ? ViewModel->GetClass()->FindPropertyByName(FieldName) : nullptr;
		return Property ? Property->ContainerPtrToValuePtr<TValue>(ViewModel) : nullptr;
	}

	float ReadFloat(const UObject* ViewModel, const TCHAR* FieldName)
	{
		const float* Value = FindField<float>(ViewModel, FieldName);
		return Value ? *Value : -1.0f;
	}

	FString ReadText(const UObject* ViewModel, const TCHAR* FieldName)
	{
		const FText* Value = FindField<FText>(ViewModel, FieldName);
		return Value ? Value->ToString() : FString(TEXT("<missing>"));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCharacterStatsViewModelAttributesTest,
	"SurvivalRpg.UI.CharacterStats.AttributesFollowAbilitySystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCharacterStatsViewModelAttributesTest::RunTest(const FString& Parameters)
{
	using namespace RpgCharacterStatsViewModelTests;

	FScopedWorld ScopedWorld;
	if (!TestNotNull(TEXT("A standalone test world exists"), ScopedWorld.World))
	{
		return false;
	}

	APawn* Pawn = ScopedWorld.World->SpawnActor<APawn>();
	URpgAbilitySystemComponent* AbilitySystem = NewObject<URpgAbilitySystemComponent>(Pawn, NAME_None, RF_Transient);
	Pawn->AddInstanceComponent(AbilitySystem);
	AbilitySystem->RegisterComponent();
	URpgHealthSet* HealthSet = NewObject<URpgHealthSet>(Pawn, NAME_None, RF_Transient);
	HealthSet->InitHealth(84.0f);
	HealthSet->InitMaxHealth(100.0f);
	URpgStaminaSet* StaminaSet = NewObject<URpgStaminaSet>(Pawn, NAME_None, RF_Transient);
	StaminaSet->InitStamina(60.0f);
	StaminaSet->InitMaxStamina(80.0f);
	URpgDefenseSet* DefenseSet = NewObject<URpgDefenseSet>(Pawn, NAME_None, RF_Transient);
	DefenseSet->InitArmor(12.4f);
	AbilitySystem->AddAttributeSetSubobject(HealthSet);
	AbilitySystem->AddAttributeSetSubobject(StaminaSet);
	AbilitySystem->AddAttributeSetSubobject(DefenseSet);
	AbilitySystem->InitAbilityActorInfo(Pawn, Pawn);

	URpgCharacterStatsViewModel* ViewModel = NewObject<URpgCharacterStatsViewModel>();
	int32 HealthTextNotifications = 0;
	const UE::FieldNotification::FFieldId HealthTextField =
		ViewModel->GetFieldNotificationDescriptor().GetField(ViewModel->GetClass(), TEXT("HealthText"));
	TestTrue(TEXT("HealthText is a field-notify field"), HealthTextField.IsValid());
	ViewModel->AddFieldValueChangedDelegate(
		HealthTextField,
		INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateLambda(
			[&HealthTextNotifications](UObject*, UE::FieldNotification::FFieldId) { ++HealthTextNotifications; }));

	ViewModel->BindAbilitySystem(AbilitySystem);
	TestEqual(TEXT("Health mirrors the attribute"), ReadFloat(ViewModel, TEXT("Health")), 84.0f);
	TestEqual(TEXT("Health progress is current over maximum"), ReadFloat(ViewModel, TEXT("HealthProgress")), 0.84f);
	TestEqual(TEXT("Health text shows current over maximum"), ReadText(ViewModel, TEXT("HealthText")), FString(TEXT("84 / 100")));
	TestEqual(TEXT("Stamina progress is current over maximum"), ReadFloat(ViewModel, TEXT("StaminaProgress")), 0.75f);
	TestEqual(TEXT("Stamina text shows current over maximum"), ReadText(ViewModel, TEXT("StaminaText")), FString(TEXT("60 / 80")));
	TestEqual(TEXT("Armour mirrors the defense attribute"), ReadFloat(ViewModel, TEXT("Armor")), 12.4f);
	TestEqual(TEXT("Armour text is rounded"), ReadText(ViewModel, TEXT("ArmorText")), FString(TEXT("12")));
	TestEqual(TEXT("Binding notifies the health text once"), HealthTextNotifications, 1);

	AbilitySystem->SetNumericAttributeBase(URpgHealthSet::GetHealthAttribute(), 40.0f);
	TestEqual(TEXT("An attribute change updates the health field"), ReadFloat(ViewModel, TEXT("Health")), 40.0f);
	TestEqual(TEXT("An attribute change updates the health text"), ReadText(ViewModel, TEXT("HealthText")), FString(TEXT("40 / 100")));
	TestEqual(TEXT("The health text notifies once per change"), HealthTextNotifications, 2);
	AbilitySystem->SetNumericAttributeBase(URpgStaminaSet::GetStaminaAttribute(), 20.0f);
	TestEqual(TEXT("A stamina change leaves the health text quiet"), HealthTextNotifications, 2);
	TestEqual(TEXT("A stamina change updates its progress"), ReadFloat(ViewModel, TEXT("StaminaProgress")), 0.25f);

	ViewModel->UnbindAbilitySystem();
	TestEqual(TEXT("Unbinding clears health"), ReadFloat(ViewModel, TEXT("Health")), 0.0f);
	TestEqual(TEXT("Unbinding clears armour"), ReadFloat(ViewModel, TEXT("Armor")), 0.0f);
	AbilitySystem->SetNumericAttributeBase(URpgHealthSet::GetHealthAttribute(), 70.0f);
	TestEqual(TEXT("An unbound view model ignores later attribute changes"), ReadFloat(ViewModel, TEXT("Health")), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCharacterStatsViewModelProgressionAndLoadTest,
	"SurvivalRpg.UI.CharacterStats.ProgressionAndLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCharacterStatsViewModelProgressionAndLoadTest::RunTest(const FString& Parameters)
{
	using namespace RpgCharacterStatsViewModelTests;

	FScopedWorld ScopedWorld;
	if (!TestNotNull(TEXT("A standalone test world exists"), ScopedWorld.World))
	{
		return false;
	}

	AActor* ProgressionOwner = ScopedWorld.World->SpawnActor<AActor>();
	URpgPlayerProgressionComponent* Progression = NewObject<URpgPlayerProgressionComponent>(ProgressionOwner, NAME_None, RF_Transient);
	URpgPlayerProgressionData* ProgressionData = NewObject<URpgPlayerProgressionData>(Progression);
	UCurveFloat* XPCurve = NewObject<UCurveFloat>(ProgressionData);
	XPCurve->FloatCurve.AddKey(1.0f, 400.0f);
	XPCurve->FloatCurve.AddKey(10.0f, 400.0f);
	ProgressionData->XPToNextLevel = XPCurve;
	ProgressionData->MaxLevel = 5;
	Progression->ConfigData = ProgressionData;
	ProgressionOwner->AddInstanceComponent(Progression);
	Progression->RegisterComponent();

	URpgCharacterStatsViewModel* ViewModel = NewObject<URpgCharacterStatsViewModel>();
	ViewModel->BindProgression(Progression);
	FPlayerProgressionState MidLevel;
	MidLevel.Level = 3;
	MidLevel.XP = 120.0f;
	TestTrue(TEXT("The authority restores a progression snapshot"), Progression->RestoreProgressionState(MidLevel));
	const int32* Level = FindField<int32>(ViewModel, TEXT("CharacterLevel"));
	TestTrue(TEXT("The level follows the progression"), Level && *Level == 3);
	TestEqual(TEXT("The level text follows the progression"), ReadText(ViewModel, TEXT("CharacterLevelText")), FString(TEXT("3")));
	TestEqual(TEXT("Experience progress uses the XP curve"), ReadFloat(ViewModel, TEXT("CharacterLevelProgress")), 0.3f);
	TestEqual(TEXT("Experience text shows XP over the next level"), ReadText(ViewModel, TEXT("CharacterXPText")), FString(TEXT("120 / 400")));

	FPlayerProgressionState MaxLevel;
	MaxLevel.Level = 5;
	Progression->RestoreProgressionState(MaxLevel);
	TestEqual(TEXT("The maximum level fills the experience bar"), ReadFloat(ViewModel, TEXT("CharacterLevelProgress")), 1.0f);
	TestEqual(TEXT("The maximum level replaces the experience numbers"), ReadText(ViewModel, TEXT("CharacterXPText")), FString(TEXT("Max level")));

	URpgPlayerProgressionComponent* CurvelessProgression = NewObject<URpgPlayerProgressionComponent>(ProgressionOwner, NAME_None, RF_Transient);
	ProgressionOwner->AddInstanceComponent(CurvelessProgression);
	CurvelessProgression->RegisterComponent();
	ViewModel->BindProgression(CurvelessProgression);
	FPlayerProgressionState CurvelessState;
	CurvelessState.XP = 10.0f;
	CurvelessProgression->RestoreProgressionState(CurvelessState);
	TestEqual(TEXT("Without an XP curve the bar stays empty"), ReadFloat(ViewModel, TEXT("CharacterLevelProgress")), 0.0f);
	TestEqual(TEXT("Without an XP curve the text shows the experience alone"), ReadText(ViewModel, TEXT("CharacterXPText")), FString(TEXT("10 XP")));

	ViewModel->BindProgression(nullptr);
	TestTrue(TEXT("Unbinding progression returns to level 1"), Level && *Level == 1);

	APlayerController* PlayerController = ScopedWorld.World->SpawnActor<APlayerController>();
	ViewModel->BindPlayerController(PlayerController);
	const float HeavyThreshold = GetDefault<URpgEquipmentLoadoutComponent>()->GetHeavyLoadThreshold();
	TestEqual(TEXT("Without a loadout the heavy threshold comes from the class defaults"), ReadFloat(ViewModel, TEXT("HeavyLoadThreshold")), HeavyThreshold);

	FRpgEquipmentLoadoutSlotsChangedMessage LoadMessage;
	LoadMessage.Owner = PlayerController;
	LoadMessage.EquipmentLoadWeight = 15.0f;
	LoadMessage.EquipmentLoadTier = ERpgEquipmentLoadTier::Medium;
	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(ScopedWorld.World);
	MessageSubsystem.BroadcastMessage(RpgGameplayTags::Rpg_EquipmentLoadout_Message_SlotsChanged, LoadMessage);
	TestEqual(TEXT("The loadout message sets the load"), ReadFloat(ViewModel, TEXT("EquipmentLoad")), 15.0f);
	TestEqual(TEXT("The load bar fills toward the heavy threshold"), ReadFloat(ViewModel, TEXT("EquipmentLoadProgress")), 15.0f / HeavyThreshold);
	const ERpgEquipmentLoadTier* Tier = FindField<ERpgEquipmentLoadTier>(ViewModel, TEXT("EquipmentLoadTier"));
	TestTrue(TEXT("The loadout message sets the tier"), Tier && *Tier == ERpgEquipmentLoadTier::Medium);
	TestEqual(TEXT("The tier text names the tier"), ReadText(ViewModel, TEXT("EquipmentLoadTierText")), FString(TEXT("Medium")));
	TestTrue(TEXT("The load text ends with the heavy threshold in kilograms"),
		ReadText(ViewModel, TEXT("EquipmentLoadText")).EndsWith(FString::Printf(TEXT("/ %d kg"), FMath::RoundToInt(HeavyThreshold))));

	FRpgEquipmentLoadoutSlotsChangedMessage OtherPlayerMessage = LoadMessage;
	OtherPlayerMessage.Owner = ProgressionOwner;
	OtherPlayerMessage.EquipmentLoadWeight = 30.0f;
	MessageSubsystem.BroadcastMessage(RpgGameplayTags::Rpg_EquipmentLoadout_Message_SlotsChanged, OtherPlayerMessage);
	TestEqual(TEXT("Another owner's loadout message is ignored"), ReadFloat(ViewModel, TEXT("EquipmentLoad")), 15.0f);

	ViewModel->Unbind();
	TestEqual(TEXT("Unbinding clears the load"), ReadFloat(ViewModel, TEXT("EquipmentLoad")), 0.0f);
	MessageSubsystem.BroadcastMessage(RpgGameplayTags::Rpg_EquipmentLoadout_Message_SlotsChanged, LoadMessage);
	TestEqual(TEXT("An unbound view model ignores later loadout messages"), ReadFloat(ViewModel, TEXT("EquipmentLoad")), 0.0f);
	return true;
}

#endif
