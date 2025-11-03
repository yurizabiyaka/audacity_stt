#include "RemoteWhisperSettings.h"

#include <wx/config.h>
#include <memory>
#include <wx/translation.h>
#include <wx/intl.h>
#include "PrefsPanel.h"

#include "ShuttleGui.h"

namespace
{
constexpr auto kConfigPath = wxT("RemoteWhisper");
constexpr auto kServerUrlKey = wxT("ServerUrl");
constexpr auto kLanguageKey = wxT("Language");

class RemoteWhisperPreferencesPage : public PrefsPanel
{
public:
    explicit RemoteWhisperPreferencesPage(wxWindow *parent)
        : PrefsPanel(parent, wxID_ANY, _( "Remote Whisper" ))
    {
        auto data = RemoteWhisperSettings::Load();

        ShuttleGui S(this, eIsCreating);
        S.StartStatic(_( "Remote Whisper Service" ));
        {
            S.StartTwoColumn();
            {
                S.AddPrompt(_( "Service URL:" ));
                mServerUrl = S.AddTextBox({}, data.serverUrl, 0);

                S.AddPrompt(_( "Language (ISO code):" ));
                mLanguage = S.AddTextBox({}, data.language, 0);
            }
            S.EndTwoColumn();
        }
        S.EndStatic();
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
    data.serverUrl = config.Read(kConfigPath + wxT("/" ) + kServerUrlKey, wxEmptyString);
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
