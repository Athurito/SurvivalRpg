#include "Animation/RpgAnimationRetargetLibrary.h"

#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMetaData.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/Skeleton.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/AssetUserData.h"
#include "ScopedTransaction.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"

bool URpgAnimationRetargetLibrary::ExportPreservedMetadata(UAnimSequence* Sequence, FString& OutMetadata)
{
	OutMetadata.Reset();
	const IAnimationDataModel* Model = Sequence ? Sequence->GetDataModel() : nullptr;
	const UObject* ModelObject = Model ? Cast<UObject>(Model) : nullptr;
	if (!Sequence || !Sequence->GetSkeleton() || !ModelObject || Model->GetNumberOfKeys() < 1)
	{
		return false;
	}

	FString Result = FString::Printf(TEXT("RpgRetargetMetadata/v1\nShape=%d,%d,%d/%d,%.17g\n"),
		Model->GetNumberOfKeys(), Model->GetNumberOfFrames(), Model->GetFrameRate().Numerator,
		Model->GetFrameRate().Denominator, Model->GetPlayLength());
	const auto AppendProperty = [](const UObject* Object, const FProperty* Property, FString& Output)
	{
		Output += Property->GetName() + TEXT("=");
		Property->ExportText_InContainer(0, Output, Object, nullptr, const_cast<UObject*>(Object), PPF_None);
		Output += TEXT("\n");
	};
	// Reflection deliberately bypasses Python's editor-visible-property restrictions.
	static const FName PreservedProperties[] = {
		TEXT("Skeleton"), TEXT("RetargetSource"), TEXT("RetargetSourceAsset"), TEXT("RetargetSourceAssetReferencePose"),
		TEXT("BoneCompressionSettings"), TEXT("CurveCompressionSettings"), TEXT("VariableFrameStrippingSettings"),
		TEXT("bAllowFrameStripping"), TEXT("CompressionErrorThresholdScale"), TEXT("bDoNotOverrideCompression"),
		TEXT("PlatformTargetFrameRate"), TEXT("bEnableRootMotion"), TEXT("bForceRootLock"),
		TEXT("bUseNormalizedRootMotionScale"), TEXT("RootMotionRootLock"), TEXT("RateScale"), TEXT("Interpolation"),
		TEXT("AdditiveAnimType"), TEXT("RefPoseType"), TEXT("RefPoseSeq"), TEXT("RefFrameIndex"),
		TEXT("PreviewSkeletalMesh"), TEXT("PreviewPoseAsset"), TEXT("Notifies"), TEXT("AnimNotifyTracks"),
		TEXT("AuthoredSyncMarkers"), TEXT("AssetUserData"), TEXT("MetaData"), TEXT("AssetImportData")
	};
	for (const FName Name : PreservedProperties)
	{
		const FProperty* Property = Sequence->GetClass()->FindPropertyByName(Name);
		if (!Property)
		{
			return false;
		}
		AppendProperty(Sequence, Property, Result);
	}

	// Use the interface: UE 5.8 may store curves in either AnimDataModel or AnimationSequencerDataModel.
	Result += TEXT("Curves=");
	FAnimationCurveData::StaticStruct()->ExportText(Result, &Model->GetCurveData(), nullptr, Sequence, PPF_None, nullptr);
	if (const FProperty* CurveMetadata = ModelObject->GetClass()->FindPropertyByName(TEXT("CurveIdentifierToMetaData")))
	{
		AppendProperty(ModelObject, CurveMetadata, Result);
	}
	for (const FAnimatedBoneAttribute& Attribute : Model->GetAttributes())
	{
		Result += TEXT("\nAttribute=");
		FAnimatedBoneAttribute::StaticStruct()->ExportText(Result, &Attribute, nullptr, Sequence, PPF_None, nullptr);
		const UScriptStruct* ValueType = Attribute.Curve.GetScriptStruct();
		if (!ValueType)
		{
			return false;
		}
		// Attribute key values use native storage, so exporting the wrapper struct alone loses them.
		for (const FAttributeKey& Key : Attribute.Curve.GetConstRefOfKeys())
		{
			const uint8* Value = Key.GetValuePtr<uint8>();
			if (!Value || !FMath::IsFinite(Key.Time))
			{
				return false;
			}
			Result += FString::Printf(TEXT("\nAttributeKey=%.9g,"), Key.Time);
			ValueType->ExportText(Result, Value, nullptr, Sequence, PPF_None, nullptr);
		}
	}

	// Object references in events export their identity only. Include the owned settings as well.
	TArray<UObject*> Objects;
	for (const FAnimNotifyEvent& Event : Sequence->Notifies)
	{
		Objects.Add(Event.Notify);
		Objects.Add(Event.NotifyStateClass);
	}
	for (UAnimMetaData* Metadata : Sequence->GetMetaData()) { Objects.Add(Metadata); }
	if (const TArray<UAssetUserData*>* UserData = Sequence->GetAssetUserDataArray())
	{
		for (UAssetUserData* Item : *UserData) { Objects.Add(Item); }
	}
	Objects.Add(Sequence->AssetImportData);
	TSet<const UObject*> Visited;
	TArray<FString> ObjectExports;
	for (int32 Index = 0; Index < Objects.Num(); ++Index)
	{
		UObject* Object = Objects[Index];
		if (!Object || Visited.Contains(Object)) { continue; }
		Visited.Add(Object);
		FString ObjectText = TEXT("\nObject=") + Object->GetPathName() + TEXT("\n");
		for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
		{
			if (!It->HasAnyPropertyFlags(CPF_Transient | CPF_SkipSerialization | CPF_Deprecated))
			{
				AppendProperty(Object, *It, ObjectText);
			}
		}
		ObjectExports.Add(MoveTemp(ObjectText));
		GetObjectsWithOuter(Object, Objects, EGetObjectsFlags::None);
	}
	ObjectExports.Sort();
	for (const FString& ObjectText : ObjectExports) { Result += ObjectText; }
	OutMetadata = MoveTemp(Result);
	return true;
}

