#include "RpgRecipeUnlockComponent.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Crafting/RpgCraftingRecipeDefinition.h"
#include "SurvivalRpg/SurvivalRpg.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgRecipeUnlockComponent)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Rpg_Crafting_Message_RecipeUnlockChanged, "Rpg.Crafting.Message.RecipeUnlockChanged");

namespace RpgRecipeUnlockPrivate
{
	const FName WorldSaveFeatureId(TEXT("Crafting.RecipeUnlocks"));

	ARpgGameModeBase* GetGameMode(const UActorComponent& Component)
	{
		const UWorld* World = Component.GetWorld();
		return World ? World->GetAuthGameMode<ARpgGameModeBase>() : nullptr;
	}
}

URpgRecipeUnlockComponent::URpgRecipeUnlockComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void URpgRecipeUnlockComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, UnlockedRecipeTags);
}

void URpgRecipeUnlockComponent::BeginPlay()
{
	Super::BeginPlay();

	const AActor* OwnerActor = GetOwner();
	if (OwnerActor && OwnerActor->HasAuthority())
	{
		if (ARpgGameModeBase* GameMode = RpgRecipeUnlockPrivate::GetGameMode(*this))
		{
			GameMode->RegisterWorldSaveParticipant(this);
		}
	}
}

void URpgRecipeUnlockComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ARpgGameModeBase* GameMode = RpgRecipeUnlockPrivate::GetGameMode(*this))
	{
		GameMode->UnregisterWorldSaveParticipant(this);
	}
	Super::EndPlay(EndPlayReason);
}

FName URpgRecipeUnlockComponent::GetWorldSaveFeatureId() const
{
	return RpgRecipeUnlockPrivate::WorldSaveFeatureId;
}

bool URpgRecipeUnlockComponent::CaptureWorldSaveData(FRpgWorldFeatureSaveData& OutData)
{
	FRpgRecipeUnlockSaveData Data;
	Data.UnlockedRecipeTags = UnlockedRecipeTags;

	OutData.SchemaVersion = CurrentSaveSchemaVersion;
	OutData.Payload.Reset();
	FMemoryWriter Writer(OutData.Payload, true);
	FObjectAndNameAsStringProxyArchive Archive(Writer, false);
	Archive.ArIsSaveGame = true;
	FRpgRecipeUnlockSaveData::StaticStruct()->SerializeItem(Archive, &Data, nullptr);
	return !Archive.IsError();
}

bool URpgRecipeUnlockComponent::RestoreWorldSaveData(const FRpgWorldFeatureSaveData* SavedData)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return false;
	}

	FRpgRecipeUnlockSaveData Data;
	if (SavedData)
	{
		if (SavedData->SchemaVersion != CurrentSaveSchemaVersion || SavedData->Payload.IsEmpty())
		{
			UE_LOG(LogRpg, Error, TEXT("Saved recipe unlocks have schema %d or no payload."), SavedData->SchemaVersion);
			return false;
		}

		FMemoryReader Reader(SavedData->Payload, true);
		FObjectAndNameAsStringProxyArchive Archive(Reader, true);
		Archive.ArIsSaveGame = true;
		FRpgRecipeUnlockSaveData::StaticStruct()->SerializeItem(Archive, &Data, nullptr);
		if (Archive.IsError() || Reader.IsError())
		{
			UE_LOG(LogRpg, Error, TEXT("Saved recipe unlocks do not deserialize."));
			return false;
		}
		for (const FGameplayTag& SavedTag : Data.UnlockedRecipeTags)
		{
			if (!IsConcreteRecipeTag(SavedTag))
			{
				UE_LOG(LogRpg, Error, TEXT("Saved recipe unlock %s is not a Recipe tag."), *SavedTag.ToString());
				return false;
			}
		}
	}

	// A null entry restores the pristine world, which has learned nothing yet.
	const FGameplayTagContainer PreviousTags = UnlockedRecipeTags;
	UnlockedRecipeTags = Data.UnlockedRecipeTags;
	if (!PreviousTags.HasAllExact(UnlockedRecipeTags) || !UnlockedRecipeTags.HasAllExact(PreviousTags))
	{
		FGameplayTagContainer AddedTags = UnlockedRecipeTags;
		AddedTags.RemoveTags(PreviousTags);
		FGameplayTagContainer RemovedTags = PreviousTags;
		RemovedTags.RemoveTags(UnlockedRecipeTags);
		for (const FGameplayTag& RemovedTag : RemovedTags)
		{
			BroadcastUnlockChanged(RemovedTag, false);
		}
		for (const FGameplayTag& AddedTag : AddedTags)
		{
			BroadcastUnlockChanged(AddedTag, true);
		}
		OwnerActor->ForceNetUpdate();
	}
	return true;
}

