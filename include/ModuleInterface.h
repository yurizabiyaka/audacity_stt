#pragma once

#include <functional>
#include <wx/string.h>

class AudacityProject;
class ShuttleGui;

class ModuleManagerInterface;

enum ModuleDispatchTypes
{
    ModuleInitialize,
    ModuleTerminate,
    AppInitialized,
    AppQuiting,
    ProjectInitialized,
    ProjectClosing
};

class ModuleInterface
{
public:
    virtual ~ModuleInterface() = default;

    virtual wxString GetSymbol() { return wxString(); }
    virtual wxString GetDescription() { return wxString(); }
    virtual wxString GetVendor() { return wxString(); }
    virtual wxString GetVersion() { return wxString(); }

    virtual bool Initialize() { return true; }
    virtual void Terminate() {}
    virtual bool RegisterModule(ModuleManagerInterface &) { return true; }

    // New: allow modules to handle AppInitialized to add menu items
    virtual void OnAppInitialized() {}
};

using ModuleCommandCallback = std::function<void(AudacityProject &)>;
using PreferencesFactory = std::function<void(ShuttleGui &)>;

// This is a stub - Audacity will provide the real implementation at runtime
class ModuleManagerInterface
{
public:
    virtual ~ModuleManagerInterface() = default;

    // NOTE: These are stubs. The real implementation will be provided
    // by Audacity at runtime, but we can't rely on them working.
    // Instead, we'll use direct menu manipulation in OnAppInitialized.
    virtual void RegisterModuleCommand(const wxString &, const wxString &, const wxString &, ModuleCommandCallback)
    {
    }

    virtual void RegisterPreferencesFactory(PreferencesFactory)
    {
    }
};

#define DECLARE_MODULE(name)
