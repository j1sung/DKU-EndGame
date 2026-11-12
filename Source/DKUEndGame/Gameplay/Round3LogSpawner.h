#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Round3LogSpawner.generated.h"

class AStaticMeshActor;
class UStaticMeshComponent;
class UPrimitiveComponent;
class ACharacter;

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

    /** 공유 스포너의 다른 맵에는 영향을 주지 않고 필요한 인스턴스에서만 켠다. */
    UPROPERTY(EditAnywhere, Category="Log|Player Impact")
    bool bEnableRollingPlayerPush = false;

    /** 이 속도 이상으로 플레이어 쪽으로 굴러올 때만 추가 밀침을 적용한다. */
    UPROPERTY(EditAnywhere, Category="Log|Player Impact", meta=(ClampMin="1.0", Units="cm/s"))
    float MinimumRollingPushSpeed = 120.f;

    UPROPERTY(EditAnywhere, Category="Log|Player Impact", meta=(ClampMin="0.0"))
    float PlayerPushMultiplier = 1.15f;

    UPROPERTY(EditAnywhere, Category="Log|Player Impact", meta=(ClampMin="0.0", Units="cm/s"))
    float MaximumPlayerPushSpeed = 650.f;

    /** 고속 접촉에서만 진행 방향 운동량 일부를 유지한다. 정지한 통나무는 계속 밀 수 있다. */
    UPROPERTY(EditAnywhere, Category="Log|Player Impact", meta=(ClampMin="0.0", ClampMax="1.0"))
    float RollingMomentumRetention = .8f;

    UPROPERTY(Transient, BlueprintReadOnly, Category="Log")
    TObjectPtr<AStaticMeshActor> ActiveLog;

private:
    void SpawnLog();
    UFUNCTION()
    void OnLogHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);
    FVector PreviousLogVelocity = FVector::ZeroVector;
    TMap<TWeakObjectPtr<ACharacter>, float> LastPlayerPushTimes;
    float RemainingDelay = 0.f;
    float LogAge = 0.f;
};
