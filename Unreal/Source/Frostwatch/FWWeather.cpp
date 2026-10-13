#include "FWWeather.h"
#include "FWGame.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PointLight.h"
#include "Engine/Light.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

AFWWeather::AFWWeather()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=.1f;
    SkyDome=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LivingSky"));SetRootComponent(SkyDome);
    SkyDome->SetCollisionEnabled(ECollisionEnabled::NoCollision);SkyDome->CastShadow=false;
    SkyDome->SetMobility(EComponentMobility::Movable);
    Snowfall=CreateDefaultSubobject<UParticleSystemComponent>(TEXT("Snowfall"));Snowfall->SetupAttachment(RootComponent);Snowfall->bAutoActivate=false;
    Blizzard=CreateDefaultSubobject<UParticleSystemComponent>(TEXT("Blizzard"));Blizzard->SetupAttachment(RootComponent);Blizzard->bAutoActivate=false;
    // Particle authoring uses centimetres. Never inherit the enormous sky-dome scale.
    Snowfall->SetAbsolute(false,false,true);Blizzard->SetAbsolute(false,false,true);
}
FVector AFWWeather::WeatherAt(float Hours)
{
    // A reproducible front progresses clear -> flurries -> snow -> clearing every six game hours.
    // Smooth interpolation prevents a hard lighting switch and needs no new save format.
    const FVector Fronts[]={FVector(.22f,0,0),FVector(.50f,.25f,0),FVector(.82f,.8f,.25f),FVector(.97f,1,1),FVector(.62f,.4f,.1f),FVector(.28f,0,0)};
    const float Phase=FMath::Fmod(FMath::Max(0.f,Hours)+3,6.f);
    const int32 Index=FMath::FloorToInt(Phase);const float T=FMath::Frac(Phase);const float Ease=T*T*(3-2*T);
    return FMath::Lerp(Fronts[Index],Fronts[(Index+1)%6],Ease);
}
void AFWWeather::BeginPlay()
{
    Super::BeginPlay();SkyMaterial=SkyDome->CreateDynamicMaterialInstance(0);
    for(TActorIterator<ALight> A(GetWorld());A;++A)
        if(A->ActorHasTag(TEXT("CastleLighting"))||A->ActorHasTag(TEXT("CastleAtmosphere")))
        {
            auto* L=A->GetLightComponent();if(!L)continue;
            const FVector At=A->GetActorLocation();
            const bool Prison=At.X>1900&&At.X<2500&&At.Y>2000&&At.Y<3400;
            const bool Point=Cast<UPointLightComponent>(L)&&!L->IsA(USpotLightComponent::StaticClass());
            const bool Interior=(At.X>=3000&&FMath::Abs(At.Y)<1500)||(Prison&&Point);
            const float Power=Prison?(Point?220.f:180.f):L->Intensity;
            if(Prison&&Point)Cast<UPointLightComponent>(L)->SetAttenuationRadius(650);
            CastleLamps.Add({L,Power,Interior});
        }
    if(FParse::Param(FCommandLine::Get(),TEXT("FrostwatchCapture")))
    {
        FParse::Value(FCommandLine::Get(),TEXT("CaptureHour="),CaptureHour);
        FParse::Value(FCommandLine::Get(),TEXT("CaptureWeather="),CaptureWeather);
    }
    Tick(.1f);
}
float AFWWeather::ColdAt(const FVector& Location)
{
    const float North=FMath::SmoothStep(-35000.f,-4000.f,float(Location.X));
    const FVector2D Ash((Location.X-39600)/17000,(Location.Y+46000)/17000);
    const float Volcanic=FMath::Exp(-Ash.SizeSquared()*2.f);
    return North*(1.f-Volcanic);
}
void AFWWeather::Tick(float Dt)
{
    Super::Tick(Dt);
    auto* P=Cast<AFWCharacter>(UGameplayStatics::GetPlayerPawn(this,0));if(!P||!P->Expedition)return;
    CurrentHour=CaptureHour>=0?FMath::Fmod(CaptureHour,24.f):P->Expedition->Hour;
    FVector W=WeatherAt((P->Expedition->Day-1)*24.f+CurrentHour);
    const float Cold=ColdAt(P->GetActorLocation());
    W.Y*=Cold;W.Z*=Cold*Cold;
    if(CaptureWeather>=0)W=CaptureWeather<.5f?FVector(.22f,0,0):CaptureWeather<1.5f?FVector(.62f,.5f,0):FVector(.97f,1,1);
    CloudCover=W.X;SnowStrength=W.Y;
    const float Altitude=FMath::Sin((CurrentHour-6)/12*PI);
    const float Day=FMath::SmoothStep(-.12f,.16f,Altitude);
    const float Dusk=(1-FMath::SmoothStep(0.f,.4f,FMath::Abs(Altitude)))*Day;
    for(const auto& Lamp:CastleLamps)if(auto* Light=Lamp.Light.Get())
        Light->SetIntensity(Lamp.NightPower*(Lamp.Interior?1.f:1.f-Day));
    if(Sun)
    {
        // A dim, cool moonlight takes over after sunset, keeping night travel readable.
        Sun->SetActorRotation(FRotator(-FMath::Max(5.f,FMath::Abs(Altitude)*65),-25+(CurrentHour-9)*12,0));
        auto* Light=Cast<UDirectionalLightComponent>(Sun->GetLightComponent());
        Light->SetIntensity(FMath::Lerp(1.25f,.8f+FMath::Max(0.f,Altitude)*4.8f,Day)*(1-W.Z*.65f));
        const FLinearColor Warm=FMath::Lerp(FLinearColor(1,.93f,.82f),FLinearColor(1,.48f,.22f),Dusk);
        Light->SetLightColor(FMath::Lerp(FLinearColor(.55f,.67f,.90f),Warm,Day));
    }
    if(SkyLight)SkyLight->GetLightComponent()->SetIntensity(FMath::Lerp(.72f,.85f,Day)*(1-.35f*W.Z));
    for(TActorIterator<APointLight> It(GetWorld());It;++It)
        if(It->ActorHasTag(TEXT("FountainLantern"))||It->ActorHasTag(TEXT("RoadLantern")))
        {
            auto* Light=It->GetLightComponent();Light->SetIntensity(FMath::Lerp(900.f,0.f,Day));
            // Retain illumination on Performance without dozens of six-face shadow maps.
            // The directional sun/moon still casts shadows; higher presets keep lamp shadows.
            Light->SetCastShadows(GEngine->GetGameUserSettings()->GetGlobalIlluminationQuality()>0);
        }
    if(Fog)
    {
        auto* F=Fog->GetComponent();F->SetFogDensity(FMath::Lerp(.0035f,.022f,W.Z)+W.Y*.003f);
        F->SetFogInscatteringColor(FMath::Lerp(FLinearColor(.025f,.045f,.09f),FLinearColor(.43f,.52f,.64f),Day));
        F->SetStartDistance(FMath::Lerp(7000.f,800.f,W.Z));
    }
    if(SkyMaterial)
    {
        SkyMaterial->SetScalarParameterValue(TEXT("Daylight"),Day);
        SkyMaterial->SetScalarParameterValue(TEXT("Dusk"),Dusk);
        SkyMaterial->SetScalarParameterValue(TEXT("CloudCover"),CloudCover);
        SkyMaterial->SetScalarParameterValue(TEXT("Storm"),W.Z);
        SkyMaterial->SetScalarParameterValue(TEXT("Hour"),CurrentHour);
    }
    const FVector Location=P->GetActorLocation();SetActorLocation(Location);
    Snowfall->SetWorldLocation(Location+FVector(0,0,450));Blizzard->SetWorldLocation(Location+FVector(0,0,450));
    ShelterTimer-=Dt;
    if(ShelterTimer<=0)
    {
        ShelterTimer=.5f;FHitResult Hit;FCollisionQueryParams Q;Q.AddIgnoredActor(P);Q.AddIgnoredActor(this);
        Sheltered=GetWorld()->LineTraceSingleByChannel(Hit,Location,Location+FVector(0,0,12000),ECC_Visibility,Q);
    }
    const bool Flurries=!Sheltered&&W.Y>.08f&&W.Z<.7f;
    const bool StormSnow=!Sheltered&&W.Z>=.7f;
    if(Flurries!=Snowfall->IsActive()){if(Flurries)Snowfall->ActivateSystem();else Snowfall->DeactivateSystem();}
    if(StormSnow!=Blizzard->IsActive()){if(StormSnow)Blizzard->ActivateSystem();else Blizzard->DeactivateSystem();}
    Snowfall->SetVisibility(!Sheltered);Blizzard->SetVisibility(!Sheltered);
    Snowfall->SetFloatParameter(TEXT("SpawnRate"),FMath::Lerp(50.f,350.f,W.Y));
}
