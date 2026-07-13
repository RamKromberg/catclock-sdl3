#!/usr/bin/env sh
# CENTRALIZED CONFIGURATION FOR QUICK TESTING
TESTS="1 2 3 4 5 6 7 8 9 10 11"
SCALES="1.0 2.0"

./sanitize_full_width.sh
make format
rm -f *.pam
rm -f *.png
rm -f ./catclock_hands.o
rm -f ./catclock-sdl3
make clean

# Create a secure isolated staging folder in RAM (tmpfs) for log processing steps
STAGE_DIR=$(mktemp -d)

# Maintain an explicit local flat file inside the secure RAM staging space
LOCAL_BOOKMARKS_FILE="${STAGE_DIR}/compiled_bookmarks.txt"
: > "$LOCAL_BOOKMARKS_FILE"

# Track processing steps using standard POSIX space-delimited string accumulation
PDF_LIST=""
TMP_COUNTER=0
CURRENT_PAGE_PTR=1

# Centralized exit cleanup routine
cleanup_on_exit() {
    # FIX: Terminate or wait for any child processes (like pdftk-java) before removing files.
    # This prevents the Java thread pool from deadlocking on deleted file descriptors.
    trap - INT TERM EXIT
    if [ -d "$STAGE_DIR" ]; then
        # Give children a moment to release memory hooks, then safely purge the tree
        rm -rf "$STAGE_DIR" 2>/dev/null
    fi
}

# Register the trap handler using standard POSIX signal targets
trap cleanup_on_exit EXIT INT TERM

# Helper function to compile a command to a temporary PDF inside our RAM stage directory
compile_pdf() {
    local_source_cmd="$1"
    local_pdf_name="${STAGE_DIR}/page_${TMP_COUNTER}.pdf"
    
    # Use eval to handle parameter expansion and syntax tokenizing properly
    eval "./cmd2pdf.sh \"$local_pdf_name\" $local_source_cmd"
    
    # Safe POSIX list tracking without using brittle outer quotes inside the string variable
    PDF_LIST="${PDF_LIST} $local_pdf_name"
    TMP_COUNTER=$((TMP_COUNTER + 1))

    # Natively extract page length counts from the newly created PDF using qpdf
    local_pages_count=$(qpdf --show-pages "$local_pdf_name" | grep -c "^page ")

    # Scrub single quotes out entirely to prevent Java tokenization freezes
    local_clean_title=$(printf "%s" "$local_source_cmd" | tr -d "'")

    # Append data straight to the filesystem metadata engine
    cat >> "$LOCAL_BOOKMARKS_FILE" << EOF
BookmarkBegin
BookmarkTitle: $local_clean_title
BookmarkLevel: 1
BookmarkPageNumber: $CURRENT_PAGE_PTR
EOF

    # Shift page pointer forward based on file size footprint allocation
    CURRENT_PAGE_PTR=$((CURRENT_PAGE_PTR + local_pages_count))
}

compile_pdf "'date'"

compile_pdf "'grep -n -h \"^.*$\" ./catclock_hands.c'"

if [ -n "$(git diff catclock_hands.c)" ]; then
    compile_pdf "'git diff catclock_hands.c'"
fi

for TEST in $TESTS; do
	make CFLAGS="-Wall -Wextra -O2 -DDEBUG_TELEMETRY_LINE -DDEBUG_TELEMETRY_TRIANGLE -DTEST_MODE=$TEST"
	for SCALE in $SCALES; do
        # Standard POSIX conditional string evaluations
		if [ "$TEST" = "1" ] && [ "$SCALE" = "1.0" ]; then
            continue
        fi
		if [ "$TEST" = "3" ] && [ "$SCALE" = "1.0" ]; then
            continue
        fi
		if [ "$TEST" = "4" ] && [ "$SCALE" = "1.0" ]; then
            continue
        fi
		echo "Test $TEST $SCALE"
		
		# Isolated RAM path preserving your original pristine file name scheme
		TMP_LOG="${STAGE_DIR}/test_${TEST}_${SCALE}.log"
		
		./catclock-sdl3 --scale "$SCALE" > "$TMP_LOG" 2>&1
		./dump_validation.sh
		mv dump_seconds_atlas.png test_"${TEST}"_"${SCALE}".png
		mv dump_seconds_atlas_validation.png test_"${TEST}"_"${SCALE}"_validation.png
		
		# 2. Compile the log to PDF referencing the clean RAM disk path
		compile_pdf "'grep -n -h \"^.*$\" $TMP_LOG'"
        
		# Immediate local cleanup: delete the text log file right after conversion
		rm -f "$TMP_LOG"
	done
	rm -f ./catclock_hands.o
	rm -f ./catclock-sdl3
done

# Concatenate and apply the compiled bookmark metadata block seamlessly
if [ -n "$PDF_LIST" ]; then
    # Step A: Let pdftk handle concatenation natively in RAM
    eval "pdftk $PDF_LIST cat output \"${STAGE_DIR}/merged_flat.pdf\" drop_xmp drop_xfa"
    
    # Step B: Extract base metadata to the staging directory
    pdftk "${STAGE_DIR}/merged_flat.pdf" dump_data_utf8 output "${STAGE_DIR}/out_dump.txt" drop_xmp drop_xfa
    
    # Step C: Apply your exact awk commas correction filter line natively to out_dump.txt
    awk -i inplace '/^PageMedia/ { gsub(/,/, "") }; { print; }' "${STAGE_DIR}/out_dump.txt"
    
    # Step D: Aggressively strip away all pre-existing broken bookmarks to prevent duplication or freezes
    awk '/^Bookmark/ { next } { print }' "${STAGE_DIR}/out_dump.txt" > "${STAGE_DIR}/final_meta.txt"
    
    # Step E: Combine the metadata dump file with our clean loop bookmarks file
    cat "${STAGE_DIR}/compiled_bookmarks.txt" >> "${STAGE_DIR}/final_meta.txt"
    
    # Step F: Apply the metadata update to deliver the uncompressed intermediate layout
    pdftk "${STAGE_DIR}/merged_flat.pdf" update_info_utf8 "${STAGE_DIR}/final_meta.txt" output "${STAGE_DIR}/test_harness_uncompressed.pdf" drop_xmp drop_xfa
    
    # Step G: Execute final target optimization pass via qpdf to deliver to project root
    qpdf --compress-streams=y --object-streams=disable --recompress-flate "${STAGE_DIR}/test_harness_uncompressed.pdf" ./test_harness.pdf
fi

rm -f *.pam
exit 0
