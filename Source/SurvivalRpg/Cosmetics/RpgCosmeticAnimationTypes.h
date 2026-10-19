// Ported from Lyra's LyraCosmeticAnimationTypes.h (Epic Games): only the body-style selection is used.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RpgCosmeticAnimationTypes.generated.h"

class UPhysicsAsset;
class USkeletalMesh;

/** One rule that picks a gameplay mesh when all of its cosmetic tags are present on the pawn's character parts. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgAnimBodyStyleSelectionEntry
{
	GENERATED_BODY()

	/** Gameplay mesh to use when the rule matches. Must share the gameplay mesh's skeleton so the AnimBP keeps running. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	TObjectPtr<USkeletalMesh> Mesh = nullptr;

	/** Cosmetic tags that must all be present on the pawn's parts for this rule to match. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics", meta = (Categories = "Cosmetic"))
	FGameplayTagContainer RequiredTags;
};

/**
 * Designer-tuned rules that choose the pawn's gameplay mesh from its character parts' cosmetic tags.
 * Typical use: swap to an invisible copy of the gameplay mesh while a visible body part is present.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgAnimBodyStyleSelectionSet
{
	GENERATED_BODY()

	/** Rules evaluated in order; the first match wins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics", meta = (TitleProperty = Mesh))
	TArray<FRpgAnimBodyStyleSelectionEntry> MeshRules;

	/** Mesh used when no rule matches. If unset, the pawn's gameplay mesh is left untouched. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	TObjectPtr<USkeletalMesh> DefaultMesh = nullptr;

	/** If set, always applied as the gameplay mesh's physics asset, independent of the selected mesh. */
	UPROPERTY(EditAnywhere, Category = "Cosmetics")
	TObjectPtr<UPhysicsAsset> ForcedPhysicsAsset = nullptr;

	/** Returns the first matching rule's mesh, otherwise DefaultMesh (which may be null). */
	USkeletalMesh* SelectBestBodyStyle(const FGameplayTagContainer& CosmeticTags) const;
};
