#if WITH_DEV_AUTOMATION_TESTS

#include "EnemyVitalsViewmodel.h"
#include "RpgEnemyVitalsIndicatorWidget.h"

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ScopeExit.h"
#include "Misc/AutomationTest.h"
#include "MVVMSubsystem.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/UI/IndicatorSystem/IndicatorDescriptor.h"
#include "UObject/StrongObjectPtr.h"
#include "View/MVVMView.h"
#include "View/MVVMViewClass.h"

namespace RpgEnemyVitalsViewmodelTests
{
	const TCHAR* const EnemyHealthBarIndicatorClassPath =
		TEXT("/GF_Combat_Core/UI/Indicators/W_EnemyHealthBarIndicator.W_EnemyHealthBarIndicator_C");

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

	/**
	 * An enemy-like character whose ability system the pawn extension has not initialized yet. ARpgCharacter refills
	 * health when its ability system initializes, so checks compare against the attributes, not the spawn values.
	 */
	struct FObservedCharacter
	{
		bool Spawn(FAutomationTestBase& Test, UWorld& World, float MaxHealth)
		{
			Character = World.SpawnActor<ARpgCharacter>();
			if (!Test.TestNotNull(TEXT("The observed character exists"), Character))
			{
				return false;
			}

			AbilitySystem = NewObject<URpgAbilitySystemComponent>(Character, NAME_None, RF_Transient);
			Character->AddInstanceComponent(AbilitySystem);
			AbilitySystem->RegisterComponent();
			URpgHealthSet* HealthSet = NewObject<URpgHealthSet>(Character, NAME_None, RF_Transient);
			HealthSet->InitHealth(MaxHealth);
			HealthSet->InitMaxHealth(MaxHealth);
			AbilitySystem->AddAttributeSetSubobject(HealthSet);

			Extension = URpgPawnExtensionComponent::FindPawnExtensionComponent(Character);
			if (!Test.TestNotNull(TEXT("The observed character has a pawn extension"), Extension))
			{
				return false;
			}
			Extension->SetPawnData(NewObject<URpgPawnData>(Character, NAME_None, RF_Transient));
			return true;
		}

		void InitializeAbilitySystem() const
		{
			Extension->InitializeAbilitySystemComponent(AbilitySystem, Character);
		}

		void SetHealth(float Health) const
		{
			AbilitySystem->SetNumericAttributeBase(URpgHealthSet::GetHealthAttribute(), Health);
		}

		float HealthPercent() const
		{
			const float MaxHealth = AbilitySystem->GetNumericAttribute(URpgHealthSet::GetMaxHealthAttribute());
			return MaxHealth > 0.0f ? AbilitySystem->GetNumericAttribute(URpgHealthSet::GetHealthAttribute()) / MaxHealth : 0.0f;
		}

