#include "FWConsequences.h"
#include "FWGame.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Algo/Reverse.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"

namespace FWConsequences
{
    struct FCargo
    {
        TWeakObjectPtr<AFWInteractable> Target;
        TWeakObjectPtr<AStaticMeshActor> Prop;
        FTransform TargetHome,PropHome;
        int Phase=-1;
    };
    struct FState
    {
        TWeakObjectPtr<UWorld> World;
        TWeakObjectPtr<UFWExpedition> Save;
        TWeakObjectPtr<AFWEnemy> Edrin;
        TWeakObjectPtr<AStaticMeshActor> PrisonGate;
        FTransform ClosedGate;
        int GateState=-1;
        FTransform Prison;
        TArray<FCargo> Cargo;
        TArray<FVector> InnRoute;
        bool Returned=false,ReviewSet=false;
        FVector ReturnedHome=FVector::ZeroVector;
        float MaxHomeDistance=0;
    };
    static FState State;
    static bool Ground(AFWCharacter* P,const FVector& At,FVector& Out)
    {
        FHitResult Hit;FCollisionQueryParams Q;Q.AddIgnoredActor(P);
        FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_WorldStatic);
        if(!P->GetWorld()->LineTraceSingleByObjectType(Hit,At+FVector(0,0,5000),At-FVector(0,0,5000),Objects,Q)||Hit.ImpactNormal.Z<.65f)return false;
        Out=Hit.ImpactPoint;return true;
    }
    static TArray<FVector> VillageRoute(AFWCharacter* P,const FVector& Start,const FVector& Goal)
    {
        // Bounded, cached planning for this flat settlement only. Query the
        // actual dressed world so fences and library props cannot invalidate a
        // legacy route. The 55 cm clearance leaves room for the 34 cm capsule.
        constexpr int NX=61,NY=49,N=NX*NY;constexpr float Step=100;
        const FVector Origin(-45000,-4200,Start.Z);
        auto Point=[&](int I){return Origin+FVector((I%NX)*Step,(I/NX)*Step,0);};
        auto Index=[&](const FVector& V){return FMath::Clamp(FMath::RoundToInt((V.X-Origin.X)/Step),0,NX-1)+NX*FMath::Clamp(FMath::RoundToInt((V.Y-Origin.Y)/Step),0,NY-1);};
        FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_WorldStatic);FCollisionQueryParams Q;Q.AddIgnoredActor(P);
        auto Clear=[&](const FVector& A,const FVector& B){FHitResult Hit;return !P->GetWorld()->SweepSingleByObjectType(Hit,A,B,FQuat::Identity,Objects,FCollisionShape::MakeSphere(55),Q);};
        const int First=Index(Start),Last=Index(Goal);
        if(!Clear(Start,Point(First))||!Clear(Point(Last),Goal))return {};
        TArray<float> Cost;Cost.Init(MAX_flt,N);TArray<int> Parent;Parent.Init(-1,N);TArray<uint8> Closed;Closed.Init(0,N);TArray<int> Open={First};Cost[First]=0;
        while(Open.Num())
        {
            int Best=0;float Score=MAX_flt;
            for(int I=0;I<Open.Num();I++){const int Node=Open[I];const float Value=Cost[Node]+FVector::Dist2D(Point(Node),Point(Last));if(Value<Score){Score=Value;Best=I;}}
            const int Here=Open[Best];Open.RemoveAtSwap(Best);if(Closed[Here])continue;Closed[Here]=1;
            if(Here==Last)break;
            const int X=Here%NX,Y=Here/NX;
            for(int DY=-1;DY<=1;DY++)for(int DX=-1;DX<=1;DX++)
            {
                if((DX==0&&DY==0)||X+DX<0||X+DX>=NX||Y+DY<0||Y+DY>=NY)continue;
                const int Next=X+DX+NX*(Y+DY);if(Closed[Next])continue;
                const float NewCost=Cost[Here]+(DX&&DY?141.42136f:100.f);
                if(NewCost>=Cost[Next]||!Clear(Point(Here),Point(Next)))continue;
                const bool New=Cost[Next]==MAX_flt;Cost[Next]=NewCost;Parent[Next]=Here;if(New)Open.Add(Next);
            }
        }
        if(!Closed[Last])return {};
        TArray<FVector> Raw;for(int I=Last;I!=-1;I=Parent[I])Raw.Add(Point(I));Algo::Reverse(Raw);Raw.Insert(Start,0);Raw.Add(Goal);
        TArray<FVector> Result={Start};int I=0;
        while(I<Raw.Num()-1){int Next=Raw.Num()-1;while(Next>I+1&&!Clear(Raw[I],Raw[Next]))Next--;Result.Add(Raw[Next]);I=Next;}
        return Result;
    }
    static void Gather(AFWCharacter* P)
    {
        State=FState();State.World=P->GetWorld();
        for(TActorIterator<AFWEnemy> E(P->GetWorld());E;++E)
        {
            if(E->Identity==TEXT("prisoner")){State.Edrin=*E;State.Prison=E->HomeTransform;}
        }
        for(TActorIterator<AFWInteractable> I(P->GetWorld());I;++I)if(I->Identity==TEXT("cargo_lake")||I->Identity==TEXT("cargo_hollow"))
        {
            FCargo C;C.Target=*I;C.TargetHome=I->GetActorTransform();float Best=400*400;
            for(TActorIterator<AStaticMeshActor> A(P->GetWorld());A;++A)
            {
                UStaticMesh* M=A->GetStaticMeshComponent()->GetStaticMesh();
                if(!M||M->GetName()!=TEXT("SM_SupplyCrateComplete"))continue;
                const float Distance=FVector::DistSquared2D(A->GetActorLocation(),I->GetActorLocation());
                if(Distance<Best){Best=Distance;C.Prop=*A;C.PropHome=A->GetActorTransform();}
            }
            State.Cargo.Add(C);
        }
    }
    void Update(AFWCharacter* P,bool Restore)
    {
        if(!P||!P->Expedition)return;
        if(State.World.Get()!=P->GetWorld())Gather(P);
        Restore|=State.Save.Get()!=P->Expedition;State.Save=P->Expedition;
        if(!State.PrisonGate.IsValid())
            for(TActorIterator<AStaticMeshActor> A(P->GetWorld());A;++A)if(A->ActorHasTag(TEXT("CastlePrisonGate")))
            {State.PrisonGate=*A;State.ClosedGate=A->GetActorTransform();break;}
        int Stage=-1;
        if(!State.ReviewSet&&FParse::Param(FCommandLine::Get(),TEXT("FrostwatchCapture"))&&FParse::Value(FCommandLine::Get(),TEXT("ConsequencesStage="),Stage))
        {
            State.ReviewSet=true;
            // The visual capture hour normally controls lighting only. Routine
            // verification must start the saved NPC clock at that hour as well.
            float Hour=10;FParse::Value(FCommandLine::Get(),TEXT("CaptureHour="),Hour);P->Expedition->Hour=FMath::Clamp(Hour,0.f,23.99f);
            for(const FString F:{TEXT("cargo_lake"),TEXT("cargo_hollow"),TEXT("supplies_done"),TEXT("prisoner")})P->Expedition->Flags.Remove(F);
            if(Stage>=1)for(const FString F:{TEXT("cargo_lake"),TEXT("cargo_hollow"),TEXT("supplies_done")})P->Expedition->Flags.AddUnique(F);
            if(Stage>=2)P->Expedition->Flags.AddUnique(TEXT("prisoner"));
        }
        if(auto* Gate=State.PrisonGate.Get())
        {
            const int Open=P->Expedition->Defeated.Contains(TEXT("jailer"))||P->HasFlag(TEXT("prisoner"));
            if(Open!=State.GateState||Restore)
            {
                auto Transform=State.ClosedGate;
                if(Open)Transform.SetRotation(FQuat(FVector::UpVector,-PI/2)*Transform.GetRotation());
                Gate->SetActorTransform(Transform);State.GateState=Open;
            }
        }
        for(auto& C:State.Cargo)if(C.Target.IsValid()&&C.Prop.IsValid())
        {
            const int Phase=P->HasFlag(TEXT("supplies_done"))?2:P->HasFlag(C.Target->Identity)?1:0;
            if(Phase==C.Phase&&!Restore)continue;
            auto Place=[&](const FTransform& Transform)
            {
                if(C.Prop->GetActorTransform().Equals(Transform))return;
                auto* Mesh=C.Prop->GetStaticMeshComponent();const auto Mobility=Mesh->Mobility;
                Mesh->SetMobility(EComponentMobility::Movable);C.Prop->SetActorTransform(Transform);Mesh->SetMobility(Mobility);
            };
            if(Phase==2)
            {
                FVector Floor;const FVector At(-40000-(C.Target->Identity==TEXT("cargo_hollow")?140:0),650,450);
                if(!Ground(P,At,Floor))continue;
                const auto B=C.Prop->GetStaticMeshComponent()->CalcBounds(C.PropHome);
                const FVector Offset=C.PropHome.GetLocation()-FVector(B.Origin.X,B.Origin.Y,B.Origin.Z-B.BoxExtent.Z);
                FTransform Delivered=C.PropHome;Delivered.SetLocation(Floor+Offset);Place(Delivered);
                C.Target->SetActorLocation(Floor+FVector(0,0,70));
            }
            else {Place(C.PropHome);C.Target->SetActorTransform(C.TargetHome);}
            C.Prop->SetActorHiddenInGame(Phase==1);C.Target->SetActorHiddenInGame(Phase==1);
            C.Phase=Phase;
        }
        auto* E=State.Edrin.Get();if(!E)return;
        // A load resets the character independently. If the arrival point is
        // temporarily occupied, leave relocation pending so later ticks retry.
        if(Restore)State.Returned=false;
        if(State.Returned)State.MaxHomeDistance=FMath::Max(State.MaxHomeDistance,FVector::Dist2D(E->GetActorLocation(),State.ReturnedHome));
        const bool Rescued=P->HasFlag(TEXT("prisoner"));
        if(!Rescued&&(State.Returned||Restore))
        {
            E->Job.Empty();E->MorningRoute.Empty();E->WorkRoute.Empty();E->InnRoute.Empty();E->SleepRoute.Empty();E->ActiveRoute.Empty();E->RoutinePhase.Empty();E->WorkPause=0;E->RouteIndex=0;
            E->SetActorTransform(State.Prison);E->SetActorHiddenInGame(false);E->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            E->GetCharacterMovement()->StopMovementImmediately();State.Returned=false;
        }
        if(!Rescued||(State.Returned&&!Restore))return;
        // Leave the rescue conversation intact. Relocate only after leaving the
        // prison area; restoring an existing save reconstructs the home state.
        if(!Restore&&(P->Menu||FVector::DistSquared2D(P->GetActorLocation(),State.Prison.GetLocation())<FMath::Square(2200.f)))return;
        FVector Home;if(!Ground(P,FVector(-39700,-900,450),Home))return;
        Home.Z+=E->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2;
        FCollisionQueryParams Q;Q.AddIgnoredActor(E);
        if(P->GetWorld()->OverlapBlockingTestByChannel(Home,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(E->GetCapsuleComponent()->GetScaledCapsuleRadius(),E->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),Q))return;
        E->Job=TEXT("cartographer");E->WorkRoute={Home,Home+FVector(-220,0,0),Home+FVector(-220,220,0)};
        // The inn and settlement share an authored floor height. A downward
        // trace inside the inn would select its roof instead of that floor.
        if(State.InnRoute.IsEmpty())State.InnRoute=VillageRoute(P,Home,FVector(-43650,-2550,Home.Z));
        E->InnRoute=State.InnRoute;
        E->SleepRoute=E->InnRoute;E->MorningRoute=E->InnRoute;Algo::Reverse(E->MorningRoute);
        E->RoutinePhase.Empty();E->ActiveRoute.Empty();E->RouteIndex=0;E->WorkPause=0;
        E->SetActorLocation(Home,false,nullptr,ETeleportType::TeleportPhysics);E->SetActorHiddenInGame(false);E->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);E->GetCharacterMovement()->StopMovementImmediately();
        State.Returned=true;State.ReturnedHome=Home;State.MaxHomeDistance=0;
        if(!Restore)P->Notify(TEXT("Edrin has returned to Hearthmere. Find him near Captain Rowan by day, or at the inn in the evening."),12);
    }
    void Describe(TSharedPtr<FJsonObject> Report)
    {
        if(!State.ReviewSet)return;
        auto R=MakeShared<FJsonObject>();
        R->SetBoolField(TEXT("mentor_returned"),State.Returned);
        R->SetNumberField(TEXT("mentor_max_home_distance_cm"),State.MaxHomeDistance);
        if(auto* E=State.Edrin.Get()){R->SetStringField(TEXT("mentor_position"),E->GetActorLocation().ToString());R->SetStringField(TEXT("mentor_activity"),E->Activity);R->SetStringField(TEXT("mentor_phase"),E->RoutinePhase);}
        TArray<TSharedPtr<FJsonValue>> Cargo;
        for(const auto& C:State.Cargo)if(C.Prop.IsValid()&&C.Target.IsValid())
        {
            auto Entry=MakeShared<FJsonObject>();Entry->SetStringField(TEXT("identity"),C.Target->Identity);Entry->SetNumberField(TEXT("phase"),C.Phase);Entry->SetBoolField(TEXT("visible"),!C.Prop->IsHidden());
            const auto B=C.Prop->GetStaticMeshComponent()->Bounds;Entry->SetStringField(TEXT("base_position"),FVector(B.Origin.X,B.Origin.Y,B.Origin.Z-B.BoxExtent.Z).ToString());Cargo.Add(MakeShared<FJsonValueObject>(Entry));
        }
        R->SetArrayField(TEXT("cargo"),Cargo);Report->SetObjectField(TEXT("consequences"),R);
    }
    void Test(AFWCharacter* P,TFunctionRef<void(const FString&,bool)> Check)
    {
        const auto Flags=P->Expedition->Flags;const FVector Location=P->GetActorLocation();const bool Menu=P->Menu;
        const auto Defeated=P->Expedition->Defeated;
        P->Expedition->Defeated.Remove(TEXT("jailer"));P->Expedition->Flags.Remove(TEXT("prisoner"));Update(P,true);
        Check(TEXT("Castle prison has an authored gate"),State.PrisonGate.IsValid());
        if(auto* Gate=State.PrisonGate.Get())
        {
            const FVector Start(1850,2900,90),End(2200,2900,90);
            FCollisionQueryParams Q;Q.AddIgnoredActor(P);
            auto Blocked=[&](){FHitResult Hit;return P->GetWorld()->SweepSingleByChannel(Hit,Start,End,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(34,88),Q);};
            Check(TEXT("Prison gate blocks entry before jailer defeat"),State.GateState==0&&Blocked());
            P->Expedition->Defeated.AddUnique(TEXT("jailer"));Update(P);
            Check(TEXT("Jailer defeat opens a capsule-clear prison entrance"),State.GateState==1&&!Blocked());
            P->Expedition->Defeated.Remove(TEXT("jailer"));P->Expedition->Flags.AddUnique(TEXT("prisoner"));Update(P,true);
            Check(TEXT("Legacy rescued save keeps prison gate open"),State.GateState==1&&!Blocked());
            P->Expedition->Flags.Remove(TEXT("prisoner"));Update(P,true);
            Check(TEXT("Fresh expedition closes the original prison gate"),State.GateState==0&&Gate->GetActorTransform().Equals(State.ClosedGate)&&Blocked());
        }
        P->Expedition->Defeated=Defeated;
        for(const FString F:{TEXT("cargo_lake"),TEXT("cargo_hollow"),TEXT("supplies_done"),TEXT("prisoner")})P->Expedition->Flags.Remove(F);
        Update(P,true);Check(TEXT("Both supply props are associated with quest targets"),State.Cargo.Num()==2&&State.Cargo[0].Prop.IsValid()&&State.Cargo[1].Prop.IsValid());
        for(auto& C:State.Cargo)if(C.Prop.IsValid())
        {
            Check(TEXT("Uncollected supply remains at its original site"),!C.Prop->IsHidden()&&C.Prop->GetActorTransform().Equals(C.PropHome));
            P->Expedition->Flags.AddUnique(C.Target->Identity);Update(P);
            Check(TEXT("Collected supply and prompt disappear together"),C.Prop->IsHidden()&&C.Target->IsHidden());
        }
        P->Expedition->Flags.AddUnique(TEXT("supplies_done"));Update(P);
        for(auto& C:State.Cargo)if(C.Prop.IsValid())
        {
            Check(TEXT("Delivered crate returns visibly beside Rowan"),!C.Prop->IsHidden()&&!C.Target->IsHidden()&&FVector::Dist2D(C.Target->GetActorLocation(),FVector(-40000,650,0))<200);
            Check(TEXT("Delivered supply preserves static rendering"),C.Prop->GetStaticMeshComponent()->Mobility==EComponentMobility::Static);
            const auto Before=C.Prop->GetActorTransform();Update(P);Check(TEXT("Repeated world updates do not duplicate or move delivery"),C.Prop->GetActorTransform().Equals(Before));
        }
        Check(TEXT("Rescued mentor actor exists"),State.Edrin.IsValid());
        if(auto* E=State.Edrin.Get())
        {
            P->Expedition->Flags.AddUnique(TEXT("prisoner"));P->SetActorLocation(State.Prison.GetLocation()+FVector(400,0,0));P->Menu=false;Update(P);
            Check(TEXT("Edrin stays during the rescue scene"),!State.Returned&&FVector::Dist(E->GetActorLocation(),State.Prison.GetLocation())<5);
            P->SetActorLocation(FVector(-39000,0,800));Update(P);
            Check(TEXT("Rescued Edrin returns to Hearthmere"),State.Returned&&E->Job==TEXT("cartographer")&&FVector::Dist2D(E->GetActorLocation(),FVector(-39700,-900,0))<10);
            Check(TEXT("Returned Edrin has work and inn routes"),E->WorkRoute.Num()==3&&E->InnRoute.Num()>1&&E->MorningRoute.Num()==E->InnRoute.Num());
            bool RouteClear=true;FString Obstacle;FCollisionQueryParams RouteQuery;RouteQuery.AddIgnoredActor(E);RouteQuery.AddIgnoredActor(P);FCollisionObjectQueryParams StaticObjects;StaticObjects.AddObjectTypesToQuery(ECC_WorldStatic);
            for(int I=1;I<E->InnRoute.Num();I++){FHitResult Hit;if(P->GetWorld()->SweepSingleByObjectType(Hit,E->InnRoute[I-1],E->InnRoute[I],FQuat::Identity,StaticObjects,FCollisionShape::MakeSphere(34),RouteQuery)){RouteClear=false;Obstacle=TEXT(": ")+Hit.ImpactPoint.ToString();break;}}
            Check(TEXT("Returned mentor inn route clears static obstacles")+Obstacle,RouteClear);
            const FVector Home=E->GetActorLocation();E->ResetEncounter();P->SetActorLocation(Home);Update(P,true);
            Check(TEXT("Occupied mentor arrival waits without overlapping player"),!State.Returned&&FVector::Dist(E->GetActorLocation(),State.Prison.GetLocation())<5);
            P->SetActorLocation(FVector(-39000,0,800));Update(P);
            Check(TEXT("Blocked checkpoint relocation retries after arrival clears"),State.Returned&&FVector::Dist(E->GetActorLocation(),Home)<5);
            E->ResetEncounter();Update(P,true);Check(TEXT("Checkpoint restoration reconstructs rescued mentor location"),FVector::Dist2D(E->GetActorLocation(),FVector(-39700,-900,0))<10);
        }
        P->Expedition->Flags.Remove(TEXT("prisoner"));P->Expedition->Flags.Remove(TEXT("supplies_done"));P->Expedition->Flags.Remove(TEXT("cargo_lake"));P->Expedition->Flags.Remove(TEXT("cargo_hollow"));Update(P,true);
        Check(TEXT("Fresh expedition restores prisoner and original supplies"),!State.Returned&&State.Cargo.Num()==2&&State.Cargo[0].Phase==0&&State.Cargo[1].Phase==0);
        P->Expedition->Flags=Flags;P->SetActorLocation(Location);P->Menu=Menu;Update(P,true);
    }
}
