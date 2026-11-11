#include "Character/ChickenAudioComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Actor.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"
#include "Engine/GameInstance.h"
#include "UI/CFLobbySubsystem.h"

namespace
{
    USoundWaveProcedural* MakeEffect(UObject* Outer, EChickenSound Event, float Strength, float& Duration)
    {
        float StartHz = 420.f, EndHz = 220.f, NoiseMix = 0.f;
        Duration = 0.14f;
        switch (Event)
        {
        case EChickenSound::Charge:
            StartHz = FMath::Lerp(260.f, 740.f, Strength);
            EndHz = StartHz * 1.1f; Duration = 0.18f; break;
        case EChickenSound::ChargedJump:
            StartHz = FMath::Lerp(380.f, 650.f, Strength);
            EndHz = 130.f; Duration = 0.26f; NoiseMix = 0.12f; break;
        case EChickenSound::Land:
            StartHz = 130.f; EndHz = 55.f; Duration = 0.16f; NoiseMix = 0.28f; break;
        case EChickenSound::Collision:
            StartHz = 190.f; EndHz = 65.f; Duration = 0.22f; NoiseMix = 0.45f; break;
        default: break;
        }

        constexpr int32 SampleRate = 24000;
        TArray<int16> Samples;
        Samples.SetNumUninitialized(FMath::CeilToInt(Duration * SampleRate));
        FRandomStream Noise(1701);
        float Phase = 0.f;
        for (int32 Index = 0; Index < Samples.Num(); ++Index)
        {
            const float Time = static_cast<float>(Index) / SampleRate;
            const float Progress = Time / Duration;
            Phase += 2.f * PI * FMath::Lerp(StartHz, EndHz, Progress) / SampleRate;
            // Short attack/release envelopes avoid clicks at pulse boundaries.
            const float Envelope = FMath::Min(Time / 0.008f, 1.f)
                * FMath::Square(1.f - Progress);
            const float Sample = FMath::Lerp(FMath::Sin(Phase), Noise.FRandRange(-1.f, 1.f), NoiseMix);
            Samples[Index] = static_cast<int16>(Sample * Envelope * 18000.f);
        }
        auto* Wave = NewObject<USoundWaveProcedural>(Outer);
        Wave->SetSampleRate(SampleRate);
        Wave->NumChannels = 1;
        Wave->Duration = Duration;
        Wave->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()), Samples.Num() * sizeof(int16));
        return Wave;
    }
}

float UChickenAudioComponent::GetUserEffectsVolume() const
{
    const auto* Owner = GetOwner();
    const auto* Instance = Owner ? Owner->GetGameInstance() : nullptr;
    const auto* Settings = Instance ? Instance->GetSubsystem<UCFLobbySubsystem>() : nullptr;
    return Settings ? Settings->EffectsVolume : 1.f;
}

void UChickenAudioComponent::Play(EChickenSound Event, float Strength)
{
    AActor* Owner = GetOwner();
    if (!Owner || Owner->GetNetMode() == NM_DedicatedServer || MasterVolume <= 0.f) return;
    Strength = FMath::Clamp(Strength, 0.f, 1.f);
    float Duration = 0.f;
    USoundBase* Sound = SoundOverrides.FindRef(Event);
    if (Sound)
    {
        Duration = FMath::Clamp(Sound->GetDuration(), 0.05f, 5.f);
    }
    else if (bEnableSynthesizedSounds)
    {
        Sound = MakeEffect(this, Event, Strength, Duration);
    }
    if (!Sound) return;

    UAudioComponent* Voice = Voices.FindRef(Event);
    if (!Voice)
    {
        Voice = NewObject<UAudioComponent>(Owner);
        Voice->bAutoActivate = false;
        Voice->bAutoDestroy = false;
        Voice->bStopWhenOwnerDestroyed = true;
        Voice->SetupAttachment(Owner->GetRootComponent());
        Voice->RegisterComponent();
        FSoundAttenuationSettings Attenuation;
        Attenuation.bAttenuate = true;
        Attenuation.bSpatialize = true;
        Attenuation.AttenuationShapeExtents = FVector(250.f, 0.f, 0.f);
        Attenuation.FalloffDistance = 1800.f;
        Voice->AdjustAttenuation(Attenuation);
        Voices.Add(Event, Voice);
    }
    Voice->Stop();
    Voice->SetSound(Sound);
    const float EventVolume = Event == EChickenSound::Charge ? 0.28f : FMath::Lerp(0.35f, 1.f, Strength);
    EventVolumes.Add(Event, EventVolume);
    Voice->SetVolumeMultiplier(MasterVolume * EventVolume * GetUserEffectsVolume());
    Voice->Play();
    // Procedural streams do not finish on queue exhaustion; explicitly release each voice.
    RemainingTimes.Add(Event, Duration + 0.05f);
}

void UChickenAudioComponent::Update(float DeltaTime, bool bCharging, float Charge)
{
    if (!GetOwner() || GetOwner()->GetNetMode() == NM_DedicatedServer) return;
    const float UserVolume = GetUserEffectsVolume();
    for (const auto& Entry : Voices)
        if (Entry.Value) Entry.Value->SetVolumeMultiplier(MasterVolume * EventVolumes.FindRef(Entry.Key) * UserVolume);
    for (auto It = RemainingTimes.CreateIterator(); It; ++It)
    {
        It.Value() -= DeltaTime;
        if (It.Value() <= 0.f)
        {
            if (UAudioComponent* Voice = Voices.FindRef(It.Key()))
            {
                Voice->Stop();
                Voice->SetSound(nullptr);
            }
            It.RemoveCurrent();
        }
    }
    if (bCharging)
    {
        ChargePulseRemaining -= DeltaTime;
        if (!bWasCharging || ChargePulseRemaining <= 0.f)
        {
            Play(EChickenSound::Charge, Charge);
            ChargePulseRemaining = 0.24f;
        }
    }
    else if (bWasCharging)
    {
        if (UAudioComponent* Voice = Voices.FindRef(EChickenSound::Charge)) Voice->FadeOut(0.04f, 0.f);
        ChargePulseRemaining = 0.f;
    }
    bWasCharging = bCharging;
}

void UChickenAudioComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    for (auto& Entry : Voices)
    {
        if (Entry.Value) Entry.Value->DestroyComponent();
    }
    Voices.Reset();
    RemainingTimes.Reset();
    EventVolumes.Reset();
    Super::EndPlay(EndPlayReason);
}
