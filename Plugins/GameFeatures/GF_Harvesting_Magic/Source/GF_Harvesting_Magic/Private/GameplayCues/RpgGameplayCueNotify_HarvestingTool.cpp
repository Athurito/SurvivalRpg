#include "GameplayCues/RpgGameplayCueNotify_HarvestingTool.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Misc/DataValidation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGameplayCueNotify_HarvestingTool)

ARpgGameplayCueNotify_HarvestingTool::ARpgGameplayCueNotify_HarvestingTool(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoAttachToOwner = false;
	bAutoDestroyOnRemove = true;
	bUniqueInstancePerInstigator = false;
	bUniqueInstancePerSourceObject = false;

	ToolMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ToolMesh"));
	SetRootComponent(ToolMeshComponent);
	ToolMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ToolMeshComponent->SetGenerateOverlapEvents(false);
	ToolMeshComponent->SetVisibility(false, true);
}

bool ARpgGameplayCueNotify_HarvestingTool::OnActive_Implementation(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters)
{
	(void)Parameters;
	return ShowToolOnTarget(MyTarget);
}

bool ARpgGameplayCueNotify_HarvestingTool::WhileActive_Implementation(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters)
{
	(void)Parameters;
	return ShowToolOnTarget(MyTarget);
}

bool ARpgGameplayCueNotify_HarvestingTool::OnRemove_Implementation(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters)
{
	(void)MyTarget;
	(void)Parameters;
	if (ToolMeshComponent)
	{
		ToolMeshComponent->SetVisibility(false, true);
		ToolMeshComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	}
	return true;
}

bool ARpgGameplayCueNotify_HarvestingTool::ShowToolOnTarget(AActor* MyTarget)
{
	if (!IsValid(MyTarget) || !ToolMeshComponent || !ToolMesh)
	{
		return false;
	}

	USkeletalMeshComponent* TargetMesh = nullptr;
	if (const ACharacter* Character = Cast<ACharacter>(MyTarget))
	{
		TargetMesh = Character->GetMesh();
	}
	if (!TargetMesh)
	{
		TargetMesh = MyTarget->FindComponentByClass<USkeletalMeshComponent>();
	}
	if (!TargetMesh)
	{
		return false;
	}

	const FName ResolvedSocket = TargetMesh->DoesSocketExist(AttachSocketName)
		? AttachSocketName
		: NAME_None;
	ToolMeshComponent->SetStaticMesh(ToolMesh);
	ToolMeshComponent->AttachToComponent(
		TargetMesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		ResolvedSocket);
	ToolMeshComponent->SetRelativeTransform(ToolRelativeTransform);
	ToolMeshComponent->SetVisibility(true, true);
	return true;
}

#if WITH_EDITOR
EDataValidationResult ARpgGameplayCueNotify_HarvestingTool::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!ToolMesh)
	{
		Context.AddError(NSLOCTEXT(
			"RpgHarvestingToolCue",
			"MissingToolMesh",
			"A harvesting tool GameplayCue requires a cosmetic ToolMesh."));
		Result = EDataValidationResult::Invalid;
	}
	if (AttachSocketName.IsNone())
	{
		Context.AddError(NSLOCTEXT(
			"RpgHarvestingToolCue",
			"MissingAttachSocket",
			"A harvesting tool GameplayCue requires an attachment socket or bone."));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif
