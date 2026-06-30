#!/bin/sh

# Exit immediately if any unintended command fails outside the loop
set -e

# Braces, Brackets, and Parentheses
KEY_LEFT_BRACE="｛"
VAL_LEFT_BRACE="{"
KEY_RIGHT_BRACE="｝"
VAL_RIGHT_BRACE="}"
KEY_LEFT_BRACKET="［"
VAL_LEFT_BRACKET="["
KEY_RIGHT_BRACKET="］"
VAL_RIGHT_BRACKET="]"
KEY_LEFT_PAREN="（"
VAL_LEFT_PAREN="("
KEY_RIGHT_PAREN="）"
VAL_RIGHT_PAREN=")"

# Quotes, Punctuation, and Ticks
KEY_DOUBLE_QUOTE="＂"
VAL_DOUBLE_QUOTE='"'
KEY_SINGLE_QUOTE="＇"
VAL_SINGLE_QUOTE="'"
KEY_BACKTICK="｀"
VAL_BACKTICK='`'
KEY_SEMICOLON='；'
VAL_SEMICOLON=';'
KEY_COMMA='，'
VAL_COMMA=','
KEY_PERIOD='．'
VAL_PERIOD='.'
KEY_COLON='：'
VAL_COLON=':'
KEY_EXCLAMATION='！'
VAL_EXCLAMATION='!'
KEY_QUESTION='？'
VAL_QUESTION='?'
KEY_AT='＠'
VAL_AT='@'

# Typographic "Curly" Punctuation and Symbols
KEY_CURLY_APOSTROPHE="’"
VAL_CURLY_APOSTROPHE="'"
KEY_CURLY_SINGLE_OPEN="‘"
VAL_CURLY_SINGLE_OPEN="'"
KEY_CURLY_DOUBLE_OPEN="“"
VAL_CURLY_DOUBLE_OPEN='"'
KEY_CURLY_DOUBLE_CLOSE="”"
VAL_CURLY_DOUBLE_CLOSE='"'
KEY_EM_DASH="—"
VAL_EM_DASH="--"

# Mathematical and Logical Operators
KEY_PLUS='＋'
VAL_PLUS='+'
KEY_MINUS='－'
VAL_MINUS='-'
KEY_ASTERISK='＊'
VAL_ASTERISK='*'
KEY_SLASH='／'
VAL_SLASH='/'
KEY_BACKSLASH='＼'
VAL_BACKSLASH='\\'
KEY_EQUAL='＝'
VAL_EQUAL='='
KEY_LT='＜'
VAL_LT='<'
KEY_GT='＞'
VAL_GT='>'
KEY_AMP='＆'
VAL_AMP='&'
KEY_PIPE='｜'
VAL_PIPE='|'
KEY_PERCENT='％'
VAL_PERCENT='%'
KEY_CARET='＾'
VAL_CARET='^'
KEY_TILDE='～'
VAL_TILDE='~'

# Full-width Numbers (0-9)
KEY_NUMS="０１２３４５６７８９"
VAL_NUMS="0123456789"

# Full-width Alphabet (A-Z)
KEY_ALPHA_UP="ＡＢＣＤＥＦＧＨＩＪＫＬＭＮＯＰＱＲＳＴＵＶＷＸＹＺ"
VAL_ALPHA_UP="ABCDEFGHIJKLMNOPQRSTUVWXYZ"

# Full-width Alphabet (a-z)
KEY_ALPHA_LO="ａｂｃｄｅｆｇｈｉｊｋｌｍｎｏｐｑｒｓｔｕｖｗｘｙｚ"
VAL_ALPHA_LO="abcdefghijklmnopqrstuvwxyz"

# Explicit literal listing including all targeted full-width and curly variants
FF_RANGE='[（）｛｝［］＂＇｀；＊，．：！？＋－／＼＝＜＞＆｜％＾～＠—’‘“”０１２３４５６７８９ＡＢＣＤＥＦＧＨＩＪＫＬＭＮＯＰＱＲＳＴＵＶＷＸＹＺａｂｃｄｅｆｇｈｉｊｋｌｍｎｏｐｑｒｓｔｕｖｗｘｙｚ]'

# Dynamically generate an unprintable ASCII SOH (0x01 / Ctrl-A) byte to use as a delimiter
DELIM=$(printf '\001')

