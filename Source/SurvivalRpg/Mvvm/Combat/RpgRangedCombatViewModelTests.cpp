#if WITH_DEV_AUTOMATION_TESTS

#include "RpgRangedCombatViewModel.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Combat/RpgProjectileShotAutomationTestTypes.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"

#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "UObject/StrongObjectPtr.h"

namespace RpgRangedCombatViewModelTests
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

	FString ReadText(const UObject* ViewModel, const TCHAR* FieldName)
	{
		const FProperty* Property = ViewModel->GetClass()->FindPropertyByName(FieldName);
		const FText* Value = Property ? Property->ContainerPtrToValuePtr<FText>(ViewModel) : nullptr;
		return Value ? Value->ToString() : FString(TEXT("<missing>"));
	}

	bool ReadBool(const UObject* ViewModel, const TCHAR* FieldName)
	{
		const FProperty* Property = ViewModel->GetClass()->FindPropertyByName(FieldName);
		const bool* Value = Property ? Property->ContainerPtrToValuePtr<bool>(ViewModel) : nullptr;
		return Value && *Value;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgRangedCombatViewModelBowContentTest,
	"SurvivalRpg.UI.RangedCombat.BowShootsTheStandardArrow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgRangedCombatViewModelBowContentTest::RunTest(const FString& Parameters)
{
	using namespace RpgRangedCombatViewModelTests;
	UClass* BowEquipment = LoadClass<URpgEquipmentDefinition>(nullptr, TEXT("/GF_Combat_Ranged/Equipment/Bow/ED_Bow.ED_Bow_C"));
	UClass* ArrowItem = LoadClass<URpgInventoryItemDefinition>(nullptr, TEXT("/GF_Combat_Ranged/Items/ID_Arrow_Standard.ID_Arrow_Standard_C"));
	if (!TestTrue(TEXT("The bow content loads"), BowEquipment && ArrowItem)) return false;

	FScopedWorld ScopedWorld;
	if (!TestNotNull(TEXT("A standalone test world exists"), ScopedWorld.World)) return false;
	APawn* Pawn = ScopedWorld.World->SpawnActor<APawn>();
	URpgAbilitySystemComponent* AbilitySystem = NewObject<URpgAbilitySystemComponent>(Pawn, NAME_None, RF_Transient);
	Pawn->AddInstanceComponent(AbilitySystem);
	AbilitySystem->RegisterComponent();
	AbilitySystem->InitAbilityActorInfo(Pawn, Pawn);
	URpgEquipmentManagerComponent* Equipment = NewObject<URpgEquipmentManagerComponent>(Pawn, NAME_None, RF_Transient);
	Pawn->AddInstanceComponent(Equipment);
	Equipment->RegisterComponent();

	URpgRangedCombatViewModel* ViewModel = NewObject<URpgRangedCombatViewModel>();
	ViewModel->BindAbilitySystem(AbilitySystem);
	TestFalse(TEXT("An unarmed pawn has no ranged weapon"), ViewModel->GetHasRangedWeapon());
	TestTrue(TEXT("A bound pawn's equipment is watched"), ViewModel->IsTickingForTests());

	// The weapon instance the bow equips names the ammunition its shot spends; the HUD shows the same.
	if (!TestNotNull(TEXT("The bow equips"), Equipment->EquipItemInSlot(BowEquipment, ERpgEquipmentSlot::MainHand))) return false;
	ViewModel->RefreshWeapon();
	TestTrue(TEXT("The equipped bow is a ranged weapon"), ViewModel->GetHasRangedWeapon());
	TestFalse(TEXT("The standard arrow names itself"), ReadText(ViewModel, TEXT("AmmunitionName")).IsEmpty());
	const FProperty* IconProperty = ViewModel->GetClass()->FindPropertyByName(TEXT("AmmunitionIcon"));
	const TSoftObjectPtr<UTexture2D>* Icon = IconProperty ? IconProperty->ContainerPtrToValuePtr<TSoftObjectPtr<UTexture2D>>(ViewModel) : nullptr;
	TestTrue(TEXT("The standard arrow has an icon for the HUD"), Icon && !Icon->IsNull());

	Equipment->UnequipItemInSlot(ERpgEquipmentSlot::MainHand);
	ViewModel->RefreshWeapon();
	TestFalse(TEXT("Putting the bow away hides the ammunition"), ViewModel->GetHasRangedWeapon());
	ViewModel->Unbind();
	TestFalse(TEXT("Unbinding stops watching"), ViewModel->IsTickingForTests());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgRangedCombatViewModelAmmunitionTest,
	"SurvivalRpg.UI.RangedCombat.AmmunitionFollowsTheInventory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgRangedCombatViewModelAmmunitionTest::RunTest(const FString& Parameters)
{
	using namespace RpgRangedCombatViewModelTests;
	FScopedWorld ScopedWorld;
	if (!TestNotNull(TEXT("A standalone test world exists"), ScopedWorld.World)) return false;

	ARpgInventoryAutomationTestPlayerController* Controller = ScopedWorld.World->SpawnActor<ARpgInventoryAutomationTestPlayerController>();
	ARpgInventoryAutomationTestPlayerState* PlayerState = ScopedWorld.World->SpawnActor<ARpgInventoryAutomationTestPlayerState>();
	if (!TestTrue(TEXT("The player fixtures exist"), Controller && PlayerState)) return false;
	Controller->SetPlayerState(PlayerState);
	PlayerState->SetOwner(Controller);
	URpgInventoryManagerComponent* Inventory = PlayerState->GetInventoryManagerComponent();
	if (!TestNotNull(TEXT("The player inventory exists"), Inventory)) return false;

	const TSubclassOf<URpgInventoryItemDefinition> Arrow = URpgProjectileShotAutomationArrowDefinition::StaticClass();
	URpgRangedCombatViewModel* ViewModel = NewObject<URpgRangedCombatViewModel>();
	ViewModel->BindPlayerController(Controller);
	TestFalse(TEXT("Without a bow in hand there is no ranged weapon"), ViewModel->GetHasRangedWeapon());
	TestFalse(TEXT("Without a bow no ammunition is missing"), ReadBool(ViewModel, TEXT("bOutOfAmmunition")));

	ViewModel->SetAmmunitionForTests(Arrow);
	TestEqual(TEXT("A bow without arrows counts none"), ViewModel->GetAmmunitionCount(), 0);
	TestTrue(TEXT("A bow without arrows is out of ammunition"), ReadBool(ViewModel, TEXT("bOutOfAmmunition")));
	TestFalse(TEXT("A bow without arrows cannot shoot"), ViewModel->GetShotAvailable());

	if (!TestNotNull(TEXT("Arrows land in the inventory"), Inventory->GrantItemDefinition(Arrow, 7))) return false;
	TestEqual(TEXT("The count follows the inventory"), ViewModel->GetAmmunitionCount(), 7);
	TestEqual(TEXT("The count text follows it"), ReadText(ViewModel, TEXT("AmmunitionCountText")), FString(TEXT("7")));
	TestTrue(TEXT("A bow with arrows can shoot"), ViewModel->GetShotAvailable());

	// Arrows in another inventory, such as a chest, never count.
	AActor* ChestOwner = ScopedWorld.World->SpawnActor<AActor>();
	URpgInventoryManagerComponent* Chest = NewObject<URpgInventoryManagerComponent>(ChestOwner, NAME_None, RF_Transient);
	ChestOwner->AddInstanceComponent(Chest);
	Chest->RegisterComponent();
	Chest->GrantItemDefinition(Arrow, 5);
	TestEqual(TEXT("Another inventory's arrows never count"), ViewModel->GetAmmunitionCount(), 7);

	Inventory->ConsumeItemsByDefinition(Arrow, 7);
	TestEqual(TEXT("Spent arrows leave the count"), ViewModel->GetAmmunitionCount(), 0);
	TestTrue(TEXT("The last arrow spent leaves the bow out of ammunition"), ReadBool(ViewModel, TEXT("bOutOfAmmunition")));

	ViewModel->SetAmmunitionForTests(nullptr);
	TestFalse(TEXT("Putting the bow away hides the ammunition"), ViewModel->GetHasRangedWeapon());
	TestFalse(TEXT("Without a bow nothing is out of ammunition"), ReadBool(ViewModel, TEXT("bOutOfAmmunition")));
	ViewModel->Unbind();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgRangedCombatViewModelDrawTest,
	"SurvivalRpg.UI.RangedCombat.DrawStrengthFollowsTheLocalDraw",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgRangedCombatViewModelDrawTest::RunTest(const FString& Parameters)
{
	using namespace RpgRangedCombatViewModelTests;
	FScopedWorld ScopedWorld;
	if (!TestNotNull(TEXT("A standalone test world exists"), ScopedWorld.World)) return false;

	APawn* Pawn = ScopedWorld.World->SpawnActor<APawn>();
	URpgAbilitySystemComponent* AbilitySystem = NewObject<URpgAbilitySystemComponent>(Pawn, NAME_None, RF_Transient);
	Pawn->AddInstanceComponent(AbilitySystem);
	AbilitySystem->RegisterComponent();
	AbilitySystem->InitAbilityActorInfo(Pawn, Pawn);

	URpgRangedCombatViewModel* ViewModel = NewObject<URpgRangedCombatViewModel>();
	ViewModel->BindAbilitySystem(AbilitySystem);
	ViewModel->SetAmmunitionForTests(URpgProjectileShotAutomationArrowDefinition::StaticClass());
	TestFalse(TEXT("No stance aims"), ViewModel->GetAiming());
	TestFalse(TEXT("No draw draws"), ViewModel->GetDrawing());

	const double Now = ScopedWorld.World->GetTimeSeconds();
	AbilitySystem->SetLocalRangedDrawing(true);
	AbilitySystem->SetLocalRangedDrawTiming(Now - 0.1, 1.0f, 0.25f);
	ViewModel->RefreshRangedState();
	TestTrue(TEXT("A draw draws"), ViewModel->GetDrawing());
	TestEqual(TEXT("The draw strength follows the drawn time"), ViewModel->GetDrawFraction(), 0.1f, 0.001f);
	TestEqual(TEXT("The nock lies at its share of the full draw"), ViewModel->GetNockFraction(), 0.25f, 0.001f);
	TestFalse(TEXT("Before the nock a release shoots nothing"), ViewModel->GetNocked());

	AbilitySystem->SetLocalRangedDrawTiming(Now - 0.5, 1.0f, 0.25f);
	ViewModel->RefreshRangedState();
	TestEqual(TEXT("Halfway the draw strength is half"), ViewModel->GetDrawFraction(), 0.5f, 0.001f);
	TestTrue(TEXT("Past the nock a release shoots"), ViewModel->GetNocked());
	TestFalse(TEXT("Halfway is no full draw"), ViewModel->GetFullyDrawn());

	AbilitySystem->SetLocalRangedDrawTiming(Now - 3.0, 1.0f, 0.25f);
	ViewModel->RefreshRangedState();
	TestEqual(TEXT("The draw strength stops at full"), ViewModel->GetDrawFraction(), 1.0f);
	TestTrue(TEXT("A long draw is a full draw"), ViewModel->GetFullyDrawn());

	AbilitySystem->SetLocalRangedDrawing(false);
	ViewModel->RefreshRangedState();
	TestFalse(TEXT("A finished draw stops drawing"), ViewModel->GetDrawing());
	TestEqual(TEXT("A finished draw clears its strength"), ViewModel->GetDrawFraction(), 0.0f);

	ViewModel->BindAbilitySystem(nullptr);
	TestFalse(TEXT("Losing the ability system stops the tick"), ViewModel->IsTickingForTests());
	TestFalse(TEXT("Losing the ability system clears the draw"), ViewModel->GetDrawing());
	ViewModel->Unbind();
	return true;
}

#endif
