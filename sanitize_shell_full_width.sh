#!/bin/sh
# === FILE: sanitize_shell_full_width.sh ===
set -e

TARGET="dump_validation.sh"

if [ ! -f "$TARGET" ]; then
    echo "[!] Target file $TARGET not found." >&2
    exit 1
fi

# Cleanly converts the full-width symbols to standard ASCII formatting
sed -e 's/％/%/g' \
    -e 's/＋/+/g' \
    -e 's/ｄ/d/g' \
    "$TARGET" > "$TARGET.tmp"

mv "$TARGET.tmp" "$TARGET"
chmod +x "$TARGET"

echo "[+] $TARGET successfully sanitized for execution!"
