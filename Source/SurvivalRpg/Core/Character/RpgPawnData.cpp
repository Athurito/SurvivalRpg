// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgPawnData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPawnData)

#if WITH_EDITOR
EDataValidationResult URpgPawnData::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context);
	if (!RpgCharacterMovementRuntime::IsProfileRuntimeValid(MovementProfile))
	{
		Context.AddError(FText::FromString(
			TEXT("PawnData movement profile must contain finite physical values and ordered gait thresholds.")));
	}

	return Context.GetNumErrors() > 0
		? EDataValidationResult::Invalid
		: EDataValidationResult::Valid;
}
#endif
