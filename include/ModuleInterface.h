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
};

using ModuleCommandCallback = std::function<void(AudacityProject &)>;
using PreferencesFactory = std::function<void(ShuttleGui &)>;

class ModuleManagerInterface
{
public:
    virtual ~ModuleManagerInterface() = default;

    virtual void RegisterModuleCommand(const wxString &, const wxString &, const wxString &, ModuleCommandCallback)
    {
    }

    virtual void RegisterPreferencesFactory(PreferencesFactory)
    {
    }
};

#define DECLARE_MODULE(name)
