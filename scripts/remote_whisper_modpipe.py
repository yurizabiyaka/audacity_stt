#!/usr/bin/env python3
"""Interact with Audacity via mod-script-pipe to run Remote Whisper STT.

This script works around Nyquist's inability to run external executables by
controlling Audacity from the outside using the mod-script-pipe module.  The
workflow mirrors the Nyquist plug-in:

1. Export the current selection (or whole project if nothing is selected) to a
   temporary WAV file.
2. Run the whisper-helper executable to contact the remote Whisper-compatible
   STT server.
3. Import the generated label track back into Audacity.

Prerequisites
-------------
* Enable *mod-script-pipe* in Audacity (Preferences → Modules →
  "mod-script-pipe" → Enable, then restart Audacity).
* Build or download ``whisper-helper`` and place it alongside the Nyquist
  plug-in, or supply the path via ``--helper``.
* Run this script while Audacity is open with the project you want to
  transcribe.

The script intentionally keeps its dependencies minimal – only the Python
standard library is required.
"""

from __future__ import annotations

import argparse
import os
import pathlib
import subprocess
import sys
import tempfile
import time
from typing import Iterable, Optional, Tuple


class AudacityPipeClient:
    """Simple client for Audacity's mod-script-pipe interface."""

    def __init__(self, timeout: float = 10.0) -> None:
        self.to_path, self.from_path = self._detect_pipe_paths()
        self.to_pipe, self.from_pipe = self._open_pipes(timeout)

    def close(self) -> None:
        try:
            self.to_pipe.close()
        finally:
            self.from_pipe.close()

    # ``__enter__`` / ``__exit__`` allow use with ``with`` statements.
    def __enter__(self) -> "AudacityPipeClient":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:  # type: ignore[override]
        self.close()

    @staticmethod
    def _detect_pipe_paths() -> Tuple[str, str]:
        if os.name == "nt":
            return r"\\\\.\\pipe\\ToSrvPipe", r"\\\\.\\pipe\\FromSrvPipe"
        user = os.getenv("USER") or str(os.getuid())
        base = pathlib.Path("/tmp")
        return (str(base / f"audacity_script_pipe.to.{user}"),
                str(base / f"audacity_script_pipe.from.{user}"))

    def _open_pipes(self, timeout: float):
        deadline = time.time() + timeout
        last_error: Exception | None = None
        while time.time() < deadline:
            try:
                to_pipe = open(self.to_path, "w", encoding="utf-8", newline="\n")
                from_pipe = open(self.from_path, "r", encoding="utf-8", newline="\n")
                return to_pipe, from_pipe
            except OSError as exc:  # pragma: no cover - depends on platform
                last_error = exc
                time.sleep(0.25)
        raise RuntimeError(
            "mod-script-pipe is not available. Ensure the module is enabled in Audacity"
        ) from last_error

    def send_command(self, command: str) -> Tuple[str, str]:
        """Send an Audacity command and return ``(status, payload)``.

        The status is ``"OK"`` or ``"Error"``.  ``payload`` contains any text
        emitted before the status marker.
        """

        print(f">>> {command}")
        self.to_pipe.write(command + "\n")
        self.to_pipe.flush()
        payload_lines = []
        status = ""
        while True:
            line = self.from_pipe.readline()
            if line == "":
                raise RuntimeError("Audacity pipe closed unexpectedly")
            stripped = line.strip()
            if not stripped:
                continue
            if stripped == "__EOT__":
                break
            if stripped in {"OK", "Error"}:
                status = stripped
            else:
                payload_lines.append(stripped)
        payload = "\n".join(payload_lines)
        if payload:
            print(f"<<< {payload}")
        print(f"<<< {status}")
        return status, payload


def format_path(path: pathlib.Path) -> str:
    """Return a path formatted for Audacity commands."""

    resolved = path.resolve()
    formatted = str(resolved)
    if os.name == "nt":
        formatted = formatted.replace("\\", "/")
    return formatted.replace('"', r"\"")


def export_selection(client: AudacityPipeClient, destination: pathlib.Path) -> None:
    export_cmd = f'Export2: Filename="{format_path(destination)}" Mode=Selection'
    status, payload = client.send_command(export_cmd)
    if status != "OK":
        raise RuntimeError(f"Failed to export audio: {payload}")


