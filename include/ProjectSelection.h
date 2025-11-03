#pragma once

class AudacityProject;

class ProjectSelection
{
public:
    static ProjectSelection &Get(AudacityProject &)
    {
        static ProjectSelection selection;
        return selection;
    }

    double GetStartTime() const { return 0.0; }
    double GetEndTime() const { return 0.0; }
};
