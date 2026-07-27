#pragma once

#include "CoreMinimal.h"

class APlayerController;
struct FGeometry;

/**
 * Pure Slate-geometry helpers shared by inventory context-menu presenters.
 *
 * Context-menu anchors use absolute Slate coordinates. Callers must not apply viewport DPI scaling manually.
 */
namespace RpgInventoryUiGeometry
{
	/** Converts one in-bounds local point through a valid, non-zero geometry into absolute Slate coordinates. */
	SURVIVALRPG_API bool TryResolveAbsolutePoint(
		const FGeometry& Geometry,
		FVector2D LocalPosition,
		FVector2D& OutAbsoluteScreenPosition);

	/** Resolves the center of a valid, non-zero geometry in absolute Slate coordinates. */
	SURVIVALRPG_API bool TryResolveAbsoluteCenter(
		const FGeometry& Geometry,
		FVector2D& OutAbsoluteScreenPosition);

	/** Resolves the owning player's screen center in absolute Slate coordinates, including DPI and window offsets. */
	SURVIVALRPG_API bool TryResolvePlayerScreenCenter(
		APlayerController* PlayerController,
		FVector2D& OutAbsoluteScreenPosition);

	/** Converts an absolute anchor to canvas-local space and clamps the complete menu inside that canvas. */
	SURVIVALRPG_API bool TryResolveClampedMenuCanvasPosition(
		const FGeometry& CanvasGeometry,
		FVector2D AbsoluteAnchor,
		FVector2D MenuSize,
		FVector2D& OutLocalPosition);
}
