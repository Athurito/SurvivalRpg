#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Harvesting/RpgHarvestableInstancedMeshActor.h"
#include "Harvesting/RpgHarvestableInstancedMeshComponent.h"
#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

namespace RpgHarvestableInstancedMeshTests
{
	class FScopedTestWorld
	{
	public:
		FScopedTestWorld()
		{
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			if (!GameInstance)
			{
				return;
			}

			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}

		~FScopedTestWorld()
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

		UWorld* GetWorld() const
		{
			return World;
		}

	private:
		TObjectPtr<UGameInstance> GameInstance = nullptr;
		TObjectPtr<UWorld> World = nullptr;
	};

	ARpgHarvestableInstancedMeshActor* SpawnThreeInstanceFixture(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		ARpgHarvestableInstancedMeshActor* Actor = World->SpawnActorDeferred<ARpgHarvestableInstancedMeshActor>(
			ARpgHarvestableInstancedMeshActor::StaticClass(),
			FTransform::Identity,
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Actor || !Actor->HarvestableInstances)
		{
			return nullptr;
		}

		for (int32 InstanceIndex = 0; InstanceIndex < 3; ++InstanceIndex)
		{
			const FTransform AuthoredTransform(
				FRotator::ZeroRotator,
				FVector(static_cast<double>(InstanceIndex) * 100.0, 0.0, 0.0),
				FVector::OneVector);
			if (Actor->HarvestableInstances->AddInstance(AuthoredTransform) != InstanceIndex)
			{
				return nullptr;
			}
		}

		Actor->FinishSpawning(FTransform::Identity);
		if (!Actor->HasActorBegunPlay())
		{
			Actor->DispatchBeginPlay();
		}
		return Actor;
	}

	bool GatherInstanceOption(
		URpgHarvestableInstancedMeshComponent* Component,
		AActor* Requester,
		int32 InstanceIndex,
		FInteractionOption& OutOption)
	{
		if (!Component || !Component->GetOwner())
		{
			return false;
		}

		FTransform InstanceTransform;
		if (!Component->GetInstanceTransform(InstanceIndex, InstanceTransform, true))
		{
			return false;
		}

		FInteractionQuery Query;
		Query.RequestingAvatar = Requester;
		Query.QueryMode = ERpgInteractionQueryMode::Focus;
		Query.QueryOrigin = Requester ? Requester->GetActorLocation() : FVector::ZeroVector;
		Query.CandidateHit = FHitResult(
			Component->GetOwner(),
			Component,
			InstanceTransform.GetLocation(),
			FVector::UpVector);
		Query.CandidateHit.Item = InstanceIndex;

		TScriptInterface<IInteractableTarget> Target;
		Target.SetObject(Component);
		Target.SetInterface(Component);
		TArray<FInteractionOption> Options;
		FInteractionOptionBuilder Builder(Target, Options);
		Component->GatherInteractionOptions(Query, Builder);
		if (Options.Num() != 1)
		{
			return false;
		}

		OutOption = Options[0];
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestableInstancedMeshStableStateTest,
	"SurvivalRpg.Interaction.Harvesting.HISM.StableIndexRevisionAndAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestableInstancedMeshStableStateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestableInstancedMeshTests;

	FScopedTestWorld TestWorld;
	ARpgHarvestableInstancedMeshActor* ResourceActor = SpawnThreeInstanceFixture(TestWorld.GetWorld());
	URpgHarvestableInstancedMeshComponent* Component = ResourceActor ? ResourceActor->HarvestableInstances : nullptr;
	FActorSpawnParameters RequesterSpawnParameters;
	RequesterSpawnParameters.Name = MakeUniqueObjectName(TestWorld.GetWorld(), AActor::StaticClass(), TEXT("HarvestRequester"));
	RequesterSpawnParameters.ObjectFlags = RF_Transient;
	AActor* Requester = TestWorld.GetWorld()
		? TestWorld.GetWorld()->SpawnActor<AActor>(RequesterSpawnParameters)
		: nullptr;
	if (!TestNotNull(TEXT("Standalone HISM test world exists"), TestWorld.GetWorld()) ||
		!TestNotNull(TEXT("Harvestable wrapper actor is spawned"), ResourceActor) ||
		!TestNotNull(TEXT("Harvestable HISM component exists"), Component) ||
		!TestNotNull(TEXT("Harvest requester exists"), Requester))
	{
		return false;
	}

	TestTrue(TEXT("Standalone resource actor has server authority"), ResourceActor->HasAuthority());
	TestEqual(TEXT("Fixture owns three stable instance identities"), Component->GetInstanceCount(), 3);
	for (int32 InstanceIndex = 0; InstanceIndex < 3; ++InstanceIndex)
	{
		TestTrue(*FString::Printf(TEXT("Instance %d starts active"), InstanceIndex), Component->IsResourceInstanceActive(InstanceIndex));
		TestEqual(*FString::Printf(TEXT("Instance %d starts at revision zero"), InstanceIndex), Component->GetResourceInstanceRevision(InstanceIndex), 0);
	}

	FTransform OriginalFirstTransform;
	FTransform OriginalTargetTransform;
	FTransform OriginalLastTransform;
	TestTrue(TEXT("First authored transform is readable"), Component->GetInstanceTransform(0, OriginalFirstTransform, false));
	TestTrue(TEXT("Target authored transform is readable"), Component->GetInstanceTransform(1, OriginalTargetTransform, false));
	TestTrue(TEXT("Last authored transform is readable"), Component->GetInstanceTransform(2, OriginalLastTransform, false));

	FInteractionOption InitialOption;
	if (!TestTrue(TEXT("Active target instance gathers one option"), GatherInstanceOption(Component, Requester, 1, InitialOption)))
	{
		return false;
	}
	TestTrue(
		TEXT("Manual harvest option uses the feature-owned semantic action"),
		InitialOption.InteractionTag == RpgHarvestingMagicGameplayTags::Rpg_Interaction_Action_Harvest_Manual);
	TestEqual(TEXT("Option preserves the concrete HISM index"), InitialOption.TargetRef.InstanceIndex, 1);
	TestEqual(TEXT("Option starts with revision zero"), InitialOption.TargetRef.Revision, 0);
	TestTrue(TEXT("Option preserves the concrete HISM component"), InitialOption.TargetRef.TargetComponent.Get() == Component);

	ResourceActor->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("A simulated proxy cannot deplete an instance"), Component->SetResourceInstanceActive(1, false));
	TestTrue(TEXT("Rejected proxy mutation leaves the instance active"), Component->IsResourceInstanceActive(1));
	TestEqual(TEXT("Rejected proxy mutation leaves revision unchanged"), Component->GetResourceInstanceRevision(1), 0);
	ResourceActor->SetRole(ROLE_Authority);

