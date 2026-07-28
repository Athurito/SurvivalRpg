#include "RpgLootSourceComponent.h"

#include "RpgInventoryContainerComponent.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"
#include "Loot/RpgLootResolver.h"
#include "Loot/RpgLootTable.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/SurvivalRpg.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgLootSourceComponent)

URpgLootSourceComponent::URpgLootSourceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URpgLootSourceComponent::BeginPlay()
{
	Super::BeginPlay();

	BoundHealthComponent = URpgHealthComponent::FindHealthComponent(GetOwner());
	if (BoundHealthComponent)
	{
		BoundHealthComponent->OnDeathFinished.AddDynamic(this, &ThisClass::HandleDeathFinished);
	}

	if (bUnlockContainerOnDeath && !bLootPopulated)
	{
		if (URpgInventoryContainerComponent* ContainerComponent = FindContainerComponent())
		{
			ContainerComponent->SetContainerAccessible(false);
		}
	}
}

void URpgLootSourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BoundHealthComponent)
	{
		BoundHealthComponent->OnDeathFinished.RemoveDynamic(this, &ThisClass::HandleDeathFinished);
		BoundHealthComponent = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void URpgLootSourceComponent::PopulateLoot()
{
	TryPopulateLoot();
}

bool URpgLootSourceComponent::TryPopulateLoot()
{
	if (bLootPopulated)
	{
		return true;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return false;
	}

	URpgInventoryManagerComponent* InventoryManager = FindInventoryManager();
	if (!InventoryManager)
	{
		return false;
	}

	const bool bHasLegacyLoot = !LootTemplates.IsEmpty() || !LootInstances.IsEmpty();
	if (LootTable && bHasLegacyLoot)
	{
		UE_LOG(
			LogRpg,
			Error,
			TEXT("Loot source [%s] has both a LootTable and deprecated fixed loot configured; population was rejected."),
			*GetNameSafe(OwnerActor));
		return false;
	}
	if (!LootTable && !bHasLegacyLoot)
	{
		UE_LOG(
			LogRpg,
			Error,
			TEXT("Loot source [%s] has no LootTable or deprecated fixed loot configured; population was rejected."),
			*GetNameSafe(OwnerActor));
		return false;
	}

	if (!bLootRollResolved)
	{
		if (LootTable)
		{
			FRpgLootRollContext Context;
			Context.SourceActor = OwnerActor;
			Context.SourceTags = SourceTags;
			Context.SourceLevel = SourceLevel;
			Context.Seed = static_cast<int32>(GetTypeHash(FGuid::NewGuid()));
			if (!FRpgLootResolver::RollLoot(
					LootTable,
					Context,
					CachedLootRoll))
			{
				UE_LOG(
					LogRpg,
					Error,
					TEXT("Loot table [%s] could not be resolved for loot source [%s]."),
					*GetNameSafe(LootTable),
					*GetNameSafe(OwnerActor));
				return false;
			}
			bLootMaterialized = false;
		}
		else
		{
			CachedLootPickup.Templates = LootTemplates;
			CachedLootPickup.Instances = LootInstances;
			bLootMaterialized = true;
		}
		bLootRollResolved = true;
	}

	if (!bLootMaterialized)
	{
		if (!FRpgLootResolver::MaterializeLoot(
				OwnerActor,
				CachedLootRoll,
				CachedLootPickup))
		{
			UE_LOG(
				LogRpg,
				Error,
				TEXT("The cached loot roll for source [%s] could not be materialized."),
				*GetNameSafe(OwnerActor));
			return false;
		}
		bLootMaterialized = true;
	}

	// An empty successful roll is a valid no-drop result and needs no inventory mutation.
	if (!CachedLootPickup.Templates.IsEmpty() || !CachedLootPickup.Instances.IsEmpty())
	{
		TArray<FRpgInventoryItemId> AffectedItemIds;
		const FRpgInventoryMutationResult Result = InventoryManager->AddPickupBatch(
			CachedLootPickup,
			AffectedItemIds);
		if (!Result.IsSuccess())
		{
			UE_LOG(
				LogRpg,
				Error,
				TEXT("Atomic loot population failed for source [%s] with inventory result code [%d]."),
				*GetNameSafe(OwnerActor),
				static_cast<int32>(Result.Code));
			return false;
		}
	}

	bLootPopulated = true;
	CachedLootRoll = FRpgLootRollResult();
	CachedLootPickup = FInventoryPickup();
	if (bUnlockContainerOnDeath)
	{
		if (URpgInventoryContainerComponent* ContainerComponent = FindContainerComponent())
		{
			ContainerComponent->SetContainerAccessible(true);
		}
	}
	return true;
}

void URpgLootSourceComponent::HandleDeathFinished(AActor* OwningActor)
{
	if (OwningActor != GetOwner())
	{
		return;
	}

	TryPopulateLoot();
}

URpgInventoryManagerComponent* URpgLootSourceComponent::FindInventoryManager() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<URpgInventoryManagerComponent>() : nullptr;
}

