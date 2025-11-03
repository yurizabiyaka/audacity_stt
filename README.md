# Remote Whisper Speech-to-Text Module

This repository contains an Audacity module that sends the current audio selection to an external Whisper-compatible speech-to-text (STT) service and renders the transcript as a label track.

## Features

* Configurable STT service URL and language (via **Edit → Preferences → Remote Whisper**).
* Sends the current project selection (or the entire project if nothing is selected) as a WAV file to the remote STT service using the HTTP API shown below.
* Creates or refreshes a label track with word-level regions returned by the service.

## Expected STT API

The module expects an HTTP POST endpoint that accepts raw WAV audio with a `filename` and `language` query. Example request:

```bash
curl -X POST "https://ai1:443/v1/files?filename=sample.wav&language=en" \
  -H "Content-Type: audio/wav" \
  --data-binary '@audio.wav'
```

Example of the JSON response consumed by the plugin:

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

### Windows (Visual Studio 2022)

1. Install the following tools:
   * [CMake](https://cmake.org/download/) 3.24 or newer and add it to your `PATH`.
   * [Visual Studio 2022](https://visualstudio.microsoft.com/vs/) with the **Desktop development with C++** workload.
   * A local checkout of the [Audacity source tree](https://github.com/audacity/audacity) that matches the Audacity release you plan to load the module into. The repository provides the necessary C++ headers such as `ModuleInterface.h` and `LabelTrack.h`.
   * The same third-party dependencies used by Audacity (typically provided by [vcpkg](https://github.com/microsoft/vcpkg)). If you built Audacity locally, you can point `VCPKG_ROOT` to the same installation before invoking CMake so that `wxWidgets` and `libcurl` are discovered automatically.
2. Launch the **x64 Native Tools Command Prompt for VS 2022** so that the MSVC compiler environment variables are available.
3. Configure the project with CMake, pointing to your Audacity checkout:

   ```bat
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
     -DAUDACITY_SOURCE_DIR="C:/path/to/audacity"
   ```

4. Build the plugin in either Debug or Release configuration:

   ```bat
   cmake --build build --config Release
   ```

5. Copy the resulting `build/Release/mod-remote-whisper.dll` into Audacity's `modules` directory (e.g., `%PROGRAMFILES%/Audacity/modules`).

### macOS / Linux

```bash
cmake -S . -B build -DAUDACITY_SOURCE_DIR=/path/to/audacity
cmake --build build
```

Copy the generated `mod-remote-whisper` library into Audacity's `modules` directory.
