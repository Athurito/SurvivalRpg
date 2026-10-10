#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "CommonButtonBase.h"
#include "CommonUserWidget.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgMvvmListEntryWidgets.generated.h"

class UCommonLazyImage;
class UMVVMViewModelBase;
class UTexture2D;

/**
 * Pooled list entry that hands its list item to one authored, optional manual MVVM source. Concrete rows stay pure
 * Widget Blueprints with read-only bindings; the entry never creates or discovers view models itself.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgMvvmListEntryWidget
	: public UCommonUserWidget
	, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	/** List item currently represented by this pooled entry. */
	UFUNCTION(BlueprintPure, Category = "List Entry")
	UObject* GetEntryItem() const { return EntryItem.Get(); }

	/** Shows a lazily loaded icon in the optional Icon widget, collapsed when the icon is null. Binding target for MVVM. */
	UFUNCTION(BlueprintCallable, Category = "List Entry")
	void SetEntryIcon(TSoftObjectPtr<UTexture2D> InIcon);

	/** Assigns the represented item outside a list view, for example from URpgViewModelEntryBox. */
	void SetEntryItem(UObject* Item);

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnEntryReleased() override;
	virtual void NativeDestruct() override;

	/** Authored MVVM source receiving the list item. Designer data on the concrete row. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "List Entry")
	FName ViewModelSourceName;

	/** Class the list items must have; null accepts any view model. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "List Entry")
	TSubclassOf<UMVVMViewModelBase> ViewModelClass;

	/** Optional icon target for SetEntryIcon. */
	UPROPERTY(BlueprintReadOnly, Category = "List Entry", meta = (BindWidgetOptional))
	TObjectPtr<UCommonLazyImage> Icon = nullptr;

private:
	void AssignEntryItem(UObject* Item);

	TWeakObjectPtr<UObject> EntryItem;
};

/** Button variant of URpgMvvmListEntryWidget for selectable or clickable rows. */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgMvvmListEntryButton
	: public UCommonButtonBase
	, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	/** List item currently represented by this pooled entry. */
	UFUNCTION(BlueprintPure, Category = "List Entry")
	UObject* GetEntryItem() const { return EntryItem.Get(); }

	/** Shows a lazily loaded icon in the optional Icon widget, collapsed when the icon is null. Binding target for MVVM. */
	UFUNCTION(BlueprintCallable, Category = "List Entry")
	void SetEntryIcon(TSoftObjectPtr<UTexture2D> InIcon);

	/** Assigns the represented item outside a list view, for example from URpgViewModelEntryBox. */
	void SetEntryItem(UObject* Item);

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnEntryReleased() override;
	virtual void NativeDestruct() override;

	/** Authored MVVM source receiving the list item. Designer data on the concrete row. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "List Entry")
	FName ViewModelSourceName;

	/** Class the list items must have; null accepts any view model. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "List Entry")
	TSubclassOf<UMVVMViewModelBase> ViewModelClass;

	/** Optional icon target for SetEntryIcon. */
	UPROPERTY(BlueprintReadOnly, Category = "List Entry", meta = (BindWidgetOptional))
	TObjectPtr<UCommonLazyImage> Icon = nullptr;

private:
	void AssignEntryItem(UObject* Item);

	TWeakObjectPtr<UObject> EntryItem;
};
