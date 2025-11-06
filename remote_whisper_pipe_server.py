import os
import signal
import subprocess
import threading

import pywintypes
import win32file
import win32pipe

PIPE_REQ = r"\\\\.\\pipe\\remote-whisper.out"  # plugin -> server (server reads)
PIPE_RES = r"\\\\.\\pipe\\remote-whisper.in"   # server -> plugin (server writes)

stop_event = threading.Event()


def _handle_sigint(_sig, _frame):
    stop_event.set()


def make_inbound(name: str):
    return win32pipe.CreateNamedPipe(
        name,
        win32pipe.PIPE_ACCESS_INBOUND,
        win32pipe.PIPE_TYPE_BYTE | win32pipe.PIPE_WAIT,
        1,
        65536,
        65536,
        0,
        None,
    )


def make_outbound(name: str):
    return win32pipe.CreateNamedPipe(
        name,
        win32pipe.PIPE_ACCESS_OUTBOUND,
        win32pipe.PIPE_TYPE_BYTE | win32pipe.PIPE_WAIT,
        1,
        65536,
        65536,
        0,
        None,
    )


def wait_connect_blocking(handle, connected_evt):
    try:
        try:
            win32pipe.ConnectNamedPipe(handle, None)
        except pywintypes.error as exc:  # type: ignore[attr-defined]
            if exc.winerror != 535:  # ERROR_PIPE_CONNECTED
                raise
        finally:
            connected_evt.set()
    except Exception:
        connected_evt.set()
        raise


def read_request(handle) -> str:
    buf = bytearray()
    while True:
        try:
            _, chunk = win32file.ReadFile(handle, 4096)
        except pywintypes.error as exc:  # type: ignore[attr-defined]
            if exc.winerror in (109, 233):  # broken/not connected
                break
            raise
        if not chunk:
            break
        buf.extend(chunk)
        if b"\n" in chunk:
            break
    data = bytes(buf).decode("utf-8", "replace")
    return data.splitlines()[0] if data else ""


def sanitize_error(message: str) -> str:
    clean = message.replace("\r", " ").replace("\n", " ")
    clean = clean.replace(",", ";")
    return clean.strip() or "unknown error"


def handle_command(command: str, helper_path: str) -> str:
    if not command:
        return format_error("empty request")

    parts = [segment.strip() for segment in command.split(",")]
    if len(parts) != 5:
        return format_error("malformed command")

    version, audio_path, labels_path, server_url, language = parts

    if version != "version 0":
        return format_error(f"unsupported protocol '{version}'")

    if not os.path.isabs(audio_path) or not os.path.exists(audio_path):
        return format_error("audio path not found")

    if not os.path.isabs(labels_path):
        return format_error("labels path must be absolute")

    if not os.path.exists(helper_path):
        return format_error("whisper-helper.exe not found")

    try:
        result = subprocess.run(
            [helper_path, audio_path, server_url, language, labels_path],
            capture_output=True,
            text=True,
            check=False,
        )
    except FileNotFoundError:
        return format_error("whisper-helper.exe missing")
    except Exception as exc:  # pragma: no cover - best effort logging
        return format_error(str(exc))

    if result.returncode != 0:
        stderr = result.stderr.strip() or result.stdout.strip()
        if not stderr:
            stderr = f"helper exit code {result.returncode}"
        return format_error(stderr)

    if not os.path.exists(labels_path):
        return format_error("labels file missing after helper run")

    return f"version 0,success {labels_path}"


def format_error(message: str) -> str:
    return f"version 0,error,{sanitize_error(message)}"


def nudge_pipe_client(pipename: str, want_write: bool) -> None:
    access = win32file.GENERIC_WRITE if want_write else win32file.GENERIC_READ
    try:
        handle = win32file.CreateFile(
            pipename,
            access,
            0,
            None,
            win32file.OPEN_EXISTING,
            0,
            0,
        )
        win32file.CloseHandle(handle)
    except pywintypes.error:  # type: ignore[attr-defined]
        pass


def main():
    signal.signal(signal.SIGINT, _handle_sigint)
    base_dir = os.path.dirname(os.path.abspath(__file__))
    helper_path = os.path.join(base_dir, "whisper-helper.exe")

    print("Remote Whisper pipe server ready (Ctrl+C to stop)")
    print(f"  Request pipe: {PIPE_REQ}")
    print(f"  Response pipe: {PIPE_RES}")
    print(f"  Helper path: {helper_path}")

    while not stop_event.is_set():
        req_handle = make_inbound(PIPE_REQ)
        res_handle = make_outbound(PIPE_RES)
        req_evt = threading.Event()
        res_evt = threading.Event()

        try:
            req_thread = threading.Thread(
                target=wait_connect_blocking,
                args=(req_handle, req_evt),
                daemon=True,
            )
            req_thread.start()

            while not req_evt.is_set():
                if stop_event.is_set():
                    nudge_pipe_client(PIPE_REQ, want_write=True)
                    req_evt.wait(0.05)
                    break
                req_evt.wait(0.05)

            if stop_event.is_set():
                break

            res_thread = threading.Thread(
                target=wait_connect_blocking,
                args=(res_handle, res_evt),
                daemon=True,
            )
            res_thread.start()

            request = read_request(req_handle)
            win32pipe.DisconnectNamedPipe(req_handle)
            print(f"Received: {request}")

            response = handle_command(request, helper_path)
            print(f"Responding: {response}")

            while not res_evt.is_set():
                if stop_event.is_set():
                    nudge_pipe_client(PIPE_RES, want_write=False)
                    res_evt.wait(0.05)
                    break
                res_evt.wait(0.05)

            if stop_event.is_set():
                break

            win32file.WriteFile(res_handle, (response + "\n").encode("utf-8"))
            win32pipe.DisconnectNamedPipe(res_handle)
        finally:
            win32file.CloseHandle(req_handle)
            win32file.CloseHandle(res_handle)

    print("Remote Whisper pipe server shutting down")


if __name__ == "__main__":
    main()
