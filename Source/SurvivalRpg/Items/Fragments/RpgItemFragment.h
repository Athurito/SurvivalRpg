#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RpgItemFragment.generated.h"

class URpgItemInstance;

UCLASS(DefaultToInstanced, EditInlineNew, Abstract, BlueprintType)
class SURVIVALRPG_API URpgItemFragment : public UObject
{
	GENERATED_BODY()

public:
	virtual void OnInstanceCreated(URpgItemInstance* Instance) const {}
};
