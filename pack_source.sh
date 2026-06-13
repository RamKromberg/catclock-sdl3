#!/bin/sh

# A small script to allow dropping source code into Gemini Google AI mode without losing indentation or being restricted by message length.
# The script convert source files in ./ to .pdf BASE64'ing their content and wrapping it with PDF boilerplate.
# Gemini need to be instructed to decode the files by telling it what they are and how they are made.

# Ensure script stops if any error occurs
set -e

# Loop over all .c and .h files in the current working directory
for src_file in *.[ch]; do
    # Skip if no matching files exist
    [ -f "$src_file" ] || continue

    output_pdf="${src_file}.pdf"
    echo "Encoding: $src_file -> $output_pdf"

    # 1. Base64 encode the source text file.
    # Using standard POSIX-friendly line wrapping for formatting inside the PDF stream.
    b64_payload=$(base64 "$src_file" | fold -w 65)

    # 2. Build the PDF 1.4 layout payload container
    cat << EOF > "$output_pdf"
%PDF-1.4
1 0 obj
<< /Type /Catalog /Pages 2 0 R >>
endobj
2 0 obj
<< /Type /Pages /Kids [3 0 R] /Count 1 >>
endobj
3 0 obj
<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents 4 0 R /Resources << /Font << /F1 << /Type /Font /Subtype /Type1 /BaseFont /Courier >> >> >> >>
endobj
4 0 obj
<< /Length ${#b64_payload} >>
stream
BT
/F1 10 Tf
12 TL
50 720 Td
(=== BASE64 ENCODED HEADER FOR: $src_file ===) Tj T*
($b64_payload) Tj T*
ET
endstream
endobj
xref
0 5
0000000000 65535 f
0000000009 00000 n
0000000058 00000 n
0000000115 00000 n
0000000305 00000 n
trailer
<< /Size 5 /Root 1 0 R >>
startxref
5000
%%EOF
EOF

done

echo "Encoding complete! Ready to upload files."
