#include "RpgGameModeBase.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "RpgWorldSaveAutomationTestTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgWorldFeatureSaveTest,
	"SurvivalRpg.Save.WorldSave.FeatureParticipants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgWorldFeatureSaveTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	struct FScopedWorld
	{
		UGameInstance* Instance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
		UWorld* World = nullptr;
		FScopedWorld() { Instance->AddToRoot(); Instance->InitializeStandalone(); World = Instance->GetWorld(); }
		~FScopedWorld()
		{
			Instance->Shutdown();
			if (World) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
			Instance->RemoveFromRoot();
		}
	} Scope;
	ARpgGameModeBase* Mode = Scope.World ? Scope.World->SpawnActor<ARpgGameModeBase>() : nullptr;
	if (!TestNotNull(TEXT("Persistence owner"), Mode))
	{
		return false;
	}
	Mode->bEnableDiskPersistence = false;

	URpgWorldSaveAutomationTestParticipant* Feature = NewObject<URpgWorldSaveAutomationTestParticipant>(Mode);
	Mode->RegisterWorldSaveParticipant(Feature);
	TestEqual(TEXT("Before selection, the whole-snapshot restore owns the participant"), Feature->RestoreCount, 0);

	FRpgWorldFeatureSaveData Saved;
	Saved.SchemaVersion = 1;
	Saved.Payload = {42};
	FRpgWorldFeatureSaveData Absent;
	Absent.SchemaVersion = 3;
	Absent.Payload = {7, 8};
	const FName AbsentFeature(TEXT("AbsentFeature"));
	Mode->WorldFeatureSaveDataMap = {{Feature->FeatureId, Saved}, {AbsentFeature, Absent}};
	TestTrue(TEXT("Selection restores every registered participant"), Mode->RestoreWorldSaveParticipants());
	TestEqual(TEXT("The participant receives its own entry"), static_cast<int32>(Feature->Value), 42);
	Mode->bWorldSaveCandidateSelectionComplete = true;

	URpgWorldSaveAutomationTestParticipant* Duplicate = NewObject<URpgWorldSaveAutomationTestParticipant>(Mode);
	AddExpectedError(TEXT("Refusing a second world save participant"), EAutomationExpectedErrorFlags::Contains, 1);
	Mode->RegisterWorldSaveParticipant(Duplicate);
	TestEqual(TEXT("A second participant for one feature is refused"), Duplicate->RestoreCount, 0);

	URpgWorldSaveAutomationTestParticipant* Late = NewObject<URpgWorldSaveAutomationTestParticipant>(Mode);
	Late->FeatureId = TEXT("LateFeature");
	Late->Value = 3;
	Mode->RegisterWorldSaveParticipant(Late);
	TestTrue(TEXT("After selection a participant is restored at once"), Late->RestoreCount == 1 && !Late->bRestoredFromEntry);

	Feature->Value = 5;
	Late->Value = 6;
	Mode->CaptureWorldSaveParticipants();
	const FRpgWorldFeatureSaveData* Captured = Mode->WorldFeatureSaveDataMap.Find(Feature->FeatureId);
	TestTrue(TEXT("Capture writes the participant's state"), Captured && Captured->Payload == TArray<uint8>{5});
	const FRpgWorldFeatureSaveData* Kept = Mode->WorldFeatureSaveDataMap.Find(AbsentFeature);
	TestTrue(
		TEXT("An inactive feature's entry stays unchanged"),
		Kept && Kept->SchemaVersion == 3 && Kept->Payload == Absent.Payload);

	URpgWorldSaveGame* Snapshot = Mode->BuildWorldSaveSnapshot();
	TArray<uint8> Bytes;
	if (!TestNotNull(TEXT("The snapshot is built"), Snapshot) ||
		!TestTrue(TEXT("The snapshot serializes"), UGameplayStatics::SaveGameToMemory(Snapshot, Bytes)))
	{
		return false;
	}
	const URpgWorldSaveGame* Loaded = Cast<URpgWorldSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	FString Error;
	if (!TestNotNull(TEXT("The snapshot deserializes"), Loaded) ||
		!TestTrue(TEXT("The loaded snapshot validates"), Loaded->ValidateForLoad(Error)))
	{
		AddInfo(Error);
		return false;
	}
	TestEqual(TEXT("Every feature entry is written"), Loaded->WorldFeatures.Num(), 3);
	const FRpgWorldFeatureSaveData* LoadedLate = Loaded->WorldFeatures.Find(Late->FeatureId);
	TestTrue(TEXT("Payloads survive the disk format"), LoadedLate && LoadedLate->Payload == TArray<uint8>{6});

	// A participant that ends play hands over its final state.
	Mode->DispatchBeginPlay();
	Feature->Value = 9;
	Mode->UnregisterWorldSaveParticipant(Feature);
	Feature->Value = 11;
	Mode->CaptureWorldSaveParticipants();
	Captured = Mode->WorldFeatureSaveDataMap.Find(Feature->FeatureId);
	TestTrue(TEXT("An unregistered participant keeps its final state"), Captured && Captured->Payload == TArray<uint8>{9});

	Late->bFailRestore = true;
	AddExpectedError(TEXT("restore failed"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("A failed feature restore rejects the snapshot"), Mode->RestoreWorldSaveParticipants());

	URpgWorldSaveGame* Invalid = NewObject<URpgWorldSaveGame>();
	Invalid->WorldFeatures.Add(NAME_None, Saved);
	TestFalse(TEXT("A feature entry without an id is rejected"), Invalid->ValidateForLoad(Error));
	Invalid->WorldFeatures.Reset();
	Invalid->WorldFeatures.Add(TEXT("Unversioned"), FRpgWorldFeatureSaveData());
	TestFalse(TEXT("A feature entry without a schema is rejected"), Invalid->ValidateForLoad(Error));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
