#include "RpgAICharacter.h"

#include "SurvivalRpg/Progression/RpgExperienceRewardComponent.h"

ARpgAICharacter::ARpgAICharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ExperienceRewardComponent = CreateDefaultSubobject<URpgExperienceRewardComponent>(TEXT("ExperienceRewardComponent"));
}
