;nyquist plug-in
;version 4
;type analyze
;name "pipe poc extended"
;action "Send and receive via two pipes"
;author "PoC"
;release 1.3

(setq pipeout "\\\\.\\pipe\\remote-whisper.out") ; plugin -> server
(setq pipein  "\\\\.\\pipe\\remote-whisper.in")  ; server -> plugin

;; crude pause: spins for ~a few ms depending on machine speed
(defun spin-wait (n)
  (let ((i 0))
    (while (< i n)
      (setf i (1+ i)))))

(defun send-then-read (msg)
  ;; phase 1: write-only
  (let ((out (open pipeout :direction :output)))
    (if (not out)
        (return-from send-then-read (format nil "ERROR: cannot open for write ~a" pipeout))
        (progn
          (format out "~a~%" msg)     ; send one line
          (close out))))
  ;; small pause to give the server time to post ConnectNamedPipe on the reply pipe
  (spin-wait 1500000)                 ; adjust up/down if needed

  ;; phase 2: read-only
  (let ((inp (open pipein :direction :input)))
    (if (not inp)
        (format nil "ERROR: cannot open for read ~a" pipein)
        (let ((line (read-line inp))) ; read a single line
          (close inp)
          (if line
              (format nil "Reversed:~%~a" line)
              "ERROR: no data read")))))

(send-then-read "Hello from Nyquist")
