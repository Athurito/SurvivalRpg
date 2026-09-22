#include "RpgMoverTraversalTypes.h"
#include "Animation/BlendProfile.h"
#include "Curves/CurveFloat.h"

/** A frame holds its own node strongly. It cannot keep arbitrarily old predecessor frames alive. */
struct FRpgMoverTraversalLineage
{
	FRpgMoverTraversalIdentity PreviousIdentity;
	FRpgMoverTraversalIdentity ObservedSyncIdentity;
	TWeakPtr<const FRpgMoverTraversalLineage> Previous;
};

void FRpgMoverTraversalRequest::Serialize(FArchive& Ar)
{
	Ar << Collider << ColliderTransform << EntryCapsuleLocation << LandingCapsuleLocation << FrontLedgeTarget;
	Ar << Montage << StartTimeSeconds << PlayRate << HandoffTimeSeconds << WarpTargetName << BackLedgeWarpTargetName << BackFloorWarpTargetName;
	if (!BackLedgeWarpTargetName.IsNone()) { Ar << BackLedgeTarget; }
	else if (Ar.IsLoading()) { BackLedgeTarget = FTransform::Identity; }
	if (!BackFloorWarpTargetName.IsNone()) { Ar << BackFloorTarget << LandingSupport << LandingSupportTransform; }
	else if (Ar.IsLoading())
	{
		BackFloorTarget = FTransform::Identity;
		LandingSupport.Reset();
		LandingSupportTransform = FTransform::Identity;
	}
}

bool FRpgMoverTraversalRequest::Equals(const FRpgMoverTraversalRequest& B) const
{
	return Collider == B.Collider && ColliderTransform.Equals(B.ColliderTransform, .01f) &&
		EntryCapsuleLocation.Equals(B.EntryCapsuleLocation, .01f) && LandingCapsuleLocation.Equals(B.LandingCapsuleLocation, .01f) &&
		FrontLedgeTarget.Equals(B.FrontLedgeTarget, .01f) && Montage == B.Montage &&
		FMath::IsNearlyEqual(StartTimeSeconds, B.StartTimeSeconds) && FMath::IsNearlyEqual(PlayRate, B.PlayRate) &&
		FMath::IsNearlyEqual(HandoffTimeSeconds, B.HandoffTimeSeconds) && WarpTargetName == B.WarpTargetName &&
		BackLedgeWarpTargetName == B.BackLedgeWarpTargetName &&
		(BackLedgeWarpTargetName.IsNone() || BackLedgeTarget.Equals(B.BackLedgeTarget, .01f)) &&
		BackFloorWarpTargetName == B.BackFloorWarpTargetName &&
		(BackFloorWarpTargetName.IsNone() || (BackFloorTarget.Equals(B.BackFloorTarget, .01f) &&
			LandingSupport == B.LandingSupport && LandingSupportTransform.Equals(B.LandingSupportTransform, .01f)));
}

bool FRpgMoverTraversalIdentity::operator==(const FRpgMoverTraversalIdentity& B) const
{
	return AbilityHandle == B.AbilityHandle && ActivationPredictionKey == B.ActivationPredictionKey &&
		bServerInitiatedKey == B.bServerInitiatedKey && MontageSequence == B.MontageSequence;
}

void FRpgMoverTraversalIdentity::Serialize(FArchive& Ar)
{
	Ar << AbilityHandle << ActivationPredictionKey << bServerInitiatedKey << MontageSequence;
}

void FRpgMoverTraversalCommand::Serialize(FArchive& Ar)
{
	if (Ar.IsLoading()) { LocalLineage.Reset(); }
	Ar << Phase;
	if (Phase == ERpgMoverTraversalPhase::None)
	{
		if (Ar.IsLoading()) { *this = FRpgMoverTraversalCommand{}; }
		return;
	}
	Identity.Serialize(Ar);
	Context.Serialize(Ar);
	Ar << BaseVisualTransform << bPreserveMomentum << bHasRecoveryLocation << RecoveryCapsuleLocation;
	SerializePresentation(Ar);
	if (Ar.IsLoading()) { RetainObjectsForHistory(); }
}

void FRpgMoverTraversalCommand::SerializePresentation(FArchive& Ar)
{
	Ar << bHasPresentationPlayId;
	if (bHasPresentationPlayId) Ar << PresentationPlayId;
	bool bHasEnd = PresentationEndPosition >= 0.f && FMath::IsFinite(PresentationEndPosition);
	Ar << bHasEnd;
	if (bHasEnd)
	{
		Ar << PresentationEndPosition << PresentationEndBlend.Blend.BlendTime << PresentationEndBlend.Blend.BlendOption;
		Ar << PresentationEndBlend.Blend.CustomCurve << PresentationEndBlend.BlendProfile;
	}
	else if (Ar.IsLoading())
	{
		PresentationEndPosition = -1.f;
		PresentationEndBlend = FMontageBlendSettings{};
	}
}

