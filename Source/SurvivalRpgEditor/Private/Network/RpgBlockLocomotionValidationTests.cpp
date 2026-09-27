#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimInstance.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimLayerInterface.h"
#include "Chooser.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "ObjectChooser_Asset.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Equipment/RpgWeaponInstance.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

#include <limits>

namespace RpgBlockLocomotionValidationTests
{
	FRpgWeaponBlockDefinition& EditTransientBlock(URpgWeaponInstance* Weapon)
	{
		// Validation must also reject malformed serialized data that editor clamps or runtime setters cannot produce.
		// These objects belong only to this test; no loaded content or persistent CDO is modified.
		check(Weapon->GetOutermost() == GetTransientPackage());
		FStructProperty* Property = FindFProperty<FStructProperty>(URpgWeaponInstance::StaticClass(), TEXT("BlockDefinition"));
		check(Property);
		return *Property->ContainerPtrToValuePtr<FRpgWeaponBlockDefinition>(Weapon);
	}

	bool HasError(const FDataValidationContext& Context, const FString& Fragment)
	{
		for (const FDataValidationContext::FIssue& Issue : Context.GetIssues())
		{
			if (Issue.Severity == EMessageSeverity::Error && Issue.Message.ToString().Contains(Fragment)) return true;
		}
		return false;
	}

	UBlueprint* CreateTransientBlueprint(UClass* ParentClass)
	{
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(ParentClass, GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("BlockValidation")), BPTYPE_Normal,
			UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
		if (Blueprint) Blueprint->SetFlags(RF_Transient);
		return Blueprint;
	}

