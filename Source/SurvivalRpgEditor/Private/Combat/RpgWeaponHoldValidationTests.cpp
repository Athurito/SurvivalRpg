#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace RpgWeaponHoldValidationTests
{
	constexpr TCHAR MannySkeletonPath[] = TEXT("/Game/SurvivalRpg/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin");
	constexpr TCHAR UefnSkeletonPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin.SK_UEFN_Mannequin");
	// Concrete hold layer on the Manny skeleton; only used here as a known-valid compiled layer class.
	constexpr TCHAR MannyHoldLayerPath[] = TEXT("/GF_Combat_Core/Animations/WeaponHold/ABP_Hold_SwordShield_MainHand_Manny.ABP_Hold_SwordShield_MainHand_Manny_C");

	bool HasError(const FDataValidationContext& Context, const FString& Fragment)
	{
		for (const FDataValidationContext::FIssue& Issue : Context.GetIssues())
		{
			if (Issue.Severity == EMessageSeverity::Error && Issue.Message.ToString().Contains(Fragment)) return true;
		}
		return false;
	}

	UBlueprint* CreateTransientEquipmentBlueprint()
	{
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(URpgEquipmentDefinition::StaticClass(), GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("HoldValidation")), BPTYPE_Normal,
			UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
		if (Blueprint) Blueprint->SetFlags(RF_Transient);
		return Blueprint;
	}

	EDataValidationResult Validate(UBlueprint* Blueprint, FDataValidationContext& Context)
	{
		return Blueprint->IsDataValid(Context);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgWeaponHoldPresentationValidationTest,
	"SurvivalRpg.Combat.WeaponHold.Validation.PresentationResolvesPerSkeletonAndHand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgWeaponHoldPresentationValidationTest::RunTest(const FString& Parameters)
{
	using namespace RpgWeaponHoldValidationTests;
	USkeleton* Manny = LoadObject<USkeleton>(nullptr, MannySkeletonPath);
	USkeleton* Uefn = LoadObject<USkeleton>(nullptr, UefnSkeletonPath);
	UClass* HoldLayer = LoadClass<UAnimInstance>(nullptr, MannyHoldLayerPath);
	TStrongObjectPtr<UBlueprint> Blueprint(CreateTransientEquipmentBlueprint());
	if (!TestNotNull(TEXT("The Manny skeleton loads"), Manny) || !TestNotNull(TEXT("The UEFN skeleton loads"), Uefn)
		|| !TestNotNull(TEXT("A compiled Manny hold layer loads"), HoldLayer)
		|| !TestNotNull(TEXT("A transient equipment Blueprint exists"), Blueprint.Get())
		|| !TestNotNull(TEXT("It has a generated class"), Blueprint->GeneratedClass.Get()))
	{
		return false;
	}
	// The transient Blueprint's CDO belongs only to this test; no loaded content is modified.
	URpgEquipmentDefinition* Definition = Blueprint->GeneratedClass->GetDefaultObject<URpgEquipmentDefinition>();
	FRpgEquipmentHoldPresentation Presentation;
	Presentation.Skeleton = Manny;
	Presentation.MainHandLayer = HoldLayer;
	Definition->HoldPresentations = { Presentation };

	FDataValidationContext ValidContext;
	TestTrue(TEXT("A compiled layer that targets its presentation skeleton is valid"),
		Validate(Blueprint.Get(), ValidContext) == EDataValidationResult::Valid);
	TestTrue(TEXT("The main hand resolves the layer on its skeleton"),
		Definition->ResolveHoldLayer(Manny, ERpgEquipmentSlot::MainHand) == HoldLayer);
	TestNull(TEXT("An unset off-hand layer resolves nothing"), Definition->ResolveHoldLayer(Manny, ERpgEquipmentSlot::OffHand).Get());
	TestNull(TEXT("Another skeleton resolves nothing"), Definition->ResolveHoldLayer(Uefn, ERpgEquipmentSlot::MainHand).Get());
	TestNull(TEXT("A non-hand slot resolves nothing"), Definition->ResolveHoldLayer(Manny, ERpgEquipmentSlot::Head).Get());
	TestNull(TEXT("A missing skeleton resolves nothing"), Definition->ResolveHoldLayer(nullptr, ERpgEquipmentSlot::MainHand).Get());

	Definition->HoldPresentations[0].MainHandLayer = UAnimInstance::StaticClass();
	FDataValidationContext NativeContext;
	TestTrue(TEXT("A native AnimInstance is not a linked hold layer"),
		Validate(Blueprint.Get(), NativeContext) == EDataValidationResult::Invalid
		&& HasError(NativeContext, TEXT("HoldPresentations")) && HasError(NativeContext, UAnimInstance::StaticClass()->GetPathName()));

	Definition->HoldPresentations[0].MainHandLayer = HoldLayer;
	Definition->HoldPresentations[0].Skeleton = Uefn;
	FDataValidationContext SkeletonContext;
	TestTrue(TEXT("A layer for another skeleton is rejected"),
		Validate(Blueprint.Get(), SkeletonContext) == EDataValidationResult::Invalid && HasError(SkeletonContext, Uefn->GetPathName()));

	Definition->HoldPresentations[0].Skeleton = nullptr;
	FDataValidationContext MissingContext;
	TestTrue(TEXT("A presentation without a skeleton is rejected"),
		Validate(Blueprint.Get(), MissingContext) == EDataValidationResult::Invalid && HasError(MissingContext, TEXT("HoldPresentations")));

	Definition->HoldPresentations[0].Skeleton = Manny;
	const FRpgEquipmentHoldPresentation Repeated = Definition->HoldPresentations[0];
	Definition->HoldPresentations.Add(Repeated);
	FDataValidationContext DuplicateContext;
	TestTrue(TEXT("A skeleton listed twice is rejected"),
		Validate(Blueprint.Get(), DuplicateContext) == EDataValidationResult::Invalid && HasError(DuplicateContext, TEXT("HoldPresentations[1]")));

	Definition->HoldPresentations.SetNum(1);
	FDataValidationContext RestoredContext;
	TestTrue(TEXT("Removing the duplicate restores a valid definition"),
		Validate(Blueprint.Get(), RestoredContext) == EDataValidationResult::Valid);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
