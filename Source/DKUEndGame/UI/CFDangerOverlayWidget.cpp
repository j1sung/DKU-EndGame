#include "UI/CFDangerOverlayWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

bool UCFDangerOverlayWidget::AddDesignerArrows(UObject* WidgetBlueprint, UMaterialInterface* Material)
{
#if WITH_EDITOR
    auto* Tree = WidgetBlueprint ? FindObject<UWidgetTree>(WidgetBlueprint, TEXT("WidgetTree")) : nullptr;
    auto* Canvas = Tree ? Cast<UCanvasPanel>(Tree->RootWidget) : nullptr;
    const FName Names[] = {TEXT("DangerArrowUp"), TEXT("DangerArrowRight"), TEXT("DangerArrowDown"), TEXT("DangerArrowLeft")};
    if (!Canvas || !Material) return false;
    for (FName Name : Names) if (Tree->FindWidget(Name)) return false;
    WidgetBlueprint->Modify(); Tree->Modify(); Canvas->Modify();
    const FVector2D Anchors[] = {{.5,0}, {1,.5}, {.5,1}, {0,.5}};
    const FVector2D Positions[] = {{0,36}, {-36,0}, {0,-36}, {36,0}};
    for (int32 Index=0; Index<4; ++Index)
    {
        auto* Arrow = Tree->ConstructWidget<UImage>(UImage::StaticClass(), Names[Index]);
        Arrow->bIsVariable = true;
        Arrow->SetBrushFromMaterial(Material);
        Arrow->SetVisibility(ESlateVisibility::HitTestInvisible);
        Arrow->SetRenderTransformPivot(FVector2D(.5,.5));
        Arrow->SetRenderTransformAngle(Index * 90.f);
        auto* ImageSlot = Canvas->AddChildToCanvas(Arrow);
        ImageSlot->SetAnchors(FAnchors(Anchors[Index].X, Anchors[Index].Y));
        ImageSlot->SetAlignment(Anchors[Index]);
        ImageSlot->SetPosition(Positions[Index]);
        ImageSlot->SetSize(FVector2D(64,64));
        ImageSlot->SetZOrder(1);
    }
    WidgetBlueprint->MarkPackageDirty();
    return true;
#else
    return false;
#endif
}

bool UCFDangerOverlayWidget::CreateDesignerContent(UObject* WidgetBlueprint, UMaterialInterface* Material)
{
#if WITH_EDITOR
    UWidgetTree* Tree = WidgetBlueprint ? FindObject<UWidgetTree>(WidgetBlueprint, TEXT("WidgetTree")) : nullptr;
    if (!Tree || Tree->RootWidget || !Material) return false;
    WidgetBlueprint->Modify();
    Tree->Modify();
    auto* Canvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DangerCanvas"));
    auto* Image = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DangerVignette"));
    Tree->RootWidget = Canvas;
    Image->bIsVariable = true;
    Image->SetBrushFromMaterial(Material);
    Canvas->SetVisibility(ESlateVisibility::HitTestInvisible);
    Image->SetVisibility(ESlateVisibility::HitTestInvisible);
    auto* ImageSlot = Canvas->AddChildToCanvas(Image);
    ImageSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
    ImageSlot->SetOffsets(FMargin(0.f));
    ImageSlot->SetAlignment(FVector2D::ZeroVector);
    WidgetBlueprint->MarkPackageDirty();
    return true;
#else
    return false;
#endif
}

void UCFDangerOverlayWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetVisibility(ESlateVisibility::HitTestInvisible);
    UImage* Arrows[] = {DangerArrowUp, DangerArrowRight, DangerArrowDown, DangerArrowLeft};
    for (int32 Index=0; Index<4; ++Index)
        if (Arrows[Index]) ArrowBaseTranslation[Index] = Arrows[Index]->GetRenderTransform().Translation;
    SetDangerIntensity(0.f);
}

void UCFDangerOverlayWidget::SetDangerIntensity(float Intensity)
{
    DangerIntensity = FMath::Clamp(Intensity, 0.f, 1.f);
    SetRenderOpacity(DangerIntensity);
    if (DangerIntensity <= 0.f) UpdateArrows(0.f);
}

void UCFDangerOverlayWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    UpdateArrows(DeltaTime);
}

void UCFDangerOverlayWidget::UpdateArrows(float DeltaTime)
{
    int32 Direction = -1;
    auto* Player = GetOwningPlayer();
    const auto* Character = Player ? Cast<ACharacter>(Player->GetPawn()) : nullptr;
    if (DangerIntensity > 0.f && Character && Character->IsLocallyControlled() && Character->GetMesh())
    {
        // 균형 표시로 실제 기울어진 메시의 위쪽 축을 읽는다. 이동/균형 판정 코드는 변경하지 않는다.
        FVector Lean = Character->GetMesh()->GetUpVector();
        Lean.Z = 0.f;
        if (!Lean.IsNearlyZero(.001f))
        {
            FVector CameraLocation; FRotator CameraRotation;
            Player->GetPlayerViewPoint(CameraLocation, CameraRotation);
            const FRotationMatrix CameraAxes(FRotator(0.f, CameraRotation.Yaw, 0.f));
            const float Forward = FVector::DotProduct(Lean, CameraAxes.GetUnitAxis(EAxis::X));
            const float Right = FVector::DotProduct(Lean, CameraAxes.GetUnitAxis(EAxis::Y));
            Direction = FMath::Abs(Forward) >= FMath::Abs(Right)
                ? (Forward >= 0.f ? 0 : 2) : (Right >= 0.f ? 1 : 3);
        }
    }
    if (Direction != ActiveDirection) ArrowAge = 0.f;
    ActiveDirection = Direction;
    ArrowAge += FMath::Max(0.f, DeltaTime);
    const float Travel = FMath::Max(0.f, ArrowTravelPixels) * .5f *
        (1.f - FMath::Cos(2.f * PI * ArrowAge / FMath::Max(.1f, ArrowCycleSeconds)));
    const FVector2D Outward[] = {{0,-1}, {1,0}, {0,1}, {-1,0}};
    UImage* Arrows[] = {DangerArrowUp, DangerArrowRight, DangerArrowDown, DangerArrowLeft};
    for (int32 Index=0; Index<4; ++Index)
    {
        if (!Arrows[Index]) continue;
        Arrows[Index]->SetVisibility(Index == Direction ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
        Arrows[Index]->SetRenderTranslation(ArrowBaseTranslation[Index] + Outward[Index] * (Index == Direction ? Travel : 0.f));
    }
}
