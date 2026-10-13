#include "FWCombatEffects.h"
#include "FWGame.h"
#include "Engine/SkeletalMesh.h"

FName FWSeverBone(USkeletalMesh* Mesh,int Part)
{
    if(!Mesh||Part<0||Part>=5)return NAME_None;
    const TCHAR* Standard[]={TEXT("head"),TEXT("upperarm_l"),TEXT("upperarm_r"),TEXT("thigh_l"),TEXT("thigh_r")};
    const TCHAR* Orc[]={TEXT("CC_Base_Head"),TEXT("CC_Base_L_Upperarm"),TEXT("CC_Base_R_Upperarm"),TEXT("CC_Base_L_Thigh"),TEXT("CC_Base_R_Thigh")};
    for(FName Name:{FName(Standard[Part]),FName(Orc[Part])})if(Mesh->GetRefSkeleton().FindBoneIndex(Name)!=INDEX_NONE)return Name;
    return NAME_None;
}
FString FWSeverAssetPath(USkeletalMesh* Mesh,int Part)
{
    const TCHAR* Names[]={TEXT("Head"),TEXT("ArmL"),TEXT("ArmR"),TEXT("LegL"),TEXT("LegR")};
    if(!Mesh||Part<0||Part>=5)return FString();
    return FString::Printf(TEXT("/Game/Combat/Parts/SM_%s_%s"),*Mesh->GetName(),Names[Part]);
}

#if WITH_EDITOR
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "StaticMeshResources.h"
#include "StaticMeshAttributes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "PhysicsEngine/BodySetup.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

static bool BakePart(USkeletalMesh* Source,int Part)
{
    FName Bone=FWSeverBone(Source,Part);if(Bone.IsNone())return false;
    const auto* Render=Source->GetResourceForRendering();if(!Render||Render->LODRenderData.IsEmpty())return false;
    const int Lod=FMath::Min(1,Render->LODRenderData.Num()-1);
    const auto& Data=Render->LODRenderData[Lod];const auto& Weights=Data.SkinWeightVertexBuffer;
    const auto* Indices=Data.MultiSizeIndexContainer.GetIndexBuffer();if(!Indices)return false;
    const auto& Ref=Source->GetRefSkeleton();const int RootBone=Ref.FindBoneIndex(Bone);
    TArray<FTransform> BoneTransforms=Ref.GetRefBonePose();
    for(int I=0;I<BoneTransforms.Num();I++)if(Ref.GetParentIndex(I)>=0)BoneTransforms[I]=BoneTransforms[I]*BoneTransforms[Ref.GetParentIndex(I)];
    const FTransform ToBone=BoneTransforms[RootBone].Inverse();
    FMeshDescription Description;FStaticMeshAttributes Attributes(Description);Attributes.Register();
    auto Positions=Attributes.GetVertexPositions();auto Normals=Attributes.GetVertexInstanceNormals();auto UV=Attributes.GetVertexInstanceUVs();UV.SetNumChannels(1);
    auto Slots=Attributes.GetPolygonGroupMaterialSlotNames();
    const FString Path=FWSeverAssetPath(Source,Part);UPackage* Package=CreatePackage(*Path);
    UStaticMesh* Mesh=NewObject<UStaticMesh>(Package,*FPaths::GetBaseFilename(Path),RF_Public|RF_Standalone);
    TArray<FVector> All;int TriangleCount=0;
    for(const auto& Section:Data.RenderSections)
    {
        const auto Group=Description.CreatePolygonGroup();const FName Slot(*FString::Printf(TEXT("Part_%d"),Group.GetValue()));Slots[Group]=Slot;
        UMaterialInterface* Material=Source->GetMaterials().IsValidIndex(Section.MaterialIndex)?Source->GetMaterials()[Section.MaterialIndex].MaterialInterface.Get():nullptr;
        Mesh->GetStaticMaterials().Add(FStaticMaterial(Material,Slot));
        auto Belongs=[&](uint32 Vertex)
        {
            uint32 Selected=0,Total=0;
            for(uint32 Influence=0;Influence<Weights.GetMaxBoneInfluences();Influence++)
            {
                const uint32 Weight=Weights.GetBoneWeight(Vertex,Influence);if(!Weight)continue;
                const uint32 Local=Weights.GetBoneIndex(Vertex,Influence);if(!Section.BoneMap.IsValidIndex(Local))continue;
                const int Global=Section.BoneMap[Local];Total+=Weight;
                if(Global==RootBone||Ref.BoneIsChildOf(Global,RootBone))Selected+=Weight;
            }
            return Total&&Selected*2>=Total;
        };
        for(uint32 T=0;T<Section.NumTriangles;T++)
        {
            uint32 V[3];int Selected=0;for(int K=0;K<3;K++){V[K]=Indices->Get(Section.BaseIndex+T*3+K);Selected+=Belongs(V[K])?1:0;}
            if(Selected<2)continue;
            TArray<FVertexInstanceID> Corners;
            for(int K=0;K<3;K++)
            {
                const FVector Point=ToBone.TransformPosition(FVector(Data.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(V[K])));
                const auto Vertex=Description.CreateVertex();Positions[Vertex]=FVector3f(Point);All.Add(Point);
                const auto Instance=Description.CreateVertexInstance(Vertex);
                Normals[Instance]=FVector3f(ToBone.TransformVectorNoScale(FVector(Data.StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(V[K]))).GetSafeNormal());
                UV.Set(Instance,0,Data.StaticVertexBuffers.StaticMeshVertexBuffer.GetVertexUV(V[K],0));Corners.Add(Instance);
            }
            Description.CreatePolygon(Group,Corners);TriangleCount++;
        }
    }
    if(TriangleCount<8)return false;
    Mesh->AddSourceModel();Mesh->CreateMeshDescription(0,MoveTemp(Description));Mesh->CommitMeshDescription(0);
    auto& Build=Mesh->GetSourceModel(0).BuildSettings;Build.bRecomputeNormals=false;Build.bRecomputeTangents=true;
    Mesh->Build(false);Mesh->CreateBodySetup();auto* Body=Mesh->GetBodySetup();Body->CollisionTraceFlag=CTF_UseSimpleAsComplex;
    const FBox Bounds(All);FKBoxElem Box;Box.Center=Bounds.GetCenter();Box.X=FMath::Max(2.f,Bounds.GetSize().X);Box.Y=FMath::Max(2.f,Bounds.GetSize().Y);Box.Z=FMath::Max(2.f,Bounds.GetSize().Z);Body->AggGeom.BoxElems.Add(Box);
    FAssetRegistryModule::AssetCreated(Mesh);Package->MarkPackageDirty();
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
    const bool Saved=UPackage::SavePackage(Package,Mesh,*FPackageName::LongPackageNameToFilename(Path,FPackageName::GetAssetPackageExtension()),Args);
    UE_LOG(LogTemp,Display,TEXT("COMBAT_PART %s bone=%s triangles=%d saved=%d"),*Path,*Bone.ToString(),TriangleCount,Saved);return Saved;
}

