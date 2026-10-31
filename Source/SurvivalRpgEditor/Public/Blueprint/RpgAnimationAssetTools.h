#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RpgAnimationAssetTools.generated.h"

class UAnimBlueprint;
class UAnimGraphNode_LinkedAnimLayer;
class UAnimGraphNode_SaveCachedPose;
class UAnimGraphNode_UseCachedPose;
class UAnimInstance;
class UAnimLayerInterface;
class UAnimMontage;
class UAnimSequence;
class UAnimSequenceBase;
class UBlendSpace;
class UBlueprint;
class UChooserTable;
class UEdGraph;
class USkeleton;

/** Editor authoring gaps for animation assets. These operations contain no gameplay selection policy. */
UCLASS()
class SURVIVALRPGEDITOR_API URpgAnimationAssetTools final : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** True for a valid mounted package whose file resolves inside this project, including project plugins. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Animation|Editor")
	static bool IsProjectContentPackage(const FString& PackageName);

	/**
	 * Adds a new animation layer using the engine's animation graph schema. An optional named local-space
	 * input pose carries parameters described by [{"Name":"...","Type":{FEdGraphPinType fields}}].
	 * Existing names, inherited layer edits and invalid signatures are rejected. Does not compile or save.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Animation|Editor")
	static UEdGraph* AddAnimationLayer(UAnimBlueprint* Blueprint, FName LayerName, FName InputPoseName, const FString& InputsJson);

	/** Binds an existing linked-layer node through the editor's function-reference update path. The host must already implement the compiled interface; reconstructs pose/context pins. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Animation|Editor")
	static bool ConfigureLinkedAnimationLayer(UAnimGraphNode_LinkedAnimLayer* Node, TSubclassOf<UAnimLayerInterface> InterfaceClass,
		FName LayerName, TSubclassOf<UAnimInstance> InstanceClass);

	/** Creates a linked-layer editor node when the action menu has not yet indexed a newly created interface. No compile or save. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Animation|Editor")
	static UAnimGraphNode_LinkedAnimLayer* CreateLinkedAnimationLayerNode(UEdGraph* Graph, TSubclassOf<UAnimLayerInterface> InterfaceClass, FName LayerName);

	/**
	 * Adds a missing montage slot and records the skeleton change for save/undo. Existing slots are preserved.
	 * GroupName None uses the default group; another name creates that group if needed, so the slot's montages
	 * no longer stop montages of other groups. An existing slot in a different group is refused, not moved.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Animation|Editor")
	static bool RegisterAnimationSlot(USkeleton* Skeleton, FName SlotName, FName GroupName = NAME_None);

	/**
	 * Copies named blend profiles to another skeleton by bone name, keeping each profile's mode and per-bone
	 * scales. Every name must exist on the source and be absent on the target, and every profiled bone must
	 * exist on the target; otherwise nothing changes. Records undo and marks the target dirty; never saves.
	 * Returns the number of created profiles, or -1 when refused.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Animation|Editor")
	static int32 CopySkeletonBlendProfiles(USkeleton* SourceSkeleton, USkeleton* TargetSkeleton, const TArray<FName>& ProfileNames);

	/** Sets serialized metadata on an own function entry; this declares intent, not proof of thread safety. Compile afterwards. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Animation|Editor")
	static bool SetLocalFunctionThreadSafety(UBlueprint* Blueprint, FName FunctionName, bool bThreadSafe);

	/** Documents a locally declared Blueprint variable without changing its default, visibility or replication. Does not compile or save. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Animation|Editor")
	static bool SetVariableTooltip(UBlueprint* Blueprint, FName VariableName, const FString& Tooltip);

	/**
	 * Replaces all BlendSpace samples with sequences at finite axis coordinates, default rate 1/unmirrored.
	 * Validates a transient candidate and rebuilds interpolation data before committing. Axes/tuning remain
	 * designer-owned properties. Returns sample count or -1; no compile or save is implicit.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Animation|Editor")
	static int32 SetBlendSpaceSamples(UBlendSpace* BlendSpace, const TArray<UAnimSequence*>& Animations, const TArray<FVector>& Positions);

	/**
	 * Configures a root Chooser through reflected JSON. Allowed fields: OutputObjectType, ResultType,
	 * ContextData, ColumnsStructs, ResultsStructs, FallbackResult, DisabledRows. Instanced structs require
	 * _structType; column row arrays must match the result count. Unspecified fields are preserved.
	 * Preflights a transient candidate, checks struct families/bindings and compiles; records undo, never saves.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Animation|Editor")
	static bool ConfigureChooser(UChooserTable* Chooser, const FString& ConfigurationJson);

	/**
	 * Sets the editor-only disabled-row flags of a root Chooser asset or one of its embedded tables, named as in
	 * the Chooser editor. One flag per result row; rows, columns and results are otherwise unchanged. Disabled
	 * rows are skipped in editor evaluation and stripped when cooking. Recompiles the root; records undo, never saves.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Animation|Editor")
	static bool SetChooserDisabledRows(UChooserTable* RootChooser, FName NestedTableName, const TArray<bool>& DisabledRows);

	/**
	 * Replaces a project montage's first slot with one segment per sequence, laid end to end, and its sections with one
	 * section per segment start. Sections are not linked, so playing from one stops and blends out at its end, as
	 * directional variants in one montage need. Sequences must share the montage's skeleton. Records undo, never saves.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Animation|Editor")
	static bool SetMontageSectionSequences(UAnimMontage* Montage, const TArray<FName>& SectionNames, const TArray<UAnimSequenceBase*>& Sequences);

	/**
	 * Plays the time range StartTime..EndTime (seconds) of a project montage at PlayRate (0.05 to 1), for example a held
	 * wind-up before an attack window. The montage must still be one unscaled segment of one sequence, so montage and
	 * animation time agree. The segment is split around the range; notifies after it move later by the added time and
	 * notifies inside it stretch with it. Returns the new montage length in seconds, or a negative value when the montage
	 * or the range does not qualify. Records undo, never saves.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Animation|Editor")
	static float RetimeMontageRange(UAnimMontage* Montage, float StartTime, float EndTime, float PlayRate);

	/**
	 * Plays only the range StartTime..EndTime (seconds of the current montage time) of a project montage, from montage
	 * time zero, for example one cycle cut from a looping clip, or a montage cut shorter again. The montage must be one
	 * unscaled segment of one sequence with at most one section, which stays at zero; on an untrimmed montage the range is
	 * animation time. Notifies keep their animation frame and move earlier by StartTime; a notify outside the range
	 * refuses the trim. Returns the new montage length in seconds, or a negative value when the montage or the range does
	 * not qualify. Records undo, never saves.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Animation|Editor")
	static float TrimMontageToRange(UAnimMontage* Montage, float StartTime, float EndTime);

	/**
	 * Rotates every key of a project sequence's root bone track by YawDegrees about the vertical axis, so an in-place clip
	 * faces another direction relative to its owner; root translation keys turn with it. Other bones keep their local
	 * keys. Refused for sequences with root motion enabled. Records undo, never saves.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Animation|Editor")
	static bool RotateSequenceRoot(UAnimSequence* Sequence, float YawDegrees);

	/**
	 * Adds a Save Cached Pose node named CacheName to an AnimBP graph, as the editor's "New Save cached pose" action does.
	 * The name must be unique among the AnimBP's cached poses. Returns the node or null. Records undo; no compile or save.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Animation|Editor")
	static UAnimGraphNode_SaveCachedPose* CreateSaveCachedPoseNode(UEdGraph* Graph, const FString& CacheName);

	/**
	 * Adds a Use Cached Pose node reading the AnimBP's Save Cached Pose node named CacheName. The editor offers this
	 * action only after its action index refreshes. Returns the node or null. Records undo; no compile or save.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Animation|Editor")
	static UAnimGraphNode_UseCachedPose* CreateUseCachedPoseNode(UEdGraph* Graph, const FString& CacheName);
};
