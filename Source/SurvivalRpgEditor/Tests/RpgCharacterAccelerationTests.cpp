// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "SurvivalRpg/Core/Character/RpgCharacter.h"

#include "Engine/ReplicatedState.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/EngineNetworkCustomVersion.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"

namespace RpgCharacterAccelerationTests
{
	class FScopedMovementWorld
	{
	public:
		FScopedMovementWorld()
		{
			const UWorld::InitializationValues InitializationValues = UWorld::InitializationValues()
				.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::PIE, false, NAME_None, nullptr, true,
				ERHIFeatureLevel::Num, &InitializationValues);
		}

		~FScopedMovementWorld()
		{
			if (World)
			{
				World->DestroyWorld(false);
			}
		}

		ARpgCharacter* SpawnCharacter(ENetRole Role) const
		{
			if (!World)
			{
				return nullptr;
			}
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.ObjectFlags = RF_Transient;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ARpgCharacter* Character = World->SpawnActor<ARpgCharacter>(SpawnParameters);
			if (Character)
			{
				Character->SetRole(Role);
			}
			return Character;
		}

		// No map, GameMode, Experience, BeginPlay, network sockets, or save lifecycle.
		UWorld* World = nullptr;
	};

	bool RoundTripMovement(FRepMovement Source, FRepMovement& Received)
	{
		FNetBitWriter Writer(nullptr, 1024);
		Writer.SetEngineNetVer(FEngineNetworkCustomVersion::LatestVersion);
		bool bWriteSuccess = false;
		if (!Source.NetSerialize(Writer, nullptr, bWriteSuccess) || !bWriteSuccess || Writer.IsError())
		{
			return false;
		}

		// Quantization settings are class defaults, not fields transmitted in the packet.
		Received.LocationQuantizationLevel = Source.LocationQuantizationLevel;
		Received.VelocityQuantizationLevel = Source.VelocityQuantizationLevel;
		Received.RotationQuantizationLevel = Source.RotationQuantizationLevel;
		FNetBitReader Reader(nullptr, Writer.GetData(), Writer.GetNumBits());
		Reader.SetEngineNetVer(FEngineNetworkCustomVersion::LatestVersion);
		bool bReadSuccess = false;
		return Received.NetSerialize(Reader, nullptr, bReadSuccess) && bReadSuccess && !Reader.IsError()
			&& Reader.GetPosBits() == Writer.GetNumBits();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgAccelerationQuantizationTest,
	"SurvivalRpg.Movement.Acceleration.QuantizesAbsoluteMovementPacket",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgAccelerationQuantizationTest::RunTest(const FString& Parameters)
{
	for (const FVector Acceleration : {FVector::ZeroVector, FVector(2400.0, 0.0, 0.0),
		FVector(-1697.06, 1697.06, -127.23), FVector(0.49, -0.49, 500.74)})
	{
		FRepMovement Source;
		Source.bRepAcceleration = true;
		Source.Acceleration = Acceleration;
		FRepMovement Received;
		if (!TestTrue(TEXT("Current engine movement serializer completes a wire round trip"),
			RpgCharacterAccelerationTests::RoundTripMovement(Source, Received)))
		{
			return false;
		}
		TestTrue(TEXT("Packet retains the acceleration presence bit, including a zero vector"), Received.bRepAcceleration != 0);
		TestTrue(TEXT("Default whole-unit quantization stays within 0.5 cm/s^2 per axis"),
			Received.Acceleration.Equals(Acceleration, 0.50001));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgAccelerationProxyInputTest,
	"SurvivalRpg.Movement.Acceleration.AuthoritativeInputReachesSimulatedProxy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgAccelerationProxyInputTest::RunTest(const FString& Parameters)
{
	using namespace RpgCharacterAccelerationTests;
	// Prove the RPG override works even if the engine-wide opt-in is disabled.
	// Restore the referenced value on exit without changing console priorities or history.
	TGuardValue<int32> DisableGlobalOptIn(CharacterCVars::EnableCharacterAccelerationReplication, 0);
	FScopedMovementWorld Fixture;
	ARpgCharacter* Server = Fixture.SpawnCharacter(ROLE_Authority);
	ARpgCharacter* Proxy = Fixture.SpawnCharacter(ROLE_SimulatedProxy);
	const FStructProperty* AccelerationProperty = FindFProperty<FStructProperty>(
		UCharacterMovementComponent::StaticClass(), TEXT("Acceleration"));
	if (!TestNotNull(TEXT("Server character exists"), Server)
		|| !TestNotNull(TEXT("Simulated proxy exists"), Proxy)
		|| !TestNotNull(TEXT("Authoritative CMC input is accessible to the fixture"), AccelerationProperty))
	{
		return false;
	}
	TestFalse(TEXT("Fixture never begins gameplay"), Fixture.World->HasBegunPlay());
	TestNull(TEXT("Fixture never creates a GameMode"), Fixture.World->GetAuthGameMode());
	TestEqual(TEXT("Source represents server authority"), Server->GetLocalRole(), ROLE_Authority);
	TestEqual(TEXT("Receiver represents a simulated proxy"), Proxy->GetLocalRole(), ROLE_SimulatedProxy);

	UCharacterMovementComponent* ServerMovement = Server->GetCharacterMovement();
	UCharacterMovementComponent* ProxyMovement = Proxy->GetCharacterMovement();
	ServerMovement->MaxAcceleration = 2400.0f;
	ProxyMovement->MaxAcceleration = 500.0f;
	ProxyMovement->Velocity = FVector(300.0, 0.0, 0.0);

	// Exercise actual gather, packet serialization, and the engine's proxy update hook.
	// The second packet represents releasing input while still moving, not stopping velocity.
	for (const FVector AuthoritativeInput : {FVector(1200.0, -600.0, 100.0), FVector::ZeroVector})
	{
		*AccelerationProperty->ContainerPtrToValuePtr<FVector>(ServerMovement) = AuthoritativeInput;
		Server->GatherCurrentMovement();
		const FRepMovement& Gathered = Server->GetReplicatedMovement();
		TestTrue(TEXT("RPG character opts in to engine acceleration replication"), Gathered.bRepAcceleration != 0);
		TestEqual(TEXT("Gather reads authoritative CMC input"), Gathered.Acceleration, AuthoritativeInput);
		FRepMovement Received;
		if (!TestTrue(TEXT("Authoritative packet reaches the receiver"), RoundTripMovement(Gathered, Received)))
		{
			return false;
		}
		Proxy->SetReplicatedMovement(Received);
		ProxyMovement->UpdateProxyAcceleration();
		TestEqual(TEXT("Proxy retains absolute input despite different local MaxAcceleration"),
			ProxyMovement->GetCurrentAcceleration(), AuthoritativeInput);
		ProxyMovement->UpdateProxyAcceleration();
		TestEqual(TEXT("Subsequent proxy updates preserve input, including zero while coasting"),
			ProxyMovement->GetCurrentAcceleration(), AuthoritativeInput);
		TestEqual(TEXT("Presentation input update does not change proxy velocity"),
			ProxyMovement->Velocity, FVector(300.0, 0.0, 0.0));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgAccelerationReplicationScopeTest,
	"SurvivalRpg.Movement.Acceleration.UsesEngineSimulatedMovementReplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgAccelerationReplicationScopeTest::RunTest(const FString& Parameters)
{
	UClass* CharacterClass = ARpgCharacter::StaticClass();
	CharacterClass->SetUpRuntimeReplicationData();
	const FProperty* MovementProperty = FindFProperty<FProperty>(CharacterClass, TEXT("ReplicatedMovement"));
	if (!TestNotNull(TEXT("Character inherits engine movement replication"), MovementProperty))
	{
		return false;
	}
	TArray<FLifetimeProperty> LifetimeProperties;
	GetDefault<ARpgCharacter>()->GetLifetimeReplicatedProps(LifetimeProperties);
	const FLifetimeProperty* MovementLifetime = LifetimeProperties.FindByPredicate(
		[MovementProperty](const FLifetimeProperty& Property) { return Property.RepIndex == MovementProperty->RepIndex; });
	if (!TestNotNull(TEXT("Movement is registered for replication"), MovementLifetime))
	{
		return false;
	}
	TestEqual(TEXT("Ordinary autonomous owners retain predicted movement; engine physics exception is preserved"),
		MovementLifetime->Condition, COND_SimulatedOrPhysics);
	TestEqual(TEXT("Received movement retains the engine's unconditional notify policy"),
		MovementLifetime->RepNotifyCondition, REPNOTIFY_Always);
	return true;
}

#endif
