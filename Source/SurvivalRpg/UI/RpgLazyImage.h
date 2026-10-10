#pragma once

#include "CommonLazyImage.h"
#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgLazyImage.generated.h"

class UTexture2D;

/**
 * Lazy image with a one-argument texture setter, so MVVM can bind a soft texture straight to it.
 * The image collapses while the texture is null.
 */
UCLASS(meta = (DisplayName = "Lazy Image (MVVM)"))
class SURVIVALRPG_API URpgLazyImage : public UCommonLazyImage
{
	GENERATED_BODY()

public:
	/** Shows InTexture, or collapses the image when it is null. Binding target for MVVM. */
	UFUNCTION(BlueprintCallable, Category = "Lazy Image")
	void SetLazyTexture(TSoftObjectPtr<UTexture2D> InTexture);

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif
};
