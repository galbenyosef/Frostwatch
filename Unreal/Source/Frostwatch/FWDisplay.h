#pragma once
#include "CoreMinimal.h"
void FWPreviewDisplayMode();
void FWPreviewDisplayResolution();
void FWKeepDisplay();
void FWRevertDisplay();
void FWUpdateDisplayPreview();
int FWDisplaySecondsRemaining();
FString FWDisplayModeName();
void FWCheckDisplaySettings(TFunctionRef<void(const FString&,bool)> Check);
void FWRunDisplayTests();
