#include "UI/CFJumpChargeWidget.h"
#include "Character/ChickenCharacter.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"

void UCFJumpChargeWidget::SetCharacter(AChickenCharacter* InCharacter)
{
    Character = InCharacter;
}

void UCFJumpChargeWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetVisibility(ESlateVisibility::HitTestInvisible);
    auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Canvas;

    // Screen-space padding places the bar beside the pawn regardless of camera yaw.
    auto* Frame = WidgetTree->ConstructWidget<UBorder>();
    Frame->SetBrush(FSlateColorBrush(FLinearColor::White));
    Frame->SetBrushColor(FLinearColor(.85f, .88f, .92f, .95f));
    Frame->SetPadding(FMargin(2.f));
    auto* FrameSlot = Canvas->AddChildToCanvas(Frame);
    FrameSlot->SetPosition(FVector2D(136.f, 8.f));
    FrameSlot->SetSize(FVector2D(18.f, 144.f));

    ChargeBar = WidgetTree->ConstructWidget<UProgressBar>();
    FProgressBarStyle Style;
    Style.SetBackgroundImage(FSlateColorBrush(FLinearColor(.025f, .03f, .04f, .85f)));
    Style.SetFillImage(FSlateColorBrush(FLinearColor::White));
    ChargeBar->SetWidgetStyle(Style);
    ChargeBar->SetBarFillType(EProgressBarFillType::BottomToTop);
    ChargeBar->SetBarFillStyle(EProgressBarFillStyle::Scale);
    ChargeBar->SetBorderPadding(FVector2D::ZeroVector);
    ChargeBar->SetPercent(0.f);
    Frame->SetContent(ChargeBar);
}

void UCFJumpChargeWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    const auto* Pawn = Character.Get();
    if (!ChargeBar) return;
    const float Charge = Pawn && Pawn->IsChargingJump() ? Pawn->GetNormalizedJumpCharge() : 0.f;
    ChargeBar->SetPercent(Charge);
    ChargeBar->SetFillColorAndOpacity(FMath::Lerp(
        FLinearColor(1.f, .65f, .06f), FLinearColor(.95f, .06f, .02f), Charge));
}
