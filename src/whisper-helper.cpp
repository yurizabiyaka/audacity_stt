/**********************************************************************

  Audacity Remote Whisper STT - Helper Executable

  This is a standalone command-line tool that:
  1. Reads a WAV file
  2. Sends it to a Whisper-compatible STT service via HTTP POST
  3. Parses the JSON response using nlohmann/json
  4. Writes Audacity label format to an output file

  Usage: whisper-helper.exe <audio-file.wav> <server-url> <language> <output-file.txt>
  Example: whisper-helper.exe audio.wav http://ai1:443/v1/files en labels.txt

  Output file format (tab-separated):
  0.000000	0.500000	Hello
  0.500000	1.000000	world

**********************************************************************/

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Callback for CURL to write response data
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    std::string *str = static_cast<std::string*>(userp);
    size_t total = size * nmemb;
    str->append(static_cast<const char*>(contents), total);
    return total;
}

// Word structure for transcription results
struct Word {
    double start;
    double end;
    std::string text;
};

// Parse words from JSON response using nlohmann/json
// Handles multiple segments by iterating through all "result" entries
std::vector<Word> parseWords(const std::string &jsonStr)
{
    std::vector<Word> words;

    try
    {
        // Parse JSON
        json j = json::parse(jsonStr);

        // Check if "result" array exists
        if (!j.contains("result") || !j["result"].is_array())
        {
            std::cerr << "ERROR: JSON response missing 'result' array" << std::endl;
            return words;
        }

        // Iterate through all segments in the result array
        for (const auto& segment : j["result"])
        {
            // Check if this segment has a "words" array
            if (!segment.contains("words") || !segment["words"].is_array())
                continue;

            // Extract all words from this segment
            for (const auto& wordObj : segment["words"])
            {
                Word word;
                word.start = wordObj.value("start", 0.0);
                word.end = wordObj.value("end", 0.0);
                word.text = wordObj.value("text", "");

                if (!word.text.empty())
                    words.push_back(word);
            }
        }
    }
    catch (const json::exception& e)
    {
        std::cerr << "ERROR: JSON parsing failed: " << e.what() << std::endl;
        std::cerr << "Response: " << jsonStr << std::endl;
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
    if (argc < 5)
    {
        std::cerr << "Usage: " << argv[0] << " <audio-file.wav> <server-url> <language> <output-file.txt>" << std::endl;
        std::cerr << "Example: " << argv[0] << " audio.wav http://ai1:443/v1/files en labels.txt" << std::endl;
        return 1;
    }

    const char *audioFile = argv[1];
    const char *serverUrl = argv[2];
    const char *language = argv[3];
    const char *outputFile = argv[4];

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
        return 1;
    }

    // Write output to file in Audacity label format (tab-separated: start\tend\ttext)
    std::ofstream outFile(outputFile);
    if (!outFile.is_open())
    {
        std::cerr << "ERROR: Failed to open output file: " << outputFile << std::endl;
        return 1;
    }

    for (const auto &word : words)
    {
        outFile << word.start << "\t" << word.end << "\t" << word.text << std::endl;
    }

    outFile.close();

    // Output success message to stderr (since Nyquist can't capture stdout anyway)
    std::cerr << "SUCCESS: Wrote " << words.size() << " labels to " << outputFile << std::endl;

    return 0;
}
