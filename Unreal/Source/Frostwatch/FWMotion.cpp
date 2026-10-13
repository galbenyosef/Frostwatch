#include "FWMotion.h"
#include "Animation/AnimSingleNodeInstanceProxy.h"

namespace
{
struct FFWTransitionProxy final : FAnimSingleNodeInstanceProxy
{
    explicit FFWTransitionProxy(UAnimInstance* Instance):FAnimSingleNodeInstanceProxy(Instance){}
    TArray<FTransform> LastPose,SourcePose;
    uint16 BoneSerial=0;
    float Elapsed=0,Duration=0;

    void Transition(float Seconds)
    {
        SourcePose=LastPose;
        Duration=SourcePose.IsEmpty()?0:FMath::Max(0.f,Seconds);
        Elapsed=0;
    }
    virtual void PreUpdate(UAnimInstance* Instance,float Dt) override
    {
        FAnimSingleNodeInstanceProxy::PreUpdate(Instance,Dt);
        Elapsed=FMath::Min(Duration,Elapsed+FMath::Max(0.f,Dt));
    }
    virtual bool Evaluate(FPoseContext& Output) override
    {
        const bool Result=FAnimSingleNodeInstanceProxy::Evaluate(Output);
        const uint16 Serial=Output.Pose.GetBoneContainer().GetSerialNumber();
        const int32 Count=Output.Pose.GetNumBones();
        // A changed LOD/bone mapping invalidates the compact-pose snapshot.
        if(Serial!=BoneSerial||LastPose.Num()!=Count)
        {
            SourcePose.Reset();Duration=0;BoneSerial=Serial;
        }
        const float T=Duration>0?FMath::Clamp(Elapsed/Duration,0.f,1.f):1.f;
        const float Alpha=T*T*(3.f-2.f*T);
        LastPose.SetNumUninitialized(Count);
        for(int32 I=0;I<Count;++I)
        {
            auto& Bone=Output.Pose[FCompactPoseBoneIndex(I)];
            if(SourcePose.Num()==Count&&Alpha<1.f)
            {
                FTransform Blended;Blended.Blend(SourcePose[I],Bone,Alpha);
                Blended.NormalizeRotation();Bone=Blended;
            }
            LastPose[I]=Bone;
        }
        return Result;
    }
};
}
FAnimInstanceProxy* UFWMotion::CreateAnimInstanceProxy(){return new FFWTransitionProxy(this);}
void UFWMotion::Transition(UAnimationAsset* Clip,bool Loop,float Seconds)
{
    // GetProxyOnGameThread synchronises any outstanding parallel evaluation.
    GetProxyOnGameThread<FFWTransitionProxy>().Transition(Seconds);
    SetAnimationAsset(Clip,Loop,1.f);
    PlayAnim(Loop,1.f,0.f);
}
