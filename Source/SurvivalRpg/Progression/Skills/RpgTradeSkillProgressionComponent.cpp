#include "RpgTradeSkillProgressionComponent.h"

#include "Curves/CurveFloat.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "RpgTradeSkillGameplayTags.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"

namespace RpgTradeSkillProgression
{
bool IsSkillTag(const FGameplayTag SkillTag)
{
	const FGameplayTag SkillRoot = FGameplayTag::RequestGameplayTag(TEXT("Skill"), false);
	return SkillTag.IsValid() && SkillRoot.IsValid() && SkillTag.MatchesTag(SkillRoot);
}

void AddCoreSkillTags(TSet<FGameplayTag>& OutTags)
{
	OutTags.Add(RpgTradeSkillGameplayTags::Skill_Gathering_Foraging);
	OutTags.Add(RpgTradeSkillGameplayTags::Skill_Gathering_Logging);
	OutTags.Add(RpgTradeSkillGameplayTags::Skill_Gathering_Mining);
	OutTags.Add(RpgTradeSkillGameplayTags::Skill_Gathering_Skinning);
	OutTags.Add(RpgTradeSkillGameplayTags::Skill_Crafting_Woodworking);
	OutTags.Add(RpgTradeSkillGameplayTags::Skill_Crafting_Blacksmithing);
}
}

URpgTradeSkillProgressionComponent::URpgTradeSkillProgressionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void URpgTradeSkillProgressionComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		EnsureDefaultSkillStates();
	}
}

void URpgTradeSkillProgressionComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(
		URpgTradeSkillProgressionComponent,
		SkillStates,
		COND_OwnerOnly,
		REPNOTIFY_Always);
}

int32 URpgTradeSkillProgressionComponent::GetSkillLevelByTag(const FGameplayTag SkillTag) const
{
	const FTradeSkillState* State = FindSkillState(SkillTag);
	return State ? State->Level : 1;
}

float URpgTradeSkillProgressionComponent::GetSkillXPByTag(const FGameplayTag SkillTag) const
{
	const FTradeSkillState* State = FindSkillState(SkillTag);
	return State ? State->XP : 0.0f;
}

float URpgTradeSkillProgressionComponent::GetXPToNextLevelByTag(const FGameplayTag SkillTag) const
{
	return GetXPToNextLevel(SkillTag, GetSkillLevelByTag(SkillTag));
}

float URpgTradeSkillProgressionComponent::GetSkillYieldMultiplier(const FGameplayTag SkillTag) const
{
	const int32 Level = GetSkillLevelByTag(SkillTag);
	if (const FTradeSkillConfig* Config = GetConfig(SkillTag))
	{
		if (Config->YieldMultiplierByLevel)
		{
			const float AuthoredValue = Config->YieldMultiplierByLevel->GetFloatValue(static_cast<float>(Level));
			if (FMath::IsFinite(AuthoredValue) && AuthoredValue > 0.0f)
			{
				return AuthoredValue;
			}
		}
	}

	return CalculateDefaultYieldMultiplier(Level);
}

float URpgTradeSkillProgressionComponent::GetSkillRareFindMultiplier(const FGameplayTag SkillTag) const
{
	const int32 Level = GetSkillLevelByTag(SkillTag);
	if (const FTradeSkillConfig* Config = GetConfig(SkillTag))
	{
		if (Config->RareFindMultiplierByLevel)
		{
			const float AuthoredValue = Config->RareFindMultiplierByLevel->GetFloatValue(static_cast<float>(Level));
			if (FMath::IsFinite(AuthoredValue) && AuthoredValue > 0.0f)
			{
				return AuthoredValue;
			}
		}
	}

	return CalculateDefaultRareFindMultiplier(Level);
}

bool URpgTradeSkillProgressionComponent::AddSkillXPByTag(
	const FGameplayTag SkillTag,
	const float Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !RpgTradeSkillProgression::IsSkillTag(SkillTag) ||
		!FMath::IsFinite(Amount) || Amount <= 0.0f)
	{
		return false;
	}

	EnsureDefaultSkillStates();
	FTradeSkillState* State = FindMutableSkillState(SkillTag);
	if (!State)
	{
		State = &SkillStates.AddDefaulted_GetRef();
		State->SkillTag = SkillTag;
	}

	const int32 MaxLevel = GetMaxLevel(SkillTag);
	if (State->Level >= MaxLevel)
	{
		State->Level = MaxLevel;
		State->XP = 0.0f;
		return false;
	}

	State->XP += Amount;
	TryLevelUp(*State);
	BroadcastSkillChanged(*State);
	GetOwner()->ForceNetUpdate();
	MarkOwnerSaveDirty();
	return true;
}

