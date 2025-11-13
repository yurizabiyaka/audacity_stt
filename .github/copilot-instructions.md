# Copilot Instructions for Remote Whisper STT for Audacity

## Project Overview

This repository contains an Audacity plugin system for remote speech-to-text transcription using Whisper-compatible services. The project consists of:

1. **Nyquist Plugin** (`remote-whisper-stt.ny`): The Audacity plugin interface
2. **Helper Executable** (`whisper-helper.cpp`): C++ utility that communicates with Whisper STT servers
3. **Pipe Server Module** (`remote-whisper-module.cpp`): Windows DLL module for Audacity
4. **Python Pipe Servers**: Alternative implementations for Windows/MacOS/Linux

## Architecture

```
Audacity (Nyquist plugin) 
  ↓ (pipes)
Pipe Server (DLL module or Python)
  ↓ (calls)
External Utility (whisper-helper)
  ↓ (HTTP POST)
Whisper Server
  ↓ (JSON response)
Label file (TSV) → Audacity
```

## Key Technologies

- **C++17**: Core helper executable and pipe server module
- **CMake 3.15+**: Build system
- **CURL**: HTTP client for STT API communication
- **nlohmann/json**: JSON parsing
- **wxWidgets**: For Audacity module integration
- **Python 3**: Alternative pipe server implementations
- **Nyquist**: Audacity's built-in scripting language

## Build System

### Dependencies Management
- Use **vcpkg** on Windows for C++ dependencies (CURL, nlohmann-json, wxWidgets)
- Dependencies are statically linked on Windows (MSVC)
- Python dependencies listed in requirements files

### Build Targets
1. `whisper-helper`: Standalone executable for STT communication
2. `pipeserver-mod`: POC DLL module (legacy)
3. `commands-pipeserver-mod`: Main pipe server DLL module

### Platform-Specific Notes
- **Windows**: Uses MSVC with static runtime (`/MT`)
- **Linux/MacOS**: Uses Python pipe servers (no C++ build required for pipe server)

## File Organization

```
/src/
  whisper-helper.cpp          - Main STT communication utility
  remote-whisper-module.cpp   - Audacity pipe server module
/POC/
  POC-pipeserver-mod-dll.cpp  - Proof of concept implementation
remote-whisper-stt.ny         - Audacity Nyquist plugin
remote_whisper_pipe_server_windows.py - Python pipe server for Windows
CMakeLists.txt                - CMake build configuration
```

## Code Style & Conventions

### C++ Code
- Use C++17 features
- Follow modern C++ practices (RAII, smart pointers when appropriate)
- Static linking on Windows for distribution
- Error handling with proper logging

### Python Code
- Python 3 compatible
- Follow PEP 8 style guidelines
- Use type hints where appropriate

### Nyquist Scripts
- Follow Lisp-style conventions
- Document plugin parameters clearly
- Handle edge cases (empty selection, invalid input)

## Testing & Debugging

### Manual Testing
1. Build the project using CMake
2. Install plugin in Audacity's plug-ins folder
3. For Windows with DLL: Copy to modules folder and enable in Preferences
4. For Python: Run pipe server script
5. Set `AUDACITY_REMOTE_WHISPER_HELPER` environment variable
6. Test with sample audio in Audacity

### Debugging
- **DLL Module**: Logs written to `%TEMP%/remote-whisper.log`
- **Python Server**: Console output
- **Helper Executable**: Check stdout/stderr
- Use appropriate debuggers: Visual Studio (Windows), GDB/LLDB (Linux/MacOS)

## API Contract

### Whisper STT Server Endpoint
- **Method**: POST
- **URL**: `{server_url}?filename={name}&language={code}`
- **Content-Type**: `audio/wav`
- **Body**: Raw WAV audio data

### Expected Response Format
```json
{
  "result": [
    {
      "transcript": "Full transcript text",
      "words": [
        { "start": 0.0, "end": 0.5, "text": "word" }
      ]
    }
  ]
}
```

### Output Format (TSV Labels)
```
start_time\tend_time\tword_text
```

## Common Development Tasks

### Adding New Features
1. Consider impact on all three components (Nyquist, helper, pipe server)
2. Maintain backward compatibility with existing Whisper API format
3. Update README.md with new usage instructions
4. Test on target platforms (Windows primary, Linux/MacOS secondary)

### Modifying Build Configuration
- Update CMakeLists.txt for new source files or dependencies
- Test with vcpkg on Windows
- Verify static linking still works
- Update build instructions in README

### Debugging Communication Issues
1. Check pipe server logs first
2. Verify environment variable `AUDACITY_REMOTE_WHISPER_HELPER`
3. Test helper executable standalone with sample WAV file
4. Verify Whisper server is accessible and responding correctly

## Dependencies

### C++ (via vcpkg)
- `curl` (with static linking on Windows)
- `nlohmann-json`
- `wxWidgets` (for pipe server module)

### Python
- See `remote_whisper_pipe_server_windows_requirements.txt`
- See `requirements_macos.txt` (not yet in repository)

## Contribution Guidelines

1. Make minimal, focused changes
2. Test on Windows (primary platform)
3. Ensure static linking works on Windows builds
4. Update documentation for user-facing changes
5. Follow existing code structure and patterns
6. Consider cross-platform compatibility

## Important Notes

- The project is designed primarily for Windows with Audacity
- Python pipe servers are alternatives to the DLL module
- Environment variable `AUDACITY_REMOTE_WHISPER_HELPER` must point to helper executable
- Audio is temporarily exported to WAV format for processing
- Label tracks are created from TSV files matching audio filenames
