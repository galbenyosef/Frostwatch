#include "FWGame.h"
#include "FWConsequences.h"
#include "FWSave.h"
#include "FWMotion.h"
#include "FWReleaseGraphics.h"
#include "FWCombatEffects.h"
#include "FWDisplay.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#endif
#include "FWLegacySave.h"
#include "FWCampaign.h"
#include "FWStory.h"
#include "FWHorse.h"
#include "FWLivestock.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "Animation/AnimSequence.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Sound/SoundBase.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Serialization/JsonSerializer.h"
#include "HighResScreenshot.h"
#include "UnrealClient.h"
#include "TimerManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/AudioComponent.h"

static void RunFrostwatchTests(AFWCharacter* P)
{
    TArray<TSharedPtr<FJsonValue>> Checks;
    bool Passed=true;
    auto Check=[&](const FString& Name,bool Result){auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("name"),Name);O->SetBoolField(TEXT("passed"),Result);Checks.Add(MakeShared<FJsonValueObject>(O));Passed&=Result;};
    Check(TEXT("Character has camera and movement"),P->Camera&&P->GetCharacterMovement());
    Check(TEXT("First-person sword and shield are visible by default"),P->Sword&&P->Shield&&P->Sword->GetStaticMesh()&&P->Shield->GetStaticMesh()&&P->Sword->IsVisible()&&P->Shield->IsVisible());
    FHitResult Ground;FCollisionQueryParams Q;Q.AddIgnoredActor(P);
    P->GetWorld()->LineTraceSingleByChannel(Ground,P->GetActorLocation(),P->GetActorLocation()-FVector(0,0,1000),ECC_Visibility,Q);
    Check(TEXT("Physical floor beneath starting position"),Ground.bBlockingHit);
    const FVector Start=P->GetActorLocation();
    FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFWEnemy* E=P->GetWorld()->SpawnActor<AFWEnemy>(AFWEnemy::StaticClass(),Start+P->GetActorForwardVector()*150,FRotator::ZeroRotator,Spawn);
    E->MeleeDirection=FVector::ForwardVector;
    Check(TEXT("Committed melee swing reaches a target ahead"),E->IsInsideMeleeArc(E->GetActorLocation()+FVector(180,0,0)));
    Check(TEXT("Committed melee swing misses a sidestep"),!E->IsInsideMeleeArc(E->GetActorLocation()+FVector(0,180,0)));
    Check(TEXT("Committed melee swing cannot hit behind or out of reach"),!E->IsInsideMeleeArc(E->GetActorLocation()+FVector(-150,0,0))&&!E->IsInsideMeleeArc(E->GetActorLocation()+FVector(300,0,0)));
    P->Health=100;P->Stamina=100;P->AttackAge=-1;
    P->StartAttack(false);Check(TEXT("Light attack spends 15 stamina"),FMath::IsNearlyEqual(P->Stamina,85.f));
    P->AttackAge=.05f;P->StartAttack(true);Check(TEXT("Early followup rejected"),P->QueuedAttack==0);
    P->AttackAge=P->AttackLength-.1f;P->StartAttack(true);Check(TEXT("Recovery followup buffered"),P->QueuedAttack==2);
    P->AttackAge=-1;P->QueuedAttack=0;P->Stamina=1;P->StartAttack(true);Check(TEXT("Exhausted attack rejected"),P->AttackAge<0);
    P->Stamina=100;P->StartAttack(false);P->AttackAge=.14f;P->StartAttack(false);
    Check(TEXT("Swing followup buffers before recovery"),P->QueuedAttack==1);
    P->HitResolved=true;P->AttackAge=P->AttackLength*.78f;const int32 FirstCombo=P->Combo;
    P->UpdateCombat(.001f);
    Check(TEXT("Buffered cut chains early with a new direction and one stamina charge"),P->AttackAge<.01f&&P->Combo!=FirstCombo&&P->Stamina==70&&P->QueuedAttack==0);
    P->BlockStart();P->UpdateCombat(.03f);
    Check(TEXT("Guard cannot cancel attack anticipation"),!P->Blocking&&P->GuardHeld&&P->AttackAge>=0);
    P->HitResolved=true;P->AttackAge=P->ImpactAt+.12f;P->UpdateCombat(.001f);
    Check(TEXT("Held guard engages on recovery without another press"),P->Blocking&&P->AttackAge<0&&P->BlockAge==0);
    P->BlockEnd();Check(TEXT("Releasing guard clears held and active state"),!P->GuardHeld&&!P->Blocking);
    P->StartAttack(false);P->AttackAge=.14f;P->StartAttack(true);P->BlockStart();P->BlockEnd();
    P->HitResolved=true;P->AttackAge=P->ImpactAt+.12f;P->UpdateCombat(.001f);
    Check(TEXT("Released buffered guard does not interrupt a cut"),!P->Blocking&&P->AttackAge>=0);
    P->AttackAge=-1;P->QueuedAttack=0;P->StrikeHold=0;
    P->Stamina=60;P->BlockStart();E->Windup=.1f;P->ReceiveEnemyHit(E,19);Check(TEXT("Timed parry prevents damage"),P->Health==100&&E->Stagger>1);
    Check(TEXT("Parry cancels the incoming swing and provides feedback"),E->Windup==0&&P->ParryFeedback>0&&P->GuardImpact>0);
    Check(TEXT("Timed parry restores stamina"),P->Stamina>60);
    P->BlockAge=1;P->Stamina=60;P->ReceiveEnemyHit(E,19);Check(TEXT("Facing guard costs stamina"),P->Health==100&&P->Stamina<60);
    E->SetActorLocation(Start-P->GetActorForwardVector()*150);P->ReceiveEnemyHit(E,19);Check(TEXT("Guard cannot block rear hit"),P->Health==81);
    P->Blocking=false;P->Stamina=100;P->DodgeCooldown=0;P->Dodge();Check(TEXT("Dodge spends stamina and grants brief avoidance"),P->Stamina==76&&P->Invulnerability>0);
    E->Friendly=true;E->Health=100;E->ReceiveStrike(200,FVector::ZeroVector,true);Check(TEXT("Friendly cannot receive combat damage"),E->Health==100&&!E->Dead);
    E->Friendly=false;E->Identity=TEXT("king");P->Expedition->Flags.Remove(TEXT("amulet_truth"));
    E->ReceiveStrike(500,FVector::ZeroVector,true);Check(TEXT("Stu ward blocks damage"),!E->Dead&&E->Health==100);
    E->Die(FVector::ZeroVector);Check(TEXT("Stu ward blocks direct death entry"),!E->Dead);
    P->Expedition->Flags.Add(TEXT("amulet_truth"));E->ReceiveStrike(500,FVector::ZeroVector,true);Check(TEXT("Amulet permits Stu defeat"),E->Dead);
    int32 Count=P->Expedition->Defeated.Num();E->Die(FVector::ZeroVector);Check(TEXT("Repeated death does not duplicate victory"),P->Expedition->Defeated.Num()==Count);
    E->ResetEncounter();Check(TEXT("Checkpoint reload can restore an unsaved defeated enemy"),!E->Dead&&E->Health==100&&E->GetCharacterMovement()->MovementMode==MOVE_Walking&&E->GetCapsuleComponent()->GetCollisionEnabled()==ECollisionEnabled::QueryAndPhysics);
    UFWExpedition* Imported=nullptr;FString ImportError;
    FString Fixture=TEXT(R"({"version":4,"seals":[0,2],"defeated":["king","king"],"won":true,"quests":{"flags":{"mirror_shield":true,"amulet_truth":true,"witness_0":true},"discovered":["hearth","invalid"],"tracked":8,"checkpoint":"bad"},"life":{"silver":124,"hour":18,"day":3},"riding":[{"id":"hearth_horse","x":5,"z":390,"yaw":0,"energy":84}]})");
    Check(TEXT("Legacy v4 import succeeds"),FWLegacySave::Convert(Fixture,Imported,ImportError));
    Check(TEXT("Legacy relics and victories preserved"),Imported&&Imported->Flags.Contains(TEXT("amulet_truth"))&&Imported->Defeated.Num()==1&&Imported->Won);
    Check(TEXT("Legacy checkpoint and discovery validation"),Imported&&Imported->CheckpointId==TEXT("hearth")&&Imported->Discovered.Num()==1);
    Check(TEXT("Legacy vehicle axes convert to centimetres"),Imported&&Imported->ParkedVehicles.Num()==1&&Imported->ParkedVehicles[0].Position.Equals(FVector(-39000,500,0)));
    TArray<uint8> Buffer;UGameplayStatics::SaveGameToMemory(Imported,Buffer);auto* RoundTrip=Cast<UFWExpedition>(UGameplayStatics::LoadGameFromMemory(Buffer));
    Check(TEXT("Unreal save memory round trip preserves legacy data"),RoundTrip&&RoundTrip->Silver==124&&RoundTrip->OriginalLegacyJson==Fixture);
    Check(TEXT("Unsupported save version rejected"),!FWLegacySave::Convert(TEXT("{\"version\":99}"),Imported,ImportError));
    Check(TEXT("Malformed JSON rejected"),!FWLegacySave::Convert(TEXT("broken"),Imported,ImportError));
    AFWEnemy* Victim=nullptr;
    for(TActorIterator<AFWEnemy> It(P->GetWorld());It;++It)if(It->Identity==TEXT("showcase_sentry_1")&&It->Death&&It->GetMesh()->GetSkeletalMeshAsset()){Victim=*It;break;}
    Check(TEXT("Imported encounter character and death animation present"),Victim!=nullptr);
    const FName HeadBone=Victim&&Victim->GetMesh()->GetBoneIndex(TEXT("head"))!=INDEX_NONE?TEXT("head"):TEXT("spine_006");
    Check(TEXT("Imported character skeleton is upright"),Victim&&Victim->GetMesh()->GetBoneIndex(HeadBone)!=INDEX_NONE&&Victim->GetMesh()->GetBoneIndex(TEXT("foot_l"))!=INDEX_NONE&&Victim->GetMesh()->GetBoneLocation(HeadBone).Z>Victim->GetMesh()->GetBoneLocation(TEXT("foot_l")).Z+100);
    if(Victim)
    {
        Check(TEXT("Authored attacks share encounter skeleton"),Victim->Idle&&Victim->AttackVariants.Num()==3&&!Victim->AttackVariants.ContainsByPredicate([Victim](const auto& Clip){return !Clip||Clip->GetSkeleton()!=Victim->Idle->GetSkeleton();}));
        Check(TEXT("Front and rear impacts select distinct clips"),Victim->SelectHitReaction(-Victim->GetActorForwardVector())==Victim->HitReaction&&Victim->SelectHitReaction(Victim->GetActorForwardVector())==Victim->HitBack&&Victim->HitBack!=Victim->HitReaction);
        Check(TEXT("Left and right impacts select distinct clips"),Victim->SelectHitReaction(Victim->GetActorRightVector())==Victim->HitLeft&&Victim->SelectHitReaction(-Victim->GetActorRightVector())==Victim->HitRight&&Victim->HitLeft!=Victim->HitRight);
        Victim->RiposteWindow=0;Victim->ReceiveStrike(1,FVector::ZeroVector,true);
        Check(TEXT("Heavy stagger does not grant a parry riposte"),Victim->Stagger>0&&Victim->RiposteWindow==0);
        Check(TEXT("Authored equipment replaces temporary enemy sword"),Victim->BuiltInWeapon&&!Victim->Weapon->GetStaticMesh());
    }
    if(Victim)Victim->Die(FVector(80,0,0));
    AFWEnemy* Orc=nullptr;
    for(TActorIterator<AFWEnemy> It(P->GetWorld());It;++It)if(It->Identity==TEXT("showcase_sentry_3")){Orc=*It;break;}
    Check(TEXT("Owned orc rig is upright at human combat scale"),Orc&&Orc->GetMesh()->GetBoneIndex(TEXT("CC_Base_Head"))!=INDEX_NONE&&Orc->GetMesh()->GetBoneLocation(TEXT("CC_Base_Head")).Z>Orc->GetMesh()->GetBoneLocation(TEXT("CC_Base_L_Foot")).Z+100);
    Check(TEXT("Orc combat clips use its own skeleton"),Orc&&Orc->Idle&&Orc->Walk&&Orc->Death&&Orc->AttackVariants.Num()==3&&Orc->Death->GetSkeleton()==Orc->Idle->GetSkeleton()&&!Orc->AttackVariants.ContainsByPredicate([Orc](const auto& Clip){return !Clip||Clip->GetSkeleton()!=Orc->Idle->GetSkeleton();}));
    Check(TEXT("Orc weapon attaches to its right hand"),Orc&&Orc->Weapon->GetStaticMesh()&&Orc->Weapon->GetAttachSocketName()==TEXT("CC_Base_R_Hand"));
    if(Orc)Orc->Die(FVector(80,0,0));
    AFWEnemy* Ste=nullptr;
    for(TActorIterator<AFWEnemy> It(P->GetWorld());It;++It)if(It->Identity==TEXT("jailer")){Ste=*It;break;}
    Check(TEXT("Ste uses the owned armored DemonLord2 with integrated hammer"),Ste&&Ste->DisplayName==TEXT("Ste Chlomaine")&&Ste->GetMesh()->GetSkeletalMeshAsset()&&Ste->GetMesh()->GetSkeletalMeshAsset()->GetName()==TEXT("SK_DemonLord2_C")&&Ste->BuiltInWeapon);
    Check(TEXT("Ste attacks and death use the original DemonLord2 skeleton"),Ste&&Ste->Idle&&Ste->Walk&&Ste->Death&&Ste->AttackVariants.Num()==3&&Ste->Death->GetSkeleton()==Ste->Idle->GetSkeleton()&&!Ste->AttackVariants.ContainsByPredicate([Ste](const auto& Clip){return !Clip||Clip->GetSkeleton()!=Ste->Idle->GetSkeleton();}));
    if(Ste)Ste->Die(FVector(80,0,0));
    TMap<FString,int32> Herds;bool LivestockClips=true,LivestockCare=true;int32 AnimalCount=0;
    for(TActorIterator<AFWLivestock> Animal(P->GetWorld());Animal;++Animal)
    {
        ++AnimalCount;++Herds.FindOrAdd(Animal->Village+TEXT("/")+Animal->Species);
        LivestockClips&=Animal->Body->GetSkeletalMeshAsset()&&Animal->Idle&&Animal->Walk&&Animal->Feed&&Animal->Idle!=Animal->Walk&&Animal->Walk!=Animal->Feed&&Animal->Idle->GetSkeleton()==Animal->Walk->GetSkeleton()&&Animal->Idle->GetSkeleton()==Animal->Feed->GetSkeleton();
        const int Visits=Animal->TendingVisits;Animal->Tend(.1f);Animal->Tend(.1f);
        LivestockCare&=Animal->Activity==TEXT("Being fed")&&Animal->TendingVisits<=Visits+1&&Animal->TendingVisits>0;
    }
    bool HerdCounts=true;for(const auto& Herd:Herds)HerdCounts&=Herd.Value==2;
    Check(TEXT("Three villages each contain two cows, pigs and sheep"),AnimalCount==18&&Herds.Num()==9&&HerdCounts);
    Check(TEXT("Livestock have separate compatible idle, walking and feeding clips"),LivestockClips&&AnimalCount==18);
    Check(TEXT("Repeated farmer tending feeds without counting every frame as a visit"),LivestockCare&&AnimalCount==18);
    TWeakObjectPtr<AFWEnemy> WeakSte(Ste);
    TWeakObjectPtr<AFWEnemy> WeakOrc(Orc);
    TWeakObjectPtr<AFWEnemy> WeakVictim(Victim);
    FTimerHandle FinishHandle;
    P->GetWorld()->GetTimerManager().SetTimer(FinishHandle,[Checks,Passed,WeakVictim,WeakOrc,WeakSte]() mutable
    {
        const bool PhysicsPassed=WeakVictim.IsValid()&&WeakVictim->Dead&&WeakVictim->GetMesh()->IsSimulatingPhysics()&&!WeakVictim->GetMesh()->GetComponentLocation().ContainsNaN();
        auto Result=MakeShared<FJsonObject>();Result->SetStringField(TEXT("name"),TEXT("Authored death transitions to live ragdoll physics"));Result->SetBoolField(TEXT("passed"),PhysicsPassed);Checks.Add(MakeShared<FJsonValueObject>(Result));Passed&=PhysicsPassed;
        const bool OrcPhysics=WeakOrc.IsValid()&&WeakOrc->Dead&&WeakOrc->GetMesh()->IsSimulatingPhysics()&&!WeakOrc->GetMesh()->GetComponentLocation().ContainsNaN();
        auto OrcResult=MakeShared<FJsonObject>();OrcResult->SetStringField(TEXT("name"),TEXT("Orc death transitions to finite ragdoll physics"));OrcResult->SetBoolField(TEXT("passed"),OrcPhysics);Checks.Add(MakeShared<FJsonValueObject>(OrcResult));Passed&=OrcPhysics;
        const bool StePhysics=WeakSte.IsValid()&&WeakSte->Dead&&WeakSte->GetMesh()->IsSimulatingPhysics()&&!WeakSte->GetMesh()->GetComponentLocation().ContainsNaN();
        auto SteResult=MakeShared<FJsonObject>();SteResult->SetStringField(TEXT("name"),TEXT("Ste death animation transitions to finite ragdoll physics"));SteResult->SetBoolField(TEXT("passed"),StePhysics);Checks.Add(MakeShared<FJsonValueObject>(SteResult));Passed&=StePhysics;
        auto Root=MakeShared<FJsonObject>();Root->SetArrayField(TEXT("checks"),Checks);Root->SetBoolField(TEXT("all_passed"),Passed);Root->SetStringField(TEXT("scope"),TEXT("First-playable combat, collision, imported animation/death physics and legacy save conversion; not full migration acceptance"));
        FString Text;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectDir()/TEXT("Verification/runtime-tests.json")));
        UE_LOG(LogTemp,Display,TEXT("FROSTWATCH_TESTS %s (%d checks)"),Passed?TEXT("PASS"):TEXT("FAIL"),Checks.Num());
        FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
    },4.0f,false);
}