bool URpgAnimationRetargetLibrary::CopyRetargetedBoneTracks(
	UAnimSequence* Candidate, UAnimSequence* Destination, FString& OutError)
{
	OutError.Reset();
	if (!Candidate || !Destination || Candidate == Destination || !Candidate->GetSkeleton() ||
		Candidate->GetSkeleton() != Destination->GetSkeleton())
	{
		OutError = TEXT("Retarget candidates require distinct sequences with the same valid skeleton.");
		return false;
	}

	const IAnimationDataModel* SourceModel = Candidate->GetDataModel();
	const IAnimationDataModel* TargetModel = Destination->GetDataModel();
	if (!SourceModel || !TargetModel || SourceModel->GetNumberOfKeys() < 2 ||
		SourceModel->GetNumberOfKeys() != TargetModel->GetNumberOfKeys() ||
		SourceModel->GetFrameRate() != TargetModel->GetFrameRate())
	{
		OutError = TEXT("Retarget candidates must preserve the destination sample count and frame rate.");
		return false;
	}

	TArray<FName> SourceNames;
	TArray<FName> TargetNames;
	SourceModel->GetBoneTrackNames(SourceNames);
	TargetModel->GetBoneTrackNames(TargetNames);
	const FReferenceSkeleton& ReferenceSkeleton = Destination->GetSkeleton()->GetReferenceSkeleton();
	if (ReferenceSkeleton.GetNum() == 0 || SourceNames.IsEmpty() || SourceNames.Num() != TargetNames.Num())
	{
		OutError = TEXT("Retarget candidates must preserve the complete bone-track domain.");
		return false;
	}
	const FName RootBone = ReferenceSkeleton.GetBoneName(0);
	TMap<FName, TArray<FTransform>> Tracks;
	for (FName BoneName : SourceNames)
	{
		if (!TargetNames.Contains(BoneName) || ReferenceSkeleton.FindBoneIndex(BoneName) == INDEX_NONE)
		{
			OutError = FString::Printf(TEXT("Incompatible retarget bone track: %s"), *BoneName.ToString());
			return false;
		}
		if (BoneName == RootBone)
		{
			continue;
		}
		TArray<FTransform>& Keys = Tracks.Add(BoneName);
		SourceModel->GetBoneTrackTransforms(BoneName, Keys);
		if (Keys.Num() != SourceModel->GetNumberOfKeys() ||
			Keys.ContainsByPredicate([](const FTransform& Key) { return !Key.IsValid(); }))
		{
			OutError = FString::Printf(TEXT("Invalid retarget keys for %s"), *BoneName.ToString());
			return false;
		}
	}

	const FScopedTransaction Transaction(NSLOCTEXT("RpgAnimationRetarget", "CopyTracks", "Apply retargeted bone tracks"));
	Destination->Modify();
	IAnimationDataController& Controller = Destination->GetController();
	const IAnimationDataController::FScopedBracket Bracket(
		Controller, NSLOCTEXT("RpgAnimationRetarget", "CopyTracksBracket", "Apply retargeted bone tracks"));
	for (const TPair<FName, TArray<FTransform>>& Track : Tracks)
	{
		TArray<FVector3f> Positions;
		TArray<FQuat4f> Rotations;
		TArray<FVector3f> Scales;
		Positions.Reserve(Track.Value.Num());
		Rotations.Reserve(Track.Value.Num());
		Scales.Reserve(Track.Value.Num());
		for (const FTransform& Key : Track.Value)
		{
			Positions.Add(FVector3f(Key.GetTranslation()));
			Rotations.Add(FQuat4f(Key.GetRotation()));
			Scales.Add(FVector3f(Key.GetScale3D()));
		}
		if (!Controller.SetBoneTrackKeys(Track.Key, Positions, Rotations, Scales))
		{
			OutError = FString::Printf(TEXT("Could not apply retarget keys for %s"), *Track.Key.ToString());
			return false;
		}
	}
	return true;
}
