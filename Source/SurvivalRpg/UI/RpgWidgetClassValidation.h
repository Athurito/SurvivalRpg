#pragma once

#include "UObject/SoftObjectPath.h"

class UClass;

#if WITH_EDITOR

namespace RpgWidgetClassValidation
{
	/**
	 * Returns a stable already-loaded class, or verifies the exact authored
	 * Blueprint generated-class path through AssetRegistry without loading it.
	 *
	 * Existing unloaded classes set bOutClassValidationDeferred so callers can
	 * postpone base/abstract checks until an authored-asset test loads them.
	 * Missing paths, subobject suffixes, loaded-class mismatches, and stale
	 * generated-class names remain invalid even when their package exists.
	 */
	SURVIVALRPG_API const UClass* ResolveAuthoredClassWithoutLoading(
		const FSoftObjectPath& ClassPath,
		const UClass* LoadedClass,
		bool& bOutClassValidationDeferred);
}

#endif
