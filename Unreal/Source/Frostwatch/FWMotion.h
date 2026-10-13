#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "FWMotion.generated.h"

// Keep the existing sequence clock/contact timing, but ease away from the last
// displayed pose. No second sequence evaluation or per-character graph asset.
UCLASS(Transient)
class FROSTWATCH_API UFWMotion : public UAnimSingleNodeInstance
{
    GENERATED_BODY()
public:
    void Transition(UAnimationAsset* Clip,bool Loop,float Seconds);
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
};
