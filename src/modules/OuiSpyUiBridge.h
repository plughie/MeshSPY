#pragma once

#include <stdint.h>

bool ouiSpyUiSettingEnabled(uint8_t index);
void ouiSpyUiToggleSetting(uint8_t index);
void ouiSpyUiSaveSettings();
void ouiSpyUiSendTestAlert();
const char *ouiSpyUiAlertChannelName();
void ouiSpyUiCycleAlertChannel();