FGameplayTag URpgTradeSkillProgressionComponent::GetSkillTagForLegacySkill(const ETradeSkill Skill)
{
	switch (Skill)
	{
	case ETradeSkill::Blacksmithing:
		return RpgTradeSkillGameplayTags::Skill_Crafting_Blacksmithing;
	case ETradeSkill::Woodworking:
		return RpgTradeSkillGameplayTags::Skill_Crafting_Woodworking;
	case ETradeSkill::Mining:
		return RpgTradeSkillGameplayTags::Skill_Gathering_Mining;
	case ETradeSkill::Harvesting:
		return RpgTradeSkillGameplayTags::Skill_Gathering_Foraging;
	case ETradeSkill::Logging:
		return RpgTradeSkillGameplayTags::Skill_Gathering_Logging;
	default:
		return FGameplayTag();
	}
}

int32 URpgTradeSkillProgressionComponent::GetSkillLevel(const ETradeSkill Skill) const
{
	return GetSkillLevelByTag(GetSkillTagForLegacySkill(Skill));
}

float URpgTradeSkillProgressionComponent::GetSkillXP(const ETradeSkill Skill) const
{
	return GetSkillXPByTag(GetSkillTagForLegacySkill(Skill));
}

void URpgTradeSkillProgressionComponent::AddSkillXP(const ETradeSkill Skill, const float Amount)
{
	AddSkillXPByTag(GetSkillTagForLegacySkill(Skill), Amount);
}

TArray<FTradeSkillState> URpgTradeSkillProgressionComponent::ExportSkillStates() const
{
	return SkillStates;
}

bool URpgTradeSkillProgressionComponent::RestoreSkillStates(
	const TArray<FTradeSkillState>& InSkillStates)
{
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		return false;
	}

	TArray<FTradeSkillState> MigratedStates = InSkillStates;
	for (int32 Index = 0; Index < MigratedStates.Num(); ++Index)
	{
		FTradeSkillState& State = MigratedStates[Index];
		if (!State.SkillTag.IsValid())
		{
			if (Index >= static_cast<int32>(ETradeSkill::MAX))
			{
				return false;
			}
			State.SkillTag = GetSkillTagForLegacySkill(
				static_cast<ETradeSkill>(Index));
		}
	}
	if (!ValidateSkillStates(MigratedStates))
	{
		return false;
	}

	SkillStates = MoveTemp(MigratedStates);
	EnsureDefaultSkillStates();
	for (const FTradeSkillState& State : SkillStates)
	{
		BroadcastSkillChanged(State);
	}

	if (GetOwner())
	{
		GetOwner()->ForceNetUpdate();
	}
	return true;
}

void URpgTradeSkillProgressionComponent::ResetSkillStatesToDefaults()
{
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		return;
	}

	SkillStates.Reset();
	EnsureDefaultSkillStates();
	for (const FTradeSkillState& State : SkillStates)
	{
		BroadcastSkillChanged(State);
	}

	if (GetOwner())
	{
		GetOwner()->ForceNetUpdate();
	}
}

bool URpgTradeSkillProgressionComponent::ValidateSkillStates(
	const TArray<FTradeSkillState>& InSkillStates,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}

	TSet<FGameplayTag> SeenSkills;
	for (const FTradeSkillState& State : InSkillStates)
	{
		if (!State.IsValid() || !RpgTradeSkillProgression::IsSkillTag(State.SkillTag))
		{
			if (OutError)
			{
				*OutError = FString::Printf(
					TEXT("Trade-skill state '%s' has an invalid tag, level, or XP value."),
					*State.SkillTag.ToString());
			}
			return false;
		}
		if (SeenSkills.Contains(State.SkillTag))
		{
			if (OutError)
			{
				*OutError = FString::Printf(
					TEXT("Trade-skill state '%s' appears more than once."),
					*State.SkillTag.ToString());
			}
			return false;
		}

		SeenSkills.Add(State.SkillTag);
	}

	return true;
}

