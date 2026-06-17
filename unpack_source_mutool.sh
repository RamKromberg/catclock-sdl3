#!/bin/sh
set -e

input_pdf="catclock_repository_dump.pdf"
output_dir="restored_repository"
output_tar="restored_repo.tar.gz"

if [ ! -f "$input_pdf" ]; then
    echo "Error: Input PDF '$input_pdf' not found."
    exit 1
fi

echo "Extracting payload safely..."

# Create a clean target subdirectory
mkdir -p "$output_dir"

# Isolate text stream objects and decode directly into the subdirectory
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
' "$input_pdf" | sed 's/\\//g' | tr -d '\n\r ' | base64 -d > "$output_dir/$output_tar"

echo "Success! Extracted payload archive."
echo "Unpacking files and running md5sum verification inside './$output_dir'..."

# Move into the directory to keep files contained
cd "$output_dir"

# Extract the archive contents locally inside the subfolder
tar -xzf "$output_tar"

# Check if the validation file exists
if [ -f "manifest.md5" ]; then
    echo "--------------------------------------------------"
    echo "Verification Results:"
    echo "--------------------------------------------------"
    
    if md5sum -c manifest.md5; then
        echo "--------------------------------------------------"
        echo "Verification SUCCESS: All extracted files match perfectly."
        # Clean up temporary and archive files completely
        rm -f manifest.md5 "$output_tar"
    else
        echo "--------------------------------------------------"
        echo "Verification FAILURE: One or more files are corrupted!"
        exit 1
    fi
else
    echo "Warning: No manifest.md5 found inside the archive. Skipping verification."
    rm -f "$output_tar"
fi

# Return to original path context safely
cd ..
echo "Process complete. Clean repository ready in './$output_dir'."
