#include "RpgAnimationThreadingTestTypes.h"

#include "Animation/AnimNodeBase.h"

namespace RpgAnimationThreadingTests
{
/** A real native graph root so multiple move updates can be observed within one engine frame. */
struct FDeltaRecordingRoot final : public FAnimNode_Base
{
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override
	{
		++UpdateCount;
		ElapsedSeconds += Context.GetDeltaTime();
	}

	virtual void Evaluate_AnyThread(FPoseContext& Output) override
	{
		Output.ResetToRefPose();
	}

	int32 UpdateCount = 0;
	float ElapsedSeconds = 0.0f;
};

struct FThreadingTestProxy final : public FRpgAnimInstanceProxy
{
	explicit FThreadingTestProxy(UAnimInstance* InInstance)
		: FRpgAnimInstanceProxy(InInstance)
	{
	}

	virtual FAnimNode_Base* GetCustomRootNode() override { return &Root; }

	FDeltaRecordingRoot Root;
};
}

FAnimInstanceProxy* URpgAnimationThreadingTestInstance::CreateAnimInstanceProxy()
{
	return new RpgAnimationThreadingTests::FThreadingTestProxy(this);
}

int32 URpgAnimationThreadingTestInstance::GetGraphUpdateCount() const
{
	return GetProxyOnGameThread<RpgAnimationThreadingTests::FThreadingTestProxy>().Root.UpdateCount;
}

float URpgAnimationThreadingTestInstance::GetGraphElapsedSeconds() const
{
	return GetProxyOnGameThread<RpgAnimationThreadingTests::FThreadingTestProxy>().Root.ElapsedSeconds;
}

void URpgAnimationThreadingTestInstance::ResetGraphObservations()
{
	RpgAnimationThreadingTests::FDeltaRecordingRoot& Root =
		GetProxyOnGameThread<RpgAnimationThreadingTests::FThreadingTestProxy>().Root;
	Root.UpdateCount = 0;
	Root.ElapsedSeconds = 0.0f;
}