def import_labels(client: AudacityPipeClient, labels_file: pathlib.Path) -> None:
    import_cmd = f'Import2: Filename="{format_path(labels_file)}" Mode=Labels'
    status, payload = client.send_command(import_cmd)
    if status != "OK":
        raise RuntimeError(f"Failed to import labels: {payload}")


def run_helper(helper: str, wav_file: pathlib.Path, server_url: str, language: str,
               labels_file: pathlib.Path, extra_args: Iterable[str]) -> None:
    cmd = [helper, str(wav_file), server_url, language, str(labels_file)]
    cmd.extend(extra_args)
    print(f"Running helper: {' '.join(cmd)}")
    try:
        subprocess.run(cmd, check=True)
    except subprocess.CalledProcessError as exc:
        raise RuntimeError(
            f"whisper-helper exited with status {exc.returncode}") from exc
    except FileNotFoundError as exc:
        raise RuntimeError(f"whisper-helper not found: {helper}") from exc


def parse_args(argv: Iterable[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Send the current Audacity selection to a remote Whisper STT service"
    )
    parser.add_argument("server_url", help="Whisper-compatible server URL")
    parser.add_argument("language", help="Language code (e.g., en)")
    parser.add_argument("labels_output", nargs="?",
                        help="Optional path to store the helper's label output")
    parser.add_argument(
        "--helper",
        default=None,
        help="Path to whisper-helper executable (tries common build output locations)"
    )
    parser.add_argument(
        "--helper-arg",
        action="append",
        default=[],
        help="Additional arguments forwarded to whisper-helper"
    )
    parser.add_argument(
        "--keep-temp",
        action="store_true",
        help="Keep temporary files for debugging"
    )
    parser.add_argument(
        "--pipe-timeout",
        type=float,
        default=10.0,
        help="Seconds to wait for mod-script-pipe to become available"
    )
    return parser.parse_args(argv)


def default_helper_path() -> str:
    repo_root = pathlib.Path(__file__).resolve().parent.parent
    candidates = [
        repo_root / "build" / "Release" / "whisper-helper.exe",
        repo_root / "build" / "Release" / "whisper-helper",
        repo_root / "build" / "whisper-helper.exe",
        repo_root / "build" / "whisper-helper",
        repo_root / "whisper-helper.exe",
        repo_root / "whisper-helper",
    ]
    for candidate in candidates:
        if candidate.exists():
            return str(candidate)
    return str(candidates[0])


def main(argv: Optional[Iterable[str]] = None) -> int:
    args = parse_args(argv if argv is not None else sys.argv[1:])
    helper_value = args.helper or default_helper_path()
    helper_path = os.path.expanduser(helper_value)
    helper_path = os.path.abspath(helper_path)
    if not os.path.exists(helper_path):
        print(f"error: helper executable not found: {helper_path}", file=sys.stderr)
        return 2

    labels_path = pathlib.Path(args.labels_output).expanduser() if args.labels_output else None
    if labels_path:
        labels_path.parent.mkdir(parents=True, exist_ok=True)

    if args.keep_temp:
        tmp_dir = pathlib.Path(tempfile.mkdtemp(prefix="audacity-whisper-"))
        cleanup_tmp = False
    else:
        tmp_dir_obj = tempfile.TemporaryDirectory(prefix="audacity-whisper-")
        tmp_dir = pathlib.Path(tmp_dir_obj.name)
        cleanup_tmp = True

    exit_code = 0
    try:
        wav_file = tmp_dir / "selection.wav"
        labels_file = labels_path or (tmp_dir / "labels.txt")

        helper_args = [str(arg) for arg in args.helper_arg]

        with AudacityPipeClient(timeout=args.pipe_timeout) as client:
            export_selection(client, wav_file)
            run_helper(helper_path, wav_file, args.server_url, args.language,
                       labels_file, helper_args)
            import_labels(client, labels_file)
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        exit_code = 1
    finally:
        if args.keep_temp:
            print(f"Temporary files kept at: {tmp_dir}")
        elif cleanup_tmp:
            tmp_dir_obj.cleanup()  # type: ignore[name-defined]

    if exit_code == 0:
        print("Transcription completed successfully.")
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
