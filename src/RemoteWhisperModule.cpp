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
    return _( "Remote Whisper transcription using an external speech-to-text service" );
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
        _( "Remote Whisper Transcription..." ),
        onCommand
    );

    moduleManager.RegisterPreferencesFactory(
        RemoteWhisperSettings::CreateFactory()
    );

    return true;
}

ModuleInterface *NewRemoteWhisperModule()
{
    return new RemoteWhisperModule();
}

DECLARE_MODULE(RemoteWhisperModule);
