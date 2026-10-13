#include "FWStory.h"
#include "FWCampaign.h"
#include "FWGame.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Misc/SecureHash.h"

FString FWStory::Line(const FString& Key)
{
    const TSharedPtr<FJsonObject>* Story=nullptr;FString Text;
    if(FWCampaign::Data()->TryGetObjectField(TEXT("story"),Story))(*Story)->TryGetStringField(Key,Text);
    return Text;
}
FString FWStory::Rowan(const AFWCharacter* P)
{
    return Line(P->Expedition->Won?TEXT("rowan_after"):P->HasFlag(TEXT("amulet_truth"))?TEXT("rowan_truth"):P->HasFlag(TEXT("prisoner"))?TEXT("rowan_rescue"):P->HasFlag(TEXT("abbey_page"))?TEXT("rowan_abbey"):TEXT("rowan_search"));
}
FString FWStory::Citizen(const AFWCharacter* P)
{
    return P->Expedition->Won?Line(TEXT("citizen_after")):P->HasFlag(TEXT("supplies_done"))?Line(TEXT("citizen_supplies")):P->HasFlag(TEXT("road_returned"))?Line(TEXT("citizen_road")):FString();
}
FString FWStory::Ending(const AFWCharacter* P)
{
    FString Text=Line(TEXT("ending"));
    if(P->HasFlag(TEXT("supplies_done")))Text+=TEXT("\n\nYour recovered supplies keep Hearthmere's kitchens and stables working.");
    if(P->MaxStamina()>100)Text+=TEXT("\nThe three signal fires already carry the news along the old roads.");
    if(P->HasFlag(TEXT("lore_done")))Text+=TEXT("\nThe reunited histories give Edrin a record no future ruler can easily erase.");
    return Text;
}
void FWStory::EnemyBark(AFWCharacter* P,AFWEnemy* E)
{
    const FString Key=E->Identity==TEXT("jailer")?TEXT("jailer_bark"):E->Identity==TEXT("king")?TEXT("king_bark"):FString();
    if(Key.IsEmpty()||E->StoryBarkPlayed)return;
    E->StoryBarkPlayed=true;const FString Text=Line(Key);P->Notify(E->DisplayName+TEXT(": ")+Text,8);
    FTCHARToUTF8 Encoded(*(E->DisplayName+TEXT("\n")+Text));const FString Hash=FMD5::HashBytes(reinterpret_cast<const uint8*>(Encoded.Get()),Encoded.Length()).ToLower();
    const FString Path=TEXT("/Game/Campaign/Voices/C_")+Hash+TEXT(".C_")+Hash;
    if(auto* Voice=LoadObject<USoundBase>(nullptr,*Path,nullptr,LOAD_NoWarn|LOAD_Quiet))
    {
        auto* Attenuation=NewObject<USoundAttenuation>(E);
        Attenuation->Attenuation.bAttenuate=true;Attenuation->Attenuation.bSpatialize=true;
        Attenuation->Attenuation.AttenuationShapeExtents=FVector(350);Attenuation->Attenuation.FalloffDistance=1700;
        UGameplayStatics::PlaySoundAtLocation(E,Voice,E->GetActorLocation(),.85f,1.f,0.f,Attenuation);
    }
}
