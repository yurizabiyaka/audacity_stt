#include "RemoteWhisperCommand.h"

#include <cstdint>
#include <algorithm>
#include <utility>
#include <memory>
#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/msgdlg.h>
#include <wx/ffile.h>
#include <wx/stdpaths.h>
#include <wx/utils.h>
#include <wx/translation.h>
#include <wx/intl.h>

#include "LabelTrack.h"
#include "Project.h"
#include "ProjectFileManager.h"
#include "ProjectSelection.h"
#include "ProjectWindow.h"
#include "ViewInfo.h"
#include "RemoteWhisperClient.h"
#include "RemoteWhisperJson.h"
#include "RemoteWhisperSettings.h"
#include "Track.h"
#include "TrackList.h"
#include "WaveTrack.h"

RemoteWhisperCommand::RemoteWhisperCommand() = default;
RemoteWhisperCommand::~RemoteWhisperCommand() = default;

void RemoteWhisperCommand::Run(AudacityProject &project)
{
    if (!EnsureProjectHasAudio(project))
    {
        wxMessageBox(wxGetTranslation(wxT("No audio tracks were found in the current project.")),
                     wxGetTranslation(wxT("Remote Whisper Transcription")), wxOK | wxICON_ERROR, project.GetProjectWindow());
        return;
    }

    wxFileName audioPath;
    double offset = 0.0;
    if (!ExportSelectionToWave(project, audioPath, offset))
    {
        wxMessageBox(wxGetTranslation(wxT("Unable to prepare audio for transcription.")),
                     wxGetTranslation(wxT("Remote Whisper Transcription")), wxOK | wxICON_ERROR, project.GetProjectWindow());
        return;
    }

    RemoteWhisperResult result;
    wxString error;
    if (!RequestTranscription(audioPath, result, error))
    {
        wxMessageBox(error,
                     wxGetTranslation(wxT("Remote Whisper Transcription")), wxOK | wxICON_ERROR, project.GetProjectWindow());
        return;
    }

    if (!CreateOrUpdateLabelTrack(project, result, offset))
    {
        wxMessageBox(wxGetTranslation(wxT("Unable to create label track for transcription results.")),
                     wxGetTranslation(wxT("Remote Whisper Transcription")), wxOK | wxICON_ERROR, project.GetProjectWindow());
        return;
    }
}

bool RemoteWhisperCommand::EnsureProjectHasAudio(AudacityProject &project)
{
    auto &trackList = TrackList::Get(project);
    bool hasAudio = false;
    for (auto track : trackList)
    {
        if (dynamic_cast<WaveTrack *>(track))
        {
            hasAudio = true;
            break;
        }
    }
    return hasAudio;
}

