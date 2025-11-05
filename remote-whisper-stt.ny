;nyquist plug-in
;version 4
;type analyze
;name "Remote Whisper Transcription..."
;action "Transcribing audio..."
;author "Audacity Remote Whisper STT"
;release 1.0.0
;copyright "MIT License"
;info "Sends audio to a remote Whisper STT service and creates word-level labels.\n\nRequires whisper-helper.exe - configure the full path in the Helper Executable Path field."

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
;control helper-path "Helper Executable Path" string "C:\\Program Files\\Audacity\\Plug-Ins\\whisper-helper.exe" "C:\\Program Files\\Audacity\\Plug-Ins\\whisper-helper.exe"

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

;; Quote a path for shell command (Windows)
(defun quote-path (path)
  (strcat "\"" path "\""))

;; Parse label line from helper output (format: start\tend\ttext)
(defun trim-trailing-whitespace (str)
  (let ((end (1- (length str)))
        (trim-codes '(32 9 10 13))) ; space, tab, newline, carriage return
    (do ()
        ((or (< end 0)
             (not (member (char-code (char str end)) trim-codes))))
      (setq end (1- end)))
    (subseq str 0 (1+ end))))

(defun parse-label-line (line)
  (let ((clean-line (trim-trailing-whitespace line))
        (tab1 nil)
        (tab2 nil)
        (tab2-relative nil)
        (start-str nil)
        (end-str nil)
        (start nil)
        (end nil)
        (text nil)
        (remainder nil))
    (setq tab1 (string-search "\t" clean-line))
    (when tab1
      (setq start-str (subseq clean-line 0 tab1))
      (setq start (read-from-string start-str))
      ;; Search for second tab in the remainder of the string
      (setq remainder (subseq clean-line (1+ tab1)))
      (setq tab2-relative (string-search "\t" remainder))
      (when tab2-relative
        ;; Calculate absolute position of second tab
        (setq tab2 (+ (1+ tab1) tab2-relative))
        (setq end-str (subseq clean-line (1+ tab1) tab2))
        (setq end (read-from-string end-str))
        (setq text (subseq clean-line (1+ tab2)))
        (when (and start end text)
          (list start end text))))))

;; Read labels from a file
(defun read-labels-from-file (filepath)
  (let ((labels nil)
        (in-file nil)
        (line-count 0))
    (format t "Attempting to open file: ~a~%" filepath)
    (setq in-file (open filepath :direction :input :if-does-not-exist nil))
    (if in-file
        (progn
          (format t "File opened successfully, reading lines...~%")
          (do ((line (read-line in-file nil) (read-line in-file nil)))
              ((null line))
            (setq line-count (1+ line-count))
            (format t "Line ~a (length ~a)~%" line-count (length line))
            (when (> (length line) 0)
              (let ((label nil))
                (setq label (parse-label-line line))
                (if label
                    (progn
                      (format t "  Parsed label successfully~%")
                      (push label labels))
                    (format t "  WARNING: Could not parse line~%")))))
          (close in-file)
          (format t "File closed. Total lines read: ~a, Labels parsed: ~a~%" line-count (length labels))
          (reverse labels))
        (progn
          (format t "ERROR: Could not open output file: ~a~%" filepath)
          nil))))

;; Main processing function
(defun process-audio ()
  (let ((temp-wav (get-temp-wav))
        (temp-labels (get-temp-labels))
        (command nil)
        (exit-code nil)
        (labels nil))

    ;; Log what we're doing
    (format t "Helper executable: ~a~%" helper-path)
    (format t "Temporary audio file: ~a~%" temp-wav)
    (format t "Temporary labels file: ~a~%" temp-labels)

    ;; Export audio to temporary WAV file
    (format t "Exporting audio...~%")
    (s-save *track* ny:all temp-wav)

    ;; Build the command with output file parameter
    (setq command (format nil "~a ~a ~a ~a ~a"
                          (quote-path helper-path)
                          (quote-path temp-wav)
                          (quote-path server-url)
                          (quote-path language)
                          (quote-path temp-labels)))

    (format t "Executing: ~a~%" command)

    ;; Execute the helper - system returns T for success in Nyquist
    (setq exit-code (system command))
    (format t "Helper returned: ~a (type: ~a)~%" exit-code (type-of exit-code))

    ;; Check if helper succeeded (T for success, or 0 for some Nyquist versions)
    (if (or (equal exit-code t) (equal exit-code 0))
        (progn
          (format t "Helper succeeded, reading labels from file...~%")
          (format t "Checking if output file exists...~%")
          (setq labels (read-labels-from-file temp-labels))
          (if labels
              (progn
                (format t "Successfully read ~a labels~%" (length labels))
                (format t "Label data: ~a~%" labels))
              (format t "WARNING: No labels found in output file~%")))
        (format t "ERROR: Helper failed (system returned NIL)~%"))

    ;; Clean up temporary files (but only if they exist)
    (format t "Cleaning up temporary files...~%")
    (when (probe-file temp-wav)
      (delete-file temp-wav))
    (when (probe-file temp-labels)
      (delete-file temp-labels))

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
        ;; Audacity expects a list of label tracks. Wrap labels in another list
        ;; so it becomes a single label track containing all parsed entries.
        (list labels-list))
      (progn
        (format t "ERROR: No labels generated. Check if:~%")
        (format t "  1. whisper-helper.exe is in the plugin directory~%")
        (format t "  2. The server URL is correct~%")
        (format t "  3. The server is running~%")
        (format t "  4. The helper produced valid output~%")
        ;; Return empty string to avoid error
        "")))
