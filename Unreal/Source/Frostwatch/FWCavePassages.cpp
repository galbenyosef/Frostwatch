#include "FWCavePassages.h"
#include "FWGame.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
namespace FWCavePassages
{
bool Use(AFWCharacter* P,const FString& Identity)
{
    // Old save/interaction identities cannot perform teleport travel anymore.
    if(P)P->Notify(TEXT("Follow the tunnel on foot. The caves join beneath the mountains."));
    return false;
}
static FVector ReadPoint(const TSharedPtr<FJsonValue>& Value)
{const auto& A=Value->AsArray();return FVector(A[0]->AsNumber(),A[1]->AsNumber(),A[2]->AsNumber())*100;}
void WalkReview(AFWCharacter* P)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("CaveWalkReview")))return;
    static TArray<FVector> Route;static int Next=1;static float Started=0,Last=0,Travelled=0;static FVector Previous;static bool Done=false;
    const float Now=P->GetWorld()->GetTimeSeconds();if(Now<4||Done)return;
    if(Route.IsEmpty())
    {
        FString Text;TSharedPtr<FJsonObject> Data;
        if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectContentDir()/TEXT("Migration/cave-network.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data))return;
        for(auto V:Data->GetArrayField(TEXT("edges"))){auto E=V->AsObject();if(E->GetStringField(TEXT("a"))==TEXT("hollow")&&E->GetStringField(TEXT("b"))==TEXT("east"))for(auto Point:E->GetArrayField(TEXT("points")))Route.Add(ReadPoint(Point));}
        if(Route.Num()<2)return;
        Started=Now;Previous=Route[0]+FVector(0,0,100);P->SetActorLocation(Previous);P->GetCharacterMovement()->StopMovementImmediately();P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    }
    Travelled+=FVector::Dist(Previous,P->GetActorLocation());Previous=P->GetActorLocation();
    while(Next<Route.Num()&&FVector::Dist2D(P->GetActorLocation(),Route[Next])<60)Next++;
    if(Next<Route.Num())
    {
        const FVector Direction=(Route[Next]-P->GetActorLocation()).GetSafeNormal2D();P->AddMovementInput(Direction);
        P->GetController()->SetControlRotation(FRotator(-8,Direction.Rotation().Yaw,0));
    }
    const bool Complete=Next>=Route.Num();Done=Complete||Now-Started>180;
    if(Now-Last>1||Done)
    {
        Last=Now;auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("all_passed"),Complete&&P->GetCharacterMovement()->IsMovingOnGround());R->SetBoolField(TEXT("complete"),Complete);
        R->SetNumberField(TEXT("travelled_metres"),Travelled/100);R->SetNumberField(TEXT("seconds"),Now-Started);R->SetNumberField(TEXT("waypoints_reached"),Next);R->SetNumberField(TEXT("total_waypoints"),Route.Num());R->SetNumberField(TEXT("speed_cm_s"),P->GetVelocity().Size2D());R->SetStringField(TEXT("location"),P->GetActorLocation().ToString());
        FString Text;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectDir()/TEXT("Verification/cave-walk-review.json")));
    }
    if(Done&&FParse::Param(FCommandLine::Get(),TEXT("CaveWalkTest")))FPlatformMisc::RequestExitWithStatus(false,Complete?0:1);
}
void Test(AFWCharacter* P,TFunctionRef<void(const FString&,bool)> Check)
{
    FString Text;TSharedPtr<FJsonObject> Data;
    const bool Loaded=FFileHelper::LoadFileToString(Text,*(FPaths::ProjectContentDir()/TEXT("Migration/cave-network.json")))&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data);
    Check(TEXT("Physical cave network is packaged"),Loaded);if(!Loaded)return;
    const auto& Edges=Data->GetArrayField(TEXT("edges"));const auto Nodes=Data->GetObjectField(TEXT("nodes"));
    TSet<FString> Reached;Reached.Add(TEXT("veil"));
    for(int Pass=0;Pass<Nodes->Values.Num();Pass++)for(const auto& V:Edges){auto E=V->AsObject();const auto A=E->GetStringField(TEXT("a")),B=E->GetStringField(TEXT("b"));if(Reached.Contains(A)||Reached.Contains(B)){Reached.Add(A);Reached.Add(B);}}
    Check(TEXT("Waterfall and all three caves share one physically connected graph"),Reached.Num()==Nodes->Values.Num()&&Reached.Contains(TEXT("hollow_in"))&&Reached.Contains(TEXT("ashfang_in"))&&Reached.Contains(TEXT("wyrmcrag_in")));
    Check(TEXT("Underground routes include multiple loops and substantial walking distance"),Edges.Num()-Nodes->Values.Num()+1>=2&&Data->GetNumberField(TEXT("length_m"))>1500);
    int OldMarkers=0;for(TActorIterator<AFWInteractable> I(P->GetWorld());I;++I)if(I->Identity.StartsWith(TEXT("passage:")))OldMarkers++;
    Check(TEXT("No interact-to-teleport cave markers remain"),OldMarkers==0);
    const auto Before=P->GetActorLocation();Check(TEXT("Legacy cave interaction cannot relocate the player"),!Use(P,TEXT("passage:hollow"))&&P->GetActorLocation()==Before);
    FCollisionQueryParams Q(SCENE_QUERY_STAT(CaveNetwork),false,P);TArray<TSharedPtr<FJsonValue>> Results;bool All=true;int Samples=0;
    // This checks permanent walking clearance. Bosses guarding two entrances
    // move during play and are covered separately by combat/contact checks.
    for(TActorIterator<AFWEnemy> I(P->GetWorld());I;++I)Q.AddIgnoredActor(*I);
    const float Half=P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),Radius=P->GetCapsuleComponent()->GetScaledCapsuleRadius();
    TArray<TSharedPtr<FJsonValue>> Routes=Edges;
    auto Approach=MakeShared<FJsonObject>();Approach->SetStringField(TEXT("a"),TEXT("pool bank"));Approach->SetStringField(TEXT("b"),TEXT("waterfall cave"));
    TArray<TSharedPtr<FJsonValue>> ApproachPoints;
    for(const FVector At:{FVector(129,350,21.25),FVector(132,350,20.55),FVector(140,350,20.55),FVector(147.5,350,21.6),FVector(152,350,21.6)})
    {TArray<TSharedPtr<FJsonValue>> Row;for(double N:{At.X,At.Y,At.Z})Row.Add(MakeShared<FJsonValueNumber>(N));ApproachPoints.Add(MakeShared<FJsonValueArray>(Row));}
    Approach->SetArrayField(TEXT("points"),ApproachPoints);Routes.Add(MakeShared<FJsonValueObject>(Approach));
    for(const FString Area:{TEXT("hollow"),TEXT("ashfang"),TEXT("wyrmcrag")})
    {
        auto Entry=MakeShared<FJsonObject>();Entry->SetStringField(TEXT("a"),Area+TEXT(" hillside"));Entry->SetStringField(TEXT("b"),Area+TEXT(" entrance"));
        const FVector Mouth=ReadPoint(MakeShared<FJsonValueArray>(Nodes->GetArrayField(Area+TEXT("_out"))))/100;
        TArray<TSharedPtr<FJsonValue>> Points;
        for(const FVector At:{Mouth-FVector(12,0,0),Mouth}){TArray<TSharedPtr<FJsonValue>> Row;for(double N:{At.X,At.Y,At.Z})Row.Add(MakeShared<FJsonValueNumber>(N));Points.Add(MakeShared<FJsonValueArray>(Row));}
        Entry->SetArrayField(TEXT("points"),Points);Routes.Add(MakeShared<FJsonValueObject>(Entry));
    }
    for(const auto& Value:Routes)
    {
        auto E=Value->AsObject();const auto& Points=E->GetArrayField(TEXT("points"));bool Clear=true,Grounded=true;FVector Previous;bool HasPrevious=false;FString FirstProblem;
        for(int I=1;I<Points.Num();I++)
        {
            const auto A=ReadPoint(Points[I-1]),B=ReadPoint(Points[I]);const int Steps=FMath::Max(1,FMath::CeilToInt(FVector::Dist(A,B)/75));
            for(int J=0;J<=Steps;J++)
            {
                const FVector Expected=FMath::Lerp(A,B,float(J)/Steps);FHitResult Floor;Samples++;
                if(!P->GetWorld()->LineTraceSingleByChannel(Floor,Expected+FVector(0,0,180),Expected-FVector(0,0,100),ECC_Visibility,Q)||Floor.ImpactNormal.Z<P->GetCharacterMovement()->GetWalkableFloorZ()||FMath::Abs(Floor.ImpactPoint.Z-Expected.Z)>90){Grounded=false;if(FirstProblem.IsEmpty())FirstProblem=FString::Printf(TEXT("Missing/wrong floor at %s; hit %s at %s normal %s"),*Expected.ToString(),Floor.GetActor()?*Floor.GetActor()->GetActorNameOrLabel():TEXT("none"),*Floor.ImpactPoint.ToString(),*Floor.ImpactNormal.ToString());HasPrevious=false;continue;}
                const FVector At=Floor.ImpactPoint+FVector(0,0,Half+8);FHitResult Hit;
                if(HasPrevious&&P->GetWorld()->SweepSingleByChannel(Hit,Previous,At,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half),Q))
                {Clear=false;if(FirstProblem.IsEmpty())FirstProblem=FString::Printf(TEXT("Blocked at %s by %s"),*At.ToString(),Hit.GetActor()?*Hit.GetActor()->GetActorNameOrLabel():TEXT("unknown"));}
                Previous=At;HasPrevious=true;
            }
        }
        const FString Name=E->GetStringField(TEXT("a"))+TEXT(" to ")+E->GetStringField(TEXT("b"));
        Check(TEXT("Continuous standing floor and player clearance: ")+Name,Clear&&Grounded);All&=Clear&&Grounded;
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("route"),Name);Row->SetBoolField(TEXT("clear"),Clear);Row->SetBoolField(TEXT("grounded"),Grounded);Row->SetStringField(TEXT("problem"),FirstProblem);Results.Add(MakeShared<FJsonValueObject>(Row));
        if(!FirstProblem.IsEmpty())UE_LOG(LogTemp,Warning,TEXT("FROSTWATCH_CAVE_ROUTE %s: %s"),*Name,*FirstProblem);
    }
    auto Report=MakeShared<FJsonObject>();Report->SetBoolField(TEXT("all_passed"),All);Report->SetNumberField(TEXT("capsule_samples"),Samples);Report->SetArrayField(TEXT("routes"),Results);
    FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectDir()/TEXT("Verification/cave-network-tests.json")));
}
}
