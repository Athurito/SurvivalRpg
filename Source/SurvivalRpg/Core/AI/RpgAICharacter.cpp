#include "RpgAICharacter.h"

#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Combat/RpgEnemyCombatLoadout.h"
#include "SurvivalRpg/Core/AI/RpgAIPlayerState.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/Progression/RpgExperienceRewardComponent.h"

ARpgAICharacter::ARpgAICharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CombatArchetypeComponent = CreateDefaultSubobject<URpgEnemyCombatArchetypeComponent>(TEXT("CombatArchetypeComponent"));
	CombatLoadoutComponent = CreateDefaultSubobject<URpgEnemyCombatLoadoutComponent>(TEXT("CombatLoadoutComponent"));
	ExperienceRewardComponent = CreateDefaultSubobject<URpgExperienceRewardComponent>(TEXT("ExperienceRewardComponent"));
}

void ARpgAICharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	ARpgAIPlayerState* AIPlayerState = Cast<ARpgAIPlayerState>(GetPlayerState());
	URpgPawnExtensionComponent* PawnExtension = URpgPawnExtensionComponent::FindPawnExtensionComponent(this);
	if (AIPlayerState && PawnExtension)
	{
		PawnExtension->InitializeAbilitySystemComponent(AIPlayerState->GetRpgAbilitySystemComponent(), AIPlayerState);
	}
}
