// remote-whisper-module.cpp
#include <windows.h>
#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <sstream>
#include <vector>

static std::atomic<bool> g_stop{false};
static std::thread g_thread;

static const char *PIPE_REQ = R"(\\.\pipe\remote-whisper.out)";
static const char *PIPE_RES = R"(\\.\pipe\remote-whisper.in)";

static void log(const std::string &s)
{
    OutputDebugStringA((s + "\n").c_str());

    // Optional: Add file logging
    char tempPath[MAX_PATH] = {0};
    GetEnvironmentVariableA("TEMP", tempPath, MAX_PATH);
    if (tempPath[0] != '\0')
    {
        std::string logFile = std::string(tempPath) + "\\remote-whisper.log";
        FILE *f = fopen(logFile.c_str(), "a");
        if (f)
        {
            fprintf(f, "%s\n", s.c_str());
            fclose(f);
        }
    }
}

// ============================================================================
// Result Interface
// ============================================================================
class Result
{
public:
    virtual ~Result() = default;
    virtual bool Respond(std::string &err, std::string &response) = 0;
};

// ============================================================================
// Command Interface
// ============================================================================
class Command
{
public:
    virtual ~Command() = default;
    virtual bool Run(std::string &err, std::unique_ptr<Result> &result) = 0;
};

// ============================================================================
// ProtocolParser Interface
// ============================================================================
class ProtocolParser
{
public:
    virtual ~ProtocolParser() = default;
    virtual bool DecodeCommand(const std::string &commandStr, std::string &err, std::unique_ptr<Command> &command) = 0;
};

// ============================================================================
// Protocol 0 Implementation
// ============================================================================

// Result implementation for protocol 0
class Protocol0Result : public Result
{
private:
    bool m_success;
    std::string m_resultsFileName;
    std::string m_errorMessage;

public:
    Protocol0Result(bool success, const std::string &resultsFileName, const std::string &errorMessage)
        : m_success(success), m_resultsFileName(resultsFileName), m_errorMessage(errorMessage)
    {
    }

    bool Respond(std::string &err, std::string &response) override
    {
        if (m_success)
        {
            response = "version 0,success " + m_resultsFileName;
        }
        else
        {
            response = "version 0,error," + m_errorMessage;
        }
        return true;
    }
};

// Command implementation for protocol 0
class Protocol0Command : public Command
{
private:
    std::string m_audioFilePath;
    std::string m_resultsFileName;
    std::string m_serverUrl;
    std::string m_language;

public:
    Protocol0Command(const std::string &audioFilePath,
                     const std::string &resultsFileName,
                     const std::string &serverUrl,
                     const std::string &language)
        : m_audioFilePath(audioFilePath),
          m_resultsFileName(resultsFileName),
          m_serverUrl(serverUrl),
          m_language(language)
    {
    }

    bool Run(std::string &err, std::unique_ptr<Result> &result) override
    {
        // Get the helper executable path from environment variable
        char helperPath[MAX_PATH] = {0};
        DWORD helperPathLen = GetEnvironmentVariableA("AUDACITY_REMOTE_WHISPER_HELPER", helperPath, MAX_PATH);

        if (helperPathLen == 0 || helperPath[0] == '\0')
        {
            err = "AUDACITY_REMOTE_WHISPER_HELPER environment variable is not set";
            log("Error: " + err);
            result = std::make_unique<Protocol0Result>(false, "", err);
            return false;
        }

        // Build command line
        std::string cmdLine = std::string("\"") + helperPath + "\"";
        cmdLine += " \"" + m_audioFilePath + "\"";
        cmdLine += " \"" + m_serverUrl + "\"";
        if (!m_language.empty())
        {
            cmdLine += " \"" + m_language + "\"";
        }
        else
        {
            cmdLine += "en";
        }
        cmdLine += " \"" + m_resultsFileName + "\"";

        log("Executing: " + cmdLine);

        // Execute the helper
        STARTUPINFOA si = {0};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi = {0};

        BOOL ok = CreateProcessA(NULL,
                                 const_cast<char *>(cmdLine.c_str()),
                                 NULL, NULL, FALSE, 0, NULL, NULL,
                                 &si, &pi);

        if (!ok)
        {
            err = "Failed to execute helper process";
            log("Error: " + err);
            result = std::make_unique<Protocol0Result>(false, "", err);
            return false;
        }

        // Wait for the process to complete
        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        if (exitCode != 0)
        {
            err = "helper exit code " + std::to_string(exitCode);
            log("Error: " + err);
            result = std::make_unique<Protocol0Result>(false, "", err);
            return false;
        }

        // Success
        log("Helper executed successfully");
        result = std::make_unique<Protocol0Result>(true, m_resultsFileName, "");
        return true;
    }
};

