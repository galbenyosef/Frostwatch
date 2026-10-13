#include "FWGame.h"
#include "Engine/World.h"
#include "FWHorse.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "FWWaterVolume.h"

void AFWCharacter::JumpStart()
{
    if(!CanAct()||MountedHorse||bIsCrouched||Blocking||AttackAge>=0)return;
    if(GetCharacterMovement()->IsSwimming())
    {
        SwimPush=.45f;GetCharacterMovement()->Velocity.Z=260;
        return;
    }
    Jump();
}
void AFWCharacter::UpdateSwimming(float Dt)
{
    auto* Movement=GetCharacterMovement();const bool Swimming=Movement->IsSwimming();
    if(Swimming)
    {
        if(!WasSwimming)Notify(TEXT("You float automatically. ")+KeyLabel(TEXT("Jump"))+TEXT(": push up toward a bank. Movement keys swim; sprint swims faster."),6);
        if(bIsCrouched)UnCrouch();
        Blocking=GuardHeld=false;AttackAge=-1;QueuedAttack=0;
        Movement->MaxSwimSpeed=Sprinting?300:220;
        if(auto* Water=Cast<AFWWaterVolume>(GetPhysicsVolume()))
        {
            SwimPush=FMath::Max(0.f,SwimPush-Dt);
            // Apply buoyancy through movement input so the native swimming
            // brake cannot cancel every small upward correction on a bank.
            AddMovementInput(FVector::UpVector,SwimPush>0?1.f:FMath::Clamp((Water->SurfaceZ()-32-GetActorLocation().Z)/70.f,-.5f,.7f));
        }
    }
    if(Swimming!=WasSwimming&&!AuthoredView){Sword->SetVisibility(!Swimming&&!MountedHorse);Shield->SetVisibility(!Swimming&&!MountedHorse);}
    WasSwimming=Swimming;
}
FString AFWCharacter::SurfaceAt(const FVector& Location)
{
    struct FZone {FVector2D Centre,Half;bool Cave;};
    struct FRoute {FVector A,B;};
    static TArray<FZone> Zones;static TArray<FRoute> Roads;static bool Loaded=false;
    if(!Loaded)
    {
        Loaded=true;FString Text;TArray<TSharedPtr<FJsonValue>> Data;
        if(FFileHelper::LoadFileToString(Text,*(FPaths::ProjectContentDir()/TEXT("Migration/surface-zones.json")))&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data))
            for(auto V:Data){auto R=V->AsObject();auto P=R->GetArrayField(TEXT("p")),H=R->GetArrayField(TEXT("half"));Zones.Add({FVector2D(P[0]->AsNumber(),P[1]->AsNumber()),FVector2D(H[0]->AsNumber(),H[1]->AsNumber()),R->GetStringField(TEXT("type"))==TEXT("cave")});}
        TSharedPtr<FJsonObject> World;
        if(FFileHelper::LoadFileToString(Text,*(FPaths::ProjectContentDir()/TEXT("Migration/world.json")))&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),World))
            for(auto V:World->GetArrayField(TEXT("roads"))){auto Points=V->AsArray();for(int32 I=1;I<Points.Num();I++){auto A=Points[I-1]->AsArray(),B=Points[I]->AsArray();Roads.Add({FVector(-A[2]->AsNumber()*100,A[0]->AsNumber()*100,A[1]->AsNumber()*100),FVector(-B[2]->AsNumber()*100,B[0]->AsNumber()*100,B[1]->AsNumber()*100)});}}
    }
    const FVector2D P(Location.Y/100,-Location.X/100);
    for(const auto& Z:Zones)if(FMath::Abs(P.X-Z.Centre.X)<Z.Half.X&&FMath::Abs(P.Y-Z.Centre.Y)<Z.Half.Y)return Z.Cave?TEXT("stone"):TEXT("wood");
    for(const auto& R:Roads){FVector A=R.A,B=R.B,Q=Location;A.Z=B.Z=Q.Z=0;const FVector Near=FMath::ClosestPointOnSegment(Q,A,B);if(FVector::DistSquared(Q,Near)<FMath::Square(380.f))return TEXT("gravel");}
    return TEXT("snow");
}
void AFWCharacter::UpdateTraversalSound(float Dt)
{
    const float Speed=GetVelocity().Size2D();
    if(Speed<30||!GetCharacterMovement()->IsMovingOnGround()){FootstepClock=0;return;}
    FootstepClock-=Dt;if(FootstepClock>0)return;
    FString Surface=SurfaceAt(GetActorLocation());
    // Underground routes share XY coordinates with the landscape above them.
    // Resolve their footsteps from the actual floor, not the surface map.
    FHitResult StepFloor;FCollisionQueryParams StepQuery(SCENE_QUERY_STAT(CaveFootstep),false,this);
    if(GetWorld()->LineTraceSingleByChannel(StepFloor,GetActorLocation(),GetActorLocation()-FVector(0,0,180),ECC_Visibility,StepQuery)
        &&StepFloor.GetActor()&&StepFloor.GetActor()->ActorHasTag(TEXT("BuriedLabyrinth")))Surface=TEXT("stone");
    const bool Horse=MountedHorse!=nullptr;
    const float Stride=Horse?(Sprinting?200.f:100.f):(Sprinting?185.f:165.f);
    FootstepClock=FMath::Clamp(Stride/Speed,Horse?.12f:.27f,.8f);
    FString Clip;
    if(Horse){Clip=TEXT("hoof-")+(Surface==TEXT("wood")?FString(TEXT("hall")):Surface==TEXT("snow")?FString(TEXT("muffled")):FString(TEXT("ground")));if(Sprinting){const float Gallop[]={.72f,.8f,.68f,1.8f};FootstepClock*=Gallop[StepSequence%4];}}
    else{const int32 Count=Surface==TEXT("wood")||Surface==TEXT("stone")?5:8;int32 Index=FMath::RandRange(1,Count);if(Index==LastStepIndex)Index=Index%Count+1;LastStepIndex=Index;Clip=FString::Printf(TEXT("%s-%02d"),*Surface,Index);}
    StepSequence++;
    auto* Sound=TraversalSounds.Find(Clip);
    if(!Sound){auto* Asset=LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/Campaign/TraversalAudio/%s.%s"),*Clip,*Clip));TraversalSounds.Add(Clip,Asset);Sound=TraversalSounds.Find(Clip);}
    if(!Sound||!Sound->Get())return;
    if(!StepAttenuation){StepAttenuation=NewObject<USoundAttenuation>(this);StepAttenuation->Attenuation.bAttenuate=true;StepAttenuation->Attenuation.bSpatialize=true;StepAttenuation->Attenuation.AttenuationShapeExtents=FVector(200);StepAttenuation->Attenuation.FalloffDistance=900;}
    const FVector Foot=GetActorLocation()-FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight())+GetActorRightVector()*((StepSequence%2)?12:-12);
    UGameplayStatics::PlaySoundAtLocation(this,Sound->Get(),Foot,(Horse?.27f:bIsCrouched?.08f:.16f)*FMath::FRandRange(.88f,1.04f),FMath::FRandRange(.96f,1.04f),0,StepAttenuation);
}

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "GameFramework/InputSettings.h"
void AFWCharacter::ValidateSwimming(float Dt)
{
    static bool Began=false,Entered=false,Floated=false,Done=false;static float Start=0,StartHealth=0;static FVector Begin;
    if(Done||GetWorld()->GetTimeSeconds()<3)return;
    if(!Began)
    {
        Began=true;Start=GetWorld()->GetTimeSeconds();StartHealth=Health;Begin=FVector(13900,34500,2180);
        SetActorLocation(Begin);GetCharacterMovement()->StopMovementImmediately();GetCharacterMovement()->SetMovementMode(MOVE_Falling);
        GetCapsuleComponent()->UpdatePhysicsVolume(true);Sprinting=false;
    }
    const float Age=GetWorld()->GetTimeSeconds()-Start;
    Entered|=GetCharacterMovement()->IsSwimming();
    Floated|=Age>2&&GetCharacterMovement()->IsSwimming()&&GetActorLocation().Z>2060;
    if(Age>3)AddMovementInput(FVector(-1,0,0),1);
    if(Age<15)return;Done=true;
    auto Report=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Checks;bool Passed=true;
    auto Check=[&](const FString& Name,bool OK){auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),Name);Row->SetBoolField(TEXT("passed"),OK);Checks.Add(MakeShared<FJsonValueObject>(Row));Passed&=OK;};
    Check(TEXT("Falling into the actual waterfall pool enters native swimming"),Entered);
    Check(TEXT("Buoyancy keeps the swimmer near the water surface"),Floated);
    Check(TEXT("Ordinary movement swims more than eight metres toward the bank"),Begin.X-GetActorLocation().X>800);
    Check(TEXT("The swimmer leaves the water and walks onto the western bank"),GetCharacterMovement()->IsMovingOnGround()&&GetActorLocation().X<12850);
    Check(TEXT("Swimming and leaving the pool preserve player health"),Health==StartHealth&&Health>0);
    Report->SetBoolField(TEXT("all_passed"),Passed);Report->SetArrayField(TEXT("checks"),Checks);Report->SetStringField(TEXT("final_location"),GetActorLocation().ToString());Report->SetNumberField(TEXT("movement_mode"),int(GetCharacterMovement()->MovementMode));
    Report->SetBoolField(TEXT("swimming_update_ran"),WasSwimming);Report->SetBoolField(TEXT("menu_open"),Menu);
    Report->SetStringField(TEXT("water_volume"),GetPhysicsVolume()->GetClass()->GetName());
    Report->SetNumberField(TEXT("immersion_depth"),GetCharacterMovement()->ImmersionDepth());Report->SetStringField(TEXT("acceleration"),GetCharacterMovement()->GetCurrentAcceleration().ToString());
    if(auto* Water=Cast<AFWWaterVolume>(GetPhysicsVolume()))Report->SetNumberField(TEXT("surface_z"),Water->SurfaceZ());
    FHitResult Barrier;FCollisionQueryParams Probe(SCENE_QUERY_STAT(SwimExit),false,this);
    if(GetWorld()->SweepSingleByChannel(Barrier,GetActorLocation(),GetActorLocation()-FVector(150,0,0),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(GetCapsuleComponent()->GetScaledCapsuleRadius(),GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),Probe))
    {Report->SetStringField(TEXT("barrier"),Barrier.GetActor()?Barrier.GetActor()->GetActorNameOrLabel():TEXT("unknown"));Report->SetStringField(TEXT("barrier_normal"),Barrier.ImpactNormal.ToString());}
    if(GetWorld()->SweepSingleByChannel(Barrier,GetActorLocation(),GetActorLocation()+FVector(0,0,150),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(GetCapsuleComponent()->GetScaledCapsuleRadius(),GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),Probe))
        Report->SetStringField(TEXT("above"),Barrier.GetActor()?Barrier.GetActor()->GetActorNameOrLabel():TEXT("unknown"));
    FString Text;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectDir()/TEXT("Verification/swimming-tests.json")));FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
}
void AFWCharacter::ValidateTraversal()
{
    static bool Began=false,Done=false;static float StartedAt=0,StartZ=0,PeakZ=0;
    if(Done||GetWorld()->GetTimeSeconds()<3)return;
    if(!Began){Began=true;StartedAt=GetWorld()->GetTimeSeconds();StartZ=PeakZ=GetActorLocation().Z;JumpStart();}
    PeakZ=FMath::Max(PeakZ,float(GetActorLocation().Z));
    if(GetWorld()->GetTimeSeconds()-StartedAt>.08f)StopJumping();
    if(GetWorld()->GetTimeSeconds()-StartedAt<3)return;Done=true;
    TArray<TSharedPtr<FJsonValue>> Checks;bool Passed=true;
    auto Check=[&](const FString& Name,bool OK){auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("name"),Name);R->SetBoolField(TEXT("passed"),OK);Checks.Add(MakeShared<FJsonValueObject>(R));Passed&=OK;};
    Check(TEXT("Jump physically lifts the player at least 70 cm"),PeakZ-StartZ>70);
    Check(TEXT("Jump lands back on the collision floor"),GetCharacterMovement()->IsMovingOnGround()&&FMath::Abs(GetActorLocation().Z-StartZ)<20);
    const auto& Keys=GetDefault<UInputSettings>()->GetActionMappings();
    Check(TEXT("Space jumps and Left Alt dodges"),Keys.ContainsByPredicate([](const FInputActionKeyMapping& M){return M.ActionName==TEXT("Jump")&&M.Key==EKeys::SpaceBar;})&&Keys.ContainsByPredicate([](const FInputActionKeyMapping& M){return M.ActionName==TEXT("Dodge")&&M.Key==EKeys::LeftAlt;}));
    Check(TEXT("Inn floor selects wood"),SurfaceAt(FVector(-43300,-2800,545))==TEXT("wood"));
    Check(TEXT("Main road selects gravel"),SurfaceAt(FVector(-41000,0,500))==TEXT("gravel"));
    Check(TEXT("Open hillside selects snow"),SurfaceAt(FVector(-23900,12700,2500))==TEXT("snow"));
    bool Assets=true;
    for(const FString Family:{TEXT("snow"),TEXT("gravel"),TEXT("stone"),TEXT("wood")})for(int32 I=1;I<=(Family==TEXT("wood")||Family==TEXT("stone")?5:8);I++){const FString Clip=FString::Printf(TEXT("%s-%02d"),*Family,I);auto* Sound=LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/Campaign/TraversalAudio/%s.%s"),*Clip,*Clip));Assets&=Sound&&Sound->GetDuration()>0;}
    for(const FString Name:{TEXT("hoof-hall"),TEXT("hoof-ground"),TEXT("hoof-muffled")}){auto* Sound=LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/Campaign/TraversalAudio/%s.%s"),*Name,*Name));Assets&=Sound&&Sound->GetDuration()>0;}
    Check(TEXT("All surface footsteps and real hoof recordings are packaged"),Assets);
    auto Root=MakeShared<FJsonObject>();Root->SetBoolField(TEXT("all_passed"),Passed);Root->SetNumberField(TEXT("jump_rise_cm"),PeakZ-StartZ);Root->SetArrayField(TEXT("checks"),Checks);FString Output;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Output));FFileHelper::SaveStringToFile(Output,*(FPaths::ProjectDir()/TEXT("Verification/traversal-polish-tests.json")));FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
}
