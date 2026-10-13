#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FWWorld.generated.h"

class UProceduralMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
UCLASS()
class FROSTWATCH_API AFWForest : public AActor
{
    GENERATED_BODY()
public:
    AFWForest();
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Instances;
};
UCLASS()
class FROSTWATCH_API AFWTerrain : public AActor
{
    GENERATED_BODY()
public:
    AFWTerrain();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> Terrain;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UMaterialInterface> Surface;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) bool BuildTerrain=true;
    UFUNCTION(BlueprintCallable,CallInEditor) void Generate();
};

UCLASS()
class FROSTWATCH_API UFWAuthoring : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Frostwatch|Authoring")
    static UStaticMesh* MakeMesh(const FString& PackagePath,const TArray<FVector>& Vertices,const TArray<int32>& Triangles,const TArray<FVector2D>& UVs,UMaterialInterface* Material);
};
