#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

namespace RpgFeatureTags
{
	/** Returns true for the root Feature tag or any tag below Feature.*. */
	inline bool IsFeatureTagName(const FName TagName)
	{
		const FString TagString = TagName.ToString();
		return TagString == TEXT("Feature") || TagString.StartsWith(TEXT("Feature."));
	}

	/** Returns true when a GameplayTag is valid and belongs to the Feature.* namespace. */
	inline bool IsFeatureTag(const FGameplayTag& Tag)
	{
		return Tag.IsValid() && IsFeatureTagName(Tag.GetTagName());
	}

	/** Validates that every tag in a container belongs to Feature.*. Empty containers are allowed. */
	inline bool DoesContainerOnlyContainFeatureTags(const FGameplayTagContainer& TagContainer)
	{
		for (const FGameplayTag& Tag : TagContainer)
		{
			if (!IsFeatureTag(Tag))
			{
				return false;
			}
		}

		return true;
	}

	/** Requests and adds a Feature.* tag by name, returning false for invalid or non-feature tags. */
	inline bool AddFeatureTagByName(FGameplayTagContainer& TagContainer, const FName TagName)
	{
		if (!IsFeatureTagName(TagName))
		{
			return false;
		}

		const FGameplayTag FeatureTag = FGameplayTag::RequestGameplayTag(TagName, false);
		if (!FeatureTag.IsValid())
		{
			return false;
		}

		TagContainer.AddTag(FeatureTag);
		return true;
	}
}