bool RemoteWhisperCommand::ExportSelectionToWave(AudacityProject &project, wxFileName &path, double &startTime)
{
    auto &selection = ProjectSelection::Get(project);
    startTime = selection.GetStartTime();
    double endTime = selection.GetEndTime();

    if (endTime <= startTime)
    {
        auto &viewInfo = ViewInfo::Get(project);
        startTime = viewInfo.selectedRegion.t0();
        endTime = viewInfo.selectedRegion.t1();
    }

    if (endTime <= startTime)
    {
        auto &trackList = TrackList::Get(project);
        auto range = trackList.GetTimeBounds();
        startTime = range.first;
        endTime = range.second;
    }

    if (endTime <= startTime)
        return false;

    auto &trackList = TrackList::Get(project);
    std::vector<WaveTrack *> waveTracks;
    for (auto track : trackList)
    {
        if (auto wave = dynamic_cast<WaveTrack *>(track))
            waveTracks.push_back(wave);
    }
    if (waveTracks.empty())
        return false;

    wxFileName tempFile;
    tempFile.AssignDir(wxStandardPaths::Get().GetTempDir());
    tempFile.SetName(wxString::Format("audacity-remote-whisper-%ld", wxGetLocalTime()));
    tempFile.SetExt("wav");

    const auto filePath = tempFile.GetFullPath();
    wxFFile outputFile(filePath, "wb");
    if (!outputFile.IsOpened())
        return false;

    const int sampleRate = static_cast<int>(waveTracks.front()->GetRate());
    const auto numChannels = waveTracks.size();

    // Write basic WAV header placeholder
    const uint32_t dataChunkSizePlaceholder = 0;
    const uint32_t byteRate = sampleRate * numChannels * sizeof(int16_t);
    const uint16_t blockAlign = numChannels * sizeof(int16_t);

    outputFile.Write("RIFF", 4);
    outputFile.Write(&dataChunkSizePlaceholder, sizeof(uint32_t));
    outputFile.Write("WAVE", 4);
    outputFile.Write("fmt ", 4);
    const uint32_t fmtChunkSize = 16;
    outputFile.Write(&fmtChunkSize, sizeof(uint32_t));
    const uint16_t audioFormat = 1; // PCM
    outputFile.Write(&audioFormat, sizeof(uint16_t));
    outputFile.Write(&numChannels, sizeof(uint16_t));
    outputFile.Write(&sampleRate, sizeof(uint32_t));
    outputFile.Write(&byteRate, sizeof(uint32_t));
    outputFile.Write(&blockAlign, sizeof(uint16_t));
    const uint16_t bitsPerSample = sizeof(int16_t) * 8;
    outputFile.Write(&bitsPerSample, sizeof(uint16_t));
    outputFile.Write("data", 4);
    outputFile.Write(&dataChunkSizePlaceholder, sizeof(uint32_t));

    const size_t bufferFrames = 8192;
    std::vector<float> buffer(bufferFrames);
    std::vector<int16_t> interleaved(bufferFrames * numChannels);
    uint32_t writtenSamples = 0;

    for (size_t channel = 0; channel < numChannels; ++channel)
    {
        auto &wave = *waveTracks[channel];
        auto reader = wave.CreateOptimizedReader(startTime, endTime);
        double pos = startTime;
        while (pos < endTime)
        {
            auto frames = reader->Read(buffer.data(), bufferFrames, pos, pos + bufferFrames / wave.GetRate());
            if (frames == 0)
                break;

            for (size_t i = 0; i < frames; ++i)
            {
                const float sample = std::clamp(buffer[i], -1.0f, 1.0f);
                const auto scaled = static_cast<int16_t>(sample * 32767.0f);
                interleaved[i * numChannels + channel] = scaled;
            }

            if (channel == numChannels - 1)
            {
                outputFile.Write(interleaved.data(), frames * numChannels * sizeof(int16_t));
                writtenSamples += frames * numChannels;
            }

            pos += static_cast<double>(frames) / wave.GetRate();
        }
    }

    const uint32_t dataChunkSize = writtenSamples * sizeof(int16_t);
    const uint32_t riffChunkSize = dataChunkSize + 36;
    outputFile.Seek(4);
    outputFile.Write(&riffChunkSize, sizeof(uint32_t));
    outputFile.Seek(40);
    outputFile.Write(&dataChunkSize, sizeof(uint32_t));
    outputFile.Flush();

    path = tempFile;
    return true;
}

bool RemoteWhisperCommand::RequestTranscription(const wxFileName &path, RemoteWhisperResult &result, wxString &error) const
{
    auto settings = RemoteWhisperSettings::Load();
    if (settings.serverUrl.empty())
    {
        error = wxGetTranslation(wxT("Remote Whisper server URL is not configured. Please update the plugin preferences."));
        return false;
    }

    RemoteWhisperClient client(settings.serverUrl, settings.language);
    auto response = client.Transcribe(path);
    if (!response.ok)
    {
        error = response.errorMessage;
        return false;
    }

    RemoteWhisperJsonParser parser;
    auto parseResult = parser.Parse(response.body);
    if (!parseResult.ok)
    {
        error = parseResult.errorMessage;
        return false;
    }

    result = std::move(parseResult.result);
    return true;
}

bool RemoteWhisperCommand::CreateOrUpdateLabelTrack(AudacityProject &project, const RemoteWhisperResult &result, double offsetSeconds)
{
    auto &trackList = TrackList::Get(project);
    LabelTrack *labelTrack = nullptr;

    for (auto track : trackList)
    {
        if (auto label = dynamic_cast<LabelTrack *>(track))
        {
            labelTrack = label;
            break;
        }
    }

    if (!labelTrack)
    {
        labelTrack = project.NewLabelTrack();
        if (!labelTrack)
            return false;
        trackList.Add(labelTrack);
    }

    labelTrack->Clear();

    for (const auto &utterance : result.utterances)
    {
        for (const auto &word : utterance.words)
        {
            const double start = offsetSeconds + word.start;
            const double end = offsetSeconds + word.end;
            labelTrack->AddLabel(start, end, word.text);
        }
    }

    return true;
}
