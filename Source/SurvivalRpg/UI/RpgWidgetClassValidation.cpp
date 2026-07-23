#include "RpgWidgetClassValidation.h"

#if WITH_EDITOR

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Blueprint/BlueprintSupport.h"
#include "Misc/PackageName.h"
#include "UObject/Class.h"

namespace
{
	bool IsStableLoadedClass(const UClass* LoadedClass)
	{
		if (!LoadedClass ||
			LoadedClass->HasAnyFlags(RF_NewerVersionExists) ||
			LoadedClass->HasAnyClassFlags(
				CLASS_NewerVersionExists))
		{
			return false;
		}

		const FString ClassName = LoadedClass->GetName();
		const FString ClassPath = LoadedClass->GetPathName();
		return !ClassPath.IsEmpty() &&
			ClassPath != TEXT("None") &&
			!ClassPath.StartsWith(TEXT("/Engine/Transient")) &&
			!ClassPath.StartsWith(TEXT("/Temp/")) &&
			!ClassName.StartsWith(TEXT("SKEL_")) &&
			!ClassName.StartsWith(TEXT("REINST_")) &&
			!ClassName.StartsWith(TEXT("TRASHCLASS_"));
	}

	bool DoesExactGeneratedClassExist(
		const FSoftObjectPath& ClassPath)
	{
		const FString PackageName =
			ClassPath.GetLongPackageName();
		if (PackageName.IsEmpty())
		{
			return false;
		}

		IAssetRegistry& AssetRegistry =
			IAssetRegistry::GetChecked();
		if (AssetRegistry.IsLoadingAssets())
		{
			// Initial editor discovery has no complete generated-class index
			// yet. Defer only when the package itself is already authored;
			// the normal post-scan validation pass performs the exact check.
			return FPackageName::DoesPackageExist(
				PackageName);
		}

		TArray<FAssetData> PackageAssets;
		AssetRegistry.GetAssetsByPackageName(
			FName(*PackageName),
			PackageAssets,
			/*bIncludeOnlyOnDiskAssets=*/ true);

		const FTopLevelAssetPath ExpectedClassPath =
			ClassPath.GetAssetPath();
		for (const FAssetData& Asset : PackageAssets)
		{
			const FString GeneratedClassExportPath =
				Asset.GetTagValueRef<FString>(
					FBlueprintTags::GeneratedClassPath);
			if (GeneratedClassExportPath.IsEmpty())
			{
				continue;
			}

			const FTopLevelAssetPath GeneratedClassPath(
				FPackageName::ExportTextPathToObjectPath(
					GeneratedClassExportPath));
			if (GeneratedClassPath == ExpectedClassPath)
			{
				return true;
			}
		}

		return false;
	}
}

const UClass* RpgWidgetClassValidation::ResolveAuthoredClassWithoutLoading(
	const FSoftObjectPath& ClassPath,
	const UClass* LoadedClass,
	bool& bOutClassValidationDeferred)
{
	bOutClassValidationDeferred = false;
	if (ClassPath.IsNull() ||
		!ClassPath.GetSubPathString().IsEmpty())
	{
		return nullptr;
	}

	if (IsStableLoadedClass(LoadedClass))
	{
		const FSoftObjectPath LoadedClassPath(LoadedClass);
		return LoadedClassPath.GetAssetPath() ==
				ClassPath.GetAssetPath()
			? LoadedClass
			: nullptr;
	}

	// AssetRegistry metadata verifies the exact generated-class object path
	// without recursively loading a Widget Blueprint from IsDataValid.
	bOutClassValidationDeferred =
		DoesExactGeneratedClassExist(ClassPath);
	return nullptr;
}

#endif
