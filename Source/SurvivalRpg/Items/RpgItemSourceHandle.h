#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RpgItemSourceHandle.generated.h"

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgItemSourceHandle
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FName ProviderId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FString ExternalId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FGameplayTagContainer SourceTags;

	bool IsValid() const
	{
		return !ProviderId.IsNone() || !ExternalId.IsEmpty() || !SourceTags.IsEmpty();
	}
};
