#pragma once
#include "CoreMinimal.h"
class AFWCharacter;
class FJsonObject;
namespace FWConsequences
{
    void Update(AFWCharacter* Player,bool Restore=false);
    void Test(AFWCharacter* Player,TFunctionRef<void(const FString&,bool)> Check);
    void Describe(TSharedPtr<FJsonObject> Report);
}
