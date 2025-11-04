#pragma once

#include "ModuleInterface.h"
#include "ShuttleGui.h"

class wxWindow;

class RemoteWhisperModule final : public ModuleInterface
{
public:
    RemoteWhisperModule();
    ~RemoteWhisperModule() override;

    wxString GetSymbol() override;
    wxString GetDescription() override;
    wxString GetVendor() override;
    wxString GetVersion() override;

    bool Initialize() override;
    void Terminate() override;
    bool RegisterModule(ModuleManagerInterface &moduleManager) override;

    // Called when Audacity has finished initializing
    void OnAppInitialized();
};

ModuleInterface *NewRemoteWhisperModule();
