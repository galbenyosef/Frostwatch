#pragma once
#include "CoreMinimal.h"
class AFWCharacter;
class AFWEnemy;
class UWorld;
class USkeletalMesh;
enum class EFWCombatCue { Swing, Impact, Clash, Parry };
void FWLoadCombatAudio(AFWCharacter* Player);
void FWLoadDamageParts(AFWEnemy* Enemy);
void FWImpactMark(AFWEnemy* Enemy,const FVector& Point,const FVector& Direction);
void FWCombatAudio(AFWCharacter* Player, EFWCombatCue Cue, const FVector& Position);
bool FWSeverFinisher(AFWEnemy* Enemy,const FVector& HitPoint,const FVector& Direction,bool Execution,int PartOverride=INDEX_NONE);
void FWResetCombatDamage(AFWEnemy* Enemy);
FName FWSeverBone(USkeletalMesh* Mesh,int Part);
FString FWSeverAssetPath(USkeletalMesh* Mesh,int Part);
void FWAuthorCombatContact(UWorld* World);
void FWAuthorDamageParts(UWorld* World);
void FWRunCombatContactTests(AFWCharacter* Player);
void FWCaptureCombatDamage(AFWCharacter* Player);

void FWCaptureSliceCombat(AFWCharacter* Player);
