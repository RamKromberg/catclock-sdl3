#!/bin/sh

# Exit immediately if a command fails or a variable is unbound
set -eu

# Check for correct arguments
if [ "$#" -lt 2 ]; then
    printf "Usage: %s <output_filename.pdf> \"command1\" [\"command2\" ...]\n" "$0" >&2
    exit 1
fi

# ==============================================================================
# VISUAL LAYOUT USER CONFIGURATIONS
# ==============================================================================
# Set to 1 to draw a solid, unbroken Base-14 Em-Dash separator rule between blocks.
# Set to 0 to disable it and keep only raw whitespace newlines.
DRAW_DIVIDER=1

# ==============================================================================
# RESTORED ORIGINAL ENVIRONMENT COLOR INJECTIONS & PAGER DISABLES
# ==============================================================================
GCC_COLORS='error=01;31:warning=01;35:note=01;36:caret=01;32:locus=01;34:quote=01;37'; export GCC_COLORS
CLANG_COLORS='error=red:warning=magenta:note=cyan:caret=green:locus=blue'; export CLANG_COLORS
FORCE_COLOR=1; export FORCE_COLOR
MYPY_FORCE_COLOR=1; export MYPY_FORCE_COLOR
PY_FORCE_COLOR=1; export PY_FORCE_COLOR
HARNESS_COLOR=1; export HARNESS_COLOR

# Core System Layout Overrides: Tells programs to force colors but bypass pagers
GIT_CONFIG_PARAMETERS="'color.ui=always' 'core.pager=cat' 'pager.grep=false'"; export GIT_CONFIG_PARAMETERS
GIT_PAGER=cat; export GIT_PAGER
PAGER=cat; export PAGER
MANPAGER=cat; export MANPAGER
CLICOLOR=1; export CLICOLOR
CLICOLOR_FORCE=1; export CLICOLOR_FORCE
GREP_COLOR='01;31'; export GREP_COLOR
GREP_COLORS='ms=01;31:mc=01;31:sl=:cx=:fn=35:ln=32:bn=32:se=36'; export GREP_COLORS
# ==============================================================================

# Extract target PDF filename from the first argument
OUTPUT_PDF="$1"
shift

# Secure temporary file generation using mktemp
TMP_LOG=$(mktemp)
TMP_PDF=$(mktemp --suffix=.pdf)
trap 'rm -f "$TMP_LOG" "$TMP_PDF"' EXIT INT TERM

# Initialize clear log buffer file
: > "$TMP_LOG"

# Clean Execution Loop: Processes each independent command string cleanly
for CMD in "$@"; do
    [ -z "$CMD" ] && continue

    # Append a clean prompt marker mimicking your shell state to the log
    printf "$ %s\n" "$CMD" >> "$TMP_LOG"
    
    # ROBUST DIRECT EXECUTION PRESERVING POSIX FUNCTIONS:
    (
        ls()   { command ls --color=always "$@"; }
        grep() { command grep --color=always "$@"; }
        diff() { command diff --color=always "$@"; }
        ip()   { command ip -color=always "$@"; }
        dir()  { command dir --color=always "$@"; }
        vdir() { command vdir --color=always "$@"; }
        
        eval "$CMD"
    ) </dev/null >> "$TMP_LOG" 2>&1 || true
    
    # SAFE LITERAL RECONSTRUCTION SYMBOL INJECTION TRIGGER:
    if [ "$DRAW_DIVIDER" -eq 1 ]; then
        printf "\n__BASE14_SOLID_DIVIDER_RULE__\n\n" >> "$TMP_LOG"
    else
        printf "\n\n" >> "$TMP_LOG"
    fi
done

