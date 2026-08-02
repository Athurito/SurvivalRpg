#include "RpgGameModeBase.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OnlineSubsystemTypes.h"
#include "SurvivalRpg/Base/RpgBaseCampActor.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgRemoteOfflineProfileTokenContractTest,
	"SurvivalRpg.Save.ProfileIdentity.RemoteOfflineTokenContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgRemoteOfflineProfileTokenContractTest::RunTest(
	const FString& Parameters)
{
	FString CanonicalToken;
	TestTrue(
		TEXT("Canonical UUID-v4 token is accepted"),
		ARpgGameModeBase::TryNormalizeRemoteOfflinePlayerProfileToken(
			TEXT("01234567-89ab-4cde-8f01-23456789abcd"),
			CanonicalToken));
	TestEqual(
		TEXT("Canonical UUID-v4 token remains stable"),
		CanonicalToken,
		FString(TEXT("01234567-89ab-4cde-8f01-23456789abcd")));

	CanonicalToken.Reset();
	TestTrue(
		TEXT("Uppercase UUID-v4 input normalizes to one collision key"),
		ARpgGameModeBase::TryNormalizeRemoteOfflinePlayerProfileToken(
			TEXT("01234567-89AB-4CDE-BF01-23456789ABCD"),
			CanonicalToken));
	TestEqual(
		TEXT("UUID-v4 normalization is lowercase"),
		CanonicalToken,
		FString(TEXT("01234567-89ab-4cde-bf01-23456789abcd")));

	const TArray<FString> InvalidTokens =
	{
		FString(),
		TEXT("friendly-profile-name"),
		TEXT("0123456789ab4cde8f0123456789abcd"),
		TEXT("01234567-89ab-3cde-8f01-23456789abcd"),
		TEXT("01234567-89ab-4cde-7f01-23456789abcd"),
		TEXT("{01234567-89ab-4cde-8f01-23456789abcd}")
	};
	for (const FString& InvalidToken : InvalidTokens)
	{
		CanonicalToken = TEXT("must-clear");
		TestFalse(
			*FString::Printf(
				TEXT("Invalid remote-offline token is rejected: %s"),
				*InvalidToken),
			ARpgGameModeBase::TryNormalizeRemoteOfflinePlayerProfileToken(
				InvalidToken,
				CanonicalToken));
		TestTrue(
			TEXT("Rejected token never leaves reusable identity output"),
			CanonicalToken.IsEmpty());
	}

	const FUniqueNetIdRef NullOssIdRef =
		FUniqueNetIdString::Create(
			TEXT("Athurito-0123456789ABCDEF"),
			TEXT("NULL"));
	const FUniqueNetIdRepl NullOssId(NullOssIdRef);
	TestTrue(
		TEXT("The synthetic NULL OSS id is structurally valid"),
		NullOssId.IsValid());
	TestFalse(
		TEXT("A process-local NULL OSS id cannot own durable save state"),
		ARpgGameModeBase::IsDurableOnlineProfileId(NullOssId));

	const FUniqueNetIdRef DurableOssIdRef =
		FUniqueNetIdString::Create(
			TEXT("stable-account-id"),
			TEXT("STEAM"));
	const FUniqueNetIdRepl DurableOssId(DurableOssIdRef);
	TestTrue(
		TEXT("A non-NULL online account id remains durable"),
		ARpgGameModeBase::IsDurableOnlineProfileId(DurableOssId));

	TestFalse(
		TEXT("Connection-scoped offline sessions can never enter a disk snapshot"),
		ARpgGameModeBase::IsPersistentPlayerProfileKey(
			TEXT("OfflineSession:LocalProfile:1")));
	TestTrue(
		TEXT("The stable local-host profile remains persistent"),
		ARpgGameModeBase::IsPersistentPlayerProfileKey(
			TEXT("Offline:LocalProfile")));
	TestTrue(
		TEXT("A UUID-backed remote offline profile remains persistent"),
		ARpgGameModeBase::IsPersistentPlayerProfileKey(
			TEXT("OfflinePlayer:01234567-89ab-4cde-8f01-23456789abcd")));

	const FProperty* OwnerProfileProperty = FindFProperty<FProperty>(
		ARpgBaseCampActor::StaticClass(),
		TEXT("OwnerProfileKey"));
	TestNotNull(
		TEXT("Base owner profile storage remains reflected for runtime lifetime tracking"),
		OwnerProfileProperty);
	if (OwnerProfileProperty)
	{
		TestFalse(
			TEXT("Base owner bearer identity is not a replicated property"),
			OwnerProfileProperty->HasAnyPropertyFlags(CPF_Net));
		TestTrue(
			TEXT("Base owner bearer identity cannot be serialized into placed map actors"),
			OwnerProfileProperty->HasAnyPropertyFlags(CPF_Transient));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
