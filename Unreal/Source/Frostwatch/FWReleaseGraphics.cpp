#include "FWReleaseGraphics.h"
#include "FWGame.h"
#include "FWConsequences.h"
#include "DynamicRHI.h"
#include "RHIStats.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "ContentStreaming.h"
#include "HAL/PlatformMemory.h"
#include "FWWorld.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

void FWApplyGraphicsBudget(int Quality)
{
    Quality=FMath::Clamp(Quality,0,3);
    const int TextureBudgets[]={384,384,1536,2560};
    // Leave transient residency headroom on older cards while keeping material
    // mips and Lumen's verified surface-cache capacity. Measure moving routes.
    const int GeometryBudgets[]={32,32,256,512};
    auto Set=[](const TCHAR* Name,int Value){if(auto* C=IConsoleManager::Get().FindConsoleVariable(Name))C->Set(Value,ECVF_SetByCode);};
    Set(TEXT("r.Streaming.PoolSize"),TextureBudgets[Quality]);
    // Bound concurrent texture replacement allocations while streaming new scenery.
    // Smaller batches leave headroom for action/capture transients on 3 GB cards.
    // This changes in-flight replacement memory, not the texture quality or pool.
    Set(TEXT("r.Streaming.MaxTempMemoryAllowed"),Quality<=1?8:64);
    Set(TEXT("r.Nanite.Streaming.StreamingPoolSize"),GeometryBudgets[Quality]);
    Set(TEXT("r.RenderTargetPoolMin"),Quality<=1?64:400);
    // Balanced retains Lumen; conventional shadows leave more time and memory
    // for near-field materials on older discrete GPUs. High keeps virtual shadows.
    // Keep the irradiance probe count; reduce angular resolution, not spatial coverage.
    // 1024 oversubscribed the castle courtyard. 1152 retains capacity in the
    // verified courtyard while leaving more streaming headroom than 1280 in
    // village street views on 3 GB cards. Material texture budgets stay intact.
    Set(TEXT("r.LumenScene.SurfaceCache.AtlasSize"),Quality==1?1152:Quality==2?3584:Quality==3?4096:2048);
    Set(TEXT("r.LumenScene.GlobalSDF.Resolution"),Quality==1?192:252);
    Set(TEXT("r.AOGlobalDistanceFieldCacheMostlyStaticSeparately"),Quality==1?0:1);
    Set(TEXT("r.Lumen.IrradianceFieldGather.ProbeResolution"),Quality==1?8:16);
    Set(TEXT("r.Lumen.IrradianceFieldGather.OcclusionProbeResolution"),Quality==1?8:16);
    Set(TEXT("r.Lumen.ScreenProbeGather.RadianceCache.ProbeResolution"),Quality==1||Quality==2?16:32);
    Set(TEXT("r.Shadow.Virtual.Enable"),Quality>=2?1:0);
    Set(TEXT("r.ShadowQuality"),Quality>=2?5:3);
    Set(TEXT("r.AntiAliasingMethod"),Quality<=1?2:4);
    Set(TEXT("r.PostProcessAAQuality"),Quality==0?3:4);
    Set(TEXT("r.TemporalAA.Upsampling"),Quality<=1?0:1);
    // Keep wind animation, but avoid its extra deformation-velocity draw pass on lower presets.
    // This CVar rebuilds render states: never set it again when the value is unchanged.
    if(auto* C=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Velocity.EnableVertexDeformation")))
        if(C->GetInt()!=(Quality<=1?0:2))C->Set(Quality<=1?0:2,ECVF_SetByCode);
    if(auto* C=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.DistanceScale")))C->Set(Quality==0?.65f:1.f,ECVF_SetByCode);
    // Keep nearby foliage animated. Distant wind deformation also invalidates
    // shadow pages, although its movement is barely visible at this distance.
    if(GEngine)for(const FWorldContext& Context:GEngine->GetWorldContexts())
        if(UWorld* World=Context.World();World&&World->IsGameWorld())
            for(TActorIterator<AFWForest> It(World);It;++It)
                It->Instances->SetWorldPositionOffsetDisableDistance(Quality<=1?8000:0);
    // Override the renderer budget without silently changing the saved scalability preset.
    // r.ShadowQuality above retains conventional sun/moon shadows on Performance.
}

namespace { TArray<double> FrameTimes,DemotedFrames,GpuTimes,SessionFrames; double PreviousFrame=0,FirstTrackedFrame=0,SessionDemotedPeak=0; }
void FWTrackCaptureFrame()
{
    const double Now=FPlatformTime::Seconds();
    if(FirstTrackedFrame==0)FirstTrackedFrame=Now;
    // Retain the complete post-warmup interval, so a smooth final camera cannot
    // hide earlier traversal stalls. Bounded to 120,000 samples (~1 MiB).
    if(PreviousFrame>0&&Now-FirstTrackedFrame>=8&&SessionFrames.Num()<120000)
    {
        SessionFrames.Add((Now-PreviousFrame)*1000);
        SessionDemotedPeak=FMath::Max(SessionDemotedPeak,GDemotedLocalMemorySize/1048576.0);
    }
    if(PreviousFrame>0) { FrameTimes.Add((Now-PreviousFrame)*1000);DemotedFrames.Add(GDemotedLocalMemorySize/1048576.0);GpuTimes.Add(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles())); if(FrameTimes.Num()>1800){FrameTimes.RemoveAt(0);DemotedFrames.RemoveAt(0);GpuTimes.RemoveAt(0);} }
    PreviousFrame=Now;
}
void FWRecordCaptureGraphics(AFWCharacter* Player)
{
    FRHIMemoryStats Memory; FTextureMemoryStats Textures;
    RHIGetMemoryStats(Memory); RHIGetTextureMemoryStats(Textures);
    auto Report=MakeShared<FJsonObject>();
    FWConsequences::Describe(Report);
    Report->SetNumberField(TEXT("player_health"),Player->Health);
    Report->SetStringField(TEXT("screen"),Player->Screen);
    Report->SetBoolField(TEXT("benchmark_invulnerable"),FParse::Param(FCommandLine::Get(),TEXT("CaptureBenchmark")));
    const auto SystemMemory=FPlatformMemory::GetStats();
    Report->SetNumberField(TEXT("system_ram_available_mib"),SystemMemory.AvailablePhysical/1048576.0);
    Report->SetNumberField(TEXT("process_ram_used_mib"),SystemMemory.UsedPhysical/1048576.0);
    for(const TCHAR* Name:{TEXT("s.IoDispatcherCacheSizeMB"),TEXT("s.IoDispatcherBufferMemoryMB")})
        if(auto* C=IConsoleManager::Get().FindConsoleVariable(Name))Report->SetNumberField(Name,C->GetInt());
    Report->SetBoolField(TEXT("memory_stats_valid"),Memory.IsValid());
    double RecentDemoted=GDemotedLocalMemorySize/1048576.0;
    for(int I=FMath::Max(0,DemotedFrames.Num()-600);I<DemotedFrames.Num();I++)RecentDemoted=FMath::Max(RecentDemoted,DemotedFrames[I]);
    Report->SetBoolField(TEXT("over_budget"),Memory.IsOverBudget()||RecentDemoted>0);
    Report->SetNumberField(TEXT("recent_engine_demoted_mib"),RecentDemoted);
    Report->SetNumberField(TEXT("gpu_budget_mib"),Memory.BudgetLocal/1048576.0);
    Report->SetNumberField(TEXT("gpu_used_mib"),Memory.UsedLocal/1048576.0);
    Report->SetNumberField(TEXT("gpu_demoted_mib"),Memory.DemotedLocal/1048576.0);
    Report->SetNumberField(TEXT("streaming_textures_mib"),Textures.StreamingMemorySize/1048576.0);
    Report->SetNumberField(TEXT("nonstreaming_textures_mib"),Textures.NonStreamingMemorySize/1048576.0);
    if(IStreamingManager::Get().IsTextureStreamingEnabled())
    {
        auto& Streamer=IStreamingManager::Get().GetRenderAssetStreamingManager();
        Report->SetNumberField(TEXT("texture_pool_mib"),Streamer.GetPoolSize()/1048576.0);
        Report->SetNumberField(TEXT("texture_pool_required_mib"),Streamer.GetRequiredPoolSize()/1048576.0);
        Report->SetNumberField(TEXT("texture_pool_over_budget_mib"),Streamer.GetMemoryOverBudget()/1048576.0);
        Report->SetBoolField(TEXT("over_budget"),Report->GetBoolField(TEXT("over_budget"))||Streamer.GetMemoryOverBudget()>0);
    }
    Report->SetStringField(TEXT("player_position"),Player->GetActorLocation().ToString());
    Report->SetBoolField(TEXT("live_world"),FParse::Param(FCommandLine::Get(),TEXT("CaptureLiveWorld")));
    if(!SessionFrames.IsEmpty())
    {
        auto Sorted=SessionFrames;Sorted.Sort();double Sum=0;int32 Slow50=0,Slow100=0;
        for(double Ms:Sorted){Sum+=Ms;Slow50+=Ms>50;Slow100+=Ms>100;}
        auto Session=MakeShared<FJsonObject>();
        Session->SetNumberField(TEXT("warmup_seconds"),8);Session->SetNumberField(TEXT("frames"),Sorted.Num());Session->SetNumberField(TEXT("duration_seconds"),Sum/1000);
        Session->SetNumberField(TEXT("mean_frame_ms"),Sum/Sorted.Num());Session->SetNumberField(TEXT("p95_frame_ms"),Sorted[FMath::Min(Sorted.Num()-1,FMath::FloorToInt(Sorted.Num()*.95))]);
        Session->SetNumberField(TEXT("p99_frame_ms"),Sorted[FMath::Min(Sorted.Num()-1,FMath::FloorToInt(Sorted.Num()*.99))]);Session->SetNumberField(TEXT("worst_frame_ms"),Sorted.Last());
        Session->SetNumberField(TEXT("frames_over_50ms"),Slow50);Session->SetNumberField(TEXT("frames_over_100ms"),Slow100);Session->SetNumberField(TEXT("peak_demoted_mib"),SessionDemotedPeak);
        Session->SetBoolField(TEXT("sample_limit_reached"),SessionFrames.Num()==120000);Report->SetObjectField(TEXT("whole_capture"),Session);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("CaptureWaterfallAudit"))||FVector::DistSquared(Player->GetActorLocation(),FVector(14000,35000,4000))<FMath::Square(15000.f))
    {
        TArray<TSharedPtr<FJsonValue>> Rocks;
        for(TActorIterator<AActor> It(Player->GetWorld());It;++It)
        {
            TArray<UStaticMeshComponent*> Components;It->GetComponents(Components);
            for(auto* C:Components)if(C->GetStaticMesh()&&FVector::DistSquared(C->Bounds.Origin,FVector(14000,35000,4000))<FMath::Square(10000.f))
            {
                auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("actor"),It->GetName());R->SetStringField(TEXT("mesh"),C->GetStaticMesh()->GetPathName());
                R->SetStringField(TEXT("bounds"),C->Bounds.Origin.ToString());R->SetBoolField(TEXT("actor_hidden"),It->IsHidden());R->SetBoolField(TEXT("component_visible"),C->IsVisible());
                R->SetBoolField(TEXT("reverse_culling"),C->IsReverseCulling());R->SetStringField(TEXT("scale"),C->GetComponentScale().ToString());
                R->SetNumberField(TEXT("draw_distance"),C->LDMaxDrawDistance);
                TArray<TSharedPtr<FJsonValue>> Materials;for(auto* M:C->GetMaterials())Materials.Add(MakeShared<FJsonValueString>(M?M->GetPathName():TEXT("none")));
                R->SetArrayField(TEXT("materials"),Materials);Rocks.Add(MakeShared<FJsonValueObject>(R));
            }
        }
        Report->SetArrayField(TEXT("waterfall_meshes"),Rocks);
    }
    if(GEngine->GameViewport&&GEngine->GameViewport->Viewport){const FIntPoint Size=GEngine->GameViewport->Viewport->GetSizeXY();Report->SetNumberField(TEXT("viewport_width"),Size.X);Report->SetNumberField(TEXT("viewport_height"),Size.Y);}
    Report->SetNumberField(TEXT("display_mode"),int(GEngine->GetGameUserSettings()->GetFullscreenMode()));
    // Discard startup frames; report wall-clock frame intervals, not simulated delta time.
    const int32 Count=FMath::Min(600,FrameTimes.Num()); TArray<double> Recent;
    for(int32 I=FrameTimes.Num()-Count;I<FrameTimes.Num();I++)Recent.Add(FrameTimes[I]);
    if(Count>0){double Sum=0;for(double T:Recent)Sum+=T;Recent.Sort();Report->SetNumberField(TEXT("sample_frames"),Count);Report->SetNumberField(TEXT("mean_frame_ms"),Sum/Count);Report->SetNumberField(TEXT("p95_frame_ms"),Recent[FMath::Min(Count-1,FMath::FloorToInt(Count*.95))]);}
    double GpuSum=0;int GpuCount=0;for(int I=FMath::Max(0,GpuTimes.Num()-600);I<GpuTimes.Num();I++)if(GpuTimes[I]>0){GpuSum+=GpuTimes[I];GpuCount++;}
    if(GpuCount)Report->SetNumberField(TEXT("mean_gpu_ms"),GpuSum/GpuCount);
    for(const TCHAR* Name:{TEXT("sg.GlobalIlluminationQuality"),TEXT("sg.TextureQuality"),TEXT("sg.ShadowQuality"),TEXT("r.Lumen.FinalGatherMethod"),TEXT("r.Lumen.DiffuseIndirect.Allow"),TEXT("r.Streaming.PoolSize"),TEXT("r.Shadow.Virtual.Enable"),TEXT("r.ScreenPercentage"),TEXT("r.AntiAliasingMethod"),TEXT("r.Shadow.DistanceScale"),TEXT("r.TemporalAA.Upsampling"),TEXT("r.Velocity.EnableVertexDeformation"),TEXT("r.Streaming.MaxTempMemoryAllowed"),TEXT("r.Nanite.Streaming.StreamingPoolSize"),TEXT("r.RenderTargetPoolMin"),TEXT("r.LumenScene.SurfaceCache.AtlasSize"),TEXT("r.LumenScene.GlobalSDF.Resolution"),TEXT("r.AOGlobalDistanceFieldCacheMostlyStaticSeparately"),TEXT("r.Lumen.IrradianceFieldGather.ProbeResolution"),TEXT("r.Lumen.IrradianceFieldGather.OcclusionProbeResolution"),TEXT("r.Shadow.Virtual.NonNanite.IncludeInCoarsePages"),TEXT("r.Shadow.Virtual.ResolutionLodBiasDirectional")})
        if(auto* C=IConsoleManager::Get().FindConsoleVariable(Name))Report->SetNumberField(Name,C->GetFloat());
    FString Text;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Text));
    FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectDir()/TEXT("Verification/capture-graphics.json")));
    if(GEngine)GEngine->Exec(Player->GetWorld(),TEXT("ListTextures"));
    if(GEngine&&FParse::Param(FCommandLine::Get(),TEXT("CaptureProfile"))){if(auto* C=IConsoleManager::Get().FindConsoleVariable(TEXT("r.ProfileGPU.ShowUI")))C->Set(0);GEngine->Exec(Player->GetWorld(),TEXT("ProfileGPU"));}
}