void FRpgMoverTraversalCommand::RetainObjectsForHistory()
{
	MontageLifetime.Reset(Context.Montage.Get());
	PresentationProfileLifetime.Reset(PresentationEndBlend.BlendProfile.Get());
	PresentationCurveLifetime.Reset(PresentationEndBlend.Blend.CustomCurve.Get());
}

void FRpgMoverTraversalCommand::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Context.Montage);
	Collector.AddReferencedObject(PresentationEndBlend.BlendProfile);
	Collector.AddReferencedObject(PresentationEndBlend.Blend.CustomCurve);
}

bool FRpgMoverTraversalCommand::Equals(const FRpgMoverTraversalCommand& B) const
{
	// Cosmetic receipt correlation and stop blending do not change predicted movement or command identity.
	return Identity == B.Identity && Phase == B.Phase && Context.Equals(B.Context) &&
		BaseVisualTransform.Equals(B.BaseVisualTransform, .001f) && bPreserveMomentum == B.bPreserveMomentum &&
		bHasRecoveryLocation == B.bHasRecoveryLocation && RecoveryCapsuleLocation.Equals(B.RecoveryCapsuleLocation, .01f);
}

void FRpgMoverTraversalCommand::RecordPredecessors(const FRpgMoverTraversalCommand& Previous,
	const FRpgMoverTraversalIdentity& ObservedSyncIdentity)
{
	TSharedPtr<FRpgMoverTraversalLineage> Lineage = MakeShared<FRpgMoverTraversalLineage>();
	Lineage->PreviousIdentity = Previous.Identity;
	Lineage->ObservedSyncIdentity = ObservedSyncIdentity;
	Lineage->Previous = Previous.LocalLineage;
	LocalLineage = Lineage;
}

bool FRpgMoverTraversalCommand::IsSuccessorOf(const FRpgMoverTraversalIdentity& Other) const
{
	for (TSharedPtr<const FRpgMoverTraversalLineage> Node = LocalLineage; Node; Node = Node->Previous.Pin())
	{
		if (Node->PreviousIdentity == Other || Node->ObservedSyncIdentity == Other) { return true; }
	}
	return false;
}

void FRpgMoverTraversalCommand::CompactAppliedEnd()
{
	const FName TargetName = Context.WarpTargetName;
	const FName BackTargetName = Context.BackLedgeWarpTargetName;
	const FName FloorTargetName = Context.BackFloorWarpTargetName;
	Context = FRpgMoverTraversalRequest{};
	Context.WarpTargetName = TargetName;
	Context.BackLedgeWarpTargetName = BackTargetName;
	Context.BackFloorWarpTargetName = FloorTargetName;
	BaseVisualTransform = FTransform::Identity;
	bPreserveMomentum = false;
	bHasRecoveryLocation = false;
	RecoveryCapsuleLocation = FVector::ZeroVector;
	RetainObjectsForHistory();
}

void FRpgMoverWarpModifierState::Serialize(FArchive& Ar)
{
	Ar << WindowIndex << Value.StartTime << Value.EndTime << Value.PreviousPosition << Value.CurrentPosition;
	Ar << Value.Weight << Value.PlayRate << Value.StartTransform << Value.ActualStartTime << Value.TotalRootMotionWithinWindow << Value.State;
	Ar << Value.AdditionalRotationOffset << Value.CachedTargetTransform << Value.RootMotionRemainingAfterNotify;
	bool bHasOffset = Value.CachedOffsetFromWarpPoint.IsSet();
	Ar << bHasOffset;
	if (bHasOffset)
	{
		FTransform Offset = Value.CachedOffsetFromWarpPoint.Get(FTransform::Identity);
		Ar << Offset;
		if (Ar.IsLoading()) { Value.CachedOffsetFromWarpPoint = Offset; }
	}
	else if (Ar.IsLoading()) { Value.CachedOffsetFromWarpPoint.Reset(); }
	Ar << Value.RotationOffset << Value.bWarpingPaused << Value.bRootMotionPaused;
}

bool FRpgMoverWarpModifierState::Equals(const FRpgMoverWarpModifierState& B) const
{
	const auto& A = Value;
	const auto& V = B.Value;
	return WindowIndex == B.WindowIndex && A.State == V.State && FMath::IsNearlyEqual(A.StartTime, V.StartTime) &&
		FMath::IsNearlyEqual(A.EndTime, V.EndTime) && FMath::IsNearlyEqual(A.PreviousPosition, V.PreviousPosition) &&
		FMath::IsNearlyEqual(A.CurrentPosition, V.CurrentPosition) && FMath::IsNearlyEqual(A.Weight, V.Weight) &&
		FMath::IsNearlyEqual(A.PlayRate, V.PlayRate) && A.StartTransform.Equals(V.StartTransform, .01f) &&
		FMath::IsNearlyEqual(A.ActualStartTime, V.ActualStartTime) && A.TotalRootMotionWithinWindow.Equals(V.TotalRootMotionWithinWindow, .001f) &&
		A.AdditionalRotationOffset.Equals(V.AdditionalRotationOffset, .001f) && A.CachedTargetTransform.Equals(V.CachedTargetTransform, .01f) &&
		A.RootMotionRemainingAfterNotify.Equals(V.RootMotionRemainingAfterNotify, .001f) &&
		A.CachedOffsetFromWarpPoint.IsSet() == V.CachedOffsetFromWarpPoint.IsSet() &&
		(!A.CachedOffsetFromWarpPoint.IsSet() || A.CachedOffsetFromWarpPoint.GetValue().Equals(V.CachedOffsetFromWarpPoint.GetValue(), .001f)) &&
		A.RotationOffset.Equals(V.RotationOffset, .001f) && A.bWarpingPaused == V.bWarpingPaused && A.bRootMotionPaused == V.bRootMotionPaused;
}

