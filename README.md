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

```bash
cmake -S . -B build -DAUDACITY_PATH=/path/to/audacity
cmake --build build
```

The resulting `mod-remote-whisper` library should be copied into Audacity's `modules` directory.
