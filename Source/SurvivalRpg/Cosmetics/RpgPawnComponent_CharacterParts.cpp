#include "RpgPawnComponent_CharacterParts.h"

#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "GameplayTagAssetInterface.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/PhysicsAsset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPawnComponent_CharacterParts)

FString FRpgAppliedCharacterPartEntry::GetDebugString() const
{
	return FString::Printf(TEXT("(PartClass: %s, Socket: %s, Instance: %s)"), *GetPathNameSafe(Part.PartClass), *Part.SocketName.ToString(), *GetPathNameSafe(SpawnedComponent));
}

void FRpgCharacterPartList::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	bool bDestroyedAnyActors = false;
	for (const int32 Index : RemovedIndices)
	{
		bDestroyedAnyActors |= DestroyActorForEntry(Entries[Index]);
	}

	if (bDestroyedAnyActors && ensure(OwnerComponent))
	{
		OwnerComponent->BroadcastChanged();
	}
}

void FRpgCharacterPartList::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	bool bCreatedAnyActors = false;
	for (const int32 Index : AddedIndices)
	{
		bCreatedAnyActors |= SpawnActorForEntry(Entries[Index]);
	}

	if (bCreatedAnyActors && ensure(OwnerComponent))
	{
		OwnerComponent->BroadcastChanged();
	}
}

void FRpgCharacterPartList::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	// Changes are not propagated piecewise; recreate the part.
	bool bChangedAnyActors = false;
	for (const int32 Index : ChangedIndices)
	{
		FRpgAppliedCharacterPartEntry& Entry = Entries[Index];
		bChangedAnyActors |= DestroyActorForEntry(Entry);
		bChangedAnyActors |= SpawnActorForEntry(Entry);
	}

	if (bChangedAnyActors && ensure(OwnerComponent))
	{
		OwnerComponent->BroadcastChanged();
	}
}

FRpgCharacterPartHandle FRpgCharacterPartList::AddEntry(const FRpgCharacterPart& NewPart)
{
	FRpgCharacterPartHandle Result;
	Result.PartHandle = PartHandleCounter++;

	if (ensure(OwnerComponent && OwnerComponent->GetOwner() && OwnerComponent->GetOwner()->HasAuthority()))
	{
		FRpgAppliedCharacterPartEntry& NewEntry = Entries.AddDefaulted_GetRef();
		NewEntry.Part = NewPart;
		NewEntry.PartHandle = Result.PartHandle;

		if (SpawnActorForEntry(NewEntry))
		{
			OwnerComponent->BroadcastChanged();
		}

		MarkItemDirty(NewEntry);
	}

	return Result;
}

void FRpgCharacterPartList::RemoveEntry(FRpgCharacterPartHandle Handle)
{
	for (auto EntryIt = Entries.CreateIterator(); EntryIt; ++EntryIt)
	{
		FRpgAppliedCharacterPartEntry& Entry = *EntryIt;
		if (Entry.PartHandle == Handle.PartHandle)
		{
			const bool bDestroyedActor = DestroyActorForEntry(Entry);
			EntryIt.RemoveCurrent();
			MarkArrayDirty();

			if (bDestroyedActor && ensure(OwnerComponent))
			{
				OwnerComponent->BroadcastChanged();
			}

			break;
		}
	}
}

void FRpgCharacterPartList::ClearAllEntries(bool bBroadcastChangeDelegate)
{
	bool bDestroyedAnyActors = false;
	for (FRpgAppliedCharacterPartEntry& Entry : Entries)
	{
		bDestroyedAnyActors |= DestroyActorForEntry(Entry);
	}
	Entries.Reset();
	MarkArrayDirty();

	if (bDestroyedAnyActors && bBroadcastChangeDelegate && ensure(OwnerComponent))
	{
		OwnerComponent->BroadcastChanged();
	}
}

FGameplayTagContainer FRpgCharacterPartList::CollectCombinedTags() const
{
	FGameplayTagContainer Result;
	for (const FRpgAppliedCharacterPartEntry& Entry : Entries)
	{
		if (Entry.SpawnedComponent != nullptr)
		{
			if (const IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(Entry.SpawnedComponent->GetChildActor()))
			{
				TagInterface->GetOwnedGameplayTags(Result);
			}
		}
	}

	return Result;
}

