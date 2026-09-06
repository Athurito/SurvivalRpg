#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimMontage.h"
#include "Engine/Blueprint.h"
#include "Misc/AutomationTest.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility_BasicWeaponAttack.h"
#include "SurvivalRpg/Animation/AnimNotify_RpgWeaponAttackWindow.h"
#include "SurvivalRpg/Equipment/RpgWeaponInstance.h"
#include "SurvivalRpg/Inventory/RpgStarterInventoryComponent.h"
#include "UObject/UnrealType.h"

namespace RpgWeaponAttackAssetTests
{
	struct FWeaponBlueprintContract
	{
		const TCHAR* Description;
		const TCHAR* ObjectPath;
	};

	constexpr FWeaponBlueprintContract WeaponBlueprintContracts[] = {
		{
			TEXT("BasicSword weapon instance"),
			TEXT(
				"/GF_Combat_Core/Equipment/Weapons/"
				"BP_WeaponInstance_BasicSword.BP_WeaponInstance_BasicSword"),
		},
		{
			TEXT("BasicTwoHandedSword weapon instance"),
			TEXT(
				"/GF_Combat_Core/Equipment/Weapons/"
				"BP_WeaponInstance_BasicTwoHandedSword."
				"BP_WeaponInstance_BasicTwoHandedSword"),
		},
	};

	const URpgWeaponInstance* LoadWeaponDefaults(
		FAutomationTestBase& Test,
		const FWeaponBlueprintContract& Contract)
	{
		UBlueprint* WeaponBlueprint = LoadObject<UBlueprint>(
			nullptr,
			Contract.ObjectPath);
		if (!Test.TestNotNull(
				*FString::Printf(
					TEXT("The %s Blueprint loads"),
					Contract.Description),
				WeaponBlueprint))
		{
			return nullptr;
		}

		UClass* GeneratedClass = WeaponBlueprint->GeneratedClass.Get();
		if (!Test.TestNotNull(
				*FString::Printf(
					TEXT("The %s Blueprint has a generated class"),
					Contract.Description),
				GeneratedClass))
		{
			return nullptr;
		}

		Test.TestTrue(
			*FString::Printf(
				TEXT("The %s generated class derives from URpgWeaponInstance"),
				Contract.Description),
			GeneratedClass->IsChildOf(URpgWeaponInstance::StaticClass()));

		const URpgWeaponInstance* WeaponDefaults =
			Cast<URpgWeaponInstance>(GeneratedClass->GetDefaultObject());
		Test.TestNotNull(
			*FString::Printf(
				TEXT("The %s CDO loads"),
				Contract.Description),
			WeaponDefaults);
		return WeaponDefaults;
	}

	void ValidateWindowNotify(
		FAutomationTestBase& Test,
		const FString& AttackContext,
		const TCHAR* WindowBoundary,
		const FAnimNotifyEvent& NotifyEvent,
		const UClass* ExpectedNotifyClass,
		float MontageLength)
	{
		const FString NotifyContext = FString::Printf(
			TEXT("%s %s notify"),
			*AttackContext,
			WindowBoundary);

		Test.TestTrue(
			*FString::Printf(
				TEXT("%s uses the direct project runtime class"),
				*NotifyContext),
			NotifyEvent.Notify &&
				NotifyEvent.Notify->GetClass() == ExpectedNotifyClass);

		const float TriggerTime = NotifyEvent.GetTriggerTime();
		Test.TestTrue(
			*FString::Printf(
				TEXT("%s occurs within the montage"),
				*NotifyContext),
			FMath::IsFinite(TriggerTime) &&
				TriggerTime >= 0.0f &&
				TriggerTime <= MontageLength);
		Test.TestEqual(
			*FString::Printf(
				TEXT("%s always triggers"),
				*NotifyContext),
			NotifyEvent.NotifyTriggerChance,
			1.0f);
		Test.TestEqual(
			*FString::Printf(
				TEXT("%s uses queued montage delivery"),
				*NotifyContext),
			static_cast<uint8>(NotifyEvent.MontageTickType.GetValue()),
			static_cast<uint8>(EMontageNotifyTickType::Queued));
		Test.TestEqual(
			*FString::Printf(
				TEXT("%s sets NotifyFilterType to NoFiltering"),
				*NotifyContext),
			static_cast<uint8>(NotifyEvent.NotifyFilterType.GetValue()),
			static_cast<uint8>(ENotifyFilterType::NoFiltering));
		Test.TestTrue(
			*FString::Printf(
				TEXT("%s triggers on dedicated servers"),
				*NotifyContext),
			NotifyEvent.bTriggerOnDedicatedServer);
	}

