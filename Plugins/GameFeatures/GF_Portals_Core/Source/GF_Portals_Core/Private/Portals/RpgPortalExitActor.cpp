#include "Portals/RpgPortalExitActor.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystem/Abilities/RpgGameplayAbility_ExitPortal.h"
#include "GameplayTags/RpgPortalGameplayTags.h"
#include "Portals/RpgPortalActor.h"
#include "Portals/RpgPortalEncounterDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPortalExitActor)

ARpgPortalExitActor::ARpgPortalExitActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	InteractionCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionCollision"));
	InteractionCollision->SetupAttachment(SceneRoot);
	InteractionCollision->InitSphereRadius(220.0f);
	InteractionCollision->SetCollisionProfileName(TEXT("Interactable_OverlapDynamic"));
	InteractionCollision->SetGenerateOverlapEvents(true);

	ExitMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExitMesh"));
	ExitMesh->SetupAttachment(SceneRoot);
	ExitMesh->SetCollisionProfileName(TEXT("Interactable_BlockDynamic"));

	ExitPortalAbilityClass = URpgGameplayAbility_ExitPortal::StaticClass();
}

void ARpgPortalExitActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, OwningPortal);
}

void ARpgPortalExitActor::GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& InteractionBuilder)
{
	if (!OwningPortal || !ExitPortalAbilityClass)
	{
		return;
	}

	FInteractionOption Option;
	Option.InteractionTag = RpgPortalGameplayTags::Rpg_Interaction_Action_Portal_Exit;
	Option.InteractionAbilityToGrant = ExitPortalAbilityClass;
	Option.Prompt.ActionText = OwningPortal->GetExitInteractionText();
	Option.Prompt.TargetText = OwningPortal->GetExitInteractionSubText();
	Option.Prompt.AwarenessRange = 1000.0f;
	Option.Prompt.FocusRange = 650.0f;
	Option.Prompt.InteractionRange = 300.0f;
	Option.Prompt.InteractionPriority = 80;
	Option.TargetRef.TargetActor = this;
	if (!OwningPortal->IsExitOpen())
	{
		Option.Availability = ERpgInteractionAvailability::Blocked;
		Option.Prompt.BlockedReason = NSLOCTEXT("RpgPortal", "PortalExitUnavailable", "The portal exit is not ready");
	}

	InteractionBuilder.AddInteractionOption(Option);
}

void ARpgPortalExitActor::CustomizeInteractionEventData(const FGameplayTag& InteractionEventTag, FGameplayEventData& InOutEventData)
{
	InOutEventData.Target = this;
	InOutEventData.OptionalObject = OwningPortal;
}

void ARpgPortalExitActor::ConfigureExitPortal(ARpgPortalActor* InOwningPortal)
{
	if (!HasAuthority())
	{
		return;
	}

	OwningPortal = InOwningPortal;
}

bool ARpgPortalExitActor::TryUseExitPortal(AActor* ExitingActor)
{
	return HasAuthority() && OwningPortal && OwningPortal->TryExitPortal(ExitingActor);
}
