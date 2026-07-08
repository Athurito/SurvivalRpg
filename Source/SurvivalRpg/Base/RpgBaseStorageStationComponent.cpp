#include "RpgBaseStorageStationComponent.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "RpgBaseCampActor.h"
#include "RpgBaseStorageUpgradeDefinition.h"
#include "SurvivalRpg/Interaction/Abilities/RpgGameplayAbility_OpenBaseStorageStation.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgBaseStorageStationComponent)

URpgBaseStorageStationComponent::URpgBaseStorageStationComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	OpenStationOption.Text = NSLOCTEXT("RpgBaseStorage", "OpenBaseStorageText", "Open");
	OpenStationOption.SubText = NSLOCTEXT("RpgBaseStorage", "OpenBaseStorageSubText", "Base Storage");
	OpenStationOption.InteractionAbilityToGrant = URpgGameplayAbility_OpenBaseStorageStation::StaticClass();
}

void URpgBaseStorageStationComponent::BeginPlay()
{
	Super::BeginPlay();

	RegisterWithLinkedBaseCamp();
	ApplyCapacityBonuses(1);
}

void URpgBaseStorageStationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ApplyCapacityBonuses(-1);
	UnregisterFromLinkedBaseCamp();

	Super::EndPlay(EndPlayReason);
}

void URpgBaseStorageStationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, LinkedBaseCamp);
	DOREPLIFETIME(ThisClass, InstalledUpgrades);
	DOREPLIFETIME(ThisClass, bAccessible);
}

void URpgBaseStorageStationComponent::SetLinkedBaseCamp(ARpgBaseCampActor* NewBaseCamp)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || LinkedBaseCamp == NewBaseCamp)
	{
		return;
	}

	ApplyCapacityBonuses(-1);
	UnregisterFromLinkedBaseCamp();
	LinkedBaseCamp = NewBaseCamp;
	RegisterWithLinkedBaseCamp();
	ApplyCapacityBonuses(1);
	OwnerActor->ForceNetUpdate();
}

void URpgBaseStorageStationComponent::OnRep_LinkedBaseCamp()
{
	RegisterWithLinkedBaseCamp();
}

void URpgBaseStorageStationComponent::OnRep_InstalledUpgrades()
{
	OnInstalledUpgradesChanged.Broadcast(this);

	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
}

void URpgBaseStorageStationComponent::GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& InteractionBuilder)
{
	if (CanActorAccess(InteractQuery.RequestingAvatar.Get()))
	{
		InteractionBuilder.AddInteractionOption(OpenStationOption);
	}
}

URpgBaseStorageComponent* URpgBaseStorageStationComponent::GetBaseStorage() const
{
	return LinkedBaseCamp ? LinkedBaseCamp->GetBaseStorageComponent() : nullptr;
}

URpgInventoryManagerComponent* URpgBaseStorageStationComponent::GetArmoryInventory() const
{
	return LinkedBaseCamp ? LinkedBaseCamp->GetArmoryInventoryComponent() : nullptr;
}

TArray<TSubclassOf<URpgInventoryItemDefinition>> URpgBaseStorageStationComponent::GetAllowedResourceDefinitions() const
{
	TArray<TSubclassOf<URpgInventoryItemDefinition>> Results;
	if (StationMode == ERpgBaseStorageStationMode::Terminal)
	{
		return Results;
	}

	for (TSubclassOf<URpgInventoryItemDefinition> ItemDefinition : AllowedResourceDefinitions)
	{
		if (ItemDefinition)
		{
			Results.AddUnique(ItemDefinition);
		}
	}

	if (Results.Num() == 0)
	{
		for (const FRpgBaseResourceCapacity& Bonus : CapacityBonuses)
		{
			if (Bonus.ItemDefinition)
			{
				Results.AddUnique(Bonus.ItemDefinition);
			}
		}

		for (const URpgBaseStorageUpgradeDefinition* UpgradeDefinition : InstalledUpgrades)
		{
			if (!UpgradeDefinition)
			{
				continue;
			}

			for (const FRpgBaseResourceCapacity& Bonus : UpgradeDefinition->CapacityBonuses)
			{
				if (Bonus.ItemDefinition)
				{
					Results.AddUnique(Bonus.ItemDefinition);
				}
			}
		}
	}

	return Results;
}

bool URpgBaseStorageStationComponent::AllowsResourceDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	if (!ItemDefinition)
	{
		return false;
	}

	if (StationMode == ERpgBaseStorageStationMode::Terminal)
	{
		return true;
	}

	return GetAllowedResourceDefinitions().Contains(ItemDefinition);
}

bool URpgBaseStorageStationComponent::CanActorAccess(const AActor* RequestingActor) const
{
	const AActor* OwnerActor = GetOwner();
	if (!bAccessible || !LinkedBaseCamp || !OwnerActor || !RequestingActor)
	{
		return false;
	}

	const APawn* RequestingPawn = Cast<APawn>(RequestingActor);
	const AController* RequestingController = Cast<AController>(RequestingActor);
	if (!RequestingController && RequestingPawn)
	{
		RequestingController = RequestingPawn->GetController();
	}

	if (!RequestingController || !RequestingController->IsPlayerController())
	{
		return false;
	}

	if (InteractionRadius <= 0.0f)
	{
		return true;
	}

	return FVector::DistSquared(OwnerActor->GetActorLocation(), RequestingActor->GetActorLocation()) <= FMath::Square(InteractionRadius);
}

