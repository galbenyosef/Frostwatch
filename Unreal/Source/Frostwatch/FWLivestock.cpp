#include "FWLivestock.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"

AFWLivestock::AFWLivestock()
{
    PrimaryActorTick.bCanEverTick=true;
    Collision=CreateDefaultSubobject<UCapsuleComponent>(TEXT("Collision"));RootComponent=Collision;
    Collision->InitCapsuleSize(35,50);Collision->SetCollisionProfileName(TEXT("Pawn"));
    Body=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));Body->SetupAttachment(RootComponent);
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Body->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    Body->bEnableUpdateRateOptimizations=true;
}
bool AFWLivestock::Ground(FVector& Position) const
{
    FHitResult Hit;FCollisionQueryParams Q;Q.AddIgnoredActor(this);
    if(!GetWorld()->LineTraceSingleByObjectType(Hit,Position+FVector(0,0,180),Position-FVector(0,0,260),FCollisionObjectQueryParams(ECC_WorldStatic),Q)||Hit.ImpactNormal.Z<.82f)return false;
    const float Z=Hit.ImpactPoint.Z+Collision->GetScaledCapsuleHalfHeight();
    if(FMath::Abs(Z-Position.Z)>90)return false;
    Position.Z=Z;return true;
}
void AFWLivestock::BeginPlay()
{
    Super::BeginPlay();Home=GetActorLocation();Goal=Home;
    Pause=FMath::FRandRange(2.f,9.f);Activity=TEXT("Grazing");Play(Feed?Feed:Idle,FeedRate);
}
void AFWLivestock::Play(UAnimSequence* Clip,float Rate)
{
    if(!Clip)return;
    if(Current!=Clip){Current=Clip;Body->PlayAnimation(Clip,true);}
    Body->SetPlayRate(Rate);
}
void AFWLivestock::Tend(float Dt)
{
    const float Now=GetWorld()->GetTimeSeconds();
    if(TendedUntil<Now)++TendingVisits;
    TendedUntil=Now+2;Pause=FMath::Max(Pause,3.f);Walking=false;
    Activity=TEXT("Being fed");Play(Feed?Feed:Idle,FeedRate);
}
void AFWLivestock::Tick(float Dt)
{
    Super::Tick(Dt);
    const APawn* Player=UGameplayStatics::GetPlayerPawn(this,0);
    const bool Far=!Player||FVector::DistSquared(Player->GetActorLocation(),GetActorLocation())>9000.f*9000.f;
    SetActorTickInterval(Far?1.f:0.f);
    if(Far){Walking=false;Pause=FMath::Max(Pause,2.f);Play(Idle);return;}
    const float Now=GetWorld()->GetTimeSeconds();
    if(TendedUntil>Now){Activity=TEXT("Being fed");Play(Feed?Feed:Idle,FeedRate);return;}
    if(Pause>0){Pause-=Dt;Activity=TEXT("Grazing");Play(Feed?Feed:Idle,FeedRate);return;}
    if(!Walking)
    {
        const float Angle=FMath::FRand()*2*PI,Radius=FMath::Sqrt(FMath::FRand())*RoamRadius;
        Goal=Home+FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,0);
        if(!Ground(Goal)){Pause=3;return;}
        Walking=true;
    }
    FVector Delta=Goal-GetActorLocation();Delta.Z=0;
    if(Delta.Size()<25){Walking=false;Pause=FMath::FRandRange(5.f,14.f);return;}
    SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(),FRotator(0,Delta.Rotation().Yaw,0),Dt,70));
    if(FVector::DotProduct(GetActorForwardVector(),Delta.GetSafeNormal())<.8f){Play(Idle);return;}
    // Bound the movement step after a distant update; never teleport through a fence.
    FVector Next=GetActorLocation()+GetActorForwardVector()*FMath::Min(Dt,.08f)*WalkSpeed;
    if(!Ground(Next)){Walking=false;Pause=3;return;}
    FHitResult Hit;SetActorLocation(Next,true,&Hit);
    if(Hit.bBlockingHit){Walking=false;Pause=FMath::FRandRange(2.f,5.f);Play(Idle);return;}
    Activity=TEXT("Wandering");Play(Walk?Walk:Idle,WalkRate);
}
