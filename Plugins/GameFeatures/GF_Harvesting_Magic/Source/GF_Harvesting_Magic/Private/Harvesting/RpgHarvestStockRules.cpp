#include "Harvesting/RpgHarvestStockRules.h"

#include "GameFramework/Actor.h"
#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
#include "Harvesting/RpgHarvestProfile.h"
#include "Harvesting/RpgHarvestRewardService.h"

DEFINE_LOG_CATEGORY(LogRpgHarvesting);

FRpgHarvestResult FRpgHarvestStockRules::Evaluate(
	const URpgHarvestProfile* Profile,
	const FRpgHarvestRequest& Request,
	const FRpgHarvestStockSnapshot& Stock)
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
	Result.SectionsTaken = FMath::Min(Request.RequestedSections, RemainingSections);
	Result.RemainingSections = RemainingSections - Result.SectionsTaken;
	return Result;
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
