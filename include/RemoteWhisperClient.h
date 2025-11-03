#pragma once

#include <wx/filename.h>
#include <wx/string.h>

struct RemoteWhisperResponse
{
    bool ok = false;
    wxString errorMessage;
    wxString body;
};

class RemoteWhisperClient
{
public:
    RemoteWhisperClient(wxString serverUrl, wxString language);

    RemoteWhisperResponse Transcribe(const wxFileName &file) const;

private:
    wxString mServerUrl;
    wxString mLanguage;
};
