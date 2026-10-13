#pragma once
#include "CoreMinimal.h"
class AFWCharacter;
namespace FWCavePassages
{
    bool Use(AFWCharacter* Player,const FString& Identity);
    void Test(AFWCharacter* Player,TFunctionRef<void(const FString&,bool)> Check);
    void WalkReview(AFWCharacter* Player);
}
