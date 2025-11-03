#pragma once

#include <memory>

#include <wx/window.h>

#include "LabelTrack.h"
#include "TrackList.h"
#include "WaveTrack.h"

class AudacityProject
{
public:
    AudacityProject()
    {
        mTrackList.Add(new WaveTrack());
    }

    wxWindow *GetProjectWindow() const
    {
        return mWindow;
    }

    void SetProjectWindow(wxWindow *window)
    {
        mWindow = window;
    }

    LabelTrack *NewLabelTrack()
    {
        return new LabelTrack();
    }

    TrackList &GetTrackList()
    {
        return mTrackList;
    }

private:
    wxWindow *mWindow = nullptr;
    TrackList mTrackList;
};

inline TrackList &TrackList::Get(AudacityProject &project)
{
    return project.GetTrackList();
}
