#pragma once

#include "Components/DynamicEntryBoxBase.h"
#include "CoreMinimal.h"

#include "RpgViewModelEntryBox.generated.h"

/**
 * Non-virtualized box that shows one authored entry widget per view model, for short lists such as tooltip stat rows
 * or HUD notifications where a scrolling list view would be the wrong tool.
 *
 * Entries receive their item through IUserObjectListEntry, so URpgMvvmListEntryWidget rows work unchanged. When items
 * are only removed or appended, surviving entries keep their widget and its presentation state.
 */
UCLASS(meta = (DisplayName = "ViewModel Entry Box"))
class SURVIVALRPG_API URpgViewModelEntryBox : public UDynamicEntryBoxBase
{
	GENERATED_BODY()

public:
	/** Shows exactly Items, in order. Binding target for MVVM view-model arrays. */
	UFUNCTION(BlueprintCallable, Category = "ViewModel Entry Box")
	void SetViewModelItems(const TArray<UObject*>& Items);

	/** Removes every entry. */
	UFUNCTION(BlueprintCallable, Category = "ViewModel Entry Box")
	void ClearViewModelItems();

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
	virtual void ValidateCompiledDefaults(class IWidgetCompilerLog& CompileLog) const override;
#endif

protected:
	/** Authored row widget; it must implement UserObjectListEntry. Designer data. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EntryLayout", meta = (MustImplement = "/Script/UMG.UserObjectListEntry"))
	TSubclassOf<UUserWidget> EntryWidgetClass;

private:
	void RemoveEntryAt(int32 Index);
	void AppendEntry(UObject* Item);

	/** Item shown by each entry, in the same order as GetAllEntries. */
	TArray<TWeakObjectPtr<UObject>> EntryItems;
};
