#pragma once
#include "CoreMinimal.h"
class AFWCharacter;
class AFWEnemy;
namespace FWStory
{
    FString Line(const FString& Key);
    FString Rowan(const AFWCharacter* Player);
    FString Citizen(const AFWCharacter* Player);
    FString Ending(const AFWCharacter* Player);
    void EnemyBark(AFWCharacter* Player,AFWEnemy* Enemy);
}
