#include "Modules/ModuleManager.h"
#include "FWReleaseGraphics.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "UObject/UObjectGlobals.h"
#include "FWCombatEffects.h"
#include "HAL/IConsoleManager.h"

class FFrostwatchModule : public FDefaultGameModuleImpl
{
    FDelegateHandle BeforeMap;
public:
    virtual void StartupModule() override
    {
#if WITH_EDITOR
        IConsoleManager::Get().RegisterConsoleCommand(TEXT("fw.AuthorDamageParts"),TEXT("Bake five body parts without modifying scenery"),FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&,UWorld* World){FWAuthorDamageParts(World);}),ECVF_Default);
        IConsoleManager::Get().RegisterConsoleCommand(TEXT("fw.AuthorCombatContact"),TEXT("Bake detached parts and fit rock support geometry in the editor world"),FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&,UWorld* World){FWAuthorCombatContact(World);}),ECVF_Default);
#endif
        // Apply after user settings load, before world components allocate lighting caches.
        // BeginPlay alone is too late and can retain larger transient allocations.
        BeforeMap=FCoreUObjectDelegates::PreLoadMap.AddLambda([](const FString&)
        {
            if(!GIsEditor&&GEngine)
                FWApplyGraphicsBudget(GEngine->GetGameUserSettings()->GetGlobalIlluminationQuality());
        });
    }
    virtual void ShutdownModule() override
    {
        FCoreUObjectDelegates::PreLoadMap.Remove(BeforeMap);
#if WITH_EDITOR
        IConsoleManager::Get().UnregisterConsoleObject(TEXT("fw.AuthorCombatContact"),false);
        IConsoleManager::Get().UnregisterConsoleObject(TEXT("fw.AuthorDamageParts"),false);
#endif
    }
};
IMPLEMENT_PRIMARY_GAME_MODULE(FFrostwatchModule, Frostwatch, "Frostwatch");
