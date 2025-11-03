#include "RemoteWhisperClient.h"

#include <cstdio>
#include <wx/longlong.h>
#include <string>
#include <memory>
#include <sstream>
#include <vector>
#include <wx/ffile.h>

#include <curl/curl.h>

namespace
{
size_t CurlWriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    auto *str = static_cast<std::string *>(userp);
    const size_t total = size * nmemb;
    str->append(static_cast<const char *>(contents), total);
    return total;
}
}

RemoteWhisperClient::RemoteWhisperClient(wxString serverUrl, wxString language)
    : mServerUrl(std::move(serverUrl)),
      mLanguage(std::move(language))
{
}

RemoteWhisperResponse RemoteWhisperClient::Transcribe(const wxFileName &file) const
{
    RemoteWhisperResponse response;

    if (!file.FileExists())
    {
        response.errorMessage = wxString::Format(wxT("Audio file %s does not exist."), file.GetFullPath());
        return response;
    }

    CURL *curl = curl_easy_init();
    if (!curl)
    {
        response.errorMessage = wxT("Failed to initialize CURL.");
        return response;
    }

    std::string buffer;

    const auto baseUrlBuffer = mServerUrl.ToUTF8();
    const std::string baseUrl(baseUrlBuffer.data());
    curl_easy_setopt(curl, CURLOPT_URL, baseUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);

    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: audio/wav");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    wxString urlWithQuery = mServerUrl;
    wxString language = mLanguage.IsEmpty() ? wxT("en") : mLanguage;

    if (!file.GetFullName().IsEmpty())
    {
        urlWithQuery += wxString::Format(wxT("?filename=%s&language=%s"),
                                         file.GetFullName(), language);
        const auto urlBuffer = urlWithQuery.ToUTF8();
        const std::string url(urlBuffer.data());
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    }

    wxFFile inputFile(file.GetFullPath(), "rb");
    if (!inputFile.IsOpened())
    {
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        response.errorMessage = wxString::Format(wxT("Unable to open file %s."), file.GetFullPath());
        return response;
    }

    const wxFileOffset length = inputFile.Length();
    if (length <= 0)
    {
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        response.errorMessage = wxT("Audio file is empty.");
        return response;
    }

    std::string payload;
    payload.resize(static_cast<size_t>(length));
    if (inputFile.Read(payload.data(), payload.size()) != payload.size())
    {
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        response.errorMessage = wxT("Failed to read audio file contents.");
        return response;
    }

    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(payload.size()));
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.data());

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

    auto res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
    {
        response.errorMessage = wxString::Format(wxT("Transcription request failed: %s"),
                                                 wxString::FromUTF8(curl_easy_strerror(res)));
        return response;
    }

    response.ok = true;
    response.body = wxString::FromUTF8(buffer.c_str());
    return response;
}
