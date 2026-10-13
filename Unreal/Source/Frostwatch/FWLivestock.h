#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FWLivestock.generated.h"
class UCapsuleComponent;
class USkeletalMeshComponent;
class UAnimSequence;

/** Ambient farm animals: bounded, grounded movement with genuine skeletal clips. */
UCLASS()
class FROSTWATCH_API AFWLivestock : public AActor
{
    GENERATED_BODY()
public:
    AFWLivestock();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UCapsuleComponent> Collision;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<USkeletalMeshComponent> Body;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FString Species;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FString Village;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UAnimSequence> Idle;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UAnimSequence> Walk;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UAnimSequence> Feed;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float WalkSpeed=65;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float WalkRate=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float FeedRate=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float RoamRadius=280;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FString Activity;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 TendingVisits=0;
    void Tend(float Dt);
private:
    FVector Home,Goal;
    float Pause=0;
    float TendedUntil=0;
    bool Walking=false;
    UPROPERTY() TObjectPtr<UAnimSequence> Current;
    void Play(UAnimSequence* Clip,float Rate=1);
    bool Ground(FVector& Position) const;
};