FMoverDataStructBase* FRpgMoverTraversalSyncState::Clone() const { return new FRpgMoverTraversalSyncState(*this); }
UScriptStruct* FRpgMoverTraversalSyncState::GetScriptStruct() const { return StaticStruct(); }

bool FRpgMoverTraversalSyncState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bool bCompactTerminal = bEndApplied && Command.IsTerminal();
	Ar << bCompactTerminal;
	if (bCompactTerminal)
	{
		if (Ar.IsLoading()) { *this = FRpgMoverTraversalSyncState{}; }
		Ar << Command.Phase;
		Command.Identity.Serialize(Ar);
		Ar << Command.Context.WarpTargetName << Command.Context.BackLedgeWarpTargetName << Command.Context.BackFloorWarpTargetName;
		Command.SerializePresentation(Ar);
		if (Ar.IsLoading()) Command.RetainObjectsForHistory();
		bEndApplied = true;
		bOutSuccess = !Ar.IsError();
		return bOutSuccess;
	}
	Command.Serialize(Ar);
	if (Command.Phase == ERpgMoverTraversalPhase::None)
	{
		if (Ar.IsLoading()) { *this = FRpgMoverTraversalSyncState{}; }
		bOutSuccess = !Ar.IsError();
		return bOutSuccess;
	}
	Ar << MontagePosition << bStartApplied << bEndApplied;
	uint8 Count = static_cast<uint8>(WarpModifiers.Num());
	Ar << Count;
	if (Count > 16) { Ar.SetError(); bOutSuccess = false; return false; }
	if (Ar.IsLoading()) { WarpModifiers.SetNum(Count); }
	for (FRpgMoverWarpModifierState& Modifier : WarpModifiers) { Modifier.Serialize(Ar); }
	bOutSuccess = !Ar.IsError();
	return bOutSuccess;
}

void FRpgMoverTraversalSyncState::AddReferencedObjects(FReferenceCollector& Collector) { Command.AddReferencedObjects(Collector); }

bool FRpgMoverTraversalSyncState::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	const auto& B = static_cast<const FRpgMoverTraversalSyncState&>(AuthorityState);
	if (!Command.Equals(B.Command) || !FMath::IsNearlyEqual(MontagePosition, B.MontagePosition, .001f) ||
		bStartApplied != B.bStartApplied || bEndApplied != B.bEndApplied || WarpModifiers.Num() != B.WarpModifiers.Num()) { return true; }
	for (int32 Index = 0; Index < WarpModifiers.Num(); ++Index)
	{
		if (!WarpModifiers[Index].Equals(B.WarpModifiers[Index])) { return true; }
	}
	return false;
}

void FRpgMoverTraversalSyncState::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	// Identity, collision and modifier activation are discrete. Interpolating cached warp starts creates a new trajectory.
	*this = static_cast<const FRpgMoverTraversalSyncState&>(Pct < 1.f ? From : To);
	const auto& FromState = static_cast<const FRpgMoverTraversalSyncState&>(From);
	const auto& ToState = static_cast<const FRpgMoverTraversalSyncState&>(To);
	if (!FromState.Command.IsActive() && ToState.Command.IsActive() && Pct > 0.f)
	{
		// Mover already presents To's movement mode and layered moves across this interval, including
		// gap-filled network frames. Start its pose with that motion instead of waiting for the endpoint.
		// Adopt discrete targets and warp caches intact; only the authored montage phase is interpolated.
		*this = ToState;
		MontagePosition = FMath::Lerp(ToState.Command.Context.StartTimeSeconds, ToState.MontagePosition,
			FMath::Clamp(Pct, 0.f, 1.f));
	}
	else if (FromState.Command.IsActive() && ToState.Command.IsActive()
		&& FromState.Command.Identity == ToState.Command.Identity
		&& FromState.Command.Context.Montage == ToState.Command.Context.Montage)
	{
		// Pose phase follows the same interpolation fraction as the capsule; simulation warp caches remain discrete.
		MontagePosition = FMath::Lerp(FromState.MontagePosition, ToState.MontagePosition, FMath::Clamp(Pct, 0.f, 1.f));
	}
}
