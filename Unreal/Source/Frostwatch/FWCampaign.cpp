#include "FWCampaign.h"
#include "FWConsequences.h"
#include "FWCavePassages.h"
#include "FWStory.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "FWGame.h"
#include "FWHorse.h"
#include "FWWeather.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Components/DirectionalLightComponent.h"

namespace FWCampaign
{
const TSharedPtr<FJsonObject>& Data()
{
    static TSharedPtr<FJsonObject> Root;
    if(!Root){FString Text;FFileHelper::LoadFileToString(Text,*(FPaths::ProjectContentDir()/TEXT("Migration/campaign.json")));FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root);if(!Root)Root=MakeShared<FJsonObject>();}
    return Root;
}
FVector Position(const TSharedPtr<FJsonObject>& R)
{
    const auto& A=R->GetArrayField(TEXT("position"));return FVector(-A[2]->AsNumber()*100,A[0]->AsNumber()*100,A[1]->AsNumber()*100);
}
bool Complete(const AFWCharacter* P,const FString& T)
{
    if(!P||!P->Expedition)return false;
    if(T==TEXT("won"))return P->Expedition->Won;
    if(T.StartsWith(TEXT("seal:")))return P->Expedition->Seals.Contains(FCString::Atoi(*T.Mid(5)));
    if(T.StartsWith(TEXT("defeated:")))return P->Expedition->Defeated.Contains(T.Mid(9));
    return P->HasFlag(T);
}
FString Objective(const AFWCharacter* P)
{
    const auto& Quests=Data()->GetArrayField(TEXT("quests"));
    if(!P->Expedition||!Quests.IsValidIndex(P->Expedition->TrackedQuest))return TEXT("Explore the vale");
    for(auto V:Quests[P->Expedition->TrackedQuest]->AsObject()->GetArrayField(TEXT("steps")))if(!Complete(P,V->AsObject()->GetStringField(TEXT("token"))))return V->AsObject()->GetStringField(TEXT("text"));
    return TEXT("Quest complete. Choose another quest in the journal.");
}
FVector Target(const AFWCharacter* P)
{
    const auto& Quests=Data()->GetArrayField(TEXT("quests"));
    if(P->Expedition&&Quests.IsValidIndex(P->Expedition->TrackedQuest))for(auto V:Quests[P->Expedition->TrackedQuest]->AsObject()->GetArrayField(TEXT("steps")))if(!Complete(P,V->AsObject()->GetStringField(TEXT("token"))))return Position(V->AsObject());
    return P->GetActorLocation();
}
bool InCombat(const AFWCharacter* P)
{
    for(TActorIterator<AFWEnemy> E(P->GetWorld());E;++E)if(!E->Dead&&!E->Friendly&&E->Alerted&&FVector::DistSquared(E->GetActorLocation(),P->GetActorLocation())<4500*4500)return true;
    return false;
}
bool Travel(AFWCharacter* P,const FString& Camp)
{
    auto Camps=Data()->GetObjectField(TEXT("camps"));
    if(!P->Expedition->Discovered.Contains(Camp)||!Camps->HasField(Camp))return false;
    if(P->Health<=0||InCombat(P)){P->Notify(TEXT("Reach safety before travelling or resting."));return false;}
    if(P->MountedHorse)P->MountedHorse->Dismount(true);
    const auto& A=Camps->GetArrayField(Camp);
    FVector Destination(-A[2]->AsNumber()*100,A[0]->AsNumber()*100,A[1]->AsNumber()*100+100);
    P->GetCharacterMovement()->StopMovementImmediately();P->SetActorLocation(Destination,false,nullptr,ETeleportType::TeleportPhysics);
    P->Expedition->Checkpoint=Destination;P->Expedition->CheckpointId=Camp;P->Health=100;P->Stamina=P->MaxStamina();P->SetScreen(TEXT(""));P->Save();P->Notify(TEXT("Rested at camp. Vitality restored and checkpoint saved."));return true;
}
bool Interact(AFWCharacter* P,const FString& Identity)
{
    auto S=P->Expedition;if(!S)return false;
    if(Identity.StartsWith(TEXT("passage:"))){FWCavePassages::Use(P,Identity);return true;}
    const FString Id=Identity==TEXT("rowan")?TEXT("mara"):Identity;
    TSharedPtr<FJsonObject> Item;
    for(auto V:Data()->GetArrayField(TEXT("items")))if(V->AsObject()->GetStringField(TEXT("id"))==Id){Item=V->AsObject();break;}
    if(!Item)return false;
    const FString Type=Item->GetStringField(TEXT("type"));FString Town;Item->TryGetStringField(TEXT("town"),Town);
    FString Words;Item->TryGetStringField(TEXT("text"),Words);
    auto Say=[&](const FString& Text){P->ShowDialogue(Item->GetStringField(TEXT("name")),Text);};
    auto Set=[&](const FString& F){S->Flags.AddUnique(F);};
    if(Type==TEXT("talk"))
    {
        Set(TEXT("briefed"));
        if(!P->HasFlag(TEXT("road_briefed")))
        {
            Set(TEXT("road_briefed"));
            const bool ExistingProgress=S->Won||P->HasFlag(TEXT("prisoner"))||P->HasFlag(TEXT("amulet_truth"))||P->HasFlag(TEXT("abbey_page"));
            Say(ExistingProgress?FWStory::Rowan(P):FWStory::Line(TEXT("rowan_brief")));
        }
        else if(P->HasFlag(TEXT("lore_falls"))&&S->Defeated.Contains(TEXT("road_sentry"))&&!P->HasFlag(TEXT("road_returned")))
        {Set(TEXT("road_returned"));S->Silver+=25;P->Health=100;S->TrackedQuest=0;Say(FWStory::Line(TEXT("rowan_return")));}
        else if(P->HasFlag(TEXT("cargo_lake"))&&P->HasFlag(TEXT("cargo_hollow"))&&!P->HasFlag(TEXT("supplies_done")))
        {Set(TEXT("supplies_done"));P->Health=100;Say(TEXT("Medicine, food, dry bandages... you have given this village another winter. Take my whetstone. Your sword now deals 37 damage instead of 31."));}
        else Say(FWStory::Rowan(P));
    }
    else if(Type==TEXT("prisoner"))
    {
        if(!P->HasFlag(TEXT("prisoner"))&&!S->Defeated.Contains(TEXT("jailer"))){Say(TEXT("Defeat Ste Chlomaine to release Edrin."));return true;}
        Set(Id);Say(FWStory::Line(S->Won?TEXT("edrin_after"):TEXT("edrin_rescue")));
    }
    else if(Type==TEXT("seal"))
    {
        const int32 Index=FCString::Atoi(*Id.Mid(5));
        if(Index==2&&!S->Defeated.Contains(TEXT("king"))){Say(TEXT("Stu Geramos still binds the Crown oath. Defeat him first."));return true;}
        S->Seals.AddUnique(Index);Say(FWStory::Line(Index==0?TEXT("oath_wolf"):Index==1?TEXT("oath_root"):TEXT("oath_crown")));
    }
    else if(Type==TEXT("beacon"))
    {
        if(S->Seals.Num()!=3||!S->Defeated.Contains(TEXT("king"))||!S->Defeated.Contains(TEXT("jailer"))||!P->HasFlag(TEXT("prisoner"))){Say(TEXT("The Last Ember needs all three oaths, Edrin's freedom and the defeat of both tyrants."));return true;}
        S->Won=true;P->Save();P->SetScreen(TEXT("Victory"));return true;
    }
    else if(Type==TEXT("camp")){S->Discovered.AddUnique(Town);return Travel(P,Town);}
    else if(Type==TEXT("bed")||Type==TEXT("keeper"))
    {
        if(InCombat(P)){Say(TEXT("Reach safety before using the inn."));return true;}
        if(Type==TEXT("keeper")&&S->Defeated.Contains(TEXT("ashwing"))&&S->Defeated.Contains(TEXT("rimefang"))&&!P->HasFlag(TEXT("drakes_done")))
        {Set(TEXT("drakes_done"));S->Silver+=80;Say(TEXT("Both drakes are defeated. The roads are safe again. The innkeepers award you the 80 silver bounty."));}
        else
        {
            const int Cost=Type==TEXT("bed")?5:3;
            if(S->Silver<Cost){Say(TEXT("You do not have enough silver. Camps offer free rest."));return true;}
            // Inn services have a separate priced choice, so merely speaking never spends silver.
            P->ShowDialogue(Item->GetStringField(TEXT("name")),Type==TEXT("bed")?TEXT("A room costs 5 in-game silver. Rest eight hours, restore vitality and set this town as your checkpoint."):TEXT("A hot meal costs 3 in-game silver. Restore 45 vitality and all endurance."));P->ServiceId=Id;return true;
        }
    }
    else if(Type==TEXT("bard"))
    {
        Set(TEXT("tale_")+Town);TArray<FString> Lines;for(auto V:Data()->GetObjectField(TEXT("tales"))->GetArrayField(Town))Lines.Add(V->AsString());Say(FString::Join(Lines,TEXT("\n\n")));
    }
    else if(Type==TEXT("orc"))
    {
        Set(TEXT("elder_met"));
        if(P->HasFlag(TEXT("hoard_ashfang"))&&!P->HasFlag(TEXT("lute_done"))){Set(TEXT("lute_done"));S->Silver+=35;Say(TEXT("My brother's lute! Take 35 silver and this ember charm. It will soften the dragons' fire."));}
        else Say(TEXT("My brother's lute lies in the hoard at Ashfang Grotto. Recover it and bring it to either woodland elder. The drake guards its cave, but its breath cannot pass through stone."));
    }
    else if(Type==TEXT("citizen"))
    {
        FString Job;Item->TryGetStringField(TEXT("job"),Job);
        const FString Changed=FWStory::Citizen(P);
        if(!Changed.IsEmpty())Say(Changed);
        else if(Job==TEXT("stable hand"))Say(TEXT("I keep the horses fed, watered and ready for the road. Mount with your interact key, hold sprint for a gallop, and dismount on open ground. A cart horse is slower, but the wagon makes a long journey easier. You'll find a saddle horse and cart at each town's eastern livery."));
        else if(Job==TEXT("herder"))Say(TEXT("The animals still need their feed, whatever the kings are doing. I check the paddock in daylight, then join the others at the inn. Keep the gate clear for travellers and their horses."));
        else Say(TEXT("I work the daylight and warm my hands at the inn after sundown. The roads have grown dangerous since the watch fell. Ask the innkeeper for a room or a hot meal. The bards know the caves and the woodland elders better than any map."));
    }
    else if(Type==TEXT("hoard")){if(!P->HasFlag(Id)){Set(Id);S->Silver+=25;}Say(Id==TEXT("hoard_ashfang")?TEXT("Recovered the lute and 25 silver. Return the lute to a woodland elder."):TEXT("Recovered the dragon's treasure: 25 silver."));}
    else
    {
        if(Id==TEXT("amulet_truth")&&!(P->HasFlag(TEXT("witness_0"))&&P->HasFlag(TEXT("witness_1"))&&P->HasFlag(TEXT("witness_2")))){Say(TEXT("Read all three witnesses before claiming the Amulet of Truth."));return true;}
        Set(Id);
        if(Type==TEXT("letter"))Words=FWStory::Line(TEXT("abbey_letter"));
        if(Type==TEXT("cargo"))Words=P->HasFlag(TEXT("supplies_done"))?TEXT("These supplies now stock Hearthmere's kitchens and stables. Captain Rowan has received both deliveries."):TEXT("Supplies recovered. Bring both chests to Captain Rowan in Hearthmere.");
        if(Type==TEXT("signal"))Words=TEXT("Signal fire restored. Relight all three to increase your endurance to 120.");
        if(Id==TEXT("lore_stones"))Words=TEXT("Nine rulers promised to keep the watch. Nine stones carry their failures. Beneath the newest name: A crown is a debt, not a prize.");
        if(Id==TEXT("lore_falls"))Words=FWStory::Line(TEXT("memorial"));
        if(Id==TEXT("lore_watch"))Words=TEXT("The first keeper was a ferryman without a title. He lit the watch because someone had to. Let the next keeper remember.");
        if(P->HasFlag(TEXT("lore_stones"))&&P->HasFlag(TEXT("lore_falls"))&&P->HasFlag(TEXT("lore_watch"))&&!P->HasFlag(TEXT("lore_done"))){Set(TEXT("lore_done"));P->Health=100;Words+=TEXT(" The three histories are reunited. You are Keeper of the Vale. Vitality restored.");}
        Say(Words.IsEmpty()?TEXT("Recorded in your journal."):Words);
    }
    FWConsequences::Update(P);P->Save();return true;
}
void Update(AFWCharacter* P,float Dt)
{
    auto S=P->Expedition;if(!S)return;FWConsequences::Update(P);if(!P->CanAct())return;
    S->Hour+=Dt/70.f;if(S->Hour>=24){S->Hour-=24;S->Day++;}
    // AFWWeather reads this saved clock and owns sky, light, fog and precipitation together.
    for(auto V:Data()->GetArrayField(TEXT("places")))
    {
        auto R=V->AsObject();const FString Id=R->GetStringField(TEXT("id"));FVector Loc(-R->GetNumberField(TEXT("z"))*100,R->GetNumberField(TEXT("x"))*100,0);
        if(!S->Discovered.Contains(Id)&&FVector::Dist2D(P->GetActorLocation(),Loc)<R->GetNumberField(TEXT("radius"))*100){S->Discovered.Add(Id);P->Notify(TEXT("DISCOVERED: ")+R->GetStringField(TEXT("name")));P->Save();}
    }
}
void Test(AFWCharacter* P)
{
    TArray<TSharedPtr<FJsonValue>> Results;bool Passed=true;
    auto Check=[&](const FString& Name,bool OK){auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("name"),Name);R->SetBoolField(TEXT("passed"),OK);Results.Add(MakeShared<FJsonValueObject>(R));Passed&=OK;};
    FWConsequences::Test(P,Check);
    WorldChecks(P,Check);
    const FVector Inn(-43300,-2800,545);
    Check(TEXT("Inn music is audible inside the inn"),AFWGameMode::InnMusicGain(Inn,Inn,true)>0);
    Check(TEXT("Inn music is silent on the street outside"),AFWGameMode::InnMusicGain(Inn+FVector(1200,0,0),Inn,true)==0);
    Check(TEXT("Inn music is silent beside the side wall"),AFWGameMode::InnMusicGain(Inn+FVector(0,1100,0),Inn,true)==0);
    Check(TEXT("Inn music is silent above the roof"),AFWGameMode::InnMusicGain(Inn+FVector(0,0,600),Inn,true)==0);
    Check(TEXT("Music toggle silences the inn"),AFWGameMode::InnMusicGain(Inn,Inn,false)==0);
    P->Restoring=true;P->Expedition=Cast<UFWExpedition>(UGameplayStatics::CreateSaveGameObject(UFWExpedition::StaticClass()));auto S=P->Expedition;
    AFWWeather* Weather=nullptr;for(TActorIterator<AFWWeather> It(P->GetWorld());It;++It){Weather=*It;break;}
    Check(TEXT("Living sky and both authored snowfall systems are present"),Weather&&Weather->SkyDome->GetMaterial(0)&&Weather->Snowfall->Template&&Weather->Blizzard->Template&&Weather->Sun&&Weather->Fog&&Weather->SkyLight);
    if(Weather)
    {
        const FVector BeforeWeather=P->GetActorLocation();
        Check(TEXT("Southern woodland has a thawed climate"),AFWWeather::ColdAt(FVector(-39000,0,500))<.05f);
        Check(TEXT("Rimefang highlands retain their frozen climate"),AFWWeather::ColdAt(FVector(38600,42000,6000))>.95f);
        Check(TEXT("Ashwing territory suppresses snowfall"),AFWWeather::ColdAt(FVector(39600,-46000,5500))<.05f);
        P->SetActorLocation(FVector(10000,0,5000));
        S->Hour=9;Weather->Tick(.1f);const float ClearCover=Weather->CloudCover;
        S->Hour=12;Weather->Tick(.1f);
        Check(TEXT("Saved clock drives a visible changing weather front"),Weather->CurrentHour==12&&Weather->CloudCover>ClearCover+.5f&&Weather->SnowStrength>.8f);
        S->Hour=21;Weather->Tick(.1f);
        Check(TEXT("Night sky follows the same clock used by civilian routines"),Weather->CurrentHour==21&&Weather->SkyMaterial!=nullptr);
        int32 NightLamps=0;for(TActorIterator<APointLight> L(P->GetWorld());L;++L)if(L->ActorHasTag(TEXT("RoadLantern"))&&L->GetLightComponent()->Intensity>500)NightLamps++;
        Check(TEXT("Settlement roads have at least 24 working night lamps"),NightLamps>=24);
        Check(TEXT("Outdoor moonlight illuminates terrain independently of fountains"),Weather->Sun&&Weather->Sun->GetLightComponent()->Intensity>=1.f);
        S->Hour=9;Weather->Tick(.1f);
        bool LampsOff=true;for(TActorIterator<APointLight> L(P->GetWorld());L;++L)if(L->ActorHasTag(TEXT("RoadLantern")))LampsOff&=L->GetLightComponent()->Intensity<1;
        Check(TEXT("Road lamps switch off in daylight"),LampsOff);
        bool CastleDay=true,PrisonLight=true;
        for(TActorIterator<ALight> L(P->GetWorld());L;++L)if(L->ActorHasTag(TEXT("CastleLighting"))||L->ActorHasTag(TEXT("CastleAtmosphere")))
        {
            const auto At=L->GetActorLocation();auto* C=L->GetLightComponent();
            const bool Prison=At.X>1900&&At.X<2500&&At.Y>2000&&At.Y<3400;
            const bool Point=Cast<APointLight>(*L)!=nullptr;
            const bool Interior=(At.X>=3000&&FMath::Abs(At.Y)<1500)||(Prison&&Point);
            if(!Interior)CastleDay&=C->Intensity<1;
            if(Prison&&Point)PrisonLight&=C->Intensity>=200&&C->Intensity<=250;
        }
        Check(TEXT("Exterior castle accents fade out in daylight"),CastleDay);
        Check(TEXT("Prison practical lamps retain bounded interior illumination"),PrisonLight);
        P->SetActorLocation(BeforeWeather);
    }
    for(TActorIterator<AFWEnemy> E(P->GetWorld());E;++E){E->Alerted=false;E->SetActorTickEnabled(false);}
    FWCavePassages::Test(P,Check);
    int32 CastleLamps=0;bool CastleBudget=true;
    for(TActorIterator<APointLight> L(P->GetWorld());L;++L)if(L->ActorHasTag(TEXT("CastleLighting")))
    {CastleLamps++;auto* C=Cast<UPointLightComponent>(L->GetLightComponent());CastleBudget&=C&&C->Intensity>=0&&C->Intensity<=2600&&C->AttenuationRadius<=2300&&!C->CastShadows;}
    Check(TEXT("Castle gate courtyard hall and prison have sixteen bounded practical lights"),CastleLamps==16&&CastleBudget);
    int32 CastleAccents=0,CastleShadows=0;bool AccentBudget=true;
    for(TActorIterator<ASpotLight> L(P->GetWorld());L;++L)if(L->ActorHasTag(TEXT("CastleAtmosphere")))
    {CastleAccents++;auto* C=Cast<USpotLightComponent>(L->GetLightComponent());AccentBudget&=C&&C->AttenuationRadius<=2900;if(C&&C->CastShadows)CastleShadows++;}
    Check(TEXT("Castle atmosphere uses eleven bounded accents and only two shadowed spotlights"),CastleAccents==11&&CastleShadows==2&&AccentBudget);
    USoundBase* PreviousVoice=nullptr;bool CastVoices=true;
    for(const FString Id:{TEXT("keeper_hearth"),TEXT("keeper_bracken"),TEXT("keeper_eastmere")})
    {Interact(P,Id);auto* Sound=P->DialogueVoice?P->DialogueVoice->Sound.Get():nullptr;CastVoices&=Sound&&Sound!=PreviousVoice&&Sound->GetName().StartsWith(TEXT("C_"));PreviousVoice=Sound;}
    Check(TEXT("Innkeepers use distinct character-specific recordings"),CastVoices);
    P->ShowDialogue(TEXT("Unavailable voice"),TEXT("No authored recording for this test."));
    Check(TEXT("Missing dialogue recording clears the previous voice"),P->DialogueVoice==nullptr);
    P->IntroPage=0;P->MenuAction(TEXT("Intro next"));
    Check(TEXT("Opening pauses gameplay and starts narrated arrival"),P->Screen==TEXT("Opening")&&P->IntroPage==1&&UGameplayStatics::IsGamePaused(P)&&P->DialogueVoice!=nullptr);
    P->MenuAction(TEXT("Intro next"));P->MenuAction(TEXT("Intro next"));
    Check(TEXT("Opening presents controls and Rowan direction before entry"),P->Screen==TEXT("Opening")&&P->IntroPage==3&&P->DialogueText.Contains(TEXT("Captain Rowan")));
    P->MenuAction(TEXT("Intro next"));
    Check(TEXT("Opening entry resumes play with the roadkeeper introduction tracked"),P->Screen.IsEmpty()&&!UGameplayStatics::IsGamePaused(P)&&P->HasFlag(TEXT("opening_seen"))&&S->TrackedQuest==9);
    S->Flags.Remove(TEXT("opening_seen"));P->IntroPage=0;P->MenuAction(TEXT("Intro next"));P->MenuAction(TEXT("Skip opening"));
    Check(TEXT("Skip opening records completion and stops narration"),P->Screen.IsEmpty()&&P->HasFlag(TEXT("opening_seen"))&&P->DialogueVoice==nullptr&&!UGameplayStatics::IsGamePaused(P));
    S->Flags.Remove(TEXT("opening_seen"));P->IntroPage=0;P->MenuAction(TEXT("Intro next"));P->AnyKey(EKeys::Enter);
    Check(TEXT("Enter advances the narrated opening"),P->Screen==TEXT("Opening")&&P->IntroPage==2);
    P->ToggleMenu();Check(TEXT("Escape skips opening with objective and narration cleaned up"),P->HasFlag(TEXT("opening_seen"))&&!P->Menu&&P->DialogueVoice==nullptr);
    P->ShowDialogue(TEXT("Test"),TEXT("Test"));P->AnyKey(EKeys::Enter);
    Check(TEXT("Enter closes dialogue without purchasing a service"),!P->Menu);
    Check(TEXT("Nine existing quest chains plus the roadkeeper introduction and eighteen locations loaded"),Data()->GetArrayField(TEXT("quests")).Num()==10&&Data()->GetArrayField(TEXT("places")).Num()==18);
    Interact(P,TEXT("mara"));
    Check(TEXT("Rowan gives the focused roadkeeper introduction"),P->HasFlag(TEXT("road_briefed")));
    const int BeforeRoadReward=S->Silver;S->Flags.AddUnique(TEXT("lore_falls"));S->Defeated.AddUnique(TEXT("road_sentry"));
    Interact(P,TEXT("mara"));
    Check(TEXT("Roadkeeper return rewards once and leads into the main campaign"),P->HasFlag(TEXT("road_returned"))&&S->Silver==BeforeRoadReward+25&&S->TrackedQuest==0);
    Interact(P,TEXT("mara"));Check(TEXT("Repeated roadkeeper return cannot duplicate its reward"),S->Silver==BeforeRoadReward+25);
    Check(TEXT("Rowan remembers the missing mentor and has an exact voiced recording"),P->DialogueText==FWStory::Line(TEXT("rowan_search"))&&P->DialogueVoice!=nullptr);
    S->Flags.AddUnique(TEXT("abbey_page"));Interact(P,TEXT("mara"));
    Check(TEXT("Abbey evidence changes Rowan's story guidance"),P->DialogueText==FWStory::Line(TEXT("rowan_abbey"))&&P->DialogueVoice!=nullptr);
    S->Flags.AddUnique(TEXT("prisoner"));Interact(P,TEXT("mara"));
    Check(TEXT("Rescuing Edrin advances Rowan beyond the abbey evidence"),P->DialogueText==FWStory::Line(TEXT("rowan_rescue"))&&P->DialogueVoice!=nullptr);
    S->Flags.Remove(TEXT("prisoner"));S->Flags.Remove(TEXT("abbey_page"));
    Interact(P,TEXT("amulet_truth"));Check(TEXT("Amulet remains sealed without witnesses"),!P->HasFlag(TEXT("amulet_truth")));
    Interact(P,TEXT("prisoner"));Check(TEXT("Prisoner requires jailer's defeat"),!P->HasFlag(TEXT("prisoner")));
    Interact(P,TEXT("seal:2"));Check(TEXT("Crown oath requires Stu's defeat"),!S->Seals.Contains(2));
    Interact(P,TEXT("won"));Check(TEXT("Beacon rejects incomplete campaign"),!S->Won);
    Interact(P,TEXT("mara"));Interact(P,TEXT("cargo_lake"));Interact(P,TEXT("mara"));Check(TEXT("One supply chest cannot claim reward"),!P->HasFlag(TEXT("supplies_done")));
    Interact(P,TEXT("cargo_hollow"));Interact(P,TEXT("mara"));Check(TEXT("Both chests unlock whetstone reward"),P->HasFlag(TEXT("supplies_done")));
    for(const FString Id:{TEXT("fire_abbey"),TEXT("fire_hollow"),TEXT("fire_watch")})Interact(P,Id);
    Check(TEXT("Three restored signals increase endurance"),P->MaxStamina()==120);
    for(const FString Id:{TEXT("lore_stones"),TEXT("lore_falls"),TEXT("lore_watch")})Interact(P,Id);
    Check(TEXT("Three histories grant Keeper of the Vale"),P->HasFlag(TEXT("lore_done")));
    for(const FString Id:{TEXT("bard_hearth"),TEXT("bard_bracken"),TEXT("bard_eastmere")})Interact(P,Id);
    Interact(P,TEXT("orc_0"));Interact(P,TEXT("hoard_ashfang"));const int Before=S->Silver;Interact(P,TEXT("hoard_ashfang"));Check(TEXT("Hoard pays only once"),S->Silver==Before);
    Interact(P,TEXT("orc_1"));const int Lute=S->Silver;Interact(P,TEXT("orc_0"));Check(TEXT("Lute reward pays once across both elders"),S->Silver==Lute&&P->HasFlag(TEXT("lute_done")));
    Interact(P,TEXT("mirror_map"));Interact(P,TEXT("truth_map"));Interact(P,TEXT("mirror_shield"));
    Interact(P,TEXT("witness_0"));Interact(P,TEXT("witness_1"));Interact(P,TEXT("amulet_truth"));Check(TEXT("Two witnesses cannot release amulet"),!P->HasFlag(TEXT("amulet_truth")));
    Interact(P,TEXT("witness_2"));Interact(P,TEXT("amulet_truth"));Check(TEXT("All witnesses release amulet"),P->HasFlag(TEXT("amulet_truth")));
    // Exercise the placed boss actors' damage/death paths rather than injecting defeated flags.
    auto DefeatBoss=[&](const FString& Id)
    {
        AFWEnemy* Boss=nullptr;for(TActorIterator<AFWEnemy> E(P->GetWorld());E;++E)if(E->Identity==Id){Boss=*E;break;}
        if(Boss)for(int32 I=0;I<100&&!Boss->Dead;I++)Boss->ReceiveStrike(48,FVector::ForwardVector,true);
        Check(TEXT("Placed boss records defeat through combat damage: ")+Id,Boss&&Boss->Dead&&S->Defeated.Contains(Id));
    };
    auto DefeatDrake=[&](const FString& Id)
    {
        AFWEnemy* Drake=nullptr;for(TActorIterator<AFWEnemy> E(P->GetWorld());E;++E)if(E->Identity==Id){Drake=*E;break;}
        const FVector BeforePosition=P->GetActorLocation();
        const FRotator BeforeRotation=P->Controller->GetControlRotation();
        if(Drake)
        {
            const float BeforeHealth=Drake->Health;Drake->ReceiveStrike(48,FVector::ForwardVector,true);
            Check(TEXT("Placed drake rejects sword damage: ")+Id,Drake->Health==BeforeHealth);
            const FVector Muzzle=Drake->BreathOrigin();
            P->SetActorLocation(Muzzle+Drake->GetActorForwardVector()*600-FVector(0,0,66));
            P->Controller->SetControlRotation((Drake->GetActorLocation()-P->GetActorLocation()).Rotation());
            P->SetActorRotation(FRotator(0,P->Controller->GetControlRotation().Yaw,0));
            P->Blocking=true;P->Invulnerability=0;
            for(int32 I=0;I<100&&!Drake->Dead;I++)
            {
                P->Stamina=100;Drake->BreathTime=2;Drake->BreathHitClock=0;
                Drake->BreathDirection=(P->Camera->GetComponentLocation()-Muzzle).GetSafeNormal();
                Drake->TickDragon(.16f,P);
            }
            P->Blocking=false;
        }
        Check(TEXT("Placed drake falls to reflected breath: ")+Id,Drake&&Drake->Dead&&S->Defeated.Contains(Id));
        P->SetActorLocation(BeforePosition);P->Controller->SetControlRotation(BeforeRotation);P->SetActorRotation(FRotator(0,BeforeRotation.Yaw,0));
    };
    Interact(P,TEXT("abbey_page"));Interact(P,TEXT("seal:0"));DefeatBoss(TEXT("jailer"));Interact(P,TEXT("prisoner"));Interact(P,TEXT("seal:1"));DefeatBoss(TEXT("king"));Interact(P,TEXT("seal:2"));Interact(P,TEXT("won"));Check(TEXT("Full main quest dependencies unlock ending"),S->Won);
    S->Flags.Remove(TEXT("road_briefed"));Interact(P,TEXT("mara"));Check(TEXT("An older completed save receives Rowan's voiced aftermath instead of restarting the missing-mentor story"),P->DialogueText==FWStory::Line(TEXT("rowan_after"))&&P->DialogueVoice!=nullptr&&P->HasFlag(TEXT("road_briefed")));
    const FString Epilogue=FWStory::Ending(P);
    Check(TEXT("Epilogue acknowledges completed supplies signals and histories"),Epilogue.Contains(TEXT("kitchens"))&&Epilogue.Contains(TEXT("signal fires"))&&Epilogue.Contains(TEXT("reunited histories")));
    DefeatDrake(TEXT("ashwing"));DefeatDrake(TEXT("rimefang"));Interact(P,TEXT("keeper_hearth"));const int Bounty=S->Silver;Interact(P,TEXT("keeper_bracken"));Check(TEXT("Dragon bounty pays once across inns"),P->HasFlag(TEXT("drakes_done"))&&S->Silver==Bounty);
    P->MenuAction(TEXT("Buy service"));Check(TEXT("Explicit inn purchase charges three silver"),S->Silver==Bounty-3);
    for(auto Q:Data()->GetArrayField(TEXT("quests"))){bool Done=true;for(auto V:Q->AsObject()->GetArrayField(TEXT("steps")))Done&=Complete(P,V->AsObject()->GetStringField(TEXT("token")));Check(TEXT("Complete quest: ")+Q->AsObject()->GetStringField(TEXT("title")),Done);}
    TArray<uint8> Bytes;UGameplayStatics::SaveGameToMemory(S,Bytes);auto* Loaded=Cast<UFWExpedition>(UGameplayStatics::LoadGameFromMemory(Bytes));Check(TEXT("Campaign save round trip preserves ending, rewards and oaths"),Loaded&&Loaded->Won&&Loaded->Silver==S->Silver&&Loaded->Seals.Num()==3&&Loaded->Flags.Num()==S->Flags.Num());
    auto Root=MakeShared<FJsonObject>();Root->SetArrayField(TEXT("checks"),Results);Root->SetBoolField(TEXT("all_passed"),Passed);Root->SetStringField(TEXT("scope"),TEXT("Campaign rules and persistence; does not prove physical traversal or combat victory"));FString Text;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectDir()/TEXT("Verification/campaign-tests.json")));FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
}
}
