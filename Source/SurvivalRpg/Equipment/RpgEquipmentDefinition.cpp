#include "RpgEquipmentDefinition.h"

#include "RpgEquipmentInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgEquipmentDefinition)

URpgEquipmentDefinition::URpgEquipmentDefinition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstanceType = URpgEquipmentInstance::StaticClass();
}
