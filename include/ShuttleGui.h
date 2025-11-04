#pragma once

#include <memory>
#include <vector>

#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/window.h>

enum ShuttleMode
{
    eIsCreating,
    eIsUpdating
};

class ShuttleGui
{
public:
    ShuttleGui(wxWindow *parent, ShuttleMode)
        : mParent(parent)
    {
    }

    wxWindow *GetParent() const
    {
        return mParent;
    }

    void StartStatic(const wxString &)
    {
    }

    void StartNotebookPage(const wxString &)
    {
    }

    void EndNotebookPage()
    {
    }

    void EndStatic()
    {
    }

    void StartTwoColumn()
    {
    }

    void EndTwoColumn()
    {
    }

    wxStaticText *AddPrompt(const wxString &prompt)
    {
        auto control = std::make_unique<wxStaticText>(mParent, wxID_ANY, prompt);
        auto result = control.get();
        mOwnedWindows.push_back(std::move(control));
        return result;
    }

    wxTextCtrl *AddTextBox(const wxString &, const wxString &value, long)
    {
        auto control = std::make_unique<wxTextCtrl>(mParent, wxID_ANY, value);
        auto result = control.get();
        mOwnedWindows.push_back(std::move(control));
        return result;
    }

    void AddWindow(wxWindow *window)
    {
        mOwnedWindows.emplace_back(window);
    }

private:
    wxWindow *mParent;
    std::vector<std::unique_ptr<wxWindow>> mOwnedWindows;
};
