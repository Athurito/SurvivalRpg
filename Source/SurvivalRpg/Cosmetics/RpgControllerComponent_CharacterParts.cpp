#include "RpgControllerComponent_CharacterParts.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "RpgPawnComponent_CharacterParts.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgControllerComponent_CharacterParts)

URpgControllerComponent_CharacterParts::URpgControllerComponent_CharacterParts(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URpgControllerComponent_CharacterParts::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		if (AController* OwningController = GetController<AController>())
		{
			OwningController->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::OnPossessedPawnChanged);

			if (APawn* ControlledPawn = GetPawn<APawn>())
			{
				OnPossessedPawnChanged(nullptr, ControlledPawn);
			}
		}
	}
}

void URpgControllerComponent_CharacterParts::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveAllCharacterParts();
	Super::EndPlay(EndPlayReason);
}

URpgPawnComponent_CharacterParts* URpgControllerComponent_CharacterParts::GetPawnCustomizer() const
{
	if (const APawn* ControlledPawn = GetPawn<APawn>())
	{
		return ControlledPawn->FindComponentByClass<URpgPawnComponent_CharacterParts>();
	}

	return nullptr;
}

void URpgControllerComponent_CharacterParts::AddCharacterPart(const FRpgCharacterPart& NewPart)
{
	FRpgControllerCharacterPartEntry& NewEntry = CharacterParts.AddDefaulted_GetRef();
	NewEntry.Part = NewPart;

	if (URpgPawnComponent_CharacterParts* PawnCustomizer = GetPawnCustomizer())
	{
		NewEntry.Handle = PawnCustomizer->AddCharacterPart(NewPart);
	}
}

void URpgControllerComponent_CharacterParts::RemoveCharacterPart(const FRpgCharacterPart& PartToRemove)
{
	for (auto EntryIt = CharacterParts.CreateIterator(); EntryIt; ++EntryIt)
	{
		if (FRpgCharacterPart::AreEquivalentParts(EntryIt->Part, PartToRemove))
		{
			if (URpgPawnComponent_CharacterParts* PawnCustomizer = GetPawnCustomizer())
			{
				PawnCustomizer->RemoveCharacterPart(EntryIt->Handle);
			}

			EntryIt.RemoveCurrent();
			break;
		}
	}
}

void URpgControllerComponent_CharacterParts::RemoveAllCharacterParts()
{
	if (URpgPawnComponent_CharacterParts* PawnCustomizer = GetPawnCustomizer())
	{
		for (const FRpgControllerCharacterPartEntry& Entry : CharacterParts)
		{
			PawnCustomizer->RemoveCharacterPart(Entry.Handle);
		}
	}

	CharacterParts.Reset();
}

void URpgControllerComponent_CharacterParts::OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	if (URpgPawnComponent_CharacterParts* OldCustomizer = OldPawn ? OldPawn->FindComponentByClass<URpgPawnComponent_CharacterParts>() : nullptr)
	{
		for (FRpgControllerCharacterPartEntry& Entry : CharacterParts)
		{
			OldCustomizer->RemoveCharacterPart(Entry.Handle);
			Entry.Handle.Reset();
		}
	}

	if (URpgPawnComponent_CharacterParts* NewCustomizer = NewPawn ? NewPawn->FindComponentByClass<URpgPawnComponent_CharacterParts>() : nullptr)
	{
		for (FRpgControllerCharacterPartEntry& Entry : CharacterParts)
		{
			// May be called with a null OldPawn after BeginPlay already applied the parts.
			if (!Entry.Handle.IsValid())
			{
				Entry.Handle = NewCustomizer->AddCharacterPart(Entry.Part);
			}
		}
	}
}
