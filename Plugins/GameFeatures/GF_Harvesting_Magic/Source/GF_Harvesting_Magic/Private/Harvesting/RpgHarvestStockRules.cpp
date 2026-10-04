#include "Harvesting/RpgHarvestStockRules.h"

#include "GameFramework/Actor.h"
#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
#include "Harvesting/RpgHarvestProfile.h"
#include "Harvesting/RpgHarvestRewardService.h"

DEFINE_LOG_CATEGORY(LogRpgHarvesting);

bool FRpgHarvestWeakPoint::IsStruckBy(const FHitResult& Hit) const
{
	if (Radius <= 0.0f)
	{
		return false;
	}
	const double RadiusSquared = FMath::Square(static_cast<double>(Radius));
	if (FVector::DistSquared(Hit.ImpactPoint, Location) <= RadiusSquared)
	{
		return true;
	}

	// Location is the sweep center at contact; for a line trace it equals ImpactPoint.
	const FVector Ray = Hit.Location - Hit.TraceStart;
	const double RayLength = Ray.Size();
	if (RayLength <= UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}
	const FVector Direction = Ray / RayLength;
	const double Along = FVector::DotProduct(Location - Hit.TraceStart, Direction);
	if (Along < 0.0 || Along > RayLength + 2.0 * Radius)
	{
		return false;
	}
	return FVector::DistSquared(Hit.TraceStart + Direction * Along, Location) <= RadiusSquared;
}

FRpgHarvestResult FRpgHarvestStockRules::Evaluate(
	const URpgHarvestProfile* Profile,
	const FRpgHarvestRequest& Request,
	const FRpgHarvestStockSnapshot& Stock,
	const FRpgHarvestWeakPoint* ActiveWeakPoint)
{
	const int32 SectionCount = FMath::Max(1, Stock.SectionCount);
	const int32 RemainingSections = Stock.GetRemainingSections();
	auto Reject = [RemainingSections, SectionCount](const ERpgHarvestOutcome Outcome)
	{
		return FRpgHarvestResult::MakeRejected(Outcome, RemainingSections, SectionCount);
	};

	if (!Request.Harvester ||
		!FMath::IsFinite(Request.HarvestPower) ||
		Request.HarvestPower <= 0.0f ||
		Request.RequestedSections < 1 ||
		!Request.AbilityId.MatchesTag(RpgHarvestingMagicGameplayTags::Ability_Harvesting))
	{
		return Reject(ERpgHarvestOutcome::Invalid);
	}
	if (RemainingSections <= 0)
	{
		return Reject(ERpgHarvestOutcome::Depleted);
	}
	if (Request.ExpectedRevision == INDEX_NONE || Request.ExpectedRevision != Stock.Revision)
	{
		return Reject(ERpgHarvestOutcome::Stale);
	}
	if (!MeetsToolRequirement(Profile, Request))
	{
		return Reject(ERpgHarvestOutcome::WrongTool);
	}
	if (!FRpgHarvestRewardService::MeetsSkillGate(Profile, Request.Harvester))
	{
		return Reject(ERpgHarvestOutcome::SkillGate);
	}

	FRpgHarvestResult Result;
	Result.Outcome = ERpgHarvestOutcome::Harvested;
	Result.SectionCount = SectionCount;
	int32 SectionsToTake = Request.RequestedSections;
	const int32 WeakPointBonus = Profile ? Profile->GetClampedWeakPointBonusSections() : 0;
	if (Request.bCanHitWeakPoint && ActiveWeakPoint && WeakPointBonus > 0 && ActiveWeakPoint->IsStruckBy(Request.Hit))
	{
		// The bonus only speeds extraction up; it comes from the same stock and is clamped below.
		Result.bWeakPointHit = true;
		SectionsToTake += WeakPointBonus;
	}
	Result.SectionsTaken = FMath::Min(SectionsToTake, RemainingSections);
	Result.RemainingSections = RemainingSections - Result.SectionsTaken;
	return Result;
}

int32 FRpgHarvestStockRules::GetActiveWeakPointIndex(const FRpgHarvestStockSnapshot& Stock, const int32 NumWeakPoints)
{
	if (NumWeakPoints <= 0 || Stock.GetRemainingSections() <= 0)
	{
		return INDEX_NONE;
	}
	const int64 Step = static_cast<int64>(FMath::Max(0, Stock.Revision)) + FMath::Max(0, Stock.HarvestedSections);
	return static_cast<int32>(Step % NumWeakPoints);
}

bool FRpgHarvestStockRules::MeetsToolRequirement(
	const URpgHarvestProfile* Profile,
	const FRpgHarvestRequest& Request)
{
	return !Profile ||
		!Profile->RequiredToolTag.IsValid() ||
		Request.ToolTag.MatchesTag(Profile->RequiredToolTag);
}

ERpgHarvestDelivery FRpgHarvestStockRules::ToDelivery(const ERpgHarvestRewardDeliveryResult DeliveryResult)
{
	switch (DeliveryResult)
	{
	case ERpgHarvestRewardDeliveryResult::Empty:
		return ERpgHarvestDelivery::Empty;
	case ERpgHarvestRewardDeliveryResult::Inventory:
		return ERpgHarvestDelivery::Inventory;
	case ERpgHarvestRewardDeliveryResult::WorldDrop:
		return ERpgHarvestDelivery::WorldDrop;
	case ERpgHarvestRewardDeliveryResult::Failed:
	default:
		return ERpgHarvestDelivery::None;
	}
}
