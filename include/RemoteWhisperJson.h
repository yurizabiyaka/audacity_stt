#pragma once

#include <wx/string.h>

#include "RemoteWhisperCommand.h"

struct RemoteWhisperParseResult
{
    bool ok = false;
    RemoteWhisperResult result;
    wxString errorMessage;
};

class RemoteWhisperJsonParser
{
public:
    RemoteWhisperParseResult Parse(const wxString &json) const;
};
