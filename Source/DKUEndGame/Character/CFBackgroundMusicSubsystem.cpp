#include "Character/CFBackgroundMusicSubsystem.h"
#include "UI/CFLobbySubsystem.h"
#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWaveProcedural.h"

namespace
{
    constexpr int32 MusicSampleRate = 24000;
    constexpr float BeatSeconds = 0.5f; // 120 BPM, eight bars / sixteen seconds.

    double NoteHz(int32 Midi) { return 440.0 * FMath::Pow(2.0, (Midi - 69) / 12.0); }

    double Pluck(int32 Midi, double Time, double Length)
    {
        const double Phase = 2.0 * PI * NoteHz(Midi) * Time;
        const double Envelope = FMath::Min(Time / 0.006, 1.0)
            * FMath::Max(0.0, 1.0 - Time / Length) * FMath::Exp(-5.0 * Time);
        return (FMath::Sin(Phase) + 0.22 * FMath::Sin(2.0 * Phase)) * Envelope;
    }
}

void UCFBackgroundMusicSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UCFLobbySubsystem>();
    bInitialized = true;
}

void UCFBackgroundMusicSubsystem::BuildLoop()
{
    // Original C-major arcade phrase: melody, offbeat chords, bass and soft drums.
    static constexpr int32 Melody[64] = {
        72, 76, 79, 76, 81, 79, 76, 74, 72, 76, 79, 84, 83, 79, 76, 0,
        69, 72, 76, 72, 79, 76, 72, 71, 69, 72, 76, 81, 79, 76, 72, 0,
        65, 69, 72, 77, 76, 72, 69, 67, 65, 69, 72, 76, 77, 76, 72, 0,
        67, 71, 74, 79, 77, 74, 71, 69, 67, 71, 74, 79, 76, 74, 72, 0
    };
    static constexpr int32 Roots[8] = {48, 48, 45, 45, 41, 41, 43, 43};
    FRandomStream Noise(20261112);
    LoopSamples.SetNumUninitialized(MusicSampleRate * 16);
    for (int32 Index = 0; Index < LoopSamples.Num(); ++Index)
    {
        const double Time = static_cast<double>(Index) / MusicSampleRate;
        const int32 Eighth = static_cast<int32>(Time / (BeatSeconds * 0.5));
        const int32 Beat = Eighth / 2;
        const int32 Bar = Beat / 4;
        const double EighthTime = Time - Eighth * BeatSeconds * 0.5;
        const double BeatTime = Time - Beat * BeatSeconds;
        double Sample = Melody[Eighth] ? 0.30 * Pluck(Melody[Eighth], EighthTime, 0.25) : 0.0;
        Sample += 0.24 * Pluck(Roots[Bar] + (Beat % 2 ? 7 : 0), BeatTime, 0.5);
        if (Eighth % 2)
        {
            const int32 Third = Bar == 2 || Bar == 3 ? 3 : 4;
            Sample += 0.065 * (Pluck(Roots[Bar] + 12, EighthTime, 0.25)
                + Pluck(Roots[Bar] + 12 + Third, EighthTime, 0.25)
                + Pluck(Roots[Bar] + 19, EighthTime, 0.25));
        }
        const double Attack = FMath::Min(BeatTime / 0.004, 1.0);
        if (Beat % 2 == 0)
            Sample += 0.20 * FMath::Sin(2.0 * PI * (58.0 * BeatTime + 2.0 * (1.0 - FMath::Exp(-30.0 * BeatTime))))
                * FMath::Exp(-22.0 * BeatTime) * Attack;
        else
            Sample += 0.07 * Noise.FRandRange(-1.f, 1.f) * FMath::Exp(-35.0 * BeatTime) * Attack;
        Sample += 0.025 * Noise.FRandRange(-1.f, 1.f) * FMath::Exp(-85.0 * EighthTime)
            * FMath::Min(EighthTime / 0.002, 1.0);
        // Silence both loop edges to avoid a discontinuity when the phrase repeats.
        Sample *= FMath::Min(1.0, (16.0 - Time) / 0.015);
        LoopSamples[Index] = static_cast<int16>(FMath::Clamp(Sample, -0.95, 0.95) * 32767.0);
    }
}

void UCFBackgroundMusicSubsystem::Tick(float DeltaTime)
{
    UWorld* World = GetWorld();
    if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_DedicatedServer) return;
    const FString Map = UGameplayStatics::GetCurrentLevelName(World, true);
    const bool bMusicMap = Map == TEXT("L_Lobby_Playground") || Map == TEXT("L_Round1")
        || Map == TEXT("L_Round2") || Map == TEXT("L_Round3") || Map == TEXT("L_Round4");
    if (!bMusicMap) { StopMusic(); return; }
    if (!GetGameInstance()->GetFirstLocalPlayerController()) return;

    if (!MusicVoice)
    {
        if (LoopSamples.IsEmpty()) BuildLoop();
        MusicWave = NewObject<USoundWaveProcedural>(this);
        MusicWave->SetSampleRate(MusicSampleRate);
        MusicWave->NumChannels = 1;
        MusicWave->Duration = INDEFINITELY_LOOPING_DURATION;
        MusicWave->VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
        MusicWave->QueueAudio(reinterpret_cast<const uint8*>(LoopSamples.GetData()), LoopSamples.Num() * sizeof(int16));
        // No actor owner, spatial attenuation or per-map sound mix: BG volume applies once.
        const float Volume = 0.35f * GetGameInstance()->GetSubsystem<UCFLobbySubsystem>()->MusicVolume;
        MusicVoice = UGameplayStatics::SpawnSound2D(World, MusicWave, Volume, 1.f, 0.f, nullptr, true, false);
        if (!MusicVoice) { MusicWave = nullptr; return; }
        MusicVoice->bIsUISound = true;
    }
    // Refill on the game thread well ahead of underflow; no UObject access on the audio thread.
    if (MusicWave->GetAvailableAudioByteCount() < MusicSampleRate * sizeof(int16) * 2)
        MusicWave->QueueAudio(reinterpret_cast<const uint8*>(LoopSamples.GetData()), LoopSamples.Num() * sizeof(int16));
    MusicVoice->SetVolumeMultiplier(0.35f * GetGameInstance()->GetSubsystem<UCFLobbySubsystem>()->MusicVolume);
}

void UCFBackgroundMusicSubsystem::StopMusic()
{
    if (MusicVoice) { MusicVoice->Stop(); MusicVoice->DestroyComponent(); MusicVoice = nullptr; }
    MusicWave = nullptr;
}

void UCFBackgroundMusicSubsystem::Deinitialize()
{
    bInitialized = false;
    StopMusic();
    LoopSamples.Reset();
    Super::Deinitialize();
}

TStatId UCFBackgroundMusicSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UCFBackgroundMusicSubsystem, STATGROUP_Tickables);
}
