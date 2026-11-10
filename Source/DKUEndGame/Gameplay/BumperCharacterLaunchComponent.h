#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BumperCharacterLaunchComponent.generated.h"

class ACharacter;
class UStaticMeshComponent;

/** Launches characters touching the top of the bumper during its upward stroke. */
UCLASS(ClassGroup=(Movement), meta=(BlueprintSpawnableComponent))
class DKUENDGAME_API UBumperCharacterLaunchComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBumperCharacterLaunchComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bumper|Launch", meta=(ClampMin="0.0", Units="cm/s"))
    float LaunchSpeed = 900.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Bumper|Launch", meta=(ClampMin="0.0", Units="cm"))
    float SurfaceTolerance = 45.f;

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UStaticMeshComponent> BumperMesh;

    TSet<TWeakObjectPtr<ACharacter>> LaunchedCharacters;
    float PreviousTop = 0.f;
    bool bWasRising = false;
};