	void ValidateAttackDefinition(
		FAutomationTestBase& Test,
		const TCHAR* WeaponDescription,
		const FGameplayTag& AttackTag,
		const FRpgWeaponAttackDefinition& AttackDefinition)
	{
		const FString AttackContext = FString::Printf(
			TEXT("%s attack %s"),
			WeaponDescription,
			*AttackTag.ToString());

		Test.TestTrue(
			*FString::Printf(
				TEXT("%s has a finite positive montage play rate"),
				*AttackContext),
			FMath::IsFinite(AttackDefinition.MontagePlayRate) &&
				AttackDefinition.MontagePlayRate > 0.0f);

		const UAnimMontage* Montage = AttackDefinition.Montage;
		if (!Test.TestNotNull(
				*FString::Printf(
					TEXT("%s references an attack montage"),
					*AttackContext),
				Montage))
		{
			return;
		}

		const float MontageLength = Montage->GetPlayLength();
		const float EffectivePlayRate =
			AttackDefinition.MontagePlayRate * Montage->RateScale;
		Test.TestTrue(
			*FString::Printf(
				TEXT("%s montage has a finite positive length"),
				*AttackContext),
			FMath::IsFinite(MontageLength) && MontageLength > 0.0f);
		Test.TestTrue(
			*FString::Printf(
				TEXT("%s montage RateScale is finite and positive"),
				*AttackContext),
			FMath::IsFinite(Montage->RateScale) && Montage->RateScale > 0.0f);
		Test.TestTrue(
			*FString::Printf(
				TEXT("%s has a finite positive effective play rate"),
				*AttackContext),
			FMath::IsFinite(EffectivePlayRate) && EffectivePlayRate > 0.0f);
		Test.TestFalse(
			*FString::Printf(
				TEXT("%s montage has no time-stretch curve"),
				*AttackContext),
			Montage->TimeStretchCurve.IsValid());
		Test.TestEqual(
			*FString::Printf(
				TEXT("%s montage has exactly one linear section"),
				*AttackContext),
			Montage->CompositeSections.Num(),
			1);
		if (Montage->CompositeSections.Num() == 1)
		{
			const FCompositeSection& Section = Montage->CompositeSections[0];
			Test.TestTrue(
				*FString::Printf(
					TEXT("%s montage section starts at zero"),
					*AttackContext),
				FMath::IsNearlyZero(Section.GetTime(), KINDA_SMALL_NUMBER));
			Test.TestTrue(
				*FString::Printf(
					TEXT("%s montage section does not loop or jump"),
					*AttackContext),
				Section.NextSectionName.IsNone());
		}

		TArray<const FAnimNotifyEvent*> StartNotifies;
		TArray<const FAnimNotifyEvent*> EndNotifies;
		for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
		{
			if (NotifyEvent.Notify &&
				NotifyEvent.Notify->IsA<UAnimNotify_RpgWeaponAttackWindowStart>())
			{
				StartNotifies.Add(&NotifyEvent);
			}
			else if (NotifyEvent.Notify &&
				NotifyEvent.Notify->IsA<UAnimNotify_RpgWeaponAttackWindowEnd>())
			{
				EndNotifies.Add(&NotifyEvent);
			}
		}

		Test.TestEqual(
			*FString::Printf(
				TEXT("%s montage has exactly one attack-window start notify"),
				*AttackContext),
			StartNotifies.Num(),
			1);
		Test.TestEqual(
			*FString::Printf(
				TEXT("%s montage has exactly one attack-window end notify"),
				*AttackContext),
			EndNotifies.Num(),
			1);

		for (const FAnimNotifyEvent* StartNotify : StartNotifies)
		{
			ValidateWindowNotify(
				Test,
				AttackContext,
				TEXT("start"),
				*StartNotify,
				UAnimNotify_RpgWeaponAttackWindowStart::StaticClass(),
				MontageLength);
		}
		for (const FAnimNotifyEvent* EndNotify : EndNotifies)
		{
			ValidateWindowNotify(
				Test,
				AttackContext,
				TEXT("end"),
				*EndNotify,
				UAnimNotify_RpgWeaponAttackWindowEnd::StaticClass(),
				MontageLength);
		}

		if (StartNotifies.Num() == 1 && EndNotifies.Num() == 1)
		{
			Test.TestTrue(
				*FString::Printf(
					TEXT("%s attack-window start precedes its end"),
					*AttackContext),
				StartNotifies[0]->GetTriggerTime() <
					EndNotifies[0]->GetTriggerTime());

			if (Montage->bEnableAutoBlendOut)
			{
				const float AuthoredBlendOutTriggerTime =
					Montage->BlendOutTriggerTime >= 0.0f
					? Montage->BlendOutTriggerTime
					: Montage->BlendOut.GetBlendTime();
				Test.TestTrue(
					*FString::Printf(
						TEXT("%s attack-window end precedes normal auto blend-out"),
						*AttackContext),
					URpgGameplayAbility_BasicWeaponAttack::
						IsAttackWindowEndBeforeAutoBlendOutForTests(
							MontageLength,
							EndNotifies[0]->GetTriggerTime(),
							EffectivePlayRate,
							AuthoredBlendOutTriggerTime));
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgWeaponAttackAssetContractTest,
	"SurvivalRpg.Combat.WeaponAttackAssetContract",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgWeaponAttackAssetContractTest::RunTest(const FString& Parameters)
{
	using namespace RpgWeaponAttackAssetTests;

	TestFalse(
		TEXT("A 2x montage rejects a window end whose remaining real time overlaps auto blend-out"),
		URpgGameplayAbility_BasicWeaponAttack::
			IsAttackWindowEndBeforeAutoBlendOutForTests(1.0f, 0.6f, 2.0f, 0.25f));
	TestTrue(
		TEXT("A 0.5x montage accepts a window end with sufficient remaining real time"),
		URpgGameplayAbility_BasicWeaponAttack::
			IsAttackWindowEndBeforeAutoBlendOutForTests(1.0f, 0.8f, 0.5f, 0.25f));

	for (const FWeaponBlueprintContract& Contract : WeaponBlueprintContracts)
	{
		const URpgWeaponInstance* WeaponDefaults = LoadWeaponDefaults(
			*this,
			Contract);
		if (!WeaponDefaults)
		{
			continue;
		}

		TArray<FGameplayTag> AttackTags =
			WeaponDefaults->GetAttackDefinitionTags();
		AttackTags.Sort(
			[](const FGameplayTag& Left, const FGameplayTag& Right)
			{
				return Left.GetTagName().LexicalLess(Right.GetTagName());
			});
		TestTrue(
			*FString::Printf(
				TEXT("The %s CDO owns at least one attack definition"),
				Contract.Description),
			!AttackTags.IsEmpty());

		for (const FGameplayTag& AttackTag : AttackTags)
		{
			const FRpgWeaponAttackDefinition* AttackDefinition =
				WeaponDefaults->FindAttackDefinition(AttackTag);
			if (!TestNotNull(
					*FString::Printf(
						TEXT("The %s CDO resolves attack definition %s"),
						Contract.Description,
						*AttackTag.ToString()),
					AttackDefinition))
			{
				continue;
			}

			ValidateAttackDefinition(
				*this,
				Contract.Description,
				AttackTag,
				*AttackDefinition);
		}
	}

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgStarterEquipmentAssetContractTest,
	"SurvivalRpg.Combat.StarterEquipmentAssetContract",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgStarterEquipmentAssetContractTest::RunTest(const FString& Parameters)
{
	const UBlueprint* StarterBlueprint = LoadObject<UBlueprint>(
		nullptr,
		TEXT("/GF_Combat_Core/Equipment/Weapons/"
			"BP_BasicSwordShieldStarterLoadout.BP_BasicSwordShieldStarterLoadout"));
	const URpgStarterInventoryComponent* StarterDefaults =
		StarterBlueprint && StarterBlueprint->GeneratedClass
		? Cast<URpgStarterInventoryComponent>(
			StarterBlueprint->GeneratedClass->GetDefaultObject())
		: nullptr;
	if (!TestNotNull(TEXT("Sword and shield starter loadout uses the native inventory component"),
		StarterDefaults))
	{
		return false;
	}

	const FArrayProperty* EntriesProperty = FindFProperty<FArrayProperty>(
		StarterDefaults->GetClass(), TEXT("StarterInventory"));
	const FStructProperty* EntryProperty = EntriesProperty
		? CastField<FStructProperty>(EntriesProperty->Inner)
		: nullptr;
	if (!TestTrue(TEXT("Starter loadout serializes native equipment assignment entries"),
		EntryProperty && EntryProperty->Struct == FRpgStarterInventoryEntry::StaticStruct()))
	{
		return false;
	}
	const TArray<FRpgStarterInventoryEntry>& Entries =
		*EntriesProperty->ContainerPtrToValuePtr<TArray<FRpgStarterInventoryEntry>>(StarterDefaults);
	TestEqual(TEXT("Sword and shield starter loadout grants two items"), Entries.Num(), 2);

	const auto ValidateEquipmentAssignment = [this, &Entries](
		const TCHAR* Description, const TCHAR* DefinitionPath, ERpgEquipmentSlot ExpectedSlot)
	{
		const FSoftObjectPath ExpectedDefinition(DefinitionPath);
		const FRpgStarterInventoryEntry* Entry = Entries.FindByPredicate(
			[&ExpectedDefinition](const FRpgStarterInventoryEntry& Candidate)
			{
				return Candidate.ItemDefinition.ToSoftObjectPath() == ExpectedDefinition;
			});
		if (!TestNotNull(FString::Printf(TEXT("Starter loadout grants %s"), Description), Entry))
		{
			return;
		}
		TestTrue(FString::Printf(TEXT("Starter %s is assigned to equipment"), Description),
			Entry->bAssignToEquipment);
		TestEqual(FString::Printf(TEXT("Starter %s uses its intended hand role"), Description),
			Entry->EquipmentSlot, ExpectedSlot);
	};
	ValidateEquipmentAssignment(TEXT("sword"),
		TEXT("/GF_Combat_Core/Items/Weapons/ID_BasicSword.ID_BasicSword_C"),
		ERpgEquipmentSlot::MainHand);
	ValidateEquipmentAssignment(TEXT("shield"),
		TEXT("/GF_Combat_Core/Items/Weapons/ID_BasicShield.ID_BasicShield_C"),
		ERpgEquipmentSlot::OffHand);

	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
