#pragma once

#include "Track.h"
#include <vector>
#include <wx/string.h>

struct LabelEntry
{
    double start = 0.0;
    double end = 0.0;
    wxString text;
};

class LabelTrack : public Track
{
public:
    void Clear()
    {
        mEntries.clear();
    }

    void AddLabel(double start, double end, const wxString &text)
    {
        mEntries.push_back({start, end, text});
    }

private:
    std::vector<LabelEntry> mEntries;
};
