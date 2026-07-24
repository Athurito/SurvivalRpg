// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgInventoryManagerComponent.h"
#include "RpgInventoryItemInstance.h"

#include "Components/ActorComponent.h"
#include "Misc/AutomationTest.h"
#include "Net/UnrealNetwork.h"
#include "Templates/UnrealTypeTraits.h"
#include "UObject/UnrealType.h"

static_assert(
	TIsDerivedFrom<
		URpgInventoryManagerComponent,
		UActorComponent>::Value,
	"The RPG inventory manager must remain an actor component.");
static_assert(
	TStructOpsTypeTraits<
		FRpgInventoryList>::WithNetDeltaSerializer,
	"The replicated RPG inventory list must retain its FastArray net-delta serializer.");

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryManagerPublicReflectionContractTest,
	"SurvivalRpg.Inventory.Manager.Facade.PublicReflectionContract",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryManagerPublicReflectionContractTest::RunTest(
	const FString& Parameters)
{
	const UClass* ManagerClass =
		URpgInventoryManagerComponent::StaticClass();
	const URpgInventoryManagerComponent* ManagerCDO =
		GetDefault<URpgInventoryManagerComponent>();
	if (!TestNotNull(
			TEXT("The RPG inventory manager class exists"),
			ManagerClass) ||
		!TestNotNull(
			TEXT("The RPG inventory manager CDO exists"),
			ManagerCDO))
	{
		return false;
	}

	TestTrue(
		TEXT("The RPG inventory manager remains an actor component"),
		ManagerClass->IsChildOf(UActorComponent::StaticClass()));
	TestTrue(
		TEXT("The RPG inventory manager remains a BlueprintType"),
		ManagerClass->GetBoolMetaData(TEXT("BlueprintType")));
	TestTrue(
		TEXT("The RPG inventory manager replicates by default"),
		ManagerCDO->GetIsReplicated());

	const FName AuthorityOnlyFunctions[] = {
		FName(TEXT("SetCapacityMode")),
		FName(TEXT("GrantItemDefinition")),
		FName(TEXT("ConsumeItemById")),
	};
	for (const FName FunctionName : AuthorityOnlyFunctions)
	{
		const UFunction* Function =
			ManagerClass->FindFunctionByName(FunctionName);
		if (!TestNotNull(
				*FString::Printf(
					TEXT("%s remains reflected"),
					*FunctionName.ToString()),
				Function))
		{
			continue;
		}

		TestTrue(
			*FString::Printf(
				TEXT("%s remains BlueprintCallable"),
				*FunctionName.ToString()),
			Function->HasAnyFunctionFlags(
				FUNC_BlueprintCallable));
		TestTrue(
			*FString::Printf(
				TEXT("%s remains BlueprintAuthorityOnly"),
				*FunctionName.ToString()),
			Function->HasAnyFunctionFlags(
				FUNC_BlueprintAuthorityOnly));
	}

	const FName PureQueryFunctions[] = {
		FName(TEXT("IsCapacityUnlimited")),
		FName(TEXT("CanBootstrapItemInstance")),
		FName(TEXT("GetItemPlacement")),
	};
	for (const FName FunctionName : PureQueryFunctions)
	{
		const UFunction* Function =
			ManagerClass->FindFunctionByName(FunctionName);
		if (!TestNotNull(
				*FString::Printf(
					TEXT("%s remains reflected"),
					*FunctionName.ToString()),
				Function))
		{
			continue;
		}

		TestTrue(
			*FString::Printf(
				TEXT("%s remains BlueprintCallable"),
				*FunctionName.ToString()),
			Function->HasAnyFunctionFlags(
				FUNC_BlueprintCallable));
		TestTrue(
			*FString::Printf(
				TEXT("%s remains BlueprintPure"),
				*FunctionName.ToString()),
			Function->HasAnyFunctionFlags(
				FUNC_BlueprintPure));
	}

	const FName DeprecatedFunctions[] = {
		FName(TEXT("CanAddItemInstance")),
		FName(TEXT("PlanInventoryMutation")),
		FName(TEXT("ExecuteCrossInventoryTransfer")),
	};
	for (const FName FunctionName : DeprecatedFunctions)
	{
		const UFunction* Function =
			ManagerClass->FindFunctionByName(FunctionName);
		if (!TestNotNull(
				*FString::Printf(
					TEXT("%s remains reflected for Blueprint migration"),
					*FunctionName.ToString()),
				Function))
		{
			continue;
		}

		TestTrue(
			*FString::Printf(
				TEXT("%s remains marked DeprecatedFunction"),
				*FunctionName.ToString()),
			Function->HasMetaData(TEXT("DeprecatedFunction")));
		TestFalse(
			*FString::Printf(
				TEXT("%s retains an actionable deprecation message"),
				*FunctionName.ToString()),
			Function->GetMetaData(
				TEXT("DeprecationMessage")).IsEmpty());
	}

	int32 ManagerNetFunctionCount = 0;
	for (TFieldIterator<UFunction> FunctionIt(
			 ManagerClass,
			 EFieldIteratorFlags::ExcludeSuper);
		 FunctionIt;
		 ++FunctionIt)
	{
		const UFunction* Function = *FunctionIt;
		if (Function &&
			Function->HasAnyFunctionFlags(FUNC_Net))
		{
			++ManagerNetFunctionCount;
			AddError(
				FString::Printf(
					TEXT(
						"The manager must not own an RPC, but %s is marked Net"),
					*Function->GetName()));
		}
	}
	TestEqual(
		TEXT("The manager owns no Net RPCs"),
		ManagerNetFunctionCount,
		0);

	const FName NativeOnlyFunctions[] = {
		FName(TEXT("EvaluatePlacement")),
		FName(TEXT("PlanMoveItem")),
		FName(TEXT("MoveItem")),
		FName(TEXT("PlanEquipmentMove")),
		FName(TEXT("MoveEquipmentItem")),
		FName(TEXT("TransferItem")),
		FName(TEXT("PickupItem")),
		FName(TEXT("CollectRootItemsBatch")),
		FName(TEXT("PlanDropItem")),
		FName(TEXT("DropItem")),
		FName(TEXT("ConvertLegacyInventorySnapshot")),
		FName(TEXT("RestoreInventoryGraph")),
		FName(TEXT("RestoreRuntimeCheckpoint")),
	};
	for (const FName FunctionName : NativeOnlyFunctions)
	{
		TestNull(
			*FString::Printf(
				TEXT("%s remains native-only"),
				*FunctionName.ToString()),
			ManagerClass->FindFunctionByName(FunctionName));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryManagerReplicationDescriptorContractTest,
	"SurvivalRpg.Inventory.Replication.ManagerPropertyAndSubobjectDescriptor",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryManagerReplicationDescriptorContractTest::RunTest(
	const FString& Parameters)
{
	auto VerifyLifetimeCondition =
		[this](
			const UObject* Object,
			const TArray<FLifetimeProperty>& LifetimeProperties,
			FName PropertyName,
			ELifetimeCondition ExpectedCondition)
		{
			if (!Object)
			{
				AddError(
					TEXT("A replication-contract CDO is missing"));
				return false;
			}

			const FProperty* Property =
				FindFProperty<FProperty>(
					Object->GetClass(),
					PropertyName);
			if (!TestNotNull(
					*FString::Printf(
						TEXT("Replicated property %s.%s exists"),
						*Object->GetClass()->GetName(),
						*PropertyName.ToString()),
					Property))
			{
				return false;
			}
			TestTrue(
				*FString::Printf(
					TEXT("%s.%s carries CPF_Net"),
					*Object->GetClass()->GetName(),
					*PropertyName.ToString()),
				Property->HasAnyPropertyFlags(CPF_Net));

			const FLifetimeProperty* LifetimeProperty =
				LifetimeProperties.FindByPredicate(
					[Property](
						const FLifetimeProperty& Candidate)
					{
						return Candidate.RepIndex ==
							Property->RepIndex;
					});
			if (!TestNotNull(
					*FString::Printf(
						TEXT(
							"%s.%s has a lifetime replication record"),
						*Object->GetClass()->GetName(),
						*PropertyName.ToString()),
					LifetimeProperty))
			{
				return false;
			}

			TestEqual(
				*FString::Printf(
					TEXT(
						"%s.%s uses the intended replication condition"),
					*Object->GetClass()->GetName(),
					*PropertyName.ToString()),
				static_cast<int32>(
					LifetimeProperty->Condition),
				static_cast<int32>(ExpectedCondition));
			return true;
		};

	auto VerifyRepNotify =
		[this](
			const UClass* Class,
			FName PropertyName,
			FName ExpectedNotifyFunction)
		{
			const FProperty* Property =
				FindFProperty<FProperty>(
					Class,
					PropertyName);
			if (!TestNotNull(
					*FString::Printf(
						TEXT("RepNotify property %s.%s exists"),
						*Class->GetName(),
						*PropertyName.ToString()),
					Property))
			{
				return false;
			}

			TestTrue(
				*FString::Printf(
					TEXT("%s.%s carries CPF_RepNotify"),
					*Class->GetName(),
					*PropertyName.ToString()),
				Property->HasAnyPropertyFlags(
					CPF_RepNotify));
			TestEqual(
				*FString::Printf(
					TEXT("%s.%s retains its RepNotify function"),
					*Class->GetName(),
					*PropertyName.ToString()),
				Property->RepNotifyFunc,
				ExpectedNotifyFunction);
			return true;
		};

	const URpgInventoryManagerComponent* ManagerCDO =
		GetDefault<URpgInventoryManagerComponent>();
	const URpgInventoryItemInstance* ItemInstanceCDO =
		GetDefault<URpgInventoryItemInstance>();
	if (!TestNotNull(
			TEXT("The RPG inventory manager CDO exists"),
			ManagerCDO) ||
		!TestNotNull(
			TEXT("The RPG inventory item-instance CDO exists"),
			ItemInstanceCDO))
	{
		return false;
	}

	ManagerCDO->GetClass()->SetUpRuntimeReplicationData();
	ItemInstanceCDO->GetClass()->SetUpRuntimeReplicationData();
	TArray<FLifetimeProperty> ManagerLifetimeProperties;
	TArray<FLifetimeProperty> ItemLifetimeProperties;
	ManagerCDO->GetLifetimeReplicatedProps(
		ManagerLifetimeProperties);
	ItemInstanceCDO->GetLifetimeReplicatedProps(
		ItemLifetimeProperties);

	bool bContractResolved = true;
	bContractResolved &= VerifyLifetimeCondition(
		ManagerCDO,
		ManagerLifetimeProperties,
		FName(TEXT("CapacityMode")),
		COND_None);
	bContractResolved &= VerifyLifetimeCondition(
		ManagerCDO,
		ManagerLifetimeProperties,
		FName(TEXT("FixedMaxEntries")),
		COND_None);
	bContractResolved &= VerifyLifetimeCondition(
		ManagerCDO,
		ManagerLifetimeProperties,
		FName(TEXT("DefaultGridSize")),
		COND_None);
	bContractResolved &= VerifyLifetimeCondition(
		ManagerCDO,
		ManagerLifetimeProperties,
		FName(TEXT("DefaultContainerId")),
		COND_None);
	bContractResolved &= VerifyLifetimeCondition(
		ManagerCDO,
		ManagerLifetimeProperties,
		FName(TEXT("InventoryList")),
		COND_Dynamic);
	bContractResolved &= VerifyLifetimeCondition(
		ManagerCDO,
		ManagerLifetimeProperties,
		FName(TEXT("InventoryRevision")),
		COND_Dynamic);

	const UClass* ManagerClass = ManagerCDO->GetClass();
	bContractResolved &= VerifyRepNotify(
		ManagerClass,
		FName(TEXT("CapacityMode")),
		FName(TEXT("OnRep_CapacitySettings")));
	bContractResolved &= VerifyRepNotify(
		ManagerClass,
		FName(TEXT("FixedMaxEntries")),
		FName(TEXT("OnRep_CapacitySettings")));
	bContractResolved &= VerifyRepNotify(
		ManagerClass,
		FName(TEXT("DefaultGridSize")),
		FName(TEXT("OnRep_CapacitySettings")));
	bContractResolved &= VerifyRepNotify(
		ManagerClass,
		FName(TEXT("DefaultContainerId")),
		FName(TEXT("OnRep_CapacitySettings")));
	bContractResolved &= VerifyRepNotify(
		ManagerClass,
		FName(TEXT("InventoryRevision")),
		FName(TEXT("OnRep_InventoryRevision")));

	const FStructProperty* InventoryListProperty =
		FindFProperty<FStructProperty>(
			ManagerClass,
			FName(TEXT("InventoryList")));
	if (TestNotNull(
			TEXT("InventoryList remains a reflected struct property"),
			InventoryListProperty))
	{
		TestEqual(
			TEXT(
				"InventoryList retains the FastArray-backed FRpgInventoryList descriptor"),
			InventoryListProperty->Struct.Get(),
			FRpgInventoryList::StaticStruct());
		TestFalse(
			TEXT("InventoryList does not use a redundant RepNotify"),
			InventoryListProperty->HasAnyPropertyFlags(
				CPF_RepNotify));
	}
	else
	{
		bContractResolved = false;
	}

	const FProperty* ReplicationPolicyProperty =
		FindFProperty<FProperty>(
			ManagerClass,
			FName(TEXT("ReplicationPolicy")));
	if (TestNotNull(
			TEXT(
				"ReplicationPolicy remains reflected for construction-time configuration"),
			ReplicationPolicyProperty))
	{
		TestFalse(
			TEXT(
				"ReplicationPolicy remains static configuration rather than replicated state"),
			ReplicationPolicyProperty->HasAnyPropertyFlags(
				CPF_Net));
	}
	else
	{
		bContractResolved = false;
	}

	bContractResolved &= VerifyLifetimeCondition(
		ItemInstanceCDO,
		ItemLifetimeProperties,
		FName(TEXT("ItemId")),
		COND_None);
	bContractResolved &= VerifyLifetimeCondition(
		ItemInstanceCDO,
		ItemLifetimeProperties,
		FName(TEXT("StatTags")),
		COND_None);
	bContractResolved &= VerifyLifetimeCondition(
		ItemInstanceCDO,
		ItemLifetimeProperties,
		FName(TEXT("ItemDef")),
		COND_None);
	TestTrue(
		TEXT(
			"Inventory item instances support replicated-subobject networking"),
		ItemInstanceCDO->IsSupportedForNetworking());

	const FProperty* ItemIdProperty =
		FindFProperty<FProperty>(
			ItemInstanceCDO->GetClass(),
			FName(TEXT("ItemId")));
	if (TestNotNull(
			TEXT("ItemId remains reflected"),
			ItemIdProperty))
	{
		TestTrue(
			TEXT("ItemId remains directly marked SaveGame"),
			ItemIdProperty->HasAnyPropertyFlags(CPF_SaveGame));
	}
	else
	{
		bContractResolved = false;
	}

	const FName RuntimePayloadProperties[] = {
		FName(TEXT("StatTags")),
		FName(TEXT("ItemDef")),
	};
	for (const FName PropertyName : RuntimePayloadProperties)
	{
		const FProperty* Property =
			FindFProperty<FProperty>(
				ItemInstanceCDO->GetClass(),
				PropertyName);
		if (!TestNotNull(
				*FString::Printf(
					TEXT("%s remains reflected"),
					*PropertyName.ToString()),
				Property))
		{
			bContractResolved = false;
			continue;
		}

		TestFalse(
			*FString::Printf(
				TEXT(
					"%s remains persisted through the graph DTO rather than direct UObject SaveGame serialization"),
				*PropertyName.ToString()),
			Property->HasAnyPropertyFlags(CPF_SaveGame));
	}

	return bContractResolved;
}

#endif // WITH_DEV_AUTOMATION_TESTS
