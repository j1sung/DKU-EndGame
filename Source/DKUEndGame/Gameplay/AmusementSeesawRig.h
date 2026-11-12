#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AmusementSeesawRig.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/** 서버 시간으로 같은 궤적을 재생하는 Round4의 교대식 놀이기구. 물리 시뮬레이션 시소는 아니다. */
UCLASS()
class DKUENDGAME_API AAmusementSeesawRig : public AActor
{
    GENERATED_BODY()

public:
    AAmusementSeesawRig();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Seesaw")
    TObjectPtr<UStaticMeshComponent> Deck;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Seesaw|Assets")
    TObjectPtr<UStaticMesh> DeckMesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Seesaw|Assets")
    TObjectPtr<UStaticMesh> RodMesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Seesaw|Assets")
    TObjectPtr<UStaticMesh> HeadMesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Seesaw|Assets")
    TObjectPtr<UStaticMesh> BridgeMesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Seesaw|Assets")
    TObjectPtr<UStaticMesh> GateMesh;

    /** 한쪽 작동 시작부터 다음 반대쪽 작동 시작까지의 간격. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Seesaw|Timing", meta=(ClampMin="1.0", Units="s"))
    float PushInterval = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Seesaw|Timing", meta=(ClampMin="0.0", Units="s"))
    float InitialDelay = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Seesaw|Timing", meta=(ClampMin="0.1", Units="s"))
    float TiltTravelTime = 1.75f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Seesaw|Timing", meta=(ClampMin="0.0", Units="s"))
    float TiltHoldTime = 2.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Seesaw|Timing", meta=(ClampMin="0.1", Units="s"))
    float BoardingTravelTime = .75f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Seesaw", meta=(ClampMin="0.0", ClampMax="45.0", Units="deg"))
    float MaximumTiltAngle = 45.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Seesaw")
    float CurrentTiltAngle = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Seesaw")
    bool bBoardingOpen = true;

private:
    float BoardingClosureAlpha = 0.f;
    UPROPERTY(VisibleAnywhere)
    TArray<TObjectPtr<UStaticMeshComponent>> Rods;
    UPROPERTY(VisibleAnywhere)
    TArray<TObjectPtr<UStaticMeshComponent>> Heads;
    UPROPERTY(VisibleAnywhere)
    TArray<TObjectPtr<UStaticMeshComponent>> Bridges;
    UPROPERTY(VisibleAnywhere)
    TArray<TObjectPtr<UStaticMeshComponent>> Gates;
    UPROPERTY(Replicated)
    double CycleStartServerTime = -1.;

    void ApplyPose(float Angle, float PusherExtension, float PushingSide = 0.f);
    double ServerTime() const;
};
