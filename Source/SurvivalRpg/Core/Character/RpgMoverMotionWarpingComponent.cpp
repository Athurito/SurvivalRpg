#include "RpgMoverMotionWarpingComponent.h"

#include "RpgCharacterMoverComponent.h"
#include "AnimNotifyState_MotionWarping.h"
#include "Components/SkeletalMeshComponent.h"
#include "MoverDataModelTypes.h"
#include "RootMotionModifier_SkewWarp.h"

void URpgMoverMotionWarpingAdapter::SetMover(URpgCharacterMoverComponent* InMover) { Mover = InMover; }
void URpgMoverMotionWarpingAdapter::SetSimulationTransform(const FTransform& ActorTransform, const FTransform& BaseVisualTransform)
{
	SimulationTransform = ActorTransform;
	SimulationBaseVisual = BaseVisualTransform;
	bInSimulation = true;
}
void URpgMoverMotionWarpingAdapter::ClearSimulationTransform() { bInSimulation = false; }
AActor* URpgMoverMotionWarpingAdapter::GetActor() const { return Mover ? Mover->GetOwner() : nullptr; }
USkeletalMeshComponent* URpgMoverMotionWarpingAdapter::GetMesh() const
{
	return Mover ? Cast<USkeletalMeshComponent>(Mover->GetPrimaryVisualComponent()) : nullptr;
}
FTransform URpgMoverMotionWarpingAdapter::GetCurrentTransform() const
{
	if (bInSimulation) { return SimulationTransform; }
	if (Mover)
	{
		if (const FMoverDefaultSyncState* State = Mover->GetSyncState().SyncStateCollection.FindDataByType<FMoverDefaultSyncState>())
		{
			return FTransform(State->GetOrientation_WorldSpace(), State->GetLocation_WorldSpace());
		}
		return Mover->GetUpdatedComponentTransform();
	}
	return FTransform::Identity;
}
FTransform URpgMoverMotionWarpingAdapter::GetBaseVisualTransform() const
{
	return bInSimulation ? SimulationBaseVisual : (Mover ? Mover->GetBaseVisualComponentTransform() : FTransform::Identity);
}
FVector URpgMoverMotionWarpingAdapter::GetVisualRootLocation() const
{
	return (GetBaseVisualTransform() * GetCurrentTransform()).GetLocation();
}
FVector URpgMoverMotionWarpingAdapter::GetBaseVisualTranslationOffset() const { return GetBaseVisualTransform().GetTranslation(); }
FQuat URpgMoverMotionWarpingAdapter::GetBaseVisualRotationOffset() const { return GetBaseVisualTransform().GetRotation(); }

bool URpgMoverMotionWarpingComponent::SupportsTraversal(const FRpgMoverTraversalRequest& Request) const
{
	// Moving targets, Blueprint modifier callbacks and switch-off logic have additional mutable/world state.
	// Grounded Mantle/Vault use audited fixed front targets and an optional translation-only rear target.
	if (!Request.Montage || Request.WarpTargetName.IsNone() || bSearchForWindowsInAnimsWithinMontages ||
		Request.FrontLedgeTarget.ContainsNaN() || Request.BackLedgeWarpTargetName == Request.WarpTargetName ||
		(!Request.BackLedgeWarpTargetName.IsNone() && Request.BackLedgeTarget.ContainsNaN()) ||
		!SwitchOffConditions.IsEmpty() || OnPreUpdate.IsBound() || IsPredictingTrajectory()) { return false; }
	TArray<FMotionWarpingWindowData> Windows;
	UMotionWarpingUtilities::GetMotionWarpingWindowsFromAnimation(Request.Montage, Windows);
	if (Windows.IsEmpty() || Windows.Num() > 16) { return false; }
	bool bHasFrontWindow = false;
	bool bHasBackWindow = false;
	for (const FMotionWarpingWindowData& Window : Windows)
	{
		const UAnimNotifyState_MotionWarping* Notify = Window.AnimNotify;
		const URootMotionModifier_SkewWarp* Modifier = Notify ? Cast<URootMotionModifier_SkewWarp>(Notify->RootMotionModifier) : nullptr;
		if (!Notify || Notify->GetClass() != UAnimNotifyState_MotionWarping::StaticClass() || !Modifier ||
			Modifier->GetClass() != URootMotionModifier_SkewWarp::StaticClass() ||
			Modifier->RotationType != EMotionWarpRotationType::Default || Window.EndTime <= Window.StartTime) { return false; }
		if (Modifier->WarpTargetName == Request.WarpTargetName) { bHasFrontWindow = true; }
		else if (!Request.BackLedgeWarpTargetName.IsNone() && Modifier->WarpTargetName == Request.BackLedgeWarpTargetName)
		{
			// The approved rear-edge warp keeps authored rotation and has no animated hand/bone offset.
			if (!Modifier->bWarpTranslation || Modifier->bIgnoreZAxis || !Modifier->bWarpToFeetLocation ||
				Modifier->bWarpRotation || Modifier->WarpPointAnimProvider != EWarpPointAnimProvider::None ||
				Modifier->bSubtractRemainingRootMotion) { return false; }
			bHasBackWindow = true;
		}
		else { return false; }
	}
	return bHasFrontWindow && (bHasBackWindow == !Request.BackLedgeWarpTargetName.IsNone());
}

