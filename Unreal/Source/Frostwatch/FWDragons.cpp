#include "FWGame.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

void AFWEnemy::SetupDragon()
{
    GetCharacterMovement()->SetMovementMode(MOVE_Flying);GetCharacterMovement()->MaxFlySpeed=280;
    auto* Plane=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane"));
    for(int I=0;I<14;I++)for(int Reflected=0;Reflected<2;Reflected++)
    {
        auto* C=NewObject<UStaticMeshComponent>(this);C->SetupAttachment(GetRootComponent());C->SetStaticMesh(Plane);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCastShadow(false);C->RegisterComponent();C->SetVisibility(false);
        if(BreathMaterial){auto* M=UMaterialInstanceDynamic::Create(BreathMaterial,this);M->SetVectorParameterValue(TEXT("Tint"),Reflected?FLinearColor(.2f,4,9):Identity==TEXT("rimefang")?FLinearColor(.6f,3,8):FLinearColor(9,1.3f,.08f));C->SetMaterial(0,M);}
        if(Reflected)ReflectedJet.Add(C);else BreathJet.Add(C);
    }
}
FVector AFWEnemy::BreathOrigin() const
{
    const FName Jaw=GetMesh()->GetBoneIndex(TEXT("MOUNTAIN_DRAGON_-Head"))!=INDEX_NONE?TEXT("MOUNTAIN_DRAGON_-Head"):TEXT("jaw_upper");
    return GetMesh()->GetBoneIndex(Jaw)!=INDEX_NONE?GetMesh()->GetBoneLocation(Jaw)+GetActorForwardVector()*55:GetActorLocation()+FVector(0,0,250);
}
void AFWEnemy::TickDragon(float Dt,AFWCharacter* P)
{
    const FVector To=P->GetActorLocation()-GetActorLocation();const float Distance=To.Size2D();
    ReflectFlash=FMath::Max(0.f,ReflectFlash-Dt);Recovery=FMath::Max(0.f,Recovery-Dt);
    const FVector Muzzle=BreathOrigin();
    if(BreathTime>0)
    {
        BreathTime-=Dt;BreathHitClock-=Dt;
        if(BreathHitClock<=0)
        {
            BreathHitClock=.15f;FVector Delta=P->Camera->GetComponentLocation()-Muzzle;const float Along=FVector::DotProduct(Delta,BreathDirection);const float Away=(Delta-BreathDirection*Along).Size();
            FHitResult Hit;FCollisionQueryParams Q;Q.AddIgnoredActor(this);GetWorld()->LineTraceSingleByChannel(Hit,Muzzle,P->Camera->GetComponentLocation(),ECC_Visibility,Q);
            if(Along>0&&Along<1900&&Away<120+Along*.13f&&(!Hit.bBlockingHit||Hit.GetActor()==P)&&P->Invulnerability<=0)
            {
                const bool Reflect=P->HasFlag(TEXT("mirror_shield"))&&P->Blocking&&P->Stamina>=6&&FVector::DotProduct(P->GetActorForwardVector(),(GetActorLocation()-P->GetActorLocation()).GetSafeNormal2D())>.88f;
                if(Reflect){P->Stamina=FMath::Max(0.f,P->Stamina-3.6f);Health-=24;ReflectFlash=.2f;P->HitFeedback=.2f;P->Notify(TEXT("MIRROR: FIRE RETURNED"),.3f);if(Health<=0){Die(FVector::ZeroVector);return;}}
                else {P->Health=FMath::Max(0.f,P->Health-(P->HasFlag(TEXT("lute_done"))?3.5f:7));P->HurtFlash=.45f;if(P->Health<=0){P->SetScreen(TEXT("Death"));return;}}
            }
        }
    }
    else if(Windup>0)
    {
        Windup-=Dt;
        if(Windup<=0){BreathTime=2.2f;BreathHitClock=0;BreathDirection=(P->Camera->GetComponentLocation()-Muzzle).GetSafeNormal();Recovery=5.8f;}
    }
    else
    {
        Alerted=Distance<4800;
        if(Alerted)
        {
            SetActorRotation(FMath::RInterpTo(GetActorRotation(),FRotator(0,To.Rotation().Yaw,0),Dt,1.8f));
            if(Distance>1200&&FVector::Dist2D(GetActorLocation(),Home)<4200){AddMovementInput(To.GetSafeNormal2D());Play(Walk,true);}
            else if(Recovery<=0){Windup=1.6f;Play(Attack,false);P->Notify(TEXT("DRAGON BREATH: face it with the Mirror Shield, or move behind stone."),2);}
            else Play(Idle,true);
        }
        else {Play(Idle,true);if(FVector::Dist2D(GetActorLocation(),Home)>100)AddMovementInput((Home-GetActorLocation()).GetSafeNormal2D());}
    }
    for(int I=0;I<BreathJet.Num();I++)
    {
        const FVector A=Muzzle+BreathDirection*(40+I*130);auto* C=BreathJet[I].Get();C->SetVisibility(BreathTime>0);C->SetWorldLocation(A);C->SetWorldRotation(FRotationMatrix::MakeFromZ(P->Camera->GetComponentLocation()-A).Rotator());C->SetWorldScale3D(FVector(.75f+I*.27f));
        auto* R=ReflectedJet[I].Get();const FVector B=FMath::Lerp(P->Camera->GetComponentLocation()-FVector(0,0,20),Muzzle,(I+.5f)/14.f);R->SetVisibility(ReflectFlash>0&&BreathTime>0);R->SetWorldLocation(B);R->SetWorldRotation(FRotationMatrix::MakeFromZ(P->Camera->GetComponentLocation()-B).Rotator());R->SetWorldScale3D(FVector(.85));
    }
}