	FInteractionQuery AuthorityQuery;
	AuthorityQuery.RequestingAvatar = Requester;
	AuthorityQuery.QueryMode = ERpgInteractionQueryMode::AuthorityValidation;
	AuthorityQuery.QueryOrigin = Requester->GetActorLocation();
	AuthorityQuery.CandidateHit = FHitResult(
		ResourceActor,
		Component,
		InitialOption.TargetRef.WorldLocation,
		FVector::UpVector);
	AuthorityQuery.CandidateHit.Item = 1;

	TestTrue(TEXT("Authority commits the exact current instance option"), Component->CommitInteraction(AuthorityQuery, InitialOption));
	TestFalse(TEXT("Committed harvest marks only the target instance inactive"), Component->IsResourceInstanceActive(1));
	TestTrue(TEXT("First neighboring instance remains active"), Component->IsResourceInstanceActive(0));
	TestTrue(TEXT("Last neighboring instance remains active"), Component->IsResourceInstanceActive(2));
	TestEqual(TEXT("Depletion advances only the target revision"), Component->GetResourceInstanceRevision(1), 1);
	TestEqual(TEXT("Depletion never removes or reorders instance identities"), Component->GetInstanceCount(), 3);

	FTransform DepletedFirstTransform;
	FTransform DepletedTargetTransform;
	FTransform DepletedLastTransform;
	TestTrue(TEXT("First transform remains readable after depletion"), Component->GetInstanceTransform(0, DepletedFirstTransform, false));
	TestTrue(TEXT("Depleted transform remains addressable by the same index"), Component->GetInstanceTransform(1, DepletedTargetTransform, false));
	TestTrue(TEXT("Last transform remains readable after depletion"), Component->GetInstanceTransform(2, DepletedLastTransform, false));
	TestTrue(TEXT("First neighboring transform is unchanged"), DepletedFirstTransform.Equals(OriginalFirstTransform));
	TestTrue(TEXT("Last neighboring transform is unchanged"), DepletedLastTransform.Equals(OriginalLastTransform));
	TestTrue(TEXT("Depleted instance keeps its location"), DepletedTargetTransform.GetLocation().Equals(OriginalTargetTransform.GetLocation()));
	TestTrue(TEXT("Depleted instance uses zero scale instead of index removal"), DepletedTargetTransform.GetScale3D().IsNearlyZero());

