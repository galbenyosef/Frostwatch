using UnrealBuildTool;
public class FrostwatchTarget : TargetRules
{
    public FrostwatchTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("Frostwatch");
    }
}
