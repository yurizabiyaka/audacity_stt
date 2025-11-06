;nyquist plug-in
;version 4
;type analyze
;name "Remote Whisper Transcription..."
;action "Transcribing audio..."
;author "Audacity Remote Whisper STT"
;release 1.0.0
;copyright "MIT License"
;info "Sends audio to a remote Whisper STT service and creates word-level labels.\n\nRequires the remote_whisper_pipe_server.py helper to be running."

;; Remote Whisper Speech-to-Text Plugin for Audacity
;;
;; This plugin:
;; 1. Exports the current selection to a temporary WAV file
;; 2. Calls whisper-helper.exe to send it to a Whisper STT service
;; 3. Parses the response and creates label tracks with word timings
;;
;; Configure the full path to whisper-helper.exe in the plugin settings

;control server-url "Server URL" string "http://localhost:8080/v1/files" "http://ai1:443/v1/files"
;control language "Language Code" string "en" "en"

(setq pipeout "\\\\.\\pipe\\remote-whisper.out") ; plugin -> server
(setq pipein  "\\\\.\\pipe\\remote-whisper.in")  ; server -> plugin

;; Get temp directory
(defun get-temp-dir ()
  (let ((tmpdir (get-env "TEMP")))
    (if (not tmpdir)
        (setq tmpdir (get-env "TMP")))
    (if (not tmpdir)
        (setq tmpdir "/tmp"))
    ;; Make sure it ends with a separator
    (if (not (or (string-equal (subseq tmpdir (1- (length tmpdir))) "/")
                 (string-equal (subseq tmpdir (1- (length tmpdir))) "\\")))
        (setq tmpdir (strcat tmpdir "/")))
    tmpdir))

;; Generate a temporary filename
(defun get-temp-wav ()
  (strcat (get-temp-dir) "audacity-whisper-" (format nil "~a" (gensym)) ".wav"))

;; Generate a temporary output filename
(defun get-temp-labels ()
  (strcat (get-temp-dir) "audacity-whisper-labels-" (format nil "~a" (gensym)) ".txt"))

;; crude pause: spins for ~a few ms depending on machine speed
(defun spin-wait (n)
  (let ((i 0))
    (while (< i n)
      (setf i (1+ i)))))

(defun string-prefix-p (prefix str)
  (let ((plen (length prefix)))
    (and (>= (length str) plen)
         (string-equal prefix (subseq str 0 plen)))))

(defun show-message-box (msg)
  (format t "MESSAGE: ~a~%" msg)
  (aud-do (format nil "Message: Text=~s" msg)))

(defun send-command-to-server (command)
  (let ((out nil)
        (inp nil)
        (line nil)
        (read-result nil))
    (format t "Opening pipe for write: ~a~%" pipeout)
    (setq out (open pipeout :direction :output))
    (if (not out)
        (list nil (format nil "Cannot open pipe for write: ~a" pipeout))
        (progn
          (format t "Sending command: ~a~%" command)
          (format out "~a~%" command)
          (close out)
          (spin-wait 1500000)
          (format t "Opening pipe for read: ~a~%" pipein)
          (setq inp (open pipein :direction :input))
          (if (not inp)
              (list nil (format nil "Cannot open pipe for read: ~a" pipein))
              (progn
                (setq read-result (errset (read-line inp) nil))
                (close inp)
                (if (and read-result (not (null read-result)))
                    (progn
                      (setq line (car read-result))
                      (format t "Received response: ~a~%" line)
                      (list t line))
                    (list nil "No response from transcription server"))))))))

