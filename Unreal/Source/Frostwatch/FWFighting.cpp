#include "FWFighting.h"
#include "FWGame.h"
#include "FWCombatEffects.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"

EFWStrike FWChooseStrike(bool Heavy,float Forward,float Strafe,bool Crouched,bool Guard,int Chain)
{
    if(!Heavy&&Guard)return EFWStrike::Bash;
    if(Heavy&&Chain>=2)return EFWStrike::Finisher;
    if(Crouched)return EFWStrike::LowCut;
    if(Heavy&&Forward>.35f)return EFWStrike::Thrust;
    if(Heavy&&Forward<-.35f)return EFWStrike::Rising;
    if(!Heavy&&FMath::Abs(Strafe)>.35f)return Strafe<0?EFWStrike::LeftCut:EFWStrike::RightCut;
    return Heavy?EFWStrike::Heavy:EFWStrike::Cut;
}
const TCHAR* FWStrikeName(EFWStrike Move)
{
    switch(Move)
    {
    case EFWStrike::LeftCut:return TEXT("LEFT CUT");case EFWStrike::RightCut:return TEXT("RIGHT CUT");
    case EFWStrike::Thrust:return TEXT("DRIVING THRUST");case EFWStrike::Rising:return TEXT("RISING CUT");
    case EFWStrike::LowCut:return TEXT("LOW CUT");case EFWStrike::Bash:return TEXT("SHIELD BASH");
    case EFWStrike::Finisher:return TEXT("COMBINATION FINISHER");case EFWStrike::Heavy:return TEXT("OVERHEAD CUT");
    default:return TEXT("CUT");
    }
}
int FWSelectHitPart(const AFWEnemy* Enemy,const FVector& Point)
{
    const auto* Body=Enemy->GetMesh();auto* Mesh=Body->GetSkeletalMeshAsset();if(!Mesh)return INDEX_NONE;
    float Best=26.f*26.f;int Selected=INDEX_NONE;
    // Segment tests follow animated limbs, including bent knees and raised arms.
    const TCHAR* Tips[]={TEXT("head"),TEXT("hand_l"),TEXT("hand_r"),TEXT("foot_l"),TEXT("foot_r")};
    const TCHAR* OrcTips[]={TEXT("CC_Base_Head"),TEXT("CC_Base_L_Hand"),TEXT("CC_Base_R_Hand"),TEXT("CC_Base_L_Foot"),TEXT("CC_Base_R_Foot")};
    for(int Part=0;Part<5;Part++)
    {
        FName Bone=FWSeverBone(Mesh,Part);if(Bone.IsNone()||Enemy->SeveredBones.Contains(Bone))continue;
        FVector A=Body->GetBoneLocation(Bone),B=A;
        FName Tip=Body->GetBoneIndex(Tips[Part])!=INDEX_NONE?FName(Tips[Part]):FName(OrcTips[Part]);
        if(Body->GetBoneIndex(Tip)!=INDEX_NONE)B=Body->GetBoneLocation(Tip);
        // The centre of the torso must not count as the thigh at its hip pivot.
        if(Part>=3&&FVector::DistSquared(Point,A)>4)A=FMath::Lerp(A,B,.18f);
        const float Distance=FMath::PointDistToSegmentSquared(Point,A,B);
        if(Distance<Best){Best=Distance;Selected=Part;}
    }
    return Selected;
}
float FWInjuryAttackScale(const AFWEnemy* Enemy)
{
    if(Enemy->SeveredBones.Contains(FWSeverBone(Enemy->GetMesh()->GetSkeletalMeshAsset(),2)))return .25f;
    return Enemy->PartDamage[1]>=20||Enemy->PartDamage[2]>=20?.65f:1.f;
}
void FWEmitBlood(AFWEnemy* Enemy,const FVector& Point,const FVector& Direction,int Count)
{
    if(Enemy->Friendly||Enemy->Dragon)return;
    if(!Enemy->BloodInstances)
    {
        auto* C=NewObject<UInstancedStaticMeshComponent>(Enemy);Enemy->BloodInstances=C;Enemy->AddInstanceComponent(C);
        C->SetMobility(EComponentMobility::Movable);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCastShadow(false);C->SetCanEverAffectNavigation(false);
        C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
        C->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Combat/M_LiquidBlood.M_LiquidBlood")));C->RegisterComponent();
        for(int I=0;I<64;I++)C->AddInstance(FTransform(FQuat::Identity,Point,FVector::ZeroVector),true);
    }
    for(int I=0;I<Count&&Enemy->BloodDrops.Num()<64;I++)
    {
        FFWBloodDrop Drop;Drop.Position=Point;Drop.Velocity=Direction.GetSafeNormal()*FMath::FRandRange(90.f,220.f)+FMath::VRand()*FMath::FRandRange(35.f,130.f)+FVector(0,0,90);
        Drop.Radius=FMath::FRandRange(.35f,1.15f);Enemy->BloodDrops.Add(Drop);
    }
}
void FWUpdateBlood(AFWEnemy* Enemy,float Dt)
{
    if(Enemy->BleedTime>0)
    {
        Enemy->BleedTime=FMath::Max(0.f,Enemy->BleedTime-Dt);Enemy->BleedClock-=Dt;
        if(Enemy->BleedClock<=0){Enemy->BleedClock=.12f;FWEmitBlood(Enemy,Enemy->GetMesh()->GetBoneLocation(Enemy->BleedBone),FVector(0,0,-1),2);}
    }
    for(int I=Enemy->BloodPools.Num()-1;I>=0;I--)
    {
        if(!Enemy->BloodPools[I].IsValid()){Enemy->BloodPools.RemoveAt(I);Enemy->BloodPoolAges.RemoveAt(I);continue;}
        Enemy->BloodPoolAges[I]+=Dt;
        const float Spread=FMath::Lerp(.35f,1.f,FMath::SmoothStep(0.f,1.5f,Enemy->BloodPoolAges[I]));
        Enemy->BloodPools[I]->SetWorldScale3D(FVector(1,Spread,Spread));
    }
    if(!Enemy->BloodInstances||Enemy->BloodDrops.IsEmpty())return;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(FrostwatchBlood),false,Enemy);
    FCollisionObjectQueryParams Objects(ECC_WorldStatic);
    const int Steps=FMath::Clamp(FMath::CeilToInt(Dt/.016f),1,16);const float Step=Dt/Steps;
    for(int I=Enemy->BloodDrops.Num()-1;I>=0;I--)
    {
        auto& Drop=Enemy->BloodDrops[I];bool Done=false;Drop.Age+=Dt;
        for(int S=0;S<Steps&&!Done;S++)
        {
            const FVector From=Drop.Position;Drop.Velocity.Z-=980*Step;Drop.Velocity*=FMath::Exp(-.45f*Step);Drop.Position+=Drop.Velocity*Step;
            FHitResult Hit;
            if(Enemy->GetWorld()->LineTraceSingleByObjectType(Hit,From,Drop.Position,Objects,Q))
            {
                Done=true;
                // A fixed pool budget prevents long fights from accumulating decals.
                if(Enemy->BloodPools.Num()>=12){if(auto* Old=Enemy->BloodPools[0].Get())Old->DestroyComponent();Enemy->BloodPools.RemoveAt(0);Enemy->BloodPoolAges.RemoveAt(0);}
                auto* M=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Combat/M_BloodMark.M_BloodMark"));
                if(M)if(auto* D=UGameplayStatics::SpawnDecalAtLocation(Enemy,M,FVector(4,Drop.Radius*10,Drop.Radius*13),Hit.ImpactPoint+Hit.ImpactNormal*.2f,(-Hit.ImpactNormal).Rotation(),18))
                {D->SetFadeOut(13,5,false);Enemy->BloodPools.Add(D);Enemy->BloodPoolAges.Add(0);}
            }
        }
        if(Done||Drop.Age>2.5f)Enemy->BloodDrops.RemoveAtSwap(I);
    }
    TArray<FTransform> Transforms;Transforms.Init(FTransform(FQuat::Identity,Enemy->GetActorLocation(),FVector::ZeroVector),64);
    for(int I=0;I<Enemy->BloodDrops.Num();I++)
    {
        const auto& Drop=Enemy->BloodDrops[I];const float R=Drop.Radius/50;
        const float Stretch=FMath::Clamp(float(Drop.Velocity.Size()/170),1.f,3.2f);
        Transforms[I]=FTransform(FRotationMatrix::MakeFromZ(Drop.Velocity).ToQuat(),Drop.Position,FVector(R/FMath::Sqrt(Stretch),R/FMath::Sqrt(Stretch),R*Stretch));
    }
    Enemy->BloodInstances->BatchUpdateInstancesTransforms(0,Transforms,true,true,true);
}
