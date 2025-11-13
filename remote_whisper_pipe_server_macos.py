import os
import signal
import socket
import subprocess
import threading

# Unix domain socket paths (stored in /tmp for Linux/macOS)
PIPE_REQ = "/tmp/remote-whisper.out"  # plugin -> server (server reads)
PIPE_RES = "/tmp/remote-whisper.in"   # server -> plugin (server writes)

stop_event = threading.Event()


def _handle_sigint(_sig, _frame):
    stop_event.set()


def create_socket(path: str) -> socket.socket:
    """Create a Unix domain socket at the given path."""
    # Remove existing socket file if it exists
    try:
        os.unlink(path)
    except OSError:
        if os.path.exists(path):
            raise
    
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.bind(path)
    sock.listen(1)
    return sock


def read_request(conn: socket.socket) -> str:
    """Read a request from the client connection."""
    buf = bytearray()
    while True:
        try:
            chunk = conn.recv(4096)
        except (ConnectionResetError, BrokenPipeError):
            break
        if not chunk:
            break
        buf.extend(chunk)
        if b"\n" in chunk:
            break
    data = bytes(buf).decode("utf-8", "replace")
    return data.splitlines()[0] if data else ""


def sanitize_error(message: str) -> str:
    """Sanitize error messages for transmission."""
    clean = message.replace("\r", " ").replace("\n", " ")
    clean = clean.replace(",", ";")
    return clean.strip() or "unknown error"


def handle_command(command: str, helper_path: str) -> str:
    """Process a command from the client."""
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

    try:
        result = subprocess.run(
            [helper_path, audio_path, server_url, language, labels_path],
            capture_output=True,
            text=True,
            check=False,
        )
    except FileNotFoundError:
        return format_error(f"Environment variable AUDACITY_REMOTE_WHISPER_HELPER is '{helper_path}', but the executable is not found")
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
    """Format an error message for transmission."""
    return f"version 0,error,{sanitize_error(message)}"


def accept_with_timeout(sock: socket.socket, timeout: float = 0.1):
    """Accept a connection with timeout support."""
    sock.settimeout(timeout)
    try:
        return sock.accept()
    except socket.timeout:
        return None, None


def main():
    signal.signal(signal.SIGINT, _handle_sigint)
    
    # Load helper path from environment variable
    helper_path = os.environ.get("AUDACITY_REMOTE_WHISPER_HELPER", "").strip()
    
    if not helper_path:
        print("ERROR: Environment variable AUDACITY_REMOTE_WHISPER_HELPER is not set or empty.")
        print("Please set it to the full path of the whisper helper executable.")
        print("Example: export AUDACITY_REMOTE_WHISPER_HELPER=/path/to/whisper-helper")
        return
    
    if not os.path.exists(helper_path):
        print(f"ERROR: Environment variable AUDACITY_REMOTE_WHISPER_HELPER is set to '{helper_path}',")
        print("but the executable does not exist at that path.")
        print("Please verify the path and ensure the file exists.")
        return

    print("Remote Whisper pipe server ready (Ctrl+C to stop)")
    print(f"  Request pipe: {PIPE_REQ}")
    print(f"  Response pipe: {PIPE_RES}")
    print(f"  Helper path: {helper_path}")

    try:
        while not stop_event.is_set():
            # Create sockets for this iteration
            req_sock = create_socket(PIPE_REQ)
            res_sock = create_socket(PIPE_RES)
            
            try:
                # Wait for client to connect to request pipe
                req_conn = None
                while not stop_event.is_set():
                    req_conn, _ = accept_with_timeout(req_sock)
                    if req_conn:
                        break
                
                if stop_event.is_set():
                    break
                
                # Read request
                request = read_request(req_conn)
                req_conn.close()
                print(f"Received: {request}")
                
                # Process request
                try:
                    response = handle_command(request, helper_path)
                except Exception as exc:  # pragma: no cover - ensure loop continues
                    response = format_error(f"unexpected error: {str(exc)}")
                print(f"Responding: {response}")
                
                # Wait for client to connect to response pipe
                res_conn = None
                while not stop_event.is_set():
                    res_conn, _ = accept_with_timeout(res_sock)
                    if res_conn:
                        break
                
                if stop_event.is_set():
                    break
                
                # Send response
                res_conn.sendall((response + "\n").encode("utf-8"))
                res_conn.close()
                
            finally:
                req_sock.close()
                res_sock.close()
                # Clean up socket files
                try:
                    os.unlink(PIPE_REQ)
                except OSError:
                    pass
                try:
                    os.unlink(PIPE_RES)
                except OSError:
                    pass
    finally:
        # Final cleanup
        try:
            os.unlink(PIPE_REQ)
        except OSError:
            pass
        try:
            os.unlink(PIPE_RES)
        except OSError:
            pass

    print("Remote Whisper pipe server shutting down")


if __name__ == "__main__":
    main()
