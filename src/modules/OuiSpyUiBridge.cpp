#include "modules/OuiSpyUiBridge.h"

#include "modules/OuiSpyModule.h"

bool ouiSpyUiSettingEnabled(uint8_t index)
{
    return ouiSpyModule != nullptr && ouiSpyModule->uiSettingEnabled(index);
}

void ouiSpyUiToggleSetting(uint8_t index)
{
    if (ouiSpyModule != nullptr)
        ouiSpyModule->uiToggleSetting(index);
}

void ouiSpyUiSaveSettings()
{
    if (ouiSpyModule != nullptr)
        ouiSpyModule->uiSaveSettings();
}

void ouiSpyUiSendTestAlert()
{
    if (ouiSpyModule != nullptr)
        ouiSpyModule->uiSendTestAlert();
}

const char *ouiSpyUiAlertChannelName()
{
    return ouiSpyModule != nullptr ? ouiSpyModule->uiAlertChannelName() : "unavailable";
}

void ouiSpyUiCycleAlertChannel()
{
    if (ouiSpyModule != nullptr)
        ouiSpyModule->uiCycleAlertChannel();
}
