#pragma once

#include <functional>
#include <wx/string.h>
#include "ShuttleGui.h"

class wxWindow;

struct RemoteWhisperSettingsData
{
    wxString serverUrl;
    wxString language;
};

class RemoteWhisperSettings
{
public:
    using Factory = std::function<void(ShuttleGui &)>;

    static RemoteWhisperSettingsData Load();
    static void Save(const RemoteWhisperSettingsData &data);

    static std::function<void(ShuttleGui &)> CreateFactory();

    static bool ShowDialog(wxWindow *parent);
};
