#include "RpgEnemyVitalsIndicatorWidget.h"

#include "EnemyVitalsViewmodel.h"
#include "SurvivalRpg/SurvivalRpg.h"
#include "SurvivalRpg/UI/IndicatorSystem/IndicatorDescriptor.h"
#include "View/MVVMView.h"

void URpgEnemyVitalsIndicatorWidget::BindIndicator_Implementation(UIndicatorDescriptor* Indicator)
{
	AActor* ObservedActor = Indicator ? Cast<AActor>(Indicator->GetDataObject()) : nullptr;
	if (!ObservedActor)
	{
		return;
	}

	if (!EnemyVitalsViewmodel)
	{
		EnemyVitalsViewmodel = NewObject<UEnemyVitalsViewmodel>(this);
	}

	EnemyVitalsViewmodel->BindToActor(ObservedActor);

	TScriptInterface<INotifyFieldValueChanged> ViewmodelInterface;
	ViewmodelInterface.SetObject(EnemyVitalsViewmodel);
	ViewmodelInterface.SetInterface(EnemyVitalsViewmodel.Get());

	if (UMVVMView* View = GetExtension<UMVVMView>())
	{
		if (!View->SetViewModelByClass(ViewmodelInterface))
		{
			UE_LOG(LogRpg, Warning, TEXT("Enemy indicator widget [%s] has no unique PlayerVitals-compatible MVVM source."), *GetNameSafe(this));
		}
	}
	else
	{
		UE_LOG(LogRpg, Warning, TEXT("Enemy indicator widget [%s] has no MVVM view configured."), *GetNameSafe(this));
	}
}

void URpgEnemyVitalsIndicatorWidget::UnbindIndicator_Implementation(const UIndicatorDescriptor* Indicator)
{
	if (EnemyVitalsViewmodel)
	{
		EnemyVitalsViewmodel->UnbindFromActor();
	}
}
