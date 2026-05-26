#include "RpgEnemyHealthBarComponent.h"

#include "EnemyVitalsViewmodel.h"
#include "View/MVVMView.h"
#include "SurvivalRpg/SurvivalRpg.h"

URpgEnemyHealthBarComponent::URpgEnemyHealthBarComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetWidgetSpace(EWidgetSpace::Screen);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetDrawSize(FVector2D(120.0f, 12.0f));
	SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
}

void URpgEnemyHealthBarComponent::InitWidget()
{
	Super::InitWidget();

	UUserWidget* HealthBarWidget = GetUserWidgetObject();
	if (!HealthBarWidget)
	{
		return;
	}

	if (!EnemyVitalsViewmodel)
	{
		EnemyVitalsViewmodel = NewObject<UEnemyVitalsViewmodel>(this);
	}

	EnemyVitalsViewmodel->BindToActor(GetOwner());

	TScriptInterface<INotifyFieldValueChanged> ViewmodelInterface;
	ViewmodelInterface.SetObject(EnemyVitalsViewmodel);
	ViewmodelInterface.SetInterface(EnemyVitalsViewmodel.Get());

	if (UMVVMView* View = HealthBarWidget->GetExtension<UMVVMView>())
	{
		if (!View->SetViewModelByClass(ViewmodelInterface))
		{
			UE_LOG(LogRpg, Warning, TEXT("Enemy health bar widget [%s] has no unique PlayerVitals-compatible MVVM source."), *GetNameSafe(HealthBarWidget));
		}
	}
	else
	{
		UE_LOG(LogRpg, Warning, TEXT("Enemy health bar widget [%s] has no MVVM view configured."), *GetNameSafe(HealthBarWidget));
	}
}

void URpgEnemyHealthBarComponent::OnUnregister()
{
	if (EnemyVitalsViewmodel)
	{
		EnemyVitalsViewmodel->UnbindFromActor();
	}

	Super::OnUnregister();
}
