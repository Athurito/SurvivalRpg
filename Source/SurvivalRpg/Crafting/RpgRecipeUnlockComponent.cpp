#include "RpgRecipeUnlockComponent.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/Crafting/RpgCraftingRecipeDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgRecipeUnlockComponent)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Rpg_Crafting_Message_RecipeUnlockChanged, "Rpg.Crafting.Message.RecipeUnlockChanged");

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

bool URpgRecipeUnlockComponent::UnlockRecipeTag(FGameplayTag RecipeUnlockTag)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !RecipeUnlockTag.IsValid())
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
	return true;
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
