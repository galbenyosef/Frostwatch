#include "FWHorse.h"
#include "FWGame.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

AFWHorse::AFWHorse()
{
    PrimaryActorTick.bCanEverTick=true;Body=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Horse"));SetRootComponent(Body);Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void AFWHorse::BeginPlay()
{
    Super::BeginPlay();StartTransform=GetActorTransform();
    Body->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPose;
    for(auto Mesh:Parts){auto* C=NewObject<USkeletalMeshComponent>(this);C->SetupAttachment(Body);C->SetSkeletalMeshAsset(Mesh);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->RegisterComponent();C->SetLeaderPoseComponent(Body);}
    if(Cart)
    {
        auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));auto* Wood=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Environment/Materials/M_WeatheredTimber_v2.M_WeatheredTimber_v2"));
        auto Part=[&](FVector L,FVector Scale){auto* C=NewObject<UStaticMeshComponent>(this);C->SetupAttachment(Body);C->SetStaticMesh(Cube);if(Wood)C->SetMaterial(0,Wood);C->SetRelativeLocation(L);C->SetRelativeScale3D(Scale);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->RegisterComponent();return C;};
        Part(FVector(0,310,65),FVector(1.7,2.1,.16));Part(FVector(-90,310,95),FVector(.12,2.1,.55));Part(FVector(90,310,95),FVector(.12,2.1,.55));Part(FVector(0,415,95),FVector(1.9,.12,.55));
        Part(FVector(-65,120,70),FVector(.08,2.5,.08));Part(FVector(65,120,70),FVector(.08,2.5,.08));
        auto* Wheel=CartWheelMesh?CartWheelMesh.Get():LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
        if(Wheel)for(int Side:{-1,1})for(float Along:{245.f,365.f})
        {
            auto* Pivot=NewObject<USceneComponent>(this);Pivot->SetupAttachment(Body);Pivot->SetRelativeLocation(FVector(Side*103,Along,44));Pivot->RegisterComponent();WheelPivots.Add(Pivot);
            auto* C=NewObject<UStaticMeshComponent>(this);C->SetupAttachment(Pivot);C->SetStaticMesh(Wheel);
            const FVector Scale=CartWheelMesh?FVector(42.f/FMath::Max(1.f,Wheel->GetBounds().BoxExtent.GetMax())):FVector(.84,.84,.12);
            const FRotator Rotation=CartWheelMesh?CartWheelRotation:FRotator(90,0,0);
            C->SetRelativeScale3D(Scale);C->SetRelativeRotation(Rotation);
            C->SetRelativeLocation(Rotation.RotateVector(-Wheel->GetBounds().Origin*Scale));
            if(!CartWheelMesh&&Wood)C->SetMaterial(0,Wood);
            C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->RegisterComponent();
        }
    }
    Body->PlayAnimation(Idle,true);CurrentClip=Idle;
    PreviousWheelPosition=GetActorLocation();
}
bool AFWHorse::Mount(AFWCharacter* P)
{
    if(Rider||P->MountedHorse||!P->CanAct())return false;
    Rider=P;P->MountedHorse=this;P->UnCrouch();P->Blocking=false;P->GetMesh()->SetVisibility(false);P->GetCapsuleComponent()->SetCapsuleRadius(Cart?100:60);P->Camera->SetRelativeLocation(Cart?FVector(-260,0,90):FVector(0,0,145));P->Sword->SetVisibility(false);P->Shield->SetVisibility(false);P->Notify(Cart?TEXT("Cart mounted. W/S drive, A/D steer, Shift trot, E dismount."):TEXT("Horse mounted. W/S ride, A/D steer, Shift gallop, E dismount."));return true;
}
bool AFWHorse::Dismount(bool Force)
{
    if(!Rider)return true;auto* P=Rider.Get();FVector Safe;bool Found=false;
    for(FVector Side:{P->GetActorRightVector(),-P->GetActorRightVector(),-P->GetActorForwardVector(),P->GetActorForwardVector()})
    {
        FVector Candidate=P->GetActorLocation()+Side*190;FHitResult Hit;FCollisionQueryParams Q;Q.AddIgnoredActor(this);Q.AddIgnoredActor(P);
        if(!GetWorld()->LineTraceSingleByChannel(Hit,Candidate+FVector(0,0,250),Candidate-FVector(0,0,400),ECC_Visibility,Q))continue;
        Candidate=Hit.ImpactPoint+FVector(0,0,92);
        if(!GetWorld()->OverlapBlockingTestByChannel(Candidate,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(34,88),Q)){Safe=Candidate;Found=true;break;}
    }
    if(!Found&&!Force){P->Notify(TEXT("Move to open ground before dismounting."));return false;}
    if(Found)P->SetActorLocation(Safe);P->GetCharacterMovement()->StopMovementImmediately();P->MountedHorse=nullptr;Rider=nullptr;
    P->GetCapsuleComponent()->SetCapsuleSize(34,88);P->Camera->SetRelativeLocation(FVector(0,0,66));P->GetMesh()->SetVisibility(P->AuthoredView);P->Sword->SetVisibility(!P->AuthoredView);P->Shield->SetVisibility(!P->AuthoredView);P->Save();return true;
}
void AFWHorse::Restore(AFWCharacter* P,bool Fresh)
{
    Dismount(true);SetActorTransform(StartTransform);Energy=100;if(Fresh)return;
    for(const auto& V:P->Expedition->ParkedVehicles)if(V.Identity==Identity)
    {
        FVector L=V.Position;FHitResult Hit;FCollisionQueryParams Q;Q.AddIgnoredActor(this);Q.AddIgnoredActor(P);
        if(L.Z==0&&GetWorld()->LineTraceSingleByChannel(Hit,L+FVector(0,0,25000),L-FVector(0,0,10000),ECC_Visibility,Q))L.Z=Hit.ImpactPoint.Z;
        SetActorLocation(L);SetActorRotation(FRotator(0,V.Yaw,0));Energy=V.Energy;break;
    }
}
void AFWHorse::Tick(float Dt)
{
    Super::Tick(Dt);UAnimSequence* Clip=Idle;
    if(Rider)
    {
        SetActorLocation(Rider->GetActorLocation()-FVector(0,0,Rider->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));SetActorRotation(FRotator(0,Rider->GetActorRotation().Yaw+90,0));
        const bool Moving=Rider->GetVelocity().Size2D()>30;const bool Fast=Moving&&Rider->Sprinting&&Energy>1;
        Energy=FMath::Clamp(Energy+Dt*(Fast?-7:Moving?2:9),0.f,100.f);Clip=Moving?(Fast?Gallop.Get():Walk.Get()):Idle.Get();
    }
    else Energy=FMath::Min(100.f,Energy+Dt*9);
    const FVector WheelTravel=GetActorLocation()-PreviousWheelPosition;PreviousWheelPosition=GetActorLocation();
    // Roll around the axle in cart space; ignore teleports and restored save positions.
    if(Cart&&Rider&&WheelTravel.Size2D()<1000)
    {
        const float Direction=FVector::DotProduct(WheelTravel,GetActorRightVector())>=0?1.f:-1.f;
        WheelAngle=FMath::Fmod(WheelAngle+Direction*FMath::RadiansToDegrees(WheelTravel.Size2D()/(42.f*GetActorScale3D().X)),360.f);
        for(auto Pivot:WheelPivots)if(Pivot)Pivot->SetRelativeRotation(FRotator(0,0,WheelAngle));
    }
    if(Clip&&Clip!=CurrentClip){Body->PlayAnimation(Clip,true);CurrentClip=Clip;}
    if(Rider&&Clip!=Idle)Body->SetPlayRate(FMath::Clamp(Rider->GetVelocity().Size2D()/(Clip==Gallop?1300.f:400.f),.5f,1.65f));
    else Body->SetPlayRate(1.f);
}
