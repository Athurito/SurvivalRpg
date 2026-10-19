// Ported from Lyra's LyraControllerComponent_CharacterParts.h (Epic Games), without the cosmetic cheats.

#pragma once

#include "CoreMinimal.h"
#include "Components/ControllerComponent.h"
#include "RpgCharacterPartTypes.h"
#include "RpgControllerComponent_CharacterParts.generated.h"

class APawn;
class URpgPawnComponent_CharacterParts;

/** A character part requested by a controller, with the handle of its application to the current pawn. */
USTRUCT()
struct FRpgControllerCharacterPartEntry
{
	GENERATED_BODY()

	/** The part to apply to every pawn this controller possesses. */
	UPROPERTY(EditAnywhere, Category = "Cosmetics", meta = (ShowOnlyInnerProperties))
	FRpgCharacterPart Part;

	/** Handle of the part on the currently possessed pawn (authority only). */
	FRpgCharacterPartHandle Handle;
};

/**
 * Keeps the character parts a controller wants (for example the chosen body) and applies them to each pawn it possesses.
 *
 * Server-authoritative: parts are added on the authority and re-applied on possession, so respawned pawns keep the
 * same look. This is the seam a later character editor writes its choices into. Added to controllers through an
 * Experience AddComponents action or a Blueprint subclass with a preset part list.
 */
UCLASS(ClassGroup = (Cosmetics), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgControllerComponent_CharacterParts : public UControllerComponent
{
	GENERATED_BODY()

public:
	URpgControllerComponent_CharacterParts(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of UActorComponent interface

	/** Adds a part to this controller and its current pawn. Authority only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Cosmetics")
	void AddCharacterPart(const FRpgCharacterPart& NewPart);

	/** Removes the first equivalent part from this controller and its current pawn. Authority only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Cosmetics")
	void RemoveCharacterPart(const FRpgCharacterPart& PartToRemove);

	/** Removes all parts from this controller and its current pawn. Authority only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Cosmetics")
	void RemoveAllCharacterParts();

protected:
	/** Designer-authored parts applied to every possessed pawn, for example the default player body. Authority-owned. */
	UPROPERTY(EditAnywhere, Category = "Cosmetics")
	TArray<FRpgControllerCharacterPartEntry> CharacterParts;

private:
	URpgPawnComponent_CharacterParts* GetPawnCustomizer() const;

	UFUNCTION()
	void OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);
};
