#include "RemoteWhisperModule.h"

#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/intl.h>
#include <wx/app.h>
#include <wx/frame.h>

#include "Project.h"
#include <wx/translation.h>
#include "ModuleManager.h"
#include "ProjectManager.h"
#include "RemoteWhisperCommand.h"
#include "RemoteWhisperSettings.h"

// Static instance pointer for event handling
static RemoteWhisperModule* g_moduleInstance = nullptr;

// Menu IDs
static const int ID_REMOTE_WHISPER_TRANSCRIBE = wxNewId();
static const int ID_REMOTE_WHISPER_SETTINGS = wxNewId();

RemoteWhisperModule::RemoteWhisperModule()
{
    g_moduleInstance = this;
}

RemoteWhisperModule::~RemoteWhisperModule()
{
    if (g_moduleInstance == this)
        g_moduleInstance = nullptr;
}

wxString RemoteWhisperModule::GetSymbol()
{
    return wxT("remote_whisper_transcription");
}

wxString RemoteWhisperModule::GetDescription()
{
    return wxGetTranslation(wxT("Remote Whisper transcription using an external speech-to-text service"));
}

wxString RemoteWhisperModule::GetVendor()
{
    return wxT("OpenAI");
}

wxString RemoteWhisperModule::GetVersion()
{
    return wxT("1.0.0");
}

bool RemoteWhisperModule::Initialize()
{
    return true;
}

void RemoteWhisperModule::Terminate()
{
}

bool RemoteWhisperModule::RegisterModule(ModuleManagerInterface &moduleManager)
{
    // RegisterModuleCommand doesn't exist in Audacity 3.7
    // We'll add menu items in OnAppInitialized instead
    return true;
}

void RemoteWhisperModule::OnAppInitialized()
{
    // This is called when Audacity has finished initializing
    // Try to add our menu items to the Tools menu

    wxMessageBox(
        wxT("Remote Whisper STT Module loaded!\n\nNote: Automatic menu registration is not supported in this Audacity version.\n\nYou can still use the module through scripting or by manually calling the functions."),
        wxT("Remote Whisper Module"),
        wxOK | wxICON_INFORMATION
    );

    // TODO: We need access to Audacity's CommandManager to properly add menu items
    // This requires building against the full Audacity source code
}

// Required module entry points for Audacity
extern "C" {

#ifdef _WIN32
__declspec(dllexport)
#endif
ModuleInterface *ModuleDispatch(ModuleDispatchTypes type)
{
    switch (type)
    {
    case ModuleInitialize:
        if (!g_moduleInstance)
            g_moduleInstance = new RemoteWhisperModule();
        return g_moduleInstance;

    case AppInitialized:
        if (g_moduleInstance)
            g_moduleInstance->OnAppInitialized();
        return g_moduleInstance;

    case ModuleTerminate:
    case AppQuiting:
        if (g_moduleInstance)
        {
            delete g_moduleInstance;
            g_moduleInstance = nullptr;
        }
        return nullptr;

    default:
        return g_moduleInstance;
    }
}

#ifdef _WIN32
__declspec(dllexport)
#endif
int GetModuleVersion()
{
    return 1;
}

#ifdef _WIN32
__declspec(dllexport)
#endif
const wchar_t *GetVersionString()
{
    // Return the Audacity version this module is compatible with
    // Using 3.7.1 to match the headers we have
    return L"3.7.1";
}

} // extern "C"