static const FString SaveSlot=TEXT("FrostwatchUnreal_Expedition_v1");

AFWCharacter::AFWCharacter()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.bTickEvenWhenPaused=true;
    GetCapsuleComponent()->InitCapsuleSize(34,88);
    GetCharacterMovement()->MaxWalkSpeed=300;
    GetCharacterMovement()->JumpZVelocity=420;GetCharacterMovement()->AirControl=.25f;JumpMaxCount=1;JumpMaxHoldTime=.08f;
    GetCharacterMovement()->BrakingDecelerationWalking=2200;
    GetCharacterMovement()->MaxAcceleration=2200;
    GetCharacterMovement()->NavAgentProps.bCanCrouch=true;
    GetCharacterMovement()->NavAgentProps.bCanSwim=true;GetCharacterMovement()->Buoyancy=1.f;
    GetCharacterMovement()->MaxSwimSpeed=220;GetCharacterMovement()->BrakingDecelerationSwimming=250;
    Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("Eyes"));
    Camera->SetupAttachment(GetCapsuleComponent());
    Camera->SetRelativeLocation(FVector(0,0,66));
    Camera->bUsePawnControlRotation=true;
    Camera->FieldOfView=85;
    Sword=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Sword"));
    Sword->SetupAttachment(Camera);
    Sword->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Sword->CastShadow=false;
    Shield=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shield"));
    Shield->SetupAttachment(Camera);
    Shield->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Shield->CastShadow=false;
    // A small local glow lights the hands; most light belongs on the route ahead.
    LanternLight=CreateDefaultSubobject<UPointLightComponent>(TEXT("TravelLantern"));LanternLight->SetupAttachment(Camera);LanternLight->SetRelativeLocation(FVector(-30,10,-35));LanternLight->SetIntensityUnits(ELightUnits::Lumens);LanternLight->SetIntensity(30);LanternLight->SetAttenuationRadius(450);LanternLight->SetLightColor(FLinearColor(1,.78f,.52f));LanternLight->SetCastShadows(false);LanternLight->SetVisibility(false);
    LanternPathLight=CreateDefaultSubobject<USpotLightComponent>(TEXT("LanternPath"));LanternPathLight->SetupAttachment(Camera);
    LanternPathLight->SetRelativeLocation(FVector(55,0,-25));LanternPathLight->SetRelativeRotation(FRotator(-16,0,0));
    // Bounded falloff avoids blowing out nearby faces while retaining useful light on the path.
    LanternPathLight->SetUseInverseSquaredFalloff(false);LanternPathLight->SetIntensityUnits(ELightUnits::Unitless);LanternPathLight->SetIntensity(8);LanternPathLight->SetLightFalloffExponent(8);LanternPathLight->SetAttenuationRadius(1800);
    LanternPathLight->SetSpecularScale(.35f);LanternPathLight->ShadowResolutionScale=.5f;
    LanternPathLight->SetInnerConeAngle(35);LanternPathLight->SetOuterConeAngle(68);LanternPathLight->SetLightColor(FLinearColor(1,.8f,.56f));
    LanternPathLight->SetCastShadows(true);LanternPathLight->SetIndirectLightingIntensity(0);LanternPathLight->SetVolumetricScatteringIntensity(0);LanternPathLight->SetVisibility(false);
    bUseControllerRotationYaw=true;
}
void AFWCharacter::BeginPlay()
{
    Super::BeginPlay();
    FWLoadCombatAudio(this);
    Expedition=Cast<UFWExpedition>(UGameplayStatics::CreateSaveGameObject(UFWExpedition::StaticClass()));
    if(FParse::Param(FCommandLine::Get(),TEXT("FrostwatchCapture")))Expedition->Lantern=FParse::Param(FCommandLine::Get(),TEXT("CaptureLantern"));
    Expedition->Checkpoint=GetActorLocation();
    InitialLocation=GetActorLocation();
    GConfig->GetFloat(TEXT("Frostwatch"),TEXT("Sensitivity"),Sensitivity,GGameUserSettingsIni);
    GConfig->GetBool(TEXT("Frostwatch"),TEXT("InvertY"),InvertY,GGameUserSettingsIni);
    GConfig->GetBool(TEXT("Frostwatch"),TEXT("ReducedMotion"),ReducedMotion,GGameUserSettingsIni);
    GConfig->GetFloat(TEXT("Frostwatch"),TEXT("StoryTextScale"),StoryTextScale,GGameUserSettingsIni);
    StoryTextScale=FMath::Clamp(StoryTextScale,1.f,1.5f);
    FWApplyGraphicsBudget(GEngine->GetGameUserSettings()->GetGlobalIlluminationQuality());
    if(auto* WeaponMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/MedievalSword/StaticMeshes/SM_Sword_08.SM_Sword_08"))) Sword->SetStaticMesh(WeaponMesh);
    if(auto* WeaponMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Showcase/SM_Shield.SM_Shield"))) Shield->SetStaticMesh(WeaponMesh);
    Sword->SetRelativeScale3D(FVector(.55f));Shield->SetRelativeScale3D(FVector(.45f));
    SetupViewModel();
    if(auto* PC=Cast<APlayerController>(GetController())) {PC->SetInputMode(FInputModeGameOnly()); PC->bShowMouseCursor=false;}
    Started=FParse::Param(FCommandLine::Get(),TEXT("TraversalTest"))||FParse::Param(FCommandLine::Get(),TEXT("FrostwatchTest"))||FParse::Param(FCommandLine::Get(),TEXT("FrostwatchCapture"))||FParse::Param(FCommandLine::Get(),TEXT("CampaignTest"));
    if(!Started)SetScreen(TEXT("Title"));
}
void AFWCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    Input->BindAxis("Forward",this,&AFWCharacter::MoveForward);
    Input->BindAxis("Strafe",this,&AFWCharacter::MoveRight);
    Input->BindAxis("LookX",this,&AFWCharacter::LookX);
    Input->BindAxis("LookY",this,&AFWCharacter::LookY);
    Input->BindAction("Light",IE_Pressed,this,&AFWCharacter::Light);
    Input->BindAction("Heavy",IE_Pressed,this,&AFWCharacter::Heavy);
    Input->BindAction("Feint",IE_Pressed,this,&AFWCharacter::Feint);
    Input->BindAction("Block",IE_Pressed,this,&AFWCharacter::BlockStart);
    Input->BindAction("Block",IE_Released,this,&AFWCharacter::BlockEnd);
    Input->BindAction("Dodge",IE_Pressed,this,&AFWCharacter::Dodge);
    Input->BindAction("Jump",IE_Pressed,this,&AFWCharacter::JumpStart);
    Input->BindAction("Jump",IE_Released,this,&ACharacter::StopJumping);
    Input->BindAction("Sprint",IE_Pressed,this,&AFWCharacter::SprintStart);
    Input->BindAction("Sprint",IE_Released,this,&AFWCharacter::SprintEnd);
    Input->BindAction("Crouch",IE_Pressed,this,&AFWCharacter::CrouchToggle);
    Input->BindAction("Interact",IE_Pressed,this,&AFWCharacter::Interact);
    Input->BindAction("Journal",IE_Pressed,this,&AFWCharacter::ToggleJournal).bExecuteWhenPaused=true;
    Input->BindAction("Menu",IE_Pressed,this,&AFWCharacter::ToggleMenu).bExecuteWhenPaused=true;
    Input->BindAction("Save",IE_Pressed,this,&AFWCharacter::Save);
    Input->BindAction("Load",IE_Pressed,this,&AFWCharacter::Load);
    Input->BindAction("Map",IE_Pressed,this,&AFWCharacter::ToggleMap).bExecuteWhenPaused=true;
    Input->BindAction("Lantern",IE_Pressed,this,&AFWCharacter::ToggleLantern);
    auto& Keys=Input->BindKey(EKeys::AnyKey,IE_Pressed,this,&AFWCharacter::AnyKey);Keys.bExecuteWhenPaused=true;Keys.bConsumeInput=false;
}
void AFWCharacter::MoveForward(float V)
{
    ForwardIntent=V;
    if(!CanAct())return;
    if(MountedHorse&&MountedHorse->Cart&&FMath::Abs(V)>.1f)
    {
        FVector Rear=GetActorLocation()-GetActorForwardVector()*260-FVector(0,0,20);FHitResult Hit;FCollisionQueryParams Q;Q.AddIgnoredActor(this);Q.AddIgnoredActor(MountedHorse);
        if(GetWorld()->SweepSingleByChannel(Hit,Rear,Rear+GetActorForwardVector()*V*70,GetActorQuat(),ECC_Visibility,FCollisionShape::MakeBox(FVector(115,82,40)),Q))return;
    }
    AddMovementInput(GetActorForwardVector(),V);
}
void AFWCharacter::MoveRight(float V){StrafeIntent=V;if(CanAct()){if(MountedHorse)AddControllerYawInput(V*GetWorld()->GetDeltaSeconds()*45);else AddMovementInput(GetActorRightVector(),V);}}
void AFWCharacter::LookX(float V){if(CanAct()) AddControllerYawInput(V*Sensitivity);}
void AFWCharacter::LookY(float V){if(CanAct()) AddControllerPitchInput(V*Sensitivity*(InvertY?-1:1));}
void AFWCharacter::SprintStart(){Sprinting=true;}
void AFWCharacter::SprintEnd(){Sprinting=false;}
void AFWCharacter::CrouchToggle(){if(MountedHorse||!CanAct())return;if(bIsCrouched) UnCrouch();else Crouch();}
void AFWCharacter::ToggleJournal(){if(RebindTarget.IsEmpty()&&Started&&Health>0)SetScreen(Screen==TEXT("Journal")?TEXT(""):TEXT("Journal"));}
void AFWCharacter::ToggleMenu(){if(!RebindTarget.IsEmpty()){RebindTarget.Empty();return;}if(Screen==TEXT("Opening")){MenuAction(TEXT("Skip opening"));return;}if(!Started)SetScreen(TEXT("Title"));else if(Health<=0)SetScreen(TEXT("Death"));else SetScreen(Screen.IsEmpty()?TEXT("Pause"):TEXT(""));}
void AFWCharacter::Light(){StartAttack(false);}
void AFWCharacter::Heavy(){StartAttack(true);}
void AFWCharacter::StartAttack(bool Strong)
{
    if(MountedHorse){Notify(TEXT("Dismount to fight."));return;}
    if(!CanAct()||GuardBroken>0||GetCharacterMovement()->IsSwimming())return;
    const EFWStrike Move=FWChooseStrike(Strong,ForwardIntent,StrafeIntent,bIsCrouched,GuardHeld,ChainTime>0?LightChain:0);
    // Capture direction when pressed so changing footwork cannot change a queued move.
    if(AttackAge>=0){if(AttackAge>=.12f){QueuedAttack=Strong?2:1;QueuedMove=Move;}return;}
    BeginStrike(Move);
}
void AFWCharacter::BeginStrike(EFWStrike Move)
{
    if(!CanAct()||MountedHorse||GuardBroken>0||FeintRecovery>0||GetCharacterMovement()->IsSwimming())return;
    const bool Strong=Move==EFWStrike::Heavy||Move==EFWStrike::Thrust||Move==EFWStrike::Rising||Move==EFWStrike::Finisher;
    const float Cost=Move==EFWStrike::Bash?20:Move==EFWStrike::Finisher?34:Strong?27:15;
    if(Stamina<Cost){Notify(TEXT("Recover your endurance"));return;}
    Blocking=false;Stamina-=Cost;HeavyAttack=Strong;AttackAge=0;HitResolved=false;StrikeHold=0;AttackSoundPlayed=false;
    AttackLength=Strong?.72f:.48f;ImpactAt=Strong?.32f:.19f;Combo=(Combo+1)%3;
    StrikeMove=Move;
    if(Move==EFWStrike::Thrust){ImpactAt=.24f;AttackLength=.61f;}
    if(Move==EFWStrike::Bash){ImpactAt=.16f;AttackLength=.5f;}
    if(Move==EFWStrike::Finisher){ImpactAt=.38f;AttackLength=.82f;}
    if(Move==EFWStrike::LowCut){ImpactAt=.23f;AttackLength=.58f;}
    LightChain=Strong||Move==EFWStrike::Bash?0:FMath::Min(2,LightChain+1);ChainTime=AttackLength+.55f;
    UpdateViewModel(true);
}
void AFWCharacter::Feint()
{
    if(!CanAct()||AttackAge<0||AttackAge>=ImpactAt*.7f||Stamina<6||FeintRecovery>0)return;
    Stamina-=6;AttackAge=-1;QueuedAttack=0;StrikeHold=0;LightChain=0;ChainTime=0;FeintRecovery=.18f;
    Blocking=false;CombatNotify(TEXT("FEINT"),.6f);
}
void AFWCharacter::BlockStart(){if(!MountedHorse&&CanAct()){GuardHeld=true;if(AttackAge<0&&Stamina>3&&!Blocking&&GuardBroken<=0){Blocking=true;BlockAge=0;}}}
void AFWCharacter::BlockEnd(){Blocking=false;GuardHeld=false;}
void AFWCharacter::Dodge()
{
    if(MountedHorse||GetCharacterMovement()->IsSwimming())return;
    if(!CanAct()||DodgeCooldown>0||Stamina<24)return;
    if(AttackAge>=0&&AttackAge<ImpactAt)return;
    AttackAge=-1;QueuedAttack=0;Blocking=false;GuardHeld=false;StrikeHold=0;Stamina-=24;DodgeCooldown=.65f;Invulnerability=.19f;
    FVector Direction=GetLastMovementInputVector().GetSafeNormal();
    if(Direction.IsNearlyZero()) Direction=-GetActorForwardVector();
    LaunchCharacter(Direction*750+FVector(0,0,50),true,false);
}
void AFWCharacter::Tick(float Dt)
{
    Super::Tick(Dt);
    // An explicitly requested diagnostic must measure an active encounter,
    // not switch to the death menu halfway through the GPU sample window.
    if(FParse::Param(FCommandLine::Get(),TEXT("FrostwatchCapture"))&&FParse::Param(FCommandLine::Get(),TEXT("CaptureBenchmark")))Invulnerability=1.f;
    static bool CombatContactTestRan=false;
    if(!CombatContactTestRan&&FParse::Param(FCommandLine::Get(),TEXT("CombatContactTest"))&&GetWorld()->GetRealTimeSeconds()>4){CombatContactTestRan=true;FWRunCombatContactTests(this);return;}
    if(FParse::Param(FCommandLine::Get(),TEXT("FrostwatchCapture")))FWCaptureCombatDamage(this);
    static bool DisplayTestRan=false;
    if(!DisplayTestRan&&FParse::Param(FCommandLine::Get(),TEXT("DisplayTest"))&&GetWorld()->GetRealTimeSeconds()>3){DisplayTestRan=true;FWRunDisplayTests();return;}
    if(FParse::Param(FCommandLine::Get(),TEXT("TraversalTest")))ValidateTraversal();
    if(FParse::Param(FCommandLine::Get(),TEXT("SwimmingTest")))ValidateSwimming(Dt);
    static bool CampaignTestRan=false;
    if(!CampaignTestRan&&FParse::Param(FCommandLine::Get(),TEXT("CampaignTest"))&&GetWorld()->GetTimeSeconds()>3){CampaignTestRan=true;FWCampaign::Test(this);return;}
    static bool TestRan=false;
    if(!TestRan&&FParse::Param(FCommandLine::Get(),TEXT("FrostwatchTest"))&&GetWorld()->GetTimeSeconds()>3){TestRan=true;RunFrostwatchTests(this);return;}
    static bool Captured=false;
    FWUpdateDisplayPreview();
    if(FParse::Param(FCommandLine::Get(),TEXT("FrostwatchCapture"))){FWTrackCaptureFrame();if(FParse::Param(FCommandLine::Get(),TEXT("PerformanceSweep")))FWRunPerformanceSweep(this,GetWorld()->GetRealTimeSeconds());}
    float CaptureSeconds=45.f;FParse::Value(FCommandLine::Get(),TEXT("CaptureSeconds="),CaptureSeconds);CaptureSeconds=FMath::Max(5.f,CaptureSeconds);
    static bool MenuCaptureSet=false;
    FString CaptureScreen;
    if(!MenuCaptureSet&&FParse::Param(FCommandLine::Get(),TEXT("FrostwatchCapture"))&&GetWorld()->GetRealTimeSeconds()>8&&FParse::Value(FCommandLine::Get(),TEXT("CaptureScreen="),CaptureScreen))
    {MenuCaptureSet=true;if(CaptureScreen==TEXT("Opening")){IntroPage=0;MenuAction(TEXT("Intro next"));}else if(CaptureScreen==TEXT("Dialogue")){FWCampaign::Interact(this,TEXT("mara"));}else if(CaptureScreen==TEXT("ReadBook")){FWCampaign::Interact(this,TEXT("abbey_page"));}else if(CaptureScreen==TEXT("SaveProblem")){SaveError=TEXT("Save failed. Progress is still in memory. Check free disk space and folder access, then retry before exiting.");Started=true;SetScreen(TEXT("Save problem"));}else if(CaptureScreen==TEXT("Pause")){Started=true;SetScreen(CaptureScreen);}else SetScreen(CaptureScreen);if(FParse::Param(FCommandLine::Get(),TEXT("CaptureDisplayPreview")))FWPreviewDisplayMode();}
    static bool OrcCapturePositioned=false;
    FString CaptureActor;const bool NamedPortrait=FParse::Value(FCommandLine::Get(),TEXT("CaptureActor="),CaptureActor);
    if(!OrcCapturePositioned&&FParse::Param(FCommandLine::Get(),TEXT("FrostwatchCapture"))&&(NamedPortrait||FParse::Param(FCommandLine::Get(),TEXT("OrcPortrait")))&&GetWorld()->GetTimeSeconds()>2)
    {
        OrcCapturePositioned=true;
        for(TActorIterator<AFWEnemy> It(GetWorld());It;++It)if(It->Identity==(NamedPortrait?CaptureActor:TEXT("showcase_sentry_3")))
        {
            SetActorLocation(It->GetActorLocation()+It->GetActorForwardVector()*(It->Dragon?1600:380));
            GetCharacterMovement()->StopMovementImmediately();
            if(auto* PC=Cast<APlayerController>(GetController()))PC->SetControlRotation((It->GetActorLocation()+FVector(0,0,15)-Camera->GetComponentLocation()).Rotation());
            if(!FParse::Param(FCommandLine::Get(),TEXT("CaptureLiveWorld")))for(TActorIterator<AFWEnemy> Other(GetWorld());Other;++Other)Other->SetActorTickEnabled(false);
            Sword->SetVisibility(false);Shield->SetVisibility(false); // Clear the diagnostic portrait view.
            GetMesh()->SetVisibility(false);
            It->GetCharacterMovement()->StopMovementImmediately();
            It->Play(It->Idle,true);
            float WalkPhase=0;
            if(FParse::Value(FCommandLine::Get(),TEXT("CaptureWalkPhase="),WalkPhase)&&It->Walk)
            {
                It->GetCharacterMovement()->StopMovementImmediately();
                It->GetMesh()->PlayAnimation(It->Walk,false);
                It->GetMesh()->SetPosition(It->Walk->GetPlayLength()*FMath::Clamp(WalkPhase,0.f,.999f),false);
                It->GetMesh()->bPauseAnims=true;
            }
            if(FParse::Param(FCommandLine::Get(),TEXT("OrcAttackPose")))
            {
                It->GetMesh()->PlayAnimation(It->Attack,false);
                It->GetMesh()->SetPosition(.75f,false);
                It->GetMesh()->bPauseAnims=true;
            }
            break;
        }
    }
    static bool HorseCapturePositioned=false;FString CaptureHorse;
    if(!HorseCapturePositioned&&FParse::Param(FCommandLine::Get(),TEXT("FrostwatchCapture"))&&FParse::Value(FCommandLine::Get(),TEXT("CaptureHorse="),CaptureHorse)&&GetWorld()->GetTimeSeconds()>2)
    {
        HorseCapturePositioned=true;
        for(TActorIterator<AFWHorse> It(GetWorld());It;++It)if(It->Identity==CaptureHorse)
        {
            const FVector Focus=It->GetActorTransform().TransformPosition(FVector(0,140,110));
            SetActorLocation(It->GetActorTransform().TransformPosition(FVector(620,640,200)));
            GetCharacterMovement()->DisableMovement();
            if(auto* PC=Cast<APlayerController>(GetController()))PC->SetControlRotation((Focus-Camera->GetComponentLocation()).Rotation());
            Sword->SetVisibility(false);Shield->SetVisibility(false);GetMesh()->SetVisibility(false);
            if(!FParse::Param(FCommandLine::Get(),TEXT("CaptureLiveWorld")))for(TActorIterator<AFWEnemy> Other(GetWorld());Other;++Other)Other->SetActorTickEnabled(false);
            break;
        }
    }
    static bool SceneCapturePositioned=false;FString CaptureAtText,CaptureLookText;
    if(!SceneCapturePositioned&&FParse::Param(FCommandLine::Get(),TEXT("FrostwatchCapture"))&&FParse::Value(FCommandLine::Get(),TEXT("CaptureAt="),CaptureAtText,false)&&FParse::Value(FCommandLine::Get(),TEXT("CaptureLookAt="),CaptureLookText,false)&&GetWorld()->GetTimeSeconds()>2)
    {
        FVector At,Look;
        if(At.InitFromString(CaptureAtText.Replace(TEXT(","),TEXT(" ")))&&Look.InitFromString(CaptureLookText.Replace(TEXT(","),TEXT(" "))))
        {
            SceneCapturePositioned=true;SetActorLocation(At);GetCharacterMovement()->DisableMovement();
            if(auto* PC=Cast<APlayerController>(GetController()))PC->SetControlRotation((Look-Camera->GetComponentLocation()).Rotation());
            Sword->SetVisibility(false);Shield->SetVisibility(false);GetMesh()->SetVisibility(false);
            if(!FParse::Param(FCommandLine::Get(),TEXT("CaptureLiveWorld")))for(TActorIterator<AFWEnemy> Other(GetWorld());Other;++Other)Other->SetActorTickEnabled(false);
        }
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("FrostwatchCapture"))&&FParse::Param(FCommandLine::Get(),TEXT("CaptureGuard")))Blocking=true;
    static double CaptureAssetsReadyAt=0,CaptureRecordedAt=0;
    bool CaptureAssetsReady=true;
#if WITH_EDITOR
    CaptureAssetsReady=FAssetCompilingManager::Get().GetNumRemainingAssets()==0;
#endif
    if(!CaptureAssetsReady)CaptureAssetsReadyAt=0;
    else if(CaptureAssetsReadyAt==0)CaptureAssetsReadyAt=GetWorld()->GetRealTimeSeconds();
    if(!Captured&&FParse::Param(FCommandLine::Get(),TEXT("FrostwatchCapture"))&&CaptureAssetsReady&&GetWorld()->GetRealTimeSeconds()>CaptureSeconds&&GetWorld()->GetRealTimeSeconds()-CaptureAssetsReadyAt>5)
    {
        Captured=true;CaptureRecordedAt=GetWorld()->GetRealTimeSeconds();FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/TEXT("Verification/first-playable.png"),true,false);
        FWRecordCaptureGraphics(this);
        FString Audit=TEXT("Player ")+GetActorLocation().ToString()+TEXT(" Camera ")+Camera->GetComponentLocation().ToString()+TEXT("\n");
        if(auto* Mode=GetWorld()->GetAuthGameMode<AFWGameMode>())
        {
            FString MusicAudit;
            for(auto Audio:Mode->Ambience)if(Audio)MusicAudit+=FString::Printf(TEXT("Inn=%s Playing=%d Volume=%.4f\n"),*Audio->GetComponentLocation().ToString(),Audio->IsPlaying()?1:0,Audio->VolumeMultiplier);
            FFileHelper::SaveStringToFile(MusicAudit,*(FPaths::ProjectDir()/TEXT("Verification/inn-music.txt")));
        }
        for(TActorIterator<AFWEnemy> It(GetWorld());It;++It)
        {
            FVector Origin,Extent;It->GetActorBounds(false,Origin,Extent);
            Audit+=It->Identity+TEXT(" location ")+It->GetActorLocation().ToString()+TEXT(" bounds ")+Origin.ToString()+TEXT(" extent ")+Extent.ToString()+TEXT("\n");
            if(It->Weapon->GetStaticMesh())Audit+=TEXT("Weapon ")+It->Weapon->GetComponentTransform().ToString()+TEXT(" blade tip ")+It->Weapon->GetComponentTransform().TransformPosition(FVector(0,0,87)).ToString()+TEXT(" hand ")+It->GetMesh()->GetSocketTransform(It->Weapon->GetAttachSocketName()).ToString()+TEXT("\n");
            if(It->Friendly)for(int32 I=0;I<It->GetMesh()->GetNumBones();I++)Audit+=It->GetMesh()->GetBoneName(I).ToString()+TEXT(" ")+It->GetMesh()->GetBoneLocation(It->GetMesh()->GetBoneName(I)).ToString()+TEXT("\n");
        }
        FFileHelper::SaveStringToFile(Audit,*(FPaths::ProjectDir()/TEXT("Verification/character-pose.txt")));
    }
    if(Captured&&FParse::Param(FCommandLine::Get(),TEXT("FrostwatchCapture"))&&GetWorld()->GetRealTimeSeconds()>CaptureRecordedAt+5)FPlatformMisc::RequestExit(false);
    if(Menu)return;
    if(FParse::Param(FCommandLine::Get(),TEXT("FrostwatchCapture")))FWCaptureSliceCombat(this);
    LanternLight->SetVisibility(Expedition->Lantern);
    LanternPathLight->SetVisibility(Expedition->Lantern);
    if(Expedition->Lantern)
    {
        // Keep the light source on this side of nearby walls, including while looking down.
        const FVector Eye=Camera->GetComponentLocation(),Desired=Camera->GetComponentTransform().TransformPosition(FVector(55,0,-25));
        FHitResult Wall;FCollisionQueryParams Query(SCENE_QUERY_STAT(LanternPlacement),false,this);
        const bool Blocked=GetWorld()->LineTraceSingleByChannel(Wall,Eye,Desired,ECC_Visibility,Query);
        LanternPathLight->SetWorldLocation(Blocked?Wall.ImpactPoint+Wall.ImpactNormal*5:Desired);
    }
    FWCampaign::Update(this,Dt);
    UpdateSwimming(Dt);
    GuardBroken=FMath::Max(0.f,GuardBroken-Dt);
    BlockAge+=Dt;DodgeCooldown=FMath::Max(0.f,DodgeCooldown-Dt);Invulnerability=FMath::Max(0.f,Invulnerability-Dt);
    HurtFlash=FMath::Max(0.f,HurtFlash-Dt*2);HitFeedback=FMath::Max(0.f,HitFeedback-Dt);
    GuardImpact=FMath::Max(0.f,GuardImpact-Dt);ParryFeedback=FMath::Max(0.f,ParryFeedback-Dt);
    MessageTime=FMath::Max(0.f,MessageTime-Dt);
    CombatMessageTime=FMath::Max(0.f,CombatMessageTime-Dt);
    const bool Running=!MountedHorse&&Sprinting&&!Blocking&&GetVelocity().Size2D()>50&&Stamina>1;
    GetCharacterMovement()->MaxWalkSpeed=Blocking?230:(Running?560:300);
    if(MountedHorse)GetCharacterMovement()->MaxWalkSpeed=MountedHorse->Cart?(Sprinting&&MountedHorse->Energy>1?650:350):(Sprinting&&MountedHorse->Energy>1?1300:400);
    Stamina=FMath::Clamp(Stamina+Dt*(Running?-10:(Blocking?3:(AttackAge<0?23:0))),0.f,MaxStamina());
    if(Stamina<=0)Sprinting=false;
    UpdateTraversalSound(Dt);
    UpdateCombat(Dt);
    AutoSaveClock+=Dt;if(AutoSaveClock>30){AutoSaveClock=0;Save();}
}
void AFWCharacter::UpdateCombat(float Dt)
{
    FeintRecovery=FMath::Max(0.f,FeintRecovery-Dt);ChainTime=FMath::Max(0.f,ChainTime-Dt);if(ChainTime<=0)LightChain=0;
    if(AttackAge>=0)
    {
        const float PreviousAge=AttackAge;
        if(StrikeHold>0)StrikeHold=FMath::Max(0.f,StrikeHold-Dt);else AttackAge+=Dt;
        // Sound begins as the blade accelerates, not at the start of anticipation.
        if(!AttackSoundPlayed&&AttackAge>=ImpactAt-.06f){AttackSoundPlayed=true;FWCombatAudio(this,EFWCombatCue::Swing,GetActorLocation());}
        // Subsample the traversed arc so a long frame cannot skip the contact window.
        if(!HitResolved&&AttackAge>=ImpactAt&&PreviousAge<ImpactAt+.12f)ResolveStrike(PreviousAge,AttackAge);
        if(GuardHeld&&StrikeMove!=EFWStrike::Bash&&GuardBroken<=0&&AttackAge>=ImpactAt+.12f&&Stamina>3){AttackAge=-1;QueuedAttack=0;StrikeHold=0;Blocking=true;BlockAge=0;}
        else if(AttackAge>=AttackLength||(QueuedAttack&&AttackAge>=AttackLength*.72f))
        {AttackAge=-1;int32 Queue=QueuedAttack;QueuedAttack=0;if(Queue)BeginStrike(QueuedMove);else if(GuardHeld&&Stamina>3&&GuardBroken<=0){Blocking=true;BlockAge=0;}}
    }
    if(AttackAge<0&&GuardHeld&&!Blocking&&GuardBroken<=0&&FeintRecovery<=0&&Stamina>3){Blocking=true;BlockAge=0;}
    // Anticipation, fast cutting motion and follow-through; consecutive cuts alternate direction.
    FVector Blade=FVector::ZeroVector;FRotator BladeRotation(-30,-10,15);
    if(AttackAge>=0)
    {
        const float Direction=StrikeMove==EFWStrike::LeftCut?-1.f:StrikeMove==EFWStrike::RightCut?1.f:Combo==2?-1.f:1.f;
        const float Wind=FMath::SmoothStep(0.f,ImpactAt*.70f,AttackAge);
        const float Cut=FMath::SmoothStep(ImpactAt*.70f,ImpactAt+.09f,AttackAge);
        const float Recover=FMath::SmoothStep(ImpactAt+.09f,AttackLength,AttackAge);
        const float Arc=(Wind-2*Cut)*(1-Recover);
        Blade=HeavyAttack?FVector(24*Cut*(1-Recover),-12*Arc,24*Arc):FVector(22*Cut*(1-Recover),28*Direction*Arc,12*Arc);
        BladeRotation+=HeavyAttack?FRotator(-65*Arc,-15*Arc,20*Arc):FRotator(-25*Arc,65*Direction*Arc,-45*Direction*Arc);
        if(StrikeMove==EFWStrike::Thrust||(StrikeMove==EFWStrike::Cut&&Combo==0))
        {Blade=FVector((48*Cut-8*Wind)*(1-Recover),-8*Wind*(1-Recover),5*Wind*(1-Recover));BladeRotation=FRotator(-30+28*Wind*(1-Recover),-10,15-15*Wind*(1-Recover));}
        if(StrikeMove==EFWStrike::Rising){Blade.Z=-24*Arc;BladeRotation.Pitch=-30+65*Arc;}
        if(StrikeMove==EFWStrike::LowCut){Blade.Z-=22*Wind*(1-Recover);BladeRotation.Pitch+=25*Wind*(1-Recover);}
        if(StrikeMove==EFWStrike::Bash){Blade=FVector(-8,12,-5)*Wind*(1-Recover);BladeRotation=FRotator(-30,-10,15);}
    }
    const float Blend=1-FMath::Exp(-Dt*32.f);
    Sword->SetRelativeLocation(FMath::Lerp(Sword->GetRelativeLocation(),SwordBase+Blade-FVector(HitFeedback*20,0,0),Blend));
    Sword->SetRelativeRotation(FQuat::Slerp(Sword->GetRelativeRotation().Quaternion(),BladeRotation.Quaternion(),Blend));
    const float Bash=AttackAge>=0&&StrikeMove==EFWStrike::Bash?FMath::Sin(FMath::Clamp(AttackAge/AttackLength,0.f,1.f)*PI):0;
    Shield->SetRelativeLocation(FMath::Lerp(Shield->GetRelativeLocation(),FVector((Blocking?55:50)-GuardImpact*24+Bash*36,Blocking?-28:-25+Bash*15,Blocking?-24:-28+Bash*12),Blend));
    Shield->SetRelativeRotation(FQuat::Slerp(Shield->GetRelativeRotation().Quaternion(),FRotator(0,Blocking?90:65,0).Quaternion(),Blend));
    UpdateViewModel();
}
AFWEnemy* AFWCharacter::TargetEnemy(float Range,FHitResult* OutHit) const
{
    FHitResult Hit; FCollisionQueryParams Params(SCENE_QUERY_STAT(FrostwatchAim),false,this);
    const FVector Start=Camera->GetComponentLocation();
    GetWorld()->SweepSingleByChannel(Hit,Start,Start+Camera->GetForwardVector()*Range,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(15),Params);
    if(OutHit)*OutHit=Hit;
    return Cast<AFWEnemy>(Hit.GetActor());
}
bool AFWCharacter::FindStrikeContact(float FromAge,float ToAge,FHitResult& Hit) const
{
    const float From=FMath::Clamp((FromAge-ImpactAt)/.12f,0.f,1.f);
    const float To=FMath::Clamp((ToAge-ImpactAt)/.12f,0.f,1.f);
    const int Steps=FMath::Max(1,FMath::CeilToInt((To-From)*12));
    const FVector Origin=Camera->GetComponentLocation();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(FrostwatchBlade),false,this);
    for(int I=0;I<=Steps;I++)
    {
        const float Phase=FMath::Lerp(From,To,float(I)/Steps);
        FRotator Aim=Camera->GetComponentRotation();
        const bool Straight=StrikeMove==EFWStrike::Thrust||StrikeMove==EFWStrike::Bash;
        if(!Straight&&HeavyAttack)Aim.Pitch+=FMath::Lerp(16.f,-16.f,Phase)*(StrikeMove==EFWStrike::Rising?-1:1);
        else if(!Straight&&(Combo!=0||StrikeMove!=EFWStrike::Cut))Aim.Yaw+=FMath::Lerp(18.f,-18.f,Phase)*(StrikeMove==EFWStrike::LeftCut?-1:StrikeMove==EFWStrike::RightCut?1:Combo==2?-1:1);
        if(StrikeMove==EFWStrike::LowCut)Aim.Pitch-=18;
        const FVector End=Origin+Aim.Vector()*(StrikeMove==EFWStrike::Bash?155:StrikeMove==EFWStrike::Thrust?285:HeavyAttack?270:235);
        if(GetWorld()->SweepSingleByChannel(Hit,Origin,End,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(Straight?8:12),Params))
        {
            // Keep the blocking capsule/world ordering, but refine wound placement
            // against the animated body rather than the capsule's round surface.
            if(auto* Enemy=Cast<AFWEnemy>(Hit.GetActor()))
            {
                FHitResult BodyHit;
                if(Enemy->GetMesh()->LineTraceComponent(BodyHit,Origin,End,Params))Hit=BodyHit;
                else if(Enemy->GetMesh()->GetSkeletalMeshAsset())
                {
                    const FVector AtDepth=Origin+Aim.Vector()*FVector::DotProduct(Enemy->GetActorLocation()-Origin,Aim.Vector());
                    auto* Body=Enemy->GetMesh();
                    const FName Chest=Body->GetBoneIndex(TEXT("spine_02"))!=INDEX_NONE?FName(TEXT("spine_02")):FName(TEXT("CC_Base_Spine01"));
                    const FVector FromChest=AtDepth-Body->GetBoneLocation(Chest);
                    // A broad movement capsule must not turn empty space beside
                    // an amputated arm into another torso hit.
                    if(FWSelectHitPart(Enemy,AtDepth)==INDEX_NONE&&(FromChest.Size2D()>24||FMath::Abs(FromChest.Z)>40))continue;
                    Hit.ImpactPoint=AtDepth;
                }
                FCollisionQueryParams Occlusion=Params;Occlusion.AddIgnoredActor(Enemy);FHitResult Obstacle;
                if(GetWorld()->LineTraceSingleByChannel(Obstacle,Origin,Hit.ImpactPoint,ECC_Visibility,Occlusion))Hit=Obstacle;
            }
            return true;
        }
    }
    return false;
}
void AFWCharacter::ResolveStrike(float FromAge,float ToAge)
{
    FHitResult Impact;if(!FindStrikeContact(FromAge,ToAge,Impact))return;
    HitResolved=true;
    AFWEnemy* Enemy=Cast<AFWEnemy>(Impact.GetActor());
    if(!Enemy){StrikeHold=.045f;HitFeedback=.10f;FWCombatAudio(this,EFWCombatCue::Clash,Impact.ImpactPoint);return;}
    if(Enemy->Dead||Enemy->Friendly)return;
    const bool Backstab=bIsCrouched&&!Enemy->Alerted&&FVector::DotProduct(Enemy->GetActorForwardVector(),(GetActorLocation()-Enemy->GetActorLocation()).GetSafeNormal())<-.35f;
    const bool Riposte=Enemy->RiposteWindow>0;
    Enemy->RiposteWindow=0;
    const float PreviousHealth=Enemy->Health;
    const bool Bash=StrikeMove==EFWStrike::Bash;
    const float Damage=Bash?8:Backstab?180:Riposte?65:StrikeMove==EFWStrike::Finisher?68:HeavyAttack?48:HasFlag(TEXT("supplies_done"))?37:31;
    Enemy->ReceiveStrike(Damage,Camera->GetForwardVector(),!Bash&&(HeavyAttack||Riposte),!Bash&&Backstab,Impact.ImpactPoint,!Bash);
    if(Bash&&Enemy->Health<PreviousHealth){Enemy->Stagger=1.1f;Enemy->RiposteWindow=.9f;}
    if(Enemy->Health>=PreviousHealth){if(!Enemy->Dragon)Notify(TEXT("Your strike is turned aside by the ward."));return;}
    HitFeedback=.18f;StrikeHold=HeavyAttack?.065f:.035f;
    FWCombatAudio(this,EFWCombatCue::Impact,Impact.ImpactPoint);
    const TCHAR* Zones[]={TEXT("HEAD"),TEXT("LEFT ARM"),TEXT("RIGHT ARM"),TEXT("LEFT LEG"),TEXT("RIGHT LEG")};
    const FString Zone=Enemy->LastHitPart>=0?Zones[Enemy->LastHitPart]:TEXT("TORSO");
    CombatNotify(FString(!Bash&&Backstab?TEXT("STEALTH STRIKE"):!Bash&&Riposte?TEXT("RIPOSTE"):FWStrikeName(StrikeMove))+TEXT(" / ")+Zone,1.2f);
}
void AFWCharacter::ReceiveEnemyHit(AFWEnemy* Enemy,float Damage)
{
    if(Health<=0||Invulnerability>0)return;
    const FVector ToEnemy=(Enemy->GetActorLocation()-GetActorLocation()).GetSafeNormal2D();
    if(Blocking&&FVector::DotProduct(GetActorForwardVector(),ToEnemy)>.35f)
    {
        GuardImpact=.22f;
        FWCombatAudio(this,BlockAge<.23f?EFWCombatCue::Parry:EFWCombatCue::Clash,GetActorLocation());
        if(BlockAge<.23f&&Stamina>=8){ParryFeedback=.35f;Enemy->Windup=0;Enemy->GetCharacterMovement()->StopMovementImmediately();Enemy->Stagger=1.4f;Enemy->RiposteWindow=1.4f;Enemy->Play(Enemy->HitReaction,false);Stamina=FMath::Min(MaxStamina(),Stamina+12);CombatNotify(TEXT("PARRY - strike to riposte"));return;}
        if(Stamina>=Damage*.8f){Stamina-=Damage*.8f;CombatNotify(TEXT("BLOCKED"),1);return;}
        Stamina=0;Blocking=false;GuardHeld=false;GuardBroken=.65f;AttackAge=-1;QueuedAttack=0;StrikeHold=0;CombatNotify(TEXT("GUARD BROKEN - create space"));
    }
    Health=FMath::Max(0.f,Health-Damage);HurtFlash=.65f;FWCombatAudio(this,EFWCombatCue::Impact,GetActorLocation());
    if(Health<=0){Blocking=false;SetScreen(TEXT("Death"));}
}
bool AFWCharacter::HasFlag(const FString& Value) const {return Expedition&&Expedition->Flags.Contains(Value);}
void AFWCharacter::Flag(const FString& Value){if(Expedition){Expedition->Flags.AddUnique(Value);Save();}}
void AFWCharacter::Notify(const FString& Text,float Seconds){Message=Text;MessageTime=Seconds;}
void AFWCharacter::CombatNotify(const FString& Text,float Seconds){CombatMessage=Text;CombatMessageTime=Seconds;}
FString AFWCharacter::Objective() const
{
    return FWCampaign::Objective(this);
}
void AFWCharacter::Interact()
{
    if(!CanAct())return;
    if(MountedHorse){MountedHorse->Dismount();return;}
    for(TActorIterator<AFWHorse> H(GetWorld());H;++H)if(FVector::Dist(GetActorLocation(),H->GetActorLocation())<350){H->Mount(this);return;}
    if(auto* Enemy=TargetEnemy(360);Enemy&&Enemy->Friendly){if(!FWCampaign::Interact(this,Enemy->Identity))Notify(TEXT("The watch keeps these roads safe."));return;}
    for(TActorIterator<AFWInteractable> It(GetWorld());It;++It)
    {
        if(It->IsHidden()||FVector::Dist(GetActorLocation(),It->GetActorLocation())>330)continue;
        FHitResult Hit;FCollisionQueryParams Params;Params.AddIgnoredActor(this);
        GetWorld()->LineTraceSingleByChannel(Hit,Camera->GetComponentLocation(),It->GetActorLocation(),ECC_Visibility,Params);
        if(Hit.bBlockingHit&&Hit.GetActor()!=*It)continue;
        if(!FWCampaign::Interact(this,It->Identity)){Flag(It->Identity);Notify(It->Title+TEXT(": ")+It->Words,12);}return;
    }
}
void AFWCharacter::Save()
{
    SaveExpedition();
}
bool AFWCharacter::SaveExpedition()
{
    if(!Expedition||!Started||Restoring||FParse::Param(FCommandLine::Get(),TEXT("TraversalTest"))||FParse::Param(FCommandLine::Get(),TEXT("FrostwatchTest"))||FParse::Param(FCommandLine::Get(),TEXT("FrostwatchCapture"))||FParse::Param(FCommandLine::Get(),TEXT("CampaignTest")))return true;
    Expedition->RespawnVersion=1;
    Expedition->ParkedVehicles.Empty();
    for(TActorIterator<AFWHorse> H(GetWorld());H;++H){FFWParkedVehicle V;V.Identity=H->Identity;V.Position=H->GetActorLocation();V.Yaw=H->GetActorRotation().Yaw;V.Energy=H->Energy;Expedition->ParkedVehicles.Add(V);}
    const bool OK=FWSave::Write(Expedition,SaveSlot);
    SaveError=OK?FString():TEXT("Save failed. Progress is still in memory. Check free disk space and folder access, then retry before exiting.");
    return OK;
}
void AFWCharacter::Load()
{
    if(FParse::Param(FCommandLine::Get(),TEXT("FrostwatchTest")))return;
    bool Recovered=false;auto* Saved=FWSave::Load(SaveSlot,Recovered);
    if(!Saved){SaveError=TEXT("Cannot read the expedition or its recovery copy. Your current session and save files have not been replaced.");SetScreen(TEXT("Save problem"));return;}
    Restoring=true;
    if(MountedHorse)MountedHorse->Dismount(true);
    Expedition=Saved;SaveError.Empty();
    Health=100;Stamina=100;AttackAge=-1;QueuedAttack=0;Blocking=false;GuardHeld=false;StrikeHold=0;Invulnerability=0;DodgeCooldown=0;GuardBroken=0;
    GetCharacterMovement()->StopMovementImmediately();
    if(Expedition)SetActorLocation(Expedition->Checkpoint,false,nullptr,ETeleportType::TeleportPhysics);
    Notify(Recovered?TEXT("Recovered the previous save and returned to camp. The latest changes may be missing."):TEXT("Returned to camp"),Recovered?12:4);
    for(TActorIterator<AFWEnemy> It(GetWorld());It;++It)
    {
        It->RestoreEncounter(Expedition);
    }
    Expedition->RespawnVersion=1;
    for(TActorIterator<AFWHorse> H(GetWorld());H;++H)H->Restore(this);
    FWConsequences::Update(this,true);Restoring=false;Started=true;SetScreen(TEXT(""));
}

