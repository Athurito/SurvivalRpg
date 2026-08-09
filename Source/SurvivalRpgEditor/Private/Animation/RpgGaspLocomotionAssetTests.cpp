#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimSequence.h"
#include "Animation/MirrorDataTable.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/DataTable.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "PoseSearch/PoseSearchFeatureChannel_Group.h"
#include "PoseSearch/PoseSearchFeatureChannel_Heading.h"
#include "PoseSearch/PoseSearchFeatureChannel_Position.h"
#include "PoseSearch/PoseSearchFeatureChannel_Trajectory.h"
#include "PoseSearch/PoseSearchFeatureChannel_Velocity.h"
#include "PoseSearch/PoseSearchIndex.h"
#include "PoseSearch/PoseSearchNormalizationSet.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "UObject/UObjectGlobals.h"

namespace RpgGaspLocomotionAssetTests
{
	constexpr TCHAR PluginRoot[] = TEXT("/RpgGaspLocomotion");
	constexpr TCHAR AnimationRoot[] = TEXT("/RpgGaspLocomotion/Animations");
	constexpr TCHAR TargetSkeletonPath[] = TEXT("/Game/SurvivalRpg/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin");
	constexpr TCHAR TargetMeshPath[] = TEXT("/Game/SurvivalRpg/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple");
	constexpr TCHAR TargetSkeletonPackage[] = TEXT("/Game/SurvivalRpg/Characters/Mannequins/Meshes/SK_Mannequin");
	constexpr TCHAR TargetMeshPackage[] = TEXT("/Game/SurvivalRpg/Characters/Mannequins/Meshes/SKM_Manny_Simple");
	constexpr TCHAR NormalizationPackage[] = TEXT("/RpgGaspLocomotion/MotionMatching/NormalizationSets/PSN_Rpg_Locomotion");
	constexpr TCHAR CrouchIdleRoot[] = TEXT("/RpgGaspLocomotion/Animations/Crouch/Idle/");
	constexpr TCHAR CrouchTransitionRoot[] = TEXT("/RpgGaspLocomotion/Animations/Crouch/Transitions/");
	constexpr TCHAR CrouchWalkRoot[] = TEXT("/RpgGaspLocomotion/Animations/Crouch/Walk/");
	constexpr TCHAR StandIdleRoot[] = TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/");
	constexpr TCHAR StandRunRoot[] = TEXT("/RpgGaspLocomotion/Animations/Stand/Run/");
	constexpr TCHAR JumpStartRoot[] = TEXT("/RpgGaspLocomotion/Animations/Jump/Starts/");
	constexpr TCHAR JumpAirborneRoot[] = TEXT("/RpgGaspLocomotion/Animations/Jump/Airborne/");
	constexpr TCHAR JumpLandRoot[] = TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/");
	constexpr TCHAR StandIdlePackage[] = TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/M_Neutral_Stand_Idle_Loop");
	constexpr TCHAR StandToCrouchPackage[] = TEXT("/RpgGaspLocomotion/Animations/Crouch/Transitions/M_Neutral_Transition_Stand_to_Crouch");
	constexpr TCHAR CrouchToStandPackage[] = TEXT("/RpgGaspLocomotion/Animations/Crouch/Transitions/M_Neutral_Transition_Crouch_to_Stand");
	constexpr TCHAR TurnInPlaceDatabasePackage[] = TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_TurnInPlace");
	constexpr TCHAR JumpDatabasePackage[] = TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Jump");
	constexpr TCHAR LandingDatabasePackage[] = TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Idle_Lands_Light");
	constexpr TCHAR JumpSchemaPackage[] = TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Jump");
	constexpr TCHAR MirrorTablePath[] = TEXT("/RpgGaspLocomotion/MotionMatching/MirrorTables/MDT_Rpg_Mannequin.MDT_Rpg_Mannequin");

