#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ChickenAudioComponent.generated.h"

class UAudioComponent;
class USoundBase;

UENUM(BlueprintType)
enum class EChickenSound : uint8
{
    Hop, Charge, ChargedJump, Land, Collision
};

/** Local presentation only; gameplay events are dispatched by the authoritative character. */
UCLASS(ClassGroup=(Audio), meta=(BlueprintSpawnableComponent))
class DKUENDGAME_API UChickenAudioComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    void Play(EChickenSound Event, float Strength);
    void Update(float DeltaTime, bool bCharging, float Charge);

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // Optional one-shot replacements. Unassigned entries use synthesized arcade effects.
    UPROPERTY(EditAnywhere, Category="Chicken|Audio")
    TMap<EChickenSound, TObjectPtr<USoundBase>> SoundOverrides;

    UPROPERTY(EditAnywhere, Category="Chicken|Audio", meta=(ClampMin="0", ClampMax="2"))
    float MasterVolume = 0.65f;

    UPROPERTY(EditAnywhere, Category="Chicken|Audio")
    bool bEnableSynthesizedSounds = true;

private:
    UPROPERTY(Transient)
    TMap<EChickenSound, TObjectPtr<UAudioComponent>> Voices;
    TMap<EChickenSound, float> RemainingTimes;
    float ChargePulseRemaining = 0.f;
    bool bWasCharging = false;
};
