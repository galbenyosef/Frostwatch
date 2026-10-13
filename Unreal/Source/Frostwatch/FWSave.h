#pragma once
#include "CoreMinimal.h"
class UFWExpedition;

// Injectable storage keeps failure tests away from the player's real slots.
namespace FWSave
{
    struct FStore
    {
        TFunction<bool(const FString&)> Exists;
        TFunction<bool(const FString&,TArray<uint8>&)> Read;
        TFunction<bool(const FString&,const TArray<uint8>&)> Write;
    };
    const FStore& Disk();
    FString Slot();
    bool Exists(const FString& Name,const FStore& Store=Disk());
    UFWExpedition* Load(const FString& Name,bool& Recovered,const FStore& Store=Disk());
    bool Write(UFWExpedition* Save,const FString& Name,const FStore& Store=Disk());
    bool Preserve(const FString& Name,const FString& Backup,const FStore& Store=Disk());
    void Test(TFunctionRef<void(const FString&,bool)> Check);
}
