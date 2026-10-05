#include "RpgWeaponAbilityLoadoutComponent.h"

#include "GameFramework/Controller.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "RpgAbilityBindingResolver.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeComponent.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeDefinition.h"
#include "SurvivalRpg/SurvivalRpg.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgWeaponAbilityLoadoutComponent)

URpgWeaponAbilityLoadoutComponent::URpgWeaponAbilityLoadoutComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void URpgWeaponAbilityLoadoutComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, Slots, COND_OwnerOnly);
}

void URpgWeaponAbilityLoadoutComponent::BeginPlay()
{
	EnsureSlotCount();
	Super::BeginPlay();
}

FRpgWeaponAbilityLoadoutSlot URpgWeaponAbilityLoadoutComponent::GetSlot(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex] : FRpgWeaponAbilityLoadoutSlot();
}

void URpgWeaponAbilityLoadoutComponent::RequestAssignAbilityToSlot_Implementation(int32 SlotIndex, FGameplayTag AbilityIdTag)
{
	EnsureSlotCount();
	if (!IsValidSlotIndex(SlotIndex) || !AbilityIdTag.IsValid())
	{
		return;
	}

	Slots[SlotIndex].AbilityIdTag = AbilityIdTag;
	Slots[SlotIndex].bDefaultSelection = false;
	RefreshAbilityBindings();
}

void URpgWeaponAbilityLoadoutComponent::RequestClearSlot_Implementation(int32 SlotIndex)
{
	EnsureSlotCount();
	if (!IsValidSlotIndex(SlotIndex))
	{
		return;
	}

	Slots[SlotIndex] = FRpgWeaponAbilityLoadoutSlot();
	RefreshAbilityBindings();
}

void URpgWeaponAbilityLoadoutComponent::RefreshAbilityBindings()
{
	EnsureSlotCount();

	URpgAbilitySystemComponent* RpgASC = GetRpgPlayerController() ? GetRpgPlayerController()->GetRpgAbilitySystemComponent() : nullptr;
	if (!RpgASC || !RpgASC->HasGrantAuthority())
	{
		UE_LOG(
			LogRpgAbilitySystem,
			Verbose,
			TEXT("Weapon ability bindings of [%s] not resolved: ability system=%d grant authority=%d."),
			*GetNameSafe(GetOwner()),
			RpgASC ? 1 : 0,
			RpgASC && RpgASC->HasGrantAuthority() ? 1 : 0);
		if (GetOwner() && GetOwner()->HasAuthority())
		{
			for (FRpgWeaponAbilityLoadoutSlot& Slot : Slots)
			{
				Slot.bAvailable = false;
				Slot.ResolveResult = Slot.AbilityIdTag.IsValid()
					? ERpgAbilityBindingResolveResult::Missing
					: ERpgAbilityBindingResolveResult::InvalidAbilityId;
			}
			OnRep_Slots();
		}
		return;
	}

	ApplyAbilityBindings(*RpgASC);
}

void URpgWeaponAbilityLoadoutComponent::ApplyAbilityBindings(URpgAbilitySystemComponent& AbilitySystem)
{
	EnsureSlotCount();

	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		FRpgWeaponAbilityLoadoutSlot& Slot = Slots[SlotIndex];
		const FGameplayTag RuntimeInputTag = GetInputTagForSlotIndex(SlotIndex);
		AbilitySystem.ClearRuntimeAbilityInputTag(RuntimeInputTag);

		// A slot without a player selection follows the skill tree of the weapon in use, then the default of the
		// currently granted ability sets.
		if (!Slot.AbilityIdTag.IsValid() || Slot.bDefaultSelection)
		{
			FGameplayTag DefaultAbilityId = ResolveSkillTreeAbilityId(AbilitySystem, SlotIndex);
			ERpgAbilityBindingResolveResult DefaultResult = ERpgAbilityBindingResolveResult::Unique;
			if (!DefaultAbilityId.IsValid())
			{
				DefaultResult = ResolveDefaultAbilityId(AbilitySystem, SlotIndex, DefaultAbilityId);
			}
			Slot.AbilityIdTag = DefaultAbilityId;
			Slot.bDefaultSelection = DefaultAbilityId.IsValid();
			if (!Slot.bDefaultSelection)
			{
				Slot.bAvailable = false;
				Slot.ResolveResult = DefaultResult == ERpgAbilityBindingResolveResult::Missing
					? ERpgAbilityBindingResolveResult::InvalidAbilityId
					: DefaultResult;
				continue;
			}
		}

		const FRpgUniqueAbilityBindingResolution Resolution = FRpgAbilityBindingResolver::ResolveUniqueAbilityId(
			&AbilitySystem,
			Slot.AbilityIdTag,
			this);
		Slot.ResolveResult = Resolution.Result;
		Slot.bAvailable = Resolution.IsUnique();

		// The unique resolver guarantees BindInputTagToAbilityId cannot silently choose between duplicate ids.
		if (Slot.bAvailable)
		{
			Slot.bAvailable = AbilitySystem.BindInputTagToAbilityId(Slot.AbilityIdTag, RuntimeInputTag);
		}
		UE_LOG(
			LogRpgAbilitySystem,
			Verbose,
			TEXT("Weapon ability slot %d of [%s]: id [%s] default=%d available=%d result=%d."),
			SlotIndex + 1,
			*GetNameSafe(GetOwner()),
			*Slot.AbilityIdTag.ToString(),
			Slot.bDefaultSelection ? 1 : 0,
			Slot.bAvailable ? 1 : 0,
			static_cast<int32>(Slot.ResolveResult));
	}

	OnRep_Slots();
}

