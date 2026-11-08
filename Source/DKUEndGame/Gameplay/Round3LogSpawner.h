#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Round3LogSpawner.generated.h"

class AStaticMeshActor;
class UStaticMeshComponent;

/** A repeatable gravity-driven log for the Round3 ramps. */
UCLASS()
class DKUENDGAME_API ARound3LogSpawner : public AActor
{
    GENERATED_BODY()

public:
    ARound3LogSpawner();
    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Log")
    TObjectPtr<UStaticMeshComponent> PreviewLog;

    UPROPERTY(EditAnywhere, Category="Log", meta=(ClampMin="0.1", Units="s"))
    float RespawnDelay = 2.f;

    UPROPERTY(EditAnywhere, Category="Log", meta=(ClampMin="1.0", Units="s"))
    float MaximumLifetime = 12.f;

    UPROPERTY(EditAnywhere, Category="Log", meta=(Units="cm"))
    float FinishHeight = 100.f;

    UPROPERTY(EditAnywhere, Category="Log", meta=(ClampMin="1.0", Units="kg"))
    float LogMass = 35.f;

    UPROPERTY(Transient, BlueprintReadOnly, Category="Log")
    TObjectPtr<AStaticMeshActor> ActiveLog;

private:
    void SpawnLog();
    float RemainingDelay = 0.f;
    float LogAge = 0.f;
};
