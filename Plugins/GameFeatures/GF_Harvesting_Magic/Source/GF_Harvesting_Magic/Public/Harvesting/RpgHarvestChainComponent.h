#pragma once

#include "Components/BoxComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"

#include "RpgHarvestChainComponent.generated.h"

struct FRpgHarvestRequest;
struct FRpgHarvestTargetEvaluation;

/**
 * Box whose harvestable resources form one chain, such as a grove whose roots connect its trees in a portal realm.
 *
 * When a harvest depletes a resource inside the box, the same harvest takes the whole remaining stock of the box's
 * other resources, nearest to the depleted one first. The harvester receives their rewards and experience in the same
 * delivery. Partial harvests do not chain, and a chain does not continue into other boxes.
 *
 * Every harvest method chains alike: swings, area powers, strides and swarms. Chained resources follow the rules of an
 * area harvest: they need the harvest's tool, and resources inside a harvest protection box are skipped. Previews mark
 * every resource a harvest would chain.
 *
 * Add it to any actor and size it with the box extent. It has no collision and is hidden in game. Every machine
 * evaluates the box it loaded, so the server commit and client previews agree. Designer-placed static data without
 * runtime state.
 */
UCLASS(ClassGroup = (Rpg), meta = (BlueprintSpawnableComponent, DisplayName = "RPG Harvest Chain"))
class GF_HARVESTING_MAGIC_API URpgHarvestChainComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	explicit URpgHarvestChainComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Returns whether WorldLocation lies inside this box, respecting its rotation and scale. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting|Chain")
	bool ContainsLocation(const FVector& WorldLocation) const;

	/** Returns the most resources one harvest takes through this chain, clamped to 1 to 64. */
	int32 GetMaxChainedTargets() const { return FMath::Clamp(MaxChainedTargets, 1, 64); }

	/** Returns the presentation speed of the chain in cm/s; zero presents every chained resource at once. */
	float GetChainSpeed() const { return FMath::Max(0.0f, ChainSpeed); }

	/** Returns the collision channel that finds the box's resources. */
	ECollisionChannel GetResourceChannel() const { return ResourceChannel; }

	/** Configures the chain from code, for example in tests; designers set the properties on the component. */
	void ConfigureChain(int32 InMaxChainedTargets, float InChainSpeed);

protected:
	//~ UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent interface

	/** Most resources one harvest takes through this chain, 1 to 64; bounds the work of one harvest. Designer-tuned. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Harvesting|Chain", meta = (ClampMin = "1", ClampMax = "64", UIMin = "1", UIMax = "32"))
	int32 MaxChainedTargets = 12;

	/**
	 * Collision channel that finds the box's resources, like the trace channel of the harvest abilities. Harvestable
	 * meshes must block or overlap it. Designer data.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Harvesting|Chain")
	TEnumAsByte<ECollisionChannel> ResourceChannel = ECC_Visibility;

	/**
	 * Speed in cm/s at which the chain's presentation travels from the depleted resource to the others, so a grove
	 * falls tree by tree. Stock and rewards change at once, and every machine presents each resource at the same
	 * server time. Zero presents every chained resource at once. Cosmetic, designer-tuned; honored by instanced
	 * resources up to their longest presentation delay of 2.5 s.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Harvesting|Chain", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "3000.0", ForceUnits = "cm/s"))
	float ChainSpeed = 800.0f;
};

/**
 * Registry of the harvest chain boxes that have begun play in a world. Read by harvests on the server and by previews
 * on clients.
 */
UCLASS()
class GF_HARVESTING_MAGIC_API URpgHarvestChainSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Appends every registered chain box that contains WorldLocation. */
	void FindChainsAt(const FVector& WorldLocation, TArray<const URpgHarvestChainComponent*>& OutChains) const;

	/** Returns the number of registered chain boxes. */
	int32 GetNumChains() const { return Chains.Num(); }

private:
	friend class URpgHarvestChainComponent;

	void RegisterChain(const URpgHarvestChainComponent& Chain);
	void UnregisterChain(const URpgHarvestChainComponent& Chain);

	TArray<TWeakObjectPtr<const URpgHarvestChainComponent>> Chains;
};

/**
 * Stateless chain rules shared by harvest abilities, strides, swarms and their previews. A target triggers a chain when
 * its harvest depletes it inside a chain box; one harvest runs each box at most once.
 */
struct GF_HARVESTING_MAGIC_API FRpgHarvestChains
{
	/**
	 * Preview: for every target of InOutTargets that RequestTemplate would deplete inside a chain box, marks the box's
	 * other resources as chained. Resources already listed take their whole remaining stock; the others are appended
	 * with bChained set. Evaluates without mutating anything; valid on the server and on clients.
	 */
	static void AppendPreview(
		const UWorld& World,
		const FRpgHarvestRequest& RequestTemplate,
		TArray<FRpgHarvestTargetEvaluation>& InOutTargets);

	/**
	 * Server: when the committed result of Trigger depleted it inside chain boxes that InOutChainedBoxes does not hold
	 * yet, commits the whole remaining stock of each box's other resources for TriggerRequest's harvester. Each chained
	 * commit presents after TriggerRequest's presentation delay plus its distance at the box's chain speed. Appends the
	 * chained results with bChained set to OutChained and adds the boxes to InOutChainedBoxes. Call it after the
	 * trigger's commit, while the harvest's reward batch is open. Returns the stock sections the chain took.
	 */
	static int32 Commit(
		UWorld& World,
		const FRpgHarvestRequest& TriggerRequest,
		const FRpgHarvestTargetEvaluation& Trigger,
		TSet<TObjectKey<URpgHarvestChainComponent>>& InOutChainedBoxes,
		TArray<FRpgHarvestTargetEvaluation>& OutChained);

	/** Returns the world location a chain measures Target from: its instance's location, or its actor's location. */
	static FVector GetTargetLocation(const FRpgHarvestTargetEvaluation& Target);
};