void FWRunPerformanceSweep(AFWCharacter* Player,double RealTime)
{
    struct FPhase {const TCHAR* Name;int Scale;int WindVelocity;int TemporalUpsample;};
    static const FPhase Phases[]={
        {TEXT("100-original"),100,2,1},{TEXT("100-no-wind-vectors"),100,0,1},
        {TEXT("85-temporal"),85,0,1},{TEXT("85-spatial"),85,0,0},
        {TEXT("70-spatial"),70,0,0},{TEXT("100-repeat"),100,0,0}};
    static int Phase=-1;static double Began=0;static bool Cleared=false;
    auto Apply=[&]()
    {
        auto Set=[](const TCHAR* Name,int Value){if(auto* C=IConsoleManager::Get().FindConsoleVariable(Name))if(C->GetInt()!=Value)C->Set(Value,ECVF_SetByConsole);};
        Set(TEXT("r.ScreenPercentage"),Phases[Phase].Scale);
        Set(TEXT("r.Velocity.EnableVertexDeformation"),Phases[Phase].WindVelocity);
        Set(TEXT("r.TemporalAA.Upsampling"),Phases[Phase].TemporalUpsample);
        Began=RealTime;Cleared=false;
    };
    if(Phase<0){if(RealTime<45)return;Phase=0;Apply();}
    if(Phase>=UE_ARRAY_COUNT(Phases))return;
    // Allow render-state changes to settle before collecting each phase's timings.
    if(!Cleared&&RealTime-Began>=20){FrameTimes.Empty();DemotedFrames.Empty();GpuTimes.Empty();PreviousFrame=0;Cleared=true;}
    if(RealTime-Began<35)return;
    FWRecordCaptureGraphics(Player);FString Text;
    FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("Verification/capture-graphics.json")));
    FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectDir()/FString::Printf(TEXT("Verification/performance-sweep-%s.json"),Phases[Phase].Name)));
    Phase++;if(Phase>=UE_ARRAY_COUNT(Phases)){FPlatformMisc::RequestExit(false);return;}Apply();
}
