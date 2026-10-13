#include "FWGame.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void AFWCharacter::SetupViewModel()
{
    // The third-person Greystone idle keeps its weapons outside the first-person
    // camera. Keep that experiment opt-in until it has a fitted arms animation set.
    Sword->SetVisibility(true);Shield->SetVisibility(true);
    if(!FParse::Param(FCommandLine::Get(),TEXT("ExperimentalBodyView")))return;
    auto* ViewMeshAsset=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Meshes/Greystone.Greystone"));
    ViewIdle=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Animations/Idle.Idle"));
    ViewWalk=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Animations/Jog_Fwd.Jog_Fwd"));
    for(const FString Letter:{TEXT("A"),TEXT("B"),TEXT("C")}){const FString Name=TEXT("Attack_Primary")+Letter;ViewAttacks.Add(LoadObject<UAnimSequence>(nullptr,*(TEXT("/Game/ParagonGreystone/Characters/Heroes/Greystone/Animations/")+Name+TEXT(".")+Name)));}
    if(!ViewMeshAsset||!ViewIdle||!ViewWalk||ViewAttacks.Contains(nullptr))return;
    AuthoredView=true;GetMesh()->SetSkeletalMeshAsset(ViewMeshAsset);GetMesh()->AttachToComponent(Camera,FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    GetMesh()->SetRelativeLocation(FVector(-42,0,-154));GetMesh()->SetRelativeRotation(FRotator(0,-90,0));GetMesh()->SetRelativeScale3D(FVector(.87f));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);GetMesh()->SetOnlyOwnerSee(true);GetMesh()->SetCastShadow(false);GetMesh()->HideBoneByName(TEXT("head"),PBO_None);
    Sword->SetVisibility(false);Shield->SetVisibility(false);UpdateViewModel(true);
}
void AFWCharacter::UpdateViewModel(bool Restart)
{
    if(!AuthoredView)return;
    const int State=AttackAge>=0?2:Blocking?3:GetVelocity().Size2D()>50?1:0;
    UAnimSequence* Clip=State==2?ViewAttacks[HeavyAttack?2:Combo%3].Get():State==3?ViewAttacks[1].Get():State==1?ViewWalk.Get():ViewIdle.Get();
    if(Restart||ViewClip!=Clip||ViewState!=State)
    {
        GetMesh()->bPauseAnims=false;GetMesh()->PlayAnimation(Clip,State<2);ViewClip=Clip;ViewState=State;
        GetMesh()->SetPlayRate(State==2?Clip->GetPlayLength()/AttackLength:1);
        if(State==3){GetMesh()->SetPosition(Clip->GetPlayLength()*.43f,false);GetMesh()->bPauseAnims=true;}
    }
}