AFWEnemy::AFWEnemy()
{
    PrimaryActorTick.bCanEverTick=true;
    GetCapsuleComponent()->InitCapsuleSize(34,88);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    GetCharacterMovement()->MaxWalkSpeed=230;
    GetCharacterMovement()->bOrientRotationToMovement=false;
    AutoPossessAI=EAutoPossessAI::PlacedInWorldOrSpawned;
    Weapon=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Weapon"));
    Weapon->SetupAttachment(GetMesh());Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WorkTool=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WorkTool"));WorkTool->SetupAttachment(GetMesh());WorkTool->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void AFWEnemy::Assemble()
{
    TArray<USkeletalMeshComponent*> Existing;GetComponents(Existing);
    for(auto* Component:Existing)if(Component->ComponentHasTag(TEXT("AssembledPart"))){RemoveInstanceComponent(Component);Component->DestroyComponent();}
    for(int32 I=0;I<Parts.Num();I++)
    {
        auto* Component=NewObject<USkeletalMeshComponent>(this);
        AddInstanceComponent(Component);Component->SetupAttachment(GetMesh());Component->SetSkeletalMeshAsset(Parts[I]);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);Component->ComponentTags.Add(TEXT("AssembledPart"));Component->RegisterComponent();
        Component->SetLeaderPoseComponent(GetMesh());
    }
}
void AFWEnemy::BeginPlay()
{
    UninjuredSpeed=GetCharacterMovement()->MaxWalkSpeed;
    Super::BeginPlay();Home=GetActorLocation();HomeTransform=GetActorTransform();MeshHomeTransform=GetMesh()->GetRelativeTransform();Assemble();Play(Idle,true);
    FWLoadDamageParts(this);
    // Keep animation time and daily routines advancing offscreen, but refresh the
    // full skeleton only when rendered. Visible animation keeps its full update rate.
    GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPose;
    if(Dragon)SetupDragon();
    if(Job==TEXT("stable hand")||Job==TEXT("herder"))
    {
        WorkTool->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
        const FName Hand=GetMesh()->GetBoneIndex(TEXT("hand_R"))!=INDEX_NONE?TEXT("hand_R"):TEXT("hand_r");
        if(GetMesh()->GetBoneIndex(Hand)!=INDEX_NONE)WorkTool->AttachToComponent(GetMesh(),FAttachmentTransformRules::SnapToTargetNotIncludingScale,Hand);
        else WorkTool->SetRelativeLocation(FVector(12,25,95));
        WorkTool->SetRelativeScale3D(FVector(.23f,.23f,.28f));
    }
    if(!Friendly&&!BuiltInWeapon)
    {
        Weapon->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Showcase/SM_Sword.SM_Sword")));
        const FName Hand=GetMesh()->GetBoneIndex(TEXT("CC_Base_R_Hand"))!=INDEX_NONE?TEXT("CC_Base_R_Hand"):TEXT("hand_R");
        Weapon->AttachToComponent(GetMesh(),FAttachmentTransformRules::SnapToTargetNotIncludingScale,Hand);
        const bool OrcHand=Hand==TEXT("CC_Base_R_Hand");
        Weapon->SetRelativeRotation(FRotator(0,0,OrcHand?-90:90));
        Weapon->SetRelativeScale3D(FVector(OrcHand?.45f:.82f));
    }
}
void AFWEnemy::Play(UAnimSequence* Clip,bool Loop)
{
    if(!Clip||Clip==CurrentClip)return;
    if(Dragon)GetMesh()->PlayAnimation(Clip,Loop);
    else
    {
        auto* Motion=Cast<UFWMotion>(GetMesh()->GetAnimInstance());
        if(!Motion){GetMesh()->SetAnimInstanceClass(UFWMotion::StaticClass());Motion=Cast<UFWMotion>(GetMesh()->GetAnimInstance());}
        if(Motion)Motion->Transition(Clip,Loop,Loop?.18f:.09f);
        else GetMesh()->PlayAnimation(Clip,Loop);
    }
    GetMesh()->SetPlayRate(1.f);CurrentClip=Clip;
}
void AFWEnemy::ResetEncounter()
{
    SetActorTickInterval(0);
    SetActorHiddenInGame(false);RoutinePhase.Empty();BreathTime=BreathHitClock=ReflectFlash=0;
    GetMesh()->SetSimulatePhysics(false);
    FWResetCombatDamage(this);
    GetMesh()->AttachToComponent(GetRootComponent(),FAttachmentTransformRules::KeepRelativeTransform);
    GetMesh()->SetRelativeTransform(MeshHomeTransform);
    GetMesh()->SetCollisionProfileName(TEXT("CharacterMesh"));
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    SetActorTransform(HomeTransform,false,nullptr,ETeleportType::TeleportPhysics);
    GetCharacterMovement()->StopMovementImmediately();GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    if(Dragon)GetCharacterMovement()->SetMovementMode(MOVE_Flying);
    Health=MaxHealth;Dead=false;Alerted=false;StoryBarkPlayed=false;Windup=Recovery=Stagger=DeathAge=RiposteWindow=0;AttackIndex=0;CurrentClip=nullptr;Play(Idle,true);
    RespawnCheckClock=0;
}
bool AFWEnemy::CanRespawn() const
{
    return !Friendly&&!Dragon&&(Identity.StartsWith(TEXT("guard_"))||Identity.StartsWith(TEXT("orc_")));
}
void AFWEnemy::RestoreEncounter(UFWExpedition* Save)
{
    if(!Save)return;
    const bool LegacyGroup=!Save->OriginalLegacyJson.IsEmpty()&&((Identity.StartsWith(TEXT("guard_"))&&Save->Defeated.Contains(TEXT("guard")))||(Identity.StartsWith(TEXT("orc_"))&&Save->Defeated.Contains(TEXT("orc"))));
    const bool WasDefeated=Save->Defeated.Contains(Identity)||LegacyGroup;
    if(CanRespawn()&&Save->RespawnVersion==0&&WasDefeated&&!Save->RespawnAtHour.Contains(Identity))
        Save->RespawnAtHour.Add(Identity,Save->CampaignHours()+24.f);
    ResetEncounter();
    if(!Friendly&&(CanRespawn()?Save->RespawnAtHour.Contains(Identity):WasDefeated))Die(FVector::ZeroVector);
}
bool AFWEnemy::RespawnDue(const UFWExpedition* Save,const FVector& PlayerLocation) const
{
    if(!Dead||!CanRespawn()||!Save)return false;
    const float* Due=Save->RespawnAtHour.Find(Identity);
    return Due&&Save->CampaignHours()>=*Due&&FVector::DistSquared2D(PlayerLocation,Home)>FMath::Square(8000.f)
        &&FVector::DistSquared(PlayerLocation,GetActorLocation())>FMath::Square(2500.f);
}
bool AFWEnemy::TryRespawn(AFWCharacter* Player)
{
    if(!Player||!Player->Started||Player->Restoring||!Player->CanAct()||!RespawnDue(Player->Expedition,Player->GetActorLocation()))return false;
    const FVector View=Player->Camera->GetComponentLocation(),Aim=Home+FVector(0,0,60);
    if(FVector::DotProduct(Player->Camera->GetForwardVector(),(Aim-View).GetSafeNormal())>0)
    {
        FHitResult Hit;FCollisionQueryParams Params;Params.AddIgnoredActor(Player);Params.AddIgnoredActor(this);
        GetWorld()->LineTraceSingleByChannel(Hit,View,Aim,ECC_Visibility,Params);
        if(!Hit.bBlockingHit)return false;
    }
    Player->Expedition->RespawnAtHour.Remove(Identity);
    ResetEncounter();Player->Save();
    return true;
}
void AFWEnemy::Tick(float Dt)
{
    Super::Tick(Dt);
    FWUpdateBlood(this,Dt);
    if(Dead)
    {
        for(auto Jet:BreathJet)Jet->SetVisibility(false);
        for(auto Jet:ReflectedJet)Jet->SetVisibility(false);
        DeathAge+=Dt;
        const bool LostLeg=SeveredBones.Contains(FWSeverBone(GetMesh()->GetSkeletalMeshAsset(),3))||SeveredBones.Contains(FWSeverBone(GetMesh()->GetSkeletalMeshAsset(),4));
        const float RagdollAt=LostLeg?.08f:Death?FMath::Clamp(Death->GetPlayLength()*.9f,.8f,3.5f):1.2f;
        if(DeathAge>RagdollAt&&DeathAge-Dt<=RagdollAt&&GetMesh()->GetPhysicsAsset())
        {
            GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));GetMesh()->SetSimulatePhysics(true);
            for(FName Bone:SeveredBones)GetMesh()->TermBodiesBelow(Bone);
            GetMesh()->AddImpulse(FinalImpulse,NAME_None,true);
        }
        if(DeathAge>12&&DeathAge-Dt<=12)
        {
            if(GetMesh()->IsSimulatingPhysics())GetMesh()->PutAllRigidBodiesToSleep();
            // Settled corpses only need the slow reinforcement check.
            SetActorTickInterval(1.f);
        }
        RespawnCheckClock+=Dt;
        if(RespawnCheckClock>=2.f){RespawnCheckClock=0;TryRespawn(Cast<AFWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0)));}
        return;
    }
    auto* Player=Cast<AFWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
    if(Friendly){if(Player)UpdateRoutine(Dt,Player);else Play(Idle,true);return;}
    if(!Player||Player->Health<=0||Player->Journal)return;
    if(Dragon){TickDragon(Dt,Player);return;}
    RiposteWindow=FMath::Max(0.f,RiposteWindow-Dt);
    if(Stagger>0){Stagger=FMath::Max(0.f,Stagger-Dt);GetCharacterMovement()->StopMovementImmediately();return;}
    if(Recovery>0){Recovery=FMath::Max(0.f,Recovery-Dt);GetCharacterMovement()->StopMovementImmediately();return;}
    const FVector Offset=Player->GetActorLocation()-GetActorLocation();
    float Distance=Offset.Size2D();
    if(Distance<1000&&!Player->bIsCrouched)Alerted=true;
    if(Distance<350)Alerted=true;
    if(!Alerted||Distance>2300){Play(Idle,true);return;}
    if(Distance<1000)FWStory::EnemyBark(Player,this);
    if(Windup>0)
    {
        GetCharacterMovement()->StopMovementImmediately();
        // Commit the final portion of the swing so a sidestep can escape its arc.
        if(Windup>.35f){SetActorRotation(FMath::RInterpTo(GetActorRotation(),FRotator(0,Offset.Rotation().Yaw,0),Dt,4));MeleeDirection=GetActorForwardVector();}
        Windup-=Dt;
        if(!MeleeSoundPlayed&&Windup<=.18f){MeleeSoundPlayed=true;FWCombatAudio(Player,EFWCombatCue::Swing,GetActorLocation());}
        if(Windup<=0)
        {
            FHitResult Hit;FCollisionQueryParams Params;Params.AddIgnoredActor(this);
            GetWorld()->LineTraceSingleByChannel(Hit,GetActorLocation(),Player->GetActorLocation(),ECC_Visibility,Params);
            if(IsInsideMeleeArc(Player->GetActorLocation())&&(!Hit.bBlockingHit||Hit.GetActor()==Player))Player->ReceiveEnemyHit(this,StrikeDamage*FWInjuryAttackScale(this));
            Recovery=CurrentClip?FMath::Max(.5f,CurrentClip->GetPlayLength()-MeleeWindupLength):1.1f;
        }
        return;
    }
    SetActorRotation(FMath::RInterpTo(GetActorRotation(),FRotator(0,Offset.Rotation().Yaw,0),Dt,6));
    if(Distance>(SeveredBones.Contains(FWSeverBone(GetMesh()->GetSkeletalMeshAsset(),2))?95:175)){AddMovementInput(Offset.GetSafeNormal2D());Play(Walk,true);}
    else
    {
        int32 Attackers=0;for(TActorIterator<AFWEnemy> It(GetWorld());It;++It)if(It->Windup>0&&!It->Dead&&!It->Friendly&&!It->Dragon&&FVector::DistSquared2D(It->GetActorLocation(),Player->GetActorLocation())<FMath::Square(700.f))Attackers++;
        if(Attackers<2){GetCharacterMovement()->StopMovementImmediately();CurrentClip=nullptr;UAnimSequence* Clip=Attack;if(AttackVariants.Num())Clip=AttackVariants[AttackIndex++%AttackVariants.Num()];MeleeWindupLength=Clip?FMath::Clamp(Clip->GetPlayLength()*.35f,.48f,.85f):.75f;Windup=MeleeWindupLength;MeleeSoundPlayed=false;MeleeDirection=GetActorForwardVector();Play(Clip,false);}else Play(Idle,true);
    }
}
bool AFWEnemy::IsInsideMeleeArc(const FVector& Target) const
{
    const FVector Offset=Target-GetActorLocation();
    const float Reach=SeveredBones.Contains(FWSeverBone(GetMesh()->GetSkeletalMeshAsset(),2))?125.f:230.f;
    return Offset.Size2D()<Reach&&FMath::Abs(Offset.Z)<150&&FVector::DotProduct(MeleeDirection.GetSafeNormal2D(),Offset.GetSafeNormal2D())>.55f;
}
void AFWEnemy::ReceiveStrike(float Damage,const FVector& Direction,bool Strong,bool Execute,const FVector& HitPoint,bool Blade)
{
    if(Dead||Friendly)return;
    if(Dragon){if(auto* P=Cast<AFWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0)))P->Notify(TEXT("The drake's ward turns steel. Reflect its breath with the Mirror Shield."));return;}
    auto* Player=Cast<AFWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
    if(Identity==TEXT("king")&&(!Player||!Player->HasFlag(TEXT("amulet_truth"))))return;
    if(Damage<=0)return;
    const FVector Contact=HitPoint.IsNearlyZero()?GetActorLocation()+FVector(0,0,35):HitPoint;
    LastHitPart=Execute?0:FWSelectHitPart(this,Contact);
    if(Blade&&LastHitPart>=0)PartDamage[LastHitPart]+=Damage;
    Alerted=true;Health-=Damage*(LastHitPart==0?1.25f:1.f);Windup=0;Recovery=0;Stagger=Strong?.8f:.42f;
    if(PartDamage[3]>=20||PartDamage[4]>=20)GetCharacterMovement()->MaxWalkSpeed=UninjuredSpeed*.58f;
    GetCharacterMovement()->StopMovementImmediately();
    LaunchCharacter(Direction*(Strong?230:85),true,false);
    CurrentClip=nullptr;Play(SelectHitReaction(Direction),false);
    if(Blade){FWImpactMark(this,Contact,Direction);FWEmitBlood(this,Contact,Direction,Strong?18:9);}
    const float Threshold=LastHitPart==0?55:LastHitPart>=3?60:40;
    if(Blade&&(Strong||Execute)&&LastHitPart>=0&&(Health<=0||PartDamage[LastHitPart]>=Threshold||Execute))
    {
        if(FWSeverFinisher(this,Contact,Direction,Execute,LastHitPart)&&(LastHitPart==0||LastHitPart>=3))Health=0;
    }
    if(Health<=0)
    {
        Die(Direction*(Strong||Execute?180:70));
    }
}
UAnimSequence* AFWEnemy::SelectHitReaction(const FVector& Direction) const
{
    const FVector Incoming=GetActorTransform().InverseTransformVectorNoScale(-Direction);
    UAnimSequence* Clip=HitReaction;
    if(FMath::Abs(Incoming.Y)>FMath::Abs(Incoming.X))Clip=Incoming.Y<0?HitLeft:HitRight;
    else if(Incoming.X<0)Clip=HitBack;
    return Clip?Clip:Idle.Get();
}
void AFWEnemy::Die(const FVector& Direction)
{
    if(Dead)return;
    auto* Player=Cast<AFWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
    if(Identity==TEXT("king")&&(!Player||!Player->HasFlag(TEXT("amulet_truth")))){Health=1;return;}
    Dead=true;Health=0;Windup=0;FinalImpulse=Direction;
    // A queued strike launch otherwise re-enables falling after the capsule is
    // disabled, dropping the corpse through the world before ragdoll starts.
    GetCharacterMovement()->ClearAccumulatedForces();GetCharacterMovement()->StopMovementImmediately();GetCharacterMovement()->DisableMovement();
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CurrentClip=nullptr;Play(Death,false);
    if(Player&&Player->Expedition)
    {
        Player->Expedition->Defeated.AddUnique(Identity);
        if(CanRespawn()&&!Player->Restoring)
        {
            Player->Expedition->RespawnAtHour.Add(Identity,Player->Expedition->CampaignHours()+24.f);
        }
        int32 Live=0;for(TActorIterator<AFWEnemy> It(GetWorld());It;++It)if(!It->Dead&&!It->Friendly)Live++;
        if(Live==0)Player->Flag(TEXT("showcase_guards"));
        Player->Save();
    }
}
AFWInteractable::AFWInteractable(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=.2f;Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));SetRootComponent(Mesh);FireLight=CreateDefaultSubobject<UPointLightComponent>(TEXT("SignalLight"));FireLight->SetupAttachment(Mesh);FireLight->SetRelativeLocation(FVector(0,0,170));FireLight->SetIntensity(0);FireLight->SetAttenuationRadius(1500);FireLight->SetLightColor(FLinearColor(1,.42f,.09f));FireLight->SetCastShadows(false);}
void AFWInteractable::Tick(float Dt)
{
    Super::Tick(Dt);auto* P=Cast<AFWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));if(!P||!P->Expedition)return;
    const bool Lit=(Identity.StartsWith(TEXT("fire_"))&&P->HasFlag(Identity))||(Identity==TEXT("won")&&P->Expedition->Won)||(Identity==TEXT("lore_falls")&&P->HasFlag(TEXT("lore_falls")));FireLight->SetIntensity(Lit?3200+FMath::Sin(GetWorld()->GetTimeSeconds()*7)*300:0);
    if(Identity.StartsWith(TEXT("seal:")))Mesh->SetVisibility(!P->Expedition->Seals.Contains(FCString::Atoi(*Identity.Mid(5))));
}
AFWGameMode::AFWGameMode(){PrimaryActorTick.bCanEverTick=true;DefaultPawnClass=AFWCharacter::StaticClass();HUDClass=AFWHUD::StaticClass();}
void AFWGameMode::BeginPlay()
{
    Super::BeginPlay();
    // The replacement water remains visible beyond the legacy architecture's 500 m
    // cutoff. Its cliff is a landmark, so retain the backdrop at the same distances.
    for(TActorIterator<AActor> It(GetWorld());It;++It)
    {
        TArray<UStaticMeshComponent*> Components;It->GetComponents(Components);
        for(auto* C:Components)if(C->GetStaticMesh()&&C->GetStaticMesh()->GetPathName()==TEXT("/Game/Campaign/World/chunk_9_4_m13.chunk_9_4_m13"))
        {
            C->SetCullDistance(0);C->bAllowCullDistanceVolume=false;
            UE_LOG(LogTemp,Display,TEXT("FROSTWATCH_WATERFALL_BACKDROP retained beyond legacy 500 m cutoff"));
        }
    }
    for(FVector Location:{FVector(-43300,-2800,545),FVector(-36900,-43000,3500),FVector(-34900,46500,2760)})
    {
        if(auto* Sound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Campaign/Audio/inn.inn")))
        {
            auto* Audio=NewObject<UAudioComponent>(this);
            Audio->bAutoActivate=false;Audio->bAutoDestroy=false;
            Audio->bOverrideAttenuation=true;Audio->AttenuationOverrides.bAttenuate=true;
            Audio->AttenuationOverrides.AttenuationShapeExtents=FVector(700);
            Audio->AttenuationOverrides.FalloffDistance=600;
            Audio->SetSound(Sound);Audio->SetVolumeMultiplier(0);
            Audio->RegisterComponentWithWorld(GetWorld());Audio->SetWorldLocation(Location);
            Ambience.Add(Audio);
        }
    }
    if(auto* Player=Cast<AFWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));Player&&Player->Expedition)Player->Expedition->Checkpoint=Player->GetActorLocation();
    UE_LOG(LogTemp,Display,TEXT("FROSTWATCH_PLAYABLE_READY"));
}
float AFWGameMode::InnMusicGain(const FVector& Player,const FVector& Inn,bool Enabled)
{
    const FVector D=(Player-Inn).GetAbs();
    // These extents match the three authored inn interiors, in Unreal centimetres.
    if(!Enabled||D.X>=1070||D.Y>=970||D.Z>=420)return 0;
    return .18f*FMath::Clamp(FMath::Min((1070-D.X)/180,(970-D.Y)/180),0.,1.);
}
void AFWGameMode::Tick(float Dt)
{
    Super::Tick(Dt);
    auto* P=Cast<AFWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
    for(auto Audio:Ambience)if(Audio)
    {
        const float Gain=P&&P->Expedition?InnMusicGain(P->GetActorLocation(),Audio->GetComponentLocation(),P->Started&&P->Expedition->Music):0;
        if(Gain<=0){Audio->Stop();Audio->SetVolumeMultiplier(0);}
        else{if(!Audio->IsPlaying())Audio->Play();Audio->SetVolumeMultiplier(FMath::FInterpTo(Audio->VolumeMultiplier,Gain,Dt,2));}
    }
}
