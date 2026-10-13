#include "FWGame.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Algo/Reverse.h"
#include "FWHorse.h"
#include "FWLivestock.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"

void AFWEnemy::UpdateRoutine(float Dt,AFWCharacter* P)
{
    if(Job.IsEmpty()||!P->Expedition){Play(Idle,true);return;}
    const float Hour=P->Expedition->Hour;
    const FString Phase=Hour>=6&&Hour<18?TEXT("working"):Hour<23&&Hour>=18?TEXT("tavern"):TEXT("sleeping");
    if(Phase!=RoutinePhase)
    {
        RoutinePhase=Phase;RouteIndex=0;WorkPause=0;
        ActiveRoute=Phase==TEXT("working")?MorningRoute:Phase==TEXT("tavern")?InnRoute:SleepRoute;
        float Best=TNumericLimits<float>::Max();for(int I=0;I<ActiveRoute.Num();I++){const float D=FVector::DistSquared(GetActorLocation(),ActiveRoute[I]);if(D<Best){Best=D;RouteIndex=I;}}
        SetActorHiddenInGame(false);GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }
    if(FVector::DistSquared(GetActorLocation(),P->GetActorLocation())<270*270)
    {
        GetCharacterMovement()->StopMovementImmediately();SetActorRotation(FMath::RInterpTo(GetActorRotation(),FRotator(0,(P->GetActorLocation()-GetActorLocation()).Rotation().Yaw,0),Dt,3));Play(Idle,true);Activity=TEXT("Talking");return;
    }
    WorkTool->SetVisibility(Phase==TEXT("working"));
    if(WorkPause>0)
    {
        GetCharacterMovement()->StopMovementImmediately();
        WorkPause-=Dt;Play(Idle,true);Activity=Job==TEXT("stable hand")?TEXT("Checking feed and tack"):Job==TEXT("herder")?TEXT("Tending the horses"):TEXT("Working: ")+Job;
        if(Job==TEXT("herder")||Job==TEXT("farmer"))
        {
            AFWLivestock* Nearest=nullptr;float Best=600*600;
            for(TActorIterator<AFWLivestock> A(GetWorld());A;++A)
            {
                const float D=FVector::DistSquared(GetActorLocation(),A->GetActorLocation());
                if(D<Best){Best=D;Nearest=*A;}
            }
            if(Nearest){Nearest->Tend(Dt);Activity=TEXT("Feeding the ")+Nearest->Species.ToLower();SetActorRotation(FMath::RInterpTo(GetActorRotation(),FRotator(0,(Nearest->GetActorLocation()-GetActorLocation()).Rotation().Yaw,0),Dt,2));return;}
        }
        if(Job==TEXT("stable hand")||Job==TEXT("herder"))for(TActorIterator<AFWHorse> Horse(GetWorld());Horse;++Horse)if(!Horse->Rider&&FVector::Dist2D(Horse->GetActorLocation(),GetActorLocation())<500){Horse->Energy=FMath::Min(100.f,Horse->Energy+Dt*(P->HasFlag(TEXT("supplies_done"))?18:12));SetActorRotation(FMath::RInterpTo(GetActorRotation(),FRotator(0,(Horse->GetActorLocation()-GetActorLocation()).Rotation().Yaw,0),Dt,2));break;}
        return;
    }
    if(ActiveRoute.IsValidIndex(RouteIndex))
    {
        FVector Delta=ActiveRoute[RouteIndex]-GetActorLocation();Delta.Z=0;
        if(Delta.Size()<45)
        {
            ++RouteIndex;
            if(Phase==TEXT("working")&&RouteIndex%6==0&&(Job==TEXT("herder")||Job==TEXT("farmer")))
                for(TActorIterator<AFWLivestock> A(GetWorld());A;++A)if(FVector::Dist2D(A->GetActorLocation(),GetActorLocation())<350){WorkPause=FMath::FRandRange(6.f,10.f);break;}
            return;
        }
        FVector Dir=Delta.GetSafeNormal();FHitResult Hit;FCollisionQueryParams Q;Q.AddIgnoredActor(this);
        if(GetWorld()->SweepSingleByChannel(Hit,GetActorLocation(),GetActorLocation()+Dir*120,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(32),Q))
        {
            // Avoid a newly dressed prop or another actor without walking through it.
            for(float Angle:{55.f,-55.f,100.f,-100.f}){FVector Alternative=Dir.RotateAngleAxis(Angle,FVector::UpVector);if(!GetWorld()->SweepSingleByChannel(Hit,GetActorLocation(),GetActorLocation()+Alternative*150,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(32),Q)){Dir=Alternative;break;}}
        }
        auto* Movement=GetCharacterMovement();
        Movement->MaxWalkSpeed=135;Movement->MaxAcceleration=360;Movement->BrakingDecelerationWalking=500;
        SetActorRotation(FMath::RInterpTo(GetActorRotation(),FRotator(0,Dir.Rotation().Yaw,0),Dt,4));
        // Turn towards the route before walking, instead of sliding sideways around corners.
        const float Facing=FMath::Max(0.f,FVector::DotProduct(GetActorForwardVector(),Dir));
        if(Facing>.65f)AddMovementInput(GetActorForwardVector(),Facing);
        else Movement->StopMovementImmediately();
        const float Speed=GetVelocity().Size2D();Play(Speed>8?Walk:Idle,true);
        if(CurrentClip==Walk)GetMesh()->SetPlayRate(FMath::Clamp(Speed/135.f,.25f,1.15f));
        Activity=Phase==TEXT("working")?TEXT("Walking to work"):Phase==TEXT("tavern")?TEXT("Heading to the inn"):TEXT("Returning home");return;
    }
    GetCharacterMovement()->StopMovementImmediately();Play(Idle,true);
    if(Phase==TEXT("working")){ActiveRoute=WorkRoute;if(FMath::RandBool())Algo::Reverse(ActiveRoute);RouteIndex=0;WorkPause=7+FMath::FRand()*8;}
    else if(Phase==TEXT("sleeping")){Activity=TEXT("Sleeping");SetActorHiddenInGame(true);GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);}
    else Activity=TEXT("Resting at the inn");
}
