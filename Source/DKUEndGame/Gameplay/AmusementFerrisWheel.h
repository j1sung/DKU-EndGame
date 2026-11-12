#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AmusementFerrisWheel.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/** 회전 링과 수평을 유지하는 탑승칸. 고정 받침대는 별도 메시다. */
UCLASS()
class DKUENDGAME_API AAmusementFerrisWheel : public AActor
{
    GENERATED_BODY()

public:
    AAmusementFerrisWheel();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ferris Wheel|Assets")
    TObjectPtr<UStaticMesh> WheelMesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ferris Wheel|Assets")
    TObjectPtr<UStaticMesh> CabinMesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ferris Wheel", meta=(Units="deg/s"))
    float RotationSpeed = 3.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ferris Wheel", meta=(ClampMin="1.0", Units="cm"))
    float Radius = 2000.f;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> Wheel;
    UPROPERTY(VisibleAnywhere)
    TArray<TObjectPtr<UStaticMeshComponent>> Cabins;
    void ApplyPose(float Angle);
};
