#include "FWCombatEffects.h"
#include "FWGame.h"
#include "FWCavePassages.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

// Deterministic input driver for an actual, unpaused encounter. Never enabled in ordinary play.
void FWCaptureSliceCombat(AFWCharacter* P)
{
    if(FParse::Param(FCommandLine::Get(),TEXT("CaveWalkReview"))){FWCavePassages::WalkReview(P);return;}
    if(FParse::Param(FCommandLine::Get(),TEXT("SliceWalkReview")))
    {
        static bool Walking=false;static FVector Start;static double Last=0;
        const double Now=P->GetWorld()->GetRealTimeSeconds();if(Now<8)return;
        if(!Walking){Walking=true;Start=FVector(-38500,0,530);P->SetActorLocation(Start,false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);P->GetController()->SetControlRotation(FRotator::ZeroRotator);}
        P->AddMovementInput(FVector::ForwardVector);
        // This is a traversal review, so use ordinary movement input to pass
        // live characters instead of walking indefinitely into their capsules.
        for(TActorIterator<AFWEnemy> It(P->GetWorld());It;++It)
        {
            const FVector Offset=It->GetActorLocation()-P->GetActorLocation();
            if(!It->Dead&&Offset.X>0&&Offset.X<350&&FMath::Abs(Offset.Y)<180)
            {P->AddMovementInput(FVector::RightVector);break;}
        }
        P->GetController()->SetControlRotation(FRotator(-3,FMath::Sin(Now*.35)*6,0));
        if(Now-Last>=1)
        {
            Last=Now;auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("travelled_metres"),FVector::Dist2D(Start,P->GetActorLocation())/100);R->SetNumberField(TEXT("speed_cm_s"),P->GetVelocity().Size2D());R->SetNumberField(TEXT("seconds"),Now-8);
            FString Text;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectDir()/TEXT("Verification/slice-walk-review.json")));
        }
        return;
    }
    if(!FParse::Param(FCommandLine::Get(),TEXT("SliceCombatReview")))return;
    static TWeakObjectPtr<AFWEnemy> Enemy;static bool Ready=false,Written=false;
    static double Began=0;static int Frame=0,Parries=0;static bool WasParrying=false;
    static TArray<TSharedPtr<FJsonValue>> Samples;
    const double Now=P->GetWorld()->GetRealTimeSeconds();if(Now<8)return;
    if(!Ready)
    {
        for(TActorIterator<AFWEnemy> It(P->GetWorld());It;++It)if(It->Identity==TEXT("road_sentry")){Enemy=*It;break;}
        if(!Enemy.IsValid())return;
        Enemy->ResetEncounter();Enemy->Alerted=true;
        P->SetActorLocation(Enemy->GetActorLocation()+FVector(-185,0,0),false,nullptr,ETeleportType::TeleportPhysics);
        P->SetActorRotation(FRotator::ZeroRotator);P->GetController()->SetControlRotation(FRotator::ZeroRotator);
        P->GetCharacterMovement()->StopMovementImmediately();P->Health=100;P->Stamina=100;P->Invulnerability=0;P->AttackAge=-1;
        Ready=true;Began=Now;
    }
    if(!Enemy.IsValid()||Written)return;
    auto* E=Enemy.Get();
    if(!E->Dead)
    {
        P->GetController()->SetControlRotation((E->GetActorLocation()+FVector(0,0,35)-P->Camera->GetComponentLocation()).Rotation());
        const float Distance=FVector::Dist2D(E->GetActorLocation(),P->GetActorLocation());
        if(Distance>195)P->AddMovementInput((E->GetActorLocation()-P->GetActorLocation()).GetSafeNormal2D());
        if(P->AttackAge<0)
        {
            if(E->RiposteWindow>0){P->BlockEnd();P->StartAttack(true);}
            else if(E->Windup>0&&E->Windup<.17f)P->BlockStart();
            else if(E->Windup>.25f||E->Recovery>0)P->BlockEnd();
        }
    }
    else P->BlockEnd();
    if(P->ParryFeedback>0&&!WasParrying)Parries++;
    WasParrying=P->ParryFeedback>0;
    if(Now-Began>=Frame*.5&&Frame<24)
    {
        auto S=MakeShared<FJsonObject>();S->SetNumberField(TEXT("seconds"),Now-Began);S->SetNumberField(TEXT("enemy_health"),E->Health);S->SetNumberField(TEXT("player_health"),P->Health);S->SetNumberField(TEXT("windup"),E->Windup);S->SetBoolField(TEXT("blocking"),P->Blocking);S->SetNumberField(TEXT("attack_age"),P->AttackAge);Samples.Add(MakeShared<FJsonValueObject>(S));
        // Keep gameplay sampling while allowing a performance run without
        // 24 synchronous image readbacks and PNG writes during the fight.
        if(!FParse::Param(FCommandLine::Get(),TEXT("SliceCombatNoFrames")))FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/FString::Printf(TEXT("Verification/slice-combat-%02d.png"),Frame),true,false);
        Frame++;
    }
    if(Now-Began>13)
    {
        Written=true;auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("parries"),Parries);R->SetBoolField(TEXT("enemy_defeated"),E->Dead);R->SetNumberField(TEXT("player_health"),P->Health);R->SetArrayField(TEXT("samples"),Samples);R->SetBoolField(TEXT("passed"),Parries>=2&&E->Dead&&P->Health>0);
        R->SetNumberField(TEXT("captured_action_frames"),FParse::Param(FCommandLine::Get(),TEXT("SliceCombatNoFrames"))?0:Frame);
        FString Text;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectDir()/TEXT("Verification/slice-combat-review.json")));
    }
}
