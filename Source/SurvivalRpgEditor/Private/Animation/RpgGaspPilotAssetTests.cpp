#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AlphaBlend.h"
#include "Abilities/GameplayAbility.h"
#include "Animation/AnimGraphNode_RpgFootPlacement.h"
#include "AnimGraph/AnimGraphNode_OrientationWarping.h"
#include "AnimGraph/AnimGraphNode_Steering.h"
#include "AnimGraphNode_BlendStackInput.h"
#include "AnimGraphNode_BlendStackResult.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "AnimGraphNode_LegIK.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimSequence.h"
#include "Animation/InputScaleBias.h"
#include "AnimationBlendStackGraph.h"
#include "AnimGraphNode_MotionMatching.h"
#include "AnimGraphNode_PoseSearchHistoryCollector.h"
#include "AnimGraphNode_ResetRoot.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_Slot.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "BoneControllers/AnimNode_OrientationWarping.h"
#include "BoneControllers/AnimNode_OffsetRootBone.h"
#include "BoneControllers/AnimNode_ResetRoot.h"
#include "BoneControllers/AnimNode_Steering.h"
#include "Components/SkeletalMeshComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/MemberReference.h"
#include "GameFeatureAction.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "K2Node_AnimNodeReference.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "Modules/ModuleManager.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "PoseSearch/PoseSearchIndex.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "SurvivalRpg/Camera/RpgCameraMode.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/Input/RpgInputConfig.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UnrealType.h"

namespace RpgGaspPilotAssetTests
{
	constexpr TCHAR PilotAnimBlueprintObject[] =
		TEXT("/Game/SurvivalRpg/Characters/Mannequins/Anims/GASP/ABP_RpgCharacter_GASP.ABP_RpgCharacter_GASP");
	constexpr TCHAR BaseCharacterBlueprintObject[] =
		TEXT("/Game/SurvivalRpg/Core/Character/BP_Rpg_Character.BP_Rpg_Character");
	constexpr TCHAR PilotCharacterBlueprintObject[] =
		TEXT("/Game/SurvivalRpg/Core/Character/GASP/BP_Rpg_Character_GASP.BP_Rpg_Character_GASP");
	constexpr TCHAR BasePawnDataObject[] =
		TEXT("/Game/SurvivalRpg/Core/Character/DA_PawnData.DA_PawnData");
	constexpr TCHAR PilotPawnDataObject[] =
		TEXT("/Game/SurvivalRpg/Core/Character/GASP/DA_PawnData_GASP.DA_PawnData_GASP");
	constexpr TCHAR BaseExperienceObject[] =
		TEXT("/Game/SurvivalRpg/System/Experiences/RpgPrototypeExperience.RpgPrototypeExperience");
	constexpr TCHAR PilotExperienceObject[] =
		TEXT("/Game/SurvivalRpg/System/Experiences/RpgGaspPilotExperience.RpgGaspPilotExperience");
	constexpr TCHAR TargetSkeletonObject[] =
		TEXT("/Game/SurvivalRpg/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin");
	constexpr TCHAR TurnInPlaceDatabaseObject[] =
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_TurnInPlace.PSD_Rpg_Stand_TurnInPlace");
	constexpr TCHAR CombatStanceInputActionObject[] =
		TEXT("/GF_Combat_Core/Input/IA_ToggleCombatStance.IA_ToggleCombatStance");
	constexpr TCHAR CombatInputMappingContextObject[] =
		TEXT("/GF_Combat_Core/Input/IMC_Combat.IMC_Combat");
	constexpr TCHAR CombatInputConfigObject[] =
		TEXT("/GF_Combat_Core/Input/DA_InputConfig_Combat.DA_InputConfig_Combat");
	constexpr TCHAR BasicWeaponAttackAbilityObject[] =
		TEXT("/GF_Combat_Core/GAS/Abilities/GA_BasicWeaponAttack.GA_BasicWeaponAttack");
	constexpr TCHAR CombatBlockAbilityObject[] =
		TEXT("/GF_Combat_Core/GAS/Abilities/GA_Combat_Block.GA_Combat_Block");
	constexpr TCHAR CombatStrafeTagName[] = TEXT("State.Rotation.CombatStrafe");
	constexpr TCHAR ToggleCombatInputTagName[] = TEXT("InputTag.RotationMode.ToggleCombat");

	constexpr TCHAR PilotAnimBlueprintPackage[] =
		TEXT("/Game/SurvivalRpg/Characters/Mannequins/Anims/GASP/ABP_RpgCharacter_GASP");
	constexpr TCHAR PilotCharacterBlueprintPackage[] =
		TEXT("/Game/SurvivalRpg/Core/Character/GASP/BP_Rpg_Character_GASP");
	constexpr TCHAR PilotPawnDataPackage[] =
		TEXT("/Game/SurvivalRpg/Core/Character/GASP/DA_PawnData_GASP");
	constexpr TCHAR PilotExperiencePackage[] =
		TEXT("/Game/SurvivalRpg/System/Experiences/RpgGaspPilotExperience");

	template <typename TObjectType>
	TObjectType* LoadRequiredAsset(
		FAutomationTestBase& Test,
		const TCHAR* ObjectPath,
		const TCHAR* Description)
	{
		TObjectType* Asset = LoadObject<TObjectType>(nullptr, ObjectPath);
		Test.TestNotNull(Description, Asset);
		return Asset;
	}

	UEdGraph* FindAnimGraph(UAnimBlueprint* AnimBlueprint)
	{
		if (!AnimBlueprint)
		{
			return nullptr;
		}

		TArray<UEdGraph*> Graphs;
		AnimBlueprint->GetAllGraphs(Graphs);
		for (UEdGraph* Graph : Graphs)
		{
			if (Graph && Graph->GetFName() == TEXT("AnimGraph"))
			{
				return Graph;
			}
		}

		return nullptr;
	}

	template <typename TNodeType>
	TNodeType* FindUniqueNode(
		FAutomationTestBase& Test,
		const UEdGraph* Graph,
		const TCHAR* Description)
	{
		TArray<TNodeType*> Matches;
		if (Graph)
		{
			for (UEdGraphNode* GraphNode : Graph->Nodes)
			{
				if (TNodeType* Match = Cast<TNodeType>(GraphNode))
				{
					Matches.Add(Match);
				}
			}
		}

		Test.TestEqual(
			*FString::Printf(TEXT("%s occurs exactly once"), Description),
			Matches.Num(),
			1);
		return Matches.Num() == 1 ? Matches[0] : nullptr;
	}

	UEdGraphNode* FindUniqueNodeByClassName(
		FAutomationTestBase& Test,
		const UEdGraph* Graph,
		FName ClassName,
		const TCHAR* Description)
	{
		TArray<UEdGraphNode*> Matches;
		if (Graph)
		{
			for (UEdGraphNode* GraphNode : Graph->Nodes)
			{
				if (GraphNode && GraphNode->GetClass()->GetFName() == ClassName)
				{
					Matches.Add(GraphNode);
				}
			}
		}

		Test.TestEqual(
			*FString::Printf(TEXT("%s occurs exactly once"), Description),
			Matches.Num(),
			1);
		return Matches.Num() == 1 ? Matches[0] : nullptr;
	}

	UK2Node_VariableGet* FindUniqueVariableGetter(
		FAutomationTestBase& Test,
		const UEdGraph* Graph,
		FName VariableName)
	{
		TArray<UK2Node_VariableGet*> Matches;
		if (Graph)
		{
			for (UEdGraphNode* GraphNode : Graph->Nodes)
			{
				UK2Node_VariableGet* VariableGetter = Cast<UK2Node_VariableGet>(GraphNode);
				if (VariableGetter && VariableGetter->VariableReference.GetMemberName() == VariableName)
				{
					Matches.Add(VariableGetter);
				}
			}
		}

		Test.TestEqual(
			*FString::Printf(TEXT("%s getter occurs exactly once"), *VariableName.ToString()),
			Matches.Num(),
			1);
		return Matches.Num() == 1 ? Matches[0] : nullptr;
	}

