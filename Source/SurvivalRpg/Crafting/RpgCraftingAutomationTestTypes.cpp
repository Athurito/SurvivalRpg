#include "RpgCraftingAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgCraftingAutomationTestTypes)

TFunction<void()> URpgCraftingAutomationReentrantFragment::OnCreate;

void URpgCraftingAutomationReentrantFragment::OnInstanceCreated(URpgInventoryItemInstance* Instance) const
{
	if (OnCreate) { OnCreate(); }
}

URpgCraftingAutomationReentrantProduct::URpgCraftingAutomationReentrantProduct(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Fragments.Add(CreateDefaultSubobject<URpgInventoryFragment_SpatialItem>(TEXT("Spatial")));
	URpgInventoryFragment_ItemTraits* Traits = CreateDefaultSubobject<URpgInventoryFragment_ItemTraits>(TEXT("Traits"));
	Traits->ItemCategory = ERpgInventoryItemCategory::Misc;
	Traits->bCanStack = false;
	Traits->MaxStackSize = 1;
	Fragments.Add(Traits);
	Fragments.Add(CreateDefaultSubobject<URpgCraftingAutomationReentrantFragment>(TEXT("ReentrantHook")));
}
