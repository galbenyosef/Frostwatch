#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FWWeather.generated.h"

class ADirectionalLight;
class ASkyLight;
class AExponentialHeightFog;
class UStaticMeshComponent;
class UParticleSystemComponent;
class UMaterialInstanceDynamic;
class ULightComponent;

/** Owns the visible weather; its time always comes from the saved campaign clock. */
UCLASS()
class FROSTWATCH_API AFWWeather : public AActor
{
    GENERATED_BODY()
public:
    AFWWeather();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> SkyDome;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UParticleSystemComponent> Snowfall;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UParticleSystemComponent> Blizzard;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<ADirectionalLight> Sun;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<ASkyLight> SkyLight;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<AExponentialHeightFog> Fog;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) float CloudCover=0.25f;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) float SnowStrength=0;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool Sheltered=false;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) float CurrentHour=9;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> SkyMaterial;
    static FVector WeatherAt(float AbsoluteHours);
    static float ColdAt(const FVector& Location);
private:
    struct FCastleLamp
    {
        TWeakObjectPtr<ULightComponent> Light;
        float NightPower=0;
        bool Interior=false;
    };
    TArray<FCastleLamp> CastleLamps;
    float ShelterTimer=0;
    float CaptureHour=-1,CaptureWeather=-1;
};
