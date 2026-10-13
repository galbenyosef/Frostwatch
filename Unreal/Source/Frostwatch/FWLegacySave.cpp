#include "FWLegacySave.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

bool FWLegacySave::Convert(const FString& Text,UFWExpedition*& Result,FString& Error)
{
    Result=nullptr;Error.Empty();
    if(Text.Len()>4*1024*1024){Error=TEXT("Legacy save exceeds 4 MiB");return false;}
    TSharedPtr<FJsonObject> Root;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root)||!Root){Error=TEXT("Invalid JSON object");return false;}
    double Version=0;if(!Root->TryGetNumberField(TEXT("version"),Version)||Version!=4){Error=TEXT("Only legacy save version 4 is supported");return false;}
    auto* Save=NewObject<UFWExpedition>();Save->OriginalLegacyJson=Text;
    auto ReadStrings=[](const TSharedPtr<FJsonObject>& Object,const FString& Name,TArray<FString>& Values)
    {
        const TArray<TSharedPtr<FJsonValue>>* Array=nullptr;
        if(Object->TryGetArrayField(Name,Array))for(const auto& Value:*Array){FString String;if(Value->TryGetString(String)&&String.Len()<=128)Values.AddUnique(String);}
    };
    ReadStrings(Root,TEXT("defeated"),Save->Defeated);
    Root->TryGetBoolField(TEXT("won"),Save->Won);
    const TArray<TSharedPtr<FJsonValue>>* Seals=nullptr;
    if(Root->TryGetArrayField(TEXT("seals"),Seals))for(const auto& Value:*Seals){double Id;if(Value->TryGetNumber(Id)&&Id>=0&&Id<=2&&FMath::FloorToDouble(Id)==Id)Save->Seals.AddUnique(int32(Id));}
    const TSharedPtr<FJsonObject>* Quests=nullptr;
    if(Root->TryGetObjectField(TEXT("quests"),Quests))
    {
        const TSharedPtr<FJsonObject>* Flags=nullptr;
        if((*Quests)->TryGetObjectField(TEXT("flags"),Flags))for(const auto& Entry:(*Flags)->Values){bool Set=false;if(Entry.Value->TryGetBool(Set)&&Set&&Entry.Key.Len()<=128)Save->Flags.AddUnique(FString(*Entry.Key));}
        ReadStrings(*Quests,TEXT("discovered"),Save->Discovered);
        (*Quests)->TryGetStringField(TEXT("checkpoint"),Save->CheckpointId);
        double Tracked=0;if((*Quests)->TryGetNumberField(TEXT("tracked"),Tracked)&&FMath::IsFinite(Tracked))Save->TrackedQuest=int32(FMath::Clamp(Tracked,0.0,8.0));
    }
    const TSharedPtr<FJsonObject>* Life=nullptr;
    if(Root->TryGetObjectField(TEXT("life"),Life))
    {
        double Value;
        if((*Life)->TryGetNumberField(TEXT("silver"),Value)&&FMath::IsFinite(Value))Save->Silver=int32(FMath::Clamp(Value,0.0,999999.0));
        if((*Life)->TryGetNumberField(TEXT("hour"),Value)&&FMath::IsFinite(Value))Save->Hour=float(FMath::Clamp(Value,0.0,23.99));
        if((*Life)->TryGetNumberField(TEXT("day"),Value)&&FMath::IsFinite(Value))Save->Day=int32(FMath::Clamp(Value,1.0,1000000.0));
        (*Life)->TryGetBoolField(TEXT("music"),Save->Music);(*Life)->TryGetBoolField(TEXT("lantern"),Save->Lantern);
    }
    FString WorldText;TSharedPtr<FJsonObject> World;
    TSet<FString> KnownPlaces;TMap<FString,FVector> Camps;
    if(FFileHelper::LoadFileToString(WorldText,*(FPaths::ProjectContentDir()/TEXT("Migration/world.json")))&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(WorldText),World))
    {
        for(auto& Value:World->GetArrayField(TEXT("places")))
        {
            auto Place=Value->AsObject();FString Id=Place->GetStringField(TEXT("id"));KnownPlaces.Add(Id);
            if(Place->GetBoolField(TEXT("camp")))Camps.Add(Id,FVector(-Place->GetNumberField(TEXT("z"))*100,Place->GetNumberField(TEXT("x"))*100,Place->GetNumberField(TEXT("y"))*100+110));
        }
        Save->Discovered.RemoveAll([&](const FString& Id){return !KnownPlaces.Contains(Id);});
        if(!Camps.Contains(Save->CheckpointId))Save->CheckpointId=TEXT("hearth");
        if(Camps.Contains(Save->CheckpointId))Save->Checkpoint=Camps[Save->CheckpointId];
    }
    const TArray<TSharedPtr<FJsonValue>>* Vehicles=nullptr;TSet<FString> Seen;
    if(Root->TryGetArrayField(TEXT("riding"),Vehicles))for(const auto& Value:*Vehicles)
    {
        const TSharedPtr<FJsonObject>* Item=nullptr;if(!Value->TryGetObject(Item))continue;
        FString Id;double X,Z,Yaw,Energy=100;
        if(!(*Item)->TryGetStringField(TEXT("id"),Id)||Seen.Contains(Id))continue;
        bool Known=false;for(const FString Town:{TEXT("hearth"),TEXT("bracken"),TEXT("eastmere")})if(Id==Town+TEXT("_horse")||Id==Town+TEXT("_cart"))Known=true;
        if(!Known||!(*Item)->TryGetNumberField(TEXT("x"),X)||!(*Item)->TryGetNumberField(TEXT("z"),Z)||!(*Item)->TryGetNumberField(TEXT("yaw"),Yaw))continue;
        if(!FMath::IsFinite(X)||!FMath::IsFinite(Z)||!FMath::IsFinite(Yaw)||FMath::Abs(X)>590||FMath::Abs(Z)>590)continue;
        (*Item)->TryGetNumberField(TEXT("energy"),Energy);if(!FMath::IsFinite(Energy))Energy=100;
        FFWParkedVehicle Vehicle;Vehicle.Identity=Id;Vehicle.Position=FVector(-100*Z,100*X,0);Vehicle.Yaw=float(-FMath::RadiansToDegrees(Yaw));Vehicle.Energy=float(FMath::Clamp(Energy,0.0,100.0));
        Save->ParkedVehicles.Add(Vehicle);Seen.Add(Id);
    }
    // Vehicle Z and safe clearance must be resolved by the future rideable system before spawning.
    Result=Save;return true;
}
