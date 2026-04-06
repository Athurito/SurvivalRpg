#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RpgEquipmentComponent.h"
#include "RpgWeaponPresentationComponent.generated.h"

class AActor;
class APawn;
class UAnimInstance;
class UCameraComponent;
class UCharacterMovementComponent;
class USkeletalMeshComponent;
class USpringArmComponent;
class URpgEquipmentComponent;
class URpgPawnExtensionComponent;

USTRUCT()
struct FRpgWeaponPresentationVisualEntry
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<URpgItemInstance> ItemInstance = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AActor> VisualActor = nullptr;
};

USTRUCT()
struct FRpgPresentationBindings
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<APawn> Pawn = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> Mesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterMovementComponent> MovementComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> CameraComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USpringArmComponent> SpringArmComponent = nullptr;
};

USTRUCT()
struct FRpgPresentationDefaults
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> AnimClass = nullptr;

	UPROPERTY(Transient)
	float MaxWalkSpeed = 600.0f;

	UPROPERTY(Transient)
	bool bOrientRotationToMovement = true;

	UPROPERTY(Transient)
	bool bUseControllerDesiredRotation = false;

	UPROPERTY(Transient)
	float CameraFOV = 90.0f;

	UPROPERTY(Transient)
	FVector SpringArmSocketOffset = FVector::ZeroVector;
};

USTRUCT()
struct FRpgPendingAnimState
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> DesiredAnimClass = nullptr;

	UPROPERTY(Transient)
	bool bPending = false;
};

USTRUCT()
struct FRpgCameraBlendState
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	float StartFOV = 90.0f;

	UPROPERTY(Transient)
	FVector StartSocketOffset = FVector::ZeroVector;

	UPROPERTY(Transient)
	float TargetFOV = 90.0f;

	UPROPERTY(Transient)
	FVector TargetSocketOffset = FVector::ZeroVector;

	UPROPERTY(Transient)
	float AppliedFOV = 90.0f;

	UPROPERTY(Transient)
	FVector AppliedSocketOffset = FVector::ZeroVector;

	UPROPERTY(Transient)
	float Duration = 0.0f;

	UPROPERTY(Transient)
	float Elapsed = 0.0f;

	UPROPERTY(Transient)
	float LastBlendTime = 0.0f;

	UPROPERTY(Transient)
	bool bActive = false;
};

UCLASS(ClassGroup = (Custom), BlueprintType, meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgWeaponPresentationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URpgWeaponPresentationComponent();

	UFUNCTION(BlueprintPure, Category = "Equipment|Presentation")
	FRpgWeaponToolCameraSettings GetActiveCameraSettings() const;

	UFUNCTION(BlueprintPure, Category = "Equipment|Presentation")
	FRpgWeaponToolCharacterSettings GetActiveWeaponToolCharacterSettings() const;

	UFUNCTION(BlueprintCallable, Category = "Equipment|Presentation")
	void ApplyWeaponToolPresentationNotifyAction(ERpgWeaponToolPresentationNotifyAction Action);

	void HandlePawnContextChanged();
	void HandleAbilitySystemInitialized();
	void HandleAbilitySystemUninitialized();

	UPROPERTY(BlueprintAssignable, Category = "Equipment|Presentation")
	FRpgActiveCameraSettingsChangedSignature OnActiveCameraSettingsChanged;

#if WITH_DEV_AUTOMATION_TESTS
	void SetPendingAnimSwitchForTests(bool bPending);
	void SetCameraBlendActiveForTests(bool bActive);
	bool IsPresentationTickEnabledForTests() const { return IsComponentTickEnabled(); }
	int32 GetVisibleWeaponSetIndexForTests() const { return VisibleWeaponSetIndex; }
#endif

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void BindPawnExtension();
	void UnbindEquipmentComponent();
	void BindEquipmentComponent(URpgEquipmentComponent* InEquipmentComponent);
	void HandleEquipmentStateChanged(const FRpgEquipmentStateChangedEvent& Event);
	void SyncFromEquipmentState(bool bAllowMontage, int32 PreviousActiveWeaponSetIndex);
	void RefreshPresentationBindings();
	void ResetPresentationBindings();
	void RestorePresentationDefaults();
	void ApplyActiveWeaponToolCharacterSettings();
	void RefreshVisiblePresentationState();
	void SetVisibleWeaponSetIndex(int32 InVisibleWeaponSetIndex);
	void BroadcastActiveCameraSettingsIfChanged();
	void QueuePendingAnimClassSwitch(TSubclassOf<UAnimInstance> DesiredAnimClass);
	void StartOrUpdateCameraBlend();
	void UpdatePendingAnimClassSwitch();
	void UpdateCameraBlend(float DeltaTime);
	void ApplyCameraBlendAlpha(float BlendAlpha);
	void UpdateTickEnabledState();
	bool ShouldApplyActiveWeaponToolCharacterSettingsToPawn(const APawn* VisualPawn) const;
	bool ShouldApplyVisibleWeaponToolAnimClassToPawn(const APawn* VisualPawn) const;
	bool ShouldApplyVisibleWeaponToolCameraSettingsToPawn(const APawn* VisualPawn) const;
	URpgEquipmentComponent* ResolveEquipmentComponent() const;
	USkeletalMeshComponent* ResolvePresentationMesh(APawn* VisualPawn) const;
	UCharacterMovementComponent* ResolvePresentationMovementComponent(APawn* VisualPawn) const;
	UCameraComponent* ResolvePresentationCameraComponent(APawn* VisualPawn) const;
	USpringArmComponent* ResolvePresentationSpringArmComponent(APawn* VisualPawn) const;
	URpgItemInstance* GetPrimaryPresentationItemForWeaponSet(int32 WeaponSetIndex) const;
	const URpgItemFragment_Visual* GetPrimaryPresentationVisualFragmentForWeaponSet(int32 WeaponSetIndex) const;
	bool WeaponSetUsesPresentationNotify(int32 WeaponSetIndex, bool bUseEquipMontage) const;
	bool PlayPresentationMontageForWeaponSet(int32 WeaponSetIndex, bool bUseEquipMontage) const;
	void RefreshVisuals();
	AActor* FindVisualActorForItem(const URpgItemInstance* ItemInstance) const;
	AActor* FindOrSpawnVisualActor(URpgItemInstance* ItemInstance);
	void DestroyVisualActorForItem(const URpgItemInstance* ItemInstance);
	void DestroyAllVisualActors();

	UPROPERTY(Transient)
	TObjectPtr<URpgEquipmentComponent> EquipmentComponent = nullptr;

	UPROPERTY(Transient)
	FRpgPresentationBindings Bindings;

	UPROPERTY(Transient)
	FRpgPresentationDefaults Defaults;

	UPROPERTY(Transient)
	FRpgPendingAnimState PendingAnimState;

	UPROPERTY(Transient)
	FRpgCameraBlendState CameraBlendState;

	UPROPERTY(Transient)
	TArray<FRpgWeaponPresentationVisualEntry> VisualEntries;

	UPROPERTY(Transient)
	FRpgWeaponToolCameraSettings LastBroadcastCameraSettings;

	UPROPERTY(Transient)
	int32 VisibleWeaponSetIndex = INDEX_NONE;
};