float URpgTradeSkillProgressionComponent::CalculateDefaultXPToNextLevel(const int32 Level)
{
	const int32 ClampedLevel = FMath::Clamp(Level, 1, SupportedMaxSkillLevel);
	return static_cast<float>(FMath::RoundToInt(
		100.0f * FMath::Pow(static_cast<float>(ClampedLevel), 1.35f)));
}

float URpgTradeSkillProgressionComponent::CalculateDefaultYieldMultiplier(const int32 Level)
{
	const float Alpha = static_cast<float>(FMath::Clamp(Level, 1, SupportedMaxSkillLevel) - 1) /
		static_cast<float>(SupportedMaxSkillLevel - 1);
	return FMath::Lerp(1.0f, 1.5f, Alpha);
}

float URpgTradeSkillProgressionComponent::CalculateDefaultRareFindMultiplier(const int32 Level)
{
	const float Alpha = static_cast<float>(FMath::Clamp(Level, 1, SupportedMaxSkillLevel) - 1) /
		static_cast<float>(SupportedMaxSkillLevel - 1);
	return FMath::Lerp(1.0f, 2.0f, Alpha);
}

void URpgTradeSkillProgressionComponent::OnRep_SkillStates()
{
	for (const FTradeSkillState& State : SkillStates)
	{
		BroadcastSkillChanged(State);
	}
}

void URpgTradeSkillProgressionComponent::EnsureDefaultSkillStates()
{
	TSet<FGameplayTag> RequiredSkills;
	RpgTradeSkillProgression::AddCoreSkillTags(RequiredSkills);
	if (ConfigData)
	{
		for (const TPair<FGameplayTag, FTradeSkillConfig>& Pair : ConfigData->TaggedSkillConfigs)
		{
			if (RpgTradeSkillProgression::IsSkillTag(Pair.Key))
			{
				RequiredSkills.Add(Pair.Key);
			}
		}
		for (const TPair<ETradeSkill, FTradeSkillConfig>& Pair : ConfigData->SkillConfigs)
		{
			const FGameplayTag LegacyTag = GetSkillTagForLegacySkill(Pair.Key);
			if (LegacyTag.IsValid())
			{
				RequiredSkills.Add(LegacyTag);
			}
		}
	}

	TMap<FGameplayTag, FTradeSkillState> NormalizedStates;
	for (int32 Index = 0; Index < SkillStates.Num(); ++Index)
	{
		FTradeSkillState State = SkillStates[Index];
		if (!State.SkillTag.IsValid() && Index < static_cast<int32>(ETradeSkill::MAX))
		{
			State.SkillTag = GetSkillTagForLegacySkill(static_cast<ETradeSkill>(Index));
		}
		if (!RpgTradeSkillProgression::IsSkillTag(State.SkillTag) || NormalizedStates.Contains(State.SkillTag))
		{
			continue;
		}

		State.Level = FMath::Clamp(State.Level, 1, GetMaxLevel(State.SkillTag));
		State.XP = FMath::IsFinite(State.XP) && State.XP >= 0.0f ? State.XP : 0.0f;
		if (State.Level >= GetMaxLevel(State.SkillTag))
		{
			State.XP = 0.0f;
		}
		NormalizedStates.Add(State.SkillTag, State);
	}

	for (const FGameplayTag SkillTag : RequiredSkills)
	{
		if (!NormalizedStates.Contains(SkillTag))
		{
			FTradeSkillState DefaultState;
			DefaultState.SkillTag = SkillTag;
			NormalizedStates.Add(SkillTag, DefaultState);
		}
	}

	NormalizedStates.GenerateValueArray(SkillStates);
	SkillStates.Sort([](const FTradeSkillState& Left, const FTradeSkillState& Right)
	{
		return Left.SkillTag.ToString() < Right.SkillTag.ToString();
	});
}

void URpgTradeSkillProgressionComponent::BroadcastSkillChanged(const FTradeSkillState& State)
{
	OnTradeSkillTagChanged.Broadcast(State.SkillTag, State);

	ETradeSkill LegacySkill = ETradeSkill::MAX;
	if (TryGetLegacySkillForTag(State.SkillTag, LegacySkill))
	{
		OnTradeSkillChanged.Broadcast(LegacySkill, State);
	}
}

