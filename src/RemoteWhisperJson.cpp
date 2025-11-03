#include "RemoteWhisperJson.h"

#include <wx/translation.h>
#include <cstdlib>
#include <utility>
#include <string>
#include <cctype>
#include <stack>

namespace
{
size_t FindMatching(const std::string &text, size_t start, char open, char close)
{
    if (start >= text.size() || text[start] != open)
        return std::string::npos;
    std::stack<char> stack;
    for (size_t i = start; i < text.size(); ++i)
    {
        char c = text[i];
        if (c == open)
            stack.push(c);
        else if (c == close)
        {
            stack.pop();
            if (stack.empty())
                return i;
        }
        else if (c == '"')
        {
            ++i;
            while (i < text.size())
            {
                char d = text[i];
                if (d == '\\')
                {
                    ++i;
                    if (i >= text.size())
                        break;
                }
                else if (d == '"')
                {
                    break;
                }
                ++i;
            }
        }
    }
    return std::string::npos;
}

size_t FindStringEnd(const std::string &text, size_t start)
{
    for (size_t i = start; i < text.size(); ++i)
    {
        if (text[i] == '\\')
        {
            ++i;
            continue;
        }
        if (text[i] == '"')
            return i;
    }
    return std::string::npos;
}

std::string DecodeJsonString(const std::string &value)
{
    std::string result;
    result.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i)
    {
        char c = value[i];
        if (c == '\\' && i + 1 < value.size())
        {
            char next = value[++i];
            switch (next)
            {
            case '"': result.push_back('"'); break;
            case '\\': result.push_back('\\'); break;
            case '/': result.push_back('/'); break;
            case 'b': result.push_back('\b'); break;
            case 'f': result.push_back('\f'); break;
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            case 'u':
                if (i + 4 < value.size())
                {
                    auto hex = value.substr(i + 1, 4);
                    unsigned code = std::strtoul(hex.c_str(), nullptr, 16);
                    if (code <= 0x7F)
                        result.push_back(static_cast<char>(code));
                    i += 4;
                }
                break;
            default:
                result.push_back(next);
                break;
            }
        }
        else
        {
            result.push_back(c);
        }
    }
    return result;
}

double ParseNumber(const std::string &text)
{
    try
    {
        return std::stod(text);
    }
    catch (...)
    {
        return 0.0;
    }
}
}

RemoteWhisperParseResult RemoteWhisperJsonParser::Parse(const wxString &json) const
{
    RemoteWhisperParseResult result;
    std::string text = json.ToStdString();
    size_t pos = 0;

    while ((pos = text.find("\"words\"", pos)) != std::string::npos)
    {
        size_t arrayStart = text.find('[', pos);
        if (arrayStart == std::string::npos)
            break;
        size_t arrayEnd = FindMatching(text, arrayStart, '[', ']');
        if (arrayEnd == std::string::npos)
            break;

        RemoteWhisperUtterance utterance;

        size_t transcriptPos = text.rfind("\"transcript\"", pos);
        if (transcriptPos != std::string::npos)
        {
            size_t colon = text.find(':', transcriptPos);
            if (colon != std::string::npos)
            {
                size_t quoteStart = text.find('"', colon);
                if (quoteStart != std::string::npos)
                {
                    size_t quoteEnd = FindStringEnd(text, quoteStart + 1);
                    if (quoteEnd != std::string::npos)
                    {
                        utterance.transcript = wxString::FromUTF8(
                            DecodeJsonString(text.substr(quoteStart + 1, quoteEnd - quoteStart - 1)).c_str());
                    }
                }
            }
        }

        auto arrayBody = text.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
        size_t cursor = 0;
        while ((cursor = arrayBody.find("\"start\"", cursor)) != std::string::npos)
        {
            size_t colon = arrayBody.find(':', cursor);
            if (colon == std::string::npos)
                break;
            size_t comma = arrayBody.find(',', colon);
            if (comma == std::string::npos)
                break;
            auto startValue = arrayBody.substr(colon + 1, comma - colon - 1);
            RemoteWhisperWord word;
            word.start = ParseNumber(startValue);

            size_t endPos = arrayBody.find("\"end\"", comma);
            if (endPos == std::string::npos)
                break;
            colon = arrayBody.find(':', endPos);
            if (colon == std::string::npos)
                break;
            comma = arrayBody.find(',', colon);
            if (comma == std::string::npos)
                break;
            auto endValue = arrayBody.substr(colon + 1, comma - colon - 1);
            word.end = ParseNumber(endValue);

            size_t textPos = arrayBody.find("\"text\"", comma);
            if (textPos == std::string::npos)
                break;
            colon = arrayBody.find(':', textPos);
            if (colon == std::string::npos)
                break;
            size_t quoteStart = arrayBody.find('"', colon);
            if (quoteStart == std::string::npos)
                break;
            size_t quoteEnd = FindStringEnd(arrayBody, quoteStart + 1);
            if (quoteEnd == std::string::npos)
                break;
            word.text = wxString::FromUTF8(
                DecodeJsonString(arrayBody.substr(quoteStart + 1, quoteEnd - quoteStart - 1)).c_str());

            utterance.words.push_back(std::move(word));
            cursor = quoteEnd + 1;
        }

        if (!utterance.words.empty())
            result.result.utterances.push_back(std::move(utterance));

        pos = arrayEnd + 1;
    }

    if (result.result.utterances.empty())
    {
        result.errorMessage = _( "The transcription response did not contain any words." );
        result.ok = false;
        return result;
    }

    result.ok = true;
    return result;
}
