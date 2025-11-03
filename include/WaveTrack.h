#pragma once

#include "Track.h"
#include <memory>

class WaveTrackReader
{
public:
    virtual ~WaveTrackReader() = default;
    virtual size_t Read(float *, size_t, double, double)
    {
        return 0;
    }
};

class WaveTrack : public Track
{
public:
    double GetRate() const
    {
        return 44100.0;
    }

    std::unique_ptr<WaveTrackReader> CreateOptimizedReader(double, double)
    {
        return std::make_unique<WaveTrackReader>();
    }
};