	bool TestExclusiveLink(
		FAutomationTestBase& Test,
		const TCHAR* Description,
		const UEdGraphNode* SourceNode,
		FName SourcePinName,
		const UEdGraphNode* TargetNode,
		FName TargetPinName)
	{
		if (!SourceNode || !TargetNode)
		{
			Test.AddError(FString::Printf(TEXT("%s cannot be checked because a graph node is missing"), Description));
			return false;
		}

		const UEdGraphPin* SourcePin = SourceNode->FindPin(SourcePinName, EGPD_Output);
		const UEdGraphPin* TargetPin = TargetNode->FindPin(TargetPinName, EGPD_Input);
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("%s source pin %s exists"), Description, *SourcePinName.ToString()),
				SourcePin) ||
			!Test.TestNotNull(
				*FString::Printf(TEXT("%s target pin %s exists"), Description, *TargetPinName.ToString()),
				TargetPin))
		{
			return false;
		}

		const bool bLinkedExclusively =
			SourcePin->LinkedTo.Num() == 1 &&
			TargetPin->LinkedTo.Num() == 1 &&
			SourcePin->LinkedTo[0] == TargetPin &&
			TargetPin->LinkedTo[0] == SourcePin;
		Test.TestTrue(Description, bLinkedExclusively);
		return bLinkedExclusively;
	}

	struct FExpectedInputPin
	{
		const UEdGraphNode* Node = nullptr;
		FName PinName;
	};

	bool TestExactOutputLinks(
		FAutomationTestBase& Test,
		const TCHAR* Description,
		const UEdGraphNode* SourceNode,
		FName SourcePinName,
		const TArray<FExpectedInputPin>& ExpectedTargets)
	{
		if (!SourceNode)
		{
			Test.AddError(FString::Printf(TEXT("%s cannot be checked because the source node is missing"), Description));
			return false;
		}

		const UEdGraphPin* SourcePin = SourceNode->FindPin(SourcePinName, EGPD_Output);
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("%s source pin %s exists"), Description, *SourcePinName.ToString()),
				SourcePin))
		{
			return false;
		}

		bool bMatches = SourcePin->LinkedTo.Num() == ExpectedTargets.Num();
		for (const FExpectedInputPin& ExpectedTarget : ExpectedTargets)
		{
			if (!ExpectedTarget.Node)
			{
				Test.AddError(FString::Printf(TEXT("%s has a missing expected target node"), Description));
				bMatches = false;
				continue;
			}

			const UEdGraphPin* TargetPin =
				ExpectedTarget.Node->FindPin(ExpectedTarget.PinName, EGPD_Input);
			if (!Test.TestNotNull(
					*FString::Printf(
						TEXT("%s target pin %s exists"),
						Description,
						*ExpectedTarget.PinName.ToString()),
					TargetPin))
			{
				bMatches = false;
				continue;
			}

			bMatches = bMatches &&
				SourcePin->LinkedTo.Contains(TargetPin) &&
				TargetPin->LinkedTo.Num() == 1 &&
				TargetPin->LinkedTo[0] == SourcePin;
		}

		Test.TestTrue(Description, bMatches);
		return bMatches;
	}

	template <typename TRuntimeNode>
	const TRuntimeNode* ReadRuntimeNode(const UEdGraphNode* EditorNode)
	{
		const FStructProperty* NodeProperty =
			EditorNode ? FindFProperty<FStructProperty>(EditorNode->GetClass(), TEXT("Node")) : nullptr;
		if (!NodeProperty || NodeProperty->Struct != TRuntimeNode::StaticStruct())
		{
			return nullptr;
		}

		return NodeProperty->ContainerPtrToValuePtr<TRuntimeNode>(EditorNode);
	}

	template <typename TStruct>
	const TStruct* ReadStructPropertyValue(const UObject* Object, FName PropertyName)
	{
		const FStructProperty* Property =
			Object ? FindFProperty<FStructProperty>(Object->GetClass(), PropertyName) : nullptr;
		if (!Property || Property->Struct != TStruct::StaticStruct())
		{
			return nullptr;
		}

		return Property->ContainerPtrToValuePtr<TStruct>(Object);
	}

	bool ReadBoolProperty(const UObject* Object, FName PropertyName, bool& OutValue)
	{
		const FBoolProperty* Property =
			Object ? FindFProperty<FBoolProperty>(Object->GetClass(), PropertyName) : nullptr;
		if (!Property)
		{
			return false;
		}

		OutValue = Property->GetPropertyValue_InContainer(Object);
		return true;
	}

	bool ReadFloatProperty(const UObject* Object, FName PropertyName, float& OutValue)
	{
		const FFloatProperty* Property =
			Object ? FindFProperty<FFloatProperty>(Object->GetClass(), PropertyName) : nullptr;
		if (!Property)
		{
			return false;
		}

		OutValue = Property->GetPropertyValue_InContainer(Object);
		return true;
	}

	const FGameplayTagContainer* ReadGameplayTagContainerProperty(
		const UObject* Object,
		FName PropertyName)
	{
		const FStructProperty* Property =
			Object ? FindFProperty<FStructProperty>(Object->GetClass(), PropertyName) : nullptr;
		if (!Property || Property->Struct != FGameplayTagContainer::StaticStruct())
		{
			return nullptr;
		}

		return Property->ContainerPtrToValuePtr<FGameplayTagContainer>(Object);
	}

	bool ReadMemberReferenceProperty(
		const UObject* Object,
		FName PropertyName,
		const FMemberReference*& OutValue)
	{
		const FStructProperty* Property =
			Object ? FindFProperty<FStructProperty>(Object->GetClass(), PropertyName) : nullptr;
		if (!Property || Property->Struct != FMemberReference::StaticStruct())
		{
			OutValue = nullptr;
			return false;
		}

		OutValue = Property->ContainerPtrToValuePtr<FMemberReference>(Object);
		return OutValue != nullptr;
	}

	bool ReadPropertyText(
		const UObject* Object,
		FName PropertyName,
		FString& OutValue)
	{
		const FProperty* Property =
			Object ? FindFProperty<FProperty>(Object->GetClass(), PropertyName) : nullptr;
		if (!Property)
		{
			return false;
		}

		const void* ValueAddress = Property->ContainerPtrToValuePtr<void>(Object);
		OutValue.Reset();
		Property->ExportTextItem_Direct(OutValue, ValueAddress, nullptr, nullptr, PPF_None);
		return true;
	}

	bool ReadStructFloatProperty(
		const UObject* Object,
		FName StructPropertyName,
		FName ValuePropertyName,
		float& OutValue)
	{
		const FStructProperty* StructProperty =
			Object ? FindFProperty<FStructProperty>(Object->GetClass(), StructPropertyName) : nullptr;
		const FFloatProperty* ValueProperty =
			StructProperty ? FindFProperty<FFloatProperty>(StructProperty->Struct, ValuePropertyName) : nullptr;
		if (!StructProperty || !ValueProperty)
		{
			return false;
		}

		const void* StructAddress = StructProperty->ContainerPtrToValuePtr<void>(Object);
		OutValue = ValueProperty->GetPropertyValue_InContainer(StructAddress);
		return true;
	}

	bool ReadStructBoolProperty(
		const UObject* Object,
		FName StructPropertyName,
		FName ValuePropertyName,
		bool& OutValue)
	{
		const FStructProperty* StructProperty =
			Object ? FindFProperty<FStructProperty>(Object->GetClass(), StructPropertyName) : nullptr;
		const FBoolProperty* ValueProperty =
			StructProperty ? FindFProperty<FBoolProperty>(StructProperty->Struct, ValuePropertyName) : nullptr;
		if (!StructProperty || !ValueProperty)
		{
			return false;
		}

		const void* StructAddress = StructProperty->ContainerPtrToValuePtr<void>(Object);
		OutValue = ValueProperty->GetPropertyValue_InContainer(StructAddress);
		return true;
	}

	bool ReadStructPropertyText(
		const UObject* Object,
		FName StructPropertyName,
		FName ValuePropertyName,
		FString& OutValue)
	{
		const FStructProperty* StructProperty =
			Object ? FindFProperty<FStructProperty>(Object->GetClass(), StructPropertyName) : nullptr;
		const FProperty* ValueProperty =
			StructProperty ? FindFProperty<FProperty>(StructProperty->Struct, ValuePropertyName) : nullptr;
		if (!StructProperty || !ValueProperty)
		{
			return false;
		}

		const void* StructAddress = StructProperty->ContainerPtrToValuePtr<void>(Object);
		const void* ValueAddress = ValueProperty->ContainerPtrToValuePtr<void>(StructAddress);
		OutValue.Reset();
		ValueProperty->ExportTextItem_Direct(OutValue, ValueAddress, nullptr, nullptr, PPF_None);
		return true;
	}

	bool ReadStructObjectProperty(
		const UObject* Object,
		FName StructPropertyName,
		FName ValuePropertyName,
		UObject*& OutValue)
	{
		const FStructProperty* StructProperty =
			Object ? FindFProperty<FStructProperty>(Object->GetClass(), StructPropertyName) : nullptr;
		const FObjectPropertyBase* ValueProperty =
			StructProperty ? FindFProperty<FObjectPropertyBase>(StructProperty->Struct, ValuePropertyName) : nullptr;
		if (!StructProperty || !ValueProperty)
		{
			return false;
		}

		const void* StructAddress = StructProperty->ContainerPtrToValuePtr<void>(Object);
		OutValue = ValueProperty->GetObjectPropertyValue_InContainer(StructAddress);
		return true;
	}

	bool ReadObjectArrayProperty(
		UObject* Object,
		FName PropertyName,
		TArray<FString>& OutObjectPaths)
	{
		OutObjectPaths.Reset();
		const FArrayProperty* ArrayProperty =
			Object ? FindFProperty<FArrayProperty>(Object->GetClass(), PropertyName) : nullptr;
		const FObjectPropertyBase* ObjectProperty =
			ArrayProperty ? CastField<FObjectPropertyBase>(ArrayProperty->Inner) : nullptr;
		if (!ArrayProperty || !ObjectProperty)
		{
			return false;
		}

		void* ArrayAddress = ArrayProperty->ContainerPtrToValuePtr<void>(Object);
		FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayAddress);
		OutObjectPaths.Reserve(ArrayHelper.Num());
		for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
		{
			const UObject* Value = ObjectProperty->GetObjectPropertyValue(ArrayHelper.GetRawPtr(Index));
			OutObjectPaths.Add(GetPathNameSafe(Value));
		}

		return true;
	}

	bool ReadStructObjectArrayProperty(
		UObject* Object,
		FName StructPropertyName,
		FName ArrayPropertyName,
		TArray<FString>& OutObjectPaths)
	{
		OutObjectPaths.Reset();
		const FStructProperty* StructProperty =
			Object ? FindFProperty<FStructProperty>(Object->GetClass(), StructPropertyName) : nullptr;
		const FArrayProperty* ArrayProperty = StructProperty
			? FindFProperty<FArrayProperty>(StructProperty->Struct, ArrayPropertyName)
			: nullptr;
		const FObjectPropertyBase* ObjectProperty =
			ArrayProperty ? CastField<FObjectPropertyBase>(ArrayProperty->Inner) : nullptr;
		if (!StructProperty || !ArrayProperty || !ObjectProperty)
		{
			return false;
		}

		void* StructAddress = StructProperty->ContainerPtrToValuePtr<void>(Object);
		void* ArrayAddress = ArrayProperty->ContainerPtrToValuePtr<void>(StructAddress);
		FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayAddress);
		OutObjectPaths.Reserve(ArrayHelper.Num());
		for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
		{
			const UObject* Value = ObjectProperty->GetObjectPropertyValue(ArrayHelper.GetRawPtr(Index));
			OutObjectPaths.Add(GetPathNameSafe(Value));
		}

		return true;
	}

	bool ReadObjectProperty(
		const UObject* Object,
		FName PropertyName,
		FString& OutObjectPath)
	{
		const FObjectPropertyBase* ObjectProperty =
			Object ? FindFProperty<FObjectPropertyBase>(Object->GetClass(), PropertyName) : nullptr;
		if (!ObjectProperty)
		{
			return false;
		}

		OutObjectPath = GetPathNameSafe(ObjectProperty->GetObjectPropertyValue_InContainer(Object));
		return true;
	}

	bool ReadGameplayTagPropertyMappings(
		const UObject* Object,
		TMap<FName, FString>& OutMappings)
	{
		OutMappings.Reset();
		const FStructProperty* MapProperty =
			Object ? FindFProperty<FStructProperty>(Object->GetClass(), TEXT("GameplayTagPropertyMap")) : nullptr;
		const FArrayProperty* MappingsProperty =
			MapProperty ? FindFProperty<FArrayProperty>(MapProperty->Struct, TEXT("PropertyMappings")) : nullptr;
		const FStructProperty* MappingProperty =
			MappingsProperty ? CastField<FStructProperty>(MappingsProperty->Inner) : nullptr;
		const FStructProperty* TagProperty =
			MappingProperty ? FindFProperty<FStructProperty>(MappingProperty->Struct, TEXT("TagToMap")) : nullptr;
		const FNameProperty* PropertyNameProperty =
			MappingProperty ? FindFProperty<FNameProperty>(MappingProperty->Struct, TEXT("PropertyName")) : nullptr;
		if (!MapProperty ||
			!MappingsProperty ||
			!MappingProperty ||
			!TagProperty ||
			TagProperty->Struct != FGameplayTag::StaticStruct() ||
			!PropertyNameProperty)
		{
			return false;
		}

		const void* MapAddress = MapProperty->ContainerPtrToValuePtr<void>(Object);
		const void* MappingsAddress = MappingsProperty->ContainerPtrToValuePtr<void>(MapAddress);
		FScriptArrayHelper MappingsHelper(
			MappingsProperty,
			const_cast<void*>(MappingsAddress));
		for (int32 Index = 0; Index < MappingsHelper.Num(); ++Index)
		{
			const void* MappingAddress = MappingsHelper.GetRawPtr(Index);
			const FGameplayTag* Tag = TagProperty->ContainerPtrToValuePtr<FGameplayTag>(MappingAddress);
			const FName PropertyName =
				PropertyNameProperty->GetPropertyValue_InContainer(MappingAddress);
			if (!Tag || PropertyName.IsNone() || OutMappings.Contains(PropertyName))
			{
				return false;
			}

			OutMappings.Add(PropertyName, Tag->ToString());
		}

		return true;
	}

	bool AreActionPropertiesEquivalent(
		const UGameFeatureAction* BaseAction,
		const UGameFeatureAction* PilotAction,
		FString& OutMismatch)
	{
		if (!BaseAction || !PilotAction)
		{
			if (BaseAction == PilotAction)
			{
				return true;
			}

			OutMismatch = TEXT("only one action is null");
			return false;
		}

		if (BaseAction->GetClass() != PilotAction->GetClass())
		{
			OutMismatch = FString::Printf(
				TEXT("class differs: %s != %s"),
				*GetPathNameSafe(BaseAction->GetClass()),
				*GetPathNameSafe(PilotAction->GetClass()));
			return false;
		}

		constexpr EPropertyFlags IgnoredFlags =
			CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient;
		constexpr uint32 ComparisonFlags = PPF_DeepComparison | PPF_DeepCompareInstances;
		for (TFieldIterator<FProperty> PropertyIt(
				BaseAction->GetClass(),
				EFieldIteratorFlags::IncludeSuper);
			PropertyIt;
			++PropertyIt)
		{
			const FProperty* Property = *PropertyIt;
			if (Property->HasAnyPropertyFlags(IgnoredFlags))
			{
				continue;
			}

			if (!Property->Identical_InContainer(
					BaseAction,
					PilotAction,
					0,
					ComparisonFlags))
			{
				OutMismatch = FString::Printf(TEXT("property %s differs"), *Property->GetName());
				return false;
			}
		}

		return true;
	}

	bool IsForbiddenPilotDependency(const FString& PackageName)
	{
		const FString LowerPackageName = PackageName.ToLower();
		if (LowerPackageName.StartsWith(TEXT("/game/")) &&
			!LowerPackageName.StartsWith(TEXT("/game/survivalrpg/")))
		{
			return true;
		}

		static const TCHAR* const ForbiddenMarkers[] = {
			TEXT("sandboxcharacter"),
			TEXT("bpi_sandbox"),
			TEXT("experimentalstatemachine"),
			TEXT("psd_sm_"),
			TEXT("/traversal/"),
			TEXT("/locomotor/"),
			TEXT("/mover/"),
			TEXT("/script/locomotor"),
			TEXT("/script/mover"),
			TEXT("/script/networkprediction"),
			TEXT("/script/metasound"),
			TEXT("/script/smartobjects"),
			TEXT("/script/survivalrpgeditor"),
			TEXT("foley"),
		};

		for (const TCHAR* Marker : ForbiddenMarkers)
		{
			if (LowerPackageName.Contains(Marker))
			{
				return true;
			}
		}

		return false;
	}

	bool ShouldTraversePilotDependency(const FString& PackageName)
	{
		return
			PackageName.StartsWith(TEXT("/RpgGaspLocomotion/")) ||
			PackageName.StartsWith(TEXT("/Game/SurvivalRpg/Characters/Mannequins/Anims/GASP/")) ||
			PackageName.StartsWith(TEXT("/Game/SurvivalRpg/Core/Character/GASP/")) ||
			PackageName == PilotExperiencePackage;
	}

	void GatherPilotDependencyClosure(
		IAssetRegistry& AssetRegistry,
		TConstArrayView<FName> RootPackages,
		TSet<FName>& OutPackages)
	{
		TArray<FName> PendingPackages;
		PendingPackages.Append(RootPackages.GetData(), RootPackages.Num());
		TSet<FName> TraversedPackages;
		for (int32 PendingIndex = 0; PendingIndex < PendingPackages.Num(); ++PendingIndex)
		{
			const FName PackageName = PendingPackages[PendingIndex];
			if (TraversedPackages.Contains(PackageName))
			{
				continue;
			}

			TraversedPackages.Add(PackageName);
			OutPackages.Add(PackageName);

			TArray<FName> Dependencies;
			AssetRegistry.GetDependencies(
				PackageName,
				Dependencies,
				UE::AssetRegistry::EDependencyCategory::Package);
			for (const FName Dependency : Dependencies)
			{
				OutPackages.Add(Dependency);
				if (ShouldTraversePilotDependency(Dependency.ToString()))
				{
					PendingPackages.Add(Dependency);
				}
			}
		}
	}

	void TestPerSampleBlendStackContract(
		FAutomationTestBase& Test,
		UAnimBlueprint* AnimBlueprint,
		UAnimGraphNode_MotionMatching* MotionMatchingNode)
	{
		if (!MotionMatchingNode)
		{
			return;
		}

		const TArray<UEdGraph*> SubGraphs = MotionMatchingNode->GetSubGraphs();
		Test.TestEqual(
			TEXT("Motion Matching owns exactly one nested graph"),
			SubGraphs.Num(),
			1);

		int32 BlendStackGraphCount = 0;
		UAnimationBlendStackGraph* BlendStackGraph = nullptr;
		for (UEdGraph* SubGraph : SubGraphs)
		{
			if (UAnimationBlendStackGraph* Candidate = Cast<UAnimationBlendStackGraph>(SubGraph))
			{
				++BlendStackGraphCount;
				BlendStackGraph = Candidate;
			}
		}
		Test.TestEqual(
			TEXT("Motion Matching owns exactly one AnimationBlendStackGraph"),
			BlendStackGraphCount,
			1);
		if (!Test.TestNotNull(TEXT("The Motion Matching BlendStack graph loads"), BlendStackGraph))
		{
			return;
		}

		Test.TestEqual(
			TEXT("The per-sample BlendStack graph contains exactly nine nodes"),
			BlendStackGraph->Nodes.Num(),
			9);

		UAnimGraphNode_BlendStackInput* InputNode =
			FindUniqueNode<UAnimGraphNode_BlendStackInput>(Test, BlendStackGraph, TEXT("BlendStack Input"));
		UAnimGraphNode_LocalToComponentSpace* LocalToComponentNode =
			FindUniqueNode<UAnimGraphNode_LocalToComponentSpace>(Test, BlendStackGraph, TEXT("Local To Component"));
		UAnimGraphNode_OrientationWarping* OrientationWarpingNode =
			FindUniqueNode<UAnimGraphNode_OrientationWarping>(Test, BlendStackGraph, TEXT("Per-sample Orientation Warping"));
		UAnimGraphNode_ResetRoot* ResetRootNode =
			FindUniqueNode<UAnimGraphNode_ResetRoot>(Test, BlendStackGraph, TEXT("Reset Root"));
		UAnimGraphNode_Steering* SteeringNode =
			FindUniqueNode<UAnimGraphNode_Steering>(Test, BlendStackGraph, TEXT("Steering"));
		UAnimGraphNode_ComponentToLocalSpace* ComponentToLocalNode =
			FindUniqueNode<UAnimGraphNode_ComponentToLocalSpace>(Test, BlendStackGraph, TEXT("Component To Local"));
		UK2Node_AnimNodeReference* InputReferenceNode =
			FindUniqueNode<UK2Node_AnimNodeReference>(Test, BlendStackGraph, TEXT("BlendStack Input Reference"));
		UK2Node_CallFunction* BlendStackInputsNode =
			FindUniqueNode<UK2Node_CallFunction>(Test, BlendStackGraph, TEXT("GetGaspBlendStackInputs call"));
		UAnimGraphNode_BlendStackResult* ResultNode =
			FindUniqueNode<UAnimGraphNode_BlendStackResult>(Test, BlendStackGraph, TEXT("BlendStack Result"));

		const auto TestExactNodeClass = [&Test](
			const TCHAR* Description,
			const UEdGraphNode* Node,
			UClass* ExpectedClass)
		{
			if (Node)
			{
				Test.TestEqual(Description, Node->GetClass(), ExpectedClass);
			}
		};
		TestExactNodeClass(TEXT("The input uses the exact BlendStack Input class"), InputNode, UAnimGraphNode_BlendStackInput::StaticClass());
		TestExactNodeClass(TEXT("The L2C node uses the exact conversion class"), LocalToComponentNode, UAnimGraphNode_LocalToComponentSpace::StaticClass());
		TestExactNodeClass(TEXT("The OW node uses the exact Orientation Warping class"), OrientationWarpingNode, UAnimGraphNode_OrientationWarping::StaticClass());
		TestExactNodeClass(TEXT("The reset node uses the exact Reset Root class"), ResetRootNode, UAnimGraphNode_ResetRoot::StaticClass());
		TestExactNodeClass(TEXT("The steering node uses the exact Steering class"), SteeringNode, UAnimGraphNode_Steering::StaticClass());
		TestExactNodeClass(TEXT("The C2L node uses the exact conversion class"), ComponentToLocalNode, UAnimGraphNode_ComponentToLocalSpace::StaticClass());
		TestExactNodeClass(TEXT("The reference uses the exact Anim Node Reference class"), InputReferenceNode, UK2Node_AnimNodeReference::StaticClass());
		TestExactNodeClass(TEXT("The helper uses the exact Call Function class"), BlendStackInputsNode, UK2Node_CallFunction::StaticClass());
		TestExactNodeClass(TEXT("The result uses the exact BlendStack Result class"), ResultNode, UAnimGraphNode_BlendStackResult::StaticClass());

		const FName ExpectedInputTag(TEXT("RpgGaspPilotBlendStackInput"));
		if (InputNode)
		{
			Test.TestEqual(
				TEXT("The BlendStack input has the stable pilot tag"),
				InputNode->GetTag(),
				ExpectedInputTag);
		}
		if (InputReferenceNode)
		{
			Test.TestEqual(
				TEXT("The Anim Node Reference targets the pilot input tag"),
				InputReferenceNode->GetTag(),
				ExpectedInputTag);
		}

		UFunction* HelperFunction = URpgAnimInstance::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(URpgAnimInstance, GetGaspBlendStackInputs));
		if (Test.TestNotNull(TEXT("GetGaspBlendStackInputs is reflected"), HelperFunction))
		{
			Test.TestTrue(
				TEXT("GetGaspBlendStackInputs is BlueprintCallable, pure, and const"),
				HelperFunction->HasAllFunctionFlags(
					FUNC_BlueprintCallable | FUNC_BlueprintPure | FUNC_Const));
			Test.TestTrue(
				TEXT("GetGaspBlendStackInputs is explicitly BlueprintThreadSafe"),
				HelperFunction->HasMetaData(TEXT("BlueprintThreadSafe")));

			const FFloatProperty* ResetRootAlphaProperty =
				FindFProperty<FFloatProperty>(HelperFunction, TEXT("ResetRootAlpha"));
			if (Test.TestNotNull(
				TEXT("GetGaspBlendStackInputs exposes the phase-safe ResetRootAlpha output"),
				ResetRootAlphaProperty))
			{
				Test.TestTrue(
					TEXT("ResetRootAlpha is an output parameter"),
					ResetRootAlphaProperty->HasAllPropertyFlags(CPF_Parm | CPF_OutParm));
			}
			Test.TestNull(
				TEXT("The obsolete grounded-only MovingAlpha output is removed"),
				FindFProperty<FProperty>(HelperFunction, TEXT("MovingAlpha")));

			const FFloatProperty* OrientationWarpingAlphaProperty =
				FindFProperty<FFloatProperty>(HelperFunction, TEXT("OrientationWarpingAlpha"));
			if (Test.TestNotNull(
				TEXT("GetGaspBlendStackInputs keeps OrientationWarpingAlpha separate from ResetRoot"),
				OrientationWarpingAlphaProperty))
			{
				Test.TestTrue(
					TEXT("OrientationWarpingAlpha is an output parameter"),
					OrientationWarpingAlphaProperty->HasAllPropertyFlags(CPF_Parm | CPF_OutParm));
			}

			const FBoolProperty* EnableSteeringProperty =
				FindFProperty<FBoolProperty>(HelperFunction, TEXT("bEnableSteering"));
			if (Test.TestNotNull(
				TEXT("GetGaspBlendStackInputs keeps Steering as a separate sample gate"),
				EnableSteeringProperty))
			{
				Test.TestTrue(
					TEXT("bEnableSteering is an output parameter"),
					EnableSteeringProperty->HasAllPropertyFlags(CPF_Parm | CPF_OutParm));
			}
		}
		if (BlendStackInputsNode)
		{
			Test.TestTrue(
				TEXT("The nested helper call is a pure K2 node"),
				BlendStackInputsNode->IsNodePure());
			Test.TestEqual(
				TEXT("The nested helper call has the expected function name"),
				BlendStackInputsNode->GetFunctionName(),
				GET_FUNCTION_NAME_CHECKED(URpgAnimInstance, GetGaspBlendStackInputs));
			Test.TestEqual(
				TEXT("The nested helper call resolves to URpgAnimInstance"),
				BlendStackInputsNode->GetTargetFunction(),
				HelperFunction);
		}

		const FAnimNode_OrientationWarping* OrientationWarping =
			ReadRuntimeNode<FAnimNode_OrientationWarping>(OrientationWarpingNode);
		if (Test.TestNotNull(TEXT("The per-sample Orientation Warping runtime node is readable"), OrientationWarping))
		{
			Test.TestEqual(TEXT("OW evaluates from graph inputs"), OrientationWarping->Mode, EWarpingEvaluationMode::Graph);
			Test.TestEqual(TEXT("OW samples 0.8 seconds ahead"), OrientationWarping->TargetTime, 0.8f);
			Test.TestEqual(TEXT("OW ignores root motion below 10 cm/s"), OrientationWarping->MinRootMotionSpeedThreshold, 10.0f);
			Test.TestEqual(TEXT("OW uses the audited 135 degree inversion threshold"), OrientationWarping->LocomotionAngleDeltaThreshold, 135.0f);

			static const FName ExpectedSpineBones[] = {
				TEXT("spine_01"),
				TEXT("spine_02"),
				TEXT("spine_03"),
				TEXT("spine_04"),
				TEXT("spine_05"),
			};
			constexpr int32 ExpectedSpineBoneCount = static_cast<int32>(UE_ARRAY_COUNT(ExpectedSpineBones));
			Test.TestEqual(TEXT("OW distributes rotation over exactly five spine bones"), OrientationWarping->SpineBones.Num(), ExpectedSpineBoneCount);
			for (int32 Index = 0; Index < ExpectedSpineBoneCount && Index < OrientationWarping->SpineBones.Num(); ++Index)
			{
				Test.TestEqual(
					*FString::Printf(TEXT("OW spine bone %d is stable"), Index),
					OrientationWarping->SpineBones[Index].BoneName,
					ExpectedSpineBones[Index]);
			}
			Test.TestEqual(TEXT("OW uses the mannequin IK foot root"), OrientationWarping->IKFootRootBone.BoneName, FName(TEXT("ik_foot_root")));
			Test.TestEqual(TEXT("OW has exactly two IK foot bones"), OrientationWarping->IKFootBones.Num(), 2);
			if (OrientationWarping->IKFootBones.Num() == 2)
			{
				Test.TestEqual(TEXT("OW left IK foot is stable"), OrientationWarping->IKFootBones[0].BoneName, FName(TEXT("ik_foot_l")));
				Test.TestEqual(TEXT("OW right IK foot is stable"), OrientationWarping->IKFootBones[1].BoneName, FName(TEXT("ik_foot_r")));
			}

			Test.TestEqual(TEXT("OW rotates around Z"), OrientationWarping->RotationAxis.GetValue(), EAxis::Z);
			Test.TestEqual(TEXT("OW distributes half of the orientation"), OrientationWarping->DistributedBoneOrientationAlpha, 0.5f);
			Test.TestEqual(TEXT("OW interpolation speed is audited"), OrientationWarping->RotationInterpSpeed, 8.0f);
			Test.TestEqual(TEXT("OW counter-compensation speed is audited"), OrientationWarping->CounterCompensateInterpSpeed, 45.0f);
			Test.TestEqual(TEXT("OW permits full correction"), OrientationWarping->MaxCorrectionDegrees, 180.0f);
			Test.TestEqual(TEXT("OW preserves pivot root-motion deltas"), OrientationWarping->MaxRootMotionDeltaToCompensateDegrees, 45.0f);
			Test.TestTrue(TEXT("OW counter-compensates animated root motion"), OrientationWarping->bCounterCompenstateInterpolationByRootMotion);
			Test.TestFalse(TEXT("OW does not scale by global blend weight"), OrientationWarping->bScaleByGlobalBlendWeight);
			Test.TestFalse(TEXT("OW does not use manual root-motion velocity"), OrientationWarping->bUseManualRootMotionVelocity);
			Test.TestEqual(TEXT("OW evaluates in OffsetRoot-compatible root-bone space"), OrientationWarping->WarpingSpace, EOrientationWarpingSpace::RootBoneTransform);
			Test.TestEqual(TEXT("OW uses a float alpha"), OrientationWarping->AlphaInputType, EAnimAlphaInputType::Float);
			Test.TestEqual(TEXT("OW has no LOD cutoff"), OrientationWarping->LODThreshold, INDEX_NONE);
		}

		const FAnimNode_ResetRoot* ResetRoot = ReadRuntimeNode<FAnimNode_ResetRoot>(ResetRootNode);
		if (Test.TestNotNull(TEXT("The Reset Root runtime node is readable"), ResetRoot))
		{
			Test.TestEqual(TEXT("Reset Root uses a float moving gate"), ResetRoot->AlphaInputType, EAnimAlphaInputType::Float);
			Test.TestEqual(TEXT("Reset Root default alpha remains one"), ResetRoot->Alpha, 1.0f);
			Test.TestEqual(TEXT("Reset Root has no LOD cutoff"), ResetRoot->LODThreshold, INDEX_NONE);
		}

		const FAnimNode_Steering* Steering = ReadRuntimeNode<FAnimNode_Steering>(SteeringNode);
		if (Test.TestNotNull(TEXT("The authored Editor Steering runtime struct is readable"), Steering))
		{
			Test.TestEqual(TEXT("Steering matches the compiled GASP 0.4-second correction target"), Steering->ProceduralTargetTime, 0.4f);
			Test.TestEqual(TEXT("Steering samples animation two seconds ahead"), Steering->AnimatedTargetTime, 2.0f);
			Test.TestEqual(TEXT("Steering root-motion threshold is audited"), Steering->RootMotionThreshold, 1.0f);
			Test.TestEqual(TEXT("Steering disables below 10 cm/s"), Steering->DisableSteeringBelowSpeed, 10.0f);
			Test.TestEqual(TEXT("Steering additive correction remains enabled"), Steering->DisableAdditiveBelowSpeed, -1.0f);
			Test.TestEqual(TEXT("Steering minimum root-motion scale is audited"), Steering->MinScaleRatio, 0.5f);
			Test.TestEqual(TEXT("Steering maximum root-motion scale is audited"), Steering->MaxScaleRatio, 1.5f);
			Test.TestEqual(TEXT("Steering uses a bool gate"), Steering->AlphaInputType, EAnimAlphaInputType::Bool);
			Test.TestTrue(TEXT("Steering's unbound bool default remains enabled"), Steering->bAlphaBoolEnabled);
			Test.TestEqual(TEXT("Steering bool blend-in is audited"), Steering->AlphaBoolBlend.BlendInTime, 0.1f);
			Test.TestEqual(TEXT("Steering bool blend-out is audited"), Steering->AlphaBoolBlend.BlendOutTime, 0.1f);
			Test.TestEqual(TEXT("Steering bool blend is linear"), Steering->AlphaBoolBlend.BlendOption, EAlphaBlendOption::Linear);
			Test.TestEqual(TEXT("Steering has no LOD cutoff"), Steering->LODThreshold, INDEX_NONE);
		}

		UAnimBlueprintGeneratedClass* GeneratedClass =
			AnimBlueprint ? AnimBlueprint->GetAnimBlueprintGeneratedClass() : nullptr;
		if (Test.TestNotNull(TEXT("The pilot AnimBlueprint has a generated runtime class"), GeneratedClass))
		{
			UObject* GeneratedClassDefaultObject = GeneratedClass->GetDefaultObject();
			if (Test.TestNotNull(TEXT("The pilot generated runtime class has a CDO"), GeneratedClassDefaultObject))
			{
				const FAnimNode_Steering* CompiledSteering = SteeringNode
					? GeneratedClass->GetPropertyInstance<FAnimNode_Steering>(
						GeneratedClassDefaultObject,
						SteeringNode->NodeGuid)
					: nullptr;
				if (Test.TestNotNull(
						TEXT("The generated CDO exposes the exact per-sample Steering runtime node by source GUID"),
						CompiledSteering))
				{
					Test.TestEqual(
						TEXT("The compiled Steering runtime node uses GASP's 0.4-second correction target"),
						CompiledSteering->ProceduralTargetTime,
						0.4f);
					Test.TestEqual(
						TEXT("The compiled Steering runtime node samples animation two seconds ahead"),
						CompiledSteering->AnimatedTargetTime,
						2.0f);
					Test.TestEqual(
						TEXT("The compiled Steering runtime node keeps the 10 cm/s disable threshold"),
						CompiledSteering->DisableSteeringBelowSpeed,
						10.0f);
				}
			}
		}

		TestExactOutputLinks(Test, TEXT("BlendStack Input feeds L2C"), InputNode, TEXT("Pose"), {{LocalToComponentNode, TEXT("LocalPose")}});
		TestExactOutputLinks(Test, TEXT("L2C feeds per-sample OW"), LocalToComponentNode, TEXT("ComponentPose"), {{OrientationWarpingNode, TEXT("ComponentPose")}});
		TestExactOutputLinks(Test, TEXT("Per-sample OW feeds Reset Root"), OrientationWarpingNode, TEXT("Pose"), {{ResetRootNode, TEXT("ComponentPose")}});
		TestExactOutputLinks(Test, TEXT("Reset Root feeds Steering"), ResetRootNode, TEXT("Pose"), {{SteeringNode, TEXT("ComponentPose")}});
		TestExactOutputLinks(Test, TEXT("Steering feeds C2L"), SteeringNode, TEXT("Pose"), {{ComponentToLocalNode, TEXT("ComponentPose")}});
		TestExactOutputLinks(Test, TEXT("C2L feeds BlendStack Result"), ComponentToLocalNode, TEXT("Pose"), {{ResultNode, TEXT("Result")}});
		TestExactOutputLinks(Test, TEXT("The tagged reference feeds the helper"), InputReferenceNode, TEXT("Value"), {{BlendStackInputsNode, TEXT("Node")}});
		TestExactOutputLinks(
			Test,
			TEXT("The helper asset feeds both procedural nodes"),
			BlendStackInputsNode,
			TEXT("CurrentAnimAsset"),
			{{OrientationWarpingNode, TEXT("CurrentAnimAsset")}, {SteeringNode, TEXT("CurrentAnimAsset")}});
		TestExactOutputLinks(
			Test,
			TEXT("The helper asset time feeds both procedural nodes"),
			BlendStackInputsNode,
			TEXT("CurrentAnimAssetTime"),
			{{OrientationWarpingNode, TEXT("CurrentAnimAssetTime")}, {SteeringNode, TEXT("CurrentAnimAssetTime")}});
		TestExactOutputLinks(Test, TEXT("The phase-safe alpha gates Reset Root"), BlendStackInputsNode, TEXT("ResetRootAlpha"), {{ResetRootNode, TEXT("Alpha")}});
		TestExactOutputLinks(Test, TEXT("Curve-gated alpha drives OW"), BlendStackInputsNode, TEXT("OrientationWarpingAlpha"), {{OrientationWarpingNode, TEXT("Alpha")}});
		TestExactOutputLinks(Test, TEXT("Desired facing drives Steering"), BlendStackInputsNode, TEXT("DesiredFacing"), {{SteeringNode, TEXT("TargetOrientation")}});
		TestExactOutputLinks(Test, TEXT("Last movement direction drives OW"), BlendStackInputsNode, TEXT("LocomotionDirection"), {{OrientationWarpingNode, TEXT("LocomotionDirection")}});
		TestExactOutputLinks(Test, TEXT("The active-sample gate drives Steering"), BlendStackInputsNode, TEXT("bEnableSteering"), {{SteeringNode, TEXT("bAlphaBoolEnabled")}});
	}

}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgGaspPilotAssetContractTest,
	"SurvivalRpg.Animation.Gasp.PilotAssetContract",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgGaspPilotAssetContractTest::RunTest(const FString& Parameters)
{
	using namespace RpgGaspPilotAssetTests;

	UAnimBlueprint* PilotAnimBlueprint = LoadRequiredAsset<UAnimBlueprint>(
		*this,
		PilotAnimBlueprintObject,
		TEXT("The isolated GASP AnimBlueprint loads"));
	UBlueprint* BaseCharacterBlueprint = LoadRequiredAsset<UBlueprint>(
		*this,
		BaseCharacterBlueprintObject,
		TEXT("The default character Blueprint loads"));
	UBlueprint* PilotCharacterBlueprint = LoadRequiredAsset<UBlueprint>(
		*this,
		PilotCharacterBlueprintObject,
		TEXT("The isolated GASP character Blueprint loads"));
	URpgPawnData* BasePawnData = LoadRequiredAsset<URpgPawnData>(
		*this,
		BasePawnDataObject,
		TEXT("The default PawnData loads"));
	URpgPawnData* PilotPawnData = LoadRequiredAsset<URpgPawnData>(
		*this,
		PilotPawnDataObject,
		TEXT("The isolated GASP PawnData loads"));
	UBlueprint* BaseExperienceBlueprint = LoadRequiredAsset<UBlueprint>(
		*this,
		BaseExperienceObject,
		TEXT("The prototype Experience Blueprint loads"));
	UBlueprint* PilotExperienceBlueprint = LoadRequiredAsset<UBlueprint>(
		*this,
		PilotExperienceObject,
		TEXT("The isolated GASP Experience Blueprint loads"));
	UPoseSearchDatabase* TurnInPlaceDatabase = LoadRequiredAsset<UPoseSearchDatabase>(
		*this,
		TurnInPlaceDatabaseObject,
		TEXT("The project-local turn-in-place database loads"));
	URpgExperienceDefinition* BaseExperience =
		BaseExperienceBlueprint && BaseExperienceBlueprint->GeneratedClass
			? Cast<URpgExperienceDefinition>(BaseExperienceBlueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
	URpgExperienceDefinition* PilotExperience =
		PilotExperienceBlueprint && PilotExperienceBlueprint->GeneratedClass
			? Cast<URpgExperienceDefinition>(PilotExperienceBlueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
	TestNotNull(TEXT("The prototype Experience defaults load"), BaseExperience);
	TestNotNull(TEXT("The isolated GASP Experience defaults load"), PilotExperience);

	if (!PilotAnimBlueprint ||
		!BaseCharacterBlueprint ||
		!PilotCharacterBlueprint ||
		!BasePawnData ||
		!PilotPawnData ||
		!BaseExperienceBlueprint ||
		!PilotExperienceBlueprint ||
		!TurnInPlaceDatabase ||
		!BaseExperience ||
		!PilotExperience)
	{
		return false;
	}

	TestEqual(
		TEXT("The GASP AnimBlueprint derives directly from URpgAnimInstance"),
		PilotAnimBlueprint->ParentClass.Get(),
		URpgAnimInstance::StaticClass());
	TestEqual(
		TEXT("The GASP AnimBlueprint targets the authoritative player skeleton"),
		GetPathNameSafe(PilotAnimBlueprint->TargetSkeleton),
		FString(TargetSkeletonObject));
	TestTrue(
		TEXT("The GASP AnimBlueprint allows multi-threaded animation update"),
		PilotAnimBlueprint->bUseMultiThreadedAnimationUpdate);
	TestTrue(
		TEXT("The GASP AnimBlueprint has no compile error"),
		PilotAnimBlueprint->Status != BS_Error);
	TestEqual(
		TEXT("The project Foot Placement graph wrapper lives in the dedicated uncooked module"),
		UAnimGraphNode_RpgFootPlacement::StaticClass()->GetOutermost()->GetName(),
		FString(TEXT("/Script/SurvivalRpgAnimGraph")));
	TestTrue(
		TEXT("The project AnimGraph wrapper package is explicitly uncooked-only"),
		UAnimGraphNode_RpgFootPlacement::StaticClass()->GetOutermost()->HasAnyPackageFlags(
			PKG_UncookedOnly));

	static const struct
	{
		const TCHAR* ObjectPath;
		float ExpectedLength;
	} ExpectedTurnSequences[] = {
		{TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/M_Neutral_Stand_Turn_045_L.M_Neutral_Stand_Turn_045_L"), 1.6666667f},
		{TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/M_Neutral_Stand_Turn_045_R.M_Neutral_Stand_Turn_045_R"), 1.6666667f},
		{TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/M_Neutral_Stand_Turn_090_L.M_Neutral_Stand_Turn_090_L"), 2.0f},
		{TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/M_Neutral_Stand_Turn_090_R.M_Neutral_Stand_Turn_090_R"), 2.0f},
		{TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/M_Neutral_Stand_Turn_135_L.M_Neutral_Stand_Turn_135_L"), 2.0f},
		{TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/M_Neutral_Stand_Turn_135_R.M_Neutral_Stand_Turn_135_R"), 2.0f},
		{TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/M_Neutral_Stand_Turn_180_L.M_Neutral_Stand_Turn_180_L"), 2.1666667f},
		{TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/M_Neutral_Stand_Turn_180_R.M_Neutral_Stand_Turn_180_R"), 2.1666667f},
	};
	TestEqual(
		TEXT("The exclusive turn-in-place database contains exactly eight clips"),
		TurnInPlaceDatabase->GetNumAnimationAssets(),
		static_cast<int32>(UE_ARRAY_COUNT(ExpectedTurnSequences)));
	for (int32 Index = 0;
		Index < UE_ARRAY_COUNT(ExpectedTurnSequences) && Index < TurnInPlaceDatabase->GetNumAnimationAssets();
		++Index)
	{
		const FPoseSearchDatabaseAnimationAsset* DatabaseEntry =
			TurnInPlaceDatabase->GetDatabaseAnimationAsset(Index);
		if (!TestNotNull(
			*FString::Printf(TEXT("Turn-in-place database entry %d is readable"), Index),
			DatabaseEntry))
		{
			continue;
		}

		UAnimSequence* Sequence = Cast<UAnimSequence>(DatabaseEntry->GetAnimationAsset());
		if (!TestNotNull(
			*FString::Printf(TEXT("Turn-in-place database entry %d references an AnimSequence"), Index),
			Sequence))
		{
			continue;
		}

		TestEqual(
			*FString::Printf(TEXT("Turn-in-place database entry %d keeps the deterministic clip order"), Index),
			GetPathNameSafe(Sequence),
			FString(ExpectedTurnSequences[Index].ObjectPath));
		TestFalse(
			*FString::Printf(TEXT("Turn-in-place clip %d is non-looping"), Index),
			Sequence->bLoop);
		TestTrue(
			*FString::Printf(TEXT("Turn-in-place clip %d keeps its authored duration"), Index),
			FMath::IsNearlyEqual(
				static_cast<float>(Sequence->GetPlayLength()),
				ExpectedTurnSequences[Index].ExpectedLength,
				0.001f));
		TestTrue(
			*FString::Printf(TEXT("Turn-in-place clip %d plays at authored speed"), Index),
			FMath::IsNearlyEqual(Sequence->RateScale, 1.0f));
		TestTrue(
			*FString::Printf(TEXT("Turn-in-place database entry %d remains enabled"), Index),
			DatabaseEntry->IsEnabled());
		TestFalse(
			*FString::Printf(TEXT("Turn-in-place database entry %d remains non-looping"), Index),
			DatabaseEntry->IsLooping());
		TestFalse(
			*FString::Printf(TEXT("Turn-in-place database entry %d keeps Epic's reselection default"), Index),
			DatabaseEntry->IsDisableReselection());
		const FFloatInterval SamplingRange = DatabaseEntry->GetSamplingRange();
		TestTrue(
			*FString::Printf(TEXT("Turn-in-place database entry %d searches only the authored start pose"), Index),
			FMath::IsNearlyZero(SamplingRange.Min) && FMath::IsNearlyEqual(SamplingRange.Max, 0.01f));
		TestTrue(
			*FString::Printf(TEXT("Turn-in-place database entry %d remains unmirrored-only"), Index),
			DatabaseEntry->GetMirrorOption() == EPoseSearchMirrorOption::UnmirroredOnly);
	}

	const UE::PoseSearch::EAsyncBuildIndexResult TurnInPlaceBuildResult =
		UE::PoseSearch::FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(
			TurnInPlaceDatabase,
			UE::PoseSearch::ERequestAsyncBuildFlag::NewRequest |
				UE::PoseSearch::ERequestAsyncBuildFlag::WaitForCompletion);
	if (TestTrue(
		TEXT("The exclusive turn-in-place search index finishes building"),
		TurnInPlaceBuildResult == UE::PoseSearch::EAsyncBuildIndexResult::Success))
	{
		const UE::PoseSearch::FSearchIndex& TurnInPlaceSearchIndex = TurnInPlaceDatabase->GetSearchIndex();
		TestEqual(
			TEXT("The exclusive turn-in-place database indexes exactly eight poses"),
			TurnInPlaceSearchIndex.GetNumPoses(),
			8);
		TestEqual(
			TEXT("The exclusive turn-in-place database creates exactly eight search-index assets"),
			TurnInPlaceSearchIndex.Assets.Num(),
			8);
		for (int32 Index = 0; Index < TurnInPlaceSearchIndex.Assets.Num(); ++Index)
		{
			TestEqual(
				*FString::Printf(TEXT("Turn-in-place search-index asset %d contains only t=0"), Index),
				TurnInPlaceSearchIndex.Assets[Index].GetNumPoses(),
				1);
		}
	}

	URpgAnimInstance* PilotAnimDefaults =
		PilotAnimBlueprint->GeneratedClass
			? Cast<URpgAnimInstance>(PilotAnimBlueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
	if (TestNotNull(TEXT("The GASP AnimBlueprint generated-class defaults load"), PilotAnimDefaults))
	{
		FDataValidationContext ValidationContext;
		TestEqual(
			TEXT("The GASP AnimBlueprint defaults pass native data validation"),
			PilotAnimDefaults->IsDataValid(ValidationContext),
			EDataValidationResult::Valid);
		TestEqual(
			TEXT("The GASP AnimBlueprint extracts root motion only from montages"),
			PilotAnimDefaults->RootMotionMode.GetValue(),
			ERootMotionMode::RootMotionFromMontagesOnly);

		bool bGeneratesTrajectory = false;
		if (TestTrue(
				TEXT("The trajectory-generation setting exists on URpgAnimInstance"),
				ReadBoolProperty(PilotAnimDefaults, TEXT("bGeneratePoseSearchTrajectory"), bGeneratesTrajectory)))
		{
			TestTrue(TEXT("The GASP AnimBlueprint generates its thread-safe trajectory snapshot"), bGeneratesTrajectory);
		}

		const FEnumProperty* JumpPhaseProperty =
			FindFProperty<FEnumProperty>(PilotAnimDefaults->GetClass(), TEXT("JumpPhase"));
		if (TestNotNull(TEXT("The cosmetic jump phase is reflected"), JumpPhaseProperty))
		{
			TestEqual(
				TEXT("JumpPhase uses the explicit ERpgJumpPhase contract"),
				JumpPhaseProperty->GetEnum(),
				StaticEnum<ERpgJumpPhase>());
			TestTrue(
				TEXT("JumpPhase is transient Blueprint-readable debug state"),
				JumpPhaseProperty->HasAllPropertyFlags(
					CPF_Transient | CPF_BlueprintVisible | CPF_BlueprintReadOnly));
		}
		FString JumpPhaseText;
		if (TestTrue(
			TEXT("The cosmetic jump phase default is readable"),
			ReadPropertyText(PilotAnimDefaults, TEXT("JumpPhase"), JumpPhaseText)))
		{
			TestEqual(TEXT("The GASP pilot starts grounded"), JumpPhaseText, FString(TEXT("Grounded")));
		}

		float LandingStateElapsed = -1.0f;
		if (TestTrue(
			TEXT("The landing elapsed debug value is readable"),
			ReadFloatProperty(PilotAnimDefaults, TEXT("LandingStateElapsed"), LandingStateElapsed)))
		{
			TestEqual(TEXT("Landing elapsed time starts at zero"), LandingStateElapsed, 0.0f);
			const FFloatProperty* Property =
				FindFProperty<FFloatProperty>(PilotAnimDefaults->GetClass(), TEXT("LandingStateElapsed"));
			TestTrue(
				TEXT("LandingStateElapsed is transient Blueprint-readable debug state"),
				Property && Property->HasAllPropertyFlags(
					CPF_Transient | CPF_BlueprintVisible | CPF_BlueprintReadOnly));
		}

		bool bLandingSelectionLatched = true;
		if (TestTrue(
			TEXT("The landing-selection latch debug value is readable"),
			ReadBoolProperty(
				PilotAnimDefaults,
				TEXT("bLandingSelectionLatched"),
				bLandingSelectionLatched)))
		{
			TestFalse(TEXT("No landing selection is latched on the class default"), bLandingSelectionLatched);
			const FBoolProperty* Property =
				FindFProperty<FBoolProperty>(PilotAnimDefaults->GetClass(), TEXT("bLandingSelectionLatched"));
			TestTrue(
				TEXT("bLandingSelectionLatched is transient Blueprint-readable debug state"),
				Property && Property->HasAllPropertyFlags(
					CPF_Transient | CPF_BlueprintVisible | CPF_BlueprintReadOnly));
		}

		float AirborneProceduralAlpha = -1.0f;
		if (TestTrue(
			TEXT("The airborne procedural debug alpha is readable"),
			ReadFloatProperty(
				PilotAnimDefaults,
				TEXT("AirborneProceduralAlpha"),
				AirborneProceduralAlpha)))
		{
			TestEqual(TEXT("Airborne procedural work starts disabled"), AirborneProceduralAlpha, 0.0f);
			const FFloatProperty* Property =
				FindFProperty<FFloatProperty>(PilotAnimDefaults->GetClass(), TEXT("AirborneProceduralAlpha"));
			TestTrue(
				TEXT("AirborneProceduralAlpha is transient Blueprint-readable debug state"),
				Property && Property->HasAllPropertyFlags(
					CPF_Transient | CPF_BlueprintVisible | CPF_BlueprintReadOnly));
		}

		const FRpgFootPlacementSettings* FootPlacementSettings =
			ReadStructPropertyValue<FRpgFootPlacementSettings>(
				PilotAnimDefaults,
				TEXT("FootPlacementSettings"));
		if (TestNotNull(
				TEXT("The GASP AnimBlueprint exposes project-local Foot Placement settings"),
				FootPlacementSettings))
		{
			TestTrue(
				TEXT("The GASP AnimBlueprint enables game-thread Foot Placement sampling"),
				FootPlacementSettings->bEnabled);
			TestFalse(
				TEXT("The bounded #60 solver preserves the already-validated authored crouch pose"),
				FootPlacementSettings->bApplyWhileCrouching);
			TestEqual(TEXT("Foot Placement traces start 75 cm above each ball"), FootPlacementSettings->TraceStartHeight, 75.0f);
			TestEqual(TEXT("Foot Placement traces end 100 cm below each ball"), FootPlacementSettings->TraceEndDepth, 100.0f);
			TestEqual(TEXT("Foot Placement uses a five-centimeter sphere sweep"), FootPlacementSettings->SweepRadius, 5.0f);
			TestEqual(TEXT("Foot Placement plants below 60 cm/s"), FootPlacementSettings->PlantSpeedThreshold, 60.0f);
			TestEqual(TEXT("Foot Placement bounds the contact-weighted roll phase at 200 cm/s"), FootPlacementSettings->UnalignmentSpeedThreshold, 200.0f);
			TestEqual(TEXT("Foot Placement plants within ten centimeters"), FootPlacementSettings->PlantDistanceThreshold, 10.0f);
			TestEqual(TEXT("Foot Placement releases outside GASP's 20-centimeter radius"), FootPlacementSettings->UnplantRadius, 20.0f);
			TestEqual(TEXT("Foot Placement replants inside 20 percent of the release radius"), FootPlacementSettings->ReplantRadiusRatio, 0.2f);
			TestEqual(TEXT("Foot Placement releases after GASP's 60-degree normal change"), FootPlacementSettings->UnplantAngle, 60.0f);
			TestEqual(TEXT("Foot Placement replants inside 20 percent of the release angle"), FootPlacementSettings->ReplantAngleRatio, 0.2f);
			TestEqual(TEXT("Foot Placement bounds locked-foot slope alignment to 60 degrees"), FootPlacementSettings->MaxFootAlignmentAngle, 60.0f);
			TestEqual(TEXT("Foot Placement bounds locked-foot translation to 50 centimeters"), FootPlacementSettings->MaxFootTranslation, 50.0f);
			TestEqual(TEXT("Foot Placement keeps a 0.1-second trace-miss grace period"), FootPlacementSettings->TraceMissGracePeriod, 0.10f);
			TestEqual(TEXT("Foot Placement blends weights with a 0.08-second half-life"), FootPlacementSettings->WeightBlendHalfLife, 0.08f);

			TestEqual(TEXT("Foot Placement samples the left FK ankle"), FootPlacementSettings->LeftLeg.FKFootBone, FName(TEXT("foot_l")));
			TestEqual(TEXT("Foot Placement drives the left IK target"), FootPlacementSettings->LeftLeg.IKFootBone, FName(TEXT("ik_foot_l")));
			TestEqual(TEXT("Foot Placement traces from the left ball"), FootPlacementSettings->LeftLeg.BallBone, FName(TEXT("ball_l")));
			TestEqual(TEXT("Foot Placement reads the left contact curve"), FootPlacementSettings->LeftLeg.SpeedCurveName, FName(TEXT("contact_l")));
			TestEqual(TEXT("Foot Placement samples the right FK ankle"), FootPlacementSettings->RightLeg.FKFootBone, FName(TEXT("foot_r")));
			TestEqual(TEXT("Foot Placement drives the right IK target"), FootPlacementSettings->RightLeg.IKFootBone, FName(TEXT("ik_foot_r")));
			TestEqual(TEXT("Foot Placement traces from the right ball"), FootPlacementSettings->RightLeg.BallBone, FName(TEXT("ball_r")));
			TestEqual(TEXT("Foot Placement reads the right contact curve"), FootPlacementSettings->RightLeg.SpeedCurveName, FName(TEXT("contact_r")));
		}

		FString OffsetRootRotationMode;
		if (TestTrue(
			TEXT("The runtime Offset Root rotation mode is readable"),
			ReadPropertyText(PilotAnimDefaults, TEXT("OffsetRootRotationMode"), OffsetRootRotationMode)))
		{
			TestEqual(
				TEXT("The runtime Offset Root rotation mode defaults to interpolation"),
				OffsetRootRotationMode,
				FString(TEXT("Interpolate")));
		}
		bool bResetOffsetRootEveryFrame = true;
		if (TestTrue(
			TEXT("The runtime Offset Root reset pulse is readable"),
			ReadBoolProperty(
				PilotAnimDefaults,
				TEXT("bResetOffsetRootEveryFrame"),
				bResetOffsetRootEveryFrame)))
		{
			TestFalse(
				TEXT("The runtime Offset Root reset pulse is disabled by default"),
				bResetOffsetRootEveryFrame);
		}

		const auto TestGroundDatabaseGroup = [this, PilotAnimDefaults](
			FName GroupPropertyName,
			const TCHAR* GroupLabel,
			const TArray<FString>& ExpectedDatabasePaths)
		{
			TArray<FString> ActualDatabasePaths;
			if (!TestTrue(
					*FString::Printf(TEXT("The %s ground database group is readable"), GroupLabel),
					ReadStructObjectArrayProperty(
						PilotAnimDefaults,
						TEXT("GroundMotionMatchingDatabaseSets"),
						GroupPropertyName,
						ActualDatabasePaths)))
			{
				return;
			}

			TestEqual(
				*FString::Printf(TEXT("The %s ground database group has the exact fixed size"), GroupLabel),
				ActualDatabasePaths.Num(),
				ExpectedDatabasePaths.Num());
			for (int32 Index = 0;
				Index < ExpectedDatabasePaths.Num() && Index < ActualDatabasePaths.Num();
				++Index)
			{
				TestEqual(
					*FString::Printf(TEXT("%s ground database %d is ordered and project-local"), GroupLabel, Index),
					ActualDatabasePaths[Index],
					ExpectedDatabasePaths[Index]);
			}
		};

		TestGroundDatabaseGroup(
			TEXT("Idle"),
			TEXT("Idle"),
			{
				TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Idle.PSD_Rpg_Stand_Idle"),
			});
		TestGroundDatabaseGroup(
			TEXT("Walk"),
			TEXT("Walk"),
			{
				TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Walk.PSD_Rpg_Stand_Walk"),
			});
		TestGroundDatabaseGroup(
			TEXT("Run"),
			TEXT("Run"),
			{
				TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Loops.PSD_Rpg_Stand_Run_Loops"),
				TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Pivots.PSD_Rpg_Stand_Run_Pivots"),
				TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Starts.PSD_Rpg_Stand_Run_Starts"),
				TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Stops.PSD_Rpg_Stand_Run_Stops"),
			});
		TestGroundDatabaseGroup(
			TEXT("Sprint"),
			TEXT("Sprint"),
			{
				TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Sprint.PSD_Rpg_Stand_Sprint"),
			});

		FString CrouchingDatabasePath;
		if (TestTrue(
			TEXT("The crouching database property is readable"),
			ReadObjectProperty(
				PilotAnimDefaults,
				TEXT("CrouchingMotionMatchingDatabase"),
				CrouchingDatabasePath)))
		{
			TestEqual(
				TEXT("Crouching uses the project-local crouch database"),
				CrouchingDatabasePath,
				FString(TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Crouch.PSD_Rpg_Crouch")));
		}

		FString TurnInPlaceDatabasePath;
		if (TestTrue(
			TEXT("The turn-in-place database property is readable"),
			ReadObjectProperty(
				PilotAnimDefaults,
				TEXT("TurnInPlaceMotionMatchingDatabase"),
				TurnInPlaceDatabasePath)))
		{
			TestEqual(
				TEXT("Turn-in-place uses the exclusive project-local database"),
				TurnInPlaceDatabasePath,
				FString(TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_TurnInPlace.PSD_Rpg_Stand_TurnInPlace")));
		}

		TMap<FName, FString> GameplayTagMappings;
		if (TestTrue(
			TEXT("The gameplay-tag property map is readable"),
			ReadGameplayTagPropertyMappings(PilotAnimDefaults, GameplayTagMappings)))
		{
			static const TPair<FName, FString> ExpectedMappings[] = {
				{TEXT("bGameplayMovementStopped"), TEXT("Gameplay.MovementStopped")},
				{TEXT("bStateBlocking"), TEXT("State.Blocking")},
				{TEXT("bStateDead"), TEXT("State.Dead")},
				{TEXT("bStateStaggered"), TEXT("State.Staggered")},
				{TEXT("bStateGuardBroken"), TEXT("State.GuardBroken")},
			};
			TestEqual(
				TEXT("Exactly five gameplay tags gate cosmetic turn-in-place"),
				GameplayTagMappings.Num(),
				static_cast<int32>(UE_ARRAY_COUNT(ExpectedMappings)));
			for (const TPair<FName, FString>& ExpectedMapping : ExpectedMappings)
			{
				const FString* ActualTag = GameplayTagMappings.Find(ExpectedMapping.Key);
				if (TestNotNull(
					*FString::Printf(TEXT("%s has a gameplay-tag mapping"), *ExpectedMapping.Key.ToString()),
					ActualTag))
				{
					TestEqual(
						*FString::Printf(TEXT("%s mirrors the intended gameplay tag"), *ExpectedMapping.Key.ToString()),
						*ActualTag,
						ExpectedMapping.Value);
				}
			}
		}

		TArray<FString> AirborneDatabasePaths;
		if (TestTrue(
				TEXT("The airborne database property is readable"),
				ReadObjectArrayProperty(
					PilotAnimDefaults,
					TEXT("AirborneMotionMatchingDatabases"),
					AirborneDatabasePaths)))
		{
			TestEqual(TEXT("Exactly one airborne database is configured"), AirborneDatabasePaths.Num(), 1);
			if (AirborneDatabasePaths.Num() == 1)
			{
				TestEqual(
					TEXT("The airborne database contains only project-local jump starts and the fall loop"),
					AirborneDatabasePaths[0],
					FString(TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Jump.PSD_Rpg_Jump")));
			}
		}

		FString LandingDatabasePath;
		if (TestTrue(
			TEXT("The dedicated landing database property is readable"),
			ReadObjectProperty(
				PilotAnimDefaults,
				TEXT("LandingMotionMatchingDatabase"),
				LandingDatabasePath)))
		{
			TestEqual(
				TEXT("Grounded light lands use the dedicated project-local database"),
				LandingDatabasePath,
				FString(TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Idle_Lands_Light.PSD_Rpg_Stand_Idle_Lands_Light")));
		}
	}

	UEdGraph* AnimGraph = FindAnimGraph(PilotAnimBlueprint);
	if (TestNotNull(TEXT("The GASP AnimBlueprint has an AnimGraph"), AnimGraph))
	{
		UAnimGraphNode_MotionMatching* MotionMatchingNode =
			FindUniqueNode<UAnimGraphNode_MotionMatching>(*this, AnimGraph, TEXT("Motion Matching"));
		UEdGraphNode* OffsetRootBoneNode = FindUniqueNodeByClassName(
			*this,
			AnimGraph,
			TEXT("AnimGraphNode_OffsetRootBone"),
			TEXT("Offset Root Bone"));
		UAnimGraphNode_Slot* SlotNode =
			FindUniqueNode<UAnimGraphNode_Slot>(*this, AnimGraph, TEXT("Montage Slot"));
		UAnimGraphNode_LocalToComponentSpace* FootPlacementLocalToComponentNode =
			FindUniqueNode<UAnimGraphNode_LocalToComponentSpace>(
				*this,
				AnimGraph,
				TEXT("Foot Placement Local To Component"));
		UAnimGraphNode_RpgFootPlacement* RpgFootPlacementNode =
			FindUniqueNode<UAnimGraphNode_RpgFootPlacement>(
				*this,
				AnimGraph,
				TEXT("Project-local Foot Placement"));
		UAnimGraphNode_LegIK* LegIkNode =
			FindUniqueNode<UAnimGraphNode_LegIK>(*this, AnimGraph, TEXT("Leg IK"));
		UAnimGraphNode_ComponentToLocalSpace* FootPlacementComponentToLocalNode =
			FindUniqueNode<UAnimGraphNode_ComponentToLocalSpace>(
				*this,
				AnimGraph,
				TEXT("Foot Placement Component To Local"));
		UAnimGraphNode_PoseSearchHistoryCollector* PoseHistoryNode =
			FindUniqueNode<UAnimGraphNode_PoseSearchHistoryCollector>(*this, AnimGraph, TEXT("Pose History"));
		UAnimGraphNode_Root* RootNode =
			FindUniqueNode<UAnimGraphNode_Root>(*this, AnimGraph, TEXT("AnimGraph Root"));
		UK2Node_VariableGet* TrajectoryGetter =
			FindUniqueVariableGetter(*this, AnimGraph, TEXT("LocomotionTrajectory"));
		UK2Node_VariableGet* OffsetRootRotationModeGetter =
			FindUniqueVariableGetter(*this, AnimGraph, TEXT("OffsetRootRotationMode"));
		UK2Node_VariableGet* ResetOffsetRootGetter =
			FindUniqueVariableGetter(*this, AnimGraph, TEXT("bResetOffsetRootEveryFrame"));
		UK2Node_VariableGet* FootPlacementSnapshotGetter =
			FindUniqueVariableGetter(*this, AnimGraph, TEXT("FootPlacementSnapshot"));
		UK2Node_VariableGet* FootPlacementAlphaGetter =
			FindUniqueVariableGetter(*this, AnimGraph, TEXT("FootPlacementAlpha"));
		TestEqual(
			TEXT("The top-level pilot graph contains exactly nine pose nodes and five property getters"),
			AnimGraph->Nodes.Num(),
			14);
		const FAnimNode_PoseSearchHistoryCollector* PoseHistoryRuntimeNode =
			ReadRuntimeNode<FAnimNode_PoseSearchHistoryCollector>(PoseHistoryNode);
		if (TestNotNull(
			TEXT("The Pose History editor node stores the expected runtime node"),
			PoseHistoryRuntimeNode))
		{
			TestEqual(
				TEXT("Pose History retains two samples"),
				PoseHistoryRuntimeNode->PoseCount,
				2);
			TestTrue(
				TEXT("Pose History samples every animation update"),
				FMath::IsNearlyZero(PoseHistoryRuntimeNode->SamplingInterval));
			TestEqual(
				TEXT("Pose History collects exactly the bones required by the local schemas"),
				PoseHistoryRuntimeNode->CollectedBones.Num(),
				3);
			TSet<FName> CollectedPoseHistoryBones;
			for (const FBoneReference& BoneReference : PoseHistoryRuntimeNode->CollectedBones)
			{
				CollectedPoseHistoryBones.Add(BoneReference.BoneName);
			}
			TestEqual(
				TEXT("Pose History does not contain duplicate collected bones"),
				CollectedPoseHistoryBones.Num(),
				PoseHistoryRuntimeNode->CollectedBones.Num());
			for (const FName RequiredBone :
				{FName(TEXT("foot_l")), FName(TEXT("foot_r")), FName(TEXT("pelvis"))})
			{
				TestTrue(
					*FString::Printf(
						TEXT("Pose History collects schema-required bone %s"),
						*RequiredBone.ToString()),
					CollectedPoseHistoryBones.Contains(RequiredBone));
			}
			TestEqual(
				TEXT("The curated local schemas require no Pose History curves"),
				PoseHistoryRuntimeNode->CollectedCurves.Num(),
				0);
			TestTrue(
				TEXT("Pose History resets when it becomes relevant again"),
				PoseHistoryRuntimeNode->bResetOnBecomingRelevant);
			TestFalse(
				TEXT("Pose History does not retain animation scales"),
				PoseHistoryRuntimeNode->bStoreScales);
			TestTrue(
				TEXT("Pose History keeps the GASP root recovery time"),
				FMath::IsNearlyEqual(PoseHistoryRuntimeNode->RootBoneRecoveryTime, 0.3f));
			TestFalse(
				TEXT("Pose History consumes the project trajectory instead of generating one"),
				PoseHistoryRuntimeNode->bGenerateTrajectory);
			TestTrue(
				TEXT("The external trajectory is not speed-scaled inside Pose History"),
				FMath::IsNearlyEqual(PoseHistoryRuntimeNode->TrajectorySpeedMultiplier, 1.0f));
			TestEqual(
				TEXT("Pose History retains the UE/GASP history default"),
				PoseHistoryRuntimeNode->TrajectoryHistoryCount,
				10);
			TestEqual(
				TEXT("Pose History retains the UE/GASP prediction default"),
				PoseHistoryRuntimeNode->TrajectoryPredictionCount,
				8);
			TestTrue(
				TEXT("Pose History retains the UE/GASP prediction interval"),
				FMath::IsNearlyEqual(
					PoseHistoryRuntimeNode->PredictionSamplingInterval,
					0.4f));
		}
		int32 StockFootPlacementNodeCount = 0;
		int32 RpgFootPlacementNodeCount = 0;
		int32 LegIkNodeCount = 0;
		int32 OrientationWarpingNodeCount = 0;
		for (const UEdGraphNode* GraphNode : AnimGraph->Nodes)
		{
			if (!GraphNode)
			{
				continue;
			}

			StockFootPlacementNodeCount += GraphNode->GetClass()->GetFName() == TEXT("AnimGraphNode_FootPlacement");
			RpgFootPlacementNodeCount += GraphNode->GetClass() == UAnimGraphNode_RpgFootPlacement::StaticClass();
			LegIkNodeCount += GraphNode->GetClass() == UAnimGraphNode_LegIK::StaticClass();
			OrientationWarpingNodeCount += GraphNode->GetClass()->GetFName() == TEXT("AnimGraphNode_OrientationWarping");
		}
		TestEqual(
			TEXT("The pilot does not silently use Epic's worker-thread-unsafe Foot Placement node"),
			StockFootPlacementNodeCount,
			0);
		TestEqual(
			TEXT("The pilot uses exactly one project-local thread-safe Foot Placement node"),
			RpgFootPlacementNodeCount,
			1);
		TestEqual(
			TEXT("The project-local Foot Placement target is resolved by exactly one stock Leg IK node"),
			LegIkNodeCount,
			1);
		TestEqual(
			TEXT("The pilot does not apply incomplete top-level Orientation Warping"),
			OrientationWarpingNodeCount,
			0);
		if (FootPlacementLocalToComponentNode)
		{
			TestEqual(
				TEXT("Foot Placement uses the exact Local To Component conversion class"),
				FootPlacementLocalToComponentNode->GetClass(),
				UAnimGraphNode_LocalToComponentSpace::StaticClass());
		}
		if (RpgFootPlacementNode)
		{
			TestEqual(
				TEXT("Foot Placement uses the exact project-local editor node class"),
				RpgFootPlacementNode->GetClass(),
				UAnimGraphNode_RpgFootPlacement::StaticClass());
		}
		if (LegIkNode)
		{
			TestEqual(
				TEXT("Leg IK uses Epic's exact stock editor node class"),
				LegIkNode->GetClass(),
				UAnimGraphNode_LegIK::StaticClass());
		}
		if (FootPlacementComponentToLocalNode)
		{
			TestEqual(
				TEXT("Foot Placement uses the exact Component To Local conversion class"),
				FootPlacementComponentToLocalNode->GetClass(),
				UAnimGraphNode_ComponentToLocalSpace::StaticClass());
		}

		if (MotionMatchingNode)
		{
			TestEqual(
				TEXT("Motion Matching runs the RPG database selector on update"),
				MotionMatchingNode->UpdateFunction.GetMemberName(),
				GET_FUNCTION_NAME_CHECKED(URpgAnimInstance, UpdateGaspMotionMatching));
			TestTrue(
				TEXT("The Motion Matching update binding uses the AnimInstance self context"),
				MotionMatchingNode->UpdateFunction.IsSelfContext());
			TestNotNull(
				TEXT("The Motion Matching update binding resolves on the generated AnimInstance class"),
				MotionMatchingNode->UpdateFunction.ResolveMember<UFunction>(PilotAnimBlueprint->GeneratedClass));
			const FMemberReference* MotionMatchingStateUpdatedFunction = nullptr;
			if (TestTrue(
				TEXT("The specialized post-search Motion Matching callback binding is readable"),
				ReadMemberReferenceProperty(
					MotionMatchingNode,
					TEXT("OnMotionMatchingStateUpdatedFunction"),
					MotionMatchingStateUpdatedFunction)))
			{
				TestTrue(
					TEXT("The specialized post-search Motion Matching callback remains deliberately unbound"),
					MotionMatchingStateUpdatedFunction->GetMemberName().IsNone());
				TestNull(
					TEXT("No specialized post-search Motion Matching callback resolves on the generated class"),
					MotionMatchingStateUpdatedFunction->ResolveMember<UFunction>(
						PilotAnimBlueprint->GeneratedClass));
			}
			float MotionMatchingBlendTime = 0.0f;
			if (TestTrue(
				TEXT("The Motion Matching blend time is readable"),
				ReadStructFloatProperty(
					MotionMatchingNode,
					TEXT("Node"),
					TEXT("BlendTime"),
					MotionMatchingBlendTime)))
			{
				TestEqual(
					TEXT("Motion Matching uses the responsive pilot blend time"),
					MotionMatchingBlendTime,
					0.2f);
			}

			UObject* BlendProfileObject = nullptr;
			if (TestTrue(
				TEXT("The Motion Matching blend profile is readable"),
				ReadStructObjectProperty(
					MotionMatchingNode,
					TEXT("Node"),
					TEXT("BlendProfile"),
					BlendProfileObject)))
			{
				TestNull(
					TEXT("Motion Matching uses uniform 0.2-second blending after the FastFeet TIR regression"),
					BlendProfileObject);
			}

			TestPerSampleBlendStackContract(*this, PilotAnimBlueprint, MotionMatchingNode);
		}

		if (OffsetRootBoneNode)
		{
			bool bResetEveryFrame = true;
			if (TestTrue(
				TEXT("The Offset Root Bone reset default is readable"),
				ReadStructBoolProperty(
					OffsetRootBoneNode,
					TEXT("Node"),
					TEXT("bResetEveryFrame"),
					bResetEveryFrame)))
			{
				TestFalse(TEXT("Offset Root Bone keeps reset disabled unless the runtime requests one frame"), bResetEveryFrame);
			}

			FString RotationMode;
			if (TestTrue(
				TEXT("The Offset Root Bone rotation mode is readable"),
				ReadStructPropertyText(
					OffsetRootBoneNode,
					TEXT("Node"),
					TEXT("RotationMode"),
					RotationMode)))
			{
				TestEqual(
					TEXT("Controller-facing locomotion interpolates visual root rotation"),
					RotationMode,
					FString(TEXT("Interpolate")));
			}

			float RotationHalfLife = 0.0f;
			if (TestTrue(
				TEXT("The Offset Root Bone rotation half-life is readable"),
				ReadStructFloatProperty(
					OffsetRootBoneNode,
					TEXT("Node"),
					TEXT("RotationHalfLife"),
					RotationHalfLife)))
			{
				TestEqual(
					TEXT("Visual root rotation catches controller-facing movement promptly"),
					RotationHalfLife,
					0.1f);
			}

			const UEdGraphPin* RotationModePin =
				OffsetRootBoneNode->FindPin(TEXT("RotationMode"), EGPD_Input);
			if (TestNotNull(TEXT("Offset Root Bone exposes its RotationMode input"), RotationModePin))
			{
				TestFalse(TEXT("The dynamic RotationMode input is visible"), RotationModePin->bHidden);
				TestEqual(
					TEXT("RotationMode uses an enum pin"),
					RotationModePin->PinType.PinCategory,
					UEdGraphSchema_K2::PC_Byte);
				TestEqual(
					TEXT("RotationMode uses EOffsetRootBoneMode"),
					RotationModePin->PinType.PinSubCategoryObject.Get(),
					static_cast<UObject*>(StaticEnum<EOffsetRootBoneMode>()));
			}

			const UEdGraphPin* ResetEveryFramePin =
				OffsetRootBoneNode->FindPin(TEXT("bResetEveryFrame"), EGPD_Input);
			if (TestNotNull(TEXT("Offset Root Bone exposes its bResetEveryFrame input"), ResetEveryFramePin))
			{
				TestFalse(TEXT("The dynamic reset input is visible"), ResetEveryFramePin->bHidden);
				TestEqual(
					TEXT("bResetEveryFrame uses a boolean pin"),
					ResetEveryFramePin->PinType.PinCategory,
					UEdGraphSchema_K2::PC_Boolean);
			}
		}

		const FAnimNode_RpgFootPlacement* RpgFootPlacement =
			ReadRuntimeNode<FAnimNode_RpgFootPlacement>(RpgFootPlacementNode);
		if (TestNotNull(
				TEXT("The project-local Foot Placement runtime node is readable"),
				RpgFootPlacement))
		{
			TestEqual(
				TEXT("Foot Placement lowers the mannequin pelvis"),
				RpgFootPlacement->PelvisBone.BoneName,
				FName(TEXT("pelvis")));
			TestEqual(
				TEXT("Foot Placement has exactly two leg definitions"),
				RpgFootPlacement->LegsDefinition.Num(),
				2);
			if (RpgFootPlacement->LegsDefinition.Num() == 2)
			{
				TestEqual(
					TEXT("Foot Placement reads the authored left FK ankle first"),
					RpgFootPlacement->LegsDefinition[0].FKFootBone.BoneName,
					FName(TEXT("foot_l")));
				TestEqual(
					TEXT("Foot Placement drives the left IK foot first"),
					RpgFootPlacement->LegsDefinition[0].IKFootBone.BoneName,
					FName(TEXT("ik_foot_l")));
				TestEqual(
					TEXT("Foot Placement pivots the left target around ball_l"),
					RpgFootPlacement->LegsDefinition[0].BallBone.BoneName,
					FName(TEXT("ball_l")));
				TestEqual(
					TEXT("Foot Placement reads the authored right FK ankle second"),
					RpgFootPlacement->LegsDefinition[1].FKFootBone.BoneName,
					FName(TEXT("foot_r")));
				TestEqual(
					TEXT("Foot Placement drives the right IK foot second"),
					RpgFootPlacement->LegsDefinition[1].IKFootBone.BoneName,
					FName(TEXT("ik_foot_r")));
				TestEqual(
					TEXT("Foot Placement pivots the right target around ball_r"),
					RpgFootPlacement->LegsDefinition[1].BallBone.BoneName,
					FName(TEXT("ball_r")));
			}
			TestEqual(TEXT("Foot Placement clamps translation to 50 cm"), RpgFootPlacement->MaxFootTranslation, 50.0f);
			TestEqual(TEXT("Foot Placement clamps alignment to 60 degrees"), RpgFootPlacement->MaxFootRotation, 60.0f);
			TestEqual(TEXT("Foot Placement clamps pelvis lowering to 50 cm"), RpgFootPlacement->MaxPelvisOffset, 50.0f);
			TestEqual(TEXT("Foot Placement raw-gates plants within 10 cm"), RpgFootPlacement->PlantDistanceThreshold, 10.0f);
			TestEqual(TEXT("Foot Placement raw-gates locks outside a 20 cm radius"), RpgFootPlacement->UnplantRadius, 20.0f);
			TestEqual(TEXT("Foot Placement smooths pelvis correction with a 0.08-second half-life"), RpgFootPlacement->PelvisBlendHalfLife, 0.08f);
			TestEqual(TEXT("Foot Placement limits pelvis correction to 120 cm/s"), RpgFootPlacement->MaxPelvisSpeed, 120.0f);
			TestEqual(TEXT("Foot Placement uses a float graph alpha"), RpgFootPlacement->AlphaInputType, EAnimAlphaInputType::Float);
			TestEqual(TEXT("Foot Placement keeps its graph-driven alpha default at one"), RpgFootPlacement->Alpha, 1.0f);
			TestEqual(TEXT("Foot Placement has no hidden LOD cutoff"), RpgFootPlacement->LODThreshold, INDEX_NONE);
		}

		const FAnimNode_LegIK* LegIk = ReadRuntimeNode<FAnimNode_LegIK>(LegIkNode);
		if (TestNotNull(TEXT("The stock Leg IK runtime node is readable"), LegIk))
		{
			TestEqual(TEXT("Leg IK reaches within 0.01 cm"), LegIk->ReachPrecision, 0.01f);
			TestEqual(TEXT("Leg IK uses twelve solver iterations"), LegIk->MaxIterations, 12);
			TestEqual(TEXT("Leg IK softens at 99 percent extension"), LegIk->SoftPercentLength, 0.99f);
			TestEqual(TEXT("Leg IK applies full soft-extension correction"), LegIk->SoftAlpha, 1.0f);
			TestEqual(TEXT("Leg IK has exactly two leg definitions"), LegIk->LegsDefinition.Num(), 2);
			static const FName ExpectedIkFeet[] = {TEXT("ik_foot_l"), TEXT("ik_foot_r")};
			static const FName ExpectedFkFeet[] = {TEXT("foot_l"), TEXT("foot_r")};
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedIkFeet) && Index < LegIk->LegsDefinition.Num(); ++Index)
			{
				const FAnimLegIKDefinition& LegDefinition = LegIk->LegsDefinition[Index];
				TestEqual(
					*FString::Printf(TEXT("Leg IK target %d is stable"), Index),
					LegDefinition.IKFootBone.BoneName,
					ExpectedIkFeet[Index]);
				TestEqual(
					*FString::Printf(TEXT("Leg IK FK ankle %d is stable"), Index),
					LegDefinition.FKFootBone.BoneName,
					ExpectedFkFeet[Index]);
				TestEqual(
					*FString::Printf(TEXT("Leg IK limb %d resolves two bones"), Index),
					LegDefinition.NumBonesInLimb,
					2);
				TestEqual(
					*FString::Printf(TEXT("Leg IK limb %d keeps a 15-degree compression limit"), Index),
					LegDefinition.MinRotationAngle,
					15.0f);
				TestEqual(
					*FString::Printf(TEXT("Leg IK foot %d faces along Y"), Index),
					LegDefinition.FootBoneForwardAxis.GetValue(),
					EAxis::Y);
				TestEqual(
					*FString::Printf(TEXT("Leg IK knee %d hinges around Z"), Index),
					LegDefinition.HingeRotationAxis.GetValue(),
					EAxis::Z);
				TestTrue(
					*FString::Printf(TEXT("Leg IK limb %d prevents backward folding"), Index),
					LegDefinition.bEnableRotationLimit);
				TestTrue(
					*FString::Printf(TEXT("Leg IK limb %d keeps knee twist correction"), Index),
					LegDefinition.bEnableKneeTwistCorrection);
				TestTrue(
					*FString::Printf(TEXT("Leg IK limb %d has no authored twist override curve"), Index),
					LegDefinition.TwistOffsetCurveName.IsNone());
			}
			TestEqual(TEXT("Leg IK uses the shared float graph alpha"), LegIk->AlphaInputType, EAnimAlphaInputType::Float);
			TestEqual(TEXT("Leg IK keeps its graph-driven alpha default at one"), LegIk->Alpha, 1.0f);
			TestEqual(TEXT("Leg IK has no hidden LOD cutoff"), LegIk->LODThreshold, INDEX_NONE);
		}

		if (SlotNode)
		{
			TestEqual(
				TEXT("Combat and harvesting montages keep the authoritative DefaultSlot"),
				SlotNode->Node.SlotName,
				FName(TEXT("DefaultSlot")));
		}

		if (OffsetRootRotationModeGetter)
		{
			const UEdGraphPin* RotationModeOutput =
				OffsetRootRotationModeGetter->FindPin(TEXT("OffsetRootRotationMode"), EGPD_Output);
			if (TestNotNull(TEXT("OffsetRootRotationMode getter exposes its value"), RotationModeOutput))
			{
				TestEqual(
					TEXT("The runtime rotation-mode output uses an enum pin"),
					RotationModeOutput->PinType.PinCategory,
					UEdGraphSchema_K2::PC_Byte);
				TestEqual(
					TEXT("The runtime rotation-mode output uses EOffsetRootBoneMode"),
					RotationModeOutput->PinType.PinSubCategoryObject.Get(),
					static_cast<UObject*>(StaticEnum<EOffsetRootBoneMode>()));
			}
		}
		if (ResetOffsetRootGetter)
		{
			const UEdGraphPin* ResetOutput =
				ResetOffsetRootGetter->FindPin(TEXT("bResetOffsetRootEveryFrame"), EGPD_Output);
			if (TestNotNull(TEXT("bResetOffsetRootEveryFrame getter exposes its value"), ResetOutput))
			{
				TestEqual(
					TEXT("The runtime reset output uses a boolean pin"),
					ResetOutput->PinType.PinCategory,
					UEdGraphSchema_K2::PC_Boolean);
			}
		}

		if (RpgFootPlacementNode)
		{
			const UEdGraphPin* SnapshotInput =
				RpgFootPlacementNode->FindPin(TEXT("Snapshot"), EGPD_Input);
			if (TestNotNull(TEXT("Foot Placement exposes its immutable snapshot input"), SnapshotInput))
			{
				TestFalse(TEXT("The Foot Placement snapshot input is visible"), SnapshotInput->bHidden);
				TestEqual(
					TEXT("The Foot Placement snapshot uses a struct pin"),
					SnapshotInput->PinType.PinCategory,
					UEdGraphSchema_K2::PC_Struct);
				TestEqual(
					TEXT("The Foot Placement snapshot pin uses FRpgFootPlacementSnapshot"),
					SnapshotInput->PinType.PinSubCategoryObject.Get(),
					static_cast<UObject*>(FRpgFootPlacementSnapshot::StaticStruct()));
			}

			const UEdGraphPin* AlphaInput = RpgFootPlacementNode->FindPin(TEXT("Alpha"), EGPD_Input);
			if (TestNotNull(TEXT("Foot Placement exposes its shared alpha input"), AlphaInput))
			{
				TestFalse(TEXT("The Foot Placement alpha input is visible"), AlphaInput->bHidden);
				TestEqual(
					TEXT("The Foot Placement alpha uses a real-number pin"),
					AlphaInput->PinType.PinCategory,
					UEdGraphSchema_K2::PC_Real);
				TestEqual(
					TEXT("The Foot Placement alpha is single precision"),
					AlphaInput->PinType.PinSubCategory,
					UEdGraphSchema_K2::PC_Float);
			}
		}

		if (LegIkNode)
		{
			const UEdGraphPin* AlphaInput = LegIkNode->FindPin(TEXT("Alpha"), EGPD_Input);
			if (TestNotNull(TEXT("Leg IK exposes its shared alpha input"), AlphaInput))
			{
				TestFalse(TEXT("The Leg IK alpha input is visible"), AlphaInput->bHidden);
				TestEqual(
					TEXT("The Leg IK alpha uses a real-number pin"),
					AlphaInput->PinType.PinCategory,
					UEdGraphSchema_K2::PC_Real);
				TestEqual(
					TEXT("The Leg IK alpha is single precision"),
					AlphaInput->PinType.PinSubCategory,
					UEdGraphSchema_K2::PC_Float);
			}
		}

		if (FootPlacementSnapshotGetter)
		{
			const UEdGraphPin* SnapshotOutput =
				FootPlacementSnapshotGetter->FindPin(TEXT("FootPlacementSnapshot"), EGPD_Output);
			if (TestNotNull(TEXT("FootPlacementSnapshot getter exposes its value"), SnapshotOutput))
			{
				TestEqual(
					TEXT("The snapshot getter uses a struct pin"),
					SnapshotOutput->PinType.PinCategory,
					UEdGraphSchema_K2::PC_Struct);
				TestEqual(
					TEXT("The snapshot getter exposes FRpgFootPlacementSnapshot"),
					SnapshotOutput->PinType.PinSubCategoryObject.Get(),
					static_cast<UObject*>(FRpgFootPlacementSnapshot::StaticStruct()));
			}
		}

		if (FootPlacementAlphaGetter)
		{
			const UEdGraphPin* AlphaOutput =
				FootPlacementAlphaGetter->FindPin(TEXT("FootPlacementAlpha"), EGPD_Output);
			if (TestNotNull(TEXT("FootPlacementAlpha getter exposes its value"), AlphaOutput))
			{
				TestEqual(
					TEXT("The Foot Placement alpha getter uses a real-number pin"),
					AlphaOutput->PinType.PinCategory,
					UEdGraphSchema_K2::PC_Real);
				TestEqual(
					TEXT("The Foot Placement alpha getter is single precision"),
					AlphaOutput->PinType.PinSubCategory,
					UEdGraphSchema_K2::PC_Float);
			}
		}

		TestExclusiveLink(
			*this,
			TEXT("Motion Matching feeds Offset Root Bone"),
			MotionMatchingNode,
			TEXT("Pose"),
			OffsetRootBoneNode,
			TEXT("Source"));
		TestExclusiveLink(
			*this,
			TEXT("Offset Root Bone feeds DefaultSlot"),
			OffsetRootBoneNode,
			TEXT("Pose"),
			SlotNode,
			TEXT("Source"));
		TestExclusiveLink(
			*this,
			TEXT("DefaultSlot feeds the Foot Placement component-space conversion"),
			SlotNode,
			TEXT("Pose"),
			FootPlacementLocalToComponentNode,
			TEXT("LocalPose"));
		TestExclusiveLink(
			*this,
			TEXT("The component-space conversion feeds project-local Foot Placement"),
			FootPlacementLocalToComponentNode,
			TEXT("ComponentPose"),
			RpgFootPlacementNode,
			TEXT("ComponentPose"));
		TestExclusiveLink(
			*this,
			TEXT("Project-local Foot Placement feeds stock Leg IK"),
			RpgFootPlacementNode,
			TEXT("Pose"),
			LegIkNode,
			TEXT("ComponentPose"));
		TestExclusiveLink(
			*this,
			TEXT("Stock Leg IK feeds the local-space conversion"),
			LegIkNode,
			TEXT("Pose"),
			FootPlacementComponentToLocalNode,
			TEXT("ComponentPose"));
		TestExclusiveLink(
			*this,
			TEXT("The Foot Placement local-space result feeds Pose History"),
			FootPlacementComponentToLocalNode,
			TEXT("Pose"),
			PoseHistoryNode,
			TEXT("Source"));
		TestExclusiveLink(
			*this,
			TEXT("Pose History feeds the AnimGraph root"),
			PoseHistoryNode,
			TEXT("Pose"),
			RootNode,
			TEXT("Result"));
		TestExclusiveLink(
			*this,
			TEXT("The game-thread trajectory snapshot feeds Pose History"),
			TrajectoryGetter,
			TEXT("LocomotionTrajectory"),
			PoseHistoryNode,
			TEXT("TransformTrajectory"));
		TestExclusiveLink(
			*this,
			TEXT("The runtime turn state drives Offset Root Bone rotation mode"),
			OffsetRootRotationModeGetter,
			TEXT("OffsetRootRotationMode"),
			OffsetRootBoneNode,
			TEXT("RotationMode"));
		TestExclusiveLink(
			*this,
			TEXT("The one-frame hard-reset request drives Offset Root Bone"),
			ResetOffsetRootGetter,
			TEXT("bResetOffsetRootEveryFrame"),
			OffsetRootBoneNode,
			TEXT("bResetEveryFrame"));
		TestExactOutputLinks(
			*this,
			TEXT("The immutable game-thread snapshot drives only project-local Foot Placement"),
			FootPlacementSnapshotGetter,
			TEXT("FootPlacementSnapshot"),
			{{RpgFootPlacementNode, TEXT("Snapshot")}});
		TestExactOutputLinks(
			*this,
			TEXT("The montage-safe Foot Placement alpha gates both skeletal controls"),
			FootPlacementAlphaGetter,
			TEXT("FootPlacementAlpha"),
			{{RpgFootPlacementNode, TEXT("Alpha")}, {LegIkNode, TEXT("Alpha")}});
	}

	TestEqual(
		TEXT("The pilot character is a direct child of the existing player Blueprint"),
		PilotCharacterBlueprint->ParentClass.Get(),
		BaseCharacterBlueprint->GeneratedClass.Get());
	TestTrue(
		TEXT("The pilot PawnClass differs from the default PawnClass"),
		PilotPawnData->PawnClass != BasePawnData->PawnClass);
	TestEqual(
		TEXT("The pilot PawnData selects the isolated character"),
		PilotPawnData->PawnClass.Get(),
		PilotCharacterBlueprint->GeneratedClass.Get());
	TestTrue(
		TEXT("The default PawnData keeps legacy-safe CombatStrafe rotation"),
		BasePawnData->DefaultRotationMode ==
			ERpgCharacterRotationMode::CombatStrafe);
	TestTrue(
		TEXT("Only the GASP pilot PawnData opts into Free rotation"),
		PilotPawnData->DefaultRotationMode == ERpgCharacterRotationMode::Free);
	TestEqual(TEXT("PawnData TeamId is unchanged"), PilotPawnData->TeamId, BasePawnData->TeamId);
	TestEqual(
		TEXT("PawnData TagRelationshipMapping is unchanged"),
		PilotPawnData->TagRelationshipMapping.Get(),
		BasePawnData->TagRelationshipMapping.Get());
	TestEqual(
		TEXT("PawnData InputConfig is unchanged"),
		PilotPawnData->InputConfig.Get(),
		BasePawnData->InputConfig.Get());
	TestTrue(TEXT("PawnData AbilitySets are unchanged"), PilotPawnData->AbilitySets == BasePawnData->AbilitySets);
	TestEqual(
		TEXT("PawnData camera mode is unchanged"),
		PilotPawnData->DefaultCameraMode,
		BasePawnData->DefaultCameraMode);
	TestEqual(
		TEXT("PawnData inventory layout is unchanged"),
		PilotPawnData->InventoryLayoutDefinition.Get(),
		BasePawnData->InventoryLayoutDefinition.Get());

	ACharacter* BaseCharacterDefaults =
		BaseCharacterBlueprint->GeneratedClass
			? Cast<ACharacter>(BaseCharacterBlueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
	ACharacter* PilotCharacterDefaults =
		PilotCharacterBlueprint->GeneratedClass
			? Cast<ACharacter>(PilotCharacterBlueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
	if (TestNotNull(TEXT("The default character CDO loads"), BaseCharacterDefaults) &&
		TestNotNull(TEXT("The pilot character CDO loads"), PilotCharacterDefaults))
	{
		USkeletalMeshComponent* BaseMesh = BaseCharacterDefaults->GetMesh();
		USkeletalMeshComponent* PilotMesh = PilotCharacterDefaults->GetMesh();
		if (TestNotNull(TEXT("The default CharacterMesh0 exists"), BaseMesh) &&
			TestNotNull(TEXT("The pilot CharacterMesh0 exists"), PilotMesh))
		{
			TestEqual(TEXT("The pilot keeps the authoritative skeletal mesh"), PilotMesh->GetSkeletalMeshAsset(), BaseMesh->GetSkeletalMeshAsset());
			TestTrue(TEXT("The pilot keeps the mesh transform"), PilotMesh->GetRelativeTransform().Equals(BaseMesh->GetRelativeTransform()));
			TestEqual(TEXT("The pilot keeps the mesh collision profile"), PilotMesh->GetCollisionProfileName(), BaseMesh->GetCollisionProfileName());
			TestEqual(
				TEXT("The pilot keeps the mesh visibility tick policy"),
				PilotMesh->VisibilityBasedAnimTickOption,
				BaseMesh->VisibilityBasedAnimTickOption);
			TestEqual(
				TEXT("The pilot keeps animation update-rate optimization policy"),
				static_cast<bool>(PilotMesh->bEnableUpdateRateOptimizations),
				static_cast<bool>(BaseMesh->bEnableUpdateRateOptimizations));
			TestEqual(
				TEXT("Only the pilot mesh AnimClass changes"),
				PilotMesh->GetAnimClass(),
				PilotAnimBlueprint->GeneratedClass.Get());
		}
	}

	TestEqual(
		TEXT("The prototype Experience still selects the default PawnData"),
		BaseExperience->DefaultPawnData.Get(),
		static_cast<const URpgPawnData*>(BasePawnData));
	TestEqual(
		TEXT("The pilot Experience selects only the pilot PawnData"),
		PilotExperience->DefaultPawnData.Get(),
		static_cast<const URpgPawnData*>(PilotPawnData));
	TestTrue(
		TEXT("The pilot Experience keeps the prototype GameFeature order"),
		PilotExperience->GameFeaturesToEnable == BaseExperience->GameFeaturesToEnable);
	TestTrue(
		TEXT("The pilot Experience keeps the prototype ActionSets"),
		PilotExperience->ActionSets == BaseExperience->ActionSets);
	TestEqual(
		TEXT("The pilot Experience keeps the prototype action count"),
		PilotExperience->Actions.Num(),
		BaseExperience->Actions.Num());
	for (int32 Index = 0;
		Index < PilotExperience->Actions.Num() && Index < BaseExperience->Actions.Num();
		++Index)
	{
		FString Mismatch;
		const bool bActionsEquivalent = AreActionPropertiesEquivalent(
			BaseExperience->Actions[Index],
			PilotExperience->Actions[Index],
			Mismatch);
		TestTrue(
			*FString::Printf(
				TEXT("Experience action %d is an unchanged duplicate%s%s"),
				Index,
				Mismatch.IsEmpty() ? TEXT("") : TEXT(": "),
				*Mismatch),
			bActionsEquivalent);
	}
	TestEqual(
		TEXT("The pilot Experience is registered with the expected primary-asset identity"),
		PilotExperience->GetPrimaryAssetId().ToString(),
		FString(TEXT("RpgExperienceDefinition:RpgGaspPilotExperience")));

	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.ScanPathsSynchronous(
		{
			FString(TEXT("/Game/SurvivalRpg/Characters/Mannequins/Anims/GASP")),
			FString(TEXT("/Game/SurvivalRpg/Core/Character/GASP")),
			FString(TEXT("/Game/SurvivalRpg/System/Experiences")),
			FString(TEXT("/RpgGaspLocomotion")),
		},
		true,
		false);
	AssetRegistry.WaitForCompletion();

	const FName PilotRootPackages[] = {
		FName(PilotAnimBlueprintPackage),
		FName(PilotCharacterBlueprintPackage),
		FName(PilotPawnDataPackage),
		FName(PilotExperiencePackage),
	};
	TArray<FName> PilotAnimBlueprintDependencies;
	AssetRegistry.GetDependencies(
		FName(PilotAnimBlueprintPackage),
		PilotAnimBlueprintDependencies,
		UE::AssetRegistry::EDependencyCategory::Package);
	TestTrue(
		TEXT("The pilot AnimBlueprint directly records the loadable uncooked AnimGraph module"),
		PilotAnimBlueprintDependencies.Contains(FName(TEXT("/Script/SurvivalRpgAnimGraph"))));
	TestFalse(
		TEXT("The pilot AnimBlueprint never directly depends on the full editor-only module"),
		PilotAnimBlueprintDependencies.Contains(FName(TEXT("/Script/SurvivalRpgEditor"))));
	TSet<FName> DependencyClosure;
	GatherPilotDependencyClosure(AssetRegistry, PilotRootPackages, DependencyClosure);
	for (const FName PackageName : DependencyClosure)
	{
		TestFalse(
			*FString::Printf(
				TEXT("The isolated pilot has no GASP sample dependency on %s"),
				*PackageName.ToString()),
			IsForbiddenPilotDependency(PackageName.ToString()));
	}

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgGaspRotationModeAssetContractTest,
	"SurvivalRpg.Animation.Gasp.RotationModeAssetContract",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgGaspRotationModeAssetContractTest::RunTest(const FString& Parameters)
{
	using namespace RpgGaspPilotAssetTests;

	const UInputAction* CombatStanceAction = LoadRequiredAsset<UInputAction>(
		*this,
		CombatStanceInputActionObject,
		TEXT("The project-local combat-stance InputAction loads"));
	const UInputMappingContext* CombatInputMappingContext =
		LoadRequiredAsset<UInputMappingContext>(
			*this,
			CombatInputMappingContextObject,
			TEXT("The combat InputMappingContext loads"));
	const URpgInputConfig* CombatInputConfig = LoadRequiredAsset<URpgInputConfig>(
		*this,
		CombatInputConfigObject,
		TEXT("The combat InputConfig loads"));
	const UBlueprint* BasicWeaponAttackAbility = LoadRequiredAsset<UBlueprint>(
		*this,
		BasicWeaponAttackAbilityObject,
		TEXT("The basic weapon attack ability Blueprint loads"));
	const UBlueprint* CombatBlockAbility = LoadRequiredAsset<UBlueprint>(
		*this,
		CombatBlockAbilityObject,
		TEXT("The combat block ability Blueprint loads"));

	const UGameplayAbility* BasicWeaponAttackDefaults =
		BasicWeaponAttackAbility && BasicWeaponAttackAbility->GeneratedClass
			? Cast<UGameplayAbility>(
				BasicWeaponAttackAbility->GeneratedClass->GetDefaultObject())
			: nullptr;
	const UGameplayAbility* CombatBlockDefaults =
		CombatBlockAbility && CombatBlockAbility->GeneratedClass
			? Cast<UGameplayAbility>(
				CombatBlockAbility->GeneratedClass->GetDefaultObject())
			: nullptr;
	TestNotNull(
		TEXT("The basic weapon attack ability defaults load"),
		BasicWeaponAttackDefaults);
	TestNotNull(
		TEXT("The combat block ability defaults load"),
		CombatBlockDefaults);

	if (!CombatStanceAction ||
		!CombatInputMappingContext ||
		!CombatInputConfig ||
		!BasicWeaponAttackDefaults ||
		!CombatBlockDefaults)
	{
		return false;
	}

	TestTrue(
		TEXT("The combat-stance InputAction is a Boolean action"),
		CombatStanceAction->ValueType == EInputActionValueType::Boolean);

	int32 RelatedEnhancedInputMappings = 0;
	for (const FEnhancedActionKeyMapping& Mapping :
		CombatInputMappingContext->GetMappings())
	{
		const bool bUsesCombatStanceAction =
			Mapping.Action.Get() == CombatStanceAction;
		const bool bUsesMiddleMouseButton =
			Mapping.Key == EKeys::MiddleMouseButton;
		if (!bUsesCombatStanceAction && !bUsesMiddleMouseButton)
		{
			continue;
		}

		++RelatedEnhancedInputMappings;
		TestTrue(
			TEXT("The combat-stance mapping uses IA_ToggleCombatStance"),
			bUsesCombatStanceAction);
		TestTrue(
			TEXT("The combat-stance mapping uses exactly MiddleMouseButton"),
			bUsesMiddleMouseButton);
	}
	TestEqual(
		TEXT("IMC_Combat owns one exclusive combat-stance Action-Key mapping"),
		RelatedEnhancedInputMappings,
		1);

	const FGameplayTag ToggleCombatInputTag =
		FGameplayTag::RequestGameplayTag(
			FName(ToggleCombatInputTagName),
			/*ErrorIfNotFound=*/ false);
	if (!TestTrue(
			TEXT("The native combat-stance input tag is registered"),
			ToggleCombatInputTag.IsValid()))
	{
		return false;
	}
	TestEqual(
		TEXT("The native combat-stance input tag keeps its exact name"),
		ToggleCombatInputTag.ToString(),
		FString(ToggleCombatInputTagName));

	int32 RelatedNativeMappings = 0;
	for (const FRpgInputAction& Mapping : CombatInputConfig->NativeInputActions)
	{
		const bool bUsesCombatStanceAction =
			Mapping.InputAction.Get() == CombatStanceAction;
		const bool bUsesToggleCombatTag =
			Mapping.InputTag == ToggleCombatInputTag;
		if (!bUsesCombatStanceAction && !bUsesToggleCombatTag)
		{
			continue;
		}

		++RelatedNativeMappings;
		TestTrue(
			TEXT("The native combat-stance mapping uses its exact InputAction"),
			bUsesCombatStanceAction);
		TestTrue(
			TEXT("The native combat-stance mapping uses its exact input tag"),
			bUsesToggleCombatTag);
	}
	TestEqual(
		TEXT("DA_InputConfig_Combat owns one combat-stance native mapping"),
		RelatedNativeMappings,
		1);
	TestEqual(
		TEXT("The combat-stance native tag resolves to its exact InputAction"),
		CombatInputConfig->FindNativeInputActionForTag(
			ToggleCombatInputTag,
			/*bLogNotFound=*/ false),
		CombatStanceAction);

	int32 RelatedAbilityInputMappings = 0;
	for (const FRpgInputAction& Mapping : CombatInputConfig->AbilityInputActions)
	{
		RelatedAbilityInputMappings +=
			Mapping.InputAction.Get() == CombatStanceAction ||
				Mapping.InputTag == ToggleCombatInputTag
				? 1
				: 0;
	}
	TestEqual(
		TEXT("The combat-stance toggle is not routed through GAS ability input"),
		RelatedAbilityInputMappings,
		0);

	const FGameplayTag CombatStrafeTag =
		FGameplayTag::RequestGameplayTag(
			FName(CombatStrafeTagName),
			/*ErrorIfNotFound=*/ false);
	if (!TestTrue(
			TEXT("The CombatStrafe state tag is registered"),
			CombatStrafeTag.IsValid()))
	{
		return false;
	}

	const auto TestCombatStrafeActivationTag =
		[this, CombatStrafeTag](
			const TCHAR* AbilityLabel,
			const UGameplayAbility* AbilityDefaults)
		{
			const FGameplayTagContainer* ActivationOwnedTags =
				ReadGameplayTagContainerProperty(
					AbilityDefaults,
					TEXT("ActivationOwnedTags"));
			if (TestNotNull(
					*FString::Printf(
						TEXT("%s exposes ActivationOwnedTags"),
						AbilityLabel),
					ActivationOwnedTags))
			{
				TestTrue(
					*FString::Printf(
						TEXT("%s owns CombatStrafe while active"),
						AbilityLabel),
					ActivationOwnedTags->HasTagExact(CombatStrafeTag));
			}
		};
	TestCombatStrafeActivationTag(
		TEXT("GA_BasicWeaponAttack"),
		BasicWeaponAttackDefaults);
	TestCombatStrafeActivationTag(
		TEXT("GA_Combat_Block"),
		CombatBlockDefaults);

	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