FGameplayTag URpgWeaponAbilityLoadoutComponent::ResolveSkillTreeAbilityId(
	const URpgAbilitySystemComponent& AbilitySystem,
	const int32 SlotIndex) const
{
	const AController* Controller = Cast<AController>(GetOwner());
	const URpgSkillTreeDefinition* SkillTree = URpgSkillTreeComponent::FindActiveWeaponSkillTree(
		Controller ? Controller->GetPawn() : nullptr);
	const URpgSkillTreeComponent* SkillTrees = URpgSkillTreeComponent::FindForActor(Controller);
	if (!SkillTree || !SkillTrees)
	{
		return FGameplayTag();
	}

	// Only an assignment whose ability is granted right now replaces the ability set default.
	const FGameplayTag AbilityId = SkillTrees->GetSlotAbilityId(SkillTree->TreeTag, SlotIndex);
	return AbilityId.IsValid() &&
			FRpgAbilityBindingResolver::ResolveUniqueAbilityId(&AbilitySystem, AbilityId, this).IsUnique()
		? AbilityId
		: FGameplayTag();
}

ERpgAbilityBindingResolveResult URpgWeaponAbilityLoadoutComponent::ResolveDefaultAbilityId(
	const URpgAbilitySystemComponent& AbilitySystem,
	const int32 SlotIndex,
	FGameplayTag& OutAbilityIdTag)
{
	OutAbilityIdTag = FGameplayTag();
	const FGameplayTag DefaultSelectionTag = GetDefaultSelectionTagForSlotIndex(SlotIndex);
	if (!DefaultSelectionTag.IsValid())
	{
		return ERpgAbilityBindingResolveResult::InvalidAbilityId;
	}

	// Ability set entries require AbilityIdTag in the "Ability" category; it is the only such spec-source tag.
	const FGameplayTag AbilityIdRoot = FGameplayTag::RequestGameplayTag(TEXT("Ability"), false);
	bool bFoundDeclaration = false;
	for (const FGameplayAbilitySpec& Spec : AbilitySystem.GetActivatableAbilities())
	{
		if (!Spec.Ability || !Spec.GetDynamicSpecSourceTags().HasTagExact(DefaultSelectionTag))
		{
			continue;
		}

		bFoundDeclaration = true;
		FGameplayTag DeclaredAbilityId;
		int32 AbilityIdCount = 0;
		for (const FGameplayTag& SourceTag : Spec.GetDynamicSpecSourceTags())
		{
			if (AbilityIdRoot.IsValid() && SourceTag.MatchesTag(AbilityIdRoot))
			{
				DeclaredAbilityId = SourceTag;
				++AbilityIdCount;
			}
		}

		if (AbilityIdCount != 1)
		{
			UE_LOG(
				LogRpgAbilitySystem,
				Error,
				TEXT("Weapon ability default blocked: [%s] declares slot %d without exactly one ability id."),
				*GetNameSafe(Spec.Ability),
				SlotIndex + 1);
			OutAbilityIdTag = FGameplayTag();
			return ERpgAbilityBindingResolveResult::InvalidAbilityId;
		}

		if (OutAbilityIdTag.IsValid() && OutAbilityIdTag != DeclaredAbilityId)
		{
			UE_LOG(
				LogRpgAbilitySystem,
				Error,
				TEXT("Weapon ability default blocked: [%s] and [%s] both declare slot %d."),
				*OutAbilityIdTag.ToString(),
				*DeclaredAbilityId.ToString(),
				SlotIndex + 1);
			OutAbilityIdTag = FGameplayTag();
			return ERpgAbilityBindingResolveResult::Ambiguous;
		}

		OutAbilityIdTag = DeclaredAbilityId;
	}

	return bFoundDeclaration ? ERpgAbilityBindingResolveResult::Unique : ERpgAbilityBindingResolveResult::Missing;
}