void URpgTradeSkillProgressionComponent::TryLevelUp(FTradeSkillState& State)
{
	const int32 MaxLevel = GetMaxLevel(State.SkillTag);
	while (State.Level < MaxLevel)
	{
		const float XPNeeded = GetXPToNextLevel(State.SkillTag, State.Level);
		if (!FMath::IsFinite(XPNeeded) || XPNeeded <= 0.0f || State.XP < XPNeeded)
		{
			break;
		}

		State.XP -= XPNeeded;
		++State.Level;
		HandleSkillLevelUp(State.SkillTag, State.Level);
	}

	if (State.Level >= MaxLevel)
	{
		State.Level = MaxLevel;
		State.XP = 0.0f;
	}
}

void URpgTradeSkillProgressionComponent::HandleSkillLevelUp(
	const FGameplayTag SkillTag,
	const int32 NewLevel)
{
	(void)SkillTag;
	(void)NewLevel;
	// Milestone choices and feature-owned unlocks intentionally attach here in later slices.
}

void URpgTradeSkillProgressionComponent::MarkOwnerSaveDirty() const
{
	const AActor* Owner = GetOwner();
	APlayerController* PlayerController = Owner ? Cast<APlayerController>(Owner->GetOwner()) : nullptr;
	if (ARpgGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARpgGameModeBase>() : nullptr)
	{
		GameMode->MarkPlayerSaveDirty(PlayerController);
	}
}

FTradeSkillState* URpgTradeSkillProgressionComponent::FindMutableSkillState(const FGameplayTag SkillTag)
{
	return SkillStates.FindByPredicate([SkillTag](const FTradeSkillState& State)
	{
		return State.SkillTag == SkillTag;
	});
}

const FTradeSkillState* URpgTradeSkillProgressionComponent::FindSkillState(const FGameplayTag SkillTag) const
{
	return SkillStates.FindByPredicate([SkillTag](const FTradeSkillState& State)
	{
		return State.SkillTag == SkillTag;
	});
}

const FTradeSkillConfig* URpgTradeSkillProgressionComponent::GetConfig(const FGameplayTag SkillTag) const
{
	if (!ConfigData)
	{
		return nullptr;
	}
	if (const FTradeSkillConfig* TaggedConfig = ConfigData->TaggedSkillConfigs.Find(SkillTag))
	{
		return TaggedConfig;
	}

	ETradeSkill LegacySkill = ETradeSkill::MAX;
	return TryGetLegacySkillForTag(SkillTag, LegacySkill)
		? ConfigData->SkillConfigs.Find(LegacySkill)
		: nullptr;
}

int32 URpgTradeSkillProgressionComponent::GetMaxLevel(const FGameplayTag SkillTag) const
{
	const FTradeSkillConfig* Config = GetConfig(SkillTag);
	return FMath::Clamp(Config ? Config->MaxLevel : SupportedMaxSkillLevel, 1, SupportedMaxSkillLevel);
}

float URpgTradeSkillProgressionComponent::GetXPToNextLevel(
	const FGameplayTag SkillTag,
	const int32 Level) const
{
	if (Level >= GetMaxLevel(SkillTag))
	{
		return 0.0f;
	}

	if (const FTradeSkillConfig* Config = GetConfig(SkillTag))
	{
		if (Config->XPToNextLevel)
		{
			const float AuthoredValue = Config->XPToNextLevel->GetFloatValue(static_cast<float>(Level));
			if (FMath::IsFinite(AuthoredValue) && AuthoredValue > 0.0f)
			{
				return AuthoredValue;
			}
		}
	}

	return CalculateDefaultXPToNextLevel(Level);
}

bool URpgTradeSkillProgressionComponent::TryGetLegacySkillForTag(
	const FGameplayTag SkillTag,
	ETradeSkill& OutSkill)
{
	for (int32 Index = 0; Index < static_cast<int32>(ETradeSkill::MAX); ++Index)
	{
		const ETradeSkill Candidate = static_cast<ETradeSkill>(Index);
		if (GetSkillTagForLegacySkill(Candidate) == SkillTag)
		{
			OutSkill = Candidate;
			return true;
		}
	}

	return false;
}