bool URpgRecipeUnlockComponent::UnlockRecipeTag(FGameplayTag RecipeUnlockTag)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !IsConcreteRecipeTag(RecipeUnlockTag))
	{
		return false;
	}

	if (UnlockedRecipeTags.HasTagExact(RecipeUnlockTag))
	{
		BroadcastUnlockChanged(RecipeUnlockTag, false);
		return false;
	}

	UnlockedRecipeTags.AddTag(RecipeUnlockTag);
	BroadcastUnlockChanged(RecipeUnlockTag, true);
	OwnerActor->ForceNetUpdate();
	if (ARpgGameModeBase* GameMode = RpgRecipeUnlockPrivate::GetGameMode(*this))
	{
		GameMode->MarkWorldFeatureSaveDirty(this);
	}
	return true;
}

bool URpgRecipeUnlockComponent::IsConcreteRecipeTag(const FGameplayTag RecipeUnlockTag)
{
	const FGameplayTag RecipeRoot = FGameplayTag::RequestGameplayTag(TEXT("Recipe"), false);
	return RecipeRoot.IsValid() && RecipeUnlockTag.IsValid() && RecipeUnlockTag != RecipeRoot &&
		RecipeUnlockTag.MatchesTag(RecipeRoot);
}

bool URpgRecipeUnlockComponent::UnlockRecipe(const URpgCraftingRecipeDefinition* RecipeDefinition)
{
	return RecipeDefinition && RecipeDefinition->RecipeUnlockTag.IsValid()
		? UnlockRecipeTag(RecipeDefinition->RecipeUnlockTag)
		: false;
}

bool URpgRecipeUnlockComponent::IsRecipeTagUnlocked(FGameplayTag RecipeUnlockTag) const
{
	return RecipeUnlockTag.IsValid() && UnlockedRecipeTags.HasTagExact(RecipeUnlockTag);
}

bool URpgRecipeUnlockComponent::IsRecipeUnlocked(const URpgCraftingRecipeDefinition* RecipeDefinition) const
{
	if (!RecipeDefinition)
	{
		return false;
	}

	return RecipeDefinition->bUnlockedByDefault ||
		(RecipeDefinition->RecipeUnlockTag.IsValid() && IsRecipeTagUnlocked(RecipeDefinition->RecipeUnlockTag));
}

void URpgRecipeUnlockComponent::OnRep_UnlockedRecipeTags(FGameplayTagContainer PreviousTags)
{
	FGameplayTagContainer AddedTags = UnlockedRecipeTags;
	AddedTags.RemoveTags(PreviousTags);

	if (AddedTags.IsEmpty())
	{
		BroadcastUnlockChanged(FGameplayTag(), false);
		return;
	}

	for (const FGameplayTag& AddedTag : AddedTags)
	{
		BroadcastUnlockChanged(AddedTag, true);
	}
}

void URpgRecipeUnlockComponent::BroadcastUnlockChanged(FGameplayTag RecipeUnlockTag, bool bNewlyUnlocked) const
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || !IsRegistered() || HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return;
	}

	FRpgRecipeUnlockChangeMessage Message;
	Message.UnlockOwner = const_cast<URpgRecipeUnlockComponent*>(this);
	Message.RecipeUnlockTag = RecipeUnlockTag;
	Message.bNewlyUnlocked = bNewlyUnlocked;

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
	MessageSubsystem.BroadcastMessage(TAG_Rpg_Crafting_Message_RecipeUnlockChanged, Message);
}