	TestFalse(TEXT("The stale pre-harvest option cannot be replayed"), Component->CommitInteraction(AuthorityQuery, InitialOption));
	FInteractionOption HiddenDepletedOption;
	TestFalse(TEXT("A depleted target instance no longer gathers an option"), GatherInstanceOption(Component, Requester, 1, HiddenDepletedOption));

	TestTrue(TEXT("Authority can reactivate the same stable instance"), Component->SetResourceInstanceActive(1, true));
	TestTrue(TEXT("Reactivated instance is active"), Component->IsResourceInstanceActive(1));
	TestEqual(TEXT("Reactivation advances the same instance revision"), Component->GetResourceInstanceRevision(1), 2);
	TestEqual(TEXT("Reactivation preserves instance count"), Component->GetInstanceCount(), 3);
	FTransform ReactivatedTransform;
	TestTrue(TEXT("Reactivated transform is readable"), Component->GetInstanceTransform(1, ReactivatedTransform, false));
	TestTrue(TEXT("Reactivation restores the authored transform"), ReactivatedTransform.Equals(OriginalTargetTransform));

	FInteractionOption ReactivatedOption;
	if (!TestTrue(TEXT("Reactivated target gathers a fresh option"), GatherInstanceOption(Component, Requester, 1, ReactivatedOption)))
	{
		return false;
	}
	TestEqual(TEXT("Fresh option carries the new revision"), ReactivatedOption.TargetRef.Revision, 2);

	FInteractionQuery WrongInstanceQuery = AuthorityQuery;
	WrongInstanceQuery.CandidateHit.Item = 2;
	TestFalse(TEXT("Authority rejects a mismatched hit instance index"), Component->CommitInteraction(WrongInstanceQuery, ReactivatedOption));
	TestTrue(TEXT("Rejected mismatched index leaves the target active"), Component->IsResourceInstanceActive(1));
	TestEqual(TEXT("Rejected mismatched index leaves revision unchanged"), Component->GetResourceInstanceRevision(1), 2);

	AuthorityQuery.CandidateHit.Item = 1;
	AuthorityQuery.CandidateHit.ImpactPoint = ReactivatedOption.TargetRef.WorldLocation;
	AuthorityQuery.CandidateHit.Location = ReactivatedOption.TargetRef.WorldLocation;
	TestTrue(TEXT("Fresh exact option can be committed after reactivation"), Component->CommitInteraction(AuthorityQuery, ReactivatedOption));
	TestEqual(TEXT("Second depletion advances revision monotonically"), Component->GetResourceInstanceRevision(1), 3);
	TestEqual(TEXT("Second depletion still preserves all instance indices"), Component->GetInstanceCount(), 3);

	const FStructProperty* ReplicatedStatesProperty = FindFProperty<FStructProperty>(
		URpgHarvestableInstancedMeshComponent::StaticClass(),
		TEXT("ReplicatedInstanceStates"));
	TestTrue(
		TEXT("Sparse instance state is a replicated property"),
		ReplicatedStatesProperty && ReplicatedStatesProperty->HasAnyPropertyFlags(CPF_Net));
	TestTrue(
		TEXT("Replicated property uses the declared sparse state-list struct"),
		ReplicatedStatesProperty && ReplicatedStatesProperty->Struct == FRpgHarvestedInstanceStateList::StaticStruct());
	TestTrue(
		TEXT("Sparse state list is configured for FastArray net-delta serialization"),
		TStructOpsTypeTraits<FRpgHarvestedInstanceStateList>::WithNetDeltaSerializer != 0);

	TestFalse(TEXT("Invalid negative instance indices are inactive"), Component->IsResourceInstanceActive(INDEX_NONE));
	TestEqual(TEXT("Invalid negative instance indices have no revision"), Component->GetResourceInstanceRevision(INDEX_NONE), INDEX_NONE);
	TestFalse(TEXT("Out-of-bounds instance mutation is rejected"), Component->SetResourceInstanceActive(3, false));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
