#pragma once

#include "CoreMinimal.h"
#include "RpgWorldSaveParticipant.h"
#include "UObject/Object.h"

#include "RpgWorldSaveAutomationTestTypes.generated.h"

/** Test-only world save participant whose durable state is one byte. */
UCLASS(NotBlueprintable, Transient)
class URpgWorldSaveAutomationTestParticipant final : public UObject, public IRpgWorldSaveParticipant
{
	GENERATED_BODY()

public:
	FName FeatureId = TEXT("AutomationFeature");
	uint8 Value = 0;
	int32 RestoreCount = 0;
	bool bRestoredFromEntry = false;
	bool bFailRestore = false;

	virtual FName GetWorldSaveFeatureId() const override
	{
		return FeatureId;
	}

	virtual bool CaptureWorldSaveData(FRpgWorldFeatureSaveData& OutData) override
	{
		OutData.SchemaVersion = 1;
		OutData.Payload = {Value};
		return true;
	}

	virtual bool RestoreWorldSaveData(const FRpgWorldFeatureSaveData* SavedData) override
	{
		++RestoreCount;
		bRestoredFromEntry = SavedData != nullptr;
		Value = SavedData && SavedData->Payload.Num() == 1 ? SavedData->Payload[0] : 0;
		return !bFailRestore;
	}
};