void FWAuthorDamageParts(UWorld* World)
{
    if(!World)return;
    TSet<USkeletalMesh*> Done;int Parts=0;
    for(TActorIterator<AFWEnemy> It(World);It;++It)if(!It->Friendly&&!It->Dragon)
    {
        auto* Mesh=It->GetMesh()->GetSkeletalMeshAsset();if(!Mesh||Done.Contains(Mesh))continue;Done.Add(Mesh);
        for(int Part=0;Part<5;Part++)
        {
            // Reuse the verified head/arm assets and make reruns idempotent.
            auto* Existing=LoadObject<UStaticMesh>(nullptr,*FWSeverAssetPath(Mesh,Part),nullptr,LOAD_NoWarn|LOAD_Quiet);
            if(Existing&&Existing->GetBodySetup()&&Existing->GetRenderData()&&!Existing->GetRenderData()->LODResources.IsEmpty())Parts++;
            else if(!Existing&&BakePart(Mesh,Part))Parts++;
        }
    }
    FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"part_assets\":%d,\"meshes\":%d}"),Parts,Done.Num()),*(FPaths::ProjectDir()/TEXT("Verification/fighting-authoring.json")));
}

void FWAuthorCombatContact(UWorld* World)
{
    if(!World)return;
    TSet<USkeletalMesh*> Done;int Parts=0;
    for(TActorIterator<AFWEnemy> It(World);It;++It)if(!It->Friendly&&!It->Dragon)
    {
        auto* Mesh=It->GetMesh()->GetSkeletalMeshAsset();if(!Mesh||Done.Contains(Mesh))continue;Done.Add(Mesh);
        for(int Part=0;Part<3;Part++)if(BakePart(Mesh,Part))Parts++;
    }
    FString Text;FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("Content/Migration/world.json")));
    TSharedPtr<FJsonObject> Data;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data))return;
    const auto& Heights=Data->GetArrayField(TEXT("heights"));const int N=Data->GetIntegerField(TEXT("width"));const double Step=Data->GetNumberField(TEXT("step")),Extent=Data->GetNumberField(TEXT("extent"));
    auto Ground=[&](double X,double Y)
    {
        const double GX=FMath::Clamp((Y/100+Extent)/Step,0.,N-1.00001),GZ=FMath::Clamp((-X/100+Extent)/Step,0.,N-1.00001);
        const int IX=int(GX),IZ=int(GZ);const double TX=GX-IX,TZ=GZ-IZ;
        const double A=Heights[IZ*N+IX]->AsNumber(),B=Heights[IZ*N+IX+1]->AsNumber(),C=Heights[(IZ+1)*N+IX]->AsNumber(),D=Heights[(IZ+1)*N+IX+1]->AsNumber();
        return 100*(TX+TZ<=1?A+(B-A)*TX+(C-A)*TZ:D+(C-D)*(1-TX)+(B-D)*(1-TZ));
    };
    TArray<TSharedPtr<FJsonValue>> Rocks;
    for(TActorIterator<AActor> It(World);It;++It)
    {
        TArray<UStaticMeshComponent*> Components;It->GetComponents(Components);
        for(auto* C:Components)
        {
            UStaticMesh* Mesh=C->GetStaticMesh();if(!Mesh)continue;const FString Name=Mesh->GetName().ToLower();
            if(!Name.Contains(TEXT("rock"))&&!Name.Contains(TEXT("boulder"))&&!Name.Contains(TEXT("embankment")))continue;
            if(!Mesh->GetRenderData()||Mesh->GetRenderData()->LODResources.IsEmpty())continue;
            const auto& Vertices=Mesh->GetRenderData()->LODResources[0].VertexBuffers.PositionVertexBuffer;
            const FBoxSphereBounds Bounds=Mesh->GetBounds();
            auto Fit=[&](FTransform Transform,int Instance)
            {
                TArray<double> Support;
                for(uint32 I=0;I<Vertices.GetNumVertices();I++)
                {
                    const FVector V(Vertices.VertexPosition(I));const FVector Relative=V-Bounds.Origin;
                    if(Relative.Z>-.2*Bounds.BoxExtent.Z||FMath::Abs(Relative.X)>.85*Bounds.BoxExtent.X||FMath::Abs(Relative.Y)>.85*Bounds.BoxExtent.Y)continue;
                    const FVector P=Transform.TransformPosition(V);Support.Add(Ground(P.X,P.Y)-P.Z);
                }
                if(Support.Num()<4)return Transform;Support.Sort();
                const double Shift=Support[FMath::FloorToInt(Support.Num()*.1)]-8;
                auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("actor"),It->GetActorLabel());R->SetStringField(TEXT("mesh"),Mesh->GetPathName());R->SetNumberField(TEXT("instance"),Instance);R->SetNumberField(TEXT("shift_cm"),Shift);R->SetNumberField(TEXT("support_samples"),Support.Num());
                Transform.AddToTranslation(FVector(0,0,Shift));R->SetStringField(TEXT("position"),Transform.GetLocation().ToString());Rocks.Add(MakeShared<FJsonValueObject>(R));return Transform;
            };
            C->Modify();It->Modify();
            if(auto* Instances=Cast<UInstancedStaticMeshComponent>(C))
                for(int I=0;I<Instances->GetInstanceCount();I++){FTransform T;Instances->GetInstanceTransform(I,T,true);Instances->UpdateInstanceTransform(I,Fit(T,I),true,I==Instances->GetInstanceCount()-1,true);}
            else C->SetWorldTransform(Fit(C->GetComponentTransform(),-1));
            It->MarkPackageDirty();
        }
    }
    auto Report=MakeShared<FJsonObject>();Report->SetNumberField(TEXT("part_assets"),Parts);Report->SetArrayField(TEXT("rocks"),Rocks);
    FString Output;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Output));FFileHelper::SaveStringToFile(Output,*(FPaths::ProjectDir()/TEXT("Verification/combat-contact-authoring.json")));
    UE_LOG(LogTemp,Display,TEXT("COMBAT_CONTACT_AUTHOR parts=%d rocks=%d"),Parts,Rocks.Num());
}
#else
void FWAuthorCombatContact(UWorld*) {}
void FWAuthorDamageParts(UWorld*) {}
#endif