		ARpgCharacter* Character = nullptr;
		URpgAbilitySystemComponent* AbilitySystem = nullptr;
		URpgPawnExtensionComponent* Extension = nullptr;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgEnemyVitalsHealthFollowsPawnExtensionTest,
	"SurvivalRpg.UI.EnemyVitals.HealthFollowsPawnExtension",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgEnemyVitalsHealthFollowsPawnExtensionTest::RunTest(const FString& Parameters)
{
	using namespace RpgEnemyVitalsViewmodelTests;

	FScopedWorld ScopedWorld;
	FObservedCharacter Observed;
	if (!TestNotNull(TEXT("A standalone test world exists"), ScopedWorld.World)
		|| !Observed.Spawn(*this, *ScopedWorld.World, 100.0f))
	{
		return false;
	}

	UEnemyVitalsViewmodel* ViewModel = NewObject<UEnemyVitalsViewmodel>();
	int32 PercentNotifications = 0;
	const UE::FieldNotification::FFieldId PercentField =
		ViewModel->GetFieldNotificationDescriptor().GetField(ViewModel->GetClass(), TEXT("GetHealthPercent"));
	TestTrue(TEXT("GetHealthPercent is a field-notify function"), PercentField.IsValid());
	ViewModel->AddFieldValueChangedDelegate(
		PercentField,
		INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateLambda(
			[&PercentNotifications](UObject*, UE::FieldNotification::FFieldId) { ++PercentNotifications; }));

	ViewModel->BindToActor(Observed.Character);
	TestEqual(TEXT("Before the ability system exists the bar is empty"), ViewModel->GetHealthPercent(), 0.0f);

	Observed.InitializeAbilitySystem();
	TestTrue(TEXT("The observed health is set"), Observed.HealthPercent() > 0.0f);
	TestEqual(TEXT("Initializing the ability system shows its health"), ViewModel->GetHealthPercent(), Observed.HealthPercent());
	TestTrue(TEXT("Initializing the ability system notifies the percent"), PercentNotifications > 0);

	const int32 NotificationsBeforeHit = PercentNotifications;
	Observed.SetHealth(30.0f);
	TestEqual(TEXT("A health change updates the percent"), ViewModel->GetHealthPercent(), 0.3f);
	TestEqual(TEXT("A health change notifies the percent once"), PercentNotifications, NotificationsBeforeHit + 1);

	Observed.Extension->UninitializeAbilitySystem();
	Observed.SetHealth(20.0f);
	TestEqual(TEXT("A torn-down ability system no longer drives the bar"), ViewModel->GetHealthPercent(), 0.3f);

	Observed.InitializeAbilitySystem();
	TestEqual(TEXT("A re-initialized ability system is observed again"), ViewModel->GetHealthPercent(), Observed.HealthPercent());
	Observed.SetHealth(50.0f);
	TestEqual(TEXT("The re-initialized ability system drives the bar"), ViewModel->GetHealthPercent(), 0.5f);

	ViewModel->UnbindFromActor();
	const int32 NotificationsAfterUnbind = PercentNotifications;
	Observed.SetHealth(10.0f);
	TestEqual(TEXT("An unbound view model stays quiet"), PercentNotifications, NotificationsAfterUnbind);
	TestEqual(TEXT("An unbound view model keeps its last value"), ViewModel->GetHealthPercent(), 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgEnemyVitalsIndicatorWidgetContractTest,
	"SurvivalRpg.UI.EnemyVitals.IndicatorWidgetTakesViewModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgEnemyVitalsIndicatorWidgetContractTest::RunTest(const FString& Parameters)
{
	using namespace RpgEnemyVitalsViewmodelTests;

	UClass* IndicatorClass = LoadClass<URpgEnemyVitalsIndicatorWidget>(nullptr, EnemyHealthBarIndicatorClassPath);
	UWidgetBlueprintGeneratedClass* GeneratedClass = Cast<UWidgetBlueprintGeneratedClass>(IndicatorClass);
	if (!TestNotNull(TEXT("The enemy health bar indicator loads as an enemy vitals indicator"), IndicatorClass)
		|| !TestNotNull(TEXT("The enemy health bar indicator is a Widget Blueprint"), GeneratedClass))
	{
		return false;
	}

	// URpgEnemyVitalsIndicatorWidget hands its view model over with SetViewModelByClass, which needs exactly one
	// settable source that accepts it.
	const TArray<UWidgetBlueprintGeneratedClassExtension*> ViewExtensions =
		GeneratedClass->GetExtensions(UMVVMViewClass::StaticClass(), /*bIncludeSuper=*/ true);
	const UMVVMViewClass* ViewClass = ViewExtensions.Num() == 1 ? Cast<UMVVMViewClass>(ViewExtensions[0]) : nullptr;
	if (!TestNotNull(TEXT("The indicator has one compiled MVVM view"), ViewClass))
	{
		return false;
	}

	int32 AcceptingSources = 0;
	for (const FMVVMViewClass_Source& Source : ViewClass->GetSources())
	{
		if (UEnemyVitalsViewmodel::StaticClass()->IsChildOf(Source.GetSourceClass()))
		{
			++AcceptingSources;
			TestTrue(TEXT("The enemy vitals source can be set from native code"), Source.CanBeSet());
		}
	}
	TestEqual(TEXT("Exactly one source accepts the enemy vitals view model"), AcceptingSources, 1);

	FScopedWorld ScopedWorld;
	FObservedCharacter Observed;
	if (!TestNotNull(TEXT("A standalone test world exists"), ScopedWorld.World)
		|| !Observed.Spawn(*this, *ScopedWorld.World, 80.0f))
	{
		return false;
	}
	Observed.InitializeAbilitySystem();

	// The indicator canvas creates indicators for its local player, and only a widget with a player context runs
	// the initialization that creates its MVVM view. A bare controller-player link provides that context; the test
	// world never initializes its actors, so the controller joins the world's controller list by hand.
	ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine, NAME_None, RF_Transient);
	APlayerController* PlayerController = ScopedWorld.World->SpawnActor<APlayerController>();
	if (!TestNotNull(TEXT("The indicator's owning controller exists"), PlayerController))
	{
		return false;
	}
	PlayerController->Player = LocalPlayer;
	LocalPlayer->PlayerController = PlayerController;
	ScopedWorld.World->AddController(PlayerController);
	ON_SCOPE_EXIT
	{
		PlayerController->Player = nullptr;
		LocalPlayer->PlayerController = nullptr;
	};

	URpgEnemyVitalsIndicatorWidget* Indicator = CreateWidget<URpgEnemyVitalsIndicatorWidget>(PlayerController, IndicatorClass);
	UMVVMView* View = Indicator ? UMVVMSubsystem::GetViewFromUserWidget(Indicator) : nullptr;
	if (!TestNotNull(TEXT("The indicator widget initializes"), Indicator)
		|| !TestNotNull(TEXT("The indicator has a runtime MVVM view"), View))
	{
		return false;
	}

	UIndicatorDescriptor* Descriptor = NewObject<UIndicatorDescriptor>();
	Descriptor->SetDataObject(Observed.Character);
	IIndicatorWidgetInterface::Execute_BindIndicator(Indicator, Descriptor);

	const UEnemyVitalsViewmodel* BoundViewModel = nullptr;
	for (const FMVVMViewClass_Source& Source : ViewClass->GetSources())
	{
		if (UEnemyVitalsViewmodel::StaticClass()->IsChildOf(Source.GetSourceClass()))
		{
			BoundViewModel = Cast<UEnemyVitalsViewmodel>(View->GetViewModel(Source.GetName()).GetObject());
		}
	}
	if (!TestNotNull(TEXT("Binding the indicator sets its enemy vitals source"), BoundViewModel))
	{
		return false;
	}
	TestEqual(TEXT("The bound view model reads the enemy's health"), BoundViewModel->GetHealthPercent(), Observed.HealthPercent());

	Observed.SetHealth(20.0f);
	TestEqual(TEXT("The bound view model follows a hit"), BoundViewModel->GetHealthPercent(), 0.25f);

	IIndicatorWidgetInterface::Execute_UnbindIndicator(Indicator, Descriptor);
	Observed.SetHealth(60.0f);
	TestEqual(TEXT("An unbound indicator stops following the enemy"), BoundViewModel->GetHealthPercent(), 0.25f);
	return true;
}

#endif