URpgInventoryContainerComponent* URpgLootSourceComponent::FindContainerComponent() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<URpgInventoryContainerComponent>() : nullptr;
}

#if WITH_EDITOR
EDataValidationResult URpgLootSourceComponent::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(
		Super::IsDataValid(Context),
		EDataValidationResult::Valid);
	const bool bHasLegacyLoot = !LootTemplates.IsEmpty() || !LootInstances.IsEmpty();
	if (LootTable && bHasLegacyLoot)
	{
		Context.AddError(NSLOCTEXT(
			"RpgLootSource",
			"MixedLootConfiguration",
			"A loot source cannot configure both LootTable and deprecated fixed loot lists."));
		Result = EDataValidationResult::Invalid;
	}
	else if (!LootTable && !bHasLegacyLoot)
	{
		Context.AddError(NSLOCTEXT(
			"RpgLootSource",
			"MissingLootConfiguration",
			"A loot source requires a LootTable or a deprecated fixed-loot fallback."));
		Result = EDataValidationResult::Invalid;
	}

	if (SourceLevel <= 0)
	{
		Context.AddError(NSLOCTEXT(
			"RpgLootSource",
			"InvalidSourceLevel",
			"Loot Source Level must be at least one."));
		Result = EDataValidationResult::Invalid;
	}

	if (LootTable)
	{
		FString Error;
		if (!LootTable->HasValidConfiguration(&Error))
		{
			Context.AddError(FText::Format(
				NSLOCTEXT(
					"RpgLootSource",
					"InvalidLootTable",
					"LootTable is invalid: {0}"),
				FText::FromString(Error)));
			Result = EDataValidationResult::Invalid;
		}
	}
	else
	{
		for (int32 Index = 0; Index < LootTemplates.Num(); ++Index)
		{
			const FPickupTemplate& Template = LootTemplates[Index];
			if (!Template.ItemDef || Template.StackCount <= 0)
			{
				Context.AddError(FText::Format(
					NSLOCTEXT(
						"RpgLootSource",
						"InvalidLegacyTemplate",
						"Deprecated fixed loot template {0} requires an item definition and a positive stack count."),
					FText::AsNumber(Index)));
				Result = EDataValidationResult::Invalid;
			}
		}

		for (int32 Index = 0; Index < LootInstances.Num(); ++Index)
		{
			const URpgInventoryItemInstance* Item = LootInstances[Index].Item;
			if (!Item || !Item->GetItemDef())
			{
				Context.AddError(FText::Format(
					NSLOCTEXT(
						"RpgLootSource",
						"InvalidLegacyInstance",
						"Deprecated fixed loot instance {0} requires a concrete item with an item definition."),
					FText::AsNumber(Index)));
				Result = EDataValidationResult::Invalid;
			}
		}
	}

	return Result;
}
#endif
