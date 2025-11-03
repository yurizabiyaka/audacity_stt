#pragma once

#include <memory>
#include <vector>
#include <vector>
#include <wx/filename.h>
#include <wx/string.h>

class AudacityProject;
struct RemoteWhisperWord;
struct RemoteWhisperResult;

class RemoteWhisperCommand
{
public:
    RemoteWhisperCommand();
    ~RemoteWhisperCommand();

    void Run(AudacityProject &project);

private:
    bool EnsureProjectHasAudio(AudacityProject &project);
    bool ExportSelectionToWave(AudacityProject &project, wxFileName &path, double &startTime);
    bool RequestTranscription(const wxFileName &path, RemoteWhisperResult &result, wxString &error) const;
    bool CreateOrUpdateLabelTrack(AudacityProject &project, const RemoteWhisperResult &result, double offsetSeconds);
};

struct RemoteWhisperWord
{
    double start = 0.0;
    double end = 0.0;
    wxString text;
};

struct RemoteWhisperUtterance
{
    wxString transcript;
    std::vector<RemoteWhisperWord> words;
};

struct RemoteWhisperResult
{
    std::vector<RemoteWhisperUtterance> utterances;
};