// ProtocolParser implementation for protocol 0
class Protocol0Parser : public ProtocolParser
{
public:
    bool DecodeCommand(const std::string &commandStr, std::string &err, std::unique_ptr<Command> &command) override
    {
        // Parse comma-separated fields: audio_file,results_file,server_url,language(optional)
        std::vector<std::string> fields;
        std::stringstream ss(commandStr);
        std::string field;

        while (std::getline(ss, field, ','))
        {
            fields.push_back(field);
        }

        if (fields.size() < 3)
        {
            err = "Protocol 0 requires at least 3 fields: audio_file,results_file,server_url";
            log("Protocol 0 parse error: " + err);
            return false;
        }

        std::string audioFile = fields[0];
        std::string resultsFile = fields[1];
        std::string serverUrl = fields[2];
        std::string language = (fields.size() >= 4) ? fields[3] : "";

        // Helper to strip trailing whitespace characters (including \r, \n, spaces)
        auto trim_trailing = [](std::string &s)
        {
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t'))
            {
                s.pop_back();
            }
        };

        // Clear serverUrl and language of trailing whitespace
        trim_trailing(audioFile);
        trim_trailing(resultsFile);
        trim_trailing(serverUrl);
        trim_trailing(language);

        command = std::make_unique<Protocol0Command>(audioFile, resultsFile, serverUrl, language);
        return true;
    }
};

// ============================================================================
// Protocol Manager
// ============================================================================
std::unique_ptr<ProtocolParser> GetProtocolParser(int version)
{
    switch (version)
    {
    case 0:
        return std::make_unique<Protocol0Parser>();
    default:
        return nullptr;
    }
}

// ============================================================================
// Pipe Operations
// ============================================================================
HANDLE make_inbound(const char *name)
{
    return CreateNamedPipeA(name,
                            PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_WAIT,
                            1, 65536, 65536, 0, NULL);
}

HANDLE make_outbound(const char *name)
{
    return CreateNamedPipeA(name,
                            PIPE_ACCESS_OUTBOUND, PIPE_TYPE_BYTE | PIPE_WAIT,
                            1, 65536, 65536, 0, NULL);
}

std::string read_request(HANDLE hReq)
{
    std::string res;
    char buf[4096];
    DWORD n;
    while (!g_stop.load())
    {
        BOOL ok = ReadFile(hReq, buf, sizeof(buf), &n, NULL);
        if (!ok || n == 0)
            break;
        res.append(buf, n);
        if (res.find('\n') != std::string::npos)
            break;
    }
    size_t pos = res.find('\n');
    if (pos != std::string::npos)
        res.resize(pos);
    return res;
}

void write_response(HANDLE hRes, const std::string &response)
{
    std::string msg = response;
    // Ensure message ends with newline
    if (msg.empty() || msg.back() != '\n')
    {
        msg.push_back('\n');
    }

    DWORD written = 0;
    WriteFile(hRes, msg.data(), (DWORD)msg.size(), &written, NULL);
    log("RES: " + msg);
}

void nudge_pipe_client(const char *name, bool want_write)
{
    DWORD access = want_write ? GENERIC_WRITE : GENERIC_READ;
    HANDLE h = CreateFileA(name, access, 0, NULL, OPEN_EXISTING, 0, 0);
    if (h != INVALID_HANDLE_VALUE)
        CloseHandle(h);
}

// ============================================================================
// Helper to format error messages
// ============================================================================
std::string format_error_response(const std::string &message)
{
    // Sanitize error message: replace commas with semicolons, remove newlines
    std::string clean = message;
    for (char &c : clean)
    {
        if (c == ',')
            c = ';';
        else if (c == '\r' || c == '\n')
            c = ' ';
    }
    // Trim spaces
    while (!clean.empty() && (clean.front() == ' ' || clean.front() == '\t'))
        clean.erase(clean.begin());
    while (!clean.empty() && (clean.back() == ' ' || clean.back() == '\t'))
        clean.pop_back();
    
    if (clean.empty())
        clean = "unknown error";
    
    return "version 0,error," + clean;
}

