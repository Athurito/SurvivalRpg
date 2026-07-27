#include "RpgInventoryUiGeometry.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Layout/Geometry.h"

namespace
{
	bool IsFiniteVector(const FVector2D& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y);
	}

	bool HasUsableGeometry(const FGeometry& Geometry)
	{
		const FVector2D LocalSize = Geometry.GetLocalSize();
		if (!IsFiniteVector(LocalSize) ||
			LocalSize.X <= KINDA_SMALL_NUMBER ||
			LocalSize.Y <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		const FVector2D AbsoluteOrigin = Geometry.LocalToAbsolute(FVector2D::ZeroVector);
		const FVector2D AbsoluteXAxis = Geometry.LocalToAbsolute(FVector2D(LocalSize.X, 0.0f));
		const FVector2D AbsoluteYAxis = Geometry.LocalToAbsolute(FVector2D(0.0f, LocalSize.Y));
		return IsFiniteVector(AbsoluteOrigin) &&
			IsFiniteVector(AbsoluteXAxis) &&
			IsFiniteVector(AbsoluteYAxis) &&
			!AbsoluteXAxis.Equals(AbsoluteOrigin, KINDA_SMALL_NUMBER) &&
			!AbsoluteYAxis.Equals(AbsoluteOrigin, KINDA_SMALL_NUMBER);
	}
}

bool RpgInventoryUiGeometry::TryResolveAbsolutePoint(
	const FGeometry& Geometry,
	FVector2D LocalPosition,
	FVector2D& OutAbsoluteScreenPosition)
{
	OutAbsoluteScreenPosition = FVector2D::ZeroVector;
	if (!HasUsableGeometry(Geometry) || !IsFiniteVector(LocalPosition))
	{
		return false;
	}

	const FVector2D LocalSize = Geometry.GetLocalSize();
	if (LocalPosition.X < 0.0f ||
		LocalPosition.Y < 0.0f ||
		LocalPosition.X > LocalSize.X ||
		LocalPosition.Y > LocalSize.Y)
	{
		return false;
	}

	const FVector2D AbsolutePosition = Geometry.LocalToAbsolute(LocalPosition);
	if (!IsFiniteVector(AbsolutePosition))
	{
		return false;
	}

	OutAbsoluteScreenPosition = AbsolutePosition;
	return true;
}

bool RpgInventoryUiGeometry::TryResolveAbsoluteCenter(
	const FGeometry& Geometry,
	FVector2D& OutAbsoluteScreenPosition)
{
	return TryResolveAbsolutePoint(
		Geometry,
		Geometry.GetLocalSize() * 0.5f,
		OutAbsoluteScreenPosition);
}

bool RpgInventoryUiGeometry::TryResolvePlayerScreenCenter(
	APlayerController* PlayerController,
	FVector2D& OutAbsoluteScreenPosition)
{
	OutAbsoluteScreenPosition = FVector2D::ZeroVector;
	if (!PlayerController)
	{
		return false;
	}

	return TryResolveAbsoluteCenter(
		UWidgetLayoutLibrary::GetPlayerScreenWidgetGeometry(PlayerController),
		OutAbsoluteScreenPosition);
}

bool RpgInventoryUiGeometry::TryResolveClampedMenuCanvasPosition(
	const FGeometry& CanvasGeometry,
	FVector2D AbsoluteAnchor,
	FVector2D MenuSize,
	FVector2D& OutLocalPosition)
{
	OutLocalPosition = FVector2D::ZeroVector;
	if (!HasUsableGeometry(CanvasGeometry) ||
		!IsFiniteVector(AbsoluteAnchor) ||
		!IsFiniteVector(MenuSize) ||
		MenuSize.X <= KINDA_SMALL_NUMBER ||
		MenuSize.Y <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector2D CanvasSize = CanvasGeometry.GetLocalSize();
	FVector2D LocalPosition = CanvasGeometry.AbsoluteToLocal(AbsoluteAnchor);
	if (!IsFiniteVector(LocalPosition))
	{
		return false;
	}

	LocalPosition.X = FMath::Clamp(
		LocalPosition.X,
		0.0f,
		FMath::Max(0.0f, CanvasSize.X - MenuSize.X));
	LocalPosition.Y = FMath::Clamp(
		LocalPosition.Y,
		0.0f,
		FMath::Max(0.0f, CanvasSize.Y - MenuSize.Y));
	OutLocalPosition = LocalPosition;
	return true;
}
