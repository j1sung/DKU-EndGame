#pragma once

#include "CoreMinimal.h"
#include "CFKnockdownTypes.generated.h"

// Local character axes at knockdown start: +X, -X, -Y, +Y.
UENUM(BlueprintType)
enum class ECFKnockdownDirection : uint8
{
    Forward,
    Back,
    Left,
    Right
};
