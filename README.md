# Remote Whisper Speech-to-Text for Audacity

This repository provides an Audacity plugin that sends audio to an external Whisper-compatible speech-to-text (STT) service and creates a label track with word-level timing.

## ⭐ Recommended: Nyquist Plugin (Easy Installation)

The **easiest way** to use this plugin is via the Nyquist script, which requires no compilation!

### Features

* ✅ **No compilation required** - works with any Audacity 3.x
* ✅ **Easy installation** - just copy 2 files to your plugins folder
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

2. **Find these two files:**
   - `remote-whisper-stt.ny` (in the project root)
   - `build/Release/whisper-helper.exe` (or `build/whisper-helper` on Linux/Mac)

3. **Copy them to Audacity's plug-ins folder:**
   - **Windows**: `C:\Users\<YourName>\AppData\Roaming\Audacity\Plug-Ins\`
   - **macOS**: `~/Library/Application Support/audacity/Plug-Ins/`
   - **Linux**: `~/.audacity-data/Plug-Ins/`

   **Both files must be in the same folder!**

4. **Restart Audacity**

### Usage

1. Select the audio you want to transcribe (or select nothing to transcribe the entire project)
2. Go to **Analyze → Remote Whisper Transcription...**
3. In the dialog:
   - **Server URL**: Enter your Whisper STT server URL (e.g., `http://ai1:443/v1/files`)
   - **Language Code**: Enter the language code (e.g., `en` for English)
4. Click **OK**
5. Wait for processing (you'll see progress in the status bar)
6. A new label track will be created with word-level timing!

### How It Works

```
┌─────────────────────┐
│  Audacity           │
│  (Nyquist Plugin)   │
└──────────┬──────────┘
           │ 1. Export selected audio to temp.wav
           ↓
┌─────────────────────┐
│ whisper-helper.exe  │
└──────────┬──────────┘
           │ 2. HTTP POST to Whisper server
           ↓
┌─────────────────────┐
│ Whisper STT Server  │
└──────────┬──────────┘
           │ 3. JSON response with words & timings
           ↓
┌─────────────────────┐
│ whisper-helper.exe  │
└──────────┬──────────┘
           │ 4. Output labels (tab-separated)
           ↓
┌─────────────────────┐
│  Audacity           │
│  (Creates labels)   │
└─────────────────────┘
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
- Make sure both `remote-whisper-stt.ny` and `whisper-helper.exe` are in the **same folder**
- Restart Audacity
- Check Audacity's plug-ins folder location in **Edit → Preferences → Effects**

### "Helper not found" error
- Verify `whisper-helper.exe` is in the same directory as the `.ny` file
- On Linux/Mac, make sure the helper is executable: `chmod +x whisper-helper`

### "No words found in response"
- Check that your STT server is running and accessible
- Verify the server URL is correct
- Test the server with curl using the example above
- Check Audacity's log for the full server response

### Labels are empty or missing
- Ensure your STT server returns JSON in the expected format
- Check that the `words` array contains `start`, `end`, and `text` fields

## License

MIT License - see project files for details.

## Contributing

Issues and pull requests welcome!
