#include "FWWaterVolume.h"
#include "Components/BoxComponent.h"
#include "Components/BrushComponent.h"
AFWWaterVolume::AFWWaterVolume()
{
    GetBrushComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WaterBounds=CreateDefaultSubobject<UBoxComponent>(TEXT("WaterBounds"));SetRootComponent(WaterBounds);
    WaterBounds->InitBoxExtent(FVector(1100,1200,450));
    WaterBounds->SetCollisionProfileName(TEXT("OverlapAllDynamic"));WaterBounds->SetGenerateOverlapEvents(true);
    bWaterVolume=true;bPhysicsOnContact=false;Priority=5;FluidFriction=.6f;
}
float AFWWaterVolume::SurfaceZ() const{return GetActorLocation().Z+WaterBounds->GetScaledBoxExtent().Z;}
bool AFWWaterVolume::IsOverlapInVolume(const USceneComponent& Component) const
{
    const FVector P=GetActorTransform().InverseTransformPosition(Component.GetComponentLocation());
    const auto E=WaterBounds->GetUnscaledBoxExtent();
    const float Angle=FMath::Atan2(P.Y/1130.f,P.X/1040.f),Wave=1+.05f*FMath::Sin(Angle*3)+.03f*FMath::Sin(Angle*7);
    return FMath::Abs(P.Z)<E.Z&&FMath::Square(P.X/(1040*Wave))+FMath::Square(P.Y/(1130*Wave))<1;
}
