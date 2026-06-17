#!/bin/sh
set -e

output_pdf="catclock_repository_dump.pdf"
manifest_file="manifest.md5"
file_targets="*.[ch] Makefile README.md shell.nix gen_cert.sh"

echo "Generating file validation manifest..."
md5sum $file_targets > "$manifest_file" 2>/dev/null || true

if [ ! -s "$manifest_file" ]; then
    echo "Error: No matching source files found to archive."
    rm -f "$manifest_file"
    exit 1
fi

# 1. Archive, compress, parse to Base64, and safely escape parentheses
raw_b64=$(tar -cf - $file_targets "$manifest_file" 2>/dev/null | gzip -9 | base64 | tr -d '\n\r ' | sed 's/(/\\(/g; s/)/\\)/g')
rm -f "$manifest_file"

tmp_text_blocks=$(mktemp)
tmp_pdf_body=$(mktemp)
trap 'rm -f "$tmp_text_blocks" "$tmp_pdf_body"' EXIT INT TERM

# Format Base64 into strict 80-character rows wrapped in safe PDF text operators
printf '%s' "$raw_b64" | fold -w 80 | sed 's/^/(/; s/$/) Tj T*/' > "$tmp_text_blocks"

total_lines=$(wc -l < "$tmp_text_blocks" | tr -d ' ')
lines_per_page=65
page_count=$(( (total_lines + lines_per_page - 1) / lines_per_page ))

# 2. Build the PDF Body sequentially into a temporary file
# This allows us to map byte offsets with 100% accurate absolute positions later.
exec {BODY_FD}>"$tmp_pdf_body"

# Map offset helper
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

    # Object X: Stream Content (Strictly separated by explicit structural newlines)
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

# Write absolute header (Exactly 9 bytes)
printf "%%PDF-1.4\n" >&3
header_shift=9

# Dump the verified body objects
cat "$tmp_pdf_body" >&3

# Calculate exact XREF structural layout starting shift position
xref_start=$(( $(wc -c < "$tmp_pdf_body" | tr -d ' ') + header_shift ))

# Write structured XREF Table
printf "xref\n0 %d\n0000000000 65535 f \n" "$next_obj_id" >&3

# Write entries (Enforcing strict PDF standard 20-byte width limit per entry)
i=1
while [ "$i" -lt "$next_obj_id" ]; do
    eval "raw_offset=\$offset_${i}"
    adjusted_offset=$((raw_offset + header_shift))
    printf "%010d 00000 n \n" "$adjusted_offset" >&3
    i=$((i + 1))
done

# Write complete trailing components
printf "trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n" "$next_obj_id" "$xref_start" >&3
exec 3>&-

echo "Success! Standard-compliant multi-page visual PDF saved cleanly to $output_pdf."


exit

<<'COMMENT'

Prompt Example:

    ## 5. 📥 CRITICAL SYSTEM RE-LOADING PROTOCOL (Compressed PDF Ingestion)
    To begin your analysis, unpack the master context repository block from the continuous data stream hidden across the text layers of the attached 'catclock_repository_dump.pdf'.

    The data stream on the visual pages is a unified alphanumeric sequence representing a single monolithic tarball archive that has been sequentially gzipped and encoded into Base64, wrapped to an 80-character line limit.

    ### Execution Instructions for Data Recovery:
    1. **Reconstruct the Master Payload**: Scan all pages of the document sequentially. Extract all raw Base64 character blocks found between the PDF text operator parentheses (e.g., `(...) Tj T*`). Ignore literal PDF structural syntax flags (such as 'BT', 'ET', 'obj', 'stream', etc.), skip page headers, and discard all layout line breaks. Concatenate everything into a single continuous, unbroken string.
    2. **Sanitize Escape Characters**: The packer script explicitly escapes literal parenthetical marks within the text layout streams. Strip out all structural escape backslashes (reverse `\(` and `\)` back to raw `(` and `)`) from the consolidated alphanumeric block prior to decoding.
    3. **Decode the Base64 Layer**: Convert the sanitized, continuous alphanumeric string back into its raw binary format.
    4. **Inflate the Gzip Stream**: Pass the resulting binary stream through standard gzip decompression to yield the raw tarball matrix.
    5. **Unpack to a Sandbox Directory**: Extract the tarball contents into a dedicated target subdirectory to reconstruct the full multi-file directory layout.
    6. **Execute Integrity Validation**: Locate the embedded `manifest.md5` file inside the extracted assets. Perform a strict MD5 checksum validation against every recovered file. If any file fails verification, halt execution immediately and report the corruption.
    7. **Workspace Cleanup**: Upon successful validation, permanently delete the intermediate payload archive and the `manifest.md5` file from the workspace to ensure a clean repository layout.

    ### Output Requirement:
    Perform this global assembly and verification first before analyzing individual components. Once decoded, verified, and unpacked, list the confirmed file manifest inventory (e.g., `catclock_main.c`, `Makefile`, `shell.nix`, etc.). Display each recovered source file clearly under its own distinct Markdown header, followed by its complete, unedited source code block. Do not truncate, summarize, or alter any lines of code during reconstruction.

COMMENT
