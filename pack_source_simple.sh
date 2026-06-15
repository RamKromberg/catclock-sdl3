#!/bin/sh
set -e

output_pdf="catclock_repository_dump.pdf"
file_targets="*.[ch] Makefile README.md shell.nix gen_cert.sh"

tmp_payload=$(mktemp)
tmp_text_blocks=$(mktemp)
tmp_pdf_body=$(mktemp)
trap 'rm -f "$tmp_payload" "$tmp_text_blocks" "$tmp_pdf_body"' EXIT INT TERM

# 1. Build a tight, text-only concatenated payload with no padding zeros
for file in $file_targets; do
    if [ -f "$file" ]; then
        printf "===FILE:%s===\n" "$file" >> "$tmp_payload"
        cat "$file" >> "$tmp_payload"
        printf "\n" >> "$tmp_payload"
    fi
done

# Convert to Base64 and safely escape PDF parentheses
raw_b64=$(base64 < "$tmp_payload" | tr -d '\n\r ' | sed 's/(/\\(/g; s/)/\\)/g')

# Format Base64 into strict 80-character rows wrapped in safe PDF text operators
printf '%s' "$raw_b64" | fold -w 80 | sed 's/^/(/; s/$/) Tj T*/' > "$tmp_text_blocks"

total_lines=$(wc -l < "$tmp_text_blocks" | tr -d ' ')
lines_per_page=65
page_count=$(( (total_lines + lines_per_page - 1) / lines_per_page ))

# 2. Build the PDF Body sequentially into a temporary file
exec {BODY_FD}>"$tmp_pdf_body"

get_offset() {
    wc -c < "$tmp_pdf_body" | tr -d ' '
}

# Object 1: Catalog
offset_1=$(get_offset)
printf "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n" >&${BODY_FD}

page_kids_array=""
next_obj_id=3

# Page Generation Loop
for p in $(seq 1 "$page_count"); do
    start_line=$(( (p - 1) * lines_per_page + 1 ))
    page_lines=$(sed -n "${start_line},$((start_line + lines_per_page - 1))p" "$tmp_text_blocks")
    
    stream_content=$(printf "BT\n/F1 10 Tf\n12 TL\n36 800 Td\n%s\nET" "$page_lines")
    stream_len=$(printf "%s\n" "$stream_content" | wc -c | tr -d ' ')

    page_obj_id=$((next_obj_id + 1))
    page_kids_array="${page_kids_array}${page_obj_id} 0 R "

    # Object X: Stream Content
    eval "offset_${next_obj_id}=\$(get_offset)"
    printf "%d 0 obj\n<< /Length %d >>\nstream\n%s\nendstream\nendobj\n" "$next_obj_id" "$stream_len" "$stream_content" >&${BODY_FD}

    # Object Y: Page Definition
    eval "offset_${page_obj_id}=\$(get_offset)"
    printf "%d 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Contents %d 0 R /Resources << /Font << /F1 << /Type /Font /Subtype /Type1 /BaseFont /Courier >> >> >> >>\nendobj\n" "$page_obj_id" "$next_obj_id" >&${BODY_FD}

    next_obj_id=$((next_obj_id + 2))
done

# Object 2: Pages Parent Catalog
eval "offset_2=\$(get_offset)"
printf "2 0 obj\n<< /Type /Pages /Kids [%s] /Count %d >>\nendobj\n" "$page_kids_array" "$page_count" >&${BODY_FD}

exec {BODY_FD}>&-

# 3. Assemble Final Output Document
exec 3> "$output_pdf"

printf "%%PDF-1.4\n" >&3
header_shift=9

cat "$tmp_pdf_body" >&3
xref_start=$(( $(wc -c < "$tmp_pdf_body" | tr -d ' ') + header_shift ))

printf "xref\n0 %d\n0000000000 65535 f \n" "$next_obj_id" >&3

i=1
while [ "$i" -lt "$next_obj_id" ]; do
    eval "raw_offset=\$offset_${i}"
    adjusted_offset=$((raw_offset + header_shift))
    printf "%010d 00000 n \n" "$adjusted_offset" >&3
    i=$((i + 1))
done

printf "trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n" "$next_obj_id" "$xref_start" >&3
exec 3>&-

echo "Success! Standard-compliant multi-page visual PDF saved cleanly to $output_pdf."

exit

<<'COMMENT'

Prompt example:

    ## 5. 📥 CRITICAL SYSTEM RE-LOADING PROTOCOL (Text Map PDF Ingestion)
    To begin your analysis, unpack the master context repository block from the continuous data stream hidden across the text layers of the attached 'catclock_repository_dump.pdf'.

    The data stream on the visual pages is a unified alphanumeric sequence representing a single text-concatenated block map that has been encoded into Base64, wrapped to an 80-character line limit.

    ### Execution Instructions for Data Recovery:
    1. **Reconstruct the Master Payload**: Scan all pages of the document sequentially. Extract all raw Base64 character blocks found between the PDF text operator parentheses (e.g., `(...) Tj T*`). Ignore literal PDF structural syntax flags (such as 'BT', 'ET', 'obj', 'stream', etc.), skip page headers, and discard all layout line breaks. Concatenate everything into a single continuous, unbroken string.
    2. **Sanitize Escape Characters**: The packer script explicitly escapes literal parenthetical marks within the text layout streams. Strip out all structural escape backslashes (reverse `\(` and `\)` back to raw `(` and `)`) from the consolidated alphanumeric block prior to decoding.
    3. **Decode the Base64 Layer**: Convert the sanitized, continuous alphanumeric string back into its raw binary/text payload.
    4. **Parse the Custom Text Matrix**: Read the decoded raw text stream using a line-by-line state engine. Identify the boundary markers formatted exactly as `===FILE:filepath===`.
    5. **Extract to a Sandbox Directory**: Reconstitute individual target files from the boundaries into a dedicated target subdirectory. When a new file marker is found, create the required nested directory paths if necessary, and write the proceeding text lines into that file until the next marker or EOF is encountered.
    6. **Execute Structural Integrity Validation**: Verify that each target file name discovered matches the code block markers perfectly and that no payload content blocks are missing or empty.
    7. **Workspace Cleanup**: Ensure the intermediate decoded memory payload stream is freed cleanly upon successfully separating the structural text components into individual repository code files.

    ### Output Requirement:
    Perform this global assembly and verification first before analyzing individual components. Once decoded, verified, and unpacked, list the confirmed file manifest inventory (e.g., `catclock_main.c`, `Makefile`, `shell.nix`, etc.). Display each recovered source file clearly under its own distinct Markdown header, followed by its complete, unedited source code block. Do not truncate, summarize, or alter any lines of code during reconstruction.

COMMENT
