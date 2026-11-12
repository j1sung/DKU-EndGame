#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "CFBackgroundMusicSubsystem.generated.h"

class UAudioComponent;
class USoundWaveProcedural;

/** One local music stream shared by the practice lobby and all four rounds. */
UCLASS()
class DKUENDGAME_API UCFBackgroundMusicSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override { return bInitialized && !IsTemplate(); }
    virtual bool IsTickableWhenPaused() const override { return true; }
    virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }
    virtual TStatId GetStatId() const override;
private:
    void BuildLoop();
    void StopMusic();
    bool bInitialized = false;
    TArray<int16> LoopSamples;
    UPROPERTY(Transient) TObjectPtr<USoundWaveProcedural> MusicWave;
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> MusicVoice;
};
