#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Animation/RpgWieldedItemAnimInstance.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace RpgWieldedItemAnimInstanceTests
{
	// The rigged bow has a root and a Bow_String bone; it serves as wielder and as item, so no character asset is needed.
	const TCHAR* BowMeshPath = TEXT("/GF_Combat_Ranged/Equipment/Bow/SkeletonArmy/Meshes/SK_Skeleton_Archer_Bow.SK_Skeleton_Archer_Bow");
	const FName StringBone(TEXT("Bow_String"));

	template <typename ValueType, typename PropertyType>
	ValueType ReadProperty(const UObject* Object, const TCHAR* Name)
	{
		const PropertyType* Property = Object ? FindFProperty<PropertyType>(Object->GetClass(), Name) : nullptr;
		return Property ? Property->GetPropertyValue_InContainer(Object) : ValueType();
	}

	FVector ReadVector(const UObject* Object, const TCHAR* Name)
	{
		const FStructProperty* Property = Object ? FindFProperty<FStructProperty>(Object->GetClass(), Name) : nullptr;
		return Property ? *Property->ContainerPtrToValuePtr<FVector>(Object) : FVector(UE_BIG_NUMBER);
	}

	void WriteName(UObject* Object, const TCHAR* Name, const FName Value)
	{
		if (const FNameProperty* Property = Object ? FindFProperty<FNameProperty>(Object->GetClass(), Name) : nullptr)
		{
			Property->SetPropertyValue_InContainer(Object, Value);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgWieldedItemAnimInstanceTest,
	"SurvivalRpg.Animation.WieldedItem.SnapshotsTheWielder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgWieldedItemAnimInstanceTest::RunTest(const FString& Parameters)
{
	using namespace RpgWieldedItemAnimInstanceTests;

	USkeletalMesh* BowMesh = LoadObject<USkeletalMesh>(nullptr, BowMeshPath);
	if (!TestNotNull(TEXT("The rigged bow loads"), BowMesh)) return false;

	TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient));
	GameInstance->InitializeStandalone();
	UWorld* World = GameInstance->GetWorld();
	ON_SCOPE_EXIT
	{
		GameInstance->Shutdown();
		if (World)
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};
	if (!TestNotNull(TEXT("The test world exists"), World)) return false;
	World->InitializeActorsForPlay(FURL());

	// The wielder: a turned mesh with the tracked bone and its own ability system, which presents its draw locally.
	AActor* Wielder = World->SpawnActor<AActor>();
	USkeletalMeshComponent* WielderMesh = NewObject<USkeletalMeshComponent>(Wielder, TEXT("WielderMesh"));
	WielderMesh->SetSkeletalMeshAsset(BowMesh);
	Wielder->SetRootComponent(WielderMesh);
	WielderMesh->RegisterComponent();
	Wielder->SetActorLocationAndRotation(FVector(100.0f, 200.0f, 300.0f), FRotator(10.0f, 90.0f, 0.0f));
	WielderMesh->RefreshBoneTransforms();
	URpgAbilitySystemComponent* ASC = NewObject<URpgAbilitySystemComponent>(Wielder, TEXT("AbilitySystem"));
	ASC->RegisterComponent();
	ASC->InitAbilityActorInfo(Wielder, Wielder);

	// The item, attached like equipment: its mesh sits under the root, 40 cm below the wielder's mesh.
	AActor* Item = World->SpawnActor<AActor>();
	USceneComponent* ItemRoot = NewObject<USceneComponent>(Item, TEXT("ItemRoot"));
	Item->SetRootComponent(ItemRoot);
	ItemRoot->RegisterComponent();
	USkeletalMeshComponent* ItemMesh = NewObject<USkeletalMeshComponent>(Item, TEXT("ItemMesh"));
	ItemMesh->SetupAttachment(ItemRoot);
	ItemMesh->SetSkeletalMeshAsset(BowMesh);
	ItemMesh->SetAnimInstanceClass(URpgWieldedItemAnimInstance::StaticClass());
	ItemMesh->RegisterComponent();
	Item->AttachToComponent(WielderMesh, FAttachmentTransformRules::KeepRelativeTransform);
	Item->SetActorRelativeLocation(FVector(0.0f, 0.0f, -40.0f));

	UAnimInstance* Anim = ItemMesh->GetAnimInstance();
	if (!TestTrue(TEXT("The item's mesh runs the wielded item animation"), Anim && Anim->IsA<URpgWieldedItemAnimInstance>())) return false;
	WriteName(Anim, TEXT("WielderBone"), StringBone);
	WriteName(Anim, TEXT("LocalFrameBone"), StringBone);
	const auto Update = [ItemMesh]() { ItemMesh->TickAnimation(1.0f / 60.0f, false); };
	const auto HasBone = [Anim]() { return ReadProperty<bool, FBoolProperty>(Anim, TEXT("bHasWielderBone")); };
	const auto Drawing = [Anim]() { return ReadProperty<bool, FBoolProperty>(Anim, TEXT("bWielderRangedDrawing")); };

	// The wielder's string bone lies 40 cm along the item's string axis, whatever the wielder's world transform.
	Update();
	TestTrue(TEXT("An attached item finds the wielder's bone"), HasBone());
	TestTrue(TEXT("The bone is located in the item's string frame"),
		ReadVector(Anim, TEXT("WielderBoneLocation")).Equals(FVector(0.0f, 0.0f, 40.0f), 0.01f));
	TestFalse(TEXT("A wielder that does not draw presents no draw"), Drawing());

	// The draw comes from the wielder's presentation read model, here its own prediction.
	ASC->SetLocalRangedDrawing(true);
	Update();
	TestTrue(TEXT("The wielder's draw reaches the item"), Drawing());
	ASC->SetLocalRangedDrawing(false);
	Update();
	TestFalse(TEXT("Its end reaches the item"), Drawing());

	// Without the bone, or once detached, nothing is tracked and no draw is presented.
	WriteName(Anim, TEXT("WielderBone"), TEXT("NoSuchBone"));
	Update();
	TestFalse(TEXT("A bone the wielder lacks is not tracked"), HasBone());
	TestTrue(TEXT("and leaves the location at zero"), ReadVector(Anim, TEXT("WielderBoneLocation")).IsZero());
	WriteName(Anim, TEXT("WielderBone"), StringBone);
	ASC->SetLocalRangedDrawing(true);
	Item->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Update();
	TestFalse(TEXT("A detached item tracks no wielder"), HasBone());
	TestFalse(TEXT("and presents no draw"), Drawing());
	return true;
}

#endif
