// Ported from Lyra's LyraTaggedActor.h (Epic Games).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "RpgTaggedActor.generated.h"

/**
 * Actor that exposes designer-authored static gameplay tags through IGameplayTagAssetInterface.
 *
 * Base class for character part Blueprints: the pawn's character parts component collects these tags
 * (for example Cosmetic.Body or Cosmetic.Cover.*) to pick the gameplay mesh and drive cosmetic reactions.
 * Blueprint cannot implement IGameplayTagAssetInterface, which is why this base is native.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API ARpgTaggedActor : public AActor, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:
	ARpgTaggedActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~IGameplayTagAssetInterface
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	//~End of IGameplayTagAssetInterface

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif

protected:
	/** Designer-authored tags owned by this actor. Static data; never replicated or changed at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actor")
	FGameplayTagContainer StaticGameplayTags;
};