// ============================================================================
// Command Processing
// ============================================================================
std::string process_command(const std::string &msg)
{
    // Parse the version prefix
    if (msg.find("version ") != 0)
    {
        log("Error: Message does not start with 'version '");
        return format_error_response("invalid protocol format");
    }

    // Extract version number
    size_t spacePos = msg.find(' ', 7); // Find space after "version "
    if (spacePos == std::string::npos)
    {
        log("Error: Invalid message format, no space after version");
        return format_error_response("invalid protocol format");
    }

    size_t commaPos = msg.find(',', spacePos);
    if (commaPos == std::string::npos)
    {
        log("Error: Invalid message format, no comma after version number");
        return format_error_response("invalid protocol format");
    }

    std::string versionStr = msg.substr(8, commaPos - 8);
    int version = -1;
    try
    {
        version = std::stoi(versionStr);
    }
    catch (...)
    {
        log("Error: Invalid version number: " + versionStr);
        return format_error_response("invalid version number");
    }

    // Get the protocol parser
    auto parser = GetProtocolParser(version);
    if (!parser)
    {
        log("Error: Unsupported protocol version: " + std::to_string(version));
        return format_error_response("unsupported protocol version " + std::to_string(version));
    }

    // Extract the command part (everything after "version X,")
    std::string commandStr = msg.substr(commaPos + 1);

    // Decode the command
    std::string err;
    std::unique_ptr<Command> command;
    if (!parser->DecodeCommand(commandStr, err, command))
    {
        log("Error: Failed to decode command: " + err);
        return format_error_response(err);
    }

    // Run the command
    std::unique_ptr<Result> result;
    if (!command->Run(err, result))
    {
        if (!result)
        {
            log("Error: Command failed: " + err);
            return format_error_response(err);
        }
        // If result exists, it takes precedence
    }

    // Get the response
    if (!result)
    {
        log("Error: Command did not return a result");
        return format_error_response("command did not return a result");
    }

    std::string response;
    if (!result->Respond(err, response))
    {
        log("Error: Failed to get response: " + err);
        return response; // Still send the response even on error
    }

    return response;
}