const UMotionWarpingBaseAdapter* URpgMoverMotionWarpingComponent::GetTraversalAdapter(URpgCharacterMoverComponent* Mover)
{
	if (!SimulationAdapter) { SimulationAdapter = NewObject<URpgMoverMotionWarpingAdapter>(this); }
	SimulationAdapter->SetMover(Mover);
	return SimulationAdapter;
}

bool URpgMoverMotionWarpingComponent::BeginSimulationWarp(URpgCharacterMoverComponent* Mover,
	FRpgMoverTraversalSyncState& State, const FTransform& ActorTransform)
{
	if (!ensure(!SimulationState) || !Mover || !State.Command.IsActive() || !SupportsTraversal(State.Command.Context)) { return false; }
	GetTraversalAdapter(Mover);
	SimulationAdapter->SetSimulationTransform(ActorTransform, State.Command.BaseVisualTransform);
	SimulationMover = Mover;
	SimulationState = &State;
	SavedOwnerAdapter = OwnerAdapter;
	OwnerAdapter = SimulationAdapter;
	Swap(Modifiers, SavedModifiers);
	Swap(WarpTargets, SavedTargets);
	WarpTargets.Add(FMotionWarpingTarget(State.Command.Context.WarpTargetName, State.Command.Context.FrontLedgeTarget));
	if (!State.Command.Context.BackLedgeWarpTargetName.IsNone())
	{
		WarpTargets.Add(FMotionWarpingTarget(State.Command.Context.BackLedgeWarpTargetName, State.Command.Context.BackLedgeTarget));
	}
	UMotionWarpingUtilities::GetMotionWarpingWindowsFromAnimation(State.Command.Context.Montage, SimulationWindows);
	for (FMotionWarpingWindowData& Window : SimulationWindows)
	{
		// Match UMotionWarpingComponent::UpdateWithContext's normalization of notify trigger offsets.
		Window.StartTime = FMath::Clamp(Window.StartTime, 0.f, State.Command.Context.Montage->GetPlayLength());
		Window.EndTime = FMath::Clamp(Window.EndTime, 0.f, State.Command.Context.Montage->GetPlayLength());
	}
	if (CachedMontage != State.Command.Context.Montage)
	{
		CachedMontage = State.Command.Context.Montage;
		SimulationModifierCache.Reset();
		SimulationModifierCache.SetNum(SimulationWindows.Num());
	}
	for (const FRpgMoverWarpModifierState& Snapshot : State.WarpModifiers)
	{
		if (!SimulationWindows.IsValidIndex(Snapshot.WindowIndex)) { continue; }
		const FMotionWarpingWindowData& Window = SimulationWindows[Snapshot.WindowIndex];
		URootMotionModifier* Modifier = SimulationModifierCache[Snapshot.WindowIndex];
		if (!Modifier)
		{
			Modifier = AddModifierFromTemplate(Window.AnimNotify->RootMotionModifier, CachedMontage, Window.StartTime, Window.EndTime);
			SimulationModifierCache[Snapshot.WindowIndex] = Modifier;
		}
		else { Modifiers.Add(Modifier); }
		Modifier->RestoreFromSwapState(MakeShared<FRootMotionModifier_WarpSwapState>(Snapshot.Value));
	}
	SavedLocalDelegate = Mover->ProcessLocalRootMotionDelegate;
	SavedWorldDelegate = Mover->ProcessWorldRootMotionDelegate;
	Mover->ProcessLocalRootMotionDelegate.BindUObject(this, &ThisClass::ProcessSimulationRootMotion);
	Mover->ProcessWorldRootMotionDelegate.BindUObject(this, &ThisClass::ConvertSimulationRootMotion);
	return true;
}