file_targets="*.[ch] ./shaders/*.[ch] ./shaders/*.glsl"
# Loop over local source files
for pattern in $file_targets; do
  for file in $pattern; do
    # 1. Basic existence check
    [ -f "$file" ] || continue

    # 2. Reject symbolic links
    if [ -L "$file" ]; then
        echo "Skipping symbolic link: $file"
        continue
    fi

    # 3. Robust hard-link checking via stat
    if stat --version >/dev/null 2>&1; then
        link_count=$(stat -c "%h" "$file") # GNU stat
    else
        link_count=$(stat -f "%l" "$file") # BSD / macOS stat
    fi

    if [ "$link_count" -gt 1 ]; then
        echo "Skipping hard-linked file (links: $link_count): $file"
        continue
    fi

    # Check and print matches before replacement using UTF-8 aware grep
    results_before=$(grep -n "$FF_RANGE" "$file" | LC_ALL=C sort -u)
    if [ -n "$results_before" ]; then
        echo "$file before:"
        echo "$results_before"
    fi

    # 4. Create a secure temporary file using mktemp
    tmp_file=$(mktemp)

    # Clean up the temp file if the script crashes or gets interrupted mid-loop
    trap 'rm -f "$tmp_file"' EXIT INT TERM

    # Execute replacements using unprintable SOH ($DELIM) delimiters to completely bypass literal conflicts
    sed \
        -e "s${DELIM}$KEY_LEFT_BRACE${DELIM}$VAL_LEFT_BRACE${DELIM}g" \
        -e "s${DELIM}$KEY_RIGHT_BRACE${DELIM}$VAL_RIGHT_BRACE${DELIM}g" \
        -e "s${DELIM}$KEY_RIGHT_BRACKET${DELIM}$VAL_RIGHT_BRACKET${DELIM}g" \
        -e "s${DELIM}$KEY_LEFT_BRACKET${DELIM}$VAL_LEFT_BRACKET${DELIM}g" \
        -e "s${DELIM}$KEY_LEFT_PAREN${DELIM}$VAL_LEFT_PAREN${DELIM}g" \
        -e "s${DELIM}$KEY_RIGHT_PAREN${DELIM}$VAL_RIGHT_PAREN${DELIM}g" \
        -e "s${DELIM}$KEY_DOUBLE_QUOTE${DELIM}$VAL_DOUBLE_QUOTE${DELIM}g" \
        -e "s${DELIM}$KEY_SINGLE_QUOTE${DELIM}$VAL_SINGLE_QUOTE${DELIM}g" \
        -e "s${DELIM}$KEY_BACKTICK${DELIM}$VAL_BACKTICK${DELIM}g" \
        -e "s${DELIM}$KEY_SEMICOLON${DELIM}$VAL_SEMICOLON${DELIM}g" \
        -e "s${DELIM}$KEY_ASTERISK${DELIM}\\$VAL_ASTERISK${DELIM}g" \
        -e "s${DELIM}$KEY_COMMA${DELIM}$VAL_COMMA${DELIM}g" \
        -e "s${DELIM}$KEY_PERIOD${DELIM}\\$VAL_PERIOD${DELIM}g" \
        -e "s${DELIM}$KEY_COLON${DELIM}$VAL_COLON${DELIM}g" \
        -e "s${DELIM}$KEY_EXCLAMATION${DELIM}$VAL_EXCLAMATION${DELIM}g" \
        -e "s${DELIM}$KEY_QUESTION${DELIM}$VAL_QUESTION${DELIM}g" \
        -e "s${DELIM}$KEY_AT${DELIM}$VAL_AT${DELIM}g" \
        -e "s${DELIM}$KEY_CURLY_APOSTROPHE${DELIM}$VAL_CURLY_APOSTROPHE${DELIM}g" \
        -e "s${DELIM}$KEY_CURLY_SINGLE_OPEN${DELIM}$VAL_CURLY_SINGLE_OPEN${DELIM}g" \
        -e "s${DELIM}$KEY_CURLY_DOUBLE_OPEN${DELIM}$VAL_CURLY_DOUBLE_OPEN${DELIM}g" \
        -e "s${DELIM}$KEY_CURLY_DOUBLE_CLOSE${DELIM}$VAL_CURLY_DOUBLE_CLOSE${DELIM}g" \
        -e "s${DELIM}$KEY_EM_DASH${DELIM}$VAL_EM_DASH${DELIM}g" \
        -e "s${DELIM}$KEY_PLUS${DELIM}$VAL_PLUS${DELIM}g" \
        -e "s${DELIM}$KEY_MINUS${DELIM}$VAL_MINUS${DELIM}g" \
        -e "s${DELIM}$KEY_SLASH${DELIM}$VAL_SLASH${DELIM}g" \
        -e "s${DELIM}$KEY_BACKSLASH${DELIM}\\\\$VAL_BACKSLASH${DELIM}g" \
        -e "s${DELIM}$KEY_EQUAL${DELIM}$VAL_EQUAL${DELIM}g" \
        -e "s${DELIM}$KEY_LT${DELIM}$VAL_LT${DELIM}g" \
        -e "s${DELIM}$KEY_GT${DELIM}$VAL_GT${DELIM}g" \
        -e "s${DELIM}$KEY_AMP${DELIM}$VAL_AMP${DELIM}g" \
        -e "s${DELIM}$KEY_PIPE${DELIM}$VAL_PIPE${DELIM}g" \
        -e "s${DELIM}$KEY_PERCENT${DELIM}$VAL_PERCENT${DELIM}g" \
        -e "s${DELIM}$KEY_CARET${DELIM}\\$VAL_CARET${DELIM}g" \
        -e "s${DELIM}$KEY_TILDE${DELIM}$VAL_TILDE${DELIM}g" \
        -e "y/$KEY_NUMS/$VAL_NUMS/" \
        -e "y/$KEY_ALPHA_UP/$VAL_ALPHA_UP/" \
        -e "y/$KEY_ALPHA_LO/$VAL_ALPHA_LO/" \
        "$file" > "$tmp_file"

    # Move back over original file
    mv "$tmp_file" "$file"
    
    # Remove the active trap for this iteration
    trap - EXIT INT TERM

    # Check and print remaining matches after replacement using UTF-8 aware grep
    results_after=$(grep -n "$FF_RANGE" "$file" | LC_ALL=C sort -u)
    if [ -n "$results_after" ]; then
        echo "$file after:"
        echo "$results_after"
    fi
  done
done

