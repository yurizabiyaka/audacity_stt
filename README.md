# Remote Whisper Speech-to-Text for Audacity

This repository provides an Audacity plugin and a companion Windows pipe server that send audio to an external Whisper-compatible speech-to-text (STT) service and create a label track with word-level timing.

## ⭐ Recommended: Nyquist Plugin (Easy Installation)

The **easiest way** to use this plugin is via the Nyquist script, which requires no compilation!

### Features

* ✅ **No compilation required** - works with any Audacity 3.x
* ✅ **Easy installation** - copy the Nyquist plugin and helper binaries
* ✅ **Configurable** - set server URL and language in the plugin dialog
* ✅ **Word-level timing** - creates labels for each word with precise timing
* ✅ **Appears in Analyze menu** automatically

### Installation

1. **Build the helper executable:**

   ```bash
   # Windows (using vcpkg for CURL)
   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
   cmake --build build --config Release
   ```

2. **Locate these files:**
   - `remote-whisper-stt.ny` (Nyquist plugin in the project root)
   - `remote_whisper_pipe_server.py` (Python pipe server in the project root)
   - `build/Release/whisper-helper.exe` (or `build/whisper-helper` on Linux/Mac)

3. **Copy `remote-whisper-stt.ny` to Audacity's plug-ins folder:**
   - **Windows**: `C:\Users\<YourName>\AppData\Roaming\Audacity\Plug-Ins\`
   - **macOS**: `~/Library/Application Support/audacity/Plug-Ins/`
   - **Linux**: `~/.audacity-data/Plug-Ins/`

4. **Keep the Python server and helper together:** place `remote_whisper_pipe_server.py` and `whisper-helper.exe` in the **same directory**. The server expects the helper executable to be next to it.

5. **Restart Audacity**

### Usage

1. Launch the pipe server from the directory that contains both the Python script and `whisper-helper.exe`:

   ```bat
   py -3 remote_whisper_pipe_server.py
   ```

   Leave this window running while you use Audacity.

2. Select the audio you want to transcribe (or select nothing to transcribe the entire project)
3. Go to **Analyze → Remote Whisper Transcription...**
4. In the dialog:
   - **Server URL**: Enter your Whisper STT server URL (e.g., `http://ai1:443/v1/files`)
   - **Language Code**: Enter the language code (e.g., `en` for English)
5. Click **OK**
6. The plugin exports the audio, waits for the server response, and then creates a label track if the filenames match.

### How It Works

```
┌─────────────────────┐              ┌────────────────────────┐
│  Audacity           │  pipes       │  remote_whisper_pipe_  │
│  (Nyquist Plugin)   │───────────▶ │  server.py             │
└──────────┬──────────┘              └──────────┬─────────────┘
           │ 1. Export audio                    │ 2. Run helper with
           ↓                                    │    requested options
┌─────────────────────┐            ┌────────────▼───────────┐               ┌────────────────────────┐
│  Temporary WAV      │───────────▶│  External Utility      │◀──────────▶ │ Whisper STT Server     │
└─────────────────────┘ 3. Read    └────────────┬───────────┘ 4. Transcribe └────────────────────────┘
                          audio                  │ 5. Create labels
                                                 ↓
                                    ┌────────────────────────┐
            ┌───────────────────────▶|  Labels (tsv)          │
            │                       └────────────┬───────────┘
            │                                    │ 6. Send result
            │                                    │    back via pipe
┌───────────▼─────────┐            ┌────────────▼───────────┐
│  Audacity           │◀───────────│  remote_whisper_pipe_  │
│  (Creates labels)   │ 7. Create  │  server.py              │
└─────────────────────┘    track   └────────────────────────┘
```

## Expected STT API

The helper expects an HTTP POST endpoint that accepts raw WAV audio with `filename` and `language` query parameters:

**Request:**
```bash
curl -X POST "http://ai1:443/v1/files?filename=sample.wav&language=en" \
  -H "Content-Type: audio/wav" \
  --data-binary '@audio.wav'
```

**Response (JSON):**
```json
{
  "result": [
    {
      "transcript": "Hello world",
      "words": [
        { "start": 0.0, "end": 0.5, "text": "Hello" },
        { "start": 0.5, "end": 1.0, "text": "world" }
      ]
    }
  ]
}
```

## Building

### Prerequisites

- CMake 3.15 or newer
- C++ compiler (Visual Studio 2022 on Windows, GCC/Clang on Linux/Mac)
- CURL library (install via vcpkg or system package manager)
- nlohmann/json library (install via vcpkg or system package manager)

### Windows (Visual Studio 2022)

Using vcpkg for dependencies:

```bat
# Install dependencies via vcpkg
vcpkg install curl:x64-windows nlohmann-json:x64-windows

# Configure
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build build --config Release

# Files will be in:
# - build/Release/whisper-helper.exe
```

### Linux / macOS

```bash
# Install dependencies (if needed)
# Ubuntu/Debian: sudo apt-get install libcurl4-openssl-dev nlohmann-json3-dev
# macOS: brew install curl nlohmann-json

# Configure and build
cmake -S . -B build
cmake --build build

# Files will be in:
# - build/whisper-helper
```


## Troubleshooting

### Plugin doesn't appear in Analyze menu
- Make sure `remote-whisper-stt.ny` is in the **Plug-Ins** folder
- Restart Audacity
- Check Audacity's plug-ins folder location in **Edit → Preferences → Effects**

### "Helper not found" or immediate pipe errors
- Verify `remote_whisper_pipe_server.py` and `whisper-helper.exe` live in the same directory
- Confirm the server console shows "Remote Whisper pipe server ready"
- On Linux/Mac, make sure the helper is executable: `chmod +x whisper-helper`

### "No words found in response"
- Check that your STT server is running and accessible
- Verify the server URL is correct
- Test the server with curl using the example above
- Check Audacity's log for the full server response

### Labels are empty or missing
- Ensure your STT server returns JSON in the expected format
- Check that the `words` array contains `start`, `end`, and `text` fields
- If the plugin never receives a response, stop the Nyquist effect and restart the pipe server

## License

MIT License - see project files for details.

## Contributing

Issues and pull requests welcome!