# 1. Pipeline the raw plain text stream directly into Groff:
(
    printf ".ds LH\n"
    printf ".ds CH\n"
    printf ".ds RH\n"
    printf ".color 1\n" # Enables Groff native typesetting color engine
    
    printf ".nr PS 9\n"  # Document Body Text Size
    printf ".nr VS 10\n" # Document Line Height Leading Space
    
    printf ".LD\n"      # Literal Display preserves tabs, spaces, and formatting cleanly
  
    # Optimized High-Performance Stream Tokenizer via POSIX awk
    tr -d '\r' < "$TMP_LOG" | expand -t 4 | awk '
    BEGIN {
        max_width = 100
        active_color = "black"
    }
    {
        if ($0 == "__BASE14_SOLID_DIVIDER_RULE__") {
            printf ".br\n\\D'\''l \\n[.l]u 0'\''\n.br\n"
            next
        }

        # Dynamically calculate leading indentation for wrapping alignment
        indent_prefix = ""
        if (match($0, /^[ ]+/)) {
            indent_prefix = substr($0, RSTART, RLENGTH)
        }
        
        if (length(indent_prefix) > 40) {
            indent_prefix = substr(indent_prefix, 1, 40)
        }

        # Inject non-spacing escape token at start of line to stop dot-macro validation errors
        printf "\\&"
        col_count = 0
        line_str = $0
        
        while (length(line_str) > 0) {
            # 1. SECURITY SECURED: Translate literal backslashes to inert structural glyphs
            if (match(line_str, /^\\/)) {
                printf "\\[rs]"
                col_count++
                line_str = substr(line_str, 2)
            }
            # 2. Match and handle ANSI Escape Sequences
            else if (match(line_str, /^\x1b\[[0-9;]*[a-zA-Z]/)) {
                seq = substr(line_str, RSTART, RLENGTH)
                line_str = substr(line_str, RSTART + RLENGTH)
                
                cmd_letter = substr(seq, length(seq), 1)
                
                if (cmd_letter == "m") {
                    if (seq ~ /;31m/ || seq ~ /\[31m/)       { active_color = "red" }
                    else if (seq ~ /;32m/ || seq ~ /\[32m/)  { active_color = "green" }
                    else if (seq ~ /;33m/ || seq ~ /\[33m/)  { active_color = "yellow" }
                    else if (seq ~ /;34m/ || seq ~ /\[34m/)  { active_color = "blue" }
                    else if (seq ~ /;35m/ || seq ~ /\[35m/)  { active_color = "magenta" }
                    else if (seq ~ /;36m/ || seq ~ /\[36m/)  { active_color = "cyan" }
                    else if (seq ~ /;37m/ || seq ~ /\[37m/)  { active_color = "white" }
                    else if (seq ~ /\[0*m/ || seq == "\x1b[m") { active_color = "black" }
                    
                    printf "\\m[%s]", active_color
                }
            }
            # 3. Match regular printable text chunks
            else {
                next_esc = match(line_str, /[\x1b\\]/)
                if (next_esc == 0) {
                    chunk = line_str
                    line_str = ""
                } else {
                    chunk = substr(line_str, 1, next_esc - 1)
                    line_str = substr(line_str, next_esc)
                }
                
                chunk_len = length(chunk)
                for (i = 1; i <= chunk_len; i++) {
                    printf "%s", substr(chunk, i, 1)
                    col_count++
                    
                    # Smart wrapping mechanism with indentation protection
                    if (col_count >= max_width && (i < chunk_len || length(line_str) > 0)) {
                        printf "\n\\&%s\\m[%s]", indent_prefix, active_color
                        col_count = length(indent_prefix)
                    }
                }
            }
        }
        printf "\n"
    }'
  
    printf ".DE\n"
) | groff -Tpdf -ms -dpaper=a4 -P-pa4 \
    -rPO=1.0cm -rLL=18.5cm -rHM=1cm -rFM=1cm \
    -f C -Ww > "$TMP_PDF"

# 2. Optimize the raw PDF objects via qpdf
qpdf --compress-streams=y --object-streams=generate --recompress-flate "$TMP_PDF" "$OUTPUT_PDF"

printf "✅ Generated and optimized zero-embed PDF: %s\n" "$OUTPUT_PDF"