	constexpr const TCHAR* SharedProfilePath = TEXT("/Game/SurvivalRpg/Characters/GASP/Shared/RPG/Block/ABP_RpgBlockLocomotion.ABP_RpgBlockLocomotion");
	constexpr const TCHAR* LayerInterfacePath = TEXT("/Game/SurvivalRpg/Characters/GASP/Shared/RPG/Block/ALI_RpgBlockLocomotion.ALI_RpgBlockLocomotion_C");
	constexpr const TCHAR* SwordShieldProfilePath = TEXT("/GF_Combat_Core/Animations/BlockLocomotion/ABP_Block_SwordShield.ABP_Block_SwordShield");
	constexpr const TCHAR* VariantProfilePath = TEXT("/GF_Combat_Core/Animations/BlockLocomotion/ABP_Block_TestVariant.ABP_Block_TestVariant");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgBlockMovementLimitValidationTest,
	"SurvivalRpg.Combat.BlockLocomotion.Validation.MovementLimitAndLegacyDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgBlockMovementLimitValidationTest::RunTest(const FString& Parameters)
{
	using namespace RpgBlockLocomotionValidationTests;
	TStrongObjectPtr<URpgWeaponInstance> Weapon(NewObject<URpgWeaponInstance>(GetTransientPackage(), NAME_None, RF_Transient));
	FRpgWeaponBlockDefinition& Block = EditTransientBlock(Weapon.Get());
	for (float Limit : { 0.f, 162.f })
	{
		Block.MovementSpeedLimit = Limit;
		FDataValidationContext Context;
		TestTrue(TEXT("Null layer preserves legacy presentation with either zero or a finite positive limit"),
			Weapon->IsDataValid(Context) == EDataValidationResult::Valid);
		TestEqual(TEXT("Valid legacy tuning has no validation errors"), Context.GetNumErrors(), 0u);
	}
	for (float Limit : { -1.f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() })
	{
		Block.MovementSpeedLimit = Limit;
		FDataValidationContext Context;
		TestTrue(TEXT("Malformed persisted movement limits fail asset validation"),
			Weapon->IsDataValid(Context) == EDataValidationResult::Invalid);
		TestTrue(TEXT("The error identifies both the authored property and owning asset"),
			HasError(Context, TEXT("MovementSpeedLimit")) && HasError(Context, Weapon->GetPathName()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgBlockLayerClassValidationTest,
	"SurvivalRpg.Combat.BlockLocomotion.Validation.NativeAnimInstanceIsNotALinkedLayer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgBlockLayerClassValidationTest::RunTest(const FString& Parameters)
{
	using namespace RpgBlockLocomotionValidationTests;
	TStrongObjectPtr<URpgWeaponInstance> Weapon(NewObject<URpgWeaponInstance>(GetTransientPackage(), NAME_None, RF_Transient));
	FRpgWeaponBlockDefinition& Block = EditTransientBlock(Weapon.Get());
	Block.BlockLocomotionLayer = UAnimInstance::StaticClass();
	FDataValidationContext InvalidContext;
	TestTrue(TEXT("A native animation class cannot silently suppress the legacy loop without implementing a layer"),
		Weapon->IsDataValid(InvalidContext) == EDataValidationResult::Invalid);
	TestTrue(TEXT("The error identifies the invalid layer class and field"),
		HasError(InvalidContext, UAnimInstance::StaticClass()->GetPathName()) && HasError(InvalidContext, TEXT("BlockLocomotionLayer")));
	Block.BlockLocomotionLayer = nullptr;
	FDataValidationContext RestoredContext;
	TestTrue(TEXT("Clearing the optional layer restores a valid legacy definition"),
		Weapon->IsDataValid(RestoredContext) == EDataValidationResult::Valid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgEquipmentBlockValidationPropagationTest,
	"SurvivalRpg.Combat.BlockLocomotion.Validation.EquipmentReportsReferencedWeaponErrors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgEquipmentBlockValidationPropagationTest::RunTest(const FString& Parameters)
{
	using namespace RpgBlockLocomotionValidationTests;
	TStrongObjectPtr<UBlueprint> WeaponBlueprint(CreateTransientBlueprint(URpgWeaponInstance::StaticClass()));
	TStrongObjectPtr<UBlueprint> EquipmentBlueprint(CreateTransientBlueprint(URpgEquipmentDefinition::StaticClass()));
	if (!TestNotNull(TEXT("Transient weapon blueprint exists"), WeaponBlueprint.Get())
		|| !TestNotNull(TEXT("Transient equipment blueprint exists"), EquipmentBlueprint.Get())
		|| !TestNotNull(TEXT("Weapon has a real generated class"), WeaponBlueprint->GeneratedClass.Get())
		|| !TestNotNull(TEXT("Equipment has a real generated class"), EquipmentBlueprint->GeneratedClass.Get())) return false;
	URpgWeaponInstance* Weapon = WeaponBlueprint->GeneratedClass->GetDefaultObject<URpgWeaponInstance>();
	URpgEquipmentDefinition* Equipment = EquipmentBlueprint->GeneratedClass->GetDefaultObject<URpgEquipmentDefinition>();
	Equipment->InstanceType = Weapon->GetClass();
	FRpgWeaponBlockDefinition& Block = EditTransientBlock(Weapon);
	Block.MovementSpeedLimit = -10.f;
	FDataValidationContext InvalidContext;
	TestTrue(TEXT("Normal equipment Blueprint validation reaches referenced weapon tuning"),
		EquipmentBlueprint->IsDataValid(InvalidContext) == EDataValidationResult::Invalid);
	TestTrue(TEXT("Nested validation reports the actual malformed weapon and property"),
		HasError(InvalidContext, Weapon->GetPathName()) && HasError(InvalidContext, TEXT("MovementSpeedLimit")));
	Block.MovementSpeedLimit = 0.f;
	FDataValidationContext ValidContext;
	TestTrue(TEXT("Fixing the transient referenced definition repairs the same asset-validation route"),
		EquipmentBlueprint->IsDataValid(ValidContext) == EDataValidationResult::Valid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgAuthoredBlockSetsValidationTest,
	"SurvivalRpg.Combat.BlockLocomotion.Validation.AuthoredSetsInheritSharedSelectionContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgAuthoredBlockSetsValidationTest::RunTest(const FString& Parameters)
{
	using namespace RpgBlockLocomotionValidationTests;
	const UAnimBlueprint* Shared = LoadObject<UAnimBlueprint>(nullptr, SharedProfilePath);
	UClass* LayerInterface = LoadClass<UAnimLayerInterface>(nullptr, LayerInterfacePath);
	if (!TestNotNull(TEXT("Shared block profile exists"), Shared)
		|| !TestNotNull(TEXT("Shared block profile has a generated class"), Shared->GeneratedClass.Get())
		|| !TestNotNull(TEXT("The real animation layer interface exists"), LayerInterface)) return false;
	UFunction* SharedValidator = Shared->GeneratedClass->FindFunctionByName(TEXT("ValidateAnimationSet"));
	if (!TestNotNull(TEXT("Shared profile implements the designer-owned validation hook"), SharedValidator)
		|| !TestTrue(TEXT("The validation hook is authored on the shared profile"), SharedValidator->GetOuterUClass() == Shared->GeneratedClass.Get())) return false;
	for (const TCHAR* ProfilePath : { SwordShieldProfilePath, VariantProfilePath })
	{
		const UAnimBlueprint* Profile = LoadObject<UAnimBlueprint>(nullptr, ProfilePath);
		if (!TestNotNull(FString::Printf(TEXT("Authored set exists: %s"), ProfilePath), Profile)
			|| !TestNotNull(TEXT("Authored set has a generated class"), Profile->GeneratedClass.Get())) return false;
		UClass* ProfileClass = Profile->GeneratedClass.Get();
		TestTrue(TEXT("The concrete set inherits its animation graph implementation from the shared root"),
			UAnimBlueprint::FindRootAnimBlueprint(Profile) == Shared && ProfileClass->IsChildOf(Shared->GeneratedClass));
		TestFalse(TEXT("The data set can be instantiated"), ProfileClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists));
		TestTrue(TEXT("The data set implements the actual block layer interface"), ProfileClass->ImplementsInterface(LayerInterface));
		TestTrue(TEXT("The shared validation graph is inherited, not replaced by a per-set implementation"),
			ProfileClass->FindFunctionByName(TEXT("ValidateAnimationSet")) == SharedValidator);
		const IAnimClassInterface* AnimClass = IAnimClassInterface::GetFromClass(ProfileClass);
		TestTrue(TEXT("The compiled block layer retains its callable implementation"), AnimClass
			&& AnimClass->GetAnimBlueprintFunctions().ContainsByPredicate([](const FAnimBlueprintFunction& Function)
			{
				return Function.Name == TEXT("BlockLocomotion") && Function.bImplemented;
			}));
		FDataValidationContext Context;
		TestTrue(FString::Printf(TEXT("Every designer-declared selection case validates for %s"), ProfilePath),
			Profile->IsDataValid(Context) == EDataValidationResult::Valid);
		TestEqual(TEXT("The complete authored set reports no validation errors"), Context.GetNumErrors(), 0u);
		for (const FDataValidationContext::FIssue& Issue : Context.GetIssues())
		{
			if (Issue.Severity == EMessageSeverity::Error) AddError(Issue.Message.ToString());
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCopiedBlockSetMissingSelectionValidationTest,
	"SurvivalRpg.Combat.BlockLocomotion.Validation.CopiedSetRejectsMissingChooserAndRequiredResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCopiedBlockSetMissingSelectionValidationTest::RunTest(const FString& Parameters)
{
	using namespace RpgBlockLocomotionValidationTests;
	const UAnimBlueprint* Profile = LoadObject<UAnimBlueprint>(nullptr, SwordShieldProfilePath);
	if (!TestNotNull(TEXT("Sword and shield data set exists"), Profile)
		|| !TestNotNull(TEXT("Sword and shield data set is compiled"), Profile->GeneratedClass.Get())) return false;
	const URpgAnimInstance* Defaults = Cast<URpgAnimInstance>(Profile->GeneratedClass->GetDefaultObject());
	if (!TestNotNull(TEXT("The concrete set uses the RPG validation seam"), Defaults)) return false;
	FObjectPropertyBase* ChooserProperty = FindFProperty<FObjectPropertyBase>(Profile->GeneratedClass, TEXT("PackChooser"));
	if (!TestNotNull(TEXT("The inherited chooser data field exists"), ChooserProperty)) return false;
	UChooserTable* OriginalChooser = Cast<UChooserTable>(ChooserProperty->GetObjectPropertyValue_InContainer(Defaults));
	if (!TestNotNull(TEXT("The concrete set supplies its chooser"), OriginalChooser)) return false;
	const bool bProfileWasDirty = Profile->GetOutermost()->IsDirty();
	const bool bChooserWasDirty = OriginalChooser->GetOutermost()->IsDirty();
	// UAnimInstance is Within=SkeletalMeshComponent. Only this unregistered transient instance is edited;
	// the public validator must copy its actual authored inputs, rather than replace them with CDO defaults.
	TStrongObjectPtr<USkeletalMeshComponent> Mesh(NewObject<USkeletalMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient));
	TStrongObjectPtr<URpgAnimInstance> Copy(NewObject<URpgAnimInstance>(Mesh.Get(), Defaults->GetClass(), NAME_None, RF_Transient,
		const_cast<URpgAnimInstance*>(Defaults)));
	FDataValidationContext BaselineContext;
	if (!TestTrue(TEXT("The untouched transient copy validates"), Copy->IsDataValid(BaselineContext) == EDataValidationResult::Valid)) return false;
	ChooserProperty->SetObjectPropertyValue_InContainer(Copy.Get(), nullptr);
	FDataValidationContext MissingChooserContext;
	TestTrue(TEXT("Missing chooser is rejected through the real inherited Blueprint validation hook"),
		Copy->IsDataValid(MissingChooserContext) == EDataValidationResult::Invalid && MissingChooserContext.GetNumErrors() > 0);
	ChooserProperty->SetObjectPropertyValue_InContainer(Copy.Get(), OriginalChooser);
	FDataValidationContext RestoredChooserContext;
	TestTrue(TEXT("Restoring the same chooser repairs the copied set"), Copy->IsDataValid(RestoredChooserContext) == EDataValidationResult::Valid);

	TStrongObjectPtr<UChooserTable> ChooserCopy(DuplicateObject<UChooserTable>(OriginalChooser, GetTransientPackage()));
	ChooserCopy->SetFlags(RF_Transient);
	ChooserCopy->Compile(true);
	ChooserProperty->SetObjectPropertyValue_InContainer(Copy.Get(), ChooserCopy.Get());
	FDataValidationContext CopiedChooserContext;
	if (!TestTrue(TEXT("The intact copied chooser retains working property bindings"), Copy->IsDataValid(CopiedChooserContext) == EDataValidationResult::Valid)) return false;
	if (!TestTrue(TEXT("The chooser has an authored first selection result"), !ChooserCopy->ResultsStructs.IsEmpty())) return false;
	FAssetChooser* Result = ChooserCopy->ResultsStructs[0].GetMutablePtr<FAssetChooser>();
	const FAssetChooser* OriginalResult = OriginalChooser->ResultsStructs[0].GetPtr<FAssetChooser>();
	if (!TestNotNull(TEXT("The required result is a typed asset selection"), Result)
		|| !TestNotNull(TEXT("The original result has the same typed contract"), OriginalResult)
		|| !TestNotNull(TEXT("The original required animation is assigned"), Result->Asset.Get())) return false;
	UObject* OriginalAsset = Result->Asset.Get();
	Result->Asset = nullptr;
	ChooserProperty->SetObjectPropertyValue_InContainer(Copy.Get(), ChooserCopy.Get());
	FDataValidationContext MissingResultContext;
	TestTrue(TEXT("A missing required animation is rejected even though the chooser and output columns exist"),
		Copy->IsDataValid(MissingResultContext) == EDataValidationResult::Invalid && MissingResultContext.GetNumErrors() > 0);
	Result->Asset = OriginalAsset;
	FDataValidationContext RestoredResultContext;
	TestTrue(TEXT("Restoring the copied result repairs the same selection contract"), Copy->IsDataValid(RestoredResultContext) == EDataValidationResult::Valid);
	TestTrue(TEXT("The persistent profile still references its original chooser"), ChooserProperty->GetObjectPropertyValue_InContainer(Defaults) == OriginalChooser);
	TestTrue(TEXT("The persistent chooser still contains its original animation"), OriginalResult->Asset == OriginalAsset);
	TestEqual(TEXT("Validation did not dirty the authored profile"), Profile->GetOutermost()->IsDirty(), bProfileWasDirty);
	TestEqual(TEXT("Validation did not dirty the authored chooser"), OriginalChooser->GetOutermost()->IsDirty(), bChooserWasDirty);
	TestFalse(TEXT("The isolated validation mesh never enters an animation or world lifecycle"), Mesh->IsRegistered());
	return true;
}

#endif
