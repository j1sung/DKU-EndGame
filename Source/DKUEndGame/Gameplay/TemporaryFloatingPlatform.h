#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TemporaryFloatingPlatform.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

USTRUCT()
struct FTemporaryPlatformCycle
{
    GENERATED_BODY()

    /** 0: 대기, 1: 밟힘, 2: 붕괴, 3: 재생성 대기. */
    UPROPERTY()
    uint8 Phase = 0;
    UPROPERTY()
    double StartedAt = 0.;
    UPROPERTY()
    float HoldSeconds = 3.f;
};

/** 발을 올린 순간부터 한 번만 시간을 세는 서버 권한 임시 대피 발판. */
UCLASS()
class DKUENDGAME_API ATemporaryFloatingPlatform : public AActor
{
    GENERATED_BODY()

public:
    ATemporaryFloatingPlatform();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Floating Platform",
        meta=(ClampMin="0.1", Units="s", ToolTip="첫 플레이어가 밟은 뒤 붕괴하기까지의 시간. 다른 플레이어가 올라와도 초기화되지 않습니다."))
    float StandDuration = 3.f;

    UPROPERTY(EditDefaultsOnly, Category="Floating Platform|Assets")
    TObjectPtr<UStaticMesh> PlatformMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Floating Platform")
    TObjectPtr<UStaticMeshComponent> Platform;

private:
    UPROPERTY(ReplicatedUsing=OnRep_Cycle)
    FTemporaryPlatformCycle Cycle;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> SurfaceMaterials;

    UFUNCTION()
    void OnRep_Cycle();
    void SetPhase(uint8 Phase);
    void UpdateVisuals();
    bool SpawnSpaceClear() const;
    double ServerTime() const;
};
