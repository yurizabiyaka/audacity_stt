# pip install pywin32
import threading
import signal
import win32file, win32pipe, win32api, pywintypes

PIPE_REQ = r'\\.\pipe\remote-whisper.out'  # plugin -> server (server reads)
PIPE_RES = r'\\.\pipe\remote-whisper.in'   # server -> plugin (server writes)

stop = threading.Event()

def _sigint(_sig, _frm):
    stop.set()
signal.signal(signal.SIGINT, _sigint)

def make_inbound(name):
    return win32pipe.CreateNamedPipe(
        name,
        win32pipe.PIPE_ACCESS_INBOUND,
        win32pipe.PIPE_TYPE_BYTE | win32pipe.PIPE_WAIT,
        1, 65536, 65536, 0, None
    )

def make_outbound(name):
    return win32pipe.CreateNamedPipe(
        name,
        win32pipe.PIPE_ACCESS_OUTBOUND,
        win32pipe.PIPE_TYPE_BYTE | win32pipe.PIPE_WAIT,
        1, 65536, 65536, 0, None
    )

def wait_connect_blocking(handle, connected_evt):
    """Run in a thread: wait until a client connects to this pipe instance."""
    try:
        try:
            win32pipe.ConnectNamedPipe(handle, None)  # blocks here
        except pywintypes.error as e:
            # Client might have connected before we called ConnectNamedPipe:
            # ERROR_PIPE_CONNECTED (535) => treat as connected.
            if e.winerror != 535:
                raise
        finally:
            connected_evt.set()
    except Exception:
        connected_evt.set()
        raise

def nudge_pipe_client(pipename, want_write):
    """Open the pipe once as a client to unblock a server ConnectNamedPipe."""
    # For server INBOUND (read), client must open for WRITE.
    # For server OUTBOUND (write), client must open for READ.
    access = win32file.GENERIC_WRITE if want_write else win32file.GENERIC_READ
    try:
        h = win32file.CreateFile(
            pipename, access, 0, None, win32file.OPEN_EXISTING, 0, 0
        )
        win32file.CloseHandle(h)
    except pywintypes.error:
        # If pipe isn't there or already connected, that's fine — best effort.
        pass

def read_request(req_handle):
    buf = bytearray()
    while True:
        try:
            _, chunk = win32file.ReadFile(req_handle, 4096)
        except pywintypes.error as e:
            # client closed
            if e.winerror in (109, 233):  # broken/not connected
                break
            raise
        if not chunk:
            break
        buf.extend(chunk)
        if b'\n' in chunk:
            break
    s = bytes(buf).decode('utf-8', 'replace')
    return s.splitlines()[0] if s else ""

def main():
    print("Server ready (Ctrl+C to stop):")
    print("  REQ:", PIPE_REQ)
    print("  RES:", PIPE_RES)

    while not stop.is_set():
        # Create fresh instances each cycle (your proven pattern)
        h_req = make_inbound(PIPE_REQ)
        h_res = make_outbound(PIPE_RES)
        t_req_evt = threading.Event()
        t_res_evt = threading.Event()

        try:
            # --- Phase 1: wait for plugin write ---
            t_req = threading.Thread(target=wait_connect_blocking, args=(h_req, t_req_evt), daemon=True)
            t_req.start()

            # If stopping, nudge and bail
            while not t_req_evt.is_set():
                if stop.is_set():
                    nudge_pipe_client(PIPE_REQ, want_write=True)   # client opens for WRITE
                    t_req_evt.wait(0.05)
                    break
                t_req_evt.wait(0.05)

            if stop.is_set():
                break

            msg = read_request(h_req)
            win32pipe.DisconnectNamedPipe(h_req)
            print("REQ:", msg)

            # --- Phase 2: wait for plugin read, then send reply ---
            t_res = threading.Thread(target=wait_connect_blocking, args=(h_res, t_res_evt), daemon=True)
            t_res.start()

            while not t_res_evt.is_set():
                if stop.is_set():
                    nudge_pipe_client(PIPE_RES, want_write=False)  # client opens for READ
                    t_res_evt.wait(0.05)
                    break
                t_res_evt.wait(0.05)

            if stop.is_set():
                break

            win32file.WriteFile(h_res, (msg[::-1] + "\n").encode('utf-8'))
            win32pipe.DisconnectNamedPipe(h_res)
            print("RES:", msg[::-1])

        finally:
            win32file.CloseHandle(h_req)
            win32file.CloseHandle(h_res)

    print("Server shutting down.")

if __name__ == "__main__":
    main()
