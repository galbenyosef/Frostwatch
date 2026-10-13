#include "FWSave.h"
#include "FWGame.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace FWSave
{
    FString Slot(){return TEXT("FrostwatchUnreal_Expedition_v1");}
    const FStore& Disk()
    {
        static const FStore Store={
            [](const FString& N){return UGameplayStatics::DoesSaveGameExist(N,0);},
            [](const FString& N,TArray<uint8>& B){return UGameplayStatics::LoadDataFromSlot(B,N,0);},
            [](const FString& N,const TArray<uint8>& B){return UGameplayStatics::SaveDataToSlot(B,N,0);}};
        return Store;
    }
    static UFWExpedition* Decode(const TArray<uint8>& B)
    {
        if(B.IsEmpty())return nullptr;
        auto* S=Cast<UFWExpedition>(UGameplayStatics::LoadGameFromMemory(B));
        return S&&S->Version==1&&!S->Checkpoint.ContainsNaN()?S:nullptr;
    }
    static bool ReadValid(const FString& N,TArray<uint8>& B,const FStore& Store)
    {return Store.Read(N,B)&&Decode(B)!=nullptr;}
    static bool VerifiedWrite(const FString& N,const TArray<uint8>& B,const FStore& Store)
    {
        if(!Store.Write(N,B))return false;
        TArray<uint8> Readback;
        return Store.Read(N,Readback)&&Readback==B;
    }
    bool Exists(const FString& N,const FStore& Store)
    {return Store.Exists(N)||Store.Exists(N+TEXT("_Previous"));}
    UFWExpedition* Load(const FString& N,bool& Recovered,const FStore& Store)
    {
        Recovered=false;TArray<uint8> Bytes;
        if(ReadValid(N,Bytes,Store))return Decode(Bytes);
        Bytes.Reset();
        if(ReadValid(N+TEXT("_Previous"),Bytes,Store)){Recovered=true;return Decode(Bytes);}
        return nullptr;
    }
    bool Write(UFWExpedition* S,const FString& N,const FStore& Store)
    {
        TArray<uint8> Bytes;
        if(!S||!UGameplayStatics::SaveGameToMemory(S,Bytes)||!Decode(Bytes))return false;
        TArray<uint8> Previous;
        // Do not overwrite a good recovery copy with an unreadable primary.
        if(ReadValid(N,Previous,Store)&&!VerifiedWrite(N+TEXT("_Previous"),Previous,Store))return false;
        // Establish a recovery copy on the first save too. Keep it if the primary write fails.
        if(!ReadValid(N,Previous,Store)&&!ReadValid(N+TEXT("_Previous"),Previous,Store))
            if(!VerifiedWrite(N+TEXT("_Previous"),Bytes,Store))return false;
        return VerifiedWrite(N,Bytes,Store);
    }
    bool Preserve(const FString& N,const FString& Backup,const FStore& Store)
    {
        if(!Exists(N,Store))return true;
        bool Recovered=false;auto* S=Load(N,Recovered,Store);
        TArray<uint8> Bytes;
        return S&&UGameplayStatics::SaveGameToMemory(S,Bytes)&&VerifiedWrite(Backup,Bytes,Store);
    }

    void Test(TFunctionRef<void(const FString&,bool)> Check)
    {
        TMap<FString,TArray<uint8>> Files;
        FString FailWrite,FailRead,CorruptWrite;
        const FStore Store={
            [&](const FString& N){return Files.Contains(N);},
            [&](const FString& N,TArray<uint8>& B){if(N==FailRead||!Files.Contains(N))return false;B=Files[N];return true;},
            [&](const FString& N,const TArray<uint8>& B){if(N==FailWrite)return false;Files.Add(N,N==CorruptWrite?TArray<uint8>():B);return true;}};
        const FString N=TEXT("IsolatedTest"),Previous=N+TEXT("_Previous");
        bool Recovered=false;
        auto* S=NewObject<UFWExpedition>();S->Silver=80;S->Flags.Add(TEXT("supplies_done"));
        Check(TEXT("Missing save returns no expedition"),!Load(N,Recovered,Store)&&!Recovered);
        Check(TEXT("First save is verified"),Write(S,N,Store));
        Check(TEXT("First save establishes recovery copy"),Files.Contains(Previous));
        S->Silver=120;S->Defeated.Add(TEXT("boss_test"));S->RespawnAtHour.Add(TEXT("guard_test"),42);
        Check(TEXT("Updated save is verified"),Write(S,N,Store));
        auto* Loaded=Load(N,Recovered,Store);
        Check(TEXT("Primary retains progress and respawn state"),Loaded&&!Recovered&&Loaded->Silver==120&&Loaded->Flags.Contains(TEXT("supplies_done"))&&Loaded->Defeated.Contains(TEXT("boss_test"))&&Loaded->RespawnAtHour.FindRef(TEXT("guard_test"))==42);
        Check(TEXT("Previous generation retains earlier progress"),Decode(Files[Previous])->Silver==80);
        const auto GoodPrimary=Files[N];FailWrite=Previous;S->Silver=160;
        Check(TEXT("Backup failure aborts primary replacement"),!Write(S,N,Store)&&Files[N]==GoodPrimary);
        FailWrite=N;
        Check(TEXT("Primary write failure reports failure"),!Write(S,N,Store));
        Check(TEXT("Primary write failure retains prior progress"),Decode(Files[Previous])->Silver==120);
        FailWrite.Empty();CorruptWrite=N;
        Check(TEXT("Readback detects incomplete primary write"),!Write(S,N,Store));
        Loaded=Load(N,Recovered,Store);
        Check(TEXT("Corrupt primary recovers previous progress"),Loaded&&Recovered&&Loaded->Silver==120);
        CorruptWrite.Empty();
        Check(TEXT("Saving after recovery succeeds"),Write(S,N,Store));
        Check(TEXT("Recovery copy is not replaced by corrupt data"),Decode(Files[Previous])->Silver==120);
        FailRead=N;Loaded=Load(N,Recovered,Store);
        Check(TEXT("Unreadable primary recovers previous generation"),Loaded&&Recovered&&Loaded->Silver==120);
        FailRead.Empty();
        Check(TEXT("New-game preservation is verified"),Preserve(N,TEXT("BeforeNewGame"),Store)&&Decode(Files[TEXT("BeforeNewGame")])->Silver==160);
        FailWrite=TEXT("BeforeImport");
        Check(TEXT("Failed import preservation blocks replacement"),!Preserve(N,TEXT("BeforeImport"),Store));
        FailWrite.Empty();Files.Remove(N);
        Check(TEXT("Continue remains available with only recovery slot"),Exists(N,Store));
        Loaded=Load(N,Recovered,Store);Check(TEXT("Missing primary recovers"),Loaded&&Recovered);
        Files[Previous].Reset();
        Check(TEXT("Both corrupt slots fail without inventing progress"),!Load(N,Recovered,Store));
        Check(TEXT("Unreadable existing expedition cannot be silently preserved"),!Preserve(N,TEXT("BeforeNewGame"),Store));
        Files.Empty();FailWrite=Previous;
        Check(TEXT("First save failure leaves primary untouched"),!Write(S,N,Store)&&!Files.Contains(N));
        FailWrite.Empty();S->Version=99;
        Check(TEXT("Unsupported save version is rejected"),!Write(S,N,Store));
        // Exercise the real platform slot backend too, using unique diagnostic
        // names. Keep the resulting files as evidence, away from player slots.
        const FString NativeName=TEXT("Frostwatch_Diagnostic_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
        auto* NativeSave=NewObject<UFWExpedition>();NativeSave->Silver=137;
        NativeSave->Flags.Add(TEXT("diagnostic_only"));
        Check(TEXT("Native slot write and readback succeed"),Write(NativeSave,NativeName));
        auto* NativeLoaded=Load(NativeName,Recovered);
        Check(TEXT("Native slot roundtrip retains expedition data"),NativeLoaded&&!Recovered&&NativeLoaded->Silver==137&&NativeLoaded->Flags.Contains(TEXT("diagnostic_only")));
        const FString Evidence=FPaths::ProjectDir()/TEXT("Verification/SaveTestArtifacts");
        IFileManager::Get().MakeDirectory(*Evidence,true);bool Retained=true;
        for(const FString& Name:{NativeName,NativeName+TEXT("_Previous")})
        {
            const FString From=FPaths::ProjectSavedDir()/TEXT("SaveGames")/(Name+TEXT(".sav"));
            const FString To=Evidence/(Name+TEXT(".sav"));
            Retained=IFileManager::Get().Move(*To,*From,false)&&Retained;
        }
        Check(TEXT("Diagnostic slot files retained outside player save folder"),Retained);
    }
}
