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

uncompressed_pdf=$(mktemp)
tmp_payload=$(mktemp)
trap 'rm -f "$uncompressed_pdf" "$tmp_payload"' EXIT INT TERM

# 1. Handle Decompression / Stream Unpacking
if command -v qpdf >/dev/null 2>&1; then
	echo "Decompressing PDF streams safely using qpdf..."
	qpdf --stream-data=uncompress "$input_pdf" "$uncompressed_pdf"
else
	echo "qpdf not found. Processing flat text stream objects directly..."
	# Because we disabled object streams during generation, the text data is inside 
	# sequential stream blocks that can be read directly by basic text parsers!
	cp "$input_pdf" "$uncompressed_pdf"
fi

# 2. Isolate text stream objects and decode into a unified memory map
echo "Parsing layout maps..."
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
' "$uncompressed_pdf" | sed 's/\\//g' | tr -d '\n\r ' | base64 -d > "$tmp_payload"

if [ ! -s "$tmp_payload" ]; then
	echo "Error: Could not extract base64 layout from streams." >&2
	exit 1
fi

echo "Success! Parsed payload map ($(wc -c < "$tmp_payload" | tr -d ' ') bytes)."
echo "Reconstituting file lines inside './$output_dir'..."

# 3. Parse the custom structural boundary mapping to recreate individual files
awk -v outdir="$output_dir" '
/^===FILE:.*===$/ {
	if (current_file != "") { close(current_file); }
	
	filename = substr($0, 9, length($0) - 11);
	current_file = outdir "/" filename;
	
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
