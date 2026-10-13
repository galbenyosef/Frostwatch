#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FWHorse.generated.h"
class AFWCharacter;
class USkeletalMeshComponent;
class USkeletalMesh;
class UAnimSequence;
class UStaticMesh;
class USceneComponent;
UCLASS()
class FROSTWATCH_API AFWHorse : public AActor
{
    GENERATED_BODY()
public:
    AFWHorse();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FString Identity;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) bool Cart=false;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<USkeletalMeshComponent> Body;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TArray<TObjectPtr<USkeletalMesh>> Parts;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UAnimSequence> Idle;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UAnimSequence> Walk;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UAnimSequence> Gallop;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UStaticMesh> CartWheelMesh;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FRotator CartWheelRotation=FRotator::ZeroRotator;
    UPROPERTY() TObjectPtr<AFWCharacter> Rider;
    float Energy=100;
    FTransform StartTransform;
    bool Mount(AFWCharacter* P);
    bool Dismount(bool Force=false);
    void Restore(AFWCharacter* P,bool Fresh=false);
private:
    UAnimSequence* CurrentClip=nullptr;
    UPROPERTY() TArray<TObjectPtr<USceneComponent>> WheelPivots;
    FVector PreviousWheelPosition=FVector::ZeroVector;
    float WheelAngle=0;
};
