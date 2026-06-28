#!/bin/sh
set -e

output_pdf="catclock_repository_dump.pdf"
file_targets="*.[ch] Makefile dump_seconds_atlas_validation.sh shell.nix"

tmp_payload=$(mktemp)
tmp_text_blocks=$(mktemp)
tmp_pdf_body=$(mktemp)
tmp_uncompressed_pdf=$(mktemp) # Temporary storage for raw assembly

# Clean up all temporary files on script termination
trap 'rm -f "$tmp_payload" "$tmp_text_blocks" "$tmp_pdf_body" "$tmp_uncompressed_pdf"' EXIT INT TERM

# 1. Build a tight, text-only concatenated payload with no padding zeros
for file in $file_targets; do
    if [[ "$file" == *"sokol"* ]]; then
        continue
    fi
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

# 3. Assemble Uncompressed Document
exec 3> "$tmp_uncompressed_pdf"

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

# 4. Compression Stage (Optional)
# Uses qpdf if available to compress page streams and generate optimized object streams.
if command -v qpdf >/dev/null 2>&1; then
    qpdf --compress-streams=y --object-streams=disable --recompress-flate "$tmp_uncompressed_pdf" "$output_pdf"
    echo "Success! High-density optimized compressed PDF saved cleanly to $output_pdf."
else
    cp "$tmp_uncompressed_pdf" "$output_pdf"
    echo "Success! saved uncompressed PDF to $output_pdf (qpdf not found)."
fi

echo ""
echo "Drop $output_pdf into Gemini and use the following prompt:"
echo "--------------------------------------------------------"
cat << EOF
## 🛠️ Stage 0: 📥 Repository Analysis Protocol
Read and parse the contents of the attached catclock_repository_dump.pdf.
## Execution Instructions:

   1. Extract Repository Files: Read through the document to identify individual files separated by the boundary markers (formatted as ===FILE:filepath===).
   2. Verify Manifest: Ensure all source files mentioned in the document are fully accounted for.

## Output Requirement:
   1. List the confirmed file manifest inventory (e.g., catclock_main.c, Makefile, shell.nix, etc.).
   2. End the response immediately after the list.
   3. Do not reference PDF structural elements such as pagination or indexes. Only refer to code by source filenames and adjunct lines.

--------------------------------------------------------

In the followup prompt, be sure to repeat the third instructions as a final note:

Note: Do not reference PDF structural elements such as pagination or indexes. Only refer to code by source filenames and adjunct lines.

--------------------------------------------------------

EOF

exit
