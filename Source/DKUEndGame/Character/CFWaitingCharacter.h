#pragma once

#include "CoreMinimal.h"
#include "DKUEndGameCharacter.h"
#include "CFWaitingCharacter.generated.h"

/** Grounded walking pawn for the arena's waiting phase. */
UCLASS()
class DKUENDGAME_API ACFWaitingCharacter : public ADKUEndGameCharacter
{
    GENERATED_BODY()
public:
    ACFWaitingCharacter();

protected:
    virtual bool CanJumpInternal_Implementation() const override;
};
