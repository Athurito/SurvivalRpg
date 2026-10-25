#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimMontage.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "SurvivalRpg/Animation/AnimNotifyState_RpgComboWindow.h"
#include "SurvivalRpg/Equipment/RpgWeaponInstance.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace RpgWeaponComboValidationTests
{
	// Real combo montages, used only as known authored window layouts: Step1 chains, Step4 has no Combo Window.
	constexpr TCHAR ChainingMontagePath[] = TEXT("/GF_Combat_Core/Animations/Combo/SwordShield/AM_SwordShieldCombo01_Step1.AM_SwordShieldCombo01_Step1");
	constexpr TCHAR FinisherMontagePath[] = TEXT("/GF_Combat_Core/Animations/Combo/SwordShield/AM_SwordShieldCombo01_Step4.AM_SwordShieldCombo01_Step4");
	constexpr TCHAR BasicSwordPath[] = TEXT("/GF_Combat_Core/Equipment/Weapons/BP_WeaponInstance_BasicSword.BP_WeaponInstance_BasicSword_C");

	FGameplayTag Tag(const TCHAR* Name) { return FGameplayTag::RequestGameplayTag(Name); }

	TMap<FGameplayTag, FRpgWeaponAttackDefinition>* AttackDefinitions(URpgWeaponInstance* Weapon)
	{
		const FMapProperty* Property = FindFProperty<FMapProperty>(URpgWeaponInstance::StaticClass(), TEXT("AttackDefinitions"));
		return Property && Weapon ? Property->ContainerPtrToValuePtr<TMap<FGameplayTag, FRpgWeaponAttackDefinition>>(Weapon) : nullptr;
	}

	FRpgWeaponAttackDefinition Attack(UAnimMontage* Montage, FGameplayTag Next)
	{
		FRpgWeaponAttackDefinition Definition;
		Definition.Montage = Montage;
		Definition.ComboNextAttack = Next;
		return Definition;
	}

	bool HasError(const FDataValidationContext& Context, const FString& Fragment)
	{
		for (const FDataValidationContext::FIssue& Issue : Context.GetIssues())
		{
			if (Issue.Severity == EMessageSeverity::Error && Issue.Message.ToString().Contains(Fragment)) return true;
		}
		return false;
	}

	/** A copy of Montage whose Combo Window spans Start..End; the source asset stays untouched. */
	UAnimMontage* WithComboWindow(UAnimMontage* Montage, float Start, float End)
	{
		UAnimMontage* Copy = DuplicateObject<UAnimMontage>(Montage, GetTransientPackage());
		for (FAnimNotifyEvent& Event : Copy->Notifies)
		{
			if (Event.NotifyStateClass && Event.NotifyStateClass->IsA<UAnimNotifyState_RpgComboWindow>())
			{
				Event.SetTime(Start);
				Event.SetDuration(End - Start);
			}
		}
		return Copy;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgWeaponComboValidationTest,
	"SurvivalRpg.Combat.WeaponCombo.Validation.ChainsNeedTargetsWindowsAndAnEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgWeaponComboValidationTest::RunTest(const FString& Parameters)
{
	using namespace RpgWeaponComboValidationTests;
	UAnimMontage* Chaining = LoadObject<UAnimMontage>(nullptr, ChainingMontagePath);
	UAnimMontage* Finisher = LoadObject<UAnimMontage>(nullptr, FinisherMontagePath);
	TStrongObjectPtr<UBlueprint> Blueprint(FKismetEditorUtilities::CreateBlueprint(URpgWeaponInstance::StaticClass(), GetTransientPackage(),
		MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("ComboValidation")), BPTYPE_Normal,
		UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass()));
	if (!TestNotNull(TEXT("The chaining montage loads"), Chaining) || !TestNotNull(TEXT("The finisher montage loads"), Finisher)
		|| !TestNotNull(TEXT("A transient weapon Blueprint exists"), Blueprint.Get()) || !TestNotNull(TEXT("It has a generated class"), Blueprint->GeneratedClass.Get()))
	{
		return false;
	}
	float WindowStart = 0.0f;
	float WindowEnd = 0.0f;
	FString Failure;
	TestTrue(TEXT("The chaining montage has one Combo Window"), UAnimNotifyState_RpgComboWindow::FindComboWindow(Chaining, WindowStart, WindowEnd, Failure));
	TestFalse(TEXT("The finisher montage has no Combo Window"), UAnimNotifyState_RpgComboWindow::FindComboWindow(Finisher, WindowStart, WindowEnd, Failure));

	// The transient Blueprint's CDO belongs only to this test; no loaded content is modified.
	URpgWeaponInstance* Weapon = Blueprint->GeneratedClass->GetDefaultObject<URpgWeaponInstance>();
	TMap<FGameplayTag, FRpgWeaponAttackDefinition>* Attacks = AttackDefinitions(Weapon);
	if (!TestNotNull(TEXT("The weapon exposes its attack definitions"), Attacks))
	{
		return false;
	}
	const FGameplayTag First = Tag(TEXT("Weapon.Attack.Primary"));
	const FGameplayTag Second = Tag(TEXT("Weapon.Attack.Primary.Combo2"));
	const FGameplayTag Missing = Tag(TEXT("Weapon.Attack.Primary.Combo3"));
	auto Validate = [Blueprint = Blueprint.Get()](FDataValidationContext& Context) { return Blueprint->IsDataValid(Context); };

	*Attacks = { { First, Attack(Chaining, Second) }, { Second, Attack(Finisher, FGameplayTag()) } };
	FDataValidationContext ValidContext;
	TestTrue(TEXT("A chain into an ending attack is valid"), Validate(ValidContext) == EDataValidationResult::Valid);

	*Attacks = { { First, Attack(Chaining, Missing) } };
	FDataValidationContext MissingContext;
	TestTrue(TEXT("A chain into an attack the weapon lacks is rejected"),
		Validate(MissingContext) == EDataValidationResult::Invalid && HasError(MissingContext, Missing.ToString()));

	*Attacks = { { First, Attack(Chaining, Second) }, { Second, Attack(Chaining, First) } };
	FDataValidationContext CycleContext;
	TestTrue(TEXT("A chain that never ends is rejected"),
		Validate(CycleContext) == EDataValidationResult::Invalid && HasError(CycleContext, TEXT("never ends")));

	*Attacks = { { First, Attack(Finisher, Second) }, { Second, Attack(Finisher, FGameplayTag()) } };
	FDataValidationContext WindowContext;
	TestTrue(TEXT("A chaining attack without a Combo Window is rejected"),
		Validate(WindowContext) == EDataValidationResult::Invalid && HasError(WindowContext, TEXT("Combo Window")));

	*Attacks = { { First, Attack(WithComboWindow(Chaining, 0.05f, 0.5f), Second) }, { Second, Attack(Finisher, FGameplayTag()) } };
	FDataValidationContext EarlyContext;
	TestTrue(TEXT("A Combo Window that opens inside the attack window is rejected"),
		Validate(EarlyContext) == EDataValidationResult::Invalid && HasError(EarlyContext, TEXT("before the attack window ends")));

	*Attacks = { { First, Attack(WithComboWindow(Chaining, 0.4f, Chaining->GetPlayLength() - 0.05f), Second) }, { Second, Attack(Finisher, FGameplayTag()) } };
	FDataValidationContext LateContext;
	TestTrue(TEXT("A Combo Window that ends inside the auto blend-out is rejected"),
		Validate(LateContext) == EDataValidationResult::Invalid && HasError(LateContext, TEXT("blend-out")));

	const UClass* SwordClass = LoadClass<URpgWeaponInstance>(nullptr, BasicSwordPath);
	const URpgWeaponInstance* Sword = SwordClass ? SwordClass->GetDefaultObject<URpgWeaponInstance>() : nullptr;
	if (TestNotNull(TEXT("The Basic Sword weapon instance loads"), Sword))
	{
		const FRpgWeaponAttackDefinition* Entry = Sword->FindAttackDefinition(First);
		TestTrue(TEXT("The Basic Sword's primary attack starts a combo"), Entry && Entry->ComboNextAttack.IsValid());
		FDataValidationContext SwordContext;
		TestTrue(TEXT("The Basic Sword's combo data is valid"), Sword->IsDataValid(SwordContext) == EDataValidationResult::Valid);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
