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
        std::cerr << "LOG: Attempting to parse JSON..." << std::endl;
        // Parse JSON
        json j = json::parse(jsonStr);
        std::cerr << "LOG: JSON parsed successfully" << std::endl;

        // Check if "result" array exists
        if (!j.contains("result") || !j["result"].is_array())
        {
            std::cerr << "ERROR: JSON response missing 'result' array" << std::endl;
            std::cerr << "ERROR: JSON keys present: ";
            for (auto& el : j.items())
            {
                std::cerr << el.key() << " ";
            }
            std::cerr << std::endl;
            return words;
        }

        std::cerr << "LOG: Found 'result' array with " << j["result"].size() << " segments" << std::endl;

        // Iterate through all segments in the result array
        int segment_idx = 0;
        for (const auto& segment : j["result"])
        {
            segment_idx++;
            // Check if this segment has a "words" array
            if (!segment.contains("words") || !segment["words"].is_array())
            {
                std::cerr << "LOG: Segment " << segment_idx << " has no 'words' array, skipping" << std::endl;
                continue;
            }

            std::cerr << "LOG: Segment " << segment_idx << " has " << segment["words"].size() << " words" << std::endl;

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

        std::cerr << "LOG: Total words extracted: " << words.size() << std::endl;
    }
    catch (const json::exception& e)
    {
        std::cerr << "ERROR: JSON parsing failed: " << e.what() << std::endl;
        std::cerr << "ERROR: Response was: " << jsonStr << std::endl;
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
    std::cerr << "=== WHISPER-HELPER STARTING ===" << std::endl;
    std::cerr << "LOG: argc = " << argc << std::endl;
    for (int i = 0; i < argc; i++)
    {
        std::cerr << "LOG: argv[" << i << "] = " << argv[i] << std::endl;
    }

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
    std::string outputFilePath(outputFile);

    auto appendStatusMessage = [&](const std::string &message) {
        std::ofstream statusFile(outputFilePath, std::ios::out | std::ios::app);
        if (statusFile.is_open())
        {
            statusFile << "; " << message << std::endl;
        }
        else
        {
            std::cerr << "ERROR: Unable to write status message to output file: " << outputFilePath << std::endl;
        }
    };

    // Create (or truncate) the output file immediately so the caller can detect that the helper ran.
    {
        std::ofstream statusFile(outputFilePath, std::ios::out | std::ios::trunc);
        if (statusFile.is_open())
        {
            statusFile << "; whisper-helper invoked" << std::endl;
        }
        else
        {
            std::cerr << "ERROR: Failed to create initial output file placeholder: " << outputFilePath << std::endl;
        }
    }

    std::cerr << "LOG: Audio file: " << audioFile << std::endl;
    std::cerr << "LOG: Server URL: " << serverUrl << std::endl;
    std::cerr << "LOG: Language: " << language << std::endl;
    std::cerr << "LOG: Output file: " << outputFile << std::endl;

    // Read the audio file
    std::cerr << "LOG: Opening audio file for reading..." << std::endl;
    std::ifstream file(audioFile, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        std::cerr << "ERROR: Failed to open audio file: " << audioFile << std::endl;
        appendStatusMessage("Failed to open audio file");
        return 1;
    }
    std::cerr << "LOG: Audio file opened successfully" << std::endl;

    std::streamsize fileSize = file.tellg();
    std::cerr << "LOG: Audio file size: " << fileSize << " bytes" << std::endl;
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(fileSize);
    std::cerr << "LOG: Reading audio file into buffer..." << std::endl;
    if (!file.read(buffer.data(), fileSize))
    {
        std::cerr << "ERROR: Failed to read audio file" << std::endl;
        appendStatusMessage("Failed to read audio file");
        return 1;
    }
    std::cerr << "LOG: Audio file read successfully" << std::endl;
    file.close();

    // Initialize CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        std::cerr << "ERROR: Failed to initialize CURL" << std::endl;
        appendStatusMessage("Failed to initialize CURL");
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
    std::cerr << "LOG: Sending HTTP POST request to: " << url << std::endl;
    std::cerr << "LOG: Audio file size: " << fileSize << " bytes" << std::endl;
    CURLcode res = curl_easy_perform(curl);

    // Get HTTP response code
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    std::cerr << "LOG: HTTP response code: " << http_code << std::endl;

    // Clean up CURL
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    if (res != CURLE_OK)
    {
        std::cerr << "ERROR: HTTP request failed: " << curl_easy_strerror(res) << std::endl;
        appendStatusMessage(std::string("HTTP request failed: ") + curl_easy_strerror(res));
        return 1;
    }

    std::cerr << "LOG: Received response (length: " << response.length() << " bytes)" << std::endl;
    std::cerr << "LOG: Response content: " << response << std::endl;

    // Parse the JSON response
    std::cerr << "LOG: Parsing JSON response..." << std::endl;
    std::vector<Word> words = parseWords(response);
    std::cerr << "LOG: Parsed " << words.size() << " words from response" << std::endl;

    if (words.empty())
    {
        std::cerr << "ERROR: No words found in response" << std::endl;
        std::cerr << "ERROR: This means the response JSON had no 'words' in 'result' array" << std::endl;
        appendStatusMessage("No words found in response JSON");
        return 1;
    }

    // Write output to file in Audacity label format (tab-separated: start\tend\ttext)
    std::cerr << "LOG: Opening output file: " << outputFile << std::endl;
    std::ofstream outFile(outputFile);
    if (!outFile.is_open())
    {
        std::cerr << "ERROR: Failed to open output file for writing: " << outputFile << std::endl;
        appendStatusMessage("Failed to open output file for writing");
        return 1;
    }
    std::cerr << "LOG: Output file opened successfully" << std::endl;

    std::cerr << "LOG: Writing " << words.size() << " labels to file..." << std::endl;
    int written_count = 0;
    for (const auto &word : words)
    {
        outFile << word.start << "\t" << word.end << "\t" << word.text << std::endl;
        written_count++;
        if (written_count <= 3 || written_count > (words.size() - 3))
        {
            // Log first 3 and last 3 words
            std::cerr << "LOG: Wrote label " << written_count << ": "
                     << word.start << "\t" << word.end << "\t" << word.text << std::endl;
        }
    }

    // Explicitly flush the file buffer
    std::cerr << "LOG: Flushing file buffer..." << std::endl;
    outFile.flush();

    // Check if write operations were successful
    if (outFile.fail())
    {
        std::cerr << "ERROR: File write operations failed!" << std::endl;
        outFile.close();
        appendStatusMessage("File write operations failed");
        return 1;
    }

    std::cerr << "LOG: Closing output file..." << std::endl;
    outFile.close();

    // Verify the file was closed properly
    if (outFile.is_open())
    {
        std::cerr << "ERROR: Failed to close output file!" << std::endl;
        appendStatusMessage("Failed to close output file");
        return 1;
    }

    std::cerr << "LOG: File closed successfully" << std::endl;

    // Output success message to stderr (since Nyquist can't capture stdout anyway)
    std::cerr << "SUCCESS: Wrote " << words.size() << " labels to " << outputFile << std::endl;
    std::cerr << "LOG: Returning exit code 0 (success)" << std::endl;

    return 0;
}
