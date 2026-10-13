#pragma once
#include "CoreMinimal.h"

// Plain runtime state: encounter resets restore injuries; no campaign save schema change.
enum class EFWStrike : uint8 { Cut, Heavy, LeftCut, RightCut, Thrust, Rising, LowCut, Bash, Finisher };
struct FFWBloodDrop
{
    FVector Position=FVector::ZeroVector, Velocity=FVector::ZeroVector;
    float Age=0, Radius=.7f;
};
class AFWCharacter;
class AFWEnemy;
EFWStrike FWChooseStrike(bool Heavy,float Forward,float Strafe,bool Crouched,bool Guard,int Chain);
const TCHAR* FWStrikeName(EFWStrike Move);
int FWSelectHitPart(const AFWEnemy* Enemy,const FVector& Point);
float FWInjuryAttackScale(const AFWEnemy* Enemy);
void FWUpdateBlood(AFWEnemy* Enemy,float Dt);
void FWEmitBlood(AFWEnemy* Enemy,const FVector& Point,const FVector& Direction,int Count);
