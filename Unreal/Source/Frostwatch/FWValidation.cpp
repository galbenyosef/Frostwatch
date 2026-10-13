#include "FWCampaign.h"
#include "FWGame.h"
#include "FWHorse.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Kismet/GameplayStatics.h"

void FWCampaign::WorldChecks(AFWCharacter* P,TFunctionRef<void(const FString&,bool)> Check)
{
    TSet<FString> ActorIds;FCollisionQueryParams Query;Query.AddIgnoredActor(P);
    int32 Horses=0,Citizens=0;
    for(TActorIterator<AFWEnemy> E(P->GetWorld());E;++E){ActorIds.Add(E->Identity);Query.AddIgnoredActor(*E);if(!E->Job.IsEmpty())Citizens++;}
    for(TActorIterator<AFWInteractable> I(P->GetWorld());I;++I){ActorIds.Add(I->Identity);Query.AddIgnoredActor(*I);}
    for(TActorIterator<AFWHorse> H(P->GetWorld());H;++H){Horses++;Query.AddIgnoredActor(*H);}
    bool AllActors=true;for(auto V:Data()->GetArrayField(TEXT("items")))AllActors&=ActorIds.Contains(V->AsObject()->GetStringField(TEXT("id")));
    for(auto V:Data()->GetArrayField(TEXT("enemies")))AllActors&=ActorIds.Contains(V->AsObject()->GetStringField(TEXT("id")));
    Check(TEXT("Every campaign NPC, encounter and interaction exists in the map"),AllActors);
    Check(TEXT("Six horses/carts and eighteen working villagers placed"),Horses==6&&Citizens==18);
    FString Text;TSharedPtr<FJsonObject> World;FFileHelper::LoadFileToString(Text,*(FPaths::ProjectContentDir()/TEXT("Migration/world.json")));FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),World);
    const auto& Heights=World->GetArrayField(TEXT("heights"));
    auto Ground=[&](double X,double Z){double GX=FMath::Clamp((X+600)/5,0.,239.999),GZ=FMath::Clamp((Z+600)/5,0.,239.999);int IX=int(GX),IZ=int(GZ);double TX=GX-IX,TZ=GZ-IZ;const double A=Heights[IZ*241+IX]->AsNumber(),B=Heights[IZ*241+IX+1]->AsNumber(),C=Heights[(IZ+1)*241+IX]->AsNumber(),D=Heights[(IZ+1)*241+IX+1]->AsNumber();return (TX+TZ<=1?A+(B-A)*TX+(C-A)*TZ:D+(C-D)*(1-TX)+(B-D)*(1-TZ))*100;};
    TArray<TSharedPtr<FJsonValue>> Problems;
    auto Problem=[&](const FString& Kind,const FString& Id,const FVector& At,const FString& Obstacle){auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("kind"),Kind);R->SetStringField(TEXT("id"),Id);R->SetStringField(TEXT("position"),At.ToString());R->SetStringField(TEXT("obstacle"),Obstacle);Problems.Add(MakeShared<FJsonValueObject>(R));};
    int Reachable=0;
    for(auto V:Data()->GetArrayField(TEXT("items")))
    {
        auto R=V->AsObject();FVector Object=Position(R),SightTarget=Object+FVector(0,0,80);bool Reach=false;
        for(TActorIterator<AFWInteractable> It(P->GetWorld());It;++It)if(It->Identity==R->GetStringField(TEXT("id"))){Object=It->GetActorLocation();SightTarget=Object;break;}
        for(int I=0;I<16&&!Reach;I++)
        {
            // Probe the actor's actual storey, including rooms beneath terrain.
            FVector Offset(FMath::Cos(I*PI/8)*190,FMath::Sin(I*PI/8)*190,0);FVector Candidate=Object+Offset;Candidate.Z=Object.Z+50;
            FHitResult Hit;const bool Floor=P->GetWorld()->LineTraceSingleByChannel(Hit,Candidate+FVector(0,0,70),Candidate-FVector(0,0,600),ECC_Visibility,Query);
            if(!Floor)continue;Candidate.Z=Hit.ImpactPoint.Z+100;
            if(P->GetWorld()->OverlapBlockingTestByChannel(Candidate,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(32,86),Query))continue;
            FHitResult Sight;bool Wall=P->GetWorld()->LineTraceSingleByChannel(Sight,Candidate+FVector(0,0,60),SightTarget,ECC_Visibility,Query);
            if(!Wall)Reach=true;
        }
        Reachable+=Reach;if(!Reach)Problem(TEXT("interaction"),R->GetStringField(TEXT("id")),Object,TEXT("No clear approach sample"));
    }
    Check(TEXT("All interaction records have a clear local approach"),Reachable==Data()->GetArrayField(TEXT("items")).Num());
    TMap<FIntPoint,FVector> Cells;
    double CaveFloor=0;for(const auto& V:World->GetArrayField(TEXT("places"))){auto R=V->AsObject();if(R->GetStringField(TEXT("id"))==TEXT("labyrinth"))CaveFloor=R->GetNumberField(TEXT("y"))*100;}
    for(auto V:Data()->GetArrayField(TEXT("labyrinth"))){auto R=V->AsObject();auto C=R->GetArrayField(TEXT("cell"));double X=R->GetNumberField(TEXT("x")),Z=R->GetNumberField(TEXT("z"));Cells.Add(FIntPoint(C[0]->AsNumber(),C[1]->AsNumber()),FVector(-Z*100,X*100,CaveFloor+115));}
    // Relic pedestals intentionally occupy some cell centres; test walking lanes around them.
    int Blocked=0,Links=0;
    for(auto V:Data()->GetArrayField(TEXT("labyrinth")))
    {
        auto R=V->AsObject();auto C=R->GetArrayField(TEXT("cell"));FIntPoint From(C[0]->AsNumber(),C[1]->AsNumber());
        for(auto L:R->GetArrayField(TEXT("links"))){auto A=L->AsArray();FIntPoint To(A[0]->AsNumber(),A[1]->AsNumber());if(!Cells.Contains(To)){Blocked++;continue;}FHitResult Hit;Links++;bool Clear=false; for(FVector Offset:{FVector::ZeroVector,FVector(230,0,0),FVector(-230,0,0),FVector(0,230,0),FVector(0,-230,0),FVector(230,230,0),FVector(-230,-230,0)}){if(!P->GetWorld()->SweepSingleByChannel(Hit,Cells[From]+Offset,Cells[To]+Offset,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeCapsule(30,70),Query)){Clear=true;break;}} if(!Clear){Blocked++;Problem(TEXT("cave passage"),From.ToString()+TEXT(" to ")+To.ToString(),Hit.ImpactPoint,Hit.GetActor()?Hit.GetActor()->GetName():TEXT("unknown"));}}
    }
    Check(TEXT("All 99 cave cells have collision-clear linked passages"),Cells.Num()==99&&Links>=196&&Blocked==0);
    // Probe connected walking lanes along the exported roads, not just their endpoints.
    // Five lanes allow ordinary obstacles to be passed; every transition is collision swept.
    int32 RoadSamples=0,BlockedRoadSamples=0,RoadIndex=0;
    for(auto RoadValue:Data()->GetArrayField(TEXT("roads")))
    {
        const auto& Points=RoadValue->AsArray();TArray<FVector> Previous;
        for(int32 Segment=1;Segment<Points.Num();Segment++)
        {
            const auto& A=Points[Segment-1]->AsArray();const auto& B=Points[Segment]->AsArray();
            const FVector From(-A[2]->AsNumber()*100,A[0]->AsNumber()*100,0),To(-B[2]->AsNumber()*100,B[0]->AsNumber()*100,0);
            const FVector Side=FVector::CrossProduct((To-From).GetSafeNormal2D(),FVector::UpVector);
            const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(From,To)/200.f));
            for(int32 Step=Segment==1?0:1;Step<=Steps;Step++)
            {
                const FVector Centre=FMath::Lerp(From,To,float(Step)/Steps);TArray<FVector> Next;FString Obstacle=TEXT("No connected walkable lane");
                for(float Offset:{0.f,-150.f,150.f,-300.f,300.f})
                {
                    FVector Lane=Centre+Side*Offset;Lane.Z=Ground(Lane.Y/100,-Lane.X/100);const float BedZ=Lane.Z;
                    // A bridge may stand above the riverbed heightfield. Probe
                    // its deck from above instead of testing a route under it.
                    for(TActorIterator<AActor> Bridge(P->GetWorld());Bridge;++Bridge)if(Bridge->ActorHasTag(TEXT("RiverBridge")))
                    {FVector O,E;Bridge->GetActorBounds(false,O,E);if(FMath::Abs(Lane.X-O.X)<E.X&&FMath::Abs(Lane.Y-O.Y)<E.Y)Lane.Z=FMath::Max(Lane.Z,O.Z+E.Z);}
                    FHitResult Floor;
                    if(!P->GetWorld()->LineTraceSingleByChannel(Floor,Lane+FVector(0,0,220),FVector(Lane.X,Lane.Y,BedZ-350),ECC_Visibility,Query)){Obstacle=TEXT("No nearby collision floor");continue;}
                    if(Floor.ImpactNormal.Z<.7f){Obstacle=FString::Printf(TEXT("Steep floor: normal Z %.3f"),Floor.ImpactNormal.Z);continue;}
                    Lane=Floor.ImpactPoint+FVector(0,0,100);
                    if(P->GetWorld()->OverlapBlockingTestByChannel(Lane,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(30,84),Query))
                    {FHitResult Hit;P->GetWorld()->SweepSingleByChannel(Hit,Lane,Lane+FVector(0,0,.1f),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(30,84),Query);if(Hit.GetActor())Obstacle=Hit.GetActor()->GetName();continue;}
                    bool Connected=Previous.IsEmpty();
                    for(const FVector& Before:Previous)
                    {
                        FHitResult Wall;
                        // A walking character may step up 45 cm; check that raised sweep too.
                        if(!P->GetWorld()->SweepSingleByChannel(Wall,Before+FVector(0,0,35),Lane+FVector(0,0,35),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(30,84),Query)){Connected=true;break;}
                        if(Wall.GetActor())Obstacle=Wall.GetActor()->GetName();
                    }
                    if(Connected)Next.Add(Lane);
                }
                RoadSamples++;
                if(Next.IsEmpty())
                {
                    BlockedRoadSamples++;
                    if(BlockedRoadSamples<=100)Problem(TEXT("road"),FString::Printf(TEXT("%d:%d:%d"),RoadIndex,Segment,Step),Centre,Obstacle);
                }
                Previous=MoveTemp(Next);
            }
        }
        RoadIndex++;
    }
    for(auto Value:Problems)
    {
        auto Record=Value->AsObject();const FString Name=Record->GetStringField(TEXT("obstacle"));
        for(TActorIterator<AActor> Actor(P->GetWorld());Actor;++Actor)if(Actor->GetName()==Name)
        {
            Record->SetStringField(TEXT("obstacle_position"),Actor->GetActorLocation().ToString());
            if(auto* Mesh=Actor->FindComponentByClass<UStaticMeshComponent>();Mesh&&Mesh->GetStaticMesh())Record->SetStringField(TEXT("obstacle_mesh"),Mesh->GetStaticMesh()->GetPathName());
            break;
        }
    }
    Check(TEXT("Every sampled road has a connected collision-clear walking lane"),RoadSamples>0&&BlockedRoadSamples==0);
    auto Root=MakeShared<FJsonObject>();Root->SetArrayField(TEXT("problems"),Problems);Root->SetNumberField(TEXT("reachable_items"),Reachable);Root->SetNumberField(TEXT("cave_links"),Links);Root->SetNumberField(TEXT("road_samples"),RoadSamples);Root->SetNumberField(TEXT("blocked_road_samples"),BlockedRoadSamples);FString Out;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Out));FFileHelper::SaveStringToFile(Out,*(FPaths::ProjectDir()/TEXT("Verification/world-traversal.json")));
}