// ============================================================================
// Server Thread
// ============================================================================
void server_thread()
{
    log("remote-whisper module: started");
    log(std::string("  REQ: ") + PIPE_REQ);
    log(std::string("  RES: ") + PIPE_RES);

    while (!g_stop.load())
    {
        HANDLE hReq = make_inbound(PIPE_REQ);
        HANDLE hRes = make_outbound(PIPE_RES);
        if (hReq == INVALID_HANDLE_VALUE || hRes == INVALID_HANDLE_VALUE)
        {
            log("Error: Failed to create pipes");
            break;
        }

        BOOL ok = ConnectNamedPipe(hReq, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (!ok)
        {
            CloseHandle(hReq);
            CloseHandle(hRes);
            continue;
        }

        std::string msg = read_request(hReq);
        DisconnectNamedPipe(hReq);
        log("REQ: " + msg);

        ok = ConnectNamedPipe(hRes, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (!ok)
        {
            CloseHandle(hReq);
            CloseHandle(hRes);
            continue;
        }

        std::string response = process_command(msg);
        write_response(hRes, response);
        DisconnectNamedPipe(hRes);

        CloseHandle(hReq);
        CloseHandle(hRes);
    }

    log("remote-whisper module: stopped");
}

// ============================================================================
// Audacity Module Interface
// ============================================================================
#include <functional>
#include <wx/string.h>

class AudacityProject;
class ShuttleGui;

enum ModuleDispatchTypes
{
    ModuleInitialize,   // 0
    ModuleTerminate,    // 1
    AppInitialized,     // 2
    AppQuiting,         // 3
    ProjectInitialized, // 4
    ProjectClosing      // 5
};

using ModuleCommandCallback = std::function<void(AudacityProject &)>;
using PreferencesFactory = std::function<void(ShuttleGui &)>;

class ModuleManagerInterface
{
public:
    virtual ~ModuleManagerInterface() = default;

    virtual void RegisterModuleCommand(const wxString &, const wxString &, const wxString &, ModuleCommandCallback)
    {
    }

    virtual void RegisterPreferencesFactory(PreferencesFactory)
    {
    }
};

class ModuleInterface
{
public:
    virtual ~ModuleInterface() = default;

    virtual wxString GetSymbol() { return wxString(); }
    virtual wxString GetDescription() { return wxString(); }
    virtual wxString GetVendor() { return wxString(); }
    virtual wxString GetVersion() { return wxString(); }

    virtual bool Initialize() { return true; }
    virtual void Terminate() {}
    virtual bool RegisterModule(ModuleManagerInterface &) { return true; }
};

class RemoteWhisperModule final : public ModuleInterface
{
public:
    RemoteWhisperModule();
    ~RemoteWhisperModule() override;

    wxString GetSymbol() override;
    wxString GetDescription() override;
    wxString GetVendor() override;
    wxString GetVersion() override;

    bool Initialize() override;
    void Terminate() override;
    bool RegisterModule(ModuleManagerInterface &moduleManager) override;
};

ModuleInterface *NewRemoteWhisperModule();

RemoteWhisperModule::RemoteWhisperModule() = default;
RemoteWhisperModule::~RemoteWhisperModule() = default;

wxString RemoteWhisperModule::GetSymbol()
{
    log("RemoteWhisperModule::GetSymbol called");
    return wxT("remote_whisper_transcription");
}

wxString RemoteWhisperModule::GetDescription()
{
    log("RemoteWhisperModule::GetDescription called");
    return wxT("Remote Whisper transcription using an external speech-to-text service");
}

wxString RemoteWhisperModule::GetVendor()
{
    log("RemoteWhisperModule::GetVendor called");
    return wxT("OpenAI");
}

wxString RemoteWhisperModule::GetVersion()
{
    log("RemoteWhisperModule::GetVersion called");
    return wxT("1.0.0");
}

bool RemoteWhisperModule::Initialize()
{
    log("RemoteWhisperModule::Initialize called - Starting server thread");
    g_stop = false;
    g_thread = std::thread(server_thread);
    return true;
}

void RemoteWhisperModule::Terminate()
{
    log("RemoteWhisperModule::Terminate called - Stopping server thread");
    g_stop = true;
    nudge_pipe_client(PIPE_REQ, true);
    nudge_pipe_client(PIPE_RES, false);
    if (g_thread.joinable())
    {
        log("RemoteWhisperModule::Terminate - Joining server thread");
        g_thread.join();
        log("RemoteWhisperModule::Terminate - Server thread joined successfully");
    }
}

bool RemoteWhisperModule::RegisterModule(ModuleManagerInterface &moduleManager)
{
    log("RegisterModule called");
    return true;
}

// -----------------------------------------------------------------------------
// Audacity module interface
// -----------------------------------------------------------------------------
extern "C" __declspec(dllexport) const wchar_t *GetVersionString()
{
    // Return the Audacity version this module is compatible with
    return L"3.7.5";
}

extern "C" __declspec(dllexport) ModuleInterface *ModuleDispatch(ModuleDispatchTypes type)
{
    static RemoteWhisperModule *g_module = nullptr;

    switch (type)
    {
    case ModuleInitialize:
        log("ModuleDispatch: ModuleInitialize - Creating module instance");
        g_module = new RemoteWhisperModule();
        return g_module;

    case ModuleTerminate:
        log("ModuleDispatch: ModuleTerminate - Deleting module instance");
        g_module->Terminate();
        delete g_module;
        g_module = nullptr;
        return nullptr;

    case AppInitialized:
        log("ModuleDispatch: AppInitialized - Calling Initialize()");
        if (g_module)
        {
            g_module->Initialize();
        }
        return nullptr;

    case AppQuiting:
        log("ModuleDispatch: AppQuiting - Calling Terminate()");
        if (g_module)
        {
            g_module->Terminate();
        }
        return nullptr;

    case ProjectInitialized:
        log("ModuleDispatch: ProjectInitialized");
        return nullptr;

    case ProjectClosing:
        log("ModuleDispatch: ProjectClosing");
        return nullptr;

    default:
        log("ModuleDispatch: Unhandled type: " + std::to_string(type));
        return nullptr;
    }
}
