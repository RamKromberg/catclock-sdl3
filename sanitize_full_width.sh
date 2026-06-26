#!/bin/sh
# =========================================================================
# UNIVERSAL IN-PLACE UNICODE NORMALIZATION ENGINE
# Processes all local .c and .h files natively in-place using sed.
# Excludes the 'nix' store and any external system dependencies.
# =========================================================================

KEY_LEFT_BRACE="｛"
VAL_LEFT_BRACE="{"

KEY_RIGHT_BRACE="｝"
VAL_RIGHT_BRACE="}"

KEY_RIGHT_BRACKET="］"
VAL_RIGHT_BRACKET="]"

KEY_LEFT_BRACKET="［"
VAL_LEFT_BRACKET="["

KEY_LEFT_PAREN="（"
VAL_LEFT_PAREN="("

KEY_RIGHT_PAREN="）"
VAL_RIGHT_PAREN=")"

KEY_DOUBLE_QUOTE="＂"
VAL_DOUBLE_QUOTE='"'

KEY_SINGLE_QUOTE="＇"
VAL_SINGLE_QUOTE="'"

# Identify and loop over local source files safely
for file in *.[ch]; do
    # Verify the file actually exists and is not a broken wildcard expansion
    [ -f "$file" ] || continue

    # Execute safe in-place replacement using a temporary staging buffer
    sed \
        -e "s/$KEY_LEFT_BRACE/$VAL_LEFT_BRACE/g" \
        -e "s/$KEY_RIGHT_BRACE/$VAL_RIGHT_BRACE/g" \
        -e "s/$KEY_RIGHT_BRACKET/$VAL_RIGHT_BRACKET/g" \
        -e "s/$KEY_LEFT_BRACKET/$VAL_LEFT_BRACKET/g" \
        -e "s/$KEY_LEFT_PAREN/$VAL_LEFT_PAREN/g" \
        -e "s/$KEY_RIGHT_PAREN/$VAL_RIGHT_PAREN/g" \
        -e "s/$KEY_DOUBLE_QUOTE/$VAL_DOUBLE_QUOTE/g" \
        -e "s/$KEY_SINGLE_QUOTE/$VAL_SINGLE_QUOTE/g" \
        "$file" > "$file.tmp" && mv "$file.tmp" "$file"
done