FTransform URpgMoverMotionWarpingComponent::ProcessSimulationRootMotion(const FTransform& LocalRootMotion,
	float DeltaSeconds, const FMotionWarpingUpdateContext* Context)
{
	check(SimulationState && Context);
	FMotionWarpingUpdateContext SimulationContext = *Context;
	// UE 5.8's game-thread montage move supplies positions/rate but leaves this context member zero.
	SimulationContext.DeltaSeconds = DeltaSeconds;
	WarpedLocalRootMotion = Super::ProcessRootMotionPreConvertToWorld(LocalRootMotion, DeltaSeconds, &SimulationContext);
	return WarpedLocalRootMotion;
}

FTransform URpgMoverMotionWarpingComponent::ConvertSimulationRootMotion(const FTransform& WorldRootMotion,
	float DeltaSeconds, const FMotionWarpingUpdateContext* Context)
{
	check(SimulationState);
	// Same transform composition as Mover's ConvertLocalRootMotionToAltWorldSpace. The fixed base offset
	// replaces the live mesh transform, which can contain presentation smoothing from a different NP frame.
	const FTransform Actor = SimulationAdapter->GetCurrentTransform();
	const FTransform Base = SimulationState->Command.BaseVisualTransform;
	const FTransform NewActor = Base.Inverse() * (WarpedLocalRootMotion * (Base * Actor));
	const FTransform Delta = NewActor.GetRelativeTransform(Actor);
	const FTransform Result(Delta.GetRotation(), NewActor.GetTranslation() - Actor.GetTranslation());
	return SavedWorldDelegate.IsBound() ? SavedWorldDelegate.Execute(Result, DeltaSeconds, Context) : Result;
}

void URpgMoverMotionWarpingComponent::EndSimulationWarp()
{
	if (!SimulationState) { return; }
	SimulationState->WarpModifiers.Reset();
	for (URootMotionModifier* Modifier : Modifiers)
	{
		if (!Modifier) { continue; }
		const int32 WindowIndex = SimulationWindows.IndexOfByPredicate([Modifier](const FMotionWarpingWindowData& Window)
		{
			const URootMotionModifier_Warp* Warp = Cast<URootMotionModifier_Warp>(Modifier);
			const URootMotionModifier_Warp* Template = Window.AnimNotify ? Cast<URootMotionModifier_Warp>(Window.AnimNotify->RootMotionModifier) : nullptr;
			return Warp && Template && Warp->WarpTargetName == Template->WarpTargetName &&
				FMath::IsNearlyEqual(Window.StartTime, Modifier->StartTime) && FMath::IsNearlyEqual(Window.EndTime, Modifier->EndTime);
		});
		if (!SimulationWindows.IsValidIndex(WindowIndex)) { continue; }
		FRpgMoverWarpModifierState& Snapshot = SimulationState->WarpModifiers.AddDefaulted_GetRef();
		Snapshot.WindowIndex = WindowIndex;
		Snapshot.Value = *StaticCastSharedPtr<FRootMotionModifier_WarpSwapState>(Modifier->GetSwapState());
		SimulationModifierCache[WindowIndex] = Modifier;
	}
	if (URpgCharacterMoverComponent* Mover = SimulationMover.Get())
	{
		Mover->ProcessLocalRootMotionDelegate = SavedLocalDelegate;
		Mover->ProcessWorldRootMotionDelegate = SavedWorldDelegate;
	}
	Modifiers.Reset();
	WarpTargets.Reset();
	Swap(Modifiers, SavedModifiers);
	Swap(WarpTargets, SavedTargets);
	OwnerAdapter = SavedOwnerAdapter;
	SavedOwnerAdapter = nullptr;
	SimulationAdapter->ClearSimulationTransform();
	SimulationState = nullptr;
	SimulationMover.Reset();
}