(defun parse-server-response (response expected-labels)
  (let ((parts (split-string response #\,))
        (status nil)
        (returned-path nil)
        (error-text nil))
    (if (or (null parts)
            (not (string-equal (trim-string (car parts)) "version 0")))
        (list nil (format nil "Unexpected response header: ~a" response))
        (progn
          (if (< (length parts) 2)
              (list nil (format nil "Malformed response: ~a" response))
              (progn
                (setq status (trim-string (cadr parts)))
                (cond
                  ((string-prefix-p "success " status)
                   (setq returned-path (trim-string (subseq status 8)))
                   (if (string-equal returned-path expected-labels)
                       (list t returned-path)
                       (list nil (format nil "Labels file mismatch. Expected ~a but got ~a"
                                         expected-labels returned-path))))
                  ((string-equal status "error")
                   (setq error-text (if (>= (length parts) 3)
                                        (trim-string (caddr parts))
                                        "Unknown error"))
                   (list nil error-text))
                  (t
                   (list nil (format nil "Unknown status: ~a" status))))))))))

;; Quote a path for shell command (Windows)
(defun quote-path (path)
  (strcat "\"" path "\""))

;; Check if a character is whitespace (space, tab, CR, LF)
(defun is-whitespace (char)
  (let ((code (char-code char)))
    (or (= code 32)   ; space
        (= code 9)    ; tab
        (= code 13)   ; carriage return
        (= code 10)))) ; line feed

;; Trim whitespace from both ends of a string (including \r and \n)
(defun trim-string (str)
  (if (or (null str) (= (length str) 0))
      ""
      (let ((start 0)
            (end (length str)))
        ;; Trim from start
        (do ((i 0 (1+ i)))
            ((or (>= i end)
                 (not (is-whitespace (char str i))))
             (setq start i)))
        ;; Trim from end
        (do ((i (1- end) (1- i)))
            ((or (< i start)
                 (not (is-whitespace (char str i))))
             (setq end (1+ i))))
        (if (>= start end)
            ""
            (subseq str start end)))))

;; Find position of tab character (code 9) in string
(defun find-tab (str &optional (start-pos 0))
  (if (or (null str) (>= start-pos (length str)))
      nil
      (do ((i start-pos (1+ i)))
          ((or (>= i (length str))
               (= (char-code (char str i)) 9))
           (if (and (< i (length str))
                    (= (char-code (char str i)) 9))
               i
               nil)))))

;; Parse label line from helper output (format: start\tend\ttext)
(defun parse-label-line (line)
  (let (tab1 tab2 start-str end-str start end text clean-line)
    ;; First, trim the line to remove any \r\n or whitespace at the end
    (setq clean-line (trim-string line))
    (format t "  Raw line length: ~a~%" (length line))
    (format t "  Cleaned line length: ~a~%" (length clean-line))

    ;; Find first tab
    (setq tab1 (find-tab clean-line))
    (format t "  First tab position: ~a~%" tab1)

    (when tab1
      ;; Extract start time
      (setq start-str (subseq clean-line 0 tab1))
      (format t "  Start string extracted~%")
      ;; Use read with make-string-input-stream instead of read-from-string
      (setq start (read (make-string-input-stream start-str)))
      (format t "  Start time: ~a~%" start)

      ;; Find second tab
      (setq tab2 (find-tab clean-line (1+ tab1)))
      (format t "  Second tab position: ~a~%" tab2)

      (when tab2
        ;; Extract end time
        (setq end-str (subseq clean-line (1+ tab1) tab2))
        (format t "  End string extracted~%")
        ;; Use read with make-string-input-stream instead of read-from-string
        (setq end (read (make-string-input-stream end-str)))
        (format t "  End time: ~a~%" end)

        ;; Extract and trim text
        (setq text (subseq clean-line (1+ tab2)))
        (setq text (trim-string text))
        (format t "  Text extracted~%")

        (when (and start end text (> (length text) 0))
          (format t "  Label created successfully~%")
          (list start end text))))))

;; Read labels from a file
(defun read-labels-from-file (filepath)
  (let ((labels nil)
        (in-file nil)
        (line-count 0)
        (parse-errors 0)
        (line nil))
    (format t "Attempting to open file: ~a~%" filepath)
    (setq in-file (open filepath :direction :input))
    (if in-file
        (progn
          (format t "File opened successfully, reading lines...~%")
          (format t "=== Beginning line-by-line parsing ===~%")
          (format t "DEBUG: About to start reading lines~%")

          ;; Read lines using a loop
          (setq line (read-line in-file))
          (while line
            (format t "DEBUG: Read line, line-count is ~a~%" line-count)
            (setq line-count (1+ line-count))
            (format t "DEBUG: After increment, line-count is ~a~%" line-count)
            (format t "DEBUG: Line length is ~a~%" (length line))

            (when (> (length line) 0)
              (format t "DEBUG: Line is non-empty, parsing...~%")
              (let ((label nil))
                (setq label (parse-label-line line))
                (if label
                    (progn
                      (format t "  SUCCESS: Label parsed~%")
                      (push label labels))
                    (progn
                      (format t "  ERROR: parse-label-line returned NIL~%")
                      (setq parse-errors (1+ parse-errors))))))

            ;; Read next line
            (format t "DEBUG: About to read next line~%")
            (setq line (read-line in-file)))

          (format t "DEBUG: Finished reading all lines~%")
          (close in-file)
          (format t "~%=== File parsing complete ===~%")
          (format t "Total lines read: ~a~%" line-count)
          (format t "Labels successfully parsed: ~a~%" (length labels))
          (format t "Parse errors: ~a~%" parse-errors)
          (reverse labels))
        (progn
          (format t "ERROR: Could not open output file: ~a~%" filepath)
          nil))))

;; Main processing function
(defun process-audio ()
  (let ((temp-wav (get-temp-wav))
        (temp-labels (get-temp-labels))
        (labels nil)
        (communication-result nil)
        (parse-result nil)
        (response-line nil)
        (command-string nil))

    ;; Log what we're doing
    (format t "Temporary audio file: ~a~%" temp-wav)
    (format t "Temporary labels file: ~a~%" temp-labels)

    ;; Export audio to temporary WAV file
    (format t "Exporting audio...~%")
    (s-save *track* ny:all temp-wav)

    (setq command-string (format nil "version 0,~a,~a,~a,~a"
                                 temp-wav
                                 temp-labels
                                 server-url
                                 language))
    (setq communication-result (send-command-to-server command-string))

    (if (car communication-result)
        (progn
          (setq response-line (cadr communication-result))
          (setq parse-result (parse-server-response response-line temp-labels))
          (if (car parse-result)
              (progn
                (format t "Server indicated success. Reading labels...~%")
                (setq labels (read-labels-from-file temp-labels))
                (if labels
                    (format t "Successfully read ~a labels~%" (length labels))
                    (progn
                      (show-message-box "Labels file was empty or unreadable.")
                      (format t "WARNING: No labels found in output file~%"))))
              (progn
                (show-message-box (cadr parse-result))
                (format t "ERROR: ~a~%" (cadr parse-result)))))
        (progn
          (show-message-box (cadr communication-result))
          (format t "ERROR: ~a~%" (cadr communication-result))))

    ;; Clean up temporary files (but only if they exist)
    (format t "Cleaning up temporary files...~%")
    (system (strcat "del " (quote-path temp-wav)))
    (system (strcat "del " (quote-path temp-labels)))

    ;; Return the labels
    (format t "Returning ~a labels from process-audio~%" (if labels (length labels) 0))
    labels))

;; Helper to split string by delimiter
(defun split-string (str delim)
  (let ((result nil)
        (start 0)
        (pos 0))
    (dotimes (i (length str))
      (when (char= (char str i) delim)
        (push (subseq str start i) result)
        (setq start (1+ i))))
    (push (subseq str start) result)
    (reverse result)))

;; Main execution
(let ((labels-list (process-audio)))
  (format t "=== DEBUG: Main execution ===~%")
  (format t "Labels-list type: ~a~%" (type-of labels-list))
  (format t "Labels-list value: ~a~%" labels-list)
  (if labels-list
      (progn
        (format t "Generated ~a labels~%" (length labels-list))
        (format t "First label example: ~a~%" (car labels-list))
        (format t "Returning labels to Audacity...~%")
        ;; Return the labels in Audacity format
        labels-list)
      (progn
        (format t "ERROR: No labels generated. Check if:~%")
        (format t "  1. remote_whisper_pipe_server.py is running~%")
        (format t "  2. whisper-helper.exe is located next to the Python server~%")
        (format t "  3. The Whisper service URL is correct~%")
        (format t "  4. The helper produced a labels file~%")
        ;; Return empty string to avoid error
        "")))
