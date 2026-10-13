#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
class AFWCharacter;
class AFWInteractable;
namespace FWCampaign
{
    const TSharedPtr<FJsonObject>& Data();
    FVector Position(const TSharedPtr<FJsonObject>& Row);
    bool Complete(const AFWCharacter* P,const FString& Token);
    FString Objective(const AFWCharacter* P);
    FVector Target(const AFWCharacter* P);
    bool InCombat(const AFWCharacter* P);
    bool Interact(AFWCharacter* P,const FString& Identity);
    bool Travel(AFWCharacter* P,const FString& Camp);
    void Update(AFWCharacter* P,float Dt);
    void Test(AFWCharacter* P);
    void WorldChecks(AFWCharacter* P,TFunctionRef<void(const FString&,bool)> Check);
}
