// remote_whisper_module.cpp
#include <windows.h>
#include <string>
#include <thread>
#include <atomic>

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

void nudge_pipe_client(const char *name, bool want_write)
{
    DWORD access = want_write ? GENERIC_WRITE : GENERIC_READ;
    HANDLE h = CreateFileA(name, access, 0, NULL, OPEN_EXISTING, 0, 0);
    if (h != INVALID_HANDLE_VALUE)
        CloseHandle(h);
}

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
            break;

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

        std::string reply(msg.rbegin(), msg.rend());
        reply.push_back('\n');
        DWORD written = 0;
        WriteFile(hRes, reply.data(), (DWORD)reply.size(), &written, NULL);
        DisconnectNamedPipe(hRes);
        log("RES: " + reply);

        CloseHandle(hReq);
        CloseHandle(hRes);
    }

    log("remote-whisper module: stopped");
}

// whisper module interface implementation:
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
    log("Audacity Pipeserver RemoteWhisperModule::GetSymbol called");
    return wxT("remote_whisper_transcription");
}

wxString RemoteWhisperModule::GetDescription()
{
    log("Audacity Pipeserver RemoteWhisperModule::GetDescription called");
    return wxT("Remote Whisper transcription using an external speech-to-text service");
}

wxString RemoteWhisperModule::GetVendor()
{
    log("Audacity Pipeserver RemoteWhisperModule::GetVendor called");
    return wxT("OpenAI");
}

wxString RemoteWhisperModule::GetVersion()
{
    log("Audacity Pipeserver RemoteWhisperModule::GetVersion called");
    return wxT("1.0.0");
}

bool RemoteWhisperModule::Initialize()
{
    log("Audacity Pipeserver RemoteWhisperModule::Initialize called - Starting server thread");
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
    // Simplified - these functions don't exist yet, so just return true
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
        log("Audacity Pipeserver ModuleDispatch: ModuleInitialize - Creating module instance");
        g_module = new RemoteWhisperModule();
        return g_module;

    case ModuleTerminate:
        log("Audacity Pipeserver ModuleDispatch: ModuleTerminate - Deleting module instance");
        return nullptr;

    case AppInitialized:
        log("Audacity Pipeserver ModuleDispatch: AppInitialized - Calling Initialize()");
        if (g_module)
        {
            g_module->Initialize();
        }
        return nullptr;

    case AppQuiting:
        log("Audacity Pipeserver ModuleDispatch: AppQuiting - Calling Terminate()");
        if (g_module)
        {
            g_module->Terminate();
        }
        return nullptr;

    case ProjectInitialized:
        log("Audacity Pipeserver ModuleDispatch: ProjectInitialized");
        return nullptr;

    case ProjectClosing:
        log("Audacity Pipeserver ModuleDispatch: ProjectClosing");
        return nullptr;

    default:
        log("ModuleDispatch: Unhandled type: " + std::to_string(type));
        return nullptr;
    }
}
