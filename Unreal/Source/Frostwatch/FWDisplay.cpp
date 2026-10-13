#include "FWDisplay.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/PlatformTime.h"
#include "RHI.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    double Deadline=0;
    FIntPoint PreviousSize;
    EWindowMode::Type PreviousMode=EWindowMode::Windowed;
    UGameUserSettings* Settings(){return GEngine->GetGameUserSettings();}
    void Remember()
    {
        if(Deadline==0){PreviousSize=Settings()->GetScreenResolution();PreviousMode=Settings()->GetFullscreenMode();}
        Deadline=FPlatformTime::Seconds()+15;
    }
    void Apply(){Settings()->ApplyResolutionSettings(false);}
    TArray<FIntPoint> Sizes()
    {
        TArray<FIntPoint> Result;
        FScreenResolutionArray Supported;
        const bool Enumerated=RHIGetAvailableResolutions(Supported,true)&&Supported.Num()>0;
        FIntPoint Desktop=Settings()->GetDesktopResolution();if(Desktop.X<1280||Desktop.Y<720)Desktop=FIntPoint(1920,1080);
        for(FIntPoint Size:{FIntPoint(1280,720),FIntPoint(1366,768),FIntPoint(1600,900),FIntPoint(1920,1080),Desktop})
            if(Size.X<=Desktop.X&&Size.Y<=Desktop.Y&&(!Enumerated||Supported.ContainsByPredicate([Size](const FScreenResolutionRHI& R){return R.Width==uint32(Size.X)&&R.Height==uint32(Size.Y);})))Result.AddUnique(Size);
        if(Result.IsEmpty())Result.Add(Desktop);
        return Result;
    }
}

void FWPreviewDisplayMode()
{
    Remember();const auto Mode=Settings()->GetFullscreenMode();
    Settings()->SetFullscreenMode(Mode==EWindowMode::Windowed?EWindowMode::Fullscreen:Mode==EWindowMode::Fullscreen?EWindowMode::WindowedFullscreen:EWindowMode::Windowed);Apply();
}
void FWPreviewDisplayResolution()
{
    if(Settings()->GetFullscreenMode()==EWindowMode::WindowedFullscreen)return;
    Remember();const auto Choices=Sizes();int32 Index=Choices.IndexOfByKey(Settings()->GetScreenResolution());
    Settings()->SetScreenResolution(Choices[(Index+1)%Choices.Num()]);Apply();
}
void FWKeepDisplay(){if(Deadline>0){Settings()->ConfirmVideoMode();Settings()->SaveSettings();Deadline=0;}}
void FWRevertDisplay()
{
    if(Deadline<=0)return;
    Settings()->SetScreenResolution(PreviousSize);Settings()->SetFullscreenMode(PreviousMode);Apply();
    Settings()->ConfirmVideoMode();Settings()->SaveSettings();Deadline=0;
}
void FWUpdateDisplayPreview(){if(Deadline>0&&FPlatformTime::Seconds()>=Deadline)FWRevertDisplay();}
int FWDisplaySecondsRemaining(){return Deadline>0?FMath::Max(1,FMath::CeilToInt(Deadline-FPlatformTime::Seconds())):0;}
FString FWDisplayModeName()
{
    const auto Mode=Settings()->GetFullscreenMode();return Mode==EWindowMode::Fullscreen?TEXT("Fullscreen"):Mode==EWindowMode::WindowedFullscreen?TEXT("Borderless"):TEXT("Windowed");
}
void FWCheckDisplaySettings(TFunctionRef<void(const FString&,bool)> Check)
{
    auto* S=Settings();const FIntPoint Original=S->GetScreenResolution();const auto Mode=S->GetFullscreenMode();
    S->SetFullscreenMode(EWindowMode::Windowed);S->SetScreenResolution(FIntPoint(1280,720));
    FWPreviewDisplayMode();Check(TEXT("Display mode enables fullscreen at the selected lower resolution"),S->GetFullscreenMode()==EWindowMode::Fullscreen&&S->GetScreenResolution()==FIntPoint(1280,720)&&Deadline>0);
    FWRevertDisplay();Check(TEXT("Rejecting a display change restores the prior window and resolution"),S->GetFullscreenMode()==EWindowMode::Windowed&&S->GetScreenResolution()==FIntPoint(1280,720)&&Deadline==0);
    FWPreviewDisplayMode();Deadline=FPlatformTime::Seconds()-1;FWUpdateDisplayPreview();
    Check(TEXT("An expired display preview reverts automatically while menus are paused"),Deadline==0&&S->GetFullscreenMode()==EWindowMode::Windowed);
    FWPreviewDisplayMode();FWKeepDisplay();
    Check(TEXT("Keeping fullscreen confirms the mode and lower resolution"),Deadline==0&&S->GetLastConfirmedFullscreenMode()==EWindowMode::Fullscreen&&S->GetLastConfirmedScreenResolution()==FIntPoint(1280,720));
    FWPreviewDisplayResolution();Check(TEXT("Changing resolution preserves fullscreen mode"),Deadline>0&&S->GetFullscreenMode()==EWindowMode::Fullscreen);FWRevertDisplay();
    FWPreviewDisplayMode();Check(TEXT("Fullscreen can switch to borderless"),S->GetFullscreenMode()==EWindowMode::WindowedFullscreen);FWKeepDisplay();
    const auto BorderlessSize=S->GetScreenResolution();FWPreviewDisplayResolution();Check(TEXT("Borderless does not pretend to apply a lower display resolution"),Deadline==0&&S->GetScreenResolution()==BorderlessSize);
    S->SetScreenResolution(Original);S->SetFullscreenMode(Mode);Apply();S->ConfirmVideoMode();S->SaveSettings();
}
void FWRunDisplayTests()
{
    TArray<TSharedPtr<FJsonValue>> Checks;bool Passed=true;
    FWCheckDisplaySettings([&](const FString& Name,bool OK){auto C=MakeShared<FJsonObject>();C->SetStringField(TEXT("name"),Name);C->SetBoolField(TEXT("passed"),OK);Checks.Add(MakeShared<FJsonValueObject>(C));Passed&=OK;});
    auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("all_passed"),Passed);R->SetArrayField(TEXT("checks"),Checks);FString Text;
    FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectDir()/TEXT("Verification/display-tests.json")));FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
}