	static const TCHAR* const TurnInPlaceAnimationPackages[] = {
		TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/M_Neutral_Stand_Turn_045_L"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/M_Neutral_Stand_Turn_045_R"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/M_Neutral_Stand_Turn_090_L"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/M_Neutral_Stand_Turn_090_R"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/M_Neutral_Stand_Turn_135_L"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/M_Neutral_Stand_Turn_135_R"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/M_Neutral_Stand_Turn_180_L"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/M_Neutral_Stand_Turn_180_R"),
	};

	static const TCHAR* const RunAnimationPackages[] = {
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Arc_Small_L"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Arc_Small_R"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Loop_B"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Loop_BL"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Loop_BR"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Loop_F"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Loop_FL"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Loop_FR"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Loop_LL"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Loop_LR"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Loop_RL"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_B_F_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_B_F_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_BL_FR_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_BL_FR_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_BR_FL_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_BR_FL_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_F_B_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_F_B_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_FL_BR_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_FL_BR_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_FR_BL_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_FR_BL_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_LL_RL_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_LL_RL_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_LR_RR_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_LR_RR_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_RL_LL_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_RL_LL_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_RR_LR_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_RR_LR_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Start_B_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Start_B_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Start_F_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Start_F_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Start_LL_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Start_RR_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Stop_B_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Stop_B_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Stop_F_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Stop_F_Rfoot"),
	};

	static const TCHAR* const RunPivotAnimationPackages[] = {
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_B_F_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_B_F_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_BL_FR_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_BL_FR_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_BR_FL_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_BR_FL_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_F_B_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_F_B_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_FL_BR_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_FL_BR_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_FR_BL_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_FR_BL_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_LL_RL_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_LL_RL_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_LR_RR_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_LR_RR_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_RL_LL_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_RL_LL_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_RR_LR_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_RR_LR_Rfoot"),
	};

	static const TCHAR* const SparseRunLoopPackages[] = {
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Loop_B"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Loop_BL"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Loop_BR"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Loop_F"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Loop_FL"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Loop_FR"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Loop_LL"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Loop_RL"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Shuffle_LR_to_LL_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Shuffle_RR_to_RL_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Transition_Walk_to_Run_Lfoot"),
	};

	static const TCHAR* const SparseRunPivotPackages[] = {
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Box_B_LL_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Box_B_RL_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Box_F_LL_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Box_F_RL_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Box_LL_B_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Box_LL_F_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Box_RL_B_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Box_RL_F_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_B_F_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_B_F_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_BL_FR_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_BL_FR_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_BR_FL_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_BR_FL_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_F_B_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_F_B_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_FL_BR_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_FL_BR_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_FR_BL_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_FR_BL_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_LL_RL_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_LR_RR_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_LR_RR_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_RL_LL_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_RL_LL_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_RR_LR_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Turn_L_090_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Turn_L_090_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Turn_L_180_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Turn_L_180_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Turn_R_090_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Turn_R_090_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Turn_R_180_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Turn_R_180_Rfoot"),
	};

	static const TCHAR* const SparseRunStartPackages[] = {
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Reface_Start_B_L_090"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Reface_Start_B_L_180"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Reface_Start_B_R_090"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Reface_Start_B_R_180"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Reface_Start_F_L_090"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Reface_Start_F_L_180"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Reface_Start_F_R_090"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Reface_Start_F_R_180"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Start_B_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Start_B_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Start_F_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Start_F_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Start_LL_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Start_RL_Rfoot"),
	};

	static const TCHAR* const SparseRunStopPackages[] = {
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Stop_B_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Stop_B_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Stop_F_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Stop_F_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Stop_LL_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Stop_LL_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Stop_LR_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Stop_LR_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Stop_RL_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Stop_RL_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Stop_RR_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Stop_RR_Rfoot"),
	};

	static const TCHAR* const AirborneJumpAnimationPackages[] = {
		TEXT("/RpgGaspLocomotion/Animations/Jump/Starts/M_Neutral_Jump_B_Start_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Starts/M_Neutral_Jump_B_Start_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Starts/M_Neutral_Jump_F_Start_Run_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Starts/M_Neutral_Jump_F_Start_Run_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Starts/M_Neutral_Jump_F_Start_Stand_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Starts/M_Neutral_Jump_F_Start_Stand_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Starts/M_Neutral_Jump_F_Start_Walk_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Starts/M_Neutral_Jump_F_Start_Walk_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Starts/M_Neutral_Jump_LL_Start_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Starts/M_Neutral_Jump_LL_Start_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Starts/M_Neutral_Jump_RL_Start_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Starts/M_Neutral_Jump_RL_Start_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Starts/M_Neutral_Jump_F_Off_Run_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Starts/M_Neutral_Jump_F_Off_Run_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Starts/M_Neutral_Jump_F_Off_Walk_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Starts/M_Neutral_Jump_F_Off_Walk_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Starts/M_Neutral_Jump_F_Start_Sprint_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Starts/M_Neutral_Jump_F_Start_Sprint_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Airborne/M_Neutral_Jump_Loop_Fall"),
	};

	static const TCHAR* const LandingAnimationPackages[] = {
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_B_Land_Stand_Light_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_F_Land_Stand_Light_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_LL_Land_Stand_Light_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_RL_Land_Stand_Light_Rfoot"),
	};

	bool IsForbiddenDependency(const FString& PackageName)
	{
		const FString LowerPackageName = PackageName.ToLower();
		static const TCHAR* const ForbiddenMarkers[] = {
			TEXT("/game/blueprints/"),
			TEXT("/game/audio/"),
			TEXT("/game/characters/uefn_mannequin/"),
			TEXT("/game/characters/ue5_mannequins/"),
			TEXT("experimentalstatemachine"),
			TEXT("psd_sm_"),
			TEXT("/traversal/"),
			TEXT("/locomotor/"),
			TEXT("/mover/"),
			TEXT("networkprediction"),
			TEXT("metasound"),
		};

		for (const TCHAR* Marker : ForbiddenMarkers)
		{
			if (LowerPackageName.Contains(Marker))
			{
				return true;
			}
		}

		return PackageName.StartsWith(TEXT("/Game/")) &&
			!PackageName.StartsWith(TargetSkeletonPackage) &&
			!PackageName.StartsWith(TargetMeshPackage);
	}

	struct FDatabaseContract
	{
		const TCHAR* PackageName;
		const TCHAR* AnimationPrefix;
		const TCHAR* SchemaPackage;
		int32 ExpectedAnimationDependencyCount;
		int32 ExpectedDatabaseEntryCount;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgGaspLocomotionContentContractTest,
	"SurvivalRpg.Animation.Gasp.ContentContract",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgGaspLocomotionContentContractTest::RunTest(const FString& Parameters)
{
	using namespace RpgGaspLocomotionAssetTests;

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("RpgGaspLocomotion"));
	if (!TestTrue(TEXT("RpgGaspLocomotion is registered"), Plugin.IsValid()))
	{
		return false;
	}
	TestTrue(TEXT("RpgGaspLocomotion is enabled"), Plugin->IsEnabled());
	TestTrue(TEXT("RpgGaspLocomotion can contain content"), Plugin->CanContainContent());
	TestEqual(TEXT("RpgGaspLocomotion has no code modules"), Plugin->GetDescriptor().Modules.Num(), 0);

	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.ScanPathsSynchronous({ FString(PluginRoot) }, true, false);
	AssetRegistry.WaitForCompletion();

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssetsByPath(
		FName(PluginRoot),
		Assets,
		true,
		true);
	TestEqual(TEXT("The curated plugin contains exactly 176 assets"), Assets.Num(), 176);

	TMap<FString, int32> ClassCounts;
	int32 AnimationCount = 0;
	int32 CrouchIdleCount = 0;
	int32 CrouchTransitionCount = 0;
	int32 CrouchWalkCount = 0;
	int32 StandIdleCount = 0;
	int32 StandRunCount = 0;
	int32 TurnInPlaceSequenceCount = 0;
	int32 JumpStartCount = 0;
	int32 JumpAirborneCount = 0;
	int32 JumpLandCount = 0;
	for (const FAssetData& AssetData : Assets)
	{
		const FString ClassName = AssetData.AssetClassPath.GetAssetName().ToString();
		++ClassCounts.FindOrAdd(ClassName);
		TestNotEqual(
			*FString::Printf(TEXT("%s is not a redirector"), *AssetData.PackageName.ToString()),
			ClassName,
			FString(TEXT("ObjectRedirector")));

		TArray<FName> Dependencies;
		AssetRegistry.GetDependencies(
			AssetData.PackageName,
			Dependencies,
			UE::AssetRegistry::EDependencyCategory::Package);
		for (const FName Dependency : Dependencies)
		{
			const FString DependencyString = Dependency.ToString();
			TestFalse(
				*FString::Printf(
					TEXT("%s has no forbidden dependency on %s"),
					*AssetData.PackageName.ToString(),
					*DependencyString),
				IsForbiddenDependency(DependencyString));
		}

		if (ClassName != TEXT("AnimSequence"))
		{
			continue;
		}

		++AnimationCount;
		const FString AnimationPackageName = AssetData.PackageName.ToString();
		CrouchIdleCount += AnimationPackageName.StartsWith(CrouchIdleRoot);
		CrouchTransitionCount += AnimationPackageName.StartsWith(CrouchTransitionRoot);
		CrouchWalkCount += AnimationPackageName.StartsWith(CrouchWalkRoot);
		StandIdleCount += AnimationPackageName.StartsWith(StandIdleRoot);
		StandRunCount += AnimationPackageName.StartsWith(StandRunRoot);
		JumpStartCount += AnimationPackageName.StartsWith(JumpStartRoot);
		JumpAirborneCount += AnimationPackageName.StartsWith(JumpAirborneRoot);
		JumpLandCount += AnimationPackageName.StartsWith(JumpLandRoot);
		TurnInPlaceSequenceCount += AnimationPackageName.StartsWith(StandIdleRoot) &&
			AnimationPackageName.Contains(TEXT("_Stand_Turn_"));
		UAnimSequence* Animation = Cast<UAnimSequence>(AssetData.GetAsset());
		if (!TestNotNull(
			*FString::Printf(TEXT("%s loads as an AnimSequence"), *AssetData.PackageName.ToString()),
			Animation))
		{
			continue;
		}

		TestEqual(
			*FString::Printf(TEXT("%s uses the authoritative skeleton"), *Animation->GetName()),
			GetPathNameSafe(Animation->GetSkeleton()),
			FString(TargetSkeletonPath));
		TestEqual(
			*FString::Printf(TEXT("%s uses the project-local preview mesh"), *Animation->GetName()),
			GetPathNameSafe(Animation->GetPreviewMesh()),
			FString(TargetMeshPath));
		if (Animation->GetSkeleton())
		{
			TestEqual(
				*FString::Printf(TEXT("%s stores the authoritative skeleton GUID"), *Animation->GetName()),
				Animation->GetSkeletonGuid(),
				Animation->GetSkeleton()->GetGuid());
		}
		TestTrue(*FString::Printf(TEXT("%s keeps root motion enabled"), *Animation->GetName()), Animation->bEnableRootMotion);
		TestEqual(
			*FString::Printf(TEXT("%s keeps RefPose root locking"), *Animation->GetName()),
			Animation->RootMotionRootLock.GetValue(),
			ERootMotionRootLock::RefPose);
		TestTrue(*FString::Printf(TEXT("%s keeps force-root-lock"), *Animation->GetName()), Animation->bForceRootLock);
		TestTrue(
			*FString::Printf(TEXT("%s keeps normalized root-motion scale"), *Animation->GetName()),
			Animation->bUseNormalizedRootMotionScale);
		TestTrue(
			*FString::Printf(TEXT("%s supplies the left Foot Placement contact curve"), *Animation->GetName()),
			Animation->HasCurveData(TEXT("contact_l"), false));
		TestTrue(
			*FString::Printf(TEXT("%s supplies the right Foot Placement contact curve"), *Animation->GetName()),
			Animation->HasCurveData(TEXT("contact_r"), false));

		const TArray<UAssetUserData*>* AssetUserData = Animation->GetAssetUserDataArray();
		TestTrue(
			*FString::Printf(TEXT("%s has no migrated sample asset user data"), *Animation->GetName()),
			!AssetUserData || AssetUserData->IsEmpty());

		for (const FAnimNotifyEvent& NotifyEvent : Animation->Notifies)
		{
			const UObject* NotifyObject = NotifyEvent.Notify
				? static_cast<const UObject*>(NotifyEvent.Notify.Get())
				: static_cast<const UObject*>(NotifyEvent.NotifyStateClass.Get());
			if (!NotifyObject)
			{
				continue;
			}

			const FString NotifyClassPath = NotifyObject->GetClass()->GetPathName();
			TestFalse(
				*FString::Printf(TEXT("%s has no sample Blueprint notify %s"), *Animation->GetName(), *NotifyClassPath),
				NotifyClassPath.StartsWith(TEXT("/Game/")) ||
					NotifyClassPath.Contains(TEXT("Foley")) ||
					NotifyClassPath.Contains(TEXT("EarlyTransition")) ||
					NotifyClassPath.Contains(TEXT("BranchIn")));
		}
	}

	TestEqual(TEXT("Exactly 159 curated AnimSequences are present"), AnimationCount, 159);
	TestEqual(TEXT("Exactly one crouch idle sequence is present"), CrouchIdleCount, 1);
	TestEqual(TEXT("Exactly two crouch transition sequences are present"), CrouchTransitionCount, 2);
	TestEqual(TEXT("Exactly eight crouch walk sequences are present"), CrouchWalkCount, 8);
	TestEqual(TEXT("Exactly nine stand-idle-folder sequences are present"), StandIdleCount, 9);
	TestEqual(TEXT("Exactly eight turn-in-place sequences are present"), TurnInPlaceSequenceCount, 8);
	TestEqual(TEXT("Exactly 77 stand-run sequences are present"), StandRunCount, 77);
	TestEqual(TEXT("Exactly 18 jump start/off sequences are present"), JumpStartCount, 18);
	TestEqual(TEXT("Exactly one neutral airborne fall loop is present"), JumpAirborneCount, 1);
	TestEqual(TEXT("Exactly four bounded stand-light landing sequences are present"), JumpLandCount, 4);
	TestEqual(TEXT("Exactly one ChooserTable is present"), ClassCounts.FindRef(TEXT("ChooserTable")), 1);
	TestEqual(TEXT("Exactly one MirrorDataTable is present"), ClassCounts.FindRef(TEXT("MirrorDataTable")), 1);
	TestEqual(TEXT("Exactly twelve PoseSearchDatabases are present"), ClassCounts.FindRef(TEXT("PoseSearchDatabase")), 12);
	TestEqual(TEXT("Exactly one PoseSearchNormalizationSet is present"), ClassCounts.FindRef(TEXT("PoseSearchNormalizationSet")), 1);
	TestEqual(TEXT("Exactly two PoseSearchSchemas are present"), ClassCounts.FindRef(TEXT("PoseSearchSchema")), 2);

	static const FDatabaseContract DatabaseContracts[] = {
		{ TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Idle"), TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/"), TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Locomotion"), 1, 2 },
		{ TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_TurnInPlace"), TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/"), TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Locomotion"), 8, 8 },
		{ TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Crouch"), TEXT("/RpgGaspLocomotion/Animations/Crouch/"), TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Locomotion"), 10, 10 },
		{ TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Walk"), TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/"), TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Locomotion"), 29, 29 },
		{ TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Loops"), TEXT("/RpgGaspLocomotion/Animations/Stand/Run/"), TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Locomotion"), 11, 11 },
		{ TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Pivots"), TEXT("/RpgGaspLocomotion/Animations/Stand/Run/"), TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Locomotion"), 34, 34 },
		{ TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Starts"), TEXT("/RpgGaspLocomotion/Animations/Stand/Run/"), TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Locomotion"), 14, 14 },
		{ TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Stops"), TEXT("/RpgGaspLocomotion/Animations/Stand/Run/"), TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Locomotion"), 12, 12 },
		{ TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Sprint"), TEXT("/RpgGaspLocomotion/Animations/Stand/Sprint/"), TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Locomotion"), 10, 10 },
		{ JumpDatabasePackage, TEXT("/RpgGaspLocomotion/Animations/Jump/"), JumpSchemaPackage, 19, 19 },
		{ LandingDatabasePackage, JumpLandRoot, JumpSchemaPackage, 4, 4 },
	};

	for (const FDatabaseContract& Contract : DatabaseContracts)
	{
		TArray<FName> Dependencies;
		AssetRegistry.GetDependencies(
			FName(Contract.PackageName),
			Dependencies,
			UE::AssetRegistry::EDependencyCategory::Package);
		int32 AnimationDependencyCount = 0;
		for (const FName Dependency : Dependencies)
		{
			if (Dependency.ToString().StartsWith(Contract.AnimationPrefix))
			{
				++AnimationDependencyCount;
			}
		}
		TestEqual(
			*FString::Printf(TEXT("%s owns the expected animation group"), Contract.PackageName),
			AnimationDependencyCount,
			Contract.ExpectedAnimationDependencyCount);
		TestTrue(
			*FString::Printf(TEXT("%s references its local schema"), Contract.PackageName),
			Dependencies.Contains(FName(Contract.SchemaPackage)));
		TestTrue(
			*FString::Printf(TEXT("%s references the shared normalization set"), Contract.PackageName),
			Dependencies.Contains(FName(NormalizationPackage)));

		const FString DatabaseObjectPath = FString::Printf(
			TEXT("%s.%s"),
			Contract.PackageName,
			*FPackageName::GetLongPackageAssetName(Contract.PackageName));
		UPoseSearchDatabase* Database = LoadObject<UPoseSearchDatabase>(nullptr, *DatabaseObjectPath);
		if (TestNotNull(
			*FString::Printf(TEXT("%s loads as a Pose Search database"), Contract.PackageName),
			Database))
		{
			TestEqual(
				*FString::Printf(TEXT("%s contains the exact entry count"), Contract.PackageName),
				Database->GetNumAnimationAssets(),
				Contract.ExpectedDatabaseEntryCount);
		}
	}

	UPoseSearchDatabase* StandIdleDatabase = LoadObject<UPoseSearchDatabase>(
		nullptr,
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Idle.PSD_Rpg_Stand_Idle"));
	if (TestNotNull(TEXT("The stand-idle database loads for transition validation"), StandIdleDatabase))
	{
		bool bContainsStandIdle = false;
		bool bContainsCrouchToStand = false;
		bool bContainsTurnInPlace = false;
		for (int32 Index = 0; Index < StandIdleDatabase->GetNumAnimationAssets(); ++Index)
		{
			const UObject* AnimationAsset = StandIdleDatabase->GetAnimationAsset(Index);
			bContainsStandIdle |= AnimationAsset &&
				AnimationAsset->GetOutermost()->GetName() == StandIdlePackage;
			bContainsCrouchToStand |= AnimationAsset &&
				AnimationAsset->GetOutermost()->GetName() == CrouchToStandPackage;
			bContainsTurnInPlace |= AnimationAsset &&
				AnimationAsset->GetOutermost()->GetName().Contains(TEXT("_Stand_Turn_"));
		}
		TestTrue(TEXT("The stand-idle database contains the neutral idle"), bContainsStandIdle);
		TestTrue(TEXT("The stand-idle database contains Crouch-to-Stand"), bContainsCrouchToStand);
		TestFalse(TEXT("The stand-idle database contains no turn-in-place clips"), bContainsTurnInPlace);
	}

	UPoseSearchDatabase* TurnInPlaceDatabase = LoadObject<UPoseSearchDatabase>(
		nullptr,
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_TurnInPlace.PSD_Rpg_Stand_TurnInPlace"));
	if (TestNotNull(TEXT("The turn-in-place database loads for exclusivity validation"), TurnInPlaceDatabase))
	{
		TestEqual(
			TEXT("The turn-in-place database exposes exactly one selection tag"),
			TurnInPlaceDatabase->Tags.Num(),
			1);
		if (TurnInPlaceDatabase->Tags.Num() == 1)
		{
			TestEqual(
				TEXT("The turn-in-place database uses the GASP TurnInPlace selection tag"),
				TurnInPlaceDatabase->Tags[0],
				FName(TEXT("TurnInPlace")));
		}
		TestTrue(
			TEXT("The turn-in-place database keeps the GASP base-cost bias"),
			FMath::IsNearlyEqual(TurnInPlaceDatabase->BaseCostBias, -0.2f));
		TestTrue(
			TEXT("The turn-in-place database keeps the GASP continuing-pose bias"),
			FMath::IsNearlyEqual(TurnInPlaceDatabase->ContinuingPoseCostBias, -0.05f));

		TSet<FString> ActualTurnPackages;
		for (int32 Index = 0; Index < TurnInPlaceDatabase->GetNumAnimationAssets(); ++Index)
		{
			const FPoseSearchDatabaseAnimationAsset* DatabaseEntry =
				TurnInPlaceDatabase->GetDatabaseAnimationAsset(Index);
			if (!TestNotNull(
				*FString::Printf(TEXT("Turn-in-place database entry %d resolves"), Index),
				DatabaseEntry))
			{
				continue;
			}

			TestEqual(
				*FString::Printf(TEXT("Turn-in-place database entry %d is unmirrored-only"), Index),
				DatabaseEntry->GetMirrorOption(),
				EPoseSearchMirrorOption::UnmirroredOnly);
			const FFloatInterval SamplingRange = DatabaseEntry->GetSamplingRange();
			TestTrue(
				*FString::Printf(TEXT("Turn-in-place database entry %d samples only the authored start pose"), Index),
				FMath::IsNearlyZero(SamplingRange.Min) && FMath::IsNearlyEqual(SamplingRange.Max, 0.01f));
			TestFalse(
				*FString::Printf(TEXT("Turn-in-place database entry %d is non-looping"), Index),
				DatabaseEntry->IsLooping());

			const UObject* AnimationAsset = DatabaseEntry->GetAnimationAsset();
			if (TestNotNull(TEXT("Every turn-in-place entry resolves"), AnimationAsset))
			{
				ActualTurnPackages.Add(AnimationAsset->GetOutermost()->GetName());
				if (const UAnimSequence* TurnSequence = Cast<UAnimSequence>(AnimationAsset))
				{
					TestFalse(
						*FString::Printf(TEXT("%s remains a non-looping authored turn"), *TurnSequence->GetName()),
						TurnSequence->bLoop);
				}
			}
		}

		TestEqual(TEXT("The turn-in-place database has no duplicate entries"), ActualTurnPackages.Num(), 8);
		for (const TCHAR* ExpectedPackage : TurnInPlaceAnimationPackages)
		{
			TestTrue(
				*FString::Printf(TEXT("The turn-in-place database contains %s"), ExpectedPackage),
				ActualTurnPackages.Contains(FString(ExpectedPackage)));
		}

		const UE::PoseSearch::EAsyncBuildIndexResult BuildResult =
			UE::PoseSearch::FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(
				TurnInPlaceDatabase,
				UE::PoseSearch::ERequestAsyncBuildFlag::NewRequest |
					UE::PoseSearch::ERequestAsyncBuildFlag::WaitForCompletion);
		if (TestTrue(
			TEXT("The turn-in-place search index finishes building"),
			BuildResult == UE::PoseSearch::EAsyncBuildIndexResult::Success))
		{
			const UE::PoseSearch::FSearchIndex& SearchIndex = TurnInPlaceDatabase->GetSearchIndex();
			TestEqual(TEXT("The turn-in-place database indexes exactly eight poses"), SearchIndex.GetNumPoses(), 8);
			TestEqual(TEXT("The turn-in-place database creates exactly eight search-index assets"), SearchIndex.Assets.Num(), 8);
			for (int32 Index = 0; Index < SearchIndex.Assets.Num(); ++Index)
			{
				TestEqual(
					*FString::Printf(TEXT("Turn-in-place search-index asset %d contains only t=0"), Index),
					SearchIndex.Assets[Index].GetNumPoses(),
					1);
			}
		}
	}

	UPoseSearchDatabase* StandRunDatabase = LoadObject<UPoseSearchDatabase>(
		nullptr,
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run.PSD_Rpg_Stand_Run"));
	if (TestNotNull(TEXT("The stand-run database loads for directional-core validation"), StandRunDatabase))
	{
		TSet<FString> RunPackages;
		int32 RunPivotCount = 0;
		for (int32 Index = 0; Index < StandRunDatabase->GetNumAnimationAssets(); ++Index)
		{
			const FPoseSearchDatabaseAnimationAsset* DatabaseEntry =
				StandRunDatabase->GetDatabaseAnimationAsset(Index);
			if (!TestNotNull(
				*FString::Printf(TEXT("Stand-run database entry %d resolves"), Index),
				DatabaseEntry))
			{
				continue;
			}

			if (const UObject* AnimationAsset = DatabaseEntry->GetAnimationAsset())
			{
				const FString PackageName = AnimationAsset->GetOutermost()->GetName();
				RunPackages.Add(PackageName);
				if (PackageName.Contains(TEXT("_Run_Pivot_")))
				{
					++RunPivotCount;
					TestTrue(
						*FString::Printf(TEXT("%s remains enabled"), *PackageName),
						DatabaseEntry->IsEnabled());
					TestEqual(
						*FString::Printf(TEXT("%s remains unmirrored-only"), *PackageName),
						DatabaseEntry->GetMirrorOption(),
						EPoseSearchMirrorOption::UnmirroredOnly);
					TestFalse(
						*FString::Printf(TEXT("%s remains reselectable"), *PackageName),
						DatabaseEntry->IsDisableReselection());
					const FFloatInterval SamplingRange = DatabaseEntry->GetSamplingRange();
					TestTrue(
						*FString::Printf(TEXT("%s keeps full-range sampling"), *PackageName),
						FMath::IsNearlyZero(SamplingRange.Min) && FMath::IsNearlyZero(SamplingRange.Max));
					TestFalse(
						*FString::Printf(TEXT("%s has no external BranchIn synchronization"), *PackageName),
						DatabaseEntry->IsSynchronizedWithExternalDependency());
					TestFalse(
						*FString::Printf(TEXT("%s remains a non-looping pivot"), *PackageName),
						DatabaseEntry->IsLooping());
				}
			}
		}
		TestEqual(TEXT("The stand-run database has no duplicate entries"), RunPackages.Num(), 41);
		TestEqual(TEXT("The stand-run database contains the complete neutral pivot set"), RunPivotCount, 20);
		for (const TCHAR* ExpectedPackage : RunAnimationPackages)
		{
			TestTrue(
				*FString::Printf(TEXT("The stand-run database contains %s"), ExpectedPackage),
				RunPackages.Contains(FString(ExpectedPackage)));
		}
		for (const TCHAR* ExpectedPackage : RunPivotAnimationPackages)
		{
			TestTrue(
				*FString::Printf(TEXT("The stand-run database contains %s"), ExpectedPackage),
				RunPackages.Contains(FString(ExpectedPackage)));
		}

		const UE::PoseSearch::EAsyncBuildIndexResult RunBuildResult =
			UE::PoseSearch::FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(
				StandRunDatabase,
				UE::PoseSearch::ERequestAsyncBuildFlag::NewRequest |
					UE::PoseSearch::ERequestAsyncBuildFlag::WaitForCompletion);
		if (TestTrue(
			TEXT("The expanded stand-run search index finishes building"),
			RunBuildResult == UE::PoseSearch::EAsyncBuildIndexResult::Success))
		{
			const UE::PoseSearch::FSearchIndex& SearchIndex = StandRunDatabase->GetSearchIndex();
			TestEqual(TEXT("The stand-run index contains all 41 animation entries"), SearchIndex.Assets.Num(), 41);
			TestTrue(TEXT("The expanded stand-run database produces searchable poses"), SearchIndex.GetNumPoses() > 0);
		}
	}

	const auto ValidateSparseRunDatabase = [this](
		const TCHAR* ObjectPath,
		TConstArrayView<const TCHAR*> ExpectedPackages)
	{
		UPoseSearchDatabase* Database = LoadObject<UPoseSearchDatabase>(nullptr, ObjectPath);
		if (!TestNotNull(
				*FString::Printf(TEXT("The source-exact Sparse run database %s loads"), ObjectPath),
				Database))
		{
			return;
		}

		TSet<FString> ExpectedPackageSet;
		for (const TCHAR* ExpectedPackage : ExpectedPackages)
		{
			ExpectedPackageSet.Add(ExpectedPackage);
		}
		TestEqual(
			*FString::Printf(TEXT("%s has no duplicate expected packages"), ObjectPath),
			ExpectedPackageSet.Num(),
			ExpectedPackages.Num());

		TSet<FString> ActualPackages;
		for (int32 Index = 0; Index < Database->GetNumAnimationAssets(); ++Index)
		{
			const FPoseSearchDatabaseAnimationAsset* Entry =
				Database->GetDatabaseAnimationAsset(Index);
			if (!TestNotNull(
					*FString::Printf(TEXT("%s entry %d resolves"), ObjectPath, Index),
					Entry))
			{
				continue;
			}

			TestFalse(
				*FString::Printf(TEXT("%s entry %d has no stale BranchIn dependency"), ObjectPath, Index),
				Entry->IsSynchronizedWithExternalDependency());
			const UObject* AnimationAsset = Entry->GetAnimationAsset();
			if (TestNotNull(
					*FString::Printf(TEXT("%s entry %d resolves its target animation"), ObjectPath, Index),
					AnimationAsset))
			{
				const FString PackageName = AnimationAsset->GetOutermost()->GetName();
				TestTrue(
					*FString::Printf(TEXT("%s is an exact physical Sparse member"), *PackageName),
					ExpectedPackageSet.Contains(PackageName));
				ActualPackages.Add(PackageName);
			}
		}

		TestEqual(
			*FString::Printf(TEXT("%s has the exact source membership without duplicates"), ObjectPath),
			ActualPackages.Num(),
			ExpectedPackageSet.Num());
		for (const FString& ExpectedPackage : ExpectedPackageSet)
		{
			TestTrue(
				*FString::Printf(TEXT("%s contains %s"), ObjectPath, *ExpectedPackage),
				ActualPackages.Contains(ExpectedPackage));
		}

		const UE::PoseSearch::EAsyncBuildIndexResult BuildResult =
			UE::PoseSearch::FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(
				Database,
				UE::PoseSearch::ERequestAsyncBuildFlag::NewRequest |
					UE::PoseSearch::ERequestAsyncBuildFlag::WaitForCompletion);
		if (TestTrue(
				*FString::Printf(TEXT("%s builds a searchable index"), ObjectPath),
				BuildResult == UE::PoseSearch::EAsyncBuildIndexResult::Success))
		{
			TestEqual(
				*FString::Printf(TEXT("%s indexes every authored entry"), ObjectPath),
				Database->GetSearchIndex().Assets.Num(),
				Database->GetNumAnimationAssets());
			TestTrue(
				*FString::Printf(TEXT("%s contains searchable poses"), ObjectPath),
				Database->GetSearchIndex().GetNumPoses() > 0);
		}
	};

	ValidateSparseRunDatabase(
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Loops.PSD_Rpg_Stand_Run_Loops"),
		MakeArrayView(SparseRunLoopPackages));
	ValidateSparseRunDatabase(
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Pivots.PSD_Rpg_Stand_Run_Pivots"),
		MakeArrayView(SparseRunPivotPackages));
	ValidateSparseRunDatabase(
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Starts.PSD_Rpg_Stand_Run_Starts"),
		MakeArrayView(SparseRunStartPackages));
	ValidateSparseRunDatabase(
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Stops.PSD_Rpg_Stand_Run_Stops"),
		MakeArrayView(SparseRunStopPackages));

	auto ValidateJumpDatabase = [this](
		const TCHAR* ObjectPath,
		TConstArrayView<const TCHAR*> ExpectedPackages,
		const bool bLandingDatabase)
	{
		UPoseSearchDatabase* Database = LoadObject<UPoseSearchDatabase>(nullptr, ObjectPath);
		if (!TestNotNull(
			bLandingDatabase
				? TEXT("The dedicated light-landing database loads")
				: TEXT("The exclusive airborne-jump database loads"),
			Database))
		{
			return;
		}

		TestTrue(
			bLandingDatabase
				? TEXT("The light-landing database keeps GASP's continuing-pose bias")
				: TEXT("The airborne-jump database keeps GASP's continuing-pose bias"),
			FMath::IsNearlyEqual(
				Database->ContinuingPoseCostBias,
				bLandingDatabase ? -0.15f : -0.5f));

		TSet<FString> ExpectedPackageSet;
		for (const TCHAR* ExpectedPackage : ExpectedPackages)
		{
			ExpectedPackageSet.Add(ExpectedPackage);
		}

		TSet<FString> ActualPackages;
		int32 StartCount = 0;
		int32 FallCount = 0;
		int32 LandCount = 0;
		int32 LoopingStartCount = 0;
		int32 LoopingFallCount = 0;
		int32 LoopingLandCount = 0;
		for (int32 Index = 0; Index < Database->GetNumAnimationAssets(); ++Index)
		{
			const FPoseSearchDatabaseAnimationAsset* Entry = Database->GetDatabaseAnimationAsset(Index);
			if (!TestNotNull(
				*FString::Printf(TEXT("Jump database entry %d resolves"), Index),
				Entry))
			{
				continue;
			}

			TestEqual(
				*FString::Printf(TEXT("Jump database entry %d is unmirrored-only"), Index),
				Entry->GetMirrorOption(),
				EPoseSearchMirrorOption::UnmirroredOnly);
			TestEqual(
				*FString::Printf(
					TEXT("Jump database entry %d keeps the GASP reselection contract"),
					Index),
				Entry->IsDisableReselection(),
				bLandingDatabase);
			const FFloatInterval SamplingRange = Entry->GetSamplingRange();
			TestTrue(
				*FString::Printf(TEXT("Jump database entry %d deliberately keeps source-style full-range sampling"), Index),
				FMath::IsNearlyZero(SamplingRange.Min) && FMath::IsNearlyZero(SamplingRange.Max));

			const UObject* AnimationAsset = Entry->GetAnimationAsset();
			if (!TestNotNull(TEXT("Every jump database entry resolves its animation"), AnimationAsset))
			{
				continue;
			}

			const FString PackageName = AnimationAsset->GetOutermost()->GetName();
			ActualPackages.Add(PackageName);
			const bool bIsStart = PackageName.StartsWith(JumpStartRoot);
			const bool bIsFall = PackageName.StartsWith(JumpAirborneRoot);
			const bool bIsLand = PackageName.StartsWith(JumpLandRoot);
			const bool bIsLooping = Entry->IsLooping();
			StartCount += bIsStart;
			FallCount += bIsFall;
			LandCount += bIsLand;
			LoopingStartCount += bIsStart && bIsLooping;
			LoopingFallCount += bIsFall && bIsLooping;
			LoopingLandCount += bIsLand && bIsLooping;
			TestTrue(
				*FString::Printf(TEXT("%s is an expected exclusive database member"), *PackageName),
				ExpectedPackageSet.Contains(PackageName));
			if (bIsStart)
			{
				TestFalse(
					*FString::Printf(TEXT("%s remains a non-looping jump start/off clip"), *PackageName),
					bIsLooping);
			}
			else if (bIsFall)
			{
				TestTrue(
					*FString::Printf(TEXT("%s remains the looping airborne hold"), *PackageName),
					bIsLooping);
			}
			else if (bLandingDatabase)
			{
				TestFalse(
					*FString::Printf(TEXT("%s remains a non-looping landing clip"), *PackageName),
					bIsLooping);
			}
		}

		TestEqual(
			TEXT("The jump-phase database contains no duplicate animations"),
			ActualPackages.Num(),
			ExpectedPackageSet.Num());
		for (const FString& ExpectedPackage : ExpectedPackageSet)
		{
			TestTrue(
				*FString::Printf(TEXT("The jump-phase database contains %s"), *ExpectedPackage),
				ActualPackages.Contains(ExpectedPackage));
		}

		if (bLandingDatabase)
		{
			TestEqual(TEXT("The light-landing database contains no starts"), StartCount, 0);
			TestEqual(TEXT("The light-landing database contains no fall loop"), FallCount, 0);
			TestEqual(TEXT("The light-landing database contains four lands"), LandCount, 4);
			TestEqual(TEXT("No light-landing entry loops"), LoopingLandCount, 0);
		}
		else
		{
			TestEqual(TEXT("The airborne database contains eighteen starts/off clips"), StartCount, 18);
			TestEqual(TEXT("The airborne database contains one fall loop"), FallCount, 1);
			TestEqual(TEXT("The airborne database contains no landing clips"), LandCount, 0);
			TestEqual(TEXT("None of the eighteen jump starts/off clips loop"), LoopingStartCount, 0);
			TestEqual(TEXT("Exactly the one airborne fall entry loops"), LoopingFallCount, 1);
		}

		const UE::PoseSearch::EAsyncBuildIndexResult BuildResult =
			UE::PoseSearch::FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(
				Database,
				UE::PoseSearch::ERequestAsyncBuildFlag::NewRequest |
					UE::PoseSearch::ERequestAsyncBuildFlag::WaitForCompletion);
		if (TestTrue(
			bLandingDatabase
				? TEXT("The light-landing database search index builds")
				: TEXT("The airborne-jump database search index builds"),
			BuildResult == UE::PoseSearch::EAsyncBuildIndexResult::Success))
		{
			TestEqual(
				TEXT("The jump-phase search index retains one asset record per authored entry"),
				Database->GetSearchIndex().Assets.Num(),
				Database->GetNumAnimationAssets());
		}
	};

	ValidateJumpDatabase(
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Jump.PSD_Rpg_Jump"),
		MakeArrayView(AirborneJumpAnimationPackages),
		false);
	ValidateJumpDatabase(
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Idle_Lands_Light.PSD_Rpg_Stand_Idle_Lands_Light"),
		MakeArrayView(LandingAnimationPackages),
		true);

	UPoseSearchSchema* GroundSchema = LoadObject<UPoseSearchSchema>(
		nullptr,
		TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Locomotion.PSS_Rpg_Locomotion"));
	if (TestNotNull(TEXT("The source-close local ground schema loads"), GroundSchema))
	{
		TestEqual(TEXT("The ground schema samples at 30 Hz"), GroundSchema->SampleRate, 30);
		TestEqual(
			TEXT("The ground schema uses GASP common-schema normalization"),
			GroundSchema->DataPreprocessor,
			EPoseSearchDataPreprocessor::NormalizeWithCommonSchema);
		TestEqual(TEXT("The ground schema finalizes to GASP cardinality 30"), GroundSchema->SchemaCardinality, 30);
		TestEqual(TEXT("The ground schema owns one roled skeleton"), GroundSchema->GetRoledSkeletons().Num(), 1);
		if (GroundSchema->GetRoledSkeletons().Num() == 1)
		{
			const FPoseSearchRoledSkeleton& RoledSkeleton = GroundSchema->GetRoledSkeletons()[0];
			TestEqual(TEXT("The ground schema uses the authoritative skeleton"), GetPathNameSafe(RoledSkeleton.Skeleton), FString(TargetSkeletonPath));
			TestEqual(TEXT("The ground schema uses the local mirror table"), GetPathNameSafe(RoledSkeleton.MirrorDataTable.Get()), FString(MirrorTablePath));
			TestTrue(TEXT("PSS_Default requires no PoseHistory curves"), RoledSkeleton.RequiredCurves.IsEmpty());
			TestEqual(TEXT("PSS_Default resolves root, feet, and pelvis"), RoledSkeleton.BoneReferences.Num(), 4);
			if (RoledSkeleton.BoneReferences.Num() == 4)
			{
				TestEqual(TEXT("Ground bone 0 is root"), RoledSkeleton.BoneReferences[0].BoneName, FName(TEXT("root")));
				TestEqual(TEXT("Ground bone 1 is foot_l"), RoledSkeleton.BoneReferences[1].BoneName, FName(TEXT("foot_l")));
				TestEqual(TEXT("Ground bone 2 is foot_r"), RoledSkeleton.BoneReferences[2].BoneName, FName(TEXT("foot_r")));
				TestEqual(TEXT("Ground bone 3 is pelvis"), RoledSkeleton.BoneReferences[3].BoneName, FName(TEXT("pelvis")));
			}
		}

		const TConstArrayView<TObjectPtr<UPoseSearchFeatureChannel>> Channels = GroundSchema->GetChannels();
		TestEqual(TEXT("PSS_Default has exactly Trajectory + Group"), Channels.Num(), 2);
		const UPoseSearchFeatureChannel_Trajectory* Trajectory =
			Channels.IsValidIndex(0) ? Cast<UPoseSearchFeatureChannel_Trajectory>(Channels[0]) : nullptr;
		const UPoseSearchFeatureChannel_Group* Group =
			Channels.IsValidIndex(1) ? Cast<UPoseSearchFeatureChannel_Group>(Channels[1]) : nullptr;
		TestNotNull(TEXT("The first ground channel is Trajectory"), Trajectory);
		if (TestNotNull(TEXT("The second ground channel is Group"), Group))
		{
			bool bHasPosition = false;
			bool bHasVelocity = false;
			bool bHasHeading = false;
			for (const UPoseSearchFeatureChannel* SubChannel : Group->SubChannels)
			{
				bHasPosition |= Cast<UPoseSearchFeatureChannel_Position>(SubChannel) != nullptr;
				bHasVelocity |= Cast<UPoseSearchFeatureChannel_Velocity>(SubChannel) != nullptr;
				bHasHeading |= Cast<UPoseSearchFeatureChannel_Heading>(SubChannel) != nullptr;
			}
			TestTrue(TEXT("The ground group retains its Position subchannel"), bHasPosition);
			TestTrue(TEXT("The ground group retains its Velocity subchannel"), bHasVelocity);
			TestTrue(TEXT("The ground group retains its Heading subchannel"), bHasHeading);
		}
	}

	UPoseSearchSchema* JumpSchema = LoadObject<UPoseSearchSchema>(
		nullptr,
		TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Jump.PSS_Rpg_Jump"));
	if (TestNotNull(TEXT("The specialized local jump schema loads"), JumpSchema))
	{
		TestEqual(TEXT("The jump schema samples at 30 Hz"), JumpSchema->SampleRate, 30);
		TestEqual(
			TEXT("The jump schema uses GASP common-schema normalization"),
			JumpSchema->DataPreprocessor,
			EPoseSearchDataPreprocessor::NormalizeWithCommonSchema);
		TestEqual(
			TEXT("The jump schema owns exactly one roled skeleton"),
			JumpSchema->GetRoledSkeletons().Num(),
			1);
		if (JumpSchema->GetRoledSkeletons().Num() == 1)
		{
			const FPoseSearchRoledSkeleton& RoledSkeleton = JumpSchema->GetRoledSkeletons()[0];
			TestEqual(
				TEXT("The jump schema uses the authoritative skeleton"),
				GetPathNameSafe(RoledSkeleton.Skeleton),
				FString(TargetSkeletonPath));
			TestEqual(
				TEXT("The jump schema uses the local mirror table"),
				GetPathNameSafe(RoledSkeleton.MirrorDataTable.Get()),
				FString(MirrorTablePath));
		}

		const TConstArrayView<TObjectPtr<UPoseSearchFeatureChannel>> Channels =
			JumpSchema->GetChannels();
		const UPoseSearchFeatureChannel_Trajectory* Trajectory =
			Channels.IsValidIndex(0)
				? Cast<UPoseSearchFeatureChannel_Trajectory>(Channels[0])
				: nullptr;
		if (TestNotNull(TEXT("The jump schema starts with its trajectory channel"), Trajectory))
		{
			TestTrue(TEXT("The jump trajectory keeps GASP weight 10"), FMath::IsNearlyEqual(Trajectory->Weight, 10.0f));
			TestEqual(TEXT("The jump trajectory has four samples"), Trajectory->Samples.Num(), 4);
			if (Trajectory->Samples.Num() == 4)
			{
				const int32 VelocityFacingFlags =
					static_cast<int32>(EPoseSearchTrajectoryFlags::VelocityDirection |
						EPoseSearchTrajectoryFlags::FacingDirection);
				const int32 FutureFlags =
					static_cast<int32>(EPoseSearchTrajectoryFlags::Velocity |
						EPoseSearchTrajectoryFlags::Position |
						EPoseSearchTrajectoryFlags::VelocityDirection);
				TestTrue(TEXT("The jump trajectory samples history at -0.2 s"), FMath::IsNearlyEqual(Trajectory->Samples[0].Offset, -0.2f));
				TestEqual(TEXT("The history sample matches position"), Trajectory->Samples[0].Flags, static_cast<int32>(EPoseSearchTrajectoryFlags::Position));
				TestTrue(TEXT("The history sample uses weight 0.25"), FMath::IsNearlyEqual(Trajectory->Samples[0].Weight, 0.25f));
				TestTrue(TEXT("The jump trajectory samples now"), FMath::IsNearlyZero(Trajectory->Samples[1].Offset));
				TestEqual(TEXT("The current sample matches velocity and facing direction"), Trajectory->Samples[1].Flags, VelocityFacingFlags);
				TestTrue(TEXT("The jump trajectory samples +0.2 s"), FMath::IsNearlyEqual(Trajectory->Samples[2].Offset, 0.2f));
				TestTrue(TEXT("The jump trajectory samples +0.5 s"), FMath::IsNearlyEqual(Trajectory->Samples[3].Offset, 0.5f));
				TestEqual(TEXT("The +0.2 sample matches the GASP future features"), Trajectory->Samples[2].Flags, FutureFlags);
				TestEqual(TEXT("The +0.5 sample matches the GASP future features"), Trajectory->Samples[3].Flags, FutureFlags);
			}
		}

		const UPoseSearchFeatureChannel_Group* Group =
			Channels.IsValidIndex(1)
				? Cast<UPoseSearchFeatureChannel_Group>(Channels[1])
				: nullptr;
		if (TestNotNull(TEXT("The jump schema retains its specialized pose group"), Group))
		{
			TestEqual(TEXT("The specialized jump group has five channels"), Group->SubChannels.Num(), 5);
			if (Group->SubChannels.Num() == 5)
			{
				const UPoseSearchFeatureChannel_Position* FootPosition =
					Cast<UPoseSearchFeatureChannel_Position>(Group->SubChannels[0]);
				const UPoseSearchFeatureChannel_Velocity* RelativeFootVelocity =
					Cast<UPoseSearchFeatureChannel_Velocity>(Group->SubChannels[1]);
				const UPoseSearchFeatureChannel_Velocity* LeftFootVelocity =
					Cast<UPoseSearchFeatureChannel_Velocity>(Group->SubChannels[2]);
				const UPoseSearchFeatureChannel_Velocity* RightFootVelocity =
					Cast<UPoseSearchFeatureChannel_Velocity>(Group->SubChannels[3]);
				const UPoseSearchFeatureChannel_Heading* PelvisHeading =
					Cast<UPoseSearchFeatureChannel_Heading>(Group->SubChannels[4]);

				if (TestNotNull(TEXT("The jump group has foot-relative position"), FootPosition))
				{
					TestEqual(TEXT("Foot position samples foot_l"), FootPosition->Bone.BoneName, FName(TEXT("foot_l")));
					TestEqual(TEXT("Foot position is relative to foot_r"), FootPosition->OriginBone.BoneName, FName(TEXT("foot_r")));
					TestTrue(TEXT("Foot position keeps weight 1"), FMath::IsNearlyEqual(FootPosition->Weight, 1.0f));
				}
				if (TestNotNull(TEXT("The jump group has foot-relative velocity"), RelativeFootVelocity))
				{
					TestEqual(TEXT("Relative velocity samples foot_l"), RelativeFootVelocity->Bone.BoneName, FName(TEXT("foot_l")));
					TestEqual(TEXT("Relative velocity uses foot_r origin"), RelativeFootVelocity->OriginBone.BoneName, FName(TEXT("foot_r")));
					TestTrue(TEXT("Relative velocity keeps weight 2"), FMath::IsNearlyEqual(RelativeFootVelocity->Weight, 2.0f));
				}
				for (const UPoseSearchFeatureChannel_Velocity* FootVelocity : { LeftFootVelocity, RightFootVelocity })
				{
					if (TestNotNull(TEXT("Each vertical-foot velocity channel resolves"), FootVelocity))
					{
						TestEqual(TEXT("Foot velocity keeps only Z"), FootVelocity->ComponentStripping, EComponentStrippingVector::StripXY);
						TestEqual(TEXT("Foot velocity shares FeetVelZ normalization"), FootVelocity->NormalizationGroup, FName(TEXT("FeetVelZ")));
						TestTrue(TEXT("Foot velocity keeps weight 0.3"), FMath::IsNearlyEqual(FootVelocity->Weight, 0.3f));
					}
				}
				if (TestNotNull(TEXT("The jump group has pelvis heading"), PelvisHeading))
				{
					TestEqual(TEXT("Pelvis heading samples pelvis"), PelvisHeading->Bone.BoneName, FName(TEXT("pelvis")));
					TestEqual(TEXT("Pelvis heading uses authored Y axis"), PelvisHeading->HeadingAxis, EHeadingAxis::Y);
					TestEqual(TEXT("Pelvis heading ignores Z"), PelvisHeading->ComponentStripping, EComponentStrippingVector::StripZ);
					TestTrue(TEXT("Pelvis heading keeps weight 0.3"), FMath::IsNearlyEqual(PelvisHeading->Weight, 0.3f));
				}
			}
		}
	}

	UPoseSearchDatabase* CrouchDatabase = LoadObject<UPoseSearchDatabase>(
		nullptr,
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Crouch.PSD_Rpg_Crouch"));
	if (TestNotNull(TEXT("The crouch database loads for category validation"), CrouchDatabase))
	{
		int32 DatabaseIdleCount = 0;
		int32 DatabaseTransitionCount = 0;
		int32 DatabaseWalkCount = 0;
		bool bContainsStandToCrouch = false;
		bool bContainsCrouchToStand = false;
		for (int32 Index = 0; Index < CrouchDatabase->GetNumAnimationAssets(); ++Index)
		{
			const UObject* AnimationAsset = CrouchDatabase->GetAnimationAsset(Index);
			if (!AnimationAsset)
			{
				continue;
			}

			const FString PackageName = AnimationAsset->GetOutermost()->GetName();
			DatabaseIdleCount += PackageName.StartsWith(CrouchIdleRoot);
			DatabaseTransitionCount += PackageName.StartsWith(CrouchTransitionRoot);
			DatabaseWalkCount += PackageName.StartsWith(CrouchWalkRoot);
			bContainsStandToCrouch |= PackageName == StandToCrouchPackage;
			bContainsCrouchToStand |= PackageName == CrouchToStandPackage;
		}
		TestEqual(TEXT("The crouch database contains one idle entry"), DatabaseIdleCount, 1);
		TestEqual(TEXT("The crouch database contains one enter-crouch transition"), DatabaseTransitionCount, 1);
		TestEqual(TEXT("The crouch database contains eight walk entries"), DatabaseWalkCount, 8);
		TestTrue(TEXT("The crouch database contains Stand-to-Crouch"), bContainsStandToCrouch);
		TestFalse(TEXT("The crouch database leaves Crouch-to-Stand to stand idle"), bContainsCrouchToStand);
	}

	static const TCHAR* const DatabasePackages[] = {
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Idle"),
		TurnInPlaceDatabasePackage,
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Crouch"),
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Walk"),
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Loops"),
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Pivots"),
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Starts"),
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Stops"),
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Sprint"),
		JumpDatabasePackage,
		LandingDatabasePackage,
	};
	static const TCHAR* const ChooserDatabasePackages[] = {
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Idle"),
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Walk"),
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run"),
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Sprint"),
		JumpDatabasePackage,
	};
	static const TCHAR* const RuntimeSplitRunDatabasePackages[] = {
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Loops"),
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Pivots"),
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Starts"),
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Stops"),
	};
	UPoseSearchNormalizationSet* NormalizationSet = LoadObject<UPoseSearchNormalizationSet>(
		nullptr,
		TEXT("/RpgGaspLocomotion/MotionMatching/NormalizationSets/PSN_Rpg_Locomotion.PSN_Rpg_Locomotion"));
	if (TestNotNull(TEXT("The shared normalization set loads"), NormalizationSet))
	{
		TestEqual(
			TEXT("The shared normalization set contains exactly eleven databases"),
			NormalizationSet->Databases.Num(),
			static_cast<int32>(UE_ARRAY_COUNT(DatabasePackages)));
	}

	constexpr TCHAR ChooserPackage[] = TEXT("/RpgGaspLocomotion/MotionMatching/Choosers/CHT_Rpg_LocomotionDatabases");
	for (const TCHAR* DatabasePackage : ChooserDatabasePackages)
	{
		TestTrue(
			*FString::Printf(TEXT("The chooser references %s"), DatabasePackage),
			AssetRegistry.ContainsDependency(
				FName(ChooserPackage),
				FName(DatabasePackage),
				UE::AssetRegistry::EDependencyCategory::Package));
	}
	for (const TCHAR* DatabasePackage : RuntimeSplitRunDatabasePackages)
	{
		TestFalse(
			*FString::Printf(TEXT("The archival chooser does not pretend to own runtime split database %s"), DatabasePackage),
			AssetRegistry.ContainsDependency(
				FName(ChooserPackage),
				FName(DatabasePackage),
				UE::AssetRegistry::EDependencyCategory::Package));
	}
	TestFalse(
		TEXT("The gait chooser does not own the runtime-latched light-landing database"),
		AssetRegistry.ContainsDependency(
			FName(ChooserPackage),
			FName(LandingDatabasePackage),
			UE::AssetRegistry::EDependencyCategory::Package));
	for (const TCHAR* DatabasePackage : DatabasePackages)
	{
		TestTrue(
			*FString::Printf(TEXT("The normalization set references %s"), DatabasePackage),
			AssetRegistry.ContainsDependency(
				FName(NormalizationPackage),
				FName(DatabasePackage),
				UE::AssetRegistry::EDependencyCategory::Package));
	}
	TestFalse(
		TEXT("The shared normalization set excludes the legacy aggregate Run database"),
		AssetRegistry.ContainsDependency(
			FName(NormalizationPackage),
			FName(TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run")),
			UE::AssetRegistry::EDependencyCategory::Package));

	UDataTable* MirrorTable = LoadObject<UDataTable>(
		nullptr,
		MirrorTablePath);
	if (TestNotNull(TEXT("The project-local mirror table loads"), MirrorTable))
	{
		TestEqual(TEXT("The mirror table uses FMirrorTableRow"), GetNameSafe(MirrorTable->GetRowStruct()), FString(TEXT("MirrorTableRow")));
		TestTrue(TEXT("The mirror table contains generated pairs"), MirrorTable->GetRowMap().Num() > 0);
	}

	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
