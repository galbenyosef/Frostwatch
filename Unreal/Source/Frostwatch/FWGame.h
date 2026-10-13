#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/SaveGame.h"
#include "FWFighting.h"
#include "FWGame.generated.h"

class UInstancedStaticMeshComponent;
class UDecalComponent;
class UCameraComponent;
class UStaticMeshComponent;
class UAnimSequence;
class AFWHorse;
class UAudioComponent;
class USoundBase;
class USoundAttenuation;
class UPointLightComponent;
class USpotLightComponent;
class AFWCharacter;

USTRUCT(BlueprintType)
struct FFWParkedVehicle
{
    GENERATED_BODY()
    UPROPERTY() FString Identity;
    UPROPERTY() FVector Position=FVector::ZeroVector;
    UPROPERTY() float Yaw=0;
    UPROPERTY() float Energy=100;
};

UCLASS()
class FROSTWATCH_API UFWExpedition : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() int32 Version=1;
    UPROPERTY() TArray<FString> Flags;
    UPROPERTY() TArray<FString> Defeated;
    // Defeated remains quest history; this map records only pending reinforcements.
    UPROPERTY() TMap<FString,float> RespawnAtHour;
    UPROPERTY() int32 RespawnVersion=0;
    float CampaignHours() const { return (Day-1)*24.f+Hour; }
    UPROPERTY() FVector Checkpoint=FVector(-39000,0,3000);
    UPROPERTY() int32 Silver=60;
    UPROPERTY() TArray<int32> Seals;
    UPROPERTY() TArray<FString> Discovered;
    UPROPERTY() FString CheckpointId="hearth";
    UPROPERTY() int32 TrackedQuest=0;
    UPROPERTY() bool Won=false;
    UPROPERTY() float Hour=9;
    UPROPERTY() int32 Day=1;
    UPROPERTY() bool Music=true;
    UPROPERTY() bool Lantern=false;
    UPROPERTY() TArray<FFWParkedVehicle> ParkedVehicles;
    UPROPERTY() FString OriginalLegacyJson;
};

UCLASS()
class FROSTWATCH_API AFWEnemy : public ACharacter
{
    GENERATED_BODY()
public:
    AFWEnemy();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    UFUNCTION(BlueprintCallable,CallInEditor) void Assemble();
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TArray<TObjectPtr<USkeletalMesh>> Parts;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Identity="guard";
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString DisplayName="Frostwatch guard";
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool Friendly=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Health=100;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxHealth=100;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float StrikeDamage=19;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool Dragon=false;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UMaterialInterface> BreathMaterial;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> BreathJet;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ReflectedJet;
    float BreathTime=0,BreathHitClock=0,ReflectFlash=0;
    FVector BreathDirection;
    void SetupDragon();
    void TickDragon(float Dt,AFWCharacter* Player);
    FVector BreathOrigin() const;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Job;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FVector> WorkRoute;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FVector> InnRoute;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FVector> SleepRoute;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FVector> MorningRoute;
    FString Activity;
    FString RoutinePhase;
    TArray<FVector> ActiveRoute;
    int32 RouteIndex=0;
    float WorkPause=0;
    void UpdateRoutine(float Dt,AFWCharacter* Player);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UAnimSequence> Idle;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UAnimSequence> Walk;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UAnimSequence> Attack;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UAnimSequence> Death;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UAnimSequence> HitReaction;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<TObjectPtr<UAnimSequence>> AttackVariants;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UAnimSequence> HitBack;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UAnimSequence> HitLeft;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UAnimSequence> HitRight;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool BuiltInWeapon=false;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Weapon;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> WorkTool;
    bool Dead=false;
    float Windup=0, Recovery=0, Stagger=0, DeathAge=0;
    float MeleeWindupLength=.75f;
    bool MeleeSoundPlayed=false;
    bool StoryBarkPlayed=false;
    FVector MeleeDirection=FVector::ForwardVector;
    bool IsInsideMeleeArc(const FVector& Target) const;
    float RiposteWindow=0;
    int32 AttackIndex=0;
    bool Alerted=false;
    FVector Home;
    FTransform HomeTransform;
    FTransform MeshHomeTransform;
    FVector FinalImpulse;
    UPROPERTY() TArray<TObjectPtr<UStaticMesh>> SeverMeshes;
    UPROPERTY() TArray<TObjectPtr<AActor>> DamageDebris;
    FName SeveredBone=NAME_None;
    TSet<FName> SeveredBones;
    float PartDamage[5]={0,0,0,0,0};
    int LastHitPart=INDEX_NONE;
    float UninjuredSpeed=230, BleedTime=0, BleedClock=0;
    FName BleedBone=NAME_None;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> BloodInstances;
    TArray<FFWBloodDrop> BloodDrops;
    TArray<TWeakObjectPtr<UDecalComponent>> BloodPools;
    TArray<float> BloodPoolAges;
    void ReceiveStrike(float Damage,const FVector& Direction,bool Heavy,bool Execute=false,const FVector& HitPoint=FVector::ZeroVector,bool Blade=true);
    void Die(const FVector& Direction);
    void ResetEncounter();
    bool CanRespawn() const;
    bool RespawnDue(const UFWExpedition* Save,const FVector& PlayerLocation) const;
    void RestoreEncounter(UFWExpedition* Save);
    bool TryRespawn(AFWCharacter* Player);
    float RespawnCheckClock=0;
    void Play(UAnimSequence* Clip,bool Loop);
    UAnimSequence* SelectHitReaction(const FVector& Direction) const;
private:
    UAnimSequence* CurrentClip=nullptr;
};

