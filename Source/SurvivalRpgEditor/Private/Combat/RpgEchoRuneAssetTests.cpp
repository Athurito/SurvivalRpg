#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameplayEffect.h"
#include "Misc/AutomationTest.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Equipment/RpgWeaponInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_EquippableItem.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Runes/RpgInventoryFragment_WeaponRunes.h"
#include "SurvivalRpg/Runes/RpgWeaponRuneDefinition.h"
#include "SurvivalRpg/Runes/RpgWeaponSwingRecorderComponent.h"

namespace RpgEchoRuneAssetTests
{
	/** Whether a Blueprint actor class, or one of its Blueprint parents, adds a component of ComponentClass. */
	bool HasBlueprintComponent(const UClass* ActorClass, const UClass* ComponentClass)
	{
		for (const UBlueprintGeneratedClass* Class = Cast<UBlueprintGeneratedClass>(ActorClass); Class;
			Class = Cast<UBlueprintGeneratedClass>(Class->GetSuperClass()))
		{
			if (!Class->SimpleConstructionScript)
			{
				continue;
			}
			for (const USCS_Node* Node : Class->SimpleConstructionScript->GetAllNodes())
			{
				if (Node && Node->ComponentClass && Node->ComponentClass->IsChildOf(ComponentClass))
				{
					return true;
				}
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgEchoTestSwordAssetContractTest,
	"SurvivalRpg.Combat.Runes.EchoTestSwordAssetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgEchoTestSwordAssetContractTest::RunTest(const FString& Parameters)
{
	using namespace RpgEchoRuneAssetTests;

	const UClass* ItemClass = LoadClass<URpgInventoryItemDefinition>(
		nullptr, TEXT("/GF_Runes_Core/Weapons/ID_EchoTestSword.ID_EchoTestSword_C"));
	const URpgInventoryItemDefinition* Item = ItemClass ? GetDefault<URpgInventoryItemDefinition>(ItemClass) : nullptr;
	if (!TestNotNull(TEXT("The Echo test sword item definition loads"), Item))
	{
		return false;
	}

	const URpgInventoryFragment_WeaponRunes* Runes = Cast<URpgInventoryFragment_WeaponRunes>(
		Item->FindFragmentByClass(URpgInventoryFragment_WeaponRunes::StaticClass()));
	const URpgWeaponRuneDefinition* EchoRune = Runes ? Runes->FindEchoRune() : nullptr;
	if (TestNotNull(TEXT("The test sword carries a rune with an enabled Echo block"), EchoRune))
	{
		const FRpgEchoStrikeSettings& Echo = EchoRune->EchoStrike;
		TestNotNull(TEXT("The Echo strike applies a damage effect"), Echo.DamageEffect.Get());
		TestTrue(TEXT("The Echo strike deals health damage"), Echo.Damage > 0.0f);
		TestTrue(TEXT("The Echo strike triggers after at least one hit"), Echo.HitsToTrigger >= 1);
		TestTrue(TEXT("The Echo strike plays a strike cue for its flare and afterimage"), Echo.StrikeCueTag.IsValid());
	}

	const URpgInventoryFragment_EquippableItem* Equippable = Cast<URpgInventoryFragment_EquippableItem>(
		Item->FindFragmentByClass(URpgInventoryFragment_EquippableItem::StaticClass()));
	const TSubclassOf<URpgEquipmentDefinition> EquipmentClass = Equippable ? Equippable->GetEquipmentDefinition() : nullptr;
	const URpgEquipmentDefinition* Equipment = EquipmentClass ? GetDefault<URpgEquipmentDefinition>(EquipmentClass) : nullptr;
	if (!TestNotNull(TEXT("The test sword is equippable"), Equipment))
	{
		return false;
	}

	TestTrue(TEXT("The test sword equips as a weapon instance, which runs the rune"),
		Equipment->InstanceType && Equipment->InstanceType->IsChildOf(URpgWeaponInstance::StaticClass()));
	const UClass* WeaponActorClass = Equipment->ActorsToSpawn.IsEmpty() ? nullptr : Equipment->ActorsToSpawn[0].ActorToSpawn.Get();
	TestTrue(TEXT("The spawned test sword records its swing for the afterimage"),
		WeaponActorClass && HasBlueprintComponent(WeaponActorClass, URpgWeaponSwingRecorderComponent::StaticClass()));
	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
