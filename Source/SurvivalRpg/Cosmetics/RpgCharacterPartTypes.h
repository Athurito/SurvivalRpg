// Ported from Lyra's LyraCharacterPartTypes.h (Epic Games). Character Parts are cosmetic: they never carry gameplay truth.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "RpgCharacterPartTypes.generated.h"

class AActor;
struct FRpgCharacterPartList;

/** How collision is handled on the primitive components of a spawned character part. */
UENUM(BlueprintType)
enum class ERpgCharacterPartCollisionMode : uint8
{
	/** Disable collision on the spawned part actor. Parts are cosmetic, so this is the default. */
	NoCollision,

	/** Keep whatever collision the part actor's components define. */
	UseCollisionFromCharacterPart
};

/**
 * Opaque handle to a part added to a pawn's character parts component.
 * Only valid on the authority that added the part; pass it back to remove exactly that part.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgCharacterPartHandle
{
	GENERATED_BODY()

	void Reset()
	{
		PartHandle = INDEX_NONE;
	}

	bool IsValid() const
	{
		return PartHandle != INDEX_NONE;
	}

private:
	UPROPERTY()
	int32 PartHandle = INDEX_NONE;

	friend FRpgCharacterPartList;
};

/**
 * Designer-authored description of one cosmetic character part.
 *
 * Every non-dedicated machine spawns PartClass locally as a child actor attached to the pawn's gameplay mesh.
 * The part class must not replicate; equipment data validation enforces this for equipment-granted parts.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgCharacterPart
{
	GENERATED_BODY()

	/** Actor class spawned for this part, usually an ARpgTaggedActor Blueprint holding skeletal meshes. Must not replicate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	TSubclassOf<AActor> PartClass;

	/** Socket on the pawn's gameplay mesh to attach the part to. None attaches to the mesh origin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	FName SocketName;

	/** How to handle collision on the part's primitive components. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetics")
	ERpgCharacterPartCollisionMode CollisionMode = ERpgCharacterPartCollisionMode::NoCollision;

	/** Compares two parts by class and socket, ignoring the collision mode. */
	static bool AreEquivalentParts(const FRpgCharacterPart& A, const FRpgCharacterPart& B)
	{
		return (A.PartClass == B.PartClass) && (A.SocketName == B.SocketName);
	}
};
