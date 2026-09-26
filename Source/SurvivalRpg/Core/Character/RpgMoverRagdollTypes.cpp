#include "RpgMoverRagdollTypes.h"

#include "Abilities/GameplayAbility.h"
#include "Animation/AnimMontage.h"
#include "Components/PrimitiveComponent.h"

bool FRpgMoverRagdollState::MatchesAbility(const UGameplayAbility* Ability) const
{
	if (!Ability || !AbilityHandle.IsValid()) { return false; }
	const FPredictionKey& Key = Ability->GetCurrentActivationInfo().GetActivationPredictionKey();
	return AbilityHandle == Ability->GetCurrentAbilitySpecHandle() && ActivationPredictionKey == Key.Current &&
		bServerInitiatedKey == Key.bIsServerInitiated;
}

bool FRpgMoverRagdollState::Equals(const FRpgMoverRagdollState& Other) const
{
	return Revision == Other.Revision && Episode == Other.Episode && Phase == Other.Phase &&
		AbilityHandle == Other.AbilityHandle && ActivationPredictionKey == Other.ActivationPredictionKey &&
		bServerInitiatedKey == Other.bServerInitiatedKey && Anchor.Equals(Other.Anchor, .01f) &&
		GetUpMontage == Other.GetUpMontage && FMath::IsNearlyEqual(GetUpStartTime, Other.GetUpStartTime, .0001f) &&
		FMath::IsNearlyEqual(GetUpPlayRate, Other.GetUpPlayRate, .0001f) && bCancelled == Other.bCancelled &&
		Support == Other.Support && SupportTransform.Equals(Other.SupportTransform, .01f);
}

void FRpgMoverRagdollState::Serialize(FArchive& Ar)
{
	Ar << Revision << Episode;
	uint8 PhaseByte = static_cast<uint8>(Phase);
	Ar << PhaseByte;
	if (Ar.IsLoading()) { Phase = static_cast<ERpgMoverRagdollPhase>(PhaseByte); }
	Ar << Anchor << GetUpMontage << GetUpStartTime << GetUpPlayRate << bCancelled;
	Ar << GetUpPlayId << bHasGetUpPlayId;
	Ar << AbilityHandle << ActivationPredictionKey << bServerInitiatedKey << Support << SupportTransform;
	if (Ar.IsLoading()) { RetainObjectsForHistory(); }
}

void FRpgMoverRagdollState::RetainObjectsForHistory() { MontageLifetime.Reset(GetUpMontage); }
void FRpgMoverRagdollState::AddReferencedObjects(FReferenceCollector& Collector) { Collector.AddReferencedObject(GetUpMontage); }
FMoverDataStructBase* FRpgMoverRagdollSyncState::Clone() const { return new FRpgMoverRagdollSyncState(*this); }
UScriptStruct* FRpgMoverRagdollSyncState::GetScriptStruct() const { return StaticStruct(); }
bool FRpgMoverRagdollSyncState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	State.Serialize(Ar);
	Ar << AppliedRevision << MontagePosition;
	bOutSuccess = !Ar.IsError();
	return true;
}
void FRpgMoverRagdollSyncState::AddReferencedObjects(FReferenceCollector& Collector) { State.AddReferencedObjects(Collector); }
bool FRpgMoverRagdollSyncState::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	const FRpgMoverRagdollSyncState& Other = static_cast<const FRpgMoverRagdollSyncState&>(AuthorityState);
	return !State.Equals(Other.State) || AppliedRevision != Other.AppliedRevision ||
		!FMath::IsNearlyEqual(MontagePosition, Other.MontagePosition, .001f);
}
void FRpgMoverRagdollSyncState::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	const FRpgMoverRagdollSyncState& A = static_cast<const FRpgMoverRagdollSyncState&>(From);
	const FRpgMoverRagdollSyncState& B = static_cast<const FRpgMoverRagdollSyncState&>(To);
	*this = Pct < 1.f ? A : B;
	if (A.State.Revision == B.State.Revision && A.State.Phase == ERpgMoverRagdollPhase::GettingUp)
	{
		MontagePosition = FMath::Lerp(A.MontagePosition, B.MontagePosition, FMath::Clamp(Pct, 0.f, 1.f));
	}
}
