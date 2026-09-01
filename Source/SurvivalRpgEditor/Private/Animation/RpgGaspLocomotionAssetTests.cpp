#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AnimPose.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimSequence.h"
#include "Animation/MirrorDataTable.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/DataTable.h"
#include "Engine/SkeletalMesh.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
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
#include "Serialization/Csv/CsvParser.h"
#include "SurvivalRpg/Animation/RpgGaspPresentationProfile.h"
#include "UObject/UnrealType.h"
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
	constexpr TCHAR StandWalkRoot[] = TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/");
	constexpr TCHAR StandRunRoot[] = TEXT("/RpgGaspLocomotion/Animations/Stand/Run/");
	constexpr TCHAR StandSprintRoot[] = TEXT("/RpgGaspLocomotion/Animations/Stand/Sprint/");
	constexpr TCHAR JumpStartRoot[] = TEXT("/RpgGaspLocomotion/Animations/Jump/Starts/");
	constexpr TCHAR JumpAirborneRoot[] = TEXT("/RpgGaspLocomotion/Animations/Jump/Airborne/");
	constexpr TCHAR JumpLandRoot[] = TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/");
	constexpr TCHAR StandIdlePackage[] = TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/M_Neutral_Stand_Idle_Loop");
	constexpr TCHAR StandToCrouchPackage[] = TEXT("/RpgGaspLocomotion/Animations/Crouch/Transitions/M_Neutral_Transition_Stand_to_Crouch");
	constexpr TCHAR CrouchToStandPackage[] = TEXT("/RpgGaspLocomotion/Animations/Crouch/Transitions/M_Neutral_Transition_Crouch_to_Stand");
	constexpr TCHAR TurnInPlaceDatabasePackage[] = TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_TurnInPlace");
	constexpr TCHAR JumpDatabasePackage[] = TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Jump");
	constexpr TCHAR IdleLightLandingDatabasePackage[] = TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Idle_Lands_Light");
	constexpr TCHAR IdleHeavyLandingDatabasePackage[] = TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Idle_Lands_Heavy");
	constexpr TCHAR WalkLightLandingDatabasePackage[] = TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Walk_Lands_Light");
	constexpr TCHAR WalkHeavyLandingDatabasePackage[] = TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Walk_Lands_Heavy");
	constexpr TCHAR RunLightLandingDatabasePackage[] = TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Lands_Light");
	constexpr TCHAR RunHeavyLandingDatabasePackage[] = TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Lands_Heavy");
	constexpr TCHAR JumpSchemaPackage[] = TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Jump");
	constexpr TCHAR StopSchemaPackage[] = TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Stop");
	constexpr TCHAR WalkMovingDatabasePackage[] = TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Walk");
	constexpr TCHAR WalkStopDatabasePackage[] = TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Walk_Stops");
	constexpr TCHAR SprintMovingDatabasePackage[] = TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Sprint");
	constexpr TCHAR SprintStopDatabasePackage[] = TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Sprint_Stops");
	constexpr TCHAR MirrorTablePath[] = TEXT("/RpgGaspLocomotion/MotionMatching/MirrorTables/MDT_Rpg_Mannequin.MDT_Rpg_Mannequin");
	constexpr TCHAR PresentationProfilePath[] = TEXT("/RpgGaspLocomotion/Profiles/DA_RpgGaspPresentationProfile.DA_RpgGaspPresentationProfile");
	constexpr TCHAR PresentationProfilePackage[] = TEXT("/RpgGaspLocomotion/Profiles/DA_RpgGaspPresentationProfile");
	constexpr TCHAR RuntimeDatabaseRoot[] = TEXT("/RpgGaspLocomotion/MotionMatching/Databases/");
	constexpr TCHAR LegacyAggregateRunDatabasePackage[] = TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run");

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

	static const TCHAR* const WalkMovingAnimationPackages[] = {
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Transition_Run_to_Walk_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Transition_Run_to_Walk_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Loop_B"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Loop_BL"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Loop_BR"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Loop_F"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Loop_F_L_20"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Loop_F_R_20"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Loop_FL"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Loop_FR"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Loop_LL"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Loop_LR_offset"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Loop_RL"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Loop_RR_offset"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Pivot_B_F_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Pivot_B_F_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Pivot_F_B_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Pivot_F_B_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Reface_Start_F_R_180"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Shuffle_LR_to_LL_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Shuffle_LR_to_LL_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Shuffle_RR_to_RL_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Shuffle_RR_to_RL_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Start_B_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Start_F_Rfoot"),
	};

	static const TCHAR* const WalkStopAnimationPackages[] = {
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Stop_RR_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Stop_RR_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Stop_RL_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Stop_RL_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Stop_LR_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Stop_LR_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Stop_LL_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Stop_LL_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Stop_F_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Stop_F_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Stop_B_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Stop_B_Lfoot"),
	};

	static const TCHAR* const SprintMovingAnimationPackages[] = {
		TEXT("/RpgGaspLocomotion/Animations/Stand/Sprint/M_Neutral_Sprint_Loop_F"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Sprint/M_Neutral_Sprint_Reface_Start_F_R_180"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Sprint/M_Neutral_Sprint_Start_F_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Sprint/M_Neutral_Sprint_Turn_L_180_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Sprint/M_Neutral_Sprint_Turn_L_180_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Sprint/M_Neutral_Sprint_Turn_R_180_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Sprint/M_Neutral_Sprint_Turn_R_180_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Sprint/M_Neutral_Transition_Run_to_Sprint_Rfoot"),
	};

	static const TCHAR* const SprintStopAnimationPackages[] = {
		TEXT("/RpgGaspLocomotion/Animations/Stand/Sprint/M_Neutral_Sprint_Stop_F_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Sprint/M_Neutral_Sprint_Stop_F_Rfoot"),
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

	static const TCHAR* const KneeGuardRunAnimationPackages[] = {
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Loop_F"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Start_F_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Start_F_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_B_F_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Pivot_B_F_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Stand/Run/M_Neutral_Run_Reface_Start_F_R_090"),
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

	static const TCHAR* const IdleLightLandingAnimationPackages[] = {
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_B_Land_Stand_Light_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_F_Land_Stand_Light_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_LL_Land_Stand_Light_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_RL_Land_Stand_Light_Rfoot"),
	};

	static const TCHAR* const IdleHeavyLandingAnimationPackages[] = {
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_B_Land_Stand_Heavy_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_F_Land_Stand_Heavy_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_LL_Land_Stand_Heavy_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_RL_Land_Stand_Heavy_Rfoot"),
	};

	static const TCHAR* const WalkLightLandingAnimationPackages[] = {
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_B_Land_Walk_Light"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_F_Land_Walk_Light_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_F_Land_Walk_Light_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_LL_Land_Walk_Light"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_RL_Land_Walk_Light"),
	};

	static const TCHAR* const WalkHeavyLandingAnimationPackages[] = {
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_B_Land_Walk_Heavy"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_F_Land_Walk_Heavy_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_LL_Land_Walk_Heavy"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_RL_Land_Walk_Heavy"),
	};

	static const TCHAR* const RunLightLandingAnimationPackages[] = {
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_B_Land_Run_Light"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_F_Land_Run_Light_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_F_Land_Run_Light_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_LL_Land_Run_Light"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_RL_Land_Run_Light"),
	};

	static const TCHAR* const RunHeavyLandingAnimationPackages[] = {
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_B_Land_Run_Heavy"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_F_Land_Run_Heavy_Lfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_F_Land_Run_Heavy_Rfoot"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_LL_Land_Run_Heavy"),
		TEXT("/RpgGaspLocomotion/Animations/Jump/Lands/M_Neutral_Jump_RL_Land_Run_Heavy"),
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
			TEXT("ragdoll"),
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

	struct FRuntimeDatabaseTagContract
	{
		ERpgMotionMatchingDatabaseRole Role;
		const TCHAR* PackageName;
		const TCHAR* ExpectedRoleTag;
		const TCHAR* ExpectedStateTag;
	};

	static const FRuntimeDatabaseTagContract RuntimeDatabaseTagContracts[] = {
		{ ERpgMotionMatchingDatabaseRole::StandIdle, TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Idle"), TEXT("Rpg.MotionMatching.Role.StandIdle"), TEXT("Rpg.MotionMatching.State.Grounded") },
		{ ERpgMotionMatchingDatabaseRole::StandWalk, WalkMovingDatabasePackage, TEXT("Rpg.MotionMatching.Role.StandWalk"), TEXT("Rpg.MotionMatching.State.Grounded") },
		{ ERpgMotionMatchingDatabaseRole::StandWalkStops, WalkStopDatabasePackage, TEXT("Rpg.MotionMatching.Role.StandWalkStops"), TEXT("Rpg.MotionMatching.State.Grounded") },
		{ ERpgMotionMatchingDatabaseRole::StandRunLoops, TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Loops"), TEXT("Rpg.MotionMatching.Role.StandRunLoops"), TEXT("Rpg.MotionMatching.State.Grounded") },
		{ ERpgMotionMatchingDatabaseRole::StandRunPivots, TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Pivots"), TEXT("Rpg.MotionMatching.Role.StandRunPivots"), TEXT("Rpg.MotionMatching.State.Grounded") },
		{ ERpgMotionMatchingDatabaseRole::StandRunStarts, TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Starts"), TEXT("Rpg.MotionMatching.Role.StandRunStarts"), TEXT("Rpg.MotionMatching.State.Grounded") },
		{ ERpgMotionMatchingDatabaseRole::StandRunStops, TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Stops"), TEXT("Rpg.MotionMatching.Role.StandRunStops"), TEXT("Rpg.MotionMatching.State.Grounded") },
		{ ERpgMotionMatchingDatabaseRole::StandSprint, SprintMovingDatabasePackage, TEXT("Rpg.MotionMatching.Role.StandSprint"), TEXT("Rpg.MotionMatching.State.Grounded") },
		{ ERpgMotionMatchingDatabaseRole::StandSprintStops, SprintStopDatabasePackage, TEXT("Rpg.MotionMatching.Role.StandSprintStops"), TEXT("Rpg.MotionMatching.State.Grounded") },
		{ ERpgMotionMatchingDatabaseRole::Crouch, TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Crouch"), TEXT("Rpg.MotionMatching.Role.Crouch"), TEXT("Rpg.MotionMatching.State.Crouching") },
		{ ERpgMotionMatchingDatabaseRole::StandTurnInPlace, TurnInPlaceDatabasePackage, TEXT("Rpg.MotionMatching.Role.StandTurnInPlace"), TEXT("Rpg.MotionMatching.State.TurnInPlace") },
		{ ERpgMotionMatchingDatabaseRole::Jump, JumpDatabasePackage, TEXT("Rpg.MotionMatching.Role.Jump"), TEXT("Rpg.MotionMatching.State.Airborne") },
		{ ERpgMotionMatchingDatabaseRole::StandLightLanding, IdleLightLandingDatabasePackage, TEXT("Rpg.MotionMatching.Role.StandLightLanding"), TEXT("Rpg.MotionMatching.State.Landing") },
		{ ERpgMotionMatchingDatabaseRole::StandHeavyLanding, IdleHeavyLandingDatabasePackage, TEXT("Rpg.MotionMatching.Role.StandHeavyLanding"), TEXT("Rpg.MotionMatching.State.Landing") },
		{ ERpgMotionMatchingDatabaseRole::WalkLightLanding, WalkLightLandingDatabasePackage, TEXT("Rpg.MotionMatching.Role.WalkLightLanding"), TEXT("Rpg.MotionMatching.State.Landing") },
		{ ERpgMotionMatchingDatabaseRole::WalkHeavyLanding, WalkHeavyLandingDatabasePackage, TEXT("Rpg.MotionMatching.Role.WalkHeavyLanding"), TEXT("Rpg.MotionMatching.State.Landing") },
		{ ERpgMotionMatchingDatabaseRole::RunLightLanding, RunLightLandingDatabasePackage, TEXT("Rpg.MotionMatching.Role.RunLightLanding"), TEXT("Rpg.MotionMatching.State.Landing") },
		{ ERpgMotionMatchingDatabaseRole::RunHeavyLanding, RunHeavyLandingDatabasePackage, TEXT("Rpg.MotionMatching.Role.RunHeavyLanding"), TEXT("Rpg.MotionMatching.State.Landing") },
	};

	struct FTuningFloatDefaultContract
	{
		const TCHAR* FieldName;
		float FRpgGaspLocomotionTuning::* Member;
		float ExpectedValue;
	};

	static const FTuningFloatDefaultContract TuningFloatDefaultContracts[] = {
		{ TEXT("LastMeaningfulVelocityThreshold"), &FRpgGaspLocomotionTuning::LastMeaningfulVelocityThreshold, 5.0f },
		{ TEXT("StationarySpeedThreshold"), &FRpgGaspLocomotionTuning::StationarySpeedThreshold, 3.0f },
		{ TEXT("ChooserVelocityTolerance"), &FRpgGaspLocomotionTuning::ChooserVelocityTolerance, 0.1f },
		{ TEXT("ChooserAccelerationTolerance"), &FRpgGaspLocomotionTuning::ChooserAccelerationTolerance, 0.0001f },
		{ TEXT("WalkStopMinimumSpeed"), &FRpgGaspLocomotionTuning::WalkStopMinimumSpeed, 20.0f },
		{ TEXT("RunStopMinimumSpeed"), &FRpgGaspLocomotionTuning::RunStopMinimumSpeed, 100.0f },
		{ TEXT("SprintStopMinimumSpeed"), &FRpgGaspLocomotionTuning::SprintStopMinimumSpeed, 550.0f },
		{ TEXT("FreeRunPivotMinimumAngle"), &FRpgGaspLocomotionTuning::FreeRunPivotMinimumAngle, 45.0f },
		{ TEXT("CombatStrafeRunPivotMinimumAngle"), &FRpgGaspLocomotionTuning::CombatStrafeRunPivotMinimumAngle, 30.0f },
		{ TEXT("AimRunPivotMinimumAngle"), &FRpgGaspLocomotionTuning::AimRunPivotMinimumAngle, 0.0f },
		{ TEXT("RunStartMinimumFutureSpeedGain"), &FRpgGaspLocomotionTuning::RunStartMinimumFutureSpeedGain, 100.0f },
		{ TEXT("RunStartFutureVelocityBeginTime"), &FRpgGaspLocomotionTuning::RunStartFutureVelocityBeginTime, 0.4f },
		{ TEXT("RunStartFutureVelocityEndTime"), &FRpgGaspLocomotionTuning::RunStartFutureVelocityEndTime, 0.5f },
		{ TEXT("TurnCollectThreshold"), &FRpgGaspLocomotionTuning::TurnCollectThreshold, 20.0f },
		{ TEXT("TurnActivationThreshold"), &FRpgGaspLocomotionTuning::TurnActivationThreshold, 30.0f },
		{ TEXT("TurnCancelThreshold"), &FRpgGaspLocomotionTuning::TurnCancelThreshold, 10.0f },
		{ TEXT("TurnInactiveYawRateThreshold"), &FRpgGaspLocomotionTuning::TurnInactiveYawRateThreshold, 6.0f },
		{ TEXT("TurnStableYawRateThreshold"), &FRpgGaspLocomotionTuning::TurnStableYawRateThreshold, 60.0f },
		{ TEXT("TurnStabilityDuration"), &FRpgGaspLocomotionTuning::TurnStabilityDuration, 0.08f },
		{ TEXT("TurnCollectionTimeout"), &FRpgGaspLocomotionTuning::TurnCollectionTimeout, 0.2f },
		{ TEXT("TurnRecoveryDuration"), &FRpgGaspLocomotionTuning::TurnRecoveryDuration, 0.15f },
		{ TEXT("TurnSelectionTimeout"), &FRpgGaspLocomotionTuning::TurnSelectionTimeout, 0.25f },
		{ TEXT("TurnActiveTimeout"), &FRpgGaspLocomotionTuning::TurnActiveTimeout, 1.75f },
		{ TEXT("TurnInactiveAccumulatorTimeout"), &FRpgGaspLocomotionTuning::TurnInactiveAccumulatorTimeout, 0.2f },
		{ TEXT("TurnFacingDuration45"), &FRpgGaspLocomotionTuning::TurnFacingDuration45, 0.45f },
		{ TEXT("TurnFacingDuration90"), &FRpgGaspLocomotionTuning::TurnFacingDuration90, 0.65f },
		{ TEXT("TurnFacingDuration135"), &FRpgGaspLocomotionTuning::TurnFacingDuration135, 0.85f },
		{ TEXT("TurnFacingDuration180"), &FRpgGaspLocomotionTuning::TurnFacingDuration180, 1.0f },
		{ TEXT("BackwardJumpStartHoldTimeout"), &FRpgGaspLocomotionTuning::BackwardJumpStartHoldTimeout, 1.25f },
		{ TEXT("BackwardJumpStartReleaseLeadTime"), &FRpgGaspLocomotionTuning::BackwardJumpStartReleaseLeadTime, 0.2f },
		{ TEXT("HeavyLandingSpeedThreshold"), &FRpgGaspLocomotionTuning::HeavyLandingSpeedThreshold, 700.0f },
		{ TEXT("LandingSelectionTimeout"), &FRpgGaspLocomotionTuning::LandingSelectionTimeout, 0.25f },
		{ TEXT("LandingActiveTimeout"), &FRpgGaspLocomotionTuning::LandingActiveTimeout, 1.25f },
		{ TEXT("LandingMovementHandoffWindow"), &FRpgGaspLocomotionTuning::LandingMovementHandoffWindow, 0.3f },
	};

	static const TCHAR* DesignerPostureTuningFields[] = {
		TEXT("UnarmedIdlePostureCorrectionDegrees"),
		TEXT("UnarmedWalkPostureCorrectionDegrees"),
		TEXT("UnarmedRunPostureCorrectionDegrees"),
		TEXT("UnarmedPostureCorrectionSpeed"),
	};

	struct FLandingDatabaseContract
	{
		const TCHAR* PackageName;
		const TCHAR* ExpectedRoleTag;
		const TCHAR* const* ExpectedAnimationPackages;
		int32 ExpectedAnimationCount;
		float ExpectedContinuingPoseCostBias;
	};

	static const FLandingDatabaseContract LandingDatabaseContracts[] = {
		{ IdleLightLandingDatabasePackage, TEXT("Rpg.MotionMatching.Role.StandLightLanding"), IdleLightLandingAnimationPackages, UE_ARRAY_COUNT(IdleLightLandingAnimationPackages), -0.15f },
		{ IdleHeavyLandingDatabasePackage, TEXT("Rpg.MotionMatching.Role.StandHeavyLanding"), IdleHeavyLandingAnimationPackages, UE_ARRAY_COUNT(IdleHeavyLandingAnimationPackages), -0.15f },
		{ WalkLightLandingDatabasePackage, TEXT("Rpg.MotionMatching.Role.WalkLightLanding"), WalkLightLandingAnimationPackages, UE_ARRAY_COUNT(WalkLightLandingAnimationPackages), -0.01f },
		{ WalkHeavyLandingDatabasePackage, TEXT("Rpg.MotionMatching.Role.WalkHeavyLanding"), WalkHeavyLandingAnimationPackages, UE_ARRAY_COUNT(WalkHeavyLandingAnimationPackages), -0.01f },
		{ RunLightLandingDatabasePackage, TEXT("Rpg.MotionMatching.Role.RunLightLanding"), RunLightLandingAnimationPackages, UE_ARRAY_COUNT(RunLightLandingAnimationPackages), -0.10f },
		{ RunHeavyLandingDatabasePackage, TEXT("Rpg.MotionMatching.Role.RunHeavyLanding"), RunHeavyLandingAnimationPackages, UE_ARRAY_COUNT(RunHeavyLandingAnimationPackages), -0.01f },
	};

	static bool TryMeasureKneeAngle(
		const FAnimPose& Pose,
		const FName HipBone,
		const FName KneeBone,
		const FName AnkleBone,
		double& OutAngleDegrees)
	{
		TArray<FName> BoneNames;
		UAnimPoseExtensions::GetBoneNames(Pose, BoneNames);
		if (!BoneNames.Contains(HipBone) ||
			!BoneNames.Contains(KneeBone) ||
			!BoneNames.Contains(AnkleBone))
		{
			return false;
		}

		const FVector HipLocation = UAnimPoseExtensions::GetBonePose(
			Pose,
			HipBone,
			EAnimPoseSpaces::World).GetTranslation();
		const FVector KneeLocation = UAnimPoseExtensions::GetBonePose(
			Pose,
			KneeBone,
			EAnimPoseSpaces::World).GetTranslation();
		const FVector AnkleLocation = UAnimPoseExtensions::GetBonePose(
			Pose,
			AnkleBone,
			EAnimPoseSpaces::World).GetTranslation();
		const FVector UpperLeg = HipLocation - KneeLocation;
		const FVector LowerLeg = AnkleLocation - KneeLocation;
		const double SegmentProduct = UpperLeg.Length() * LowerLeg.Length();
		if (SegmentProduct <= UE_SMALL_NUMBER)
		{
			return false;
		}

		const double Cosine = FMath::Clamp(
			FVector::DotProduct(UpperLeg, LowerLeg) / SegmentProduct,
			-1.0,
			1.0);
		OutAngleDegrees = FMath::RadiansToDegrees(FMath::Acos(Cosine));
		return FMath::IsFinite(OutAngleDegrees);
	}

	struct FKneePoseMetrics
	{
		double MaximumAngleDegrees = 0.0;
		double NearStraightFrameFraction = 1.0;
		int32 SampleCount = 0;
	};

	static bool TryMeasureKneePoseMetrics(
		UAnimSequence* Animation,
		USkeletalMesh* TargetMesh,
		const EAnimDataEvalType EvaluationType,
		const double SampleRate,
		const double NearStraightDegrees,
		FKneePoseMetrics& OutMetrics,
		FString& OutFailure)
	{
		const double Duration = Animation ? Animation->GetPlayLength() : 0.0;
		if (Duration <= 0.0)
		{
			OutFailure = TEXT("animation has no duration");
			return false;
		}

		const int32 LastSampleIndex = FMath::CeilToInt(Duration * SampleRate);
		TArray<double> SampleTimes;
		SampleTimes.Reserve(LastSampleIndex + 1);
		for (int32 SampleIndex = 0; SampleIndex <= LastSampleIndex; ++SampleIndex)
		{
			SampleTimes.Add(FMath::Min(Duration, SampleIndex / SampleRate));
		}

		FAnimPoseEvaluationOptions EvaluationOptions;
		EvaluationOptions.EvaluationType = EvaluationType;
		EvaluationOptions.bShouldRetarget = true;
		EvaluationOptions.bExtractRootMotion = false;
		EvaluationOptions.bEvaluateCurves = false;
		EvaluationOptions.OptionalSkeletalMesh = TargetMesh;

		TArray<FAnimPose> Poses;
		UAnimPoseExtensions::GetAnimPoseAtTimeIntervals(
			Animation,
			SampleTimes,
			EvaluationOptions,
			Poses);
		if (Poses.Num() != SampleTimes.Num())
		{
			OutFailure = FString::Printf(
				TEXT("evaluated %d of %d requested poses"),
				Poses.Num(),
				SampleTimes.Num());
			return false;
		}

		const FName LeftHipBone(TEXT("thigh_l"));
		const FName LeftKneeBone(TEXT("calf_l"));
		const FName LeftAnkleBone(TEXT("foot_l"));
		const FName RightHipBone(TEXT("thigh_r"));
		const FName RightKneeBone(TEXT("calf_r"));
		const FName RightAnkleBone(TEXT("foot_r"));
		int32 NearStraightFrameCount = 0;
		for (int32 PoseIndex = 0; PoseIndex < Poses.Num(); ++PoseIndex)
		{
			const FAnimPose& Pose = Poses[PoseIndex];
			if (!UAnimPoseExtensions::IsValid(Pose))
			{
				OutFailure = FString::Printf(
					TEXT("invalid pose at %.6f s"),
					SampleTimes[PoseIndex]);
				return false;
			}

			double LeftKneeAngle = 0.0;
			double RightKneeAngle = 0.0;
			if (!TryMeasureKneeAngle(
					Pose,
					LeftHipBone,
					LeftKneeBone,
					LeftAnkleBone,
					LeftKneeAngle) ||
				!TryMeasureKneeAngle(
					Pose,
					RightHipBone,
					RightKneeBone,
					RightAnkleBone,
					RightKneeAngle))
			{
				OutFailure = FString::Printf(
					TEXT("invalid leg chain at %.6f s"),
					SampleTimes[PoseIndex]);
				return false;
			}

			const double FrameKneeAngle = FMath::Max(LeftKneeAngle, RightKneeAngle);
			OutMetrics.MaximumAngleDegrees = FMath::Max(
				OutMetrics.MaximumAngleDegrees,
				FrameKneeAngle);
			NearStraightFrameCount += FrameKneeAngle >= NearStraightDegrees;
		}

		OutMetrics.SampleCount = Poses.Num();
		OutMetrics.NearStraightFrameFraction = Poses.IsEmpty()
			? 1.0
			: static_cast<double>(NearStraightFrameCount) / Poses.Num();
		return !Poses.IsEmpty();
	}
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
	TestEqual(TEXT("The curated plugin contains exactly 216 assets"), Assets.Num(), 216);

	TMap<FString, int32> ClassCounts;
	int32 AnimationCount = 0;
	int32 CrouchIdleCount = 0;
	int32 CrouchTransitionCount = 0;
	int32 CrouchWalkCount = 0;
	int32 StandIdleCount = 0;
	int32 StandWalkCount = 0;
	int32 StandRunCount = 0;
	int32 StandSprintCount = 0;
	int32 TurnInPlaceSequenceCount = 0;
	int32 JumpStartCount = 0;
	int32 JumpAirborneCount = 0;
	int32 JumpLandCount = 0;
	TMap<FString, ERpgGaspPresentationAssetCategory> ExpectedPresentationMembership;
	TSet<FString> StandRunAnimationPackages;
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
		StandWalkCount += AnimationPackageName.StartsWith(StandWalkRoot);
		const bool bIsStandRunAnimation = AnimationPackageName.StartsWith(StandRunRoot);
		StandRunCount += bIsStandRunAnimation;
		if (bIsStandRunAnimation)
		{
			StandRunAnimationPackages.Add(AnimationPackageName);
		}
		StandSprintCount += AnimationPackageName.StartsWith(StandSprintRoot);
		JumpStartCount += AnimationPackageName.StartsWith(JumpStartRoot);
		JumpAirborneCount += AnimationPackageName.StartsWith(JumpAirborneRoot);
		JumpLandCount += AnimationPackageName.StartsWith(JumpLandRoot);
		if (AnimationPackageName.StartsWith(StandWalkRoot) ||
			AnimationPackageName.StartsWith(StandRunRoot) ||
			AnimationPackageName.StartsWith(StandSprintRoot))
		{
			ExpectedPresentationMembership.Add(
				AnimationPackageName,
				ERpgGaspPresentationAssetCategory::GroundMoving);
		}
		else if (AnimationPackageName.StartsWith(JumpStartRoot))
		{
			ExpectedPresentationMembership.Add(
				AnimationPackageName,
				AnimationPackageName.Contains(TEXT("/M_Neutral_Jump_B_Start_"))
					? ERpgGaspPresentationAssetCategory::BackwardJumpStart
					: ERpgGaspPresentationAssetCategory::JumpStart);
		}
		else if (AnimationPackageName.StartsWith(JumpAirborneRoot))
		{
			ExpectedPresentationMembership.Add(
				AnimationPackageName,
				ERpgGaspPresentationAssetCategory::AirborneFall);
		}
		else if (AnimationPackageName.StartsWith(JumpLandRoot))
		{
			ExpectedPresentationMembership.Add(
				AnimationPackageName,
				ERpgGaspPresentationAssetCategory::Landing);
		}
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

	TestEqual(TEXT("Exactly 190 curated AnimSequences are present"), AnimationCount, 190);
	TestEqual(TEXT("Exactly one crouch idle sequence is present"), CrouchIdleCount, 1);
	TestEqual(TEXT("Exactly two crouch transition sequences are present"), CrouchTransitionCount, 2);
	TestEqual(TEXT("Exactly eight crouch walk sequences are present"), CrouchWalkCount, 8);
	TestEqual(TEXT("Exactly nine stand-idle-folder sequences are present"), StandIdleCount, 9);
	TestEqual(TEXT("Exactly eight turn-in-place sequences are present"), TurnInPlaceSequenceCount, 8);
	TestEqual(TEXT("Exactly 37 stand-walk sequences are present"), StandWalkCount, 37);
	TestEqual(TEXT("Exactly 77 stand-run sequences are present"), StandRunCount, 77);

	const FString ManifestPath = FPaths::Combine(
		Plugin->GetBaseDir(),
		TEXT("Docs"),
		TEXT("CuratedAssetManifest.csv"));
	FString ManifestSource;
	if (TestTrue(
			TEXT("The curated asset manifest is readable"),
			FFileHelper::LoadFileToString(ManifestSource, *ManifestPath)))
	{
		const FCsvParser ManifestParser(MoveTemp(ManifestSource));
		const FCsvParser::FRows& ManifestRows = ManifestParser.GetRows();
		if (TestTrue(TEXT("The curated asset manifest has a header"), !ManifestRows.IsEmpty()))
		{
			const TArray<const TCHAR*>& Header = ManifestRows[0];
			const auto FindColumn = [&Header](const TCHAR* ColumnName) -> int32
			{
				for (int32 ColumnIndex = 0; ColumnIndex < Header.Num(); ++ColumnIndex)
				{
					if (FCString::Strcmp(Header[ColumnIndex], ColumnName) == 0)
					{
						return ColumnIndex;
					}
				}
				return INDEX_NONE;
			};
			const int32 TargetPackageColumn = FindColumn(TEXT("TargetPackage"));
			const int32 DatabaseGroupColumn = FindColumn(TEXT("DatabaseGroup"));
			const int32 RetargetProfileColumn = FindColumn(TEXT("RetargetProfile"));
			const bool bHasRequiredColumns =
				TargetPackageColumn != INDEX_NONE &&
				DatabaseGroupColumn != INDEX_NONE &&
				RetargetProfileColumn != INDEX_NONE;
			if (TestTrue(
					TEXT("The curated asset manifest exposes the rollout contract columns"),
					bHasRequiredColumns))
			{
				const int32 LastRequiredColumn = FMath::Max3(
					TargetPackageColumn,
					DatabaseGroupColumn,
					RetargetProfileColumn);
				TSet<FString> ManifestRunPackages;
				int32 StandRunManifestRowCount = 0;
				for (int32 RowIndex = 1; RowIndex < ManifestRows.Num(); ++RowIndex)
				{
					const TArray<const TCHAR*>& Row = ManifestRows[RowIndex];
					if (Row.Num() <= DatabaseGroupColumn ||
						FCString::Strcmp(Row[DatabaseGroupColumn], TEXT("Stand.Run")) != 0)
					{
						continue;
					}

					++StandRunManifestRowCount;
					if (!TestTrue(
							*FString::Printf(TEXT("Stand.Run manifest row %d is complete"), RowIndex + 1),
							Row.Num() > LastRequiredColumn))
					{
						continue;
					}

					const FString TargetPackage(Row[TargetPackageColumn]);
					TestEqual(
						*FString::Printf(TEXT("%s uses the complete run retarget profile"), *TargetPackage),
						FString(Row[RetargetProfileColumn]),
						FString(TEXT("rpg_no_leg_source_blend_v1")));
					TestFalse(
						*FString::Printf(TEXT("%s appears only once in the Stand.Run manifest"), *TargetPackage),
						ManifestRunPackages.Contains(TargetPackage));
					ManifestRunPackages.Add(TargetPackage);
				}

				TestEqual(
					TEXT("Exactly 77 Stand.Run manifest rows use the rollout contract"),
					StandRunManifestRowCount,
					77);
				TestEqual(
					TEXT("The Stand.Run manifest contains 77 unique target packages"),
					ManifestRunPackages.Num(),
					77);
				for (const FString& ManifestPackage : ManifestRunPackages)
				{
					TestTrue(
						*FString::Printf(TEXT("The manifest target %s exists in the curated run folder"), *ManifestPackage),
						StandRunAnimationPackages.Contains(ManifestPackage));
				}
				for (const FString& AnimationPackage : StandRunAnimationPackages)
				{
					TestTrue(
						*FString::Printf(TEXT("The curated run asset %s is covered by the manifest"), *AnimationPackage),
						ManifestRunPackages.Contains(AnimationPackage));
				}
			}
		}
	}
	TestEqual(TEXT("Exactly ten stand-sprint sequences are present"), StandSprintCount, 10);
	TestEqual(TEXT("Exactly 18 jump start/off sequences are present"), JumpStartCount, 18);
	TestEqual(TEXT("Exactly one neutral airborne fall loop is present"), JumpAirborneCount, 1);
	TestEqual(TEXT("Exactly 27 curated Idle/Walk/Run landing sequences are present"), JumpLandCount, 27);
	TestEqual(TEXT("Exactly one ChooserTable is present"), ClassCounts.FindRef(TEXT("ChooserTable")), 1);
	TestEqual(TEXT("Exactly one MirrorDataTable is present"), ClassCounts.FindRef(TEXT("MirrorDataTable")), 1);
	TestEqual(TEXT("Exactly nineteen PoseSearchDatabases are present"), ClassCounts.FindRef(TEXT("PoseSearchDatabase")), 19);
	TestEqual(TEXT("Exactly one PoseSearchNormalizationSet is present"), ClassCounts.FindRef(TEXT("PoseSearchNormalizationSet")), 1);
	TestEqual(TEXT("Exactly three PoseSearchSchemas are present"), ClassCounts.FindRef(TEXT("PoseSearchSchema")), 3);
	TestEqual(TEXT("Exactly one GASP presentation profile is present"), ClassCounts.FindRef(TEXT("RpgGaspPresentationProfile")), 1);

	URpgGaspPresentationProfile* PresentationProfile =
		LoadObject<URpgGaspPresentationProfile>(nullptr, PresentationProfilePath);
	if (TestNotNull(TEXT("The project-owned GASP presentation profile loads"), PresentationProfile))
	{
		FDataValidationContext PresentationValidationContext;
		TestEqual(
			TEXT("The presentation profile passes Unreal asset data validation"),
			PresentationProfile->IsDataValid(PresentationValidationContext),
			EDataValidationResult::Valid);
		TestTrue(
			TEXT("The presentation profile passes reusable native validation"),
			PresentationProfile->ValidateProfile().IsValid());
		TestEqual(
			TEXT("The old package classifier's complete curated domain has 170 assets"),
			ExpectedPresentationMembership.Num(),
			170);
		TestEqual(
			TEXT("The presentation profile preserves all 170 old classifier memberships"),
			PresentationProfile->AssetMemberships.Num(),
			170);

		TSet<FString> ActualPresentationPackages;
		TMap<ERpgGaspPresentationAssetCategory, int32> CategoryCounts;
		for (int32 Index = 0; Index < PresentationProfile->AssetMemberships.Num(); ++Index)
		{
			const FRpgGaspPresentationAssetMembership& Membership =
				PresentationProfile->AssetMemberships[Index];
			const UAnimSequenceBase* Sequence = Membership.Asset.Get();
			if (!TestNotNull(
					*FString::Printf(TEXT("Presentation membership %d resolves its sequence"), Index),
					Sequence))
			{
				continue;
			}

			const FString PackageName = Sequence->GetOutermost()->GetName();
			TestFalse(
				*FString::Printf(TEXT("%s appears only once in the presentation profile"), *PackageName),
				ActualPresentationPackages.Contains(PackageName));
			ActualPresentationPackages.Add(PackageName);
			++CategoryCounts.FindOrAdd(Membership.Category);

			const ERpgGaspPresentationAssetCategory* ExpectedCategory =
				ExpectedPresentationMembership.Find(PackageName);
			if (TestNotNull(
					*FString::Printf(TEXT("%s belonged to the old classifier domain"), *PackageName),
					ExpectedCategory))
			{
				TestEqual(
					*FString::Printf(TEXT("%s preserves its explicit presentation category"), *PackageName),
					static_cast<uint8>(Membership.Category),
					static_cast<uint8>(*ExpectedCategory));
			}
		}

		TestEqual(TEXT("GroundMoving membership preserves all Walk/Run/Sprint clips"), CategoryCounts.FindRef(ERpgGaspPresentationAssetCategory::GroundMoving), 124);
		TestEqual(TEXT("Ordinary JumpStart membership contains exactly sixteen clips"), CategoryCounts.FindRef(ERpgGaspPresentationAssetCategory::JumpStart), 16);
		TestEqual(TEXT("BackwardJumpStart membership contains exactly two clips"), CategoryCounts.FindRef(ERpgGaspPresentationAssetCategory::BackwardJumpStart), 2);
		TestEqual(TEXT("AirborneFall membership contains exactly one clip"), CategoryCounts.FindRef(ERpgGaspPresentationAssetCategory::AirborneFall), 1);
		TestEqual(TEXT("Landing membership contains all twenty-seven clips"), CategoryCounts.FindRef(ERpgGaspPresentationAssetCategory::Landing), 27);
		for (const TPair<FString, ERpgGaspPresentationAssetCategory>& Expected : ExpectedPresentationMembership)
		{
			TestTrue(
				*FString::Printf(TEXT("%s is explicitly represented in the profile"), *Expected.Key),
				ActualPresentationPackages.Contains(Expected.Key));
		}

		TestEqual(
			TEXT("The presentation profile hard-references exactly eighteen runtime databases"),
			PresentationProfile->RuntimeMotionMatchingDatabases.Num(),
			static_cast<int32>(UE_ARRAY_COUNT(RuntimeDatabaseTagContracts)));
		TSet<const UPoseSearchDatabase*> UniqueProfileDatabases;
		TSet<ERpgMotionMatchingDatabaseRole> UniqueProfileRoles;
		TSet<FString> ProfileDatabaseObjectPaths;
		for (int32 Index = 0;
			Index < PresentationProfile->RuntimeMotionMatchingDatabases.Num();
			++Index)
		{
			const UPoseSearchDatabase* Database =
				PresentationProfile->RuntimeMotionMatchingDatabases[Index].Get();
			if (!TestNotNull(
					*FString::Printf(TEXT("Profile runtime database %d resolves"), Index),
					Database))
			{
				continue;
			}

			TestFalse(
				*FString::Printf(TEXT("Profile runtime database %s appears only once"), *Database->GetName()),
				UniqueProfileDatabases.Contains(Database));
			UniqueProfileDatabases.Add(Database);
			ProfileDatabaseObjectPaths.Add(Database->GetPathName());

			const ERpgMotionMatchingDatabaseRole Role =
				RpgGaspLocomotionConfig::ResolveDatabaseRoleTag(MakeArrayView(Database->Tags));
			TestTrue(
				*FString::Printf(TEXT("Profile runtime database %s resolves one project role tag"), *Database->GetName()),
				Role != ERpgMotionMatchingDatabaseRole::None &&
					Role != ERpgMotionMatchingDatabaseRole::Count);
			TestFalse(
				*FString::Printf(TEXT("Profile role %d is assigned only once"), static_cast<int32>(Role)),
				UniqueProfileRoles.Contains(Role));
			UniqueProfileRoles.Add(Role);

			const FRuntimeDatabaseTagContract* ExpectedContract = nullptr;
			for (const FRuntimeDatabaseTagContract& Contract : RuntimeDatabaseTagContracts)
			{
				if (Contract.Role == Role)
				{
					ExpectedContract = &Contract;
					break;
				}
			}
			if (TestNotNull(
					*FString::Printf(TEXT("Profile role %d has an exact asset contract"), static_cast<int32>(Role)),
					ExpectedContract))
			{
				const FString ExpectedObjectPath = FString::Printf(
					TEXT("%s.%s"),
					ExpectedContract->PackageName,
					*FPackageName::GetLongPackageAssetName(ExpectedContract->PackageName));
				TestEqual(
					*FString::Printf(TEXT("Profile role %s resolves the exact project database"), ExpectedContract->ExpectedRoleTag),
					Database->GetPathName(),
					ExpectedObjectPath);
				TestEqual(
					*FString::Printf(TEXT("Native role %s retains its exact authored tag"), ExpectedContract->ExpectedRoleTag),
					RpgGaspLocomotionConfig::GetDatabaseRoleTag(Role),
					FName(ExpectedContract->ExpectedRoleTag));
			}
		}
		TestEqual(
			TEXT("The presentation profile runtime database references are unique"),
			UniqueProfileDatabases.Num(),
			static_cast<int32>(UE_ARRAY_COUNT(RuntimeDatabaseTagContracts)));
		TestEqual(
			TEXT("The presentation profile assigns every runtime database role exactly once"),
			UniqueProfileRoles.Num(),
			static_cast<int32>(UE_ARRAY_COUNT(RuntimeDatabaseTagContracts)));
		for (const FRuntimeDatabaseTagContract& Contract : RuntimeDatabaseTagContracts)
		{
			TestTrue(
				*FString::Printf(TEXT("The presentation profile contains role %s"), Contract.ExpectedRoleTag),
				UniqueProfileRoles.Contains(Contract.Role));
		}
		TestFalse(
			TEXT("The presentation profile excludes the archival aggregate Run database"),
			ProfileDatabaseObjectPaths.Contains(
				FString::Printf(
					TEXT("%s.%s"),
					LegacyAggregateRunDatabasePackage,
					*FPackageName::GetLongPackageAssetName(LegacyAggregateRunDatabasePackage))));

		TestTrue(
			TEXT("The profile locomotion tuning remains runtime-valid"),
			RpgGaspLocomotionConfig::IsTuningRuntimeValid(PresentationProfile->LocomotionTuning));
		TestEqual(
			TEXT("The presentation tuning contract covers all thirty-four compatibility defaults"),
			static_cast<int32>(UE_ARRAY_COUNT(TuningFloatDefaultContracts)),
			34);
		int32 ReflectedCompatibilityTuningFloatCount = 0;
		for (TFieldIterator<FFloatProperty> PropertyIt(FRpgGaspLocomotionTuning::StaticStruct());
			PropertyIt;
			++PropertyIt)
		{
			bool bIsDesignerPostureTuning = false;
			for (const TCHAR* FieldName : DesignerPostureTuningFields)
			{
				if (PropertyIt->GetFName() == FName(FieldName))
				{
					bIsDesignerPostureTuning = true;
					break;
				}
			}
			ReflectedCompatibilityTuningFloatCount += bIsDesignerPostureTuning ? 0 : 1;
		}
		TestEqual(
			TEXT("Every non-posture tuning float has an explicit compatibility default contract"),
			ReflectedCompatibilityTuningFloatCount,
			static_cast<int32>(UE_ARRAY_COUNT(TuningFloatDefaultContracts)));
		for (const TCHAR* FieldName : DesignerPostureTuningFields)
		{
			TestNotNull(
				*FString::Printf(TEXT("Designer posture tuning field %s remains a reflected float"), FieldName),
				FindFProperty<FFloatProperty>(
					FRpgGaspLocomotionTuning::StaticStruct(),
					FieldName));
		}
		for (const FTuningFloatDefaultContract& Contract : TuningFloatDefaultContracts)
		{
			TestNotNull(
				*FString::Printf(TEXT("Profile tuning field %s remains a reflected float"), Contract.FieldName),
				FindFProperty<FFloatProperty>(
					FRpgGaspLocomotionTuning::StaticStruct(),
					Contract.FieldName));
			TestEqual(
				*FString::Printf(TEXT("Profile tuning %s preserves its compatibility default"), Contract.FieldName),
				PresentationProfile->LocomotionTuning.*Contract.Member,
				Contract.ExpectedValue);
		}
		UE::AssetRegistry::FDependencyQuery RuntimeCookDependencyQuery;
		RuntimeCookDependencyQuery.Required =
			UE::AssetRegistry::EDependencyProperty::Hard |
			UE::AssetRegistry::EDependencyProperty::Game;
		TArray<FName> DirectProfileCookDependencies;
		AssetRegistry.GetDependencies(
			FName(PresentationProfilePackage),
			DirectProfileCookDependencies,
			UE::AssetRegistry::EDependencyCategory::Package,
			RuntimeCookDependencyQuery);
		TSet<FName> DirectRuntimeDatabaseCookDependencies;
		for (const FName Dependency : DirectProfileCookDependencies)
		{
			if (Dependency.ToString().StartsWith(RuntimeDatabaseRoot))
			{
				DirectRuntimeDatabaseCookDependencies.Add(Dependency);
			}
		}
		TestEqual(
			TEXT("The profile records exactly eighteen direct hard game/cook database dependencies"),
			DirectRuntimeDatabaseCookDependencies.Num(),
			static_cast<int32>(UE_ARRAY_COUNT(RuntimeDatabaseTagContracts)));
		for (const FRuntimeDatabaseTagContract& Contract : RuntimeDatabaseTagContracts)
		{
			TestTrue(
				*FString::Printf(TEXT("The profile directly hard-references cook database %s"), Contract.PackageName),
				DirectRuntimeDatabaseCookDependencies.Contains(FName(Contract.PackageName)));
		}
		TestFalse(
			TEXT("The profile cook dependencies exclude the archival aggregate Run database"),
			DirectRuntimeDatabaseCookDependencies.Contains(FName(LegacyAggregateRunDatabasePackage)));
	}

	static const FDatabaseContract DatabaseContracts[] = {
		{ TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Idle"), TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/"), TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Locomotion"), 1, 2 },
		{ TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_TurnInPlace"), TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/"), TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Locomotion"), 8, 8 },
		{ TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Crouch"), TEXT("/RpgGaspLocomotion/Animations/Crouch/"), TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Locomotion"), 10, 10 },
		{ WalkMovingDatabasePackage, TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/"), TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Locomotion"), 25, 25 },
		{ WalkStopDatabasePackage, TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/"), TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Locomotion"), 12, 12 },
		{ TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Loops"), TEXT("/RpgGaspLocomotion/Animations/Stand/Run/"), TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Locomotion"), 11, 11 },
		{ TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Pivots"), TEXT("/RpgGaspLocomotion/Animations/Stand/Run/"), TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Locomotion"), 34, 34 },
		{ TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Starts"), TEXT("/RpgGaspLocomotion/Animations/Stand/Run/"), TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Locomotion"), 14, 14 },
		{ TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Stops"), TEXT("/RpgGaspLocomotion/Animations/Stand/Run/"), TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Locomotion"), 12, 12 },
		{ SprintMovingDatabasePackage, TEXT("/RpgGaspLocomotion/Animations/Stand/Sprint/"), TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Locomotion"), 8, 8 },
		{ SprintStopDatabasePackage, TEXT("/RpgGaspLocomotion/Animations/Stand/Sprint/"), StopSchemaPackage, 2, 2 },
		{ JumpDatabasePackage, TEXT("/RpgGaspLocomotion/Animations/Jump/"), JumpSchemaPackage, 19, 19 },
		{ IdleLightLandingDatabasePackage, JumpLandRoot, JumpSchemaPackage, 4, 4 },
		{ IdleHeavyLandingDatabasePackage, JumpLandRoot, JumpSchemaPackage, 4, 4 },
		{ WalkLightLandingDatabasePackage, JumpLandRoot, JumpSchemaPackage, 5, 5 },
		{ WalkHeavyLandingDatabasePackage, JumpLandRoot, JumpSchemaPackage, 4, 4 },
		{ RunLightLandingDatabasePackage, JumpLandRoot, JumpSchemaPackage, 5, 5 },
		{ RunHeavyLandingDatabasePackage, JumpLandRoot, JumpSchemaPackage, 5, 5 },
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

	TestEqual(
		TEXT("Exactly eighteen project runtime database tag contracts are declared"),
		static_cast<int32>(UE_ARRAY_COUNT(RuntimeDatabaseTagContracts)),
		18);
	const auto IsProjectRoleTag = [](FName Tag)
	{
		return Tag.ToString().StartsWith(
			TEXT("Rpg.MotionMatching.Role."),
			ESearchCase::CaseSensitive);
	};
	const auto IsProjectStateTag = [](FName Tag)
	{
		return Tag.ToString().StartsWith(
			TEXT("Rpg.MotionMatching.State."),
			ESearchCase::CaseSensitive);
	};
	for (const FRuntimeDatabaseTagContract& Contract : RuntimeDatabaseTagContracts)
	{
		const FString ObjectPath = FString::Printf(
			TEXT("%s.%s"),
			Contract.PackageName,
			*FPackageName::GetLongPackageAssetName(Contract.PackageName));
		UPoseSearchDatabase* Database = LoadObject<UPoseSearchDatabase>(nullptr, *ObjectPath);
		if (!TestNotNull(
				*FString::Printf(TEXT("Runtime database %s loads for tag validation"), Contract.PackageName),
				Database))
		{
			continue;
		}

		int32 ProjectRoleTagCount = 0;
		int32 ProjectStateTagCount = 0;
		int32 ExpectedRoleTagCount = 0;
		int32 ExpectedStateTagCount = 0;
		for (const FName Tag : Database->Tags)
		{
			ProjectRoleTagCount += IsProjectRoleTag(Tag);
			ProjectStateTagCount += IsProjectStateTag(Tag);
			ExpectedRoleTagCount += Tag == FName(Contract.ExpectedRoleTag);
			ExpectedStateTagCount += Tag == FName(Contract.ExpectedStateTag);
		}

		TestEqual(
			*FString::Printf(TEXT("%s carries exactly one project role tag"), Contract.PackageName),
			ProjectRoleTagCount,
			1);
		TestEqual(
			*FString::Printf(TEXT("%s carries its exact project role tag"), Contract.PackageName),
			ExpectedRoleTagCount,
			1);
		TestEqual(
			*FString::Printf(TEXT("%s carries exactly one project state tag"), Contract.PackageName),
			ProjectStateTagCount,
			1);
		TestEqual(
			*FString::Printf(TEXT("%s carries its exact project state tag"), Contract.PackageName),
			ExpectedStateTagCount,
			1);
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
		TestTrue(
			TEXT("The turn-in-place database keeps the GASP TurnInPlace selection tag"),
			TurnInPlaceDatabase->Tags.Contains(FName(TEXT("TurnInPlace"))));
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
		int32 LegacyProjectRoleTagCount = 0;
		int32 LegacyProjectStateTagCount = 0;
		for (const FName Tag : StandRunDatabase->Tags)
		{
			LegacyProjectRoleTagCount += IsProjectRoleTag(Tag);
			LegacyProjectStateTagCount += IsProjectStateTag(Tag);
		}
		TestEqual(
			TEXT("The archival aggregate Run database owns no project runtime role tag"),
			LegacyProjectRoleTagCount,
			0);
		TestEqual(
			TEXT("The archival aggregate Run database owns no project runtime state tag"),
			LegacyProjectStateTagCount,
			0);

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
	UPoseSearchDatabase* RunPivotDatabase = LoadObject<UPoseSearchDatabase>(
		nullptr,
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Pivots.PSD_Rpg_Stand_Run_Pivots"));
	if (TestNotNull(TEXT("The Sparse run-pivot database loads for source-tag validation"), RunPivotDatabase))
	{
		TestTrue(
			TEXT("The Sparse run-pivot database keeps the source Pivots tag"),
			RunPivotDatabase->Tags.Contains(FName(TEXT("Pivots"))));
	}

	const auto ValidateOrderedDatabaseMembers = [this](
		const TCHAR* PackageName,
		TConstArrayView<const TCHAR*> ExpectedPackages) -> UPoseSearchDatabase*
	{
		const FString ObjectPath = FString::Printf(
			TEXT("%s.%s"),
			PackageName,
			*FPackageName::GetLongPackageAssetName(PackageName));
		UPoseSearchDatabase* Database = LoadObject<UPoseSearchDatabase>(nullptr, *ObjectPath);
		if (!TestNotNull(
				*FString::Printf(TEXT("The ordered database %s loads"), PackageName),
				Database))
		{
			return nullptr;
		}

		TestEqual(
			*FString::Printf(TEXT("%s has the exact member count"), PackageName),
			Database->GetNumAnimationAssets(),
			ExpectedPackages.Num());
		TSet<FString> UniquePackages;
		for (int32 Index = 0;
			Index < ExpectedPackages.Num() && Index < Database->GetNumAnimationAssets();
			++Index)
		{
			const FPoseSearchDatabaseAnimationAsset* Entry = Database->GetDatabaseAnimationAsset(Index);
			const UObject* AnimationAsset = Entry ? Entry->GetAnimationAsset() : nullptr;
			if (TestNotNull(
					*FString::Printf(TEXT("%s entry %d resolves"), PackageName, Index),
					AnimationAsset))
			{
				const FString ActualPackage = AnimationAsset->GetOutermost()->GetName();
				TestEqual(
					*FString::Printf(TEXT("%s entry %d keeps source order"), PackageName, Index),
					ActualPackage,
					FString(ExpectedPackages[Index]));
				UniquePackages.Add(ActualPackage);
			}
		}
		TestEqual(
			*FString::Printf(TEXT("%s contains no duplicate animations"), PackageName),
			UniquePackages.Num(),
			ExpectedPackages.Num());
		return Database;
	};

	UPoseSearchDatabase* WalkMovingDatabase = ValidateOrderedDatabaseMembers(
		WalkMovingDatabasePackage,
		MakeArrayView(WalkMovingAnimationPackages));
	UPoseSearchDatabase* SprintMovingDatabase = ValidateOrderedDatabaseMembers(
		SprintMovingDatabasePackage,
		MakeArrayView(SprintMovingAnimationPackages));

	const auto ValidateStopDatabase = [this, &ValidateOrderedDatabaseMembers](
		const TCHAR* PackageName,
		TConstArrayView<const TCHAR*> ExpectedPackages,
		const TCHAR* ExpectedSchemaPackage,
		const float ExpectedContinuingPoseBias) -> UPoseSearchDatabase*
	{
		UPoseSearchDatabase* Database = ValidateOrderedDatabaseMembers(PackageName, ExpectedPackages);
		if (!Database)
		{
			return nullptr;
		}

		const FString ExpectedSchemaObject = FString::Printf(
			TEXT("%s.%s"),
			ExpectedSchemaPackage,
			*FPackageName::GetLongPackageAssetName(ExpectedSchemaPackage));
		TestEqual(
			*FString::Printf(TEXT("%s uses its exact local schema"), PackageName),
			GetPathNameSafe(Database->Schema),
			ExpectedSchemaObject);
		TestEqual(
			*FString::Printf(TEXT("%s uses shared normalization"), PackageName),
			GetPathNameSafe(Database->NormalizationSet.Get()),
			FString(TEXT("/RpgGaspLocomotion/MotionMatching/NormalizationSets/PSN_Rpg_Locomotion.PSN_Rpg_Locomotion")));
		TestEqual(
			*FString::Printf(TEXT("%s uses the local preview mesh"), PackageName),
			GetPathNameSafe(Database->PreviewMesh),
			FString(TargetMeshPath));

		TestTrue(
			*FString::Printf(TEXT("%s keeps its source continuing-pose bias"), PackageName),
			FMath::IsNearlyEqual(Database->ContinuingPoseCostBias, ExpectedContinuingPoseBias));
		TestTrue(TEXT("Stop database base bias remains zero"), FMath::IsNearlyZero(Database->BaseCostBias));
		TestTrue(TEXT("Stop database looping bias remains -0.005"), FMath::IsNearlyEqual(Database->LoopingCostBias, -0.005f));
		TestTrue(TEXT("Stop database continuing-interaction bias remains zero"), FMath::IsNearlyZero(Database->ContinuingInteractionCostBias));
		TestTrue(TEXT("Stop database context-interaction bias remains zero"), FMath::IsNearlyZero(Database->ContinuingContextInteractionCostBias));
		TestTrue(
			TEXT("Stop database keeps the source exclusion interval"),
			FMath::IsNearlyZero(Database->ExcludeFromDatabaseParameters.Min) &&
				FMath::IsNearlyEqual(Database->ExcludeFromDatabaseParameters.Max, -0.3f));
		TestTrue(
			TEXT("Stop database keeps source extrapolation bounds"),
			FMath::IsNearlyEqual(Database->AdditionalExtrapolationTime.Min, -100.0f) &&
				FMath::IsNearlyEqual(Database->AdditionalExtrapolationTime.Max, 100.0f));
		TestEqual(TEXT("Stop database uses PCA KD-tree search"), Database->PoseSearchMode, EPoseSearchMode::PCAKDTree);
		TestEqual(TEXT("Stop database keeps four principal components"), Database->NumberOfPrincipalComponents, 4);
		TestEqual(TEXT("Stop database keeps leaf size 16"), Database->KDTreeMaxLeafSize, 16);
		TestEqual(TEXT("Stop database keeps 200 query neighbors"), Database->KDTreeQueryNumNeighbors, 200);
		TestTrue(TEXT("Stop database pose pruning stays disabled"), FMath::IsNearlyZero(Database->PosePruningSimilarityThreshold));
		TestTrue(TEXT("Stop database PCA pruning stays disabled"), FMath::IsNearlyZero(Database->PCAValuesPruningSimilarityThreshold));
		TestEqual(TEXT("Stop database duplicate-neighbor cap stays disabled"), Database->KDTreeQueryNumNeighborsWithDuplicates, 0);

		for (int32 Index = 0; Index < Database->GetNumAnimationAssets(); ++Index)
		{
			const FPoseSearchDatabaseAnimationAsset* Entry = Database->GetDatabaseAnimationAsset(Index);
			if (!TestNotNull(TEXT("Every Stop entry resolves for metadata validation"), Entry))
			{
				continue;
			}
			TestTrue(TEXT("Every Stop entry remains enabled"), Entry->IsEnabled());
			TestTrue(TEXT("Every Stop entry disables reselection"), Entry->IsDisableReselection());
			TestEqual(TEXT("Every Stop entry remains unmirrored-only"), Entry->GetMirrorOption(), EPoseSearchMirrorOption::UnmirroredOnly);
			TestEqual(TEXT("Every Stop entry has no external BranchIn id"), Entry->BranchInId, 0);
			TestFalse(TEXT("Every Stop entry has no external synchronization"), Entry->IsSynchronizedWithExternalDependency());
			TestFalse(TEXT("Every Stop entry uses ordinary sequence sampling"), Entry->bUseSingleSample);
			TestFalse(TEXT("Every Stop entry disables blend-space grid sampling"), Entry->bUseGridForSampling);
			TestEqual(TEXT("Every Stop entry keeps nine horizontal samples"), Entry->NumberOfHorizontalSamples, 9);
			TestEqual(TEXT("Every Stop entry keeps two vertical samples"), Entry->NumberOfVerticalSamples, 2);
			TestTrue(TEXT("Every Stop entry keeps zero blend parameters"), FMath::IsNearlyZero(Entry->BlendParamX) && FMath::IsNearlyZero(Entry->BlendParamY));
			const FFloatInterval SamplingRange = Entry->GetSamplingRange();
			TestTrue(TEXT("Every Stop entry uses full-range [0,0] sampling"), FMath::IsNearlyZero(SamplingRange.Min) && FMath::IsNearlyZero(SamplingRange.Max));
			TestFalse(TEXT("Every Stop entry is non-looping"), Entry->IsLooping());
		}
		return Database;
	};

	UPoseSearchDatabase* WalkStopDatabase = ValidateStopDatabase(
		WalkStopDatabasePackage,
		MakeArrayView(WalkStopAnimationPackages),
		TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Locomotion"),
		-0.01f);
	UPoseSearchDatabase* SprintStopDatabase = ValidateStopDatabase(
		SprintStopDatabasePackage,
		MakeArrayView(SprintStopAnimationPackages),
		StopSchemaPackage,
		-0.2f);
	if (SprintStopDatabase)
	{
		TestTrue(
			TEXT("Sprint Stop database keeps the source Stops tag"),
			SprintStopDatabase->Tags.Contains(FName(TEXT("Stops"))));
	}

	const auto ReadDatabasePackageSet = [](const UPoseSearchDatabase* Database)
	{
		TSet<FString> Packages;
		if (Database)
		{
			for (int32 Index = 0; Index < Database->GetNumAnimationAssets(); ++Index)
			{
				const UObject* Asset = Database->GetAnimationAsset(Index);
				if (Asset)
				{
					Packages.Add(Asset->GetOutermost()->GetName());
				}
			}
		}
		return Packages;
	};
	const auto TestNoPoolOverlap = [this, &ReadDatabasePackageSet](
		const TCHAR* Label,
		const UPoseSearchDatabase* MovingDatabase,
		const UPoseSearchDatabase* StopDatabase,
		const int32 ExpectedUnionCount)
	{
		TSet<FString> MovingPackages = ReadDatabasePackageSet(MovingDatabase);
		const TSet<FString> StopPackages = ReadDatabasePackageSet(StopDatabase);
		int32 OverlapCount = 0;
		for (const FString& StopPackage : StopPackages)
		{
			OverlapCount += MovingPackages.Contains(StopPackage);
			MovingPackages.Add(StopPackage);
		}
		TestEqual(*FString::Printf(TEXT("%s moving and Stop pools do not overlap"), Label), OverlapCount, 0);
		TestEqual(*FString::Printf(TEXT("%s pools retain the exact unique union"), Label), MovingPackages.Num(), ExpectedUnionCount);
	};
	TestNoPoolOverlap(TEXT("Walk"), WalkMovingDatabase, WalkStopDatabase, 37);
	TestNoPoolOverlap(TEXT("Sprint"), SprintMovingDatabase, SprintStopDatabase, 10);

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

	const FString ExpectedJumpSchemaObject = FString::Printf(
		TEXT("%s.%s"),
		JumpSchemaPackage,
		*FPackageName::GetLongPackageAssetName(JumpSchemaPackage));
	const FString ExpectedNormalizationObject = FString::Printf(
		TEXT("%s.%s"),
		NormalizationPackage,
		*FPackageName::GetLongPackageAssetName(NormalizationPackage));
	for (const FLandingDatabaseContract& Contract : LandingDatabaseContracts)
	{
		const TConstArrayView<const TCHAR*> ExpectedPackages(
			Contract.ExpectedAnimationPackages,
			Contract.ExpectedAnimationCount);
		UPoseSearchDatabase* LandingDatabase = ValidateOrderedDatabaseMembers(
			Contract.PackageName,
			ExpectedPackages);
		if (!LandingDatabase)
		{
			continue;
		}

		TestEqual(
			*FString::Printf(TEXT("%s uses PSS_Rpg_Jump"), Contract.PackageName),
			GetPathNameSafe(LandingDatabase->Schema),
			ExpectedJumpSchemaObject);
		TestEqual(
			*FString::Printf(TEXT("%s uses PSN_Rpg_Locomotion"), Contract.PackageName),
			GetPathNameSafe(LandingDatabase->NormalizationSet.Get()),
			ExpectedNormalizationObject);
		const bool bLegacyIdleLightDatabase =
			FCString::Strcmp(Contract.PackageName, IdleLightLandingDatabasePackage) == 0;
		TestEqual(
			*FString::Printf(TEXT("%s keeps its curated preview-mesh contract"), Contract.PackageName),
			GetPathNameSafe(LandingDatabase->PreviewMesh),
			bLegacyIdleLightDatabase ? FString(TEXT("None")) : FString(TargetMeshPath));

		TestTrue(
			*FString::Printf(TEXT("%s keeps its source continuing-pose bias"), Contract.PackageName),
			FMath::IsNearlyEqual(
				LandingDatabase->ContinuingPoseCostBias,
				Contract.ExpectedContinuingPoseCostBias));
		TestTrue(
			*FString::Printf(TEXT("%s keeps zero base bias"), Contract.PackageName),
			FMath::IsNearlyZero(LandingDatabase->BaseCostBias));
		TestTrue(
			*FString::Printf(TEXT("%s keeps looping bias -0.005"), Contract.PackageName),
			FMath::IsNearlyEqual(LandingDatabase->LoopingCostBias, -0.005f));
		TestTrue(
			*FString::Printf(TEXT("%s keeps zero continuing-interaction bias"), Contract.PackageName),
			FMath::IsNearlyZero(LandingDatabase->ContinuingInteractionCostBias));
		TestTrue(
			*FString::Printf(TEXT("%s keeps zero context-interaction bias"), Contract.PackageName),
			FMath::IsNearlyZero(LandingDatabase->ContinuingContextInteractionCostBias));
		TestTrue(
			*FString::Printf(TEXT("%s keeps the source exclusion interval"), Contract.PackageName),
			FMath::IsNearlyZero(LandingDatabase->ExcludeFromDatabaseParameters.Min) &&
				FMath::IsNearlyEqual(LandingDatabase->ExcludeFromDatabaseParameters.Max, -0.3f));
		TestTrue(
			*FString::Printf(TEXT("%s keeps source extrapolation bounds"), Contract.PackageName),
			FMath::IsNearlyEqual(LandingDatabase->AdditionalExtrapolationTime.Min, -100.0f) &&
				FMath::IsNearlyEqual(LandingDatabase->AdditionalExtrapolationTime.Max, 100.0f));

		int32 ProjectRoleTagCount = 0;
		int32 ProjectStateTagCount = 0;
		int32 ExpectedRoleTagCount = 0;
		int32 LandingStateTagCount = 0;
		for (const FName Tag : LandingDatabase->Tags)
		{
			ProjectRoleTagCount += IsProjectRoleTag(Tag);
			ProjectStateTagCount += IsProjectStateTag(Tag);
			ExpectedRoleTagCount += Tag == FName(Contract.ExpectedRoleTag);
			LandingStateTagCount += Tag == FName(TEXT("Rpg.MotionMatching.State.Landing"));
		}
		TestEqual(
			*FString::Printf(TEXT("%s carries exactly one project role tag"), Contract.PackageName),
			ProjectRoleTagCount,
			1);
		TestEqual(
			*FString::Printf(TEXT("%s carries its exact landing role"), Contract.PackageName),
			ExpectedRoleTagCount,
			1);
		TestEqual(
			*FString::Printf(TEXT("%s carries exactly one project state tag"), Contract.PackageName),
			ProjectStateTagCount,
			1);
		TestEqual(
			*FString::Printf(TEXT("%s carries State.Landing"), Contract.PackageName),
			LandingStateTagCount,
			1);

		for (int32 Index = 0; Index < LandingDatabase->GetNumAnimationAssets(); ++Index)
		{
			const FPoseSearchDatabaseAnimationAsset* Entry = LandingDatabase->GetDatabaseAnimationAsset(Index);
			if (!TestNotNull(
					*FString::Printf(TEXT("%s entry %d resolves for landing metadata"), Contract.PackageName, Index),
					Entry))
			{
				continue;
			}

			TestTrue(
				*FString::Printf(TEXT("%s entry %d remains enabled"), Contract.PackageName, Index),
				Entry->IsEnabled());
			TestTrue(
				*FString::Printf(TEXT("%s entry %d disables reselection"), Contract.PackageName, Index),
				Entry->IsDisableReselection());
			TestEqual(
				*FString::Printf(TEXT("%s entry %d remains unmirrored-only"), Contract.PackageName, Index),
				Entry->GetMirrorOption(),
				EPoseSearchMirrorOption::UnmirroredOnly);
			TestEqual(
				*FString::Printf(TEXT("%s entry %d has no external BranchIn id"), Contract.PackageName, Index),
				Entry->BranchInId,
				0);
			TestFalse(
				*FString::Printf(TEXT("%s entry %d has no external synchronization"), Contract.PackageName, Index),
				Entry->IsSynchronizedWithExternalDependency());
			TestFalse(
				*FString::Printf(TEXT("%s entry %d uses ordinary sequence sampling"), Contract.PackageName, Index),
				Entry->bUseSingleSample);
			TestFalse(
				*FString::Printf(TEXT("%s entry %d disables blend-space grid sampling"), Contract.PackageName, Index),
				Entry->bUseGridForSampling);
			TestEqual(
				*FString::Printf(TEXT("%s entry %d keeps nine horizontal samples"), Contract.PackageName, Index),
				Entry->NumberOfHorizontalSamples,
				9);
			TestEqual(
				*FString::Printf(TEXT("%s entry %d keeps two vertical samples"), Contract.PackageName, Index),
				Entry->NumberOfVerticalSamples,
				2);
			TestTrue(
				*FString::Printf(TEXT("%s entry %d keeps zero blend parameters"), Contract.PackageName, Index),
				FMath::IsNearlyZero(Entry->BlendParamX) && FMath::IsNearlyZero(Entry->BlendParamY));
			const FFloatInterval SamplingRange = Entry->GetSamplingRange();
			TestTrue(
				*FString::Printf(TEXT("%s entry %d uses full-range sampling"), Contract.PackageName, Index),
				FMath::IsNearlyZero(SamplingRange.Min) && FMath::IsNearlyZero(SamplingRange.Max));
			TestFalse(
				*FString::Printf(TEXT("%s entry %d remains non-looping"), Contract.PackageName, Index),
				Entry->IsLooping());

			UAnimSequence* LandingAnimation = Cast<UAnimSequence>(Entry->GetAnimationAsset());
			if (TestNotNull(
					*FString::Printf(TEXT("%s entry %d resolves as an AnimSequence"), Contract.PackageName, Index),
					LandingAnimation))
			{
				TestTrue(
					*FString::Printf(TEXT("%s keeps root motion enabled"), *LandingAnimation->GetName()),
					LandingAnimation->bEnableRootMotion);
				TestTrue(
					*FString::Printf(TEXT("%s keeps contact_l"), *LandingAnimation->GetName()),
					LandingAnimation->HasCurveData(TEXT("contact_l"), false));
				TestTrue(
					*FString::Printf(TEXT("%s keeps contact_r"), *LandingAnimation->GetName()),
					LandingAnimation->HasCurveData(TEXT("contact_r"), false));
			}

			if (ExpectedPackages.IsValidIndex(Index))
			{
				TArray<FName> Dependencies;
				AssetRegistry.GetDependencies(
					FName(ExpectedPackages[Index]),
					Dependencies,
					UE::AssetRegistry::EDependencyCategory::Package);
				for (const FName Dependency : Dependencies)
				{
					TestFalse(
						*FString::Printf(TEXT("%s has no excluded landing dependency on %s"), ExpectedPackages[Index], *Dependency.ToString()),
						IsForbiddenDependency(Dependency.ToString()));
				}
			}
		}

		const UE::PoseSearch::EAsyncBuildIndexResult BuildResult =
			UE::PoseSearch::FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(
				LandingDatabase,
				UE::PoseSearch::ERequestAsyncBuildFlag::NewRequest |
					UE::PoseSearch::ERequestAsyncBuildFlag::WaitForCompletion);
		if (TestTrue(
				*FString::Printf(TEXT("%s search index builds"), Contract.PackageName),
				BuildResult == UE::PoseSearch::EAsyncBuildIndexResult::Success))
		{
			TestEqual(
				*FString::Printf(TEXT("%s indexes every authored landing entry"), Contract.PackageName),
				LandingDatabase->GetSearchIndex().Assets.Num(),
				Contract.ExpectedAnimationCount);
		}
	}

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

	UPoseSearchSchema* StopSchema = LoadObject<UPoseSearchSchema>(
		nullptr,
		TEXT("/RpgGaspLocomotion/MotionMatching/Schemas/PSS_Rpg_Stop.PSS_Rpg_Stop"));
	if (TestNotNull(TEXT("The source-exact local Stop schema loads"), StopSchema))
	{
		TestEqual(TEXT("The Stop schema samples at 30 Hz"), StopSchema->SampleRate, 30);
		TestEqual(
			TEXT("The Stop schema uses common-schema normalization"),
			StopSchema->DataPreprocessor,
			EPoseSearchDataPreprocessor::NormalizeWithCommonSchema);
		TestEqual(TEXT("The Stop schema finalizes to cardinality 30"), StopSchema->SchemaCardinality, 30);
		TestEqual(TEXT("The Stop schema indexes one permutation"), StopSchema->NumberOfPermutations, 1);
		TestEqual(TEXT("The Stop schema permutation rate stays at 30 Hz"), StopSchema->PermutationsSampleRate, 30);
		TestTrue(TEXT("The Stop schema has no permutation offset"), FMath::IsNearlyZero(StopSchema->PermutationsTimeOffset));
		TestFalse(TEXT("The Stop schema adds no data padding"), StopSchema->bAddDataPadding);
		TestFalse(TEXT("The Stop schema injects no debug channels"), StopSchema->bInjectAdditionalDebugChannels);
		TestFalse(TEXT("The Stop schema draws no injected debug channels"), StopSchema->bDrawInjectAdditionalDebugChannels);

		TestEqual(TEXT("The Stop schema owns one roled skeleton"), StopSchema->GetRoledSkeletons().Num(), 1);
		if (StopSchema->GetRoledSkeletons().Num() == 1)
		{
			const FPoseSearchRoledSkeleton& RoledSkeleton = StopSchema->GetRoledSkeletons()[0];
			TestEqual(TEXT("The Stop schema uses the authoritative skeleton"), GetPathNameSafe(RoledSkeleton.Skeleton), FString(TargetSkeletonPath));
			TestEqual(TEXT("The Stop schema uses the local mirror table"), GetPathNameSafe(RoledSkeleton.MirrorDataTable.Get()), FString(MirrorTablePath));
			TestTrue(TEXT("The Stop schema default role stays unnamed"), RoledSkeleton.Role.IsNone());
			TestTrue(TEXT("The Stop schema requires no PoseHistory curves"), RoledSkeleton.RequiredCurves.IsEmpty());
			TestEqual(TEXT("The Stop schema resolves exactly four bones"), RoledSkeleton.BoneReferences.Num(), 4);
			static const FName ExpectedStopBones[] = {
				FName(TEXT("root")),
				FName(TEXT("foot_l")),
				FName(TEXT("foot_r")),
				FName(TEXT("pelvis")),
			};
			for (int32 Index = 0;
				Index < UE_ARRAY_COUNT(ExpectedStopBones) && Index < RoledSkeleton.BoneReferences.Num();
				++Index)
			{
				TestEqual(
					*FString::Printf(TEXT("Stop schema bone %d keeps source order"), Index),
					RoledSkeleton.BoneReferences[Index].BoneName,
					ExpectedStopBones[Index]);
			}
		}

		const auto TestChannelSpan = [this](
			const TCHAR* Label,
			const UPoseSearchFeatureChannel* Channel,
			const int32 ExpectedOffset,
			const int32 ExpectedCardinality)
		{
			if (TestNotNull(Label, Channel))
			{
				TestEqual(*FString::Printf(TEXT("%s data offset"), Label), Channel->GetChannelDataOffset(), ExpectedOffset);
				TestEqual(*FString::Printf(TEXT("%s cardinality"), Label), Channel->GetChannelCardinality(), ExpectedCardinality);
			}
		};
		const auto TestPositionChannel = [this, &TestChannelSpan](
			const TCHAR* Label,
			const UPoseSearchFeatureChannel* Channel,
			const int32 ExpectedOffset,
			const int32 ExpectedCardinality,
			const FName ExpectedBone,
			const FName ExpectedOriginBone,
			const float ExpectedTime,
			const float ExpectedWeight,
			const EInputQueryPose ExpectedQuery,
			const EComponentStrippingVector ExpectedStripping)
		{
			const UPoseSearchFeatureChannel_Position* Position = Cast<UPoseSearchFeatureChannel_Position>(Channel);
			TestChannelSpan(Label, Position, ExpectedOffset, ExpectedCardinality);
			if (!Position)
			{
				return;
			}
			TestTrue(*FString::Printf(TEXT("%s defaults empty bones to root"), Label), Position->bDefaultWithRootBone);
			TestEqual(*FString::Printf(TEXT("%s bone"), Label), Position->Bone.BoneName, ExpectedBone);
			TestEqual(*FString::Printf(TEXT("%s origin bone"), Label), Position->OriginBone.BoneName, ExpectedOriginBone);
			TestTrue(*FString::Printf(TEXT("%s sample role"), Label), Position->SampleRole.IsNone());
			TestTrue(*FString::Printf(TEXT("%s origin role"), Label), Position->OriginRole.IsNone());
			TestTrue(*FString::Printf(TEXT("%s weight"), Label), FMath::IsNearlyEqual(Position->Weight, ExpectedWeight));
			TestEqual(*FString::Printf(TEXT("%s sampling attribute"), Label), Position->SamplingAttributeId, INDEX_NONE);
			TestTrue(*FString::Printf(TEXT("%s sample time"), Label), FMath::IsNearlyEqual(Position->SampleTimeOffset, ExpectedTime));
			TestTrue(*FString::Printf(TEXT("%s origin time"), Label), FMath::IsNearlyZero(Position->OriginTimeOffset));
			TestEqual(*FString::Printf(TEXT("%s query pose"), Label), Position->InputQueryPose, ExpectedQuery);
			TestEqual(*FString::Printf(TEXT("%s component stripping"), Label), Position->ComponentStripping, ExpectedStripping);
			TestEqual(*FString::Printf(TEXT("%s permutation mode"), Label), Position->PermutationTimeType, EPermutationTimeType::UseSampleTime);
			TestFalse(*FString::Printf(TEXT("%s does not normalize displacement"), Label), Position->bNormalizeDisplacement);
			TestTrue(*FString::Printf(TEXT("%s has no normalization group"), Label), Position->NormalizationGroup.IsNone());
		};
		const auto TestVelocityChannel = [this, &TestChannelSpan](
			const TCHAR* Label,
			const UPoseSearchFeatureChannel* Channel,
			const int32 ExpectedOffset,
			const int32 ExpectedCardinality,
			const FName ExpectedBone,
			const FName ExpectedOriginBone,
			const float ExpectedTime,
			const float ExpectedWeight,
			const EInputQueryPose ExpectedQuery,
			const EComponentStrippingVector ExpectedStripping,
			const bool bExpectedCharacterSpace,
			const bool bExpectedNormalized,
			const FName ExpectedNormalizationGroup)
		{
			const UPoseSearchFeatureChannel_Velocity* Velocity = Cast<UPoseSearchFeatureChannel_Velocity>(Channel);
			TestChannelSpan(Label, Velocity, ExpectedOffset, ExpectedCardinality);
			if (!Velocity)
			{
				return;
			}
			TestTrue(*FString::Printf(TEXT("%s defaults empty bones to root"), Label), Velocity->bDefaultWithRootBone);
			TestEqual(*FString::Printf(TEXT("%s bone"), Label), Velocity->Bone.BoneName, ExpectedBone);
			TestEqual(*FString::Printf(TEXT("%s origin bone"), Label), Velocity->OriginBone.BoneName, ExpectedOriginBone);
			TestTrue(*FString::Printf(TEXT("%s sample role"), Label), Velocity->SampleRole.IsNone());
			TestTrue(*FString::Printf(TEXT("%s origin role"), Label), Velocity->OriginRole.IsNone());
			TestTrue(*FString::Printf(TEXT("%s weight"), Label), FMath::IsNearlyEqual(Velocity->Weight, ExpectedWeight));
			TestEqual(*FString::Printf(TEXT("%s sampling attribute"), Label), Velocity->SamplingAttributeId, INDEX_NONE);
			TestTrue(*FString::Printf(TEXT("%s sample time"), Label), FMath::IsNearlyEqual(Velocity->SampleTimeOffset, ExpectedTime));
			TestTrue(*FString::Printf(TEXT("%s origin time"), Label), FMath::IsNearlyZero(Velocity->OriginTimeOffset));
			TestEqual(*FString::Printf(TEXT("%s query pose"), Label), Velocity->InputQueryPose, ExpectedQuery);
			TestEqual(*FString::Printf(TEXT("%s component stripping"), Label), Velocity->ComponentStripping, ExpectedStripping);
			TestEqual(*FString::Printf(TEXT("%s permutation mode"), Label), Velocity->PermutationTimeType, EPermutationTimeType::UseSampleTime);
			TestEqual(*FString::Printf(TEXT("%s character-space velocity"), Label), Velocity->bUseCharacterSpaceVelocities, bExpectedCharacterSpace);
			TestEqual(*FString::Printf(TEXT("%s normalized velocity"), Label), Velocity->bNormalize, bExpectedNormalized);
			TestEqual(*FString::Printf(TEXT("%s normalization group"), Label), Velocity->NormalizationGroup, ExpectedNormalizationGroup);
		};
		const auto TestHeadingChannel = [this, &TestChannelSpan](
			const TCHAR* Label,
			const UPoseSearchFeatureChannel* Channel,
			const int32 ExpectedOffset,
			const int32 ExpectedCardinality,
			const FName ExpectedBone,
			const FName ExpectedOriginBone,
			const float ExpectedTime,
			const float ExpectedWeight,
			const EInputQueryPose ExpectedQuery,
			const EHeadingAxis ExpectedAxis,
			const EComponentStrippingVector ExpectedStripping)
		{
			const UPoseSearchFeatureChannel_Heading* Heading = Cast<UPoseSearchFeatureChannel_Heading>(Channel);
			TestChannelSpan(Label, Heading, ExpectedOffset, ExpectedCardinality);
			if (!Heading)
			{
				return;
			}
			TestTrue(*FString::Printf(TEXT("%s defaults empty bones to root"), Label), Heading->bDefaultWithRootBone);
			TestEqual(*FString::Printf(TEXT("%s bone"), Label), Heading->Bone.BoneName, ExpectedBone);
			TestEqual(*FString::Printf(TEXT("%s origin bone"), Label), Heading->OriginBone.BoneName, ExpectedOriginBone);
			TestTrue(*FString::Printf(TEXT("%s sample role"), Label), Heading->SampleRole.IsNone());
			TestTrue(*FString::Printf(TEXT("%s origin role"), Label), Heading->OriginRole.IsNone());
			TestTrue(*FString::Printf(TEXT("%s weight"), Label), FMath::IsNearlyEqual(Heading->Weight, ExpectedWeight));
			TestEqual(*FString::Printf(TEXT("%s sampling attribute"), Label), Heading->SamplingAttributeId, INDEX_NONE);
			TestTrue(*FString::Printf(TEXT("%s sample time"), Label), FMath::IsNearlyEqual(Heading->SampleTimeOffset, ExpectedTime));
			TestTrue(*FString::Printf(TEXT("%s origin time"), Label), FMath::IsNearlyZero(Heading->OriginTimeOffset));
			TestEqual(*FString::Printf(TEXT("%s heading axis"), Label), Heading->HeadingAxis, ExpectedAxis);
			TestEqual(*FString::Printf(TEXT("%s query pose"), Label), Heading->InputQueryPose, ExpectedQuery);
			TestEqual(*FString::Printf(TEXT("%s component stripping"), Label), Heading->ComponentStripping, ExpectedStripping);
			TestEqual(*FString::Printf(TEXT("%s permutation mode"), Label), Heading->PermutationTimeType, EPermutationTimeType::UseSampleTime);
			TestTrue(*FString::Printf(TEXT("%s has no normalization group"), Label), Heading->NormalizationGroup.IsNone());
		};

		const TConstArrayView<TObjectPtr<UPoseSearchFeatureChannel>> Channels = StopSchema->GetChannels();
		TestEqual(TEXT("The Stop schema has exactly Trajectory + Group"), Channels.Num(), 2);
		const UPoseSearchFeatureChannel_Trajectory* Trajectory =
			Channels.IsValidIndex(0) ? Cast<UPoseSearchFeatureChannel_Trajectory>(Channels[0]) : nullptr;
		const UPoseSearchFeatureChannel_Group* Group =
			Channels.IsValidIndex(1) ? Cast<UPoseSearchFeatureChannel_Group>(Channels[1]) : nullptr;
		TestChannelSpan(TEXT("Stop Trajectory"), Trajectory, 0, 19);
		TestChannelSpan(TEXT("Stop Group"), Group, 19, 11);
		if (Trajectory)
		{
			TestTrue(TEXT("Stop Trajectory keeps global weight 5"), FMath::IsNearlyEqual(Trajectory->Weight, 5.0f));
			TestTrue(TEXT("Stop Trajectory uses the default role"), Trajectory->SampleRole.IsNone());
			TestEqual(TEXT("Stop Trajectory has no debug weight group"), Trajectory->DebugWeightGroupID, INDEX_NONE);
			TestEqual(TEXT("Stop Trajectory owns four source samples"), Trajectory->Samples.Num(), 4);
			static const float ExpectedOffsets[] = { -0.05f, 0.0f, 0.35f, 0.7f };
			static const int32 ExpectedFlags[] = { 32, 148, 160, 176 };
			static const float ExpectedWeights[] = { 0.3f, 2.0f, 1.0f, 1.0f };
			for (int32 Index = 0; Index < 4 && Index < Trajectory->Samples.Num(); ++Index)
			{
				const FPoseSearchTrajectorySample& Sample = Trajectory->Samples[Index];
				TestTrue(*FString::Printf(TEXT("Stop sample %d keeps its time"), Index), FMath::IsNearlyEqual(Sample.Offset, ExpectedOffsets[Index]));
				TestEqual(*FString::Printf(TEXT("Stop sample %d keeps its feature flags"), Index), Sample.Flags, ExpectedFlags[Index]);
				TestTrue(*FString::Printf(TEXT("Stop sample %d keeps its weight"), Index), FMath::IsNearlyEqual(Sample.Weight, ExpectedWeights[Index]));
				TestTrue(*FString::Printf(TEXT("Stop sample %d has no normalization group"), Index), Sample.NormalizationGroup.IsNone());
			}

			TestEqual(TEXT("Stop Trajectory finalizes nine leaves"), Trajectory->SubChannels.Num(), 9);
			if (Trajectory->SubChannels.Num() == 9)
			{
				TestPositionChannel(TEXT("Stop history PositionXY"), Trajectory->SubChannels[0], 0, 2, NAME_None, NAME_None, -0.05f, 1.5f, EInputQueryPose::UseCharacterPose, EComponentStrippingVector::StripZ);
				TestVelocityChannel(TEXT("Stop current VelocityXY"), Trajectory->SubChannels[1], 2, 2, NAME_None, NAME_None, 0.0f, 10.0f, EInputQueryPose::UseCharacterPose, EComponentStrippingVector::StripZ, false, false, NAME_None);
				TestVelocityChannel(TEXT("Stop current normalized VelocityDirection"), Trajectory->SubChannels[2], 4, 3, NAME_None, NAME_None, 0.0f, 10.0f, EInputQueryPose::UseCharacterPose, EComponentStrippingVector::None, false, true, NAME_None);
				TestHeadingChannel(TEXT("Stop current HeadingXY"), Trajectory->SubChannels[3], 7, 2, NAME_None, NAME_None, 0.0f, 10.0f, EInputQueryPose::UseCharacterPose, EHeadingAxis::X, EComponentStrippingVector::StripZ);
				TestPositionChannel(TEXT("Stop future PositionXY 0.35"), Trajectory->SubChannels[4], 9, 2, NAME_None, NAME_None, 0.35f, 5.0f, EInputQueryPose::UseCharacterPose, EComponentStrippingVector::StripZ);
				TestHeadingChannel(TEXT("Stop future HeadingXY 0.35"), Trajectory->SubChannels[5], 11, 2, NAME_None, NAME_None, 0.35f, 5.0f, EInputQueryPose::UseCharacterPose, EHeadingAxis::X, EComponentStrippingVector::StripZ);
				TestPositionChannel(TEXT("Stop future PositionXY 0.7"), Trajectory->SubChannels[6], 13, 2, NAME_None, NAME_None, 0.7f, 5.0f, EInputQueryPose::UseCharacterPose, EComponentStrippingVector::StripZ);
				TestVelocityChannel(TEXT("Stop future VelocityXY 0.7"), Trajectory->SubChannels[7], 15, 2, NAME_None, NAME_None, 0.7f, 5.0f, EInputQueryPose::UseCharacterPose, EComponentStrippingVector::StripZ, false, false, NAME_None);
				TestHeadingChannel(TEXT("Stop future HeadingXY 0.7"), Trajectory->SubChannels[8], 17, 2, NAME_None, NAME_None, 0.7f, 5.0f, EInputQueryPose::UseCharacterPose, EHeadingAxis::X, EComponentStrippingVector::StripZ);
			}
		}
		if (Group)
		{
			TestTrue(TEXT("Stop Group uses the default role"), Group->SampleRole.IsNone());
			TestEqual(TEXT("Stop Group has no debug weight group"), Group->DebugWeightGroupID, INDEX_NONE);
			TestEqual(TEXT("Stop Group owns four source leaves"), Group->SubChannels.Num(), 4);
			if (Group->SubChannels.Num() == 4)
			{
				TestPositionChannel(TEXT("Stop foot-relative position"), Group->SubChannels[0], 19, 3, FName(TEXT("foot_l")), FName(TEXT("foot_r")), 0.0f, 1.0f, EInputQueryPose::UseContinuingPose, EComponentStrippingVector::None);
				TestVelocityChannel(TEXT("Stop left-foot velocity"), Group->SubChannels[1], 22, 3, FName(TEXT("foot_l")), NAME_None, 0.0f, 0.3f, EInputQueryPose::UseContinuingPose, EComponentStrippingVector::None, true, false, FName(TEXT("FeetVelZ")));
				TestVelocityChannel(TEXT("Stop right-foot velocity"), Group->SubChannels[2], 25, 3, FName(TEXT("foot_r")), NAME_None, 0.0f, 0.3f, EInputQueryPose::UseContinuingPose, EComponentStrippingVector::None, true, false, FName(TEXT("FeetVelZ")));
				TestHeadingChannel(TEXT("Stop pelvis heading"), Group->SubChannels[3], 28, 2, FName(TEXT("pelvis")), NAME_None, 0.0f, 0.3f, EInputQueryPose::UseContinuingPose, EHeadingAxis::Y, EComponentStrippingVector::StripZ);
			}
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
		WalkMovingDatabasePackage,
		WalkStopDatabasePackage,
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Loops"),
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Pivots"),
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Starts"),
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Stops"),
		SprintMovingDatabasePackage,
		SprintStopDatabasePackage,
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Crouch"),
		TurnInPlaceDatabasePackage,
		JumpDatabasePackage,
		IdleLightLandingDatabasePackage,
		IdleHeavyLandingDatabasePackage,
		WalkLightLandingDatabasePackage,
		WalkHeavyLandingDatabasePackage,
		RunLightLandingDatabasePackage,
		RunHeavyLandingDatabasePackage,
	};
	static const TCHAR* const ChooserDatabasePackages[] = {
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Idle"),
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Walk"),
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run"),
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Sprint"),
		JumpDatabasePackage,
	};
	static const TCHAR* const ChooserExcludedRuntimeDatabasePackages[] = {
		TurnInPlaceDatabasePackage,
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Crouch"),
		WalkStopDatabasePackage,
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Loops"),
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Pivots"),
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Starts"),
		TEXT("/RpgGaspLocomotion/MotionMatching/Databases/PSD_Rpg_Stand_Run_Stops"),
		SprintStopDatabasePackage,
		IdleLightLandingDatabasePackage,
		IdleHeavyLandingDatabasePackage,
		WalkLightLandingDatabasePackage,
		WalkHeavyLandingDatabasePackage,
		RunLightLandingDatabasePackage,
		RunHeavyLandingDatabasePackage,
	};
	UPoseSearchNormalizationSet* NormalizationSet = LoadObject<UPoseSearchNormalizationSet>(
		nullptr,
		TEXT("/RpgGaspLocomotion/MotionMatching/NormalizationSets/PSN_Rpg_Locomotion.PSN_Rpg_Locomotion"));
	if (TestNotNull(TEXT("The shared normalization set loads"), NormalizationSet))
	{
		TestEqual(
			TEXT("The shared normalization set contains exactly eighteen databases"),
			NormalizationSet->Databases.Num(),
			static_cast<int32>(UE_ARRAY_COUNT(DatabasePackages)));
		TSet<FString> UniqueNormalizationPackages;
		for (int32 Index = 0;
			Index < UE_ARRAY_COUNT(DatabasePackages) && Index < NormalizationSet->Databases.Num();
			++Index)
		{
			const UPoseSearchDatabase* Database = NormalizationSet->Databases[Index];
			const FString ActualPackage = Database ? Database->GetOutermost()->GetName() : FString();
			TestEqual(
				*FString::Printf(TEXT("Normalization member %d keeps exact runtime order"), Index),
				ActualPackage,
				FString(DatabasePackages[Index]));
			if (!ActualPackage.IsEmpty())
			{
				UniqueNormalizationPackages.Add(ActualPackage);
			}
		}
		TestEqual(
			TEXT("The normalization set contains no duplicate databases"),
			UniqueNormalizationPackages.Num(),
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
	for (const TCHAR* DatabasePackage : ChooserExcludedRuntimeDatabasePackages)
	{
		TestFalse(
			*FString::Printf(TEXT("The archival chooser excludes runtime-only database %s"), DatabasePackage),
			AssetRegistry.ContainsDependency(
				FName(ChooserPackage),
				FName(DatabasePackage),
				UE::AssetRegistry::EDependencyCategory::Package));
	}
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgGaspRunKneePoseContractTest,
	"SurvivalRpg.Animation.Gasp.RunKneePoseContract",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgGaspRunKneePoseContractTest::RunTest(const FString& Parameters)
{
	using namespace RpgGaspLocomotionAssetTests;

	constexpr double SampleRate = 60.0;
	constexpr double NearStraightDegrees = 175.0;
	constexpr double MaximumAllowedDegrees = 177.0;
	struct FEvaluationContract
	{
		EAnimDataEvalType Type;
		const TCHAR* Name;
		double MaximumNearStraightFrameFraction;
	};
	static const FEvaluationContract EvaluationContracts[] = {
		{ EAnimDataEvalType::Raw, TEXT("Raw"), 0.0 },
		{ EAnimDataEvalType::Compressed, TEXT("Compressed"), 0.01 },
	};

	USkeletalMesh* TargetMesh = LoadObject<USkeletalMesh>(nullptr, TargetMeshPath);
	if (!TestNotNull(TEXT("The GASP knee-pose contract loads the project Manny mesh"), TargetMesh))
	{
		return false;
	}

	for (const TCHAR* AnimationPackage : KneeGuardRunAnimationPackages)
	{
		const FString ObjectPath = FString::Printf(
			TEXT("%s.%s"),
			AnimationPackage,
			*FPackageName::GetLongPackageAssetName(AnimationPackage));
		UAnimSequence* Animation = LoadObject<UAnimSequence>(nullptr, *ObjectPath);
		if (!TestNotNull(
			*FString::Printf(TEXT("The knee-guard animation loads: %s"), AnimationPackage),
			Animation))
		{
			continue;
		}

		for (const FEvaluationContract& Contract : EvaluationContracts)
		{
			FKneePoseMetrics Metrics;
			FString Failure;
			if (!TryMeasureKneePoseMetrics(
					Animation,
					TargetMesh,
					Contract.Type,
					SampleRate,
					NearStraightDegrees,
					Metrics,
					Failure))
			{
				AddError(FString::Printf(
					TEXT("%s %s knee evaluation failed: %s"),
					AnimationPackage,
					Contract.Name,
					*Failure));
				continue;
			}

			TestTrue(
				*FString::Printf(
					TEXT("%s %s stays below %.1f degrees (observed %.4f)"),
					AnimationPackage,
					Contract.Name,
					MaximumAllowedDegrees,
					Metrics.MaximumAngleDegrees),
				Metrics.MaximumAngleDegrees < MaximumAllowedDegrees);
			TestTrue(
				*FString::Printf(
					TEXT("%s %s keeps near-straight frames at or below %.1f%% (observed %.2f%%)"),
					AnimationPackage,
					Contract.Name,
					Contract.MaximumNearStraightFrameFraction * 100.0,
					Metrics.NearStraightFrameFraction * 100.0),
				Metrics.NearStraightFrameFraction <=
					Contract.MaximumNearStraightFrameFraction);
			AddInfo(FString::Printf(
				TEXT("%s %s: max knee %.4f degrees, near-straight %.2f%% across %d poses"),
				AnimationPackage,
				Contract.Name,
				Metrics.MaximumAngleDegrees,
				Metrics.NearStraightFrameFraction * 100.0,
				Metrics.SampleCount));
		}
	}

	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
