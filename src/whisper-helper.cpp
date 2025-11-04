/**********************************************************************

  Audacity Remote Whisper STT - Helper Executable

  This is a standalone command-line tool that:
  1. Reads a WAV file
  2. Sends it to a Whisper-compatible STT service via HTTP POST
  3. Parses the JSON response
  4. Outputs Audacity label format to stdout

  Usage: whisper-helper.exe <audio-file.wav> <server-url> <language>
  Example: whisper-helper.exe audio.wav http://ai1:443/v1/files en

  Output format (tab-separated):
  0.000000	0.500000	Hello
  0.500000	1.000000	world

**********************************************************************/

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <curl/curl.h>

// Callback for CURL to write response data
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    std::string *str = static_cast<std::string*>(userp);
    size_t total = size * nmemb;
    str->append(static_cast<const char*>(contents), total);
    return total;
}

// Simple JSON word parser - extracts start, end, text from JSON
struct Word {
    double start;
    double end;
    std::string text;
};

// Parse a number from JSON
double parseNumber(const std::string &str, size_t &pos)
{
    while (pos < str.size() && std::isspace(str[pos])) pos++;
    size_t start = pos;
    while (pos < str.size() && (std::isdigit(str[pos]) || str[pos] == '.' || str[pos] == '-' || str[pos] == 'e' || str[pos] == 'E' || str[pos] == '+'))
        pos++;
    return std::atof(str.substr(start, pos - start).c_str());
}

// Parse a JSON string value
std::string parseString(const std::string &str, size_t &pos)
{
    std::string result;
    while (pos < str.size() && str[pos] != '"') pos++; // Find opening quote
    pos++; // Skip opening quote

    while (pos < str.size() && str[pos] != '"')
    {
        if (str[pos] == '\\' && pos + 1 < str.size())
        {
            pos++;
            switch (str[pos])
            {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'r': result += '\r'; break;
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                default: result += str[pos]; break;
            }
        }
        else
        {
            result += str[pos];
        }
        pos++;
    }
    pos++; // Skip closing quote
    return result;
}

// Parse words from JSON response
std::vector<Word> parseWords(const std::string &json)
{
    std::vector<Word> words;

    // Find "words" array
    size_t pos = json.find("\"words\"");
    if (pos == std::string::npos)
        return words;

    // Find the opening bracket of the array
    pos = json.find('[', pos);
    if (pos == std::string::npos)
        return words;

    pos++; // Move past '['

    // Parse each word object
    while (pos < json.size())
    {
        // Skip whitespace
        while (pos < json.size() && std::isspace(json[pos])) pos++;

        if (json[pos] == ']') break; // End of array
        if (json[pos] != '{') { pos++; continue; } // Not an object

        Word word;
        word.start = 0.0;
        word.end = 0.0;

        // Parse the object
        pos++; // Skip '{'
        while (pos < json.size() && json[pos] != '}')
        {
            // Find the key
            size_t keyStart = json.find('"', pos);
            if (keyStart == std::string::npos) break;
            size_t keyEnd = json.find('"', keyStart + 1);
            if (keyEnd == std::string::npos) break;

            std::string key = json.substr(keyStart + 1, keyEnd - keyStart - 1);
            pos = keyEnd + 1;

            // Find the colon
            while (pos < json.size() && json[pos] != ':') pos++;
            pos++; // Skip ':'

            // Parse the value based on the key
            if (key == "start")
            {
                word.start = parseNumber(json, pos);
            }
            else if (key == "end")
            {
                word.end = parseNumber(json, pos);
            }
            else if (key == "text")
            {
                word.text = parseString(json, pos);
            }
            else
            {
                // Skip unknown value
                while (pos < json.size() && json[pos] != ',' && json[pos] != '}') pos++;
            }

            // Skip to next field or end of object
            while (pos < json.size() && (std::isspace(json[pos]) || json[pos] == ',')) pos++;
        }

        if (!word.text.empty())
            words.push_back(word);

        // Skip past '}'
        while (pos < json.size() && json[pos] != '}') pos++;
        pos++;

        // Skip comma
        while (pos < json.size() && (std::isspace(json[pos]) || json[pos] == ',')) pos++;
    }

    return words;
}

// Extract filename from path
std::string getFilename(const std::string &path)
{
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos)
        return path.substr(pos + 1);
    return path;
}

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        std::cerr << "Usage: " << argv[0] << " <audio-file.wav> <server-url> <language>" << std::endl;
        std::cerr << "Example: " << argv[0] << " audio.wav http://ai1:443/v1/files en" << std::endl;
        return 1;
    }

    const char *audioFile = argv[1];
    const char *serverUrl = argv[2];
    const char *language = argv[3];

    // Read the audio file
    std::ifstream file(audioFile, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        std::cerr << "ERROR: Failed to open audio file: " << audioFile << std::endl;
        return 1;
    }

    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(fileSize);
    if (!file.read(buffer.data(), fileSize))
    {
        std::cerr << "ERROR: Failed to read audio file" << std::endl;
        return 1;
    }
    file.close();

    // Initialize CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        std::cerr << "ERROR: Failed to initialize CURL" << std::endl;
        curl_global_cleanup();
        return 1;
    }

    // Build URL with query parameters
    std::string filename = getFilename(audioFile);
    std::string url = std::string(serverUrl) + "?filename=" + filename + "&language=" + language;

    // Set up the request
    std::string response;
    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: audio/wav");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buffer.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, fileSize);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // Perform the request
    CURLcode res = curl_easy_perform(curl);

    // Clean up CURL
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    if (res != CURLE_OK)
    {
        std::cerr << "ERROR: HTTP request failed: " << curl_easy_strerror(res) << std::endl;
        return 1;
    }

    // Parse the JSON response
    std::vector<Word> words = parseWords(response);

    if (words.empty())
    {
        std::cerr << "ERROR: No words found in response" << std::endl;
        std::cerr << "Response: " << response << std::endl;
        return 1;
    }

    // Output in Audacity label format (tab-separated: start\tend\ttext)
    for (const auto &word : words)
    {
        std::cout << word.start << "\t" << word.end << "\t" << word.text << std::endl;
    }

    return 0;
}
