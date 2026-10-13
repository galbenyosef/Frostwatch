#include "FWWorld.h"
#include "FWGame.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
AFWForest::AFWForest()
{
    Instances=CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Trees"));SetRootComponent(Instances);
    Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);Instances->SetCullDistances(18000,26000);
}
#include "ProceduralMeshComponent.h"
#include "KismetProceduralMeshLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "StaticMeshCompiler.h"
#endif

AFWTerrain::AFWTerrain()
{
    Terrain=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Terrain"));
    SetRootComponent(Terrain);Terrain->bUseAsyncCooking=true;
}
void AFWTerrain::OnConstruction(const FTransform& Transform){Super::OnConstruction(Transform);if(BuildTerrain)Generate();}
void AFWTerrain::BeginPlay()
{
    Super::BeginPlay();if(Terrain->GetNumSections()==0)Generate();
    if(FParse::Param(FCommandLine::Get(),TEXT("UntiledTerrain")))return;
    // Keep the original collision mesh. Separate render components give the renderer
    // independent bounds for frustum, occlusion and shadow culling without changing detail.
    const FProcMeshSection* Source=Terrain->GetProcMeshSection(0);
    if(!Source)return;
    const int32 Width=FMath::RoundToInt(FMath::Sqrt(float(Source->ProcVertexBuffer.Num())));
    if(Width*Width!=Source->ProcVertexBuffer.Num()||Source->ProcIndexBuffer.Num()%3!=0)return;
    // Only split our regular grid; never reinterpret an arbitrary authored mesh.
    TBitArray<> Present(false,(Width-1)*(Width-1)*2);
    for(int32 I=0;I<Source->ProcIndexBuffer.Num();I+=3)
    {
        const uint32 A=Source->ProcIndexBuffer[I],B=Source->ProcIndexBuffer[I+1],C=Source->ProcIndexBuffer[I+2];
        const bool First=B==A+Width&&C==A+1;
        const bool Second=A>0&&B==A+Width-1&&C==A+Width;
        if(!First&&!Second)return;
        const int32 Base=First?A:A-1,X=Base%Width,Z=Base/Width;
        if(X>=Width-1||Z>=Width-1)return;
        Present[(Z*(Width-1)+X)*2+(First?0:1)]=true;
    }
    int32 TileCells=64;FParse::Value(FCommandLine::Get(),TEXT("TerrainTileCells="),TileCells);TileCells=FMath::Clamp(TileCells,16,128);
    int32 TileCount=0,IndexCount=0;
    for(int32 Z=0;Z<Width-1;Z+=TileCells)for(int32 X=0;X<Width-1;X+=TileCells)
    {
        const int32 NX=FMath::Min(TileCells,Width-1-X),NZ=FMath::Min(TileCells,Width-1-Z);
        FProcMeshSection Section;
        for(int32 J=0;J<=NZ;J++)for(int32 I=0;I<=NX;I++)
        {
            const FProcMeshVertex& Vertex=Source->ProcVertexBuffer[(Z+J)*Width+X+I];
            Section.ProcVertexBuffer.Add(Vertex);Section.SectionLocalBox+=Vertex.Position;
        }
        for(int32 J=0;J<NZ;J++)for(int32 I=0;I<NX;I++)
        {
            const uint32 A=J*(NX+1)+I,B=A+1,C=A+NX+1,D=C+1;
            const int32 Cell=((Z+J)*(Width-1)+X+I)*2;
            if(Present[Cell])Section.ProcIndexBuffer.Append({A,C,B});
            if(Present[Cell+1])Section.ProcIndexBuffer.Append({B,C,D});
        }
        auto* Tile=NewObject<UProceduralMeshComponent>(this);
        AddInstanceComponent(Tile);Tile->SetupAttachment(Terrain);
        Tile->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Tile->SetCastShadow(Terrain->CastShadow);Tile->SetProcMeshSection(0,Section);
        Tile->SetMaterial(0,Terrain->GetMaterial(0));Tile->RegisterComponent();
        IndexCount+=Section.ProcIndexBuffer.Num();TileCount++;
    }
    Terrain->SetMeshSectionVisible(0,false);Terrain->SetCastShadow(false);
    UE_LOG(LogTemp,Display,TEXT("FROSTWATCH_TERRAIN_TILES count=%d indices=%d original=%d collision_retained=1"),TileCount,IndexCount,Source->ProcIndexBuffer.Num());
}
void AFWTerrain::Generate()
{
    FString Text; if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectContentDir()/TEXT("Migration/world.json"))))return;
    TSharedPtr<FJsonObject> Data;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data))return;
    int32 Width=Data->GetIntegerField(TEXT("width"));const auto& Heights=Data->GetArrayField(TEXT("heights"));
    if(Width<2||Width>1025||Heights.Num()!=Width*Width)return;
    TArray<FVector> V,N;TArray<FVector2D> UV;TArray<int32> T;TArray<FProcMeshTangent> Tangents;
    float Step=Data->GetNumberField(TEXT("step")),Extent=Data->GetNumberField(TEXT("extent"));
    const int32 SourceWidth=Width;Width=(Width-1)*2+1;Step*=.5f;
    for(int32 Z=0;Z<Width;Z++)for(int32 X=0;X<Width;X++)
    {
        const int32 SX=FMath::Min(X/2,SourceWidth-2),SZ=FMath::Min(Z/2,SourceWidth-2);
        const float TX=X*.5f-SX,TZ=Z*.5f-SZ;
        const float A=Heights[SZ*SourceWidth+SX]->AsNumber(),B=Heights[SZ*SourceWidth+SX+1]->AsNumber(),C=Heights[(SZ+1)*SourceWidth+SX]->AsNumber(),D=Heights[(SZ+1)*SourceWidth+SX+1]->AsNumber();
        float H=(TX+TZ<=1?A+(B-A)*TX+(C-A)*TZ:D+(C-D)*(1-TX)+(B-D)*(1-TZ))*100;
        FVector Position((Extent-Z*Step)*100,(-Extent+X*Step)*100,H);
        if(AFWCharacter::SurfaceAt(Position)==TEXT("snow"))Position.Z+=FMath::PerlinNoise2D(FVector2D(Position.X,Position.Y)/750.f)*14+FMath::PerlinNoise2D(FVector2D(Position.X+4123,Position.Y-6511)/270.f)*5;
        V.Add(Position);
        UV.Add(FVector2D(X*Step/4,Z*Step/4));
    }
    struct FCaveCut {FVector A,B;float Radius,Bottom,Top;};TArray<FCaveCut> Cuts;
    const TArray<TSharedPtr<FJsonValue>>* CutRows=nullptr;
    if(Data->TryGetArrayField(TEXT("cave_cutouts"),CutRows))for(const auto& Value:*CutRows)
    {
        const auto Row=Value->AsObject();const auto& A=Row->GetArrayField(TEXT("a"));const auto& B=Row->GetArrayField(TEXT("b"));
        Cuts.Add({FVector(A[0]->AsNumber(),A[1]->AsNumber(),A[2]->AsNumber()),FVector(B[0]->AsNumber(),B[1]->AsNumber(),B[2]->AsNumber()),float(Row->GetNumberField(TEXT("radius"))),float(Row->GetNumberField(TEXT("bottom"))),float(Row->GetNumberField(TEXT("top")))});
    }
    auto Open=[&](const FVector& P)
    {
        for(const auto& C:Cuts)
        {
            const FVector2D A(C.A.X,C.A.Y),D(C.B.X-C.A.X,C.B.Y-C.A.Y),Q(P.X-C.A.X,P.Y-C.A.Y);
            const float Alpha=FMath::Clamp(FVector2D::DotProduct(Q,D)/FMath::Max(1.0,D.SizeSquared()),0.0,1.0);
            const float Floor=FMath::Lerp(C.A.Z,C.B.Z,Alpha);
            if((Q-D*Alpha).SizeSquared()<FMath::Square(C.Radius)&&P.Z>Floor+C.Bottom&&P.Z<Floor+C.Top)return true;
        }
        return false;
    };
    for(int32 Z=0;Z<Width-1;Z++)for(int32 X=0;X<Width-1;X++)
    {
        const int32 A=Z*Width+X,B=A+1,C=A+Width,D=C+1;
        if(!Open((V[A]+V[C]+V[B])/3))T.Append({A,C,B});
        if(!Open((V[B]+V[C]+V[D])/3))T.Append({B,C,D});
    }
    // The regular height grid has no split vertices: compute derivatives in linear time.
    // The general mesh helper searches for coincident vertices and is prohibitive here.
    N.SetNum(V.Num());Tangents.SetNum(V.Num());
    for(int32 Z=0;Z<Width;Z++)for(int32 X=0;X<Width;X++)
    {
        const FVector DX=V[Z*Width+FMath::Min(X+1,Width-1)]-V[Z*Width+FMath::Max(X-1,0)];
        const FVector DZ=V[FMath::Min(Z+1,Width-1)*Width+X]-V[FMath::Max(Z-1,0)*Width+X];
        N[Z*Width+X]=FVector::CrossProduct(DX,DZ).GetSafeNormal();
        Tangents[Z*Width+X]=FProcMeshTangent(DX.GetSafeNormal(),false);
    }
    Terrain->CreateMeshSection(0,V,T,N,UV,TArray<FColor>(),Tangents,true);Terrain->SetMaterial(0,Surface);
}
UStaticMesh* UFWAuthoring::MakeMesh(const FString& PackagePath,const TArray<FVector>& V,const TArray<int32>& T,const TArray<FVector2D>& UV,UMaterialInterface* Material)
{
#if WITH_EDITOR
    if(V.IsEmpty()||T.Num()%3!=0)return nullptr;
    for(int32 Index:T)if(!V.IsValidIndex(Index))return nullptr;
    UPackage* Package=CreatePackage(*PackagePath);
    UStaticMesh* Mesh=LoadObject<UStaticMesh>(nullptr,*PackagePath);
    const bool IsNew=Mesh==nullptr;
    if(!Mesh)Mesh=NewObject<UStaticMesh>(Package,*FPaths::GetBaseFilename(PackagePath),RF_Public|RF_Standalone);
    // Loading an existing mesh can queue compilation. Complete it before
    // rebuilding its source models during a repeatable authoring pass.
    UStaticMesh* PendingMesh=Mesh;
    FStaticMeshCompilingManager::Get().FinishCompilation(MakeArrayView(&PendingMesh,1));
    Mesh->SetNumSourceModels(0);Mesh->GetStaticMaterials().Reset();
    Mesh->GetStaticMaterials().Add(FStaticMaterial(Material));
    FMeshDescription Description;FStaticMeshAttributes Attributes(Description);Attributes.Register();
    auto Positions=Attributes.GetVertexPositions();auto Normals=Attributes.GetVertexInstanceNormals();auto TexCoords=Attributes.GetVertexInstanceUVs();TexCoords.SetNumChannels(1);
    TArray<FVertexID> IDs;
    for(const FVector& Point:V){auto Id=Description.CreateVertex();Positions[Id]=FVector3f(Point);IDs.Add(Id);}
    auto Group=Description.CreatePolygonGroup();
    for(int32 I=0;I<T.Num();I+=3)
    {
        const FVector Normal=FVector::CrossProduct(V[T[I+1]]-V[T[I]],V[T[I+2]]-V[T[I]]).GetSafeNormal();
        TArray<FVertexInstanceID> Corners;
        for(int32 J=0;J<3;J++){int32 Index=T[I+(J==0?0:3-J)];auto Id=Description.CreateVertexInstance(IDs[Index]);Normals[Id]=FVector3f(Normal);TexCoords.Set(Id,0,FVector2f(UV.IsValidIndex(Index)?UV[Index]:FVector2D::ZeroVector));Corners.Add(Id);}
        Description.CreatePolygon(Group,Corners);
    }
    Mesh->AddSourceModel();Mesh->CreateMeshDescription(0,MoveTemp(Description));Mesh->CommitMeshDescription(0);
    Mesh->GetSourceModel(0).BuildSettings.bRecomputeNormals=false;
    Mesh->GetSourceModel(0).BuildSettings.bRecomputeTangents=true;
    Mesh->Build(false);Mesh->CreateBodySetup();
    Mesh->GetBodySetup()->CollisionTraceFlag=CTF_UseComplexAsSimple;
    Mesh->GetBodySetup()->bDoubleSidedGeometry=true;
    if(IsNew)FAssetRegistryModule::AssetCreated(Mesh);Package->MarkPackageDirty();
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
    UPackage::SavePackage(Package,Mesh,*FPackageName::LongPackageNameToFilename(PackagePath,FPackageName::GetAssetPackageExtension()),Args);
    return Mesh;
#else
    return nullptr;
#endif
}
