#include "FWParchment.h"
#include "FWGame.h"
#include "FWCampaign.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "CanvasItem.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"
#include "GameFramework/PlayerController.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "HighResScreenshot.h"
#include "UnrealClient.h"
#include "Fonts/CompositeFont.h"

namespace
{
const FLinearColor Ink(.065f,.033f,.014f),Red(.28f,.075f,.035f),Rule(.36f,.22f,.10f);
struct FPaperState
{
    TWeakObjectPtr<AFWCharacter> Owner;
    FString Key,ReportKey;
    int32 Page=0,Pages=1,MotionFrame=0;
    double Start=0;
    bool Book=false,AutoTurned=false;
} ParchmentState;

struct FPaperCanvas
{
    AFWHUD* H;UCanvas* C;float S;mutable bool AllTextSubmitted=true;
    FSlateFontInfo Font(float Size) const
    {
        // Canvas requires a runtime UFont, even when Slate can measure a standalone composite.
        static UFont* Typeface=[](){auto* F=NewObject<UFont>();F->AddToRoot();F->FontCacheType=EFontCacheType::Runtime;F->GetMutableInternalCompositeFont()=FCompositeFont(FName(TEXT("Regular")),FPaths::ProjectContentDir()/TEXT("Migration/UI/FrostwatchStorySerif.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);return F;}();
        return FSlateFontInfo(Typeface,Size*S,FName(TEXT("Regular")));
    }
    FVector2D Measure(const FString& T,float Size) const
    {
        return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(T,Font(Size));
    }
    void Text(const FString& T,float X,float Y,float Size=16,FLinearColor Color=Ink) const
    {
        FCanvasTextItem Item(FVector2D(X,Y),FText::FromString(T),Font(Size),Color);
        C->DrawItem(Item);
        if(!T.TrimStartAndEnd().IsEmpty()&&Item.DrawnSize.X<=0)AllTextSubmitted=false;
    }
    void Rect(FLinearColor Color,float X,float Y,float W,float Height) const{H->DrawRect(Color,X,Y,W,Height);}
    void Line(float X,float Y,float XX,float YY,FLinearColor Color=Rule,float Width=1) const{H->DrawLine(X,Y,XX,YY,Color,Width*S);}
    void Quad(FVector2D A,FVector2D B,FVector2D D,FVector2D E,FLinearColor Color) const
    {
        FCanvasUVTri T;T.V0_Pos=A;T.V1_Pos=B;T.V2_Pos=D;T.V0_Color=T.V1_Color=T.V2_Color=Color;
        FCanvasUVTri U=T;U.V1_Pos=D;U.V2_Pos=E;C->K2_DrawTriangle(nullptr,{T,U});
    }
    void Paper(float X,float Y,float W,float Height,int Seed=17) const
    {
        Rect(FLinearColor(0,0,0,.35f),X+7*S,Y+9*S,W,Height);
        Rect(FLinearColor(.49f,.34f,.18f),X-3*S,Y-3*S,W+6*S,Height+6*S);
        for(int I=0;I<32;I++)
        {
            const float T=(I+.5f)/32,Shade=.5f+.5f*FMath::Sin(T*PI);
            Rect(FMath::Lerp(FLinearColor(.65f,.48f,.28f),FLinearColor(.88f,.76f,.54f),Shade),X,Y+Height*I/32,W,Height/32+1);
        }
        FRandomStream R(Seed);
        for(int I=0;I<260;I++)
        {
            const float PX=X+R.FRand()*W,PY=Y+R.FRand()*Height;
            Rect(FLinearColor(.30f,.16f,.06f,R.FRandRange(.035f,.09f)),PX,PY,R.FRandRange(1,4)*S,R.FRandRange(.5f,1.4f)*S);
        }
        for(int I=0;I<26;I++)
        {
            const float PX=X+I*W/26,Notch=R.FRandRange(1,4)*S;
            Rect(FLinearColor(.31f,.18f,.07f,.25f),PX,Y,W/26,Notch);
            Rect(FLinearColor(.31f,.18f,.07f,.2f),PX,Y+Height-Notch,W/26,Notch);
        }
        Line(X+18*S,Y+17*S,X+W-18*S,Y+17*S,FLinearColor(.47f,.31f,.15f,.55f));
        Line(X+18*S,Y+Height-17*S,X+W-18*S,Y+Height-17*S,FLinearColor(.47f,.31f,.15f,.55f));
    }
    void Button(const FString& Id,const FString& Label,float X,float Y,float W,float Height=34) const
    {
        Height*=S;float MX=0,MY=0;H->GetOwningPlayerController()->GetMousePosition(MX,MY);
        const bool Hover=MX>=X&&MX<=X+W&&MY>=Y&&MY<=Y+Height;
        Rect(Hover?FLinearColor(.40f,.23f,.10f,.22f):FLinearColor(.40f,.23f,.10f,.07f),X,Y,W,Height);
        Line(X,Y+Height,X+W,Y+Height,Hover?Red:Rule);
        Text(Label,X+9*S,Y+3*S,14,Hover?Red:Ink);
        H->AddHitBox(FVector2D(X,Y),FVector2D(W,Height),FName(*Id),true);
    }
    TArray<FString> Wrap(const FString& T,float Width,float Size=16) const
    {
        TArray<FString> Result,Paragraphs;T.ParseIntoArray(Paragraphs,TEXT("\n"),false);
        for(const auto& Para:Paragraphs)
        {
            TArray<FString> Words;Para.ParseIntoArrayWS(Words);FString Row;
            for(const auto& Word:Words)
            {
                const FString Next=Row.IsEmpty()?Word:Row+TEXT(" ")+Word;
                if(!Row.IsEmpty()&&Measure(Next,Size).X>Width){Result.Add(Row);Row=Word;}else Row=Next;
            }
            Result.Add(Row);
        }
        return Result;
    }
};
bool Reading(AFWCharacter* P)
{
    if(P->Screen==TEXT("Opening")||P->Screen==TEXT("Journal"))return true;
    if(P->Screen!=TEXT("Dialogue"))return false;
    for(auto V:FWCampaign::Data()->GetArrayField(TEXT("items")))
    {
        const auto I=V->AsObject();if(I->GetStringField(TEXT("name"))!=P->DialogueTitle)continue;
        const FString T=I->GetStringField(TEXT("type"));
        return T==TEXT("letter")||T==TEXT("lore")||T==TEXT("relic_quest")||T==TEXT("seal");
    }
    return false;
}
void Emblem(const FPaperCanvas& D,float X,float Y,float Radius)
{
    for(int I=0;I<48;I++)
    {
        const float A=I*2*PI/48,B=(I+1)*2*PI/48;
        D.Line(X+FMath::Cos(A)*Radius,Y+FMath::Sin(A)*Radius,X+FMath::Cos(B)*Radius,Y+FMath::Sin(B)*Radius,Rule,1.2f);
    }
    for(int I=0;I<8;I++)
    {
        const float A=I*PI/4;
        D.Line(X+FMath::Cos(A)*Radius*.62f,Y+FMath::Sin(A)*Radius*.62f,X+FMath::Cos(A)*Radius*1.15f,Y+FMath::Sin(A)*Radius*1.15f,Red);
    }
    const FVector2D Points[]={{-.6f,-.35f},{-.42f,.48f},{.42f,.48f},{.6f,-.35f},{.22f,0},{0,-.65f},{-.22f,0},{-.6f,-.35f}};
    for(int I=1;I<UE_ARRAY_COUNT(Points);I++)D.Line(X+Points[I-1].X*Radius,Y+Points[I-1].Y*Radius,X+Points[I].X*Radius,Y+Points[I].Y*Radius,Red,2);
}
}

bool FWParchment::Action(AFWCharacter* P,const FString& Action)
{
    if(!Action.StartsWith(TEXT("paper:")))return false;
    if(ParchmentState.Owner!=P||!P->Menu)return true;
    const int Next=FMath::Clamp(ParchmentState.Page+(Action==TEXT("paper:next")?1:-1),0,ParchmentState.Pages-1);
    if(Next!=ParchmentState.Page){ParchmentState.Page=Next;ParchmentState.Start=FPlatformTime::Seconds();ParchmentState.MotionFrame=0;}
    return true;
}

bool FWParchment::Draw(AFWHUD* H,AFWCharacter* P,UCanvas* C)
{
    const bool Journal=P->Screen==TEXT("Journal"),Opening=P->Screen==TEXT("Opening");
    if(!P->Menu||(!Journal&&!Opening&&P->Screen!=TEXT("Dialogue")))
    {if(ParchmentState.Owner==P)ParchmentState.Key.Empty();return false;}
    if(!FSlateApplication::IsInitialized())return false;
    const float W=C->SizeX,Height=C->SizeY,S=FMath::Min(W/1280.f,Height/720.f);
    FPaperCanvas D{H,C,S};const bool Book=Reading(P);
    const FString Key=P->Screen+P->DialogueTitle+P->DialogueText+(Journal?FString::FromInt(P->Expedition->TrackedQuest):FString());
    if(ParchmentState.Owner!=P||ParchmentState.Key!=Key)
    {ParchmentState.Owner=P;ParchmentState.Key=Key;ParchmentState.Page=0;ParchmentState.Pages=1;ParchmentState.Start=FPlatformTime::Seconds();ParchmentState.MotionFrame=0;ParchmentState.Book=Book;ParchmentState.AutoTurned=false;}
    const float Age=float(FPlatformTime::Seconds()-ParchmentState.Start),T=P->ReducedMotion?1.f:FMath::Clamp(Age/.55f,0.f,1.f),Ease=1-FMath::Pow(1-T,3);
    const float BodySize=16*FMath::Clamp(P->StoryTextScale,1.f,1.5f);
    FString Title=P->DialogueTitle,Body=P->DialogueText;
    if(Journal)
    {
        const auto& Quests=FWCampaign::Data()->GetArrayField(TEXT("quests"));
        auto Q=Quests[FMath::Clamp(P->Expedition->TrackedQuest,0,Quests.Num()-1)]->AsObject();Title=Q->GetStringField(TEXT("title"));Body.Empty();
        int N=0;for(auto V:Q->GetArrayField(TEXT("steps")))
        {auto Step=V->AsObject();Body+=FString::Printf(TEXT("%s %d. %s\n"),FWCampaign::Complete(P,Step->GetStringField(TEXT("token")))?TEXT("[done]"):TEXT("[  ]"),++N,*Step->GetStringField(TEXT("text")));}
    }
    Body.TrimEndInline();
    // Exercise pagination with an intentionally long passage only in explicit capture runs.
    if(FParse::Param(FCommandLine::Get(),TEXT("FrostwatchCapture"))&&FParse::Param(FCommandLine::Get(),TEXT("CapturePaperLong")))Body=Body+TEXT("\n\n")+Body+TEXT("\n\n")+Body;
    float X,Y,PW,PH,TX,TY,TW,Available,FooterY;
    const float LineHeight=D.Measure(TEXT("Ag"),BodySize).Y+5*S;
    if(Book)
    {
        D.Rect(FLinearColor(.006f,.009f,.012f,.68f),0,0,W,Height);
        PW=FMath::Min(W-80*S,1200*S);PH=FMath::Min(Height-90*S,620*S);X=(W-PW)/2;Y=(Height-PH)/2;
        D.Rect(FLinearColor(0,0,0,.5f),X-10*S,Y+8*S,PW+28*S,PH+14*S);
        D.Rect(FLinearColor(.095f,.043f,.020f),X-12*S,Y-10*S,PW+24*S,PH+20*S);
        for(int I=0;I<5;I++)D.Rect(FLinearColor(.48f+.02f*I,.35f+.018f*I,.20f+.013f*I),X+I*S,Y+PH+I*2*S,PW-I*2*S,2*S);
        D.Paper(X,Y,PW/2-3*S,PH,31);D.Paper(X+PW/2+3*S,Y,PW/2-3*S,PH,73);
        for(int I=0;I<20;I++)D.Rect(FLinearColor(.16f,.07f,.018f,.3f*(1-I/20.f)),X+PW/2-2*S+I*S,Y,2*S,PH);
        const float LX=X+32*S,LeftWidth=PW/2-65*S;
        D.Text(Journal?TEXT("THE KEEPER'S JOURNAL"):TEXT("FROSTWATCH"),LX,Y+27*S,21,Red);
        D.Line(LX,Y+65*S,LX+LeftWidth,Y+65*S);
        if(Journal)
        {
            const auto& Quests=FWCampaign::Data()->GetArrayField(TEXT("quests"));float QY=Y+88*S;
            D.Text(TEXT("Select a chapter to follow its trail."),LX,QY,14);QY+=35*S;
            for(int I=0;I<Quests.Num();I++)
            {
                auto Q=Quests[I]->AsObject();int Done=0;const auto& Steps=Q->GetArrayField(TEXT("steps"));for(auto V:Steps)Done+=FWCampaign::Complete(P,V->AsObject()->GetStringField(TEXT("token")));
                const FString Label=FString::Printf(TEXT("%s%d  %s  %d/%d"),I==P->Expedition->TrackedQuest?TEXT("> "):TEXT(""),I+1,*Q->GetStringField(TEXT("title")),Done,Steps.Num());
                D.Button(FString::Printf(TEXT("quest:%d"),I),Label,LX,QY,LeftWidth,33);QY+=39*S;
            }
        }
        else
        {
            D.Text(TEXT("The Last Ember"),LX,Y+96*S,32,Red);
            Emblem(D,X+PW*.25f,Y+PH*.49f,78*S);
            const auto Quote=D.Wrap(TEXT("A keeper carries light for others, never for a throne."),LeftWidth-30*S,18);
            float QY=Y+PH*.72f;for(auto& L:Quote){D.Text(L,LX+15*S,QY,18);QY+=30*S;}
            D.Text(Opening?TEXT("An account of your arrival"):TEXT("From the records of the vale"),LX,Y+PH-100*S,14,Rule);
        }
        TX=X+PW/2+34*S;TW=PW/2-72*S;TY=Y+31*S;
        for(auto& L:D.Wrap(Title,TW,23)){D.Text(L,TX,TY,23,Red);TY+=34*S;}
        TY+=17*S;D.Line(TX,TY-7*S,TX+TW,TY-7*S);FooterY=Y+PH-68*S;Available=FooterY-TY-20*S;
    }
    else
    {
        PW=FMath::Min(W-100*S,1160*S);TW=PW-90*S;
        const int Lines=D.Wrap(Body,TW,BodySize).Num();PH=FMath::Clamp(Lines*LineHeight+175*S,255*S,Height*.62f);
        X=(W-PW)/2;Y=Height-PH-24*S+(1-Ease)*(PH+35*S);
        D.Rect(FLinearColor(.015f,.008f,.002f,.18f),0,0,W,Height);
        D.Paper(X,Y,PW,PH,19);
        for(int I=0;I<10;I++)
        {
            const float V=FMath::Sin((I+.5f)/10*PI);
            const FLinearColor Roll=FMath::Lerp(FLinearColor(.37f,.22f,.09f),FLinearColor(.89f,.76f,.52f),V);
            D.Rect(Roll,X-8*S,Y+I*1.3f*S,PW+16*S,1.5f*S);
        }
        TX=X+45*S;TY=Y+62*S;D.Text(Title,TX,Y+23*S,23,Red);D.Line(TX,Y+57*S,TX+TW,Y+57*S);
        FooterY=Y+PH-86*S;Available=FooterY-TY-15*S;
    }
    auto Lines=D.Wrap(Body,TW,BodySize);const int PerPage=FMath::Max(1,FMath::FloorToInt(Available/LineHeight));
    ParchmentState.Pages=FMath::Max(1,FMath::DivideAndRoundUp(Lines.Num(),PerPage));ParchmentState.Page=FMath::Clamp(ParchmentState.Page,0,ParchmentState.Pages-1);
    const int Start=ParchmentState.Page*PerPage,End=FMath::Min(Lines.Num(),Start+PerPage);
    float Bottom=TY;FString Visible;
    for(int I=Start;I<End;I++){D.Text(Lines[I],TX,Bottom,BodySize);Bottom+=LineHeight;Visible+=Lines[I]+TEXT("\n");}
    if(ParchmentState.Pages>1)
    {
        if(ParchmentState.Page>0)D.Button(TEXT("paper:previous"),TEXT("< Previous leaf"),TX,FooterY,TW*.38f);
        if(ParchmentState.Page<ParchmentState.Pages-1)D.Button(TEXT("paper:next"),TEXT("Next leaf >"),TX+TW*.58f,FooterY,TW*.42f);
        D.Text(FString::Printf(TEXT("%d / %d"),ParchmentState.Page+1,ParchmentState.Pages),TX+TW*.43f,FooterY+5*S,13,Rule);
    }
    if(Book)
    {
        D.Button(Opening?TEXT("Skip opening"):TEXT("Resume"),Opening?TEXT("Close prologue [Esc]"):TEXT("Close book [Esc]"),X+30*S,Y+PH-54*S,215*S);
        if(Opening&&ParchmentState.Page==ParchmentState.Pages-1)D.Button(TEXT("Intro next"),P->IntroPage<3?TEXT("Turn the page >"):TEXT("Enter Hearthmere >"),TX,FooterY,TW);
        // A leaf rotates around the spine while game time remains paused. Real time drives the turn.
        if(T<1)
        {
            const float Cosa=FMath::Cos(T*PI),Spine=X+PW/2;
            const float Far=Spine+Cosa*(PW/2-4*S),Lift=FMath::Sin(T*PI)*24*S;
            D.Quad({Spine,Y},{Far,Y+Lift},{Far,Y+PH-Lift},{Spine,Y+PH},FMath::Lerp(FLinearColor(.56f,.39f,.19f),FLinearColor(.86f,.72f,.49f),FMath::Abs(Cosa)));
            D.Rect(FLinearColor(.13f,.06f,.015f,.23f),FMath::Min(Spine,Far),Y+Lift,4*S,PH-2*Lift);
        }
    }
    else
    {
        if(!P->ServiceId.IsEmpty())D.Button(TEXT("Buy service"),TEXT("Pay with in-game silver"),TX,FooterY+34*S,275*S);
        D.Button(TEXT("Resume"),TEXT("Close  [Enter / Esc]"),X+PW-250*S,FooterY+34*S,205*S);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("FrostwatchCapture")))
    {
        int32 RequestedPage=1;FParse::Value(FCommandLine::Get(),TEXT("CapturePaperPage="),RequestedPage);
        if(RequestedPage>1&&!ParchmentState.AutoTurned&&Age>.8f)
        {ParchmentState.AutoTurned=true;for(int32 I=1;I<FMath::Min(RequestedPage,20);I++)P->MenuAction(TEXT("paper:next"));return true;}
        const FString ReportKey=Key+FString::FromInt(ParchmentState.Page)+FString::FromInt(int(W))+FString::FromInt(int(Height));
        if(Age>.7f&&ParchmentState.ReportKey!=ReportKey)
        {
            ParchmentState.ReportKey=ReportKey;auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("text_submitted"),D.AllTextSubmitted);R->SetStringField(TEXT("screen"),P->Screen);R->SetBoolField(TEXT("book"),Book);R->SetNumberField(TEXT("page"),ParchmentState.Page+1);R->SetNumberField(TEXT("pages"),ParchmentState.Pages);
            float Widest=0;for(int32 I=Start;I<End;I++)Widest=FMath::Max(Widest,float(D.Measure(Lines[I],BodySize).X));
            R->SetNumberField(TEXT("story_text_scale"),P->StoryTextScale);R->SetBoolField(TEXT("reduced_motion"),P->ReducedMotion);
            R->SetStringField(TEXT("visible_text"),Visible);R->SetBoolField(TEXT("text_fits"),Bottom<=FooterY+1&&Widest<=TW+1);R->SetNumberField(TEXT("text_bottom"),Bottom);R->SetNumberField(TEXT("footer_y"),FooterY);R->SetNumberField(TEXT("width"),W);R->SetNumberField(TEXT("height"),Height);R->SetNumberField(TEXT("requested_page"),RequestedPage);R->SetBoolField(TEXT("page_request_satisfied"),ParchmentState.Page+1==FMath::Clamp(RequestedPage,1,ParchmentState.Pages));
            FString Json;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*(FPaths::ProjectDir()/TEXT("Verification/parchment-layout.json")));
        }
        const float Frames[]={.07f,.18f,.32f,.46f,.68f};
        if(FParse::Param(FCommandLine::Get(),TEXT("CapturePaperMotion"))&&ParchmentState.MotionFrame<UE_ARRAY_COUNT(Frames)&&Age>=Frames[ParchmentState.MotionFrame])
        {const FString File=FPaths::ProjectDir()/FString::Printf(TEXT("Verification/parchment-motion-%d.png"),ParchmentState.MotionFrame++);FScreenshotRequest::RequestScreenshot(File,true,false);}
    }
    return true;
}
