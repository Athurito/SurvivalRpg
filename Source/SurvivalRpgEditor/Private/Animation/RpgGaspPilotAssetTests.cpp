#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimBlueprint.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_MotionMatching.h"
#include "AnimGraphNode_PoseSearchHistoryCollector.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_Slot.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Components/SkeletalMeshComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "GameFeatureAction.h"
#include "GameFramework/Character.h"
#include "K2Node_VariableGet.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "Modules/ModuleManager.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "SurvivalRpg/Camera/RpgCameraMode.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
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

		TArray<FString> GroundDatabasePaths;
		if (TestTrue(
				TEXT("The grounded database property is readable"),
				ReadObjectArrayProperty(
					PilotAnimDefaults,
					TEXT("GroundMotionMatchingDatabases"),
					GroundDatabasePaths)))
		{
			static const TCHAR* const ExpectedGroundDatabases[] = {
				TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Idle.PSD_Rpg_Stand_Idle"),
				TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Walk.PSD_Rpg_Stand_Walk"),
				TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run.PSD_Rpg_Stand_Run"),
				TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Sprint.PSD_Rpg_Stand_Sprint"),
			};
			TestEqual(TEXT("Exactly four grounded databases are configured"), GroundDatabasePaths.Num(), 4);
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedGroundDatabases) && Index < GroundDatabasePaths.Num(); ++Index)
			{
				TestEqual(
					*FString::Printf(TEXT("Ground database %d remains project-local"), Index),
					GroundDatabasePaths[Index],
					FString(ExpectedGroundDatabases[Index]));
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
					TEXT("The airborne database is the project-local jump database"),
					AirborneDatabasePaths[0],
					FString(TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Jump.PSD_Rpg_Jump")));
			}
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
		UAnimGraphNode_LocalToComponentSpace* LocalToComponentNode =
			FindUniqueNode<UAnimGraphNode_LocalToComponentSpace>(*this, AnimGraph, TEXT("Local To Component"));
		UEdGraphNode* OrientationWarpingNode = FindUniqueNodeByClassName(
			*this,
			AnimGraph,
			TEXT("AnimGraphNode_OrientationWarping"),
			TEXT("Orientation Warping"));
		UAnimGraphNode_ComponentToLocalSpace* ComponentToLocalNode =
			FindUniqueNode<UAnimGraphNode_ComponentToLocalSpace>(*this, AnimGraph, TEXT("Component To Local"));
		UAnimGraphNode_Slot* SlotNode =
			FindUniqueNode<UAnimGraphNode_Slot>(*this, AnimGraph, TEXT("Montage Slot"));
		UAnimGraphNode_PoseSearchHistoryCollector* PoseHistoryNode =
			FindUniqueNode<UAnimGraphNode_PoseSearchHistoryCollector>(*this, AnimGraph, TEXT("Pose History"));
		UAnimGraphNode_Root* RootNode =
			FindUniqueNode<UAnimGraphNode_Root>(*this, AnimGraph, TEXT("AnimGraph Root"));
		UK2Node_VariableGet* LocomotionAngleGetter =
			FindUniqueVariableGetter(*this, AnimGraph, TEXT("LocomotionAngle"));
		UK2Node_VariableGet* ProceduralAlphaGetter =
			FindUniqueVariableGetter(*this, AnimGraph, TEXT("ProceduralLocomotionAlpha"));
		UK2Node_VariableGet* TrajectoryGetter =
			FindUniqueVariableGetter(*this, AnimGraph, TEXT("LocomotionTrajectory"));
		int32 FootPlacementNodeCount = 0;
		int32 LegIkNodeCount = 0;
		for (const UEdGraphNode* GraphNode : AnimGraph->Nodes)
		{
			if (!GraphNode)
			{
				continue;
			}

			FootPlacementNodeCount += GraphNode->GetClass()->GetFName() == TEXT("AnimGraphNode_FootPlacement");
			LegIkNodeCount += GraphNode->GetClass()->GetFName() == TEXT("AnimGraphNode_LegIK");
		}
		TestEqual(
			TEXT("The pilot does not silently use Epic's worker-thread-unsafe Foot Placement node"),
			FootPlacementNodeCount,
			0);
		TestEqual(
			TEXT("Leg IK remains part of the thread-safe Foot Placement follow-up"),
			LegIkNodeCount,
			0);

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
		}

		if (SlotNode)
		{
			TestEqual(
				TEXT("Combat and harvesting montages keep the authoritative DefaultSlot"),
				SlotNode->Node.SlotName,
				FName(TEXT("DefaultSlot")));
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
			TEXT("Offset Root Bone feeds Local To Component"),
			OffsetRootBoneNode,
			TEXT("Pose"),
			LocalToComponentNode,
			TEXT("LocalPose"));
		TestExclusiveLink(
			*this,
			TEXT("Local To Component feeds Orientation Warping"),
			LocalToComponentNode,
			TEXT("ComponentPose"),
			OrientationWarpingNode,
			TEXT("ComponentPose"));
		TestExclusiveLink(
			*this,
			TEXT("Orientation Warping feeds Component To Local"),
			OrientationWarpingNode,
			TEXT("Pose"),
			ComponentToLocalNode,
			TEXT("ComponentPose"));
		TestExclusiveLink(
			*this,
			TEXT("Component To Local feeds DefaultSlot"),
			ComponentToLocalNode,
			TEXT("Pose"),
			SlotNode,
			TEXT("Source"));
		TestExclusiveLink(
			*this,
			TEXT("DefaultSlot feeds Pose History"),
			SlotNode,
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
			TEXT("The proxy-derived angle drives Orientation Warping"),
			LocomotionAngleGetter,
			TEXT("LocomotionAngle"),
			OrientationWarpingNode,
			TEXT("LocomotionAngle"));
		TestExclusiveLink(
			*this,
			TEXT("The montage-safe procedural alpha gates Orientation Warping"),
			ProceduralAlphaGetter,
			TEXT("ProceduralLocomotionAlpha"),
			OrientationWarpingNode,
			TEXT("Alpha"));
		TestExclusiveLink(
			*this,
			TEXT("The game-thread trajectory snapshot feeds Pose History"),
			TrajectoryGetter,
			TEXT("LocomotionTrajectory"),
			PoseHistoryNode,
			TEXT("TransformTrajectory"));
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

#endif // WITH_DEV_AUTOMATION_TESTS
