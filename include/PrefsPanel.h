#pragma once

#include <wx/panel.h>

class PrefsPanel : public wxPanel
{
public:
    PrefsPanel(wxWindow *parent, wxWindowID id, const wxString &title)
        : wxPanel(parent, id)
    {
        SetName(title);
    }

    virtual bool Commit()
    {
        return true;
    }
};
