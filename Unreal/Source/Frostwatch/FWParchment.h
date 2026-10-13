#pragma once
#include "CoreMinimal.h"
class AFWHUD;
class AFWCharacter;
class UCanvas;
namespace FWParchment
{
    bool Draw(AFWHUD* HUD,AFWCharacter* Player,UCanvas* Canvas);
    bool Action(AFWCharacter* Player,const FString& Action);
}
