using UnrealBuildTool;
public class Frostwatch : ModuleRules
{
    public Frostwatch(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new [] {"Core", "CoreUObject", "Engine", "InputCore", "Json", "JsonUtilities", "ProceduralMeshComponent", "AIModule", "NavigationSystem", "MeshDescription", "StaticMeshDescription"});
        PrivateDependencyModuleNames.AddRange(new [] {"RHI", "Slate", "SlateCore"});
        if(Target.bBuildEditor) PrivateDependencyModuleNames.AddRange(new [] {"UnrealEd", "AssetRegistry"});
    }
}