bool FRpgCharacterPartList::SpawnActorForEntry(FRpgAppliedCharacterPartEntry& Entry)
{
	bool bCreatedAnyActors = false;

	if (ensure(OwnerComponent) && !OwnerComponent->IsNetMode(NM_DedicatedServer) && Entry.Part.PartClass != nullptr)
	{
		if (USceneComponent* ComponentToAttachTo = OwnerComponent->GetSceneComponentToAttachTo())
		{
			UChildActorComponent* PartComponent = NewObject<UChildActorComponent>(OwnerComponent->GetOwner());
			PartComponent->SetupAttachment(ComponentToAttachTo, Entry.Part.SocketName);
			PartComponent->SetChildActorClass(Entry.Part.PartClass);
			PartComponent->RegisterComponent();

			if (AActor* SpawnedActor = PartComponent->GetChildActor())
			{
				if (Entry.Part.CollisionMode == ERpgCharacterPartCollisionMode::NoCollision)
				{
					SpawnedActor->SetActorEnableCollision(false);
				}

				// The child actor component provides no tick dependency on its parent; add one so parts follow the mesh.
				if (USceneComponent* SpawnedRootComponent = SpawnedActor->GetRootComponent())
				{
					SpawnedRootComponent->AddTickPrerequisiteComponent(ComponentToAttachTo);
				}
			}

			Entry.SpawnedComponent = PartComponent;
			bCreatedAnyActors = true;
		}
	}

	return bCreatedAnyActors;
}

bool FRpgCharacterPartList::DestroyActorForEntry(FRpgAppliedCharacterPartEntry& Entry)
{
	if (Entry.SpawnedComponent == nullptr)
	{
		return false;
	}

	Entry.SpawnedComponent->DestroyComponent();
	Entry.SpawnedComponent = nullptr;
	return true;
}

URpgPawnComponent_CharacterParts::URpgPawnComponent_CharacterParts(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void URpgPawnComponent_CharacterParts::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, CharacterPartList);
}

FRpgCharacterPartHandle URpgPawnComponent_CharacterParts::AddCharacterPart(const FRpgCharacterPart& NewPart)
{
	return CharacterPartList.AddEntry(NewPart);
}

void URpgPawnComponent_CharacterParts::RemoveCharacterPart(FRpgCharacterPartHandle Handle)
{
	CharacterPartList.RemoveEntry(Handle);
}

void URpgPawnComponent_CharacterParts::RemoveAllCharacterParts()
{
	CharacterPartList.ClearAllEntries(/*bBroadcastChangeDelegate=*/ true);
}

void URpgPawnComponent_CharacterParts::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CharacterPartList.ClearAllEntries(/*bBroadcastChangeDelegate=*/ false);

	Super::EndPlay(EndPlayReason);
}

void URpgPawnComponent_CharacterParts::OnRegister()
{
	Super::OnRegister();

	if (!IsTemplate())
	{
		CharacterPartList.SetOwnerComponent(this);
	}
}

TArray<AActor*> URpgPawnComponent_CharacterParts::GetCharacterPartActors() const
{
	TArray<AActor*> Result;
	Result.Reserve(CharacterPartList.Entries.Num());

	for (const FRpgAppliedCharacterPartEntry& Entry : CharacterPartList.Entries)
	{
		if (const UChildActorComponent* PartComponent = Entry.SpawnedComponent)
		{
			if (AActor* SpawnedActor = PartComponent->GetChildActor())
			{
				Result.Add(SpawnedActor);
			}
		}
	}

	return Result;
}

USkeletalMeshComponent* URpgPawnComponent_CharacterParts::GetParentMeshComponent() const
{
	if (const ACharacter* OwningCharacter = Cast<ACharacter>(GetOwner()))
	{
		return OwningCharacter->GetMesh();
	}

	return nullptr;
}

USceneComponent* URpgPawnComponent_CharacterParts::GetSceneComponentToAttachTo() const
{
	if (USkeletalMeshComponent* MeshComponent = GetParentMeshComponent())
	{
		return MeshComponent;
	}

	const AActor* OwnerActor = GetOwner();
	return OwnerActor ? OwnerActor->GetRootComponent() : nullptr;
}

FGameplayTagContainer URpgPawnComponent_CharacterParts::GetCombinedTags(FGameplayTag RequiredPrefix) const
{
	const FGameplayTagContainer Result = CharacterPartList.CollectCombinedTags();
	return RequiredPrefix.IsValid() ? Result.Filter(FGameplayTagContainer(RequiredPrefix)) : Result;
}

void URpgPawnComponent_CharacterParts::BroadcastChanged()
{
	if (USkeletalMeshComponent* MeshComponent = GetParentMeshComponent())
	{
		const FGameplayTagContainer MergedTags = GetCombinedTags(FGameplayTag());

		// Unlike Lyra, an unconfigured selection leaves the gameplay mesh alone, and the swap keeps the running AnimBP
		// instance so equipment-linked animation layers (block) survive a body part arriving after equip.
		if (USkeletalMesh* DesiredMesh = BodyMeshes.SelectBestBodyStyle(MergedTags))
		{
			if (MeshComponent->GetSkeletalMeshAsset() != DesiredMesh)
			{
				MeshComponent->SetSkeletalMesh(DesiredMesh, /*bReinitPose=*/ false);
			}
		}

		if (UPhysicsAsset* PhysicsAsset = BodyMeshes.ForcedPhysicsAsset)
		{
			MeshComponent->SetPhysicsAsset(PhysicsAsset, /*bForceReInit=*/ false);
		}
	}

	OnCharacterPartsChanged.Broadcast(this);
}