void URpgBaseStorageStationComponent::SetStationAccessible(bool bNewAccessible)
{
	if (AActor* OwnerActor = GetOwner(); OwnerActor && OwnerActor->HasAuthority())
	{
		bAccessible = bNewAccessible;
	}
}

TArray<URpgBaseStorageUpgradeDefinition*> URpgBaseStorageStationComponent::GetInstalledUpgrades() const
{
	TArray<URpgBaseStorageUpgradeDefinition*> Results;
	Results.Reserve(InstalledUpgrades.Num());
	for (URpgBaseStorageUpgradeDefinition* UpgradeDefinition : InstalledUpgrades)
	{
		if (UpgradeDefinition)
		{
			Results.Add(UpgradeDefinition);
		}
	}
	return Results;
}

bool URpgBaseStorageStationComponent::HasInstalledUpgrade(const URpgBaseStorageUpgradeDefinition* UpgradeDefinition) const
{
	return UpgradeDefinition && InstalledUpgrades.Contains(UpgradeDefinition);
}

bool URpgBaseStorageStationComponent::HasUpgradeTag(FGameplayTag UpgradeTag) const
{
	return UpgradeTag.IsValid() && GetGrantedUpgradeTags().HasTagExact(UpgradeTag);
}

FGameplayTagContainer URpgBaseStorageStationComponent::GetGrantedUpgradeTags() const
{
	FGameplayTagContainer GrantedTags;
	for (const URpgBaseStorageUpgradeDefinition* UpgradeDefinition : InstalledUpgrades)
	{
		if (UpgradeDefinition)
		{
			GrantedTags.AppendTags(UpgradeDefinition->GrantedUpgradeTags);
		}
	}
	return GrantedTags;
}

bool URpgBaseStorageStationComponent::CanInstallUpgrade(const URpgBaseStorageUpgradeDefinition* UpgradeDefinition) const
{
	if (!UpgradeDefinition || HasInstalledUpgrade(UpgradeDefinition))
	{
		return false;
	}

	if (!UpgradeDefinition->AllowedStationTags.IsEmpty() && !StationTags.HasAny(UpgradeDefinition->AllowedStationTags))
	{
		return false;
	}

	return true;
}

bool URpgBaseStorageStationComponent::InstallUpgrade(URpgBaseStorageUpgradeDefinition* UpgradeDefinition)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !CanInstallUpgrade(UpgradeDefinition))
	{
		return false;
	}

	InstalledUpgrades.Add(UpgradeDefinition);
	if (bCapacityBonusesApplied)
	{
		ApplyCapacityList(UpgradeDefinition->CapacityBonuses, 1);
	}

	OwnerActor->ForceNetUpdate();
	if (LinkedBaseCamp)
	{
		LinkedBaseCamp->ForceNetUpdate();
	}
	OnInstalledUpgradesChanged.Broadcast(this);
	return true;
}

void URpgBaseStorageStationComponent::ApplyCapacityBonuses(int32 Sign)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	if (Sign > 0 && bCapacityBonusesApplied)
	{
		return;
	}

	if (Sign < 0 && !bCapacityBonusesApplied)
	{
		return;
	}

	if (!GetBaseStorage())
	{
		return;
	}

	ApplyCapacityList(CapacityBonuses, Sign);
	for (const URpgBaseStorageUpgradeDefinition* UpgradeDefinition : InstalledUpgrades)
	{
		if (UpgradeDefinition)
		{
			ApplyCapacityList(UpgradeDefinition->CapacityBonuses, Sign);
		}
	}

	bCapacityBonusesApplied = Sign > 0;
}

void URpgBaseStorageStationComponent::ApplyCapacityList(const TArray<FRpgBaseResourceCapacity>& Bonuses, int32 Sign)
{
	URpgBaseStorageComponent* BaseStorage = GetBaseStorage();
	if (!BaseStorage)
	{
		return;
	}

	for (const FRpgBaseResourceCapacity& Bonus : Bonuses)
	{
		BaseStorage->AddResourceCapacity(Bonus.ItemDefinition, Bonus.Capacity * Sign);
	}
}

void URpgBaseStorageStationComponent::RegisterWithLinkedBaseCamp()
{
	ARpgBaseCampActor* CurrentBaseCamp = LinkedBaseCamp;
	if (RegisteredBaseCamp.Get() == CurrentBaseCamp)
	{
		return;
	}

	UnregisterFromLinkedBaseCamp();
	if (CurrentBaseCamp)
	{
		CurrentBaseCamp->RegisterStorageStation(this);
		RegisteredBaseCamp = CurrentBaseCamp;
	}
}

void URpgBaseStorageStationComponent::UnregisterFromLinkedBaseCamp()
{
	if (ARpgBaseCampActor* ExistingBaseCamp = RegisteredBaseCamp.Get())
	{
		ExistingBaseCamp->UnregisterStorageStation(this);
	}
	RegisteredBaseCamp.Reset();
}
