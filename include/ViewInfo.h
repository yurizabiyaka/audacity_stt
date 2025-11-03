#pragma once

class AudacityProject;

class ViewInfo
{
public:
    class SelectionRegion
    {
    public:
        double t0() const { return 0.0; }
        double t1() const { return 0.0; }
    };

    static ViewInfo &Get(AudacityProject &)
    {
        static ViewInfo info;
        return info;
    }

    SelectionRegion selectedRegion;
};
