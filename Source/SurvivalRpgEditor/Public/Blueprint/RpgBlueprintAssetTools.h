#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RpgBlueprintAssetTools.generated.h"

class UBlueprint;
class UAnimSequenceBase;
class UActorComponent;
class UEdGraph;
class UEdGraphNode;
class UEdGraphNode_Comment;

/** Reusable editor operations for authoring Blueprint contracts through Unreal MCP/Python. */
UCLASS()
class SURVIVALRPGEDITOR_API URpgBlueprintAssetTools final : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Adds an interface with Unreal's native graph setup; idempotent for inherited or existing interfaces. Compile and save the Blueprint afterwards. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Blueprint|Editor")
	static bool ImplementInterface(UBlueprint* Blueprint, TSubclassOf<UInterface> InterfaceClass);

	/**
	 * Adds the standard EventGraph to a Blueprint that has none, for example a Widget Blueprint authored without graphs.
	 * Engine tools can add events only to an existing EventGraph. Idempotent: an existing EventGraph returns true unchanged.
	 * Interfaces, macro and function libraries are rejected. Records undo; compile and save the Blueprint afterwards.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Blueprint|Editor")
	static bool EnsureEventGraph(UBlueprint* Blueprint);

	/**
	 * Changes an existing component declared in this Blueprint's own SCS to an instantiable subclass.
	 * Keeps its node, variable name/GUID, attachments and graph links; duplicates template defaults and
	 * owned UObject subobjects, then remaps references within this Blueprint and its generated classes.
	 * Native/inherited components and templates with nested ActorComponents are unsupported. The engine's
	 * ChangeSubobjectClass covers native components only; this operation supplies the SCS authoring gap.
	 * Runs on the editor game thread outside PIE, records undo, regenerates the skeleton and refreshes graph nodes.
	 * An already matching class still refreshes nodes, allowing recovery from a previous incomplete authoring step.
	 * Normal Blueprint reconstruction can refresh loaded instances/derived Blueprints; use an isolated working asset.
	 * Does not save or run a full Blueprint compile. Compile and validate the Blueprint afterwards.
	 * Returns true after replacement/refresh; false leaves the SCS unchanged.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Blueprint|Editor")
	static bool ChangeOwnSCSComponentClass(UBlueprint* Blueprint, FName ComponentVariableName, TSubclassOf<UActorComponent> NewComponentClass);

	/**
	 * Sets the parent socket of a scene component declared in this Blueprint's own SCS, such as an arrow that rides a
	 * bowstring bone; None attaches to the parent itself. Its relative transform then counts from that socket. The socket
	 * is not validated against the parent's mesh. Records undo; compile and save the Blueprint afterwards. False for a
	 * missing, inherited or root component.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Blueprint|Editor")
	static bool SetOwnSCSComponentParentSocket(UBlueprint* Blueprint, FName ComponentVariableName, FName SocketName);

	/**
	 * Remaps serialized object references only in this asset and its nested Outer-owned objects.
	 * Uses Unreal's replacement archive, including protected properties and instanced struct data.
	 * Hard references and resolvable weak/soft references are supported; unresolved soft paths and
	 * arbitrary string paths require separate validation. Outer/archetype references are preserved.
	 * The caller supplies non-null, type-compatible replacements without mapping chains or cycles.
	 * Package roots and package replacements are rejected. The archive does not traverse external objects
	 * or package-sibling Blueprint generated classes/CDOs. Normal PostEditChange notifications still run
	 * and may refresh loaded referencers; callers must restrict those referencers to their authorized assets.
	 * Makes owned objects transactional, records undo
	 * when the editor transactor is available, and marks changed content dirty. Does not compile, save,
	 * rename, or consolidate assets. Compile Blueprints afterwards.
	 * Returns the archive's replacement count, zero for no changes, or -1 for invalid arguments.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Blueprint|Editor")
	static int32 RemapOwnedObjectReferences(UObject* Asset, const TMap<UObject*, UObject*>& Replacements);

	/**
	 * Replaces target notifies with exact event metadata from an animation with the same class, skeleton and timeline.
	 * Montage slots, segment timing and sequence references must match. Notify objects are duplicated into the target;
	 * event GUIDs, cached linkage, out-of-range state end times and shared animation references are preserved.
	 * Runs only on the editor game thread outside PIE, records target undo and marks it dirty. Does not compile or save.
	 * Returns the copied event count, or -1 for incompatible assets or an unavailable editor context.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Animation|Editor")
	static int32 CopyAnimationNotifies(UAnimSequenceBase* Source, UAnimSequenceBase* Target);

	/**
	 * Replaces animation-owned notify instances with explicitly mapped, serialization-compatible Blueprint classes.
	 * TypeReplacements permits copied Blueprint parents, enums and script structs only after validating their layouts.
	 * Direct hard object properties may change their class constraint only with an explicit type mapping and a
	 * compatible ObjectReplacements entry for every non-null instance value; no class-default fallback is used.
	 * Referenced assets are not copied or edited. Their class ancestry is checked, not their in-memory field layout.
	 * Preserves complete event records (including GUIDs and cached timing), instance values and shared notify identities.
	 * Runs on the editor game thread outside PIE; records undo and dirties only the project animation. Does not save.
	 * Returns the number of distinct replaced instances, zero for no changes, or -1 for an incompatible contract.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Animation|Editor")
	static int32 RemapAnimationNotifyClasses(UAnimSequenceBase* Animation, const TMap<UClass*, UClass*>& Replacements,
		const TMap<UObject*, UObject*>& TypeReplacements, const TMap<UObject*, UObject*>& ObjectReplacements);

	/**
	 * Documents a function declared in this Blueprint: the tooltip shows on its call nodes and in My Blueprint. bPure
	 * makes callers show it without exec pins; set it before writing callers, because refreshed call nodes of a function
	 * that turns pure lose their exec links. Records undo; compile and save afterwards. False for an inherited or missing
	 * function, or an empty tooltip.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Blueprint|Editor")
	static bool DescribeLocalFunction(UBlueprint* Blueprint, FName FunctionName, const FString& Tooltip, bool bPure);

	/**
	 * Adds a comment box behind Nodes, which must all belong to Graph. The box is sized from the nodes' positions and an
	 * estimate of each node's size from its title and pins, because node sizes exist only in an open graph editor; check
	 * the picture there. Records undo; save afterwards. Returns the comment node, or null for no nodes, nodes of another
	 * graph or an empty text.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Blueprint|Editor")
	static UEdGraphNode_Comment* AddGraphComment(UEdGraph* Graph, const TArray<UEdGraphNode*>& Nodes, const FString& Text);

	/**
	 * Removes every comment box of a Blueprint graph, so they can be added again after its nodes moved; the engine's node
	 * deletion accepts no comments. Records undo; save afterwards. Returns the number removed, or -1 for no graph.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Blueprint|Editor")
	static int32 RemoveGraphComments(UEdGraph* Graph);
};
