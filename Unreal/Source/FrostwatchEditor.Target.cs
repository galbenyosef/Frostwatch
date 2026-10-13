using UnrealBuildTool;
public class FrostwatchEditorTarget : TargetRules
{
    public FrostwatchEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("Frostwatch");
    }
}
