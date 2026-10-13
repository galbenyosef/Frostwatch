#include "FWCombatEffects.h"
#include "FWGame.h"
#include "FWSave.h"
#include "FWMotion.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "TimerManager.h"
#include "Components/DecalComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"

void FWRunCombatContactTests(AFWCharacter* Player)
{
    TArray<TSharedPtr<FJsonValue>> Checks;bool Passed=true;
    auto Check=[&](const FString& Name,bool OK){auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("name"),Name);R->SetBoolField(TEXT("passed"),OK);Checks.Add(MakeShared<FJsonValueObject>(R));Passed&=OK;};
    Check(TEXT("Six recorded clash variants are available"),Player->CombatClashes.Num()==6);
    FWSave::Test(Check);
    Player->Notify(TEXT("Quest message remains available"));Player->CombatNotify(TEXT("HIT"));
    Check(TEXT("Combat feedback does not replace quest guidance"),Player->Message==TEXT("Quest message remains available")&&Player->CombatMessage==TEXT("HIT")&&Player->CombatMessageTime>0);
    Check(TEXT("Four recorded sword swings are available"),Player->CombatSwings.Num()==4);
    Check(TEXT("Four recorded impact variants are available"),Player->CombatImpacts.Num()==4);
    // Evaluate actual skeletal poses: a transition must preserve continuity,
    // advance the incoming clip immediately, and still reach its target pose.
    bool MotionCovered=false;
    for(TActorIterator<AFWEnemy> It(Player->GetWorld());It;++It)if(!It->Dragon&&!It->Friendly&&It->Idle&&It->Walk)
    {
        auto* Mesh=It->GetMesh();It->ResetEncounter();
        auto* Motion=Cast<UFWMotion>(Mesh->GetAnimInstance());
        Check(TEXT("Humanoid uses the smooth sequence player"),Motion!=nullptr);
        if(!Motion)break;
        const auto Visibility=Mesh->VisibilityBasedAnimTickOption;
        Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        auto Sample=[&](float Dt){Mesh->TickAnimation(Dt,false);Mesh->RefreshBoneTransforms();return Mesh->GetBoneSpaceTransforms();};
        Motion->Transition(It->Idle,true,0);const auto Before=Sample(.2f);
        Motion->Transition(It->Walk,true,.18f);const auto Early=Sample(.001f);
        float StartError=0;for(int32 I=0;I<Before.Num()&&I<Early.Num();++I)StartError=FMath::Max(StartError,float(Before[I].GetRotation().AngularDistance(Early[I].GetRotation())));
        Check(TEXT("Walk transition starts continuously from the displayed pose"),Before.Num()>0&&Early.Num()==Before.Num()&&StartError<.02f);
        Check(TEXT("Incoming sequence clock advances during the blend"),Motion->GetCurrentTime()>0);
        const auto End=Sample(.2f);float MotionChange=0;
        for(int32 I=0;I<Before.Num()&&I<End.Num();++I)MotionChange=FMath::Max(MotionChange,float(Before[I].GetRotation().AngularDistance(End[I].GetRotation())));
        Check(TEXT("Walk transition reaches a moving pose rather than freezing"),MotionChange>.02f);
        Motion->Transition(It->Walk,true,.18f);Check(TEXT("Repeated clip can restart for consecutive actions"),FMath::IsNearlyZero(Motion->GetCurrentTime()));
        Mesh->VisibilityBasedAnimTickOption=Visibility;It->ResetEncounter();MotionCovered=true;break;
    }
    Check(TEXT("A real humanoid rig was exercised by transition checks"),MotionCovered);
    // Exercise actual world collision and attack progression, not just damage helpers.
    const FVector OriginalLocation=Player->GetActorLocation();
    const FRotator OriginalView=Player->GetController()->GetControlRotation();
    const FRotator OriginalActorRotation=Player->GetActorRotation();
    const FVector Arena(0,0,30000);
    Player->SetActorRotation(FRotator::ZeroRotator);
    Player->SetActorLocation(Arena,false,nullptr,ETeleportType::TeleportPhysics);
    Player->GetController()->SetControlRotation(FRotator::ZeroRotator);
    Player->Camera->SetWorldRotation(FRotator::ZeroRotator);
    FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* ContactEnemy=Player->GetWorld()->SpawnActor<AFWEnemy>(AFWEnemy::StaticClass(),Arena+FVector(175,65,40),FRotator::ZeroRotator,Spawn);
    ContactEnemy->SetActorTickEnabled(false);ContactEnemy->GetCharacterMovement()->DisableMovement();
    auto PrepareCut=[&]()
    {
        ContactEnemy->Health=1000;ContactEnemy->Friendly=false;
        Player->Health=100;Player->Stamina=100;Player->AttackAge=-1;Player->QueuedAttack=0;Player->GuardHeld=false;Player->Blocking=false;Player->GuardBroken=0;Player->Combo=0;Player->LightChain=0;Player->ChainTime=0;
        Player->StartAttack(false);
    };
    Check(TEXT("Forward heavy selects thrust"),FWChooseStrike(true,1,0,false,false,0)==EFWStrike::Thrust);
    Check(TEXT("Backward heavy selects rising cut"),FWChooseStrike(true,-1,0,false,false,0)==EFWStrike::Rising);
    Check(TEXT("Strafing chooses cut direction"),FWChooseStrike(false,0,-1,false,false,0)==EFWStrike::LeftCut&&FWChooseStrike(false,0,1,false,false,0)==EFWStrike::RightCut);
    Check(TEXT("Crouching selects low cut"),FWChooseStrike(false,0,0,true,false,0)==EFWStrike::LowCut);
    Check(TEXT("Guard plus light selects bash"),FWChooseStrike(false,0,0,false,true,0)==EFWStrike::Bash);
    Check(TEXT("Two light attacks unlock heavy finisher"),FWChooseStrike(true,0,0,false,false,2)==EFWStrike::Finisher);
    PrepareCut();Player->Feint();Check(TEXT("Early feint spends extra endurance and cancels swing"),Player->AttackAge<0&&Player->Stamina==79&&Player->FeintRecovery>0);
    Player->StartAttack(false);Check(TEXT("Feint recovery prevents instant attack restart"),Player->AttackAge<0);
    Player->FeintRecovery=0;PrepareCut();Player->AttackAge=Player->ImpactAt;Player->Feint();Check(TEXT("Committed attack cannot feint"),Player->AttackAge>=0);
    Player->AttackAge=-1;Player->Stamina=100;Player->BeginStrike(EFWStrike::Bash);Player->GuardHeld=true;Player->HitResolved=true;Player->UpdateCombat(.6f);
    Check(TEXT("Shield bash returns to held guard after recovery"),Player->AttackAge<0&&Player->Blocking&&Player->Stamina==80);
    Player->BlockEnd();PrepareCut();Player->ForwardIntent=1;Player->AttackAge=.14f;Player->StartAttack(true);Player->ForwardIntent=0;Player->HitResolved=true;Player->AttackAge=Player->AttackLength*.78f;Player->UpdateCombat(.001f);
    Check(TEXT("Buffered attack preserves direction at key press"),Player->StrikeMove==EFWStrike::Thrust);
    Player->ForwardIntent=Player->StrafeIntent=0;Player->LightChain=0;Player->ChainTime=0;
    const float CutDamage=Player->HasFlag(TEXT("supplies_done"))?37.f:31.f;
    for(float Step:{1.f/120,1.f/60,1.f/30,.2f})
    {
        PrepareCut();for(float T=0;T<.8f;T+=Step)Player->UpdateCombat(Step);
        Check(FString::Printf(TEXT("Swept off-centre cut damages exactly once at %.1f ms frames"),Step*1000),ContactEnemy->Health==1000-CutDamage);
    }
    ContactEnemy->SetActorLocation(Arena+FVector(-175,0,40));PrepareCut();Player->UpdateCombat(.4f);
    Check(TEXT("Swept cut cannot hit behind the player"),ContactEnemy->Health==1000);
    ContactEnemy->SetActorLocation(Arena+FVector(175,65,40));PrepareCut();ContactEnemy->Friendly=true;Player->UpdateCombat(.4f);
    Check(TEXT("Swept cut preserves friendly health"),ContactEnemy->Health==1000);
    auto* Wall=Player->GetWorld()->SpawnActor<AActor>();auto* WallMesh=NewObject<UStaticMeshComponent>(Wall);
    Wall->SetRootComponent(WallMesh);Wall->AddInstanceComponent(WallMesh);
    WallMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    WallMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);WallMesh->SetCollisionResponseToAllChannels(ECR_Block);WallMesh->RegisterComponent();
    Wall->SetActorLocation(Arena+FVector(80,0,50));Wall->SetActorScale3D(FVector(.2f,4,3));
    PrepareCut();Player->UpdateCombat(.4f);
    Check(TEXT("Solid wall absorbs the swing before the enemy"),ContactEnemy->Health==1000&&Player->HitResolved&&Player->StrikeHold>0);
    Wall->Destroy();PrepareCut();Player->AttackAge=-1;Player->BlockStart();Player->BlockAge=1;Player->Stamina=2;Player->ReceiveEnemyHit(ContactEnemy,19);
    Player->Stamina=100;Player->BlockStart();Player->StartAttack(false);
    Check(TEXT("Broken guard cannot be instantly raised or cancelled into an attack"),Player->GuardBroken>0&&!Player->Blocking&&Player->AttackAge<0);
    Player->GuardBroken=0;Player->BlockStart();
    Check(TEXT("Guard becomes available after break recovery"),Player->Blocking);
    ContactEnemy->Destroy();Player->SetActorLocation(OriginalLocation,false,nullptr,ETeleportType::TeleportPhysics);Player->SetActorRotation(OriginalActorRotation);Player->GetController()->SetControlRotation(OriginalView);Player->Camera->SetWorldRotation(OriginalView);
    Player->Health=100;Player->Stamina=100;Player->BlockEnd();Player->AttackAge=-1;Player->GuardBroken=0;Player->StrikeHold=0;Player->QueuedAttack=0;
    Player->Expedition->Flags.AddUnique(TEXT("amulet_truth"));TSet<USkeletalMesh*> Tested;
    for(TActorIterator<AFWEnemy> It(Player->GetWorld());It;++It)
    {
        AFWEnemy* Enemy=*It;if(Enemy->Friendly||Enemy->Dragon)continue;
        auto* Mesh=Enemy->GetMesh()->GetSkeletalMeshAsset();if(!Mesh||Tested.Contains(Mesh))continue;Tested.Add(Mesh);
        const FString Label=Mesh->GetName();Check(Label+TEXT(" has all detached parts"),Enemy->SeverMeshes.Num()==5&&!Enemy->SeverMeshes.Contains(nullptr));
        for(int Part=0;Part<5;Part++)
        {
            Enemy->ResetEncounter();Enemy->GetMesh()->RefreshBoneTransforms();
            const FName Bone=FWSeverBone(Mesh,Part);FVector Point=Enemy->GetMesh()->GetBoneLocation(Bone);
            // Arm aim is below the head threshold, toward the selected shoulder.
            Check(Label+FString::Printf(TEXT(" exact bone selects part %d"),Part),FWSelectHitPart(Enemy,Point)==Part);
            Enemy->ReceiveStrike(Enemy->MaxHealth+1,Enemy->GetActorForwardVector(),true,Part==0,Point);
            Check(Label+FString::Printf(TEXT(" lethal strike removes part %d"),Part),Enemy->Dead&&Enemy->Health==0&&Enemy->SeveredBone==Bone&&Enemy->GetMesh()->IsBoneHiddenByName(Bone));
            if(Part>=3){Enemy->Tick(.1f);Check(Label+FString::Printf(TEXT(" leg %d removal promptly enters ragdoll"),Part),Enemy->GetMesh()->IsSimulatingPhysics());}
            bool PhysicsPiece=false;
            for(AActor* Piece:Enemy->DamageDebris)if(IsValid(Piece))if(auto* C=Cast<UStaticMeshComponent>(Piece->GetRootComponent()))PhysicsPiece|=C->GetStaticMesh()==Enemy->SeverMeshes[Part]&&C->IsSimulatingPhysics();
            Check(Label+FString::Printf(TEXT(" part %d has a physical detached mesh"),Part),PhysicsPiece);
            Enemy->ResetEncounter();Check(Label+FString::Printf(TEXT(" reset restores part %d and clears debris"),Part),!Enemy->Dead&&Enemy->SeveredBone.IsNone()&&!Enemy->GetMesh()->IsBoneHiddenByName(Bone)&&Enemy->DamageDebris.IsEmpty());
        }
        Enemy->ResetEncounter();Enemy->GetMesh()->RefreshBoneTransforms();
        const FVector Arm=Enemy->GetMesh()->GetBoneLocation(FWSeverBone(Mesh,2));
        Enemy->ReceiveStrike(21,FVector::ForwardVector,false,false,Arm);
        Check(Label+TEXT(" arm wound reduces outgoing damage without severing"),!Enemy->Dead&&Enemy->SeveredBones.IsEmpty()&&FWInjuryAttackScale(Enemy)<1);
        Enemy->ReceiveStrike(21,FVector::ForwardVector,true,false,Arm);
        Check(Label+TEXT(" accumulated arm trauma permits nonfatal dismemberment"),!Enemy->Dead&&Enemy->SeveredBones.Contains(FWSeverBone(Mesh,2))&&FWInjuryAttackScale(Enemy)==.25f);
        Check(Label+TEXT(" removed sword arm hides equipped weapon"),!Enemy->Weapon||!Enemy->Weapon->IsVisible());
        Check(Label+TEXT(" removed arm is excluded from subsequent anatomical hits"),FWSelectHitPart(Enemy,Arm)!=2);
        for(FName EquipmentBone:{FName(TEXT("sword_bottom")),FName(TEXT("sword_top")),FName(TEXT("joint_weapon"))})if(Enemy->GetMesh()->GetBoneIndex(EquipmentBone)!=INDEX_NONE)
            Check(Label+TEXT(" integrated weapon is hidden: ")+EquipmentBone.ToString(),Enemy->GetMesh()->IsBoneHiddenByName(EquipmentBone));
        Enemy->ResetEncounter();Enemy->GetMesh()->RefreshBoneTransforms();
        const FVector Leg=Enemy->GetMesh()->GetBoneLocation(FWSeverBone(Mesh,3));
        Enemy->ReceiveStrike(25,FVector::ForwardVector,false,false,Leg);
        Check(Label+TEXT(" leg injury slows pursuit without removing the leg"),!Enemy->Dead&&Enemy->SeveredBones.IsEmpty()&&Enemy->GetCharacterMovement()->MaxWalkSpeed<Enemy->UninjuredSpeed);
        Enemy->ReceiveStrike(36,FVector::ForwardVector,true,false,Leg);
        Check(Label+TEXT(" leg sever is fatal even with remaining general health"),Enemy->Dead&&Enemy->SeveredBones.Contains(FWSeverBone(Mesh,3)));
        Enemy->ResetEncounter();FWEmitBlood(Enemy,Enemy->GetActorLocation()+FVector(0,0,1000),FVector::ForwardVector,1000);
        Check(Label+TEXT(" blood spray uses at most 64 instances"),Enemy->BloodDrops.Num()==64&&Enemy->BloodInstances&&Enemy->BloodInstances->GetInstanceCount()==64);
        const FVector First=Enemy->BloodDrops[0].Position;FWUpdateBlood(Enemy,.016f);
        Check(Label+TEXT(" blood droplets move with gravity"),!Enemy->BloodDrops.IsEmpty()&&!Enemy->BloodDrops[0].Position.Equals(First));
        for(int I=0;I<180;I++)FWUpdateBlood(Enemy,.016f);
        Check(Label+TEXT(" blood droplets expire and ground decals remain bounded"),Enemy->BloodDrops.IsEmpty()&&Enemy->BloodPools.Num()<=12);
        Enemy->ResetEncounter();
        Check(Label+TEXT(" reset clears injuries blood and mobility penalties"),Enemy->PartDamage[3]==0&&Enemy->PartDamage[2]==0&&Enemy->BloodDrops.IsEmpty()&&!Enemy->BloodInstances&&Enemy->BloodPools.IsEmpty()&&Enemy->GetCharacterMovement()->MaxWalkSpeed==Enemy->UninjuredSpeed);
        Enemy->ReceiveStrike(1,Enemy->GetActorForwardVector(),false,false,Enemy->GetActorLocation());
        Check(Label+TEXT(" normal hit damages without severing"),Enemy->Health==Enemy->MaxHealth-1&&Enemy->SeveredBone.IsNone()&&!Enemy->Dead);Enemy->ResetEncounter();
    }
    Check(TEXT("All four humanoid enemy meshes tested"),Tested.Num()==4);
    for(TActorIterator<AFWEnemy> It(Player->GetWorld());It;++It)
    {
        AFWEnemy* E=*It;
        if(!E->CanRespawn())
        {
            if(E->Friendly||E->Dragon||E->Identity==TEXT("king")||E->Identity==TEXT("jailer"))
                Check(E->Identity+TEXT(" remains excluded from ordinary respawns"),!E->CanRespawn());
            continue;
        }
        UFWExpedition* S=Player->Expedition.Get();
        const int Day=S->Day;const float Hour=S->Hour;
        E->ResetEncounter();E->ReceiveStrike(E->MaxHealth+1,FVector::ForwardVector,true,true,E->GetMesh()->GetBoneLocation(FWSeverBone(E->GetMesh()->GetSkeletalMeshAsset(),0)));
        const FVector Far=E->Home+FVector(12000,0,0);
        Check(TEXT("Death schedules ordinary reinforcement for the next day"),S->RespawnAtHour.Contains(E->Identity)&&FMath::IsNearlyEqual(S->RespawnAtHour.FindRef(E->Identity),S->CampaignHours()+24));
        Check(TEXT("Reinforcements cannot return early"),!E->RespawnDue(S,Far));
        S->Day++;
        Check(TEXT("Reinforcement becomes due after one game day"),E->RespawnDue(S,Far));
        Check(TEXT("Player near home prevents respawn"),!E->RespawnDue(S,E->Home+FVector(100,0,0)));
        E->SetActorLocation(Far,false,nullptr,ETeleportType::TeleportPhysics);
        Check(TEXT("Player beside a displaced corpse prevents respawn"),!E->RespawnDue(S,Far));
        E->SetActorLocation(E->Home,false,nullptr,ETeleportType::TeleportPhysics);
        TArray<uint8> Bytes;UGameplayStatics::SaveGameToMemory(S,Bytes);
        auto* RoundTrip=Cast<UFWExpedition>(UGameplayStatics::LoadGameFromMemory(Bytes));
        Check(TEXT("Respawn deadline and quest history survive save serialization"),RoundTrip&&RoundTrip->RespawnAtHour.FindRef(E->Identity)==S->RespawnAtHour.FindRef(E->Identity)&&RoundTrip->Defeated.Contains(E->Identity));
        const FVector Location=Player->GetActorLocation();const FRotator View=Player->GetControlRotation();const bool Started=Player->Started;
        Player->Started=true;Player->SetActorLocation(Far,false,nullptr,ETeleportType::TeleportPhysics);
        Player->GetController()->SetControlRotation(FRotator(0,0,0));
        Check(TEXT("Due reinforcement actually respawns while player faces away"),E->TryRespawn(Player));
        Check(TEXT("Respawn restores health collision limbs and home position"),!E->Dead&&E->Health==E->MaxHealth&&E->DamageDebris.IsEmpty()&&E->SeveredBone.IsNone()&&E->GetCapsuleComponent()->GetCollisionEnabled()==ECollisionEnabled::QueryAndPhysics&&E->GetActorLocation().Equals(E->Home,1));
        Check(TEXT("Respawn clears deadline but preserves defeat history"),!S->RespawnAtHour.Contains(E->Identity)&&S->Defeated.Contains(E->Identity));
        S->RespawnVersion=1;Player->Restoring=true;E->RestoreEncounter(S);
        Check(TEXT("Reload does not kill a previously respawned enemy"),!E->Dead);
        S->RespawnVersion=0;E->RestoreEncounter(S);
        Check(TEXT("Old saves migrate defeated guards to a fresh one-day delay"),E->Dead&&FMath::IsNearlyEqual(S->RespawnAtHour.FindRef(E->Identity),S->CampaignHours()+24));
        const float Deadline=S->RespawnAtHour.FindRef(E->Identity);S->RespawnVersion=1;S->Hour+=1;E->RestoreEncounter(S);
        Check(TEXT("Reload does not extend an existing respawn deadline"),S->RespawnAtHour.FindRef(E->Identity)==Deadline);
        Player->Restoring=false;Player->Started=Started;Player->SetActorLocation(Location,false,nullptr,ETeleportType::TeleportPhysics);Player->GetController()->SetControlRotation(View);
        S->Day=Day;S->Hour=Hour;S->RespawnAtHour.Remove(E->Identity);E->ResetEncounter();
        break;
    }
    TWeakObjectPtr<AFWEnemy> Probe;TWeakObjectPtr<UDecalComponent> Mark;
    for(TActorIterator<AFWEnemy> It(Player->GetWorld());It;++It)if(!It->Friendly&&!It->Dragon)
    {
        Probe=*It;It->ReceiveStrike(1,FVector::ForwardVector,false,false,It->GetActorLocation());
        TArray<USceneComponent*> Children;It->GetMesh()->GetChildrenComponents(false,Children);
        for(auto* Child:Children)if(auto* Decal=Cast<UDecalComponent>(Child))Mark=Decal;
        break;
    }
    Check(TEXT("Nonfatal hit creates a temporary wound mark"),Mark.IsValid());
    FTimerHandle Timer;Player->GetWorld()->GetTimerManager().SetTimer(Timer,[Checks,Passed,Probe,Mark]() mutable
    {
        auto LateCheck=[&](const FString& Name,bool OK){auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("name"),Name);R->SetBoolField(TEXT("passed"),OK);Checks.Add(MakeShared<FJsonValueObject>(R));Passed&=OK;};
        LateCheck(TEXT("Faded wound mark preserves the living enemy"),Probe.IsValid()&&!Probe->Dead&&Probe->Health==Probe->MaxHealth-1);
        LateCheck(TEXT("Expired wound decal is released"),!Mark.IsValid());
        auto Report=MakeShared<FJsonObject>();Report->SetBoolField(TEXT("all_passed"),Passed);Report->SetArrayField(TEXT("checks"),Checks);
        FString Text;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectDir()/TEXT("Verification/combat-contact-tests.json")));
        FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
    },18,false);
}

