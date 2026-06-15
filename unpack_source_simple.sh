#!/bin/sh
set -e

input_pdf="catclock_repository_dump.pdf"
output_dir="restored_repository"

if [ ! -f "$input_pdf" ]; then
    echo "Error: Input PDF '$input_pdf' not found."
    exit 1
fi

echo "Extracting payload safely..."
mkdir -p "$output_dir"

# 1. Isolate text stream objects and decode into a unified memory map
tmp_payload=$(mktemp)
trap 'rm -f "$tmp_payload"' EXIT INT TERM

awk '
inside_stream && /^\(.*\)[[:space:]]*Tj/ {
    start = index($0, "(");
    end = index($0, ") Tj");
    if (start > 0 && end > start) {
        print substr($0, start + 1, end - start - 1);
    }
}
/stream/    { inside_stream = 1; }
/endstream/ { inside_stream = 0; }
' "$input_pdf" | sed 's/\\//g' | tr -d '\n\r ' | base64 -d > "$tmp_payload"

echo "Success! Parsed payload map."
echo "Reconstituting file lines inside './$output_dir'..."

# 2. Parse the custom structural boundary mapping to recreate individual files
awk -v outdir="$output_dir" '
/^===FILE:.*===$/ {
    # Close previous file context safely if one exists
    if (current_file != "") { close(current_file); }
    
    # Extract filename from structural boundaries
    filename = substr($0, 9, length($0) - 11);
    current_file = outdir "/" filename;
    
    # Ensure subdirectory paths exist if files are nested
    if (filename ~ /\//) {
        sub(/\/[^\/]+$/, "", filename);
        system("mkdir -p \"" outdir "/" filename "\"");
    }
    next;
}
current_file != "" {
    print $0 > current_file;
}
' "$tmp_payload"

echo "Process complete. Clean repository ready in './$output_dir'."
