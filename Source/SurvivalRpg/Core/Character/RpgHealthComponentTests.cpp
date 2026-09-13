#include "RpgHealthComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "UObject/UnrealType.h"

namespace RpgHealthComponentTests
{
	class FScopedHealthWorld
	{
	public:
		FScopedHealthWorld()
		{
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}

		~FScopedHealthWorld()
		{
			GameInstance->Shutdown();
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
			GameInstance->RemoveFromRoot();
		}

		UWorld* GetWorld() const { return World; }

	private:
		TObjectPtr<UGameInstance> GameInstance;
		TObjectPtr<UWorld> World;
	};

	URpgAbilitySystemComponent* AddExternalASC(AActor* Owner, APawn* Avatar)
	{
		URpgAbilitySystemComponent* ASC = NewObject<URpgAbilitySystemComponent>(Owner, NAME_None, RF_Transient);
		Owner->AddInstanceComponent(ASC);
		ASC->RegisterComponent();
		ASC->AddAttributeSetSubobject(NewObject<URpgHealthSet>(Owner, NAME_None, RF_Transient));
		ASC->InitAbilityActorInfo(Owner, Avatar);
		return ASC;
	}

	URpgHealthComponent* AddHealth(APawn* Pawn)
	{
		URpgHealthComponent* Health = NewObject<URpgHealthComponent>(Pawn, NAME_None, RF_Transient);
		Pawn->AddInstanceComponent(Health);
		Health->RegisterComponent();
		return Health;
	}

	void CheckDeathTags(FAutomationTestBase& Test, const URpgAbilitySystemComponent* ASC, ERpgDeathState ExpectedState)
	{
		Test.TestEqual(TEXT("Dead gameplay gate follows the current avatar"),
			ASC->HasMatchingGameplayTag(RpgGameplayTags::State_Dead), ExpectedState != ERpgDeathState::NotDead);
		Test.TestEqual(TEXT("Death status follows the current avatar"),
			ASC->HasMatchingGameplayTag(RpgGameplayTags::Status_Death), ExpectedState != ERpgDeathState::NotDead);
		Test.TestEqual(TEXT("Dying status follows the current avatar's death phase"),
			ASC->HasMatchingGameplayTag(RpgGameplayTags::Status_Death_Dying), ExpectedState == ERpgDeathState::DeathStarted);
		Test.TestEqual(TEXT("Finished-death status follows the current avatar's death phase"),
			ASC->HasMatchingGameplayTag(RpgGameplayTags::Status_Death_Dead), ExpectedState == ERpgDeathState::DeathFinished);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHealthLateASCBindingTest,
	"SurvivalRpg.Health.Lifecycle.LateASCBindingReconstructsDeathTags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHealthLateASCBindingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHealthComponentTests;
	FScopedHealthWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	if (!TestNotNull(TEXT("Standalone lifecycle test world is available"), World)) return false;

	for (ERpgDeathState State : { ERpgDeathState::DeathStarted, ERpgDeathState::DeathFinished })
	{
		AActor* ASCOwner = World->SpawnActor<AActor>();
		APawn* Avatar = World->SpawnActor<APawn>();
		if (!TestNotNull(TEXT("External ASC owner exists"), ASCOwner)
			|| !TestNotNull(TEXT("Avatar exists"), Avatar)) return false;

		URpgHealthComponent* Health = AddHealth(Avatar);
		// Exercise the same public state transitions used by replication before the ASC becomes available.
		Health->StartDeath();
		if (State == ERpgDeathState::DeathFinished) Health->FinishDeath();
		URpgAbilitySystemComponent* ASC = AddExternalASC(ASCOwner, Avatar);
		Health->InitializeWithAbilitySystem(ASC);

		TestEqual(TEXT("ASC availability preserves the avatar's existing death phase"), Health->GetDeathState(), State);
		CheckDeathTags(*this, ASC, State);
		Health->UninitializeFromAbilitySystem();
		CheckDeathTags(*this, ASC, ERpgDeathState::NotDead);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHealthAvatarReplacementIsolationTest,
	"SurvivalRpg.Health.Lifecycle.RetiredAvatarCannotChangeReplacementDeathTags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHealthAvatarReplacementIsolationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHealthComponentTests;
	FScopedHealthWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	if (!TestNotNull(TEXT("Standalone lifecycle test world is available"), World)) return false;

	AActor* ASCOwner = World->SpawnActor<AActor>();
	APawn* OldAvatar = World->SpawnActor<APawn>();
	APawn* NewAvatar = World->SpawnActor<APawn>();
	if (!TestNotNull(TEXT("External ASC owner exists"), ASCOwner)
		|| !TestNotNull(TEXT("Retiring avatar exists"), OldAvatar)
		|| !TestNotNull(TEXT("Replacement avatar exists"), NewAvatar)) return false;

	URpgAbilitySystemComponent* ASC = AddExternalASC(ASCOwner, OldAvatar);
	URpgHealthComponent* OldHealth = AddHealth(OldAvatar);
	OldHealth->InitializeWithAbilitySystem(ASC);
	OldHealth->StartDeath();
	CheckDeathTags(*this, ASC, ERpgDeathState::DeathStarted);

	ASC->InitAbilityActorInfo(ASCOwner, NewAvatar);
	URpgHealthComponent* NewHealth = AddHealth(NewAvatar);
	NewHealth->InitializeWithAbilitySystem(ASC);
	CheckDeathTags(*this, ASC, ERpgDeathState::NotDead);

	// Deliver a delayed replicated death-finish to the retired component without introducing a test subclass.
	FEnumProperty* DeathStateProperty = FindFProperty<FEnumProperty>(URpgHealthComponent::StaticClass(), TEXT("DeathState"));
	UFunction* RepNotify = OldHealth->FindFunction(TEXT("OnRep_DeathState"));
	if (!TestNotNull(TEXT("Death state is available for a simulated network delivery"), DeathStateProperty)
		|| !TestNotNull(TEXT("Death-state replication callback exists"), RepNotify)) return false;
	DeathStateProperty->GetUnderlyingProperty()->SetIntPropertyValue(
		DeathStateProperty->ContainerPtrToValuePtr<void>(OldHealth), static_cast<int64>(ERpgDeathState::DeathFinished));
	struct FDeathStateRepNotifyParameters
	{
		ERpgDeathState OldDeathState = ERpgDeathState::DeathStarted;
	} RepNotifyParameters;
	OldHealth->ProcessEvent(RepNotify, &RepNotifyParameters);
	TestEqual(TEXT("Retired avatar still completes its own replicated transition"),
		OldHealth->GetDeathState(), ERpgDeathState::DeathFinished);
	CheckDeathTags(*this, ASC, ERpgDeathState::NotDead);

	NewHealth->StartDeath();
	OldHealth->UninitializeFromAbilitySystem();
	OldHealth->DestroyComponent();
	CheckDeathTags(*this, ASC, ERpgDeathState::DeathStarted);

	// PawnExtension detaches the avatar before it broadcasts normal component cleanup.
	ASC->SetAvatarActor(nullptr);
	NewHealth->UninitializeFromAbilitySystem();
	CheckDeathTags(*this, ASC, ERpgDeathState::NotDead);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
