#include "FWCombatEffects.h"
#include "FWGame.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "TimerManager.h"

void FWLoadCombatAudio(AFWCharacter* Player)
{
    if(!Player->StepAttenuation)
    {
        Player->StepAttenuation=NewObject<USoundAttenuation>(Player);
        Player->StepAttenuation->Attenuation.bAttenuate=true;Player->StepAttenuation->Attenuation.bSpatialize=true;
        Player->StepAttenuation->Attenuation.AttenuationShapeExtents=FVector(200);Player->StepAttenuation->Attenuation.FalloffDistance=900;
    }
    auto Load=[](const TCHAR* Family,int Count,TArray<TObjectPtr<USoundBase>>& Sounds)
    {
        Sounds.Reset();for(int I=1;I<=Count;I++)if(auto* S=LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/Combat/Audio/%s_%02d.%s_%02d"),Family,I,Family,I)))Sounds.Add(S);
    };
    Load(TEXT("clash"),6,Player->CombatClashes);Load(TEXT("swing"),4,Player->CombatSwings);Load(TEXT("impact"),4,Player->CombatImpacts);
}
void FWCombatAudio(AFWCharacter* Player,EFWCombatCue Cue,const FVector& Position)
{
    if(!Player)return;
    const auto& Sounds=Cue==EFWCombatCue::Swing?Player->CombatSwings:Cue==EFWCombatCue::Impact?Player->CombatImpacts:Player->CombatClashes;
    if(Sounds.IsEmpty())return;
    const float Volume=Cue==EFWCombatCue::Swing?.36f:Cue==EFWCombatCue::Parry?.85f:.65f;
    UGameplayStatics::PlaySoundAtLocation(Player,Sounds[FMath::RandHelper(Sounds.Num())],Position,Volume,FMath::FRandRange(.94f,1.06f),0,Player->StepAttenuation);
}
void FWLoadDamageParts(AFWEnemy* Enemy)
{
    Enemy->SeverMeshes.Reset();if(Enemy->Friendly||Enemy->Dragon)return;
    auto* Mesh=Enemy->GetMesh()->GetSkeletalMeshAsset();if(!Mesh)return;
    for(int Part=0;Part<5;Part++)Enemy->SeverMeshes.Add(LoadObject<UStaticMesh>(nullptr,*FWSeverAssetPath(Mesh,Part)));
}
static AActor* SpawnMeshPiece(AFWEnemy* Enemy,UStaticMesh* Mesh,const FTransform& Transform,bool Physics)
{
    auto* Actor=Enemy->GetWorld()->SpawnActor<AActor>();if(!Actor)return nullptr;
    auto* Component=NewObject<UStaticMeshComponent>(Actor);Actor->SetRootComponent(Component);Actor->AddInstanceComponent(Component);
    Component->SetStaticMesh(Mesh);Component->SetMobility(EComponentMobility::Movable);Component->SetCastShadow(Physics);
    Component->SetCollisionEnabled(Physics?ECollisionEnabled::PhysicsOnly:ECollisionEnabled::NoCollision);
    Component->SetCollisionObjectType(ECC_PhysicsBody);Component->SetCollisionResponseToAllChannels(ECR_Ignore);Component->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block);
    Component->RegisterComponent();Actor->SetActorTransform(Transform);
    if(Physics){Component->SetSimulatePhysics(true);Component->SetLinearDamping(.45f);Component->SetAngularDamping(.7f);Component->SetMassOverrideInKg(NAME_None,3.f,true);}
    Actor->SetLifeSpan(18);Enemy->DamageDebris.Add(Actor);return Actor;
}
bool FWSeverFinisher(AFWEnemy* Enemy,const FVector& HitPoint,const FVector& Direction,bool Execution,int PartOverride)
{
    if(Enemy->Friendly||Enemy->Dragon||Enemy->SeverMeshes.Num()!=5)return false;
    auto* Body=Enemy->GetMesh();auto* Mesh=Body->GetSkeletalMeshAsset();if(!Mesh)return false;
    Body->RefreshBoneTransforms();
    const int Part=Execution?0:(PartOverride!=INDEX_NONE?PartOverride:FWSelectHitPart(Enemy,HitPoint));
    if(Part<0||Part>=5)return false;
    const FName Bone=FWSeverBone(Mesh,Part);if(Bone.IsNone()||Enemy->SeveredBones.Contains(Bone)||!Enemy->SeverMeshes[Part])return false;
    const FTransform Transform=Body->GetSocketTransform(Bone,RTS_World);
    AActor* Piece=SpawnMeshPiece(Enemy,Enemy->SeverMeshes[Part],Transform,true);if(!Piece)return false;
    auto* Fragment=CastChecked<UStaticMeshComponent>(Piece->GetRootComponent());
    Fragment->AddImpulse((Direction.GetSafeNormal()*230+FVector(0,0,180))*Fragment->GetMass());Fragment->AddAngularImpulseInDegrees(FVector(80,170,110),NAME_None,true);
    const FName Parent=Body->GetParentBone(Bone);
    if(auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")))
    {
        const FVector ParentAt=Body->GetBoneLocation(Parent);
        FTransform Cap=Transform;Cap.SetRotation(FRotationMatrix::MakeFromZ((Transform.GetLocation()-ParentAt).GetSafeNormal()).ToQuat());
        // A thin cross-section reads as a cut surface. A rounded cap protrudes
        // from the joint; head pivots also sit above the visible neck opening.
        const float Radius=Part==0?.105f:Part>=3?.19f:.15f;
        Cap.SetScale3D(Transform.GetScale3D()*FVector(Radius,Radius,.018f));
        if(Part==0)Cap.SetLocation(FMath::Lerp(ParentAt,Transform.GetLocation(),.5f));
        if(auto* Stump=SpawnMeshPiece(Enemy,Sphere,Cap,false))
        {
            Stump->SetLifeSpan(0);Stump->AttachToComponent(Body,FAttachmentTransformRules::KeepWorldTransform,Parent);
            if(auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Combat/M_Wound.M_Wound")))CastChecked<UStaticMeshComponent>(Stump->GetRootComponent())->SetMaterial(0,Material);
        }
        if(auto* End=SpawnMeshPiece(Enemy,Sphere,Cap,false))
        {
            End->AttachToActor(Piece,FAttachmentTransformRules::KeepWorldTransform);
            if(auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Combat/M_Wound.M_Wound")))CastChecked<UStaticMeshComponent>(End->GetRootComponent())->SetMaterial(0,Material);
        }
    }
    Body->HideBoneByName(Bone,PBO_Term);Enemy->SeveredBone=Bone;Enemy->SeveredBones.Add(Bone);
    Enemy->BleedBone=Parent;Enemy->BleedTime=3;Enemy->BleedClock=0;
    FWEmitBlood(Enemy,Transform.GetLocation(),Direction,28);
    if(Part==2&&Enemy->Weapon)Enemy->Weapon->SetVisibility(false);
    // Some integrated equipment has a separate skeleton branch. Explicitly
    // hide it as well as the arm; encounter reset restores every hidden bone.
    const TArray<FName> Equipment=Part==2?TArray<FName>{TEXT("sword_bottom"),TEXT("sword_top"),TEXT("joint_weapon")}:Part==1?TArray<FName>{TEXT("shield_inner"),TEXT("shield_outer")}:TArray<FName>{};
    for(FName EquipmentBone:Equipment)if(Body->GetBoneIndex(EquipmentBone)!=INDEX_NONE){Body->HideBoneByName(EquipmentBone,PBO_Term);Enemy->SeveredBones.Add(EquipmentBone);}
    UE_LOG(LogTemp,Display,TEXT("COMBAT_SEVER enemy=%s bone=%s part=%d"),*Enemy->Identity,*Bone.ToString(),Part);
    return true;
}
void FWImpactMark(AFWEnemy* Enemy,const FVector& Point,const FVector& Direction)
{
    auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Combat/M_BloodMark.M_BloodMark"));if(!Material)return;
    // Attached surface wound accompanies the separately animated droplets.
    const FName Bone=Enemy->GetMesh()->FindClosestBone(Point);
    auto* Decal=UGameplayStatics::SpawnDecalAttached(Material,FVector(45,11,11),Enemy->GetMesh(),Bone,Point,(-Direction).Rotation(),EAttachLocation::KeepWorldPosition,15);
    // Attached decals belong to the enemy. The engine default would destroy
    // that owner when the mark fades, including an enemy that is still alive.
    if(Decal)Decal->SetFadeOut(10,5,false);
}
void FWResetCombatDamage(AFWEnemy* Enemy)
{
    for(AActor* Actor:Enemy->DamageDebris)if(IsValid(Actor))Actor->Destroy();Enemy->DamageDebris.Reset();
    for(FName Bone:Enemy->SeveredBones)Enemy->GetMesh()->UnHideBoneByName(Bone);
    if(!Enemy->SeveredBones.IsEmpty())Enemy->GetMesh()->RecreatePhysicsState();
    Enemy->SeveredBones.Reset();for(float& Damage:Enemy->PartDamage)Damage=0;Enemy->LastHitPart=INDEX_NONE;
    Enemy->GetCharacterMovement()->MaxWalkSpeed=Enemy->UninjuredSpeed;
    Enemy->BleedTime=Enemy->BleedClock=0;Enemy->BloodDrops.Reset();
    if(Enemy->BloodInstances){Enemy->BloodInstances->DestroyComponent();Enemy->BloodInstances=nullptr;}
    for(auto Pool:Enemy->BloodPools)if(Pool.IsValid())Pool->DestroyComponent();Enemy->BloodPools.Reset();Enemy->BloodPoolAges.Reset();
    Enemy->SeveredBone=NAME_None;if(Enemy->Weapon)Enemy->Weapon->SetVisibility(true);
    TArray<USceneComponent*> Children;Enemy->GetMesh()->GetChildrenComponents(false,Children);
    for(auto* Child:Children)if(auto* Decal=Cast<UDecalComponent>(Child))Decal->DestroyComponent();
}