void FWCaptureCombatDamage(AFWCharacter* Player)
{
    static bool Prepared=false,Set=false,Audited=false;static TWeakObjectPtr<AFWEnemy> Target;static double RootZ=0;
    FString Identity;if(!FParse::Value(FCommandLine::Get(),TEXT("CaptureSever="),Identity))return;
    const double Now=Player->GetWorld()->GetRealTimeSeconds();if(Now<8)return;
    const bool Live=FParse::Param(FCommandLine::Get(),TEXT("CaptureSeverLive"));
    const bool Reaction=FParse::Param(FCommandLine::Get(),TEXT("CaptureReaction"));
    float CaptureSeconds=65,After=4;FParse::Value(FCommandLine::Get(),TEXT("CaptureSeconds="),CaptureSeconds);FParse::Value(FCommandLine::Get(),TEXT("SeverAfter="),After);
    if(Set)
    {
        if(!Audited&&Now>=CaptureSeconds)
        {
            Audited=true;auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("enemy"),Identity);R->SetBoolField(TEXT("live_death"),Live);R->SetBoolField(TEXT("body_exists"),Target.IsValid());
            if(Target.IsValid()){R->SetNumberField(TEXT("root_delta_z"),Target->GetActorLocation().Z-RootZ);R->SetBoolField(TEXT("dead"),Target->Dead);R->SetNumberField(TEXT("death_age"),Target->DeathAge);R->SetBoolField(TEXT("ragdoll"),Target->GetMesh()->IsSimulatingPhysics());R->SetStringField(TEXT("bone"),Target->SeveredBone.ToString());R->SetStringField(TEXT("mesh_position"),Target->GetMesh()->GetComponentLocation().ToString());}
            FString Text;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectDir()/TEXT("Verification/sever-capture.json")));
        }
        return;
    }
    int Part=0;FParse::Value(FCommandLine::Get(),TEXT("SeverPart="),Part);Part=FMath::Clamp(Part,0,4);
    for(TActorIterator<AFWEnemy> It(Player->GetWorld());It;++It)if(It->Identity==Identity)
    {
        AFWEnemy* Enemy=*It;
        if(!Prepared)
        {
            Prepared=true;Target=Enemy;Enemy->ResetEncounter();RootZ=Enemy->GetActorLocation().Z;
            for(TActorIterator<AFWEnemy> Other(Player->GetWorld());Other;++Other){Other->GetCharacterMovement()->ClearAccumulatedForces();Other->GetCharacterMovement()->StopMovementImmediately();Other->GetCharacterMovement()->DisableMovement();Other->SetActorTickEnabled(false);}
            Enemy->GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
            const FVector Aim=Enemy->GetActorLocation()+FVector(0,0,40);
            const FVector View=Aim+Enemy->GetActorForwardVector()*320+Enemy->GetActorRightVector()*70+FVector(0,0,35);
            Player->SetActorLocation(View-FVector(0,0,Player->Camera->GetRelativeLocation().Z));Player->GetCharacterMovement()->DisableMovement();
            if(auto* PC=Cast<APlayerController>(Player->GetController()))PC->SetControlRotation((Aim-Player->Camera->GetComponentLocation()).Rotation());
            Player->Sword->SetVisibility(false);Player->Shield->SetVisibility(false);Player->GetMesh()->SetVisibility(false);
            return;
        }
        if(Now<(Live?CaptureSeconds-FMath::Clamp(After,.15f,10.f):10.f))return;
        Set=true;Enemy->GetMesh()->RefreshBoneTransforms();Player->Expedition->Flags.AddUnique(TEXT("amulet_truth"));
        auto* Mesh=Enemy->GetMesh()->GetSkeletalMeshAsset();const FName Bone=FWSeverBone(Mesh,Part);
        FVector Aim=Enemy->GetMesh()->GetBoneLocation(Bone);
        if(Live)Enemy->SetActorTickEnabled(true);
        const FVector Direction=-Enemy->GetActorForwardVector();Enemy->ReceiveStrike(Reaction?1:FParse::Param(FCommandLine::Get(),TEXT("SeverNonfatal"))?48:Enemy->MaxHealth+1,Direction,true,!Reaction&&Part==0,Aim);
        if(Live)return;
        if(Reaction){Enemy->GetMesh()->SetPosition(Enemy->HitReaction?Enemy->HitReaction->GetPlayLength()*.4f:.15f,false);Enemy->GetMesh()->bPauseAnims=true;Player->Notify(TEXT("Recorded hit-reaction review"),120);return;}
        // Freeze a close diagnostic pose; normal gameplay keeps death animation and physics.
        Enemy->GetMesh()->PlayAnimation(Enemy->Idle,true);Enemy->SetActorTickEnabled(false);
        int PieceIndex=0;
        for(AActor* Piece:Enemy->DamageDebris)if(IsValid(Piece))
        {
            Piece->SetLifeSpan(0);
            if(auto* C=Cast<UStaticMeshComponent>(Piece->GetRootComponent()))if(C->IsSimulatingPhysics())
            {C->SetSimulatePhysics(false);Piece->SetActorLocation(Aim+Enemy->GetActorRightVector()*100+FVector(0,0,15));PieceIndex++;}
        }
        Player->Notify(FString::Printf(TEXT("Damage review: %s / %s"),*Identity,*Bone.ToString()),120);
        auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("enemy"),Identity);R->SetStringField(TEXT("bone"),Enemy->SeveredBone.ToString());R->SetNumberField(TEXT("pieces"),PieceIndex);
        FString Text;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectDir()/TEXT("Verification/sever-capture.json")));break;
    }
}
