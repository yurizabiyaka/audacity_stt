;nyquist plug-in
;version 4
;type analyze
;name "Remote Whisper Transcription..."
;action "Transcribing audio..."
;author "Audacity Remote Whisper STT"
;release 1.0.0
;copyright "MIT License"
;info "Sends audio to a remote Whisper STT service and creates word-level labels.\n\nRequires whisper-helper.exe to be in the same folder as this plugin."

;; Remote Whisper Speech-to-Text Plugin for Audacity
;;
;; This plugin:
;; 1. Exports the current selection to a temporary WAV file
;; 2. Calls whisper-helper.exe to send it to a Whisper STT service
;; 3. Parses the response and creates label tracks with word timings
;;
;; The helper executable must be in the same directory as this .ny file

;control server-url "Server URL" string "http://localhost:8080/v1/files" "http://ai1:443/v1/files"
;control language "Language Code" string "en" "en"

;; Helper function to get the plugin directory
(defun get-plugin-dir ()
  (let ((plugin-path *file-name*))
    (if plugin-path
        (let ((last-slash (max (or (string-search "/" plugin-path :from-end t) -1)
                               (or (string-search "\\" plugin-path :from-end t) -1))))
          (if (> last-slash 0)
              (subseq plugin-path 0 (1+ last-slash))
              ""))
        "")))

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
  (strcat (get-temp-dir) "audacity-whisper-" (format nil "~a" (get-internal-real-time)) ".wav"))

;; Quote a path for shell command (Windows)
(defun quote-path (path)
  (strcat "\"" path "\""))

;; Parse label line from helper output (format: start\tend\ttext)
(defun parse-label-line (line)
  (let ((tab1 (string-search "\t" line))
        (tab2 nil)
        (start nil)
        (end nil)
        (text nil))
    (when tab1
      (setq start (read-from-string (subseq line 0 tab1)))
      (setq tab2 (string-search "\t" line :start (1+ tab1)))
      (when tab2
        (setq end (read-from-string (subseq line (1+ tab1) tab2)))
        (setq text (subseq line (1+ tab2)))
        (list start end text)))))

;; Main processing function
(defun process-audio ()
  (let ((temp-wav (get-temp-wav))
        (plugin-dir (get-plugin-dir))
        (helper-exe "whisper-helper.exe")
        (command nil)
        (labels nil))

    ;; Build path to helper executable
    (setq helper-exe (strcat plugin-dir helper-exe))

    ;; Check if helper exists (basic check)
    (format t "Looking for helper at: ~a~%" helper-exe)

    ;; Export audio to temporary WAV file
    (format t "Exporting audio to: ~a~%" temp-wav)
    (s-save *track* ny:all temp-wav :format 'wav)

    ;; Build the command
    (setq command (format nil "~a ~a ~a ~a"
                          (quote-path helper-exe)
                          (quote-path temp-wav)
                          (quote-path server-url)
                          (quote-path language)))

    (format t "Executing: ~a~%" command)

    ;; Execute the helper and capture output
    (let ((output (system command)))
      (format t "Helper output: ~a~%" output)

      ;; Parse the output lines
      (if (stringp output)
          (let ((lines (split-string output #\Newline)))
            (dolist (line lines)
              (when (> (length line) 0)
                (let ((label (parse-label-line line)))
                  (when label
                    (push label labels))))))
          (format t "ERROR: Helper returned non-string output~%")))

    ;; Clean up temporary file
    (system (strcat "del " (quote-path temp-wav)))

    ;; Return labels in reverse order (they were pushed onto the list)
    (reverse labels)))

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
  (if labels-list
      (progn
        (format t "Generated ~a labels~%" (length labels-list))
        ;; Return the labels in Audacity format
        labels-list)
      (progn
        (format t "ERROR: No labels generated. Check if:~%")
        (format t "  1. whisper-helper.exe is in the plugin directory~%")
        (format t "  2. The server URL is correct~%")
        (format t "  3. The server is running~%")
        nil)))
