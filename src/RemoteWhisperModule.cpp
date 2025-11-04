#include "RemoteWhisperModule.h"

#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/intl.h>

#include "Project.h"
#include <wx/translation.h>
#include "ModuleManager.h"
#include "ProjectManager.h"
#include "RemoteWhisperCommand.h"
#include "RemoteWhisperSettings.h"

RemoteWhisperModule::RemoteWhisperModule() = default;
RemoteWhisperModule::~RemoteWhisperModule() = default;

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
    auto onCommand = [] (AudacityProject &project) {
        RemoteWhisperCommand command;
        command.Run(project);
    };

    moduleManager.RegisterModuleCommand(
        wxT("Tools"),
        wxT("remote-whisper-transcription"),
        wxGetTranslation(wxT("Remote Whisper Transcription...")),
        onCommand
    );

    moduleManager.RegisterPreferencesFactory(
        RemoteWhisperSettings::CreateFactory()
    );

    return true;
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
        return new RemoteWhisperModule();
    default:
        return nullptr;
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
    return L"3.7.5";
}

} // extern "C"
