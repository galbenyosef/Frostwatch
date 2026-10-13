#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PhysicsVolume.h"
#include "FWWaterVolume.generated.h"
class UBoxComponent;
UCLASS()
class FROSTWATCH_API AFWWaterVolume : public APhysicsVolume
{
    GENERATED_BODY()
public:
    AFWWaterVolume();
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UBoxComponent> WaterBounds;
    virtual bool IsOverlapInVolume(const USceneComponent& Component) const override;
    float SurfaceZ() const;
};
