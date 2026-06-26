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
    printf ".LD\n"     # Literal Display preserves tabs, spaces, and formatting cleanly
  
    # Reconstructed Color Translation Matrix:
    # - Translates the __BASE14__ trigger phrase directly into 76 consecutive 
    #   solid Base-14 mathematical Em-Dash block lines (\\[em]).
    tr -d '\r' < "$TMP_LOG" | expand -t 4 | fold -w 100 | sed -e 's/\\/\\e/g' | sed \
        -e 's/^__BASE14_SOLID_DIVIDER_RULE__$/\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]\\[em]/' \
        -e 's/\x1b\[01;31m/\\m[red]/g' \
        -e 's/\x1b\[1;31m/\\m[red]/g' \
        -e 's/\x1b\[31m/\\m[red]/g' \
        -e 's/\x1b\[01;32m/\\m[green]/g' \
        -e 's/\x1b\[1;32m/\\m[green]/g' \
        -e 's/\x1b\[32m/\\m[green]/g' \
        -e 's/\x1b\[01;33m/\\m[yellow]/g' \
        -e 's/\x1b\[1;33m/\\m[yellow]/g' \
        -e 's/\x1b\[33m/\\m[yellow]/g' \
        -e 's/\x1b\[01;34m/\\m[blue]/g' \
        -e 's/\x1b\[1;34m/\\m[blue]/g' \
        -e 's/\x1b\[34m/\\m[blue]/g' \
        -e 's/\x1b\[01;35m/\\m[magenta]/g' \
        -e 's/\x1b\[1;35m/\\m[magenta]/g' \
        -e 's/\x1b\[35m/\\m[magenta]/g' \
        -e 's/\x1b\[01;36m/\\m[cyan]/g' \
        -e 's/\x1b\[1;36m/\\m[cyan]/g' \
        -e 's/\x1b\[36m/\\m[cyan]/g' \
        -e 's/\x1b\[37m/\\m[white]/g' \
        -e 's/\x1b\[m/\\m[black]/g' \
        -e 's/\x1b\[0m/\\m[black]/g' \
        -e 's/\x1b\[1m//g' \
        -e 's/\x1b\[[0-9;]*[a-zA-Z]//g'
  
    printf ".DE\n"
) | groff -Tpdf -ms \
    -rPO=1cm -rLL=19cm -rHM=1cm -rFM=1cm \
    -f C -Ww > "$TMP_PDF"

# 2. Optimize the raw PDF objects via qpdf
qpdf --compress-streams=y --object-streams=generate --recompress-flate "$TMP_PDF" "$OUTPUT_PDF"

printf "✅ Generated and optimized zero-embed PDF: %s\n" "$OUTPUT_PDF"