UCLASS()
class FROSTWATCH_API AFWCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    AFWCharacter();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Sword;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Shield;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> LanternLight;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USpotLightComponent> LanternPathLight;
    float Health=100, Stamina=100;
    bool Blocking=false, GuardHeld=false, Sprinting=false, Journal=false, Menu=false;
    float BlockAge=100, AttackAge=-1, AttackLength=.55f, ImpactAt=.22f;
    float DodgeCooldown=0, Invulnerability=0, HurtFlash=0, HitFeedback=0;
    float GuardImpact=0,ParryFeedback=0;
    float StrikeHold=0, GuardBroken=0;
    bool AttackSoundPlayed=false;
    UPROPERTY() TArray<TObjectPtr<USoundBase>> CombatClashes;
    UPROPERTY() TArray<TObjectPtr<USoundBase>> CombatSwings;
    UPROPERTY() TArray<TObjectPtr<USoundBase>> CombatImpacts;
    bool HeavyAttack=false, HitResolved=false;
    void UpdateCombat(float Dt);
    int32 Combo=0, QueuedAttack=0;
    FString Message="Reach Captain Rowan at the gate. E to speak.";
    float MessageTime=10;
    FString CombatMessage, SaveError;
    float CombatMessageTime=0;
    UPROPERTY() TObjectPtr<UFWExpedition> Expedition;
    UPROPERTY() TObjectPtr<AFWHorse> MountedHorse;
    UPROPERTY() TObjectPtr<UAudioComponent> DialogueVoice;
    FString Screen, DialogueTitle, DialogueText, ServiceId, RebindTarget;
    bool Started=false, Restoring=false;
    float Sensitivity=.65f;
    bool InvertY=false;
    bool ReducedMotion=false;
    float StoryTextScale=1.f;
    int32 IntroPage=0;
    float FootstepClock=0;
    int32 LastStepIndex=-1, StepSequence=0;
    UPROPERTY() TMap<FString,TObjectPtr<USoundBase>> TraversalSounds;
    UPROPERTY() TObjectPtr<USoundAttenuation> StepAttenuation;
    void UpdateTraversalSound(float Dt);
    static FString SurfaceAt(const FVector& Location);
    void JumpStart();
    void ValidateTraversal();
    void UpdateSwimming(float Dt);
    void ValidateSwimming(float Dt);
    float SwimPush=0;
    bool WasSwimming=false;
    bool AuthoredView=false;
    UPROPERTY() TObjectPtr<UAnimSequence> ViewIdle;
    UPROPERTY() TObjectPtr<UAnimSequence> ViewWalk;
    UPROPERTY() TArray<TObjectPtr<UAnimSequence>> ViewAttacks;
    UPROPERTY() TObjectPtr<UAnimSequence> ViewClip;
    int32 ViewState=-1;
    void SetupViewModel();
    void UpdateViewModel(bool Restart=false);
    FVector InitialLocation;
    void SetScreen(const FString& Name);
    void ShowDialogue(const FString& Title,const FString& Text);
    void MenuAction(const FString& Action);
    void AnyKey(FKey Key);
    void ToggleMap();
    void ToggleLantern();
    float MaxStamina() const;
    FString KeyLabel(const FString& Action) const;
    void MoveForward(float Value);
    void MoveRight(float Value);
    void LookX(float Value);
    void LookY(float Value);
    EFWStrike StrikeMove=EFWStrike::Cut, QueuedMove=EFWStrike::Cut;
    float ForwardIntent=0, StrafeIntent=0, ChainTime=0, FeintRecovery=0;
    int LightChain=0;
    void Light(); void Heavy(); void StartAttack(bool Strong);
    void BeginStrike(EFWStrike Move); void Feint();
    void BlockStart(); void BlockEnd(); void Dodge();
    void SprintStart(); void SprintEnd(); void CrouchToggle();
    void Interact(); void ToggleJournal(); void ToggleMenu();
    void Save(); void Load();
    bool SaveExpedition();
    void CombatNotify(const FString& Text,float Seconds=1.2f);
    void ReceiveEnemyHit(AFWEnemy* Enemy,float Damage);
    void Notify(const FString& Text,float Seconds=4);
    AFWEnemy* TargetEnemy(float Range=400,FHitResult* OutHit=nullptr) const;
    bool FindStrikeContact(float FromAge,float ToAge,FHitResult& Hit) const;
    bool HasFlag(const FString& Flag) const;
    void Flag(const FString& Value);
    FString Objective() const;
    bool CanAct() const {return Health>0 && !Journal && !Menu;}
private:
    void ResolveStrike(float FromAge,float ToAge);
    float AutoSaveClock=0;
    FVector SwordBase=FVector(40,23,-15);
};

UCLASS()
class FROSTWATCH_API AFWHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
    virtual void NotifyHitBoxClick(FName BoxName) override;
    void Button(const FString& Id,const FString& Text,float X,float Y,float W,float H=34);
    float Paragraph(const FString& Text,float X,float Y,float W,float Scale=1);
};

UCLASS()
class FROSTWATCH_API AFWGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AFWGameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    UPROPERTY() TArray<TObjectPtr<UAudioComponent>> Ambience;
    static float InnMusicGain(const FVector& Player, const FVector& Inn, bool Enabled);
};

UCLASS()
class FROSTWATCH_API AFWInteractable : public AActor
{
    GENERATED_BODY()
public:
    AFWInteractable();
    virtual void Tick(float Dt) override;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> FireLight;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FString Identity;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FString Title;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) FString Words;
};