void URpgWeaponAbilityLoadoutComponent::HandleInputPressed(int32 SlotIndex)
{
	EnsureSlotCount();
	UE_LOG(
		LogRpgAbilitySystem,
		Verbose,
		TEXT("Weapon ability slot %d pressed on [%s]: available=%d."),
		SlotIndex + 1,
		*GetNameSafe(GetOwner()),
		IsValidSlotIndex(SlotIndex) && Slots[SlotIndex].bAvailable ? 1 : 0);
	if (!IsValidSlotIndex(SlotIndex) || !Slots[SlotIndex].bAvailable)
	{
		return;
	}

	if (ARpgPlayerController* RpgPC = GetRpgPlayerController())
	{
		if (URpgAbilitySystemComponent* RpgASC = RpgPC->GetRpgAbilitySystemComponent())
		{
			// RuntimeInputTag is server-bound to exactly one spec after unique id validation.
			RpgASC->AbilityInputTagPressed(GetInputTagForSlotIndex(SlotIndex));
		}
	}
}

void URpgWeaponAbilityLoadoutComponent::HandleInputReleased(int32 SlotIndex)
{
	EnsureSlotCount();
	if (!IsValidSlotIndex(SlotIndex) || !Slots[SlotIndex].bAvailable)
	{
		return;
	}

	if (ARpgPlayerController* RpgPC = GetRpgPlayerController())
	{
		if (URpgAbilitySystemComponent* RpgASC = RpgPC->GetRpgAbilitySystemComponent())
		{
			RpgASC->AbilityInputTagReleased(GetInputTagForSlotIndex(SlotIndex));
		}
	}
}

FGameplayTag URpgWeaponAbilityLoadoutComponent::GetInputTagForSlotIndex(int32 SlotIndex)
{
	switch (SlotIndex)
	{
	case 0:
		return RpgGameplayTags::InputTag_Weapon_Ability_1;
	case 1:
		return RpgGameplayTags::InputTag_Weapon_Ability_2;
	case 2:
		return RpgGameplayTags::InputTag_Weapon_Ability_3;
	default:
		return FGameplayTag();
	}
}

int32 URpgWeaponAbilityLoadoutComponent::GetSlotIndexForInputTag(const FGameplayTag InputTag)
{
	if (InputTag == RpgGameplayTags::InputTag_Weapon_Ability_1)
	{
		return 0;
	}
	if (InputTag == RpgGameplayTags::InputTag_Weapon_Ability_2)
	{
		return 1;
	}
	if (InputTag == RpgGameplayTags::InputTag_Weapon_Ability_3)
	{
		return 2;
	}
	return INDEX_NONE;
}

FGameplayTag URpgWeaponAbilityLoadoutComponent::GetDefaultSelectionTagForSlotIndex(const int32 SlotIndex)
{
	switch (SlotIndex)
	{
	case 0:
		return RpgGameplayTags::Rpg_WeaponAbilityLoadout_DefaultSlot_1;
	case 1:
		return RpgGameplayTags::Rpg_WeaponAbilityLoadout_DefaultSlot_2;
	case 2:
		return RpgGameplayTags::Rpg_WeaponAbilityLoadout_DefaultSlot_3;
	default:
		return FGameplayTag();
	}
}

void URpgWeaponAbilityLoadoutComponent::OnRep_Slots()
{
	EnsureSlotCount();
	BroadcastSlotsChanged();
}

void URpgWeaponAbilityLoadoutComponent::EnsureSlotCount()
{
	const int32 ClampedSlotCount = FMath::Clamp(SlotCount, 1, 3);
	if (Slots.Num() != ClampedSlotCount)
	{
		Slots.SetNum(ClampedSlotCount);
	}
}

void URpgWeaponAbilityLoadoutComponent::BroadcastSlotsChanged() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FRpgWeaponAbilityLoadoutChangedMessage Message;
	Message.Owner = GetTypedOuter<APlayerController>();
	Message.LoadoutComponent = const_cast<URpgWeaponAbilityLoadoutComponent*>(this);

	UGameplayMessageSubsystem& MessageSystem = UGameplayMessageSubsystem::Get(World);
	MessageSystem.BroadcastMessage(RpgGameplayTags::Rpg_WeaponAbilityLoadout_Message_SlotsChanged, Message);
}

bool URpgWeaponAbilityLoadoutComponent::IsValidSlotIndex(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex);
}

ARpgPlayerController* URpgWeaponAbilityLoadoutComponent::GetRpgPlayerController() const
{
	return Cast<ARpgPlayerController>(GetOwner());
}
