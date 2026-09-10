// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgAnimInstanceTestTypes.h"

#include "Animation/AnimInstanceProxy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgAnimInstanceTestTypes)

namespace
{
	struct FRpgTimingTestAnimInstanceProxy final : public FAnimInstanceProxy
	{
		explicit FRpgTimingTestAnimInstanceProxy(UAnimInstance* Instance)
			: FAnimInstanceProxy(Instance)
		{
		}

		int32 GraphUpdateCount = 0;
		float GraphElapsedSeconds = 0.0f;

	protected:
		virtual void UpdateAnimationNode(const FAnimationUpdateContext& Context) override
		{
			// NativeThreadSafeUpdateAnimation is itself limited to once per world frame.
			// The graph dispatch point observes every independently consumed movement delta.
			++GraphUpdateCount;
			GraphElapsedSeconds += Context.GetDeltaTime();
		}
	};
}

bool URpgAnimInstanceTestInstance::CanRunParallelWork() const
{
	return bUseRpgTimingGuard ? Super::CanRunParallelWork() : UAnimInstance::CanRunParallelWork();
}

FAnimInstanceProxy* URpgAnimInstanceTestInstance::CreateAnimInstanceProxy()
{
	return new FRpgTimingTestAnimInstanceProxy(this);
}

void URpgAnimInstanceTestInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete static_cast<FRpgTimingTestAnimInstanceProxy*>(InProxy);
}

int32 URpgAnimInstanceTestInstance::GetGraphUpdateCount()
{
	return GetProxyOnGameThread<FRpgTimingTestAnimInstanceProxy>().GraphUpdateCount;
}

float URpgAnimInstanceTestInstance::GetGraphElapsedSeconds()
{
	return GetProxyOnGameThread<FRpgTimingTestAnimInstanceProxy>().GraphElapsedSeconds;
}
