#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

namespace RpgFeatureTags
{
	inline bool IsFeatureTagName(const FName TagName)
	{
		const FString TagString = TagName.ToString();
		return TagString == TEXT("Feature") || TagString.StartsWith(TEXT("Feature."));
	}

	inline bool IsFeatureTag(const FGameplayTag& Tag)
	{
		return Tag.IsValid() && IsFeatureTagName(Tag.GetTagName());
	}

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
