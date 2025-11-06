# pip install pywin32
import win32pipe, win32file, pywintypes

PIPE_REQ = r'\\.\pipe\remote-whisper.out'  # plugin -> server
PIPE_RES = r'\\.\pipe\remote-whisper.in'   # server -> plugin

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

def main():
    print("Server ready:\n  REQ:", PIPE_REQ, "\n  RES:", PIPE_RES)
    try:
        while True:
            h_req = make_inbound(PIPE_REQ)
            h_res = make_outbound(PIPE_RES)
            # --- read request ---
            win32pipe.ConnectNamedPipe(h_req, None)
            buf = bytearray()
            try:
                while True:
                    try:
                        _, chunk = win32file.ReadFile(h_req, 4096)
                    except pywintypes.error:
                        break
                    if not chunk:
                        break
                    buf.extend(chunk)
                    if b'\n' in chunk:
                        break
            finally:
                win32pipe.DisconnectNamedPipe(h_req)

            msg = bytes(buf).decode('utf-8', 'replace').splitlines()[0] if buf else ""
            print("REQ:", msg)

            # --- write response ---
            win32pipe.ConnectNamedPipe(h_res, None)
            try:
                win32file.WriteFile(h_res, (msg[::-1] + "\n").encode('utf-8'))
            finally:
                win32pipe.DisconnectNamedPipe(h_res)
            print("RES:", msg[::-1])
            win32file.CloseHandle(h_req)
            win32file.CloseHandle(h_res)
    except KeyboardInterrupt:
        print("Server shutting down.")

if __name__ == "__main__":
    main()
