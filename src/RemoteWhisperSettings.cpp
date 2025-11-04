#include "RemoteWhisperSettings.h"

#include <wx/config.h>
#include <memory>
#include <wx/translation.h>
#include <wx/intl.h>
#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/button.h>
#include "PrefsPanel.h"

#include "ShuttleGui.h"

namespace
{
const wxString kConfigPath{wxT("RemoteWhisper")};
const wxString kServerUrlKey{wxT("ServerUrl")};
const wxString kLanguageKey{wxT("Language")};

class RemoteWhisperPreferencesPage : public PrefsPanel
{
public:
    explicit RemoteWhisperPreferencesPage(wxWindow *parent)
        : PrefsPanel(parent, wxID_ANY, wxGetTranslation(wxT("Remote Whisper")))
    {
        auto data = RemoteWhisperSettings::Load();

        ShuttleGui S(this, eIsCreating);
        S.StartNotebookPage(wxGetTranslation(wxT("Remote Whisper")));
        S.StartStatic(wxGetTranslation(wxT("Remote Whisper Service")));
        {
            S.StartTwoColumn();
            {
                S.AddPrompt(wxGetTranslation(wxT("Service URL:")));
                mServerUrl = S.AddTextBox({}, data.serverUrl, 0);

                S.AddPrompt(wxGetTranslation(wxT("Language (ISO code):")));
                mLanguage = S.AddTextBox({}, data.language, 0);
            }
            S.EndTwoColumn();
        }
        S.EndStatic();
        S.EndNotebookPage();
    }

    bool Commit() override
    {
        RemoteWhisperSettingsData data;
        data.serverUrl = mServerUrl->GetValue();
        data.language = mLanguage->GetValue();
        RemoteWhisperSettings::Save(data);
        return true;
    }

private:
    wxTextCtrl *mServerUrl = nullptr;
    wxTextCtrl *mLanguage = nullptr;
};
}

RemoteWhisperSettingsData RemoteWhisperSettings::Load()
{
    wxConfig config(wxT("audacity"));
    RemoteWhisperSettingsData data;
    data.serverUrl = config.Read(kConfigPath + wxT("/") + kServerUrlKey, wxEmptyString);
    data.language = config.Read(kConfigPath + wxT("/") + kLanguageKey, wxT("en"));
    return data;
}

void RemoteWhisperSettings::Save(const RemoteWhisperSettingsData &data)
{
    wxConfig config(wxT("audacity"));
    config.Write(kConfigPath + wxT("/") + kServerUrlKey, data.serverUrl);
    config.Write(kConfigPath + wxT("/") + kLanguageKey, data.language);
}

std::function<void(ShuttleGui &)> RemoteWhisperSettings::CreateFactory()
{
    return [](ShuttleGui &S) {
        auto window = S.GetParent();
        auto panel = std::make_unique<RemoteWhisperPreferencesPage>(window);
        S.AddWindow(panel.release());
    };
}

bool RemoteWhisperSettings::ShowDialog(wxWindow *parent)
{
    wxDialog dialog(parent, wxID_ANY, wxGetTranslation(wxT("Remote Whisper Settings")),
                    wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);

    auto *panel = new RemoteWhisperPreferencesPage(&dialog);

    auto *sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(panel, 1, wxEXPAND | wxALL, 10);

    auto *buttonSizer = dialog.CreateStdDialogButtonSizer(wxOK | wxCANCEL);
    if (buttonSizer)
        sizer->Add(buttonSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    dialog.SetSizerAndFit(sizer);

    if (dialog.ShowModal() == wxID_OK)
        return panel->Commit();

    return false;
}
