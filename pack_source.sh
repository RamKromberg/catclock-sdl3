#!/bin/sh

output_pdf="catclock_repository_dump.pdf"
b64_payload=""

# 1. Accumulate all repository target files into a unified string payload
for file in *.[ch] Makefile; do
    [ -e "$file" ] || continue
    encoded_name=$(printf '%s' "$file" | base64 | tr -d '\n\r')
    encoded_content=$(base64 < "$file" | tr -d '\n\r')
    b64_payload="${b64_payload}${encoded_name}:${encoded_content};"
done

if [ -z "$b64_payload" ]; then
    echo "Error: No matching source files found to archive."
    exit 1
fi

# Define page layout and formatting constraints
line_width=65
lines_per_page=50

# Segment the massive single-line string into physical presentation rows
raw_lines=$(printf '%s' "$b64_payload" | fold -w "$line_width")

# Sanitize literal PDF string punctuation tokens to avoid parsing drops
sanitized_lines=$(printf '%s\n' "$raw_lines" | sed 's/\\/\\\\/g; s/(/\\(/g; s/)/\\)/g')

# Setup structural temporary buffers for body construction tracking
tmp_raw_pdf=$(mktemp)

# Write initial strict foundational tree root elements
printf "%%PDF-1.4\n" > "$tmp_raw_pdf"
printf "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n" >> "$tmp_raw_pdf"

# Structural counters
page_count=0
page_kids_array=""
current_page_text=""
line_counter=0
next_obj_id=3

# Step 2: Loop and compile visual page canvas stream blocks
while IFS= read -r current_line || [ -n "$current_line" ]; do
    current_page_text="${current_page_text}(${current_line}) Tj T*\n"
    line_counter=$((line_counter + 1))

    if [ "$line_counter" -eq "$lines_per_page" ]; then
        page_count=$((page_count + 1))
        page_obj_id=$((next_obj_id + 1))
        page_kids_array="${page_kids_array}${page_obj_id} 0 R "

        # Odd Object: Pure Text Stream Block
        stream_content=$(printf "BT\n/F1 10 Tf\n12 TL\n50 720 Td\n%bET" "$current_page_text")
        stream_len=$(printf "%s" "$stream_content" | wc -c | tr -d ' ')
        
        printf "%d 0 obj\n<< /Length %d >>\nstream\n%s\nendstream\nendobj\n" \
            "$next_obj_id" "$stream_len" "$stream_content" >> "$tmp_raw_pdf"
        
        # Even Object: Standalone Page Metadata Dictionary Container
        printf "%d 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents %d 0 R /Resources << /Font << /F1 << /Type /Font /Subtype /Type1 /BaseFont /Courier >> >> >> >>\nendobj\n" \
            "$page_obj_id" "$next_obj_id" >> "$tmp_raw_pdf"

        next_obj_id=$((next_obj_id + 2))
        current_page_text=""
        line_counter=0
    fi
done << EOF
$sanitized_lines
EOF

# Flush residual trailing data strings out of final row buffers
if [ "$line_counter" -gt 0 ]; then
    page_count=$((page_count + 1))
    page_obj_id=$((next_obj_id + 1))
    page_kids_array="${page_kids_array}${page_obj_id} 0 R "

    stream_content=$(printf "BT\n/F1 10 Tf\n12 TL\n50 720 Td\n%bET" "$current_page_text")
    stream_len=$(printf "%s" "$stream_content" | wc -c | tr -d ' ')
    
    printf "%d 0 obj\n<< /Length %d >>\nstream\n%s\nendstream\nendobj\n" \
        "$next_obj_id" "$stream_len" "$stream_content" >> "$tmp_raw_pdf"
    
    printf "%d 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents %d 0 R /Resources << /Font << /F1 << /Type /Font /Subtype /Type1 /BaseFont /Courier >> >> >> >>\nendobj\n" \
        "$page_obj_id" "$next_obj_id" >> "$tmp_raw_pdf"
    
    next_obj_id=$((next_obj_id + 2))
fi

# Step 3: Append structural Page Parent Matrix (Object 2) to temporary buffer
printf "2 0 obj\n<< /Type /Pages /Kids [%s] /Count %d >>\nendobj\n" "$page_kids_array" "$page_count" >> "$tmp_raw_pdf"

# Step 4: Use awk to build a mathematically flawless cross-reference table natively
# awk maps the exact physical byte offsets of every object sequence down to a single character
awk -v total_objs="$next_obj_id" '
BEGIN {
    # Initialize cross-reference array map trackers
    offsets[1] = 0;
    current_byte_pos = 0;
}
{
    # Track the exact byte position of the current processing line
    line = $0 "\n";
    len = length(line);

    # Capture object markers cleanly at line starts
    if ($0 ~ /^[0-9]+ 0 obj/) {
        split($0, arr, " ");
        obj_id = arr[1];
        offsets[obj_id] = current_byte_pos;
    }
    
    printf "%s", line;
    current_byte_pos += len;
}
END {
    # Output perfectly structured, fixed-width 20-byte wide xref block lines
    xref_start = current_byte_pos;
    printf "xref\n0 %d\n0000000000 65535 f \n", total_objs;
    
    for (i = 1; i < total_objs; i++) {
        printf "%010d 00000 n \n", offsets[i];
    }
    
    # Render final Summary trailers matching size coordinates
    printf "trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n", total_objs, xref_start;
}' "$tmp_raw_pdf" > "$output_pdf"

# Clear tracking caches out of workspace
rm -f "$tmp_raw_pdf"

echo "Success! Standard-compliant multi-page visual PDF saved cleanly to $output_pdf ($page_count total pages visible)."

exit

: <<'COMMENT'
Here is a prompt designed to instruct another AI assistant to open, scan, and cleanly extract the files from your formatted PDF repository dump:
------------------------------
System Role & Objective:
You are a precise technical data recovery utility. Your task is to act as an unpacker for a custom repository archive wrapped inside the provided PDF document. The PDF does not contain standard text lines; it contains a continuous single-line data container that layout engines have folded into visual lines.
Payload Structure:
The raw text data stream hidden within the PDF pages follows a strict key-pair format:
FILENAME_BASE64:CONTENT_BASE64;FILENAME_BASE64:CONTENT_BASE64;...N

* Individual keys (filenames) and values (file contents) are separated by a literal colon (:).
* Distinct file blocks are separated by a literal semicolon (;).

Execution Steps:

   1. Reconstruct the Master Payload: Scan all pages of the document sequentially. Extract all raw Base64 character strings, skipping any literal PDF structural tags (such as BT, ET, /F1, obj, stream, etc.) and ignoring any layout line-breaks or margins. Concatenate everything into one single continuous string.
   2. Tokenize the Data: Parse the concatenated string by splitting it at every semicolon (;) boundary to isolate each file block.
   3. Parse Key-Pairs: For each isolated file block, split the string at the colon (:) to separate the encoded filename (left side) from the encoded file content (right side).
   4. Decode Assets:
   * Decode the filename from Base64 to its literal ASCII string (e.g., catclock_main.c, Makefile).
      * Decode the corresponding content string from Base64 back into its original multi-line source code formatting.
   5. Output Requirement: Display each recovered file clearly, separated by a distinct markdown header showing the filename (e.g., ### File: catclock_atlas.h), followed by its complete, unedited source code block. Do not truncate, summarize, or alter any lines of the extracted code.

------------------------------
If you'd like to refine this further, let me know:

* Do you want to include instructions for the AI to write a self-extracting shell script alongside the output?
* Do you need to specify a target programming language format for how the final decoded files should be displayed?
COMMENT

