#include "FWGame.h"
#include "FWSave.h"
#include "FWReleaseGraphics.h"
#include "FWDisplay.h"
#include "FWCampaign.h"
#include "FWStory.h"
#include "FWParchment.h"
#include "FWLegacySave.h"
#include "FWHorse.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "EngineUtils.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Misc/SecureHash.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

static const FLinearColor Gold(.86f,.68f,.39f), Ink(.025f,.038f,.055f,.97f), Paper(.88f,.91f,.94f);
static const TCHAR* ExpeditionSlotName=TEXT("FrostwatchUnreal_Expedition_v1");
float AFWCharacter::MaxStamina() const{return HasFlag(TEXT("fire_abbey"))&&HasFlag(TEXT("fire_hollow"))&&HasFlag(TEXT("fire_watch"))?120:100;}
FString AFWCharacter::KeyLabel(const FString& Action) const{for(auto M:GetDefault<UInputSettings>()->GetActionMappings())if(M.ActionName.ToString()==Action)return M.Key.GetDisplayName().ToString();return Action;}
void AFWCharacter::SetScreen(const FString& Name)
{
    if(DialogueVoice&&Name!=TEXT("Dialogue")&&Name!=TEXT("Opening")){DialogueVoice->Stop();DialogueVoice=nullptr;}
    Screen=Name;Menu=!Name.IsEmpty();Journal=Name==TEXT("Journal");Blocking=false;GuardHeld=false;StrikeHold=0;Sprinting=false;AttackAge=-1;QueuedAttack=0;LightChain=0;ChainTime=0;ForwardIntent=StrafeIntent=0;
    if(Menu)GetCharacterMovement()->StopMovementImmediately();
    UGameplayStatics::SetGamePaused(this,Menu);
    if(auto* PC=Cast<APlayerController>(Controller))
    {
        PC->bShowMouseCursor=Menu;PC->bEnableClickEvents=Menu;
        if(Menu){FInputModeGameAndUI Mode;Mode.SetHideCursorDuringCapture(false);PC->SetInputMode(Mode);}else PC->SetInputMode(FInputModeGameOnly());
    }
}
void AFWCharacter::ShowDialogue(const FString& Title,const FString& Text)
{
    if(DialogueVoice)DialogueVoice->Stop();DialogueVoice=nullptr;ServiceId.Empty();DialogueTitle=Title;DialogueText=Text;SetScreen(TEXT("Dialogue"));
    FTCHARToUTF8 Encoded(*Text);const FString Hash=FMD5::HashBytes(reinterpret_cast<const uint8*>(Encoded.Get()),Encoded.Length()).ToLower();
    const FString Path=TEXT("/Game/Campaign/Voices/V_")+Hash+TEXT(".V_")+Hash;
    FTCHARToUTF8 CastEncoded(*(Title+TEXT("\n")+Text));
    const FString CastHash=FMD5::HashBytes(reinterpret_cast<const uint8*>(CastEncoded.Get()),CastEncoded.Length()).ToLower();
    const FString CastPath=TEXT("/Game/Campaign/Voices/C_")+CastHash+TEXT(".C_")+CastHash;
    USoundBase* Sound=LoadObject<USoundBase>(nullptr,*CastPath,nullptr,LOAD_NoWarn|LOAD_Quiet);
    if(!Sound)Sound=LoadObject<USoundBase>(nullptr,*Path,nullptr,LOAD_NoWarn|LOAD_Quiet);
    if(Sound){DialogueVoice=UGameplayStatics::SpawnSound2D(this,Sound,.9f);if(DialogueVoice)DialogueVoice->bIsUISound=true;}
}
void AFWCharacter::ToggleMap(){if(RebindTarget.IsEmpty()&&Started&&Health>0)SetScreen(Screen==TEXT("Map")?TEXT(""):TEXT("Map"));}
void AFWCharacter::ToggleLantern(){if(CanAct()){Expedition->Lantern=!Expedition->Lantern;Save();Notify(Expedition->Lantern?TEXT("Lantern lit"):TEXT("Lantern extinguished"));}}
void AFWCharacter::AnyKey(FKey Key)
{
    if(!RebindTarget.IsEmpty())
    {
        if(Key==EKeys::Escape){RebindTarget.Empty();return;}
        if(Key.IsGamepadKey()) {Notify(TEXT("Choose a keyboard key or mouse button."));return;}
        auto* Settings=GetMutableDefault<UInputSettings>();
        const auto Actions=Settings->GetActionMappings();const auto Axes=Settings->GetAxisMappings();
        FKey Old;bool Axis=RebindTarget.StartsWith(TEXT("axis:"));
        FString Name=Axis?RebindTarget.Mid(5):RebindTarget;float Sign=1;
        if(Axis&&Name.EndsWith(TEXT("-"))){Name.LeftChopInline(1);Sign=-1;}
        else if(Axis&&Name.EndsWith(TEXT("+")))Name.LeftChopInline(1);
        for(auto M:Actions)if(!Axis&&M.ActionName.ToString()==Name){Old=M.Key;break;}
        for(auto M:Axes)if(Axis&&M.AxisName.ToString()==Name&&M.Scale==Sign){Old=M.Key;break;}
        // Swap an existing keyboard assignment so rebinding never silently removes another control.
        for(auto M:Actions)if(M.Key==Key||(!Axis&&M.ActionName.ToString()==Name)){Settings->RemoveActionMapping(M,false);M.Key=M.Key==Key?Old:Key;Settings->AddActionMapping(M,false);}
        for(auto M:Axes)if(M.Key==Key||(Axis&&M.AxisName.ToString()==Name&&M.Scale==Sign)){Settings->RemoveAxisMapping(M,false);M.Key=M.Key==Key?Old:Key;Settings->AddAxisMapping(M,false);}
        Settings->SaveKeyMappings();Settings->ForceRebuildKeymaps();RebindTarget.Empty();return;
    }
    if(Menu&&(Key==EKeys::Right||Key==EKeys::Left)){FWParchment::Action(this,Key==EKeys::Right?TEXT("paper:next"):TEXT("paper:previous"));return;}
    if(Key==EKeys::Enter && Menu)
    {
        if(Screen==TEXT("Opening"))MenuAction(TEXT("Intro next"));
        else if(Screen==TEXT("Dialogue"))SetScreen(TEXT(""));
        else if(Screen==TEXT("Death"))MenuAction(TEXT("Return to checkpoint"));
        return;
    }
    if(Screen==TEXT("Journal"))
    {
        const TArray<FKey> Numbers={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six,EKeys::Seven,EKeys::Eight,EKeys::Nine,EKeys::Zero};
        const int32 Index=Numbers.IndexOfByKey(Key);if(Index!=INDEX_NONE){Expedition->TrackedQuest=Index;Save();}
    }
}
void AFWCharacter::MenuAction(const FString& Action)
{
    if(FWParchment::Action(this,Action))return;
    if(Action==TEXT("Skip opening"))
    {
        IntroPage=3;MenuAction(TEXT("Intro next"));return;
    }
    if(Action==TEXT("Resume")){SetScreen(TEXT(""));return;}
    if(Action==TEXT("Import original save"))
    {
        const FString Original=FPaths::Combine(FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA")),TEXT("Godot/app_userdata"),FString(TEXT("Frostwatch "))+FString::Chr(0x2014)+TEXT(" The Last Ember/expedition.json"));
        FString Text,Error;UFWExpedition* Imported=nullptr;
        if(!FFileHelper::LoadFileToString(Text,*Original)||!FWLegacySave::Convert(Text,Imported,Error)){ShowDialogue(TEXT("Original save import"),Error.IsEmpty()?TEXT("No original Frostwatch expedition was found in this Windows account."):Error);return;}
        if(!FWSave::Preserve(ExpeditionSlotName,TEXT("FrostwatchUnreal_BeforeImport"))||!FWSave::Write(Imported,ExpeditionSlotName)){SaveError=TEXT("Import could not be safely saved. Your original game save is unchanged. Check disk space and folder access.");SetScreen(TEXT("Save problem"));return;}
        Load();return;
    }
    if(Action==TEXT("Intro next"))
    {
        const TArray<FString> Pages={TEXT("Edrin taught you to keep a fire through rain and carry a blade without reaching for it first. Then his letters stopped. You return to Hearthmere with the keeper's blade he left you. The watchfires are dark, the winter roads are closing, and Captain Rowan has news of your missing teacher."),TEXT("Walk with W, A, S and D. Look with the mouse. Press E near a person or an object to interact. Your journal, opened with J, follows ten stories across the vale. M opens the map; its gold marker leads to your current objective. You can change these keys in the controls menu."),TEXT("Find Captain Rowan beside the road in Hearthmere. Your first task is to open the woodland road and restore a small light at Widow's Veil. Watch an enemy prepare a blow, raise your guard as it arrives, then counter while they recover. Your journal follows the journey. Camps restore health and save your return point; the stable hand can help you travel farther.")};
        if(IntroPage<Pages.Num()){ShowDialogue(TEXT("A KEEPER ARRIVES"),Pages[IntroPage++]);SetScreen(TEXT("Opening"));}
        else {Expedition->Flags.AddUnique(TEXT("opening_seen"));Expedition->TrackedQuest=9;Save();SetScreen(TEXT(""));Notify(FString::Printf(TEXT("Find Captain Rowan. %s opens your journal; %s shows the road."),*KeyLabel(TEXT("Journal")),*KeyLabel(TEXT("Map"))),12);}return;
    }
    if(Action==TEXT("New game")){SetScreen(TEXT("New game"));return;}
    if(Action==TEXT("Begin expedition"))
    {
        if(!FWSave::Preserve(ExpeditionSlotName,TEXT("FrostwatchUnreal_BeforeNewGame"))){SaveError=TEXT("The existing expedition could not be safely backed up. New game was cancelled. Check disk space and folder access.");SetScreen(TEXT("Save problem"));return;}
        Expedition=Cast<UFWExpedition>(UGameplayStatics::CreateSaveGameObject(UFWExpedition::StaticClass()));Expedition->Checkpoint=InitialLocation;
        for(TActorIterator<AFWEnemy> E(GetWorld());E;++E)E->ResetEncounter();
        for(TActorIterator<AFWHorse> Horse(GetWorld());Horse;++Horse)Horse->Restore(this,true);
        Health=Stamina=100;SetActorLocation(InitialLocation);Started=true;if(!SaveExpedition()){SetScreen(TEXT("Save problem"));return;}IntroPage=0;MenuAction(TEXT("Intro next"));return;
    }
    if(Action==TEXT("Continue")||Action==TEXT("Return to checkpoint")){Load();return;}
    if(Action==TEXT("Save expedition")){if(SaveExpedition()){Notify(TEXT("Expedition saved. Continue returns to your last camp."));if(Screen==TEXT("Save problem"))SetScreen(Started?TEXT("Pause"):TEXT("Title"));}else SetScreen(TEXT("Save problem"));return;}
    if(Action==TEXT("Title")){if(SaveExpedition())SetScreen(TEXT("Title"));else SetScreen(TEXT("Save problem"));return;}
    if(Action==TEXT("Quit")){if(!SaveExpedition()){SetScreen(TEXT("Save problem"));return;}if(auto* PC=Cast<APlayerController>(Controller))PC->ConsoleCommand(TEXT("quit"));return;}
    if(Action==TEXT("Quit without saving")){if(auto* PC=Cast<APlayerController>(Controller))PC->ConsoleCommand(TEXT("quit"));return;}
    if(Action==TEXT("Back from save problem")){SetScreen(Started?TEXT("Pause"):TEXT("Title"));return;}
    if(Action.StartsWith(TEXT("quest:"))){Expedition->TrackedQuest=FCString::Atoi(*Action.Mid(6));Save();return;}
    if(Action.StartsWith(TEXT("travel:"))){FWCampaign::Travel(this,Action.Mid(7));return;}
    if(Action.StartsWith(TEXT("bind:"))){RebindTarget=Action.Mid(5);return;}
    if(Action==TEXT("Buy service"))
    {
        const FString Id=ServiceId;ServiceId.Empty();const bool Bed=Id.StartsWith(TEXT("bed_"));const int Cost=Bed?5:3;const FString Town=Id.Mid(Bed?4:7);
        if(!Id.IsEmpty()&&Expedition->Silver>=Cost&&!FWCampaign::InCombat(this))
        {
            Expedition->Silver-=Cost;Health=Bed?100:FMath::Min(100.f,Health+45);Stamina=MaxStamina();
            if(Bed){Expedition->Discovered.AddUnique(Town);Expedition->Hour+=8;if(Expedition->Hour>=24){Expedition->Hour-=24;Expedition->Day++;}FWCampaign::Travel(this,Town);}
            Save();SetScreen(TEXT(""));Notify(Bed?TEXT("Rested eight hours."):TEXT("A hot meal restores your strength."));
        }
        return;
    }
    if(Action==TEXT("Keep display")){FWKeepDisplay();return;}
    if(Action==TEXT("Revert display")){FWRevertDisplay();return;}
    if(Action==TEXT("Display mode")){FWPreviewDisplayMode();return;}
    if(Action==TEXT("Window size")){FWPreviewDisplayResolution();return;}
    if(FWDisplaySecondsRemaining()>0)return;
    if(Action.StartsWith(TEXT("Quality:")))
    {
        auto* S=GEngine->GetGameUserSettings();float Normalized,Scale,Minimum,Maximum;S->GetResolutionScaleInformationEx(Normalized,Scale,Minimum,Maximum);
        const int Quality=FMath::Clamp(FCString::Atoi(*Action.Mid(8)),0,3);S->SetOverallScalabilityLevel(Quality);S->SetResolutionScaleValueEx(Scale);S->ApplySettings(false);FWApplyGraphicsBudget(Quality);S->SaveSettings();return;
    }
    if(Action==TEXT("Sensitivity -"))Sensitivity=FMath::Max(.1f,Sensitivity-.1f);
    else if(Action==TEXT("Sensitivity +"))Sensitivity=FMath::Min(2.f,Sensitivity+.1f);
    else if(Action==TEXT("Invert look"))InvertY=!InvertY;
    else if(Action==TEXT("Reduced motion"))ReducedMotion=!ReducedMotion;
    else if(Action==TEXT("Story text size"))StoryTextScale=StoryTextScale<1.2f?1.25f:StoryTextScale<1.45f?1.5f:1.f;
    else if(Action==TEXT("VSync")){auto* S=GEngine->GetGameUserSettings();S->SetVSyncEnabled(!S->IsVSyncEnabled());S->ApplySettings(false);S->SaveSettings();return;}
    else if(Action==TEXT("Frame cap")){auto* S=GEngine->GetGameUserSettings();S->SetFrameRateLimit(S->GetFrameRateLimit()==30?60:S->GetFrameRateLimit()==60?0:30);S->ApplySettings(false);S->SaveSettings();return;}
    else if(Action==TEXT("Render scale"))
    {
        auto* S=GEngine->GetGameUserSettings();float Normalized,Scale,Minimum,Maximum;S->GetResolutionScaleInformationEx(Normalized,Scale,Minimum,Maximum);
        S->SetResolutionScaleValueEx(Scale<59?60:Scale<69?70:Scale<84?85:Scale<99?100:50);S->ApplySettings(false);S->SaveSettings();return;
    }
    else if(Action==TEXT("Music")){Expedition->Music=!Expedition->Music;Save();return;}
    else {SetScreen(Action);return;}
    GConfig->SetFloat(TEXT("Frostwatch"),TEXT("Sensitivity"),Sensitivity,GGameUserSettingsIni);GConfig->SetBool(TEXT("Frostwatch"),TEXT("InvertY"),InvertY,GGameUserSettingsIni);
    GConfig->SetBool(TEXT("Frostwatch"),TEXT("ReducedMotion"),ReducedMotion,GGameUserSettingsIni);GConfig->SetFloat(TEXT("Frostwatch"),TEXT("StoryTextScale"),StoryTextScale,GGameUserSettingsIni);GConfig->Flush(false,GGameUserSettingsIni);
}
void AFWHUD::Button(const FString& Id,const FString& Text,float X,float Y,float W,float H)
{
    float MX=0,MY=0;GetOwningPlayerController()->GetMousePosition(MX,MY);const bool Hover=MX>=X&&MX<=X+W&&MY>=Y&&MY<=Y+H;
    DrawRect(Hover?FLinearColor(.24f,.21f,.15f,.98f):FLinearColor(.085f,.115f,.14f,.96f),X,Y,W,H);
    DrawText(Text,Hover?Gold:Paper,X+12,Y+9,nullptr,.9f);AddHitBox(FVector2D(X,Y),FVector2D(W,H),FName(*Id),true);
}
void AFWHUD::NotifyHitBoxClick(FName Name){if(auto* P=Cast<AFWCharacter>(GetOwningPawn());P&&P->Menu)P->MenuAction(Name.ToString());}
float AFWHUD::Paragraph(const FString& Text,float X,float Y,float W,float Scale)
{
    TArray<FString> Paragraphs;Text.ParseIntoArray(Paragraphs,TEXT("\n"),false);
    for(auto Para:Paragraphs)
    {
        TArray<FString> Words;Para.ParseIntoArrayWS(Words);FString Line;
        for(auto Word:Words){FString Next=Line.IsEmpty()?Word:Line+TEXT(" ")+Word;float TW,TH;GetTextSize(Next,TW,TH,nullptr,Scale);if(TW>W&&!Line.IsEmpty()){DrawText(Line,Paper,X,Y,nullptr,Scale);Y+=21*Scale;Line=Word;}else Line=Next;}
        DrawText(Line,Paper,X,Y,nullptr,Scale);Y+=24*Scale;
    }
    return Y;
}
void AFWHUD::DrawHUD()
{
    Super::DrawHUD();auto* P=Cast<AFWCharacter>(GetOwningPawn());if(!P||!Canvas||!P->Expedition)return;
    const float W=Canvas->SizeX,H=Canvas->SizeY;auto S=P->Expedition;
    if(FWParchment::Draw(this,P,Canvas))return;
    DrawText(TEXT("F R O S T W A T C H"),Gold,28,20,nullptr,1.2f);
    if(!P->Menu)
    {
        Paragraph(P->Objective(),28,55,W*.55f,.95f);
        const FVector To=FWCampaign::Target(P)-P->GetActorLocation();
        DrawText(FString::Printf(TEXT("Objective: %.0f m    Silver: %d    Day %d  %02d:%02d"),To.Size2D()/100,S->Silver,S->Day,int(S->Hour),int(FMath::Frac(S->Hour)*60)),Gold,28,102,nullptr,.8f);
        DrawRect(Ink,28,H-74,234,13);DrawRect(FLinearColor(.65f,.16f,.13f),30,H-72,230*P->Health/100,9);
        DrawRect(Ink,28,H-54,234,9);DrawRect(Gold,30,H-52,230*P->Stamina/P->MaxStamina(),5);
        DrawText(TEXT("Esc menu / controls    ")+P->KeyLabel(TEXT("Journal"))+TEXT(" journal    ")+P->KeyLabel(TEXT("Map"))+TEXT(" map    ")+P->KeyLabel(TEXT("Lantern"))+TEXT(" lantern"),Paper,28,H-29,nullptr,.85f);
        if(P->MountedHorse)DrawText(FString::Printf(TEXT("%s endurance %.0f / 100   %s dismount"),P->MountedHorse->Cart?TEXT("Cart horse"):TEXT("Horse"),P->MountedHorse->Energy,*P->KeyLabel(TEXT("Interact"))),Gold,290,H-54,nullptr,1);
        else for(TActorIterator<AFWHorse> Horse(P->GetWorld());Horse;++Horse)if(FVector::Dist(P->GetActorLocation(),Horse->GetActorLocation())<350){DrawText(P->KeyLabel(TEXT("Interact"))+(Horse->Cart?TEXT("  Drive cart"):TEXT("  Mount horse")),Gold,W/2-100,H/2+65,nullptr,1);break;}
        if(P->CanAct())DrawRect(P->HitFeedback>0?Gold:Paper,W/2-2,H/2-2,4,4);
        if(P->HitFeedback>0)for(float X:{-1.f,1.f})for(float Y:{-1.f,1.f})DrawLine(W/2+X*7,H/2+Y*7,W/2+X*14,H/2+Y*14,Gold,2);
        if(P->ParryFeedback>0){DrawLine(W/2-22,H/2,W/2-12,H/2,FLinearColor(.45f,.85f,1),3);DrawLine(W/2+12,H/2,W/2+22,H/2,FLinearColor(.45f,.85f,1),3);}
        if(auto* E=P->TargetEnemy(1800);E&&!E->Dead)
        {
            DrawText(E->DisplayName,E->Windup>0?FLinearColor(1,.24f,.1f):Paper,W/2-100,H/2-85,nullptr,1);
            if(E->Friendly)DrawText(P->KeyLabel(TEXT("Interact"))+TEXT("  Speak ")+E->Activity,Gold,W/2-110,H/2+30,nullptr,.85f);
            if(!E->Friendly){DrawRect(Ink,W/2-100,H/2-60,200,6);DrawRect(Gold,W/2-100,H/2-60,200*FMath::Clamp(E->Health/E->MaxHealth,0.f,1.f),6);}
            if(!E->Friendly&&E->Windup>0&&!E->Dragon){DrawRect(Ink,W/2-100,H/2-49,200,4);DrawRect(FLinearColor(1,.3f,.12f),W/2-100,H/2-49,200*(1-FMath::Clamp(E->Windup/E->MeleeWindupLength,0.f,1.f)),4);}
            else if(!E->Friendly&&E->RiposteWindow>0)DrawText(TEXT("RIPOSTE"),FLinearColor(.45f,.85f,1),W/2-35,H/2-47,nullptr,.85f);
        }
        for(TActorIterator<AFWInteractable> It(P->GetWorld());It;++It)if(!It->IsHidden()&&FVector::Dist(P->GetActorLocation(),It->GetActorLocation())<330){DrawText(P->KeyLabel(TEXT("Interact"))+TEXT("  ")+It->Title,Gold,W/2-130,H/2+32,nullptr,.95f);break;}
        if(P->CombatMessageTime>0){float TW,TH;GetTextSize(P->CombatMessage,TW,TH,nullptr,1);const float Fade=FMath::Clamp(P->CombatMessageTime/.25f,0.f,1.f);DrawText(P->CombatMessage,FLinearColor(0,0,0,Fade),W/2-TW/2+1,H*.64f+1,nullptr,1);DrawText(P->CombatMessage,FLinearColor(Gold.R,Gold.G,Gold.B,Fade),W/2-TW/2,H*.64f,nullptr,1);}
        if(P->MessageTime>0){DrawRect(Ink,W*.15f,H*.77f,W*.7f,75);Paragraph(P->Message,W*.15f+16,H*.77f+12,W*.7f-32,.9f);}
        if(!P->SaveError.IsEmpty())DrawText(TEXT("PROGRESS NOT SAVED - open the pause menu to retry"),FLinearColor(1,.6f,.25f),28,126,nullptr,.9f);
        if(P->HurtFlash>0)DrawRect(FLinearColor(.5f,0,0,P->HurtFlash*.3f),0,0,W,H);
        return;
    }
    DrawRect(Ink,0,0,W,H);DrawText(TEXT("F R O S T W A T C H  /  THE LAST EMBER"),Gold,35,28,nullptr,1.5f);
    DrawText(P->Screen.ToUpper(),Paper,35,66,nullptr,1.1f);
    if(P->Screen==TEXT("Save problem"))
    {
        Paragraph(P->SaveError,45,115,W-90,1);
        if(P->Started)Button(TEXT("Save expedition"),TEXT("Retry saving this session"),45,230,W-90);
        Button(TEXT("Back from save problem"),TEXT("Back - keep this session open"),45,280,W-90);
        Button(TEXT("Quit without saving"),TEXT("Quit without saving latest progress"),45,350,W-90);
        return;
    }
    if(P->Screen==TEXT("Title")||P->Screen==TEXT("Pause")||P->Screen==TEXT("Death")||P->Screen==TEXT("Victory")||P->Screen==TEXT("New game"))
    {
        const float Step=FMath::Clamp((H-140)/12.f,28.f,47.f);float Y=115;auto Row=[&](const FString& Id,const FString& Text){Button(Id,Text,35,Y,FMath::Min(310.f,W*.36f),Step-6);Y+=Step;};
        if(P->Screen==TEXT("New game")){Paragraph(TEXT("Begin a new expedition? Your current Unreal save is copied to the BeforeNewGame backup slot. Your original game and its saves are preserved."),400,150,W-450,1.1f);Row(TEXT("Begin expedition"),TEXT("Begin expedition"));Row(P->Started?TEXT("Pause"):TEXT("Title"),TEXT("Back"));return;}
        if(!P->SaveError.IsEmpty())Paragraph(P->SaveError,410,145,W-460,1.05f);
        else if(P->Screen==TEXT("Victory"))Paragraph(FWStory::Ending(P),410,145,W-460,1.05f);
        else if(P->Screen==TEXT("Death"))Paragraph(TEXT("YOU FELL\n\nReturn to your saved camp. Completed quests and defeated bosses remain recorded. Ordinary guards and orcs can return after one game day when you have left the area."),410,145,W-460,1.15f);
        else Paragraph(TEXT("Across a winterbound vale, three broken oaths wait to be reunited. Follow the old roads, uncover the witnesses' truth and restore the watch."),410,145,W-460,1.2f);
        if(P->Started&&P->Health>0)Row(TEXT("Resume"),TEXT("Return to the vale"));
        if(P->Screen==TEXT("Death"))Row(TEXT("Return to checkpoint"),TEXT("Return to checkpoint"));
        else if(FWSave::Exists(ExpeditionSlotName))Row(TEXT("Continue"),TEXT("Continue saved expedition"));
        Row(TEXT("New game"),TEXT("New game"));
        if(P->Screen==TEXT("Title"))Row(TEXT("Import original save"),TEXT("Import original game save"));
        if(P->Started&&P->Health>0){Row(TEXT("Journal"),TEXT("Quest journal"));Row(TEXT("Map"),TEXT("Map and fast travel"));Row(TEXT("Save expedition"),TEXT("Save expedition"));}
        Row(TEXT("Settings"),TEXT("Settings"));Row(TEXT("Controls"),TEXT("Controls and key mapping"));Row(TEXT("Fighting"),TEXT("Fighting techniques"));Row(TEXT("Credits"),TEXT("Credits"));Row(TEXT("Quit"),TEXT("Save and quit"));
    }
    else if(P->Screen==TEXT("Journal"))
    {
        const auto& Q=FWCampaign::Data()->GetArrayField(TEXT("quests"));float Y=125;
        for(int I=0;I<Q.Num();I++){auto R=Q[I]->AsObject();int Done=0;const auto& Steps=R->GetArrayField(TEXT("steps"));for(auto V:Steps)Done+=FWCampaign::Complete(P,V->AsObject()->GetStringField(TEXT("token")));
            Button(FString::Printf(TEXT("quest:%d"),I),FString::Printf(TEXT("%d  %s  %d/%d"),I+1,*R->GetStringField(TEXT("title")),Done,Steps.Num()),35,Y,340);Y+=43;}
        auto R=Q[FMath::Clamp(S->TrackedQuest,0,Q.Num()-1)]->AsObject();DrawText(R->GetStringField(TEXT("title")),Gold,410,124,nullptr,1.3f);Y=170;
        for(auto V:R->GetArrayField(TEXT("steps"))){auto Step=V->AsObject();Y=Paragraph((FWCampaign::Complete(P,Step->GetStringField(TEXT("token")))?TEXT("[DONE] "):TEXT("[  ] "))+Step->GetStringField(TEXT("text")),410,Y,W-455,.92f)+10;}
    }
    else if(P->Screen==TEXT("Map"))
    {
        const float Size=FMath::Min(H-180,W*.58f);const FVector2D O(35,120);
        DrawRect(FLinearColor(.105f,.14f,.15f),O.X,O.Y,Size,Size);
        auto Point=[&](double X,double Z){return O+FVector2D((X+600)/1200*Size,(Z+600)/1200*Size);};
        for(int I=0;I<=12;I++){float D=I*Size/12;DrawLine(O.X+D,O.Y,O.X+D,O.Y+Size,FLinearColor(.2f,.25f,.26f,.5f));DrawLine(O.X,O.Y+D,O.X+Size,O.Y+D,FLinearColor(.2f,.25f,.26f,.5f));}
        for(auto Road:FWCampaign::Data()->GetArrayField(TEXT("roads"))){const auto& Points=Road->AsArray();for(int I=1;I<Points.Num();I++){const auto& A=Points[I-1]->AsArray();const auto& B=Points[I]->AsArray();auto Start=Point(A[0]->AsNumber(),A[2]->AsNumber()),End=Point(B[0]->AsNumber(),B[2]->AsNumber());DrawLine(Start.X,Start.Y,End.X,End.Y,FLinearColor(.45f,.4f,.29f),2);}}
        for(auto V:FWCampaign::Data()->GetArrayField(TEXT("places"))){auto R=V->AsObject();auto A=Point(R->GetNumberField(TEXT("x")),R->GetNumberField(TEXT("z")));DrawRect(S->Discovered.Contains(R->GetStringField(TEXT("id")))?Gold:Paper,A.X-3,A.Y-3,6,6);DrawText(R->GetStringField(TEXT("name")),Paper,A.X+5,A.Y,nullptr,.62f);}
        auto Player=Point(P->GetActorLocation().Y/100,-P->GetActorLocation().X/100);DrawRect(FLinearColor(.2f,.85f,1),Player.X-4,Player.Y-4,8,8);
        auto Target=FWCampaign::Target(P);auto T=Point(Target.Y/100,-Target.X/100);DrawLine(T.X-6,T.Y,T.X+6,T.Y,Gold,2);DrawLine(T.X,T.Y-6,T.X,T.Y+6,Gold,2);
        const float X=Size+75;DrawText(TEXT("NORTH ^   /   1.2 km x 1.2 km"),Gold,35,95,nullptr,.8f);Paragraph(TEXT("Blue: your position. Gold cross: tracked objective. Rest at camps to save a checkpoint. Fast travel unlocks when you discover a camp's destination and is unavailable during combat."),X,125,W-X-35,.92f);
        float Y=260;for(auto& Pair:FWCampaign::Data()->GetObjectField(TEXT("camps"))->Values){const FString Id(Pair.Key);if(S->Discovered.Contains(Id)){Button(FString(TEXT("travel:"))+Id,FString(TEXT("Travel to "))+Id,X,Y,W-X-35);Y+=41;}}
    }
    else if(P->Screen==TEXT("Dialogue")||P->Screen==TEXT("Opening"))
    {
        DrawText(P->DialogueTitle,Gold,70,140,nullptr,1.4f);Paragraph(P->DialogueText,70,200,W-140,1.15f);
        if(!P->ServiceId.IsEmpty())Button(TEXT("Buy service"),TEXT("Pay with in-game silver"),70,H-130,310);
        if(P->Screen==TEXT("Opening"))Button(TEXT("Intro next"),P->IntroPage<3?TEXT("Continue"):TEXT("Enter Hearthmere"),70,H-130,310);
        DrawText(P->Screen==TEXT("Opening")?TEXT("Enter: continue    Escape: skip opening"):TEXT("Enter or Escape: close"),Gold,70,H-165,nullptr,.85f);
    }
    else if(P->Screen==TEXT("Fighting"))
    {
        const FString Light=P->KeyLabel(TEXT("Light")),Heavy=P->KeyLabel(TEXT("Heavy")),Guard=P->KeyLabel(TEXT("Block"));
        Paragraph(TEXT("Aim at the body part you want to strike. Wounded arms weaken attacks; wounded legs slow pursuit. Strong cuts can sever a damaged limb. Head or leg removal is fatal. The drake's ward still turns steel."),45,112,W-90,H<650?.8f:1.f);
        TArray<FString> Moves={Light+TEXT(" : alternating cuts. Strafe + ")+Light+TEXT(" : choose left or right cut."),
            TEXT("Forward + ")+Heavy+TEXT(" : driving thrust. Backward + ")+Heavy+TEXT(" : rising cut."),
            P->KeyLabel(TEXT("Crouch"))+TEXT(" to crouch, then ")+Light+TEXT(" : low cut. Look down to aim at a leg."),
            Guard+TEXT(" + ")+Light+TEXT(" : shield bash; follow with a quick riposte."),
            Light+TEXT(" -> ")+Light+TEXT(" -> ")+Heavy+TEXT(" : powerful combination finisher."),
            P->KeyLabel(TEXT("Feint"))+TEXT(" during early wind-up : feint. Costs endurance and a brief recovery."),
            TEXT("Raise ")+Guard+TEXT(" as a blow arrives to parry, then strike during the opening."),
            TEXT("Buffer one follow-up after the swing begins. Watch endurance; walls stop the blade.")};
        const float GuideScale=H<650?.8f:.95f;float Y=H<650?195:235;for(const auto& Move:Moves)Y=Paragraph(Move,45,Y,W-90,GuideScale)+5;
        Button(TEXT("Pause"),TEXT("Back to menu"),45,H-110,220);
    }
    else if(P->Screen==TEXT("Controls"))
    {
        Paragraph(TEXT("Click an assignment, then press a key. Escape cancels. In deep water you float automatically: movement keys swim, Sprint swims faster, and Jump pushes up toward a bank. Face a dragon's breath with the Mirror Shield to reflect it."),35,112,W-70,.95f);
        TArray<TPair<FString,FString>> Rows={{TEXT("axis:Forward+"),TEXT("Move forward")},{TEXT("axis:Forward-"),TEXT("Move backward")},{TEXT("axis:Strafe-"),TEXT("Move left")},{TEXT("axis:Strafe+"),TEXT("Move right")}};
        for(const FString N:{TEXT("Light"),TEXT("Block"),TEXT("Heavy"),TEXT("Feint"),TEXT("Jump"),TEXT("Dodge"),TEXT("Interact"),TEXT("Sprint"),TEXT("Crouch"),TEXT("Journal"),TEXT("Map"),TEXT("Save"),TEXT("Load"),TEXT("Lantern")})Rows.Emplace(N,N);
        auto* Input=GetDefault<UInputSettings>();
        for(int I=0;I<Rows.Num();I++)
        {
            FString Key;auto Id=Rows[I].Key;
            if(Id.StartsWith(TEXT("axis:"))){FString N=Id.Mid(5,Id.Len()-6);float Sign=Id.EndsWith(TEXT("+"))?1:-1;for(auto M:Input->GetAxisMappings())if(M.AxisName.ToString()==N&&M.Scale==Sign)Key=M.Key.GetDisplayName().ToString();}
            else for(auto M:Input->GetActionMappings())if(M.ActionName.ToString()==Id)Key=M.Key.GetDisplayName().ToString();
            Button(TEXT("bind:")+Id,Rows[I].Value+TEXT("    [ ")+Key+TEXT(" ]"),35+(I%2)*(W/2),230+(I/2)*44,W/2-65);
        }
        if(!P->RebindTarget.IsEmpty())DrawText(TEXT("PRESS A KEY FOR: ")+P->RebindTarget,Gold,35,H-85,nullptr,1.2f);
    }
    else if(P->Screen==TEXT("Accessibility"))
    {
        Button(TEXT("Story text size"),FString::Printf(TEXT("Story text: %.0f%%"),P->StoryTextScale*100),45,140,360);
        Button(TEXT("Reduced motion"),P->ReducedMotion?TEXT("Page and dialogue motion: off"):TEXT("Page and dialogue motion: on"),45,200,360);
        Paragraph(TEXT("Larger text reflows onto additional pages in dialogue, found texts and the journal. Page controls and story voices stay available. Turning page motion off also removes the dialogue slide."),45,270,W-90,1);
        Button(TEXT("Settings"),TEXT("Back to settings"),45,380,300);
    }
    else if(P->Screen==TEXT("Settings"))
    {
        const int Selected=GEngine->GetGameUserSettings()->GetGlobalIlluminationQuality();
        const TArray<FString> Names={TEXT("Performance"),TEXT("Balanced"),TEXT("High"),TEXT("Epic")};
        for(int I=0;I<Names.Num();I++)Button(FString::Printf(TEXT("Quality:%d"),I),(Selected==I?TEXT("* "):TEXT(""))+Names[I],45+I*180,140,170);
        Paragraph(TEXT("Performance uses lighter antialiasing and a shorter sun-shadow distance, recommended for 3 GB GPUs. Balanced adds Lumen; High and Epic need substantially more graphics memory. Lowering resolution alone does not remove that memory cost. Sky, weather and water stay active."),45,200,W-90,1);
        Button(TEXT("VSync"),GEngine->GetGameUserSettings()->IsVSyncEnabled()?TEXT("VSync: on"):TEXT("VSync: off"),45,280,270);
        const float Cap=GEngine->GetGameUserSettings()->GetFrameRateLimit();
        Button(TEXT("Frame cap"),Cap>0?FString::Printf(TEXT("Frame cap: %.0f"),Cap):TEXT("Frame cap: uncapped"),335,280,270);
        Button(TEXT("Sensitivity -"),TEXT("Look sensitivity -"),45,340,270);Button(TEXT("Sensitivity +"),TEXT("Look sensitivity +"),335,340,270);
        DrawText(FString::Printf(TEXT("Sensitivity %.2f"),P->Sensitivity),Gold,630,350,nullptr,1);
        Button(TEXT("Invert look"),P->InvertY?TEXT("Invert look: on"):TEXT("Invert look: off"),45,400,270);
        Button(TEXT("Music"),S->Music?TEXT("Inn music: on"):TEXT("Inn music: off"),335,400,270);
        Button(TEXT("Accessibility"),TEXT("Reading and motion"),625,400,270);
        auto* Video=GEngine->GetGameUserSettings();float Normalized,Scale,Minimum,Maximum;Video->GetResolutionScaleInformationEx(Normalized,Scale,Minimum,Maximum);
        Button(TEXT("Render scale"),FString::Printf(TEXT("Render resolution: %.0f%%"),Scale),45,460,270);
        const bool Borderless=Video->GetFullscreenMode()==EWindowMode::WindowedFullscreen;
        const FIntPoint Size=Borderless?Video->GetDesktopResolution():Video->GetScreenResolution();Button(TEXT("Window size"),FString::Printf(TEXT("%s: %d x %d"),Borderless?TEXT("Desktop"):TEXT("Resolution"),Size.X,Size.Y),335,460,270);
        Button(TEXT("Display mode"),FWDisplayModeName(),625,460,270);
        Paragraph(TEXT("Choose Fullscreen for a lower resolution across the whole screen. Borderless uses the desktop size. Render resolution (50-100%) controls internal detail; menus stay sharp. Display changes revert after 15 seconds unless kept."),45,510,W-90,1);
        if(const int Seconds=FWDisplaySecondsRemaining()){Button(TEXT("Keep display"),FString::Printf(TEXT("Keep display (%d seconds)"),Seconds),45,585,270);Button(TEXT("Revert display"),TEXT("Revert display"),335,585,270);}
    }
    else if(P->Screen==TEXT("Credits"))Paragraph(TEXT("FROSTWATCH - THE LAST EMBER\nOriginal game and Unreal migration for Jonathan.\n\nCharacter art: Epic Games Paragon: Greystone; Yagami Orc Character; Inventive World Orc Warrior.\nEnvironment: Poly Haven CC0 textures, fir sapling, wooden crate, barrel and roof tiles.\nHorse: Lyndon Daniels, rig by Chad M (CC0).\n\nBook typography: Cormorant Garamond by Christian Thalmann / The Cormorant Project Authors (SIL OFL 1.1), adapted as Frostwatch Story Serif.\n\nSee CREDITS.md and Licenses in the game folder for complete creator and licence records. Assets remain subject to their respective licences."),50,140,W-100,1.1f);
    if(P->Screen==TEXT("Opening"))Button(TEXT("Skip opening"),TEXT("Skip opening"),35,H-55,240);
    else if(P->Screen!=TEXT("Title")&&P->Screen!=TEXT("Pause")&&P->Screen!=TEXT("Death")&&P->Screen!=TEXT("Victory"))Button(P->Started?(P->Health>0?TEXT("Resume"):TEXT("Death")):TEXT("Title"),P->Started?TEXT("Back to game"):TEXT("Back to title"),35,H-55,240);
}
