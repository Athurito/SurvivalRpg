#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "UObject/Package.h"
#include "RpgCharacterPartsAutomationTestTypes.h"
#include "RpgCosmeticAnimationTypes.h"
#include "RpgPawnComponent_CharacterParts.h"
#include "SurvivalRpg/Equipment/RpgEquipmentInstance.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace RpgCharacterPartsTests
{
	/** Standalone (non-dedicated) world in which parts are spawned locally, as on a listen server or client. */
	class FScopedPartsWorld
	{
	public:
		FScopedPartsWorld()
		{
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			if (GameInstance)
			{
				GameInstance->AddToRoot();
				GameInstance->InitializeStandalone();
				World = GameInstance->GetWorld();
			}
		}

		~FScopedPartsWorld()
		{
			UWorld* WorldToDestroy = World;
			if (GameInstance)
			{
				GameInstance->Shutdown();
			}
			if (WorldToDestroy)
			{
				GEngine->DestroyWorldContext(WorldToDestroy);
				WorldToDestroy->DestroyWorld(false);
			}
			if (GameInstance)
			{
				GameInstance->RemoveFromRoot();
			}
		}

		UWorld* GetWorld() const { return World; }

	private:
		TObjectPtr<UGameInstance> GameInstance = nullptr;
		TObjectPtr<UWorld> World = nullptr;
	};

	FGameplayTag Tag(const TCHAR* Name)
	{
		return FGameplayTag::RequestGameplayTag(Name);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCharacterPartsAddRemoveTest,
	"SurvivalRpg.Cosmetics.CharacterParts.AddRemoveSpawnsAttachedTaggedParts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCharacterPartsAddRemoveTest::RunTest(const FString& Parameters)
{
	using namespace RpgCharacterPartsTests;
	FScopedPartsWorld TestWorld;
	ARpgCharacterPartsTestCharacter* Character = TestWorld.GetWorld() ? TestWorld.GetWorld()->SpawnActor<ARpgCharacterPartsTestCharacter>() : nullptr;
	if (!TestNotNull(TEXT("Character fixture spawns"), Character)) return false;
	URpgPawnComponent_CharacterParts* Parts = Character->GetCharacterParts();

	FRpgCharacterPart BodyPart;
	BodyPart.PartClass = ARpgCharacterPartsTestBodyPart::StaticClass();
	const FRpgCharacterPartHandle BodyHandle = Parts->AddCharacterPart(BodyPart);
	TestTrue(TEXT("The authority receives a valid handle"), BodyHandle.IsValid());

	TArray<AActor*> Actors = Parts->GetCharacterPartActors();
	if (!TestEqual(TEXT("A non-dedicated world spawns the part locally"), Actors.Num(), 1)) return false;
	AActor* BodyActor = Actors[0];
	const UChildActorComponent* PartComponent = BodyActor->GetParentComponent();
	TestTrue(TEXT("The part's child actor component attaches to the gameplay mesh"),
		PartComponent != nullptr && PartComponent->GetAttachParent() == Character->GetMesh());
	TestFalse(TEXT("Parts default to no collision"), BodyActor->GetActorEnableCollision());
	TestTrue(TEXT("Collected tags include the part's tags"), Parts->GetCombinedTags(FGameplayTag()).HasTagExact(Tag(TEXT("Cosmetic.Body"))));

	FRpgCharacterPart ArmorPart;
	ArmorPart.PartClass = ARpgCharacterPartsTestArmorPart::StaticClass();
	const FRpgCharacterPartHandle ArmorHandle = Parts->AddCharacterPart(ArmorPart);
	TestEqual(TEXT("A second part spawns next to the first"), Parts->GetCharacterPartActors().Num(), 2);
	const FGameplayTagContainer CoverTags = Parts->GetCombinedTags(Tag(TEXT("Cosmetic.Cover")));
	TestTrue(TEXT("The prefix filter keeps cover tags"), CoverTags.HasTagExact(Tag(TEXT("Cosmetic.Cover.UpperBody"))));
	TestFalse(TEXT("The prefix filter drops other tags"), CoverTags.HasTagExact(Tag(TEXT("Cosmetic.Body"))));

	Parts->RemoveCharacterPart(ArmorHandle);
	TestEqual(TEXT("Removing a handle removes exactly that part"), Parts->GetCharacterPartActors().Num(), 1);
	TestFalse(TEXT("Its tags are gone"), Parts->GetCombinedTags(FGameplayTag()).HasTagExact(Tag(TEXT("Cosmetic.Cover.UpperBody"))));

	Parts->RemoveCharacterPart(ArmorHandle);
	TestEqual(TEXT("Removing a stale handle changes nothing"), Parts->GetCharacterPartActors().Num(), 1);

	Parts->RemoveAllCharacterParts();
	TestEqual(TEXT("RemoveAll clears every part"), Parts->GetCharacterPartActors().Num(), 0);
	TestFalse(TEXT("The removed body actor is destroyed"), IsValid(BodyActor));
	TestTrue(TEXT("No tags remain"), Parts->GetCombinedTags(FGameplayTag()).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCharacterPartsBodyStyleTest,
	"SurvivalRpg.Cosmetics.CharacterParts.BodyStyleSelectsFirstMatchingRule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCharacterPartsBodyStyleTest::RunTest(const FString& Parameters)
{
	using namespace RpgCharacterPartsTests;
	USkeletalMesh* Default = NewObject<USkeletalMesh>(GetTransientPackage(), NAME_None, RF_Transient);
	USkeletalMesh* Hidden = NewObject<USkeletalMesh>(GetTransientPackage(), NAME_None, RF_Transient);
	USkeletalMesh* Shadowed = NewObject<USkeletalMesh>(GetTransientPackage(), NAME_None, RF_Transient);

	FRpgAnimBodyStyleSelectionSet Set;
	TestNull(TEXT("An empty selection returns no mesh, so the gameplay mesh is left alone"), Set.SelectBestBodyStyle(FGameplayTagContainer()));

	Set.DefaultMesh = Default;
	FRpgAnimBodyStyleSelectionEntry& Rule = Set.MeshRules.AddDefaulted_GetRef();
	Rule.Mesh = Hidden;
	Rule.RequiredTags.AddTag(Tag(TEXT("Cosmetic.Body")));
	FRpgAnimBodyStyleSelectionEntry& Later = Set.MeshRules.AddDefaulted_GetRef();
	Later.Mesh = Shadowed;
	Later.RequiredTags.AddTag(Tag(TEXT("Cosmetic.Body")));

	TestEqual(TEXT("Without matching tags the default mesh applies"), Set.SelectBestBodyStyle(FGameplayTagContainer()), Default);
	FGameplayTagContainer BodyTags;
	BodyTags.AddTag(Tag(TEXT("Cosmetic.Body")));
	BodyTags.AddTag(Tag(TEXT("Cosmetic.Cover.Head")));
	TestEqual(TEXT("The first rule whose tags are all present wins"), Set.SelectBestBodyStyle(BodyTags), Hidden);

	// The component must not clear the gameplay mesh when no body style is configured.
	FScopedPartsWorld TestWorld;
	ARpgCharacterPartsTestCharacter* Character = TestWorld.GetWorld() ? TestWorld.GetWorld()->SpawnActor<ARpgCharacterPartsTestCharacter>() : nullptr;
	if (!TestNotNull(TEXT("Character fixture spawns"), Character)) return false;
	USkeletalMesh* MeshBefore = Character->GetMesh()->GetSkeletalMeshAsset();
	FRpgCharacterPart BodyPart;
	BodyPart.PartClass = ARpgCharacterPartsTestBodyPart::StaticClass();
	Character->GetCharacterParts()->AddCharacterPart(BodyPart);
	TestEqual(TEXT("An unconfigured body style leaves the gameplay mesh untouched"), Character->GetMesh()->GetSkeletalMeshAsset(), MeshBefore);
	Character->GetCharacterParts()->RemoveAllCharacterParts();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCharacterPartsEquipmentTest,
	"SurvivalRpg.Cosmetics.CharacterParts.EquipmentAddsAndRemovesItsParts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCharacterPartsEquipmentTest::RunTest(const FString& Parameters)
{
	using namespace RpgCharacterPartsTests;
	FScopedPartsWorld TestWorld;
	ARpgCharacterPartsTestCharacter* Character = TestWorld.GetWorld() ? TestWorld.GetWorld()->SpawnActor<ARpgCharacterPartsTestCharacter>() : nullptr;
	if (!TestNotNull(TEXT("Character fixture spawns"), Character)) return false;
	URpgPawnComponent_CharacterParts* Parts = Character->GetCharacterParts();
	URpgEquipmentManagerComponent* Equipment = Character->GetEquipmentManager();

	URpgEquipmentInstance* Chest = Equipment->EquipItemInSlot(URpgCharacterPartsTestChestDefinition::StaticClass(), ERpgEquipmentSlot::Chest);
	if (!TestNotNull(TEXT("The chest equips"), Chest)) return false;
	TestEqual(TEXT("Equipping adds the definition's part"), Parts->GetCharacterPartActors().Num(), 1);
	TestTrue(TEXT("The worn part carries its cover tag"), Parts->GetCombinedTags(FGameplayTag()).HasTagExact(Tag(TEXT("Cosmetic.Cover.UpperBody"))));

	FRpgCharacterPart BodyPart;
	BodyPart.PartClass = ARpgCharacterPartsTestBodyPart::StaticClass();
	Parts->AddCharacterPart(BodyPart);

	Equipment->UnequipItem(Chest);
	TArray<AActor*> Remaining = Parts->GetCharacterPartActors();
	TestEqual(TEXT("Unequipping removes only the equipment's parts"), Remaining.Num(), 1);
	TestTrue(TEXT("The other part stays"), Remaining.Num() == 1 && Remaining[0]->IsA<ARpgCharacterPartsTestBodyPart>());

	Chest->RemoveCharacterParts();
	TestEqual(TEXT("Removing again is harmless"), Parts->GetCharacterPartActors().Num(), 1);

	// Pawns without a parts component simply wear nothing.
	ACharacter* Plain = TestWorld.GetWorld()->SpawnActor<ACharacter>();
	URpgEquipmentInstance* Orphan = NewObject<URpgEquipmentInstance>(Plain);
	Orphan->AddCharacterParts(GetDefault<URpgCharacterPartsTestChestDefinition>()->CharacterParts);
	Orphan->RemoveCharacterParts();
	TestTrue(TEXT("A pawn without parts component is a no-op"), Plain->FindComponentByClass<UChildActorComponent>() == nullptr);
	Parts->RemoveAllCharacterParts();
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCharacterPartsValidationTest,
	"SurvivalRpg.Cosmetics.CharacterParts.EquipmentValidationRejectsReplicatedOrMissingParts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCharacterPartsValidationTest::RunTest(const FString& Parameters)
{
	URpgCharacterPartsTestChestDefinition* Definition = NewObject<URpgCharacterPartsTestChestDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
	const auto Errors = [Definition]()
	{
		FDataValidationContext Context;
		Definition->IsDataValid(Context);
		return Context.GetNumErrors();
	};

	// Compare against the definition's own baseline so unrelated base-class checks do not matter.
	const uint32 Baseline = Errors();
	Definition->CharacterParts[0].PartClass = ARpgCharacterPartsTestReplicatedPart::StaticClass();
	TestTrue(TEXT("A replicated part class is an error"), Errors() > Baseline);

	Definition->CharacterParts[0].PartClass = nullptr;
	TestTrue(TEXT("A missing part class is an error"), Errors() > Baseline);
	return true;
}
#endif

#endif
