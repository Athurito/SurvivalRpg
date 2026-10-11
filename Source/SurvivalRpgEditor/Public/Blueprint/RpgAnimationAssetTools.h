#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RpgAnimationAssetTools.generated.h"

class UAnimBlueprint;
class UAnimGraphNode_LinkedAnimLayer;
class UAnimInstance;
class UAnimLayerInterface;
class UAnimSequence;
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

	/** Adds a missing montage slot in the default group and records the skeleton change for save/undo. Existing slots are preserved. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Animation|Editor")
	static bool RegisterAnimationSlot(USkeleton* Skeleton, FName SlotName);

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
};
