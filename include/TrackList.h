#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "Track.h"

class AudacityProject;

class TrackList
{
public:
    using Storage = std::vector<std::unique_ptr<Track>>;

    class Iterator
    {
    public:
        explicit Iterator(Storage::iterator it) : mIt(it) {}

        Iterator &operator++()
        {
            ++mIt;
            return *this;
        }

        bool operator!=(const Iterator &other) const
        {
            return mIt != other.mIt;
        }

        Track *operator*() const
        {
            return mIt->get();
        }

    private:
        Storage::iterator mIt;
    };

    class ConstIterator
    {
    public:
        explicit ConstIterator(Storage::const_iterator it) : mIt(it) {}

        ConstIterator &operator++()
        {
            ++mIt;
            return *this;
        }

        bool operator!=(const ConstIterator &other) const
        {
            return mIt != other.mIt;
        }

        Track *operator*() const
        {
            return mIt->get();
        }

    private:
        Storage::const_iterator mIt;
    };

    Iterator begin() { return Iterator(mTracks.begin()); }
    Iterator end() { return Iterator(mTracks.end()); }
    ConstIterator begin() const { return ConstIterator(mTracks.begin()); }
    ConstIterator end() const { return ConstIterator(mTracks.end()); }

    void Add(Track *track)
    {
        mTracks.emplace_back(track);
    }

    std::pair<double, double> GetTimeBounds() const
    {
        return {0.0, 0.0};
    }

    static TrackList &Get(AudacityProject &project);

private:
    Storage mTracks;
};
