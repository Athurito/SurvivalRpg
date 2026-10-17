#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "SurvivalRpg/Equipment/RpgWeaponInstance.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace RpgWeaponBlockPresentationTests
{
	UAnimMontage* MakeMontage(USkeleton* Skeleton)
	{
		UAnimMontage* Montage = NewObject<UAnimMontage>(GetTransientPackage(), NAME_None, RF_Transient);
		Montage->SetSkeleton(Skeleton);
		return Montage;
	}

	FRpgWeaponBlockPresentation MakePresentation(USkeleton* Skeleton, UAnimMontage* Montage)
	{
		FRpgWeaponBlockPresentation Presentation;
		Presentation.Skeleton = Skeleton;
		Presentation.BlockStartMontage = Montage;
		Presentation.BlockEndMontage = Montage;
		Presentation.BlockHitMontage = Montage;
		return Presentation;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgWeaponBlockPresentationResolveTest,
	"SurvivalRpg.Equipment.BlockPresentation.ResolvesBySkeleton",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgWeaponBlockPresentationResolveTest::RunTest(const FString& Parameters)
{
	using namespace RpgWeaponBlockPresentationTests;
	USkeleton* Default = NewObject<USkeleton>(GetTransientPackage(), NAME_None, RF_Transient);
	USkeleton* Variant = NewObject<USkeleton>(GetTransientPackage(), NAME_None, RF_Transient);
	UAnimMontage* DefaultMontage = MakeMontage(Default);
	UAnimMontage* VariantMontage = MakeMontage(Variant);
	UAnimMontage* ShadowedMontage = MakeMontage(Variant);

	FRpgWeaponBlockDefinition Definition;
	Definition.StaminaCost = 12.0f;
	Definition.BlockStartMontage = DefaultMontage;
	Definition.BlockLoopMontage = DefaultMontage;
	Definition.BlockEndMontage = DefaultMontage;
	Definition.GuardBreakMontage = DefaultMontage;
	Definition.SkeletonPresentations.Add(MakePresentation(Variant, VariantMontage));
	Definition.SkeletonPresentations.Add(MakePresentation(Variant, ShadowedMontage));

	for (const USkeleton* Unmatched : { static_cast<const USkeleton*>(nullptr), static_cast<const USkeleton*>(Default) })
	{
		const FRpgWeaponBlockDefinition Resolved = Definition.ResolveForSkeleton(Unmatched);
		TestTrue(TEXT("Without a matching presentation the default montages apply"),
			Resolved.BlockStartMontage == DefaultMontage && Resolved.BlockLoopMontage == DefaultMontage
			&& Resolved.BlockEndMontage == DefaultMontage && Resolved.GuardBreakMontage == DefaultMontage);
	}

	const FRpgWeaponBlockDefinition Resolved = Definition.ResolveForSkeleton(Variant);
	TestTrue(TEXT("The first presentation for the skeleton supplies its montages"),
		Resolved.BlockStartMontage == VariantMontage && Resolved.BlockEndMontage == VariantMontage
		&& Resolved.BlockHitMontage == VariantMontage);
	TestTrue(TEXT("A matching presentation replaces every presentation field, including unset ones"),
		Resolved.BlockLoopMontage == nullptr && Resolved.GuardBreakMontage == nullptr && !Resolved.BlockLocomotionLayer);
	TestEqual(TEXT("Gameplay tuning is not part of the presentation"), Resolved.StaminaCost, 12.0f);
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgWeaponBlockPresentationValidationTest,
	"SurvivalRpg.Equipment.BlockPresentation.ValidationRequiresUniqueMatchingSkeletons",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgWeaponBlockPresentationValidationTest::RunTest(const FString& Parameters)
{
	using namespace RpgWeaponBlockPresentationTests;
	USkeleton* First = NewObject<USkeleton>(GetTransientPackage(), NAME_None, RF_Transient);
	USkeleton* Second = NewObject<USkeleton>(GetTransientPackage(), NAME_None, RF_Transient);
	URpgWeaponInstance* Weapon = NewObject<URpgWeaponInstance>(GetTransientPackage(), NAME_None, RF_Transient);
	const FStructProperty* Property = FindFProperty<FStructProperty>(URpgWeaponInstance::StaticClass(), TEXT("BlockDefinition"));
	if (!TestNotNull(TEXT("Weapon instances expose the block definition to reflection"), Property)) return false;
	FRpgWeaponBlockDefinition& Definition = *Property->ContainerPtrToValuePtr<FRpgWeaponBlockDefinition>(Weapon);

	const auto Errors = [Weapon]()
	{
		FDataValidationContext Context;
		Weapon->IsDataValid(Context);
		return Context.GetNumErrors();
	};

	// Compare against the instance's own baseline so unrelated base-class checks do not matter.
	const uint32 Baseline = Errors();
	Definition.SkeletonPresentations = { MakePresentation(First, MakeMontage(First)), MakePresentation(Second, MakeMontage(Second)) };
	TestEqual(TEXT("Distinct skeletons with matching montages add no error"), Errors(), Baseline);

	Definition.SkeletonPresentations = { MakePresentation(nullptr, nullptr) };
	TestTrue(TEXT("A presentation without a skeleton is an error"), Errors() > Baseline);

	Definition.SkeletonPresentations = { MakePresentation(First, nullptr), MakePresentation(First, nullptr) };
	TestTrue(TEXT("A skeleton listed twice is an error"), Errors() > Baseline);

	Definition.SkeletonPresentations = { MakePresentation(First, MakeMontage(Second)) };
	TestTrue(TEXT("A montage on another skeleton than its presentation is an error"), Errors() > Baseline);
	return true;
}
#endif

#endif
