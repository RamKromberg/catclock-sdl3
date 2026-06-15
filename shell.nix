{ pkgs ? import <nixpkgs> {} }:

let
  windowsPkgs = pkgs.pkgsCross.mingwW64;
in
pkgs.mkShell {
  nativeBuildInputs = with pkgs; [
    pkg-config
    gcc
    gnumake
    sdl3
    libfaketime
    imagemagick
    osslsigncode
    openssl # ./gen_cert.sh
    clang-tools # $ make format
    #mupdf-headless # $ ./pack_source.sh ; mutool info ./catclock_repository_dump.pdf
  ];

  buildInputs = [
    windowsPkgs.stdenv.cc
    windowsPkgs.sdl3
  ];

  shellHook = ''
    # For testing the clock at different times. e.g.
    # $ FAKETIME="2026-01-01 12:10:00" ./catclock-sdl3 & FAKETIME="2026-01-01 12:10:00" xclock
    # $ FAKETIME="2026-01-01 12:15:00" ./catclock-sdl3 & FAKETIME="2026-01-01 12:15:00" xclock
    # $ FAKETIME="2026-01-01 12:20:00" ./catclock-sdl3 & FAKETIME="2026-01-01 12:20:00" xclock
    # $ FAKETIME="2026-01-01 12:25:00" ./catclock-sdl3 & FAKETIME="2026-01-01 12:25:00" xclock
    # $ FAKETIME="2026-01-01 12:35:00" ./catclock-sdl3 & FAKETIME="2026-01-01 12:35:00" xclock
    # $ FAKETIME="2026-01-01 12:40:00" ./catclock-sdl3 & FAKETIME="2026-01-01 12:40:00" xclock
    # $ FAKETIME="2026-01-01 12:45:00" ./catclock-sdl3 & FAKETIME="2026-01-01 12:45:00" xclock
    # $ FAKETIME="2026-01-01 12:50:00" ./catclock-sdl3 & FAKETIME="2026-01-01 12:50:00" xclock
    if [ -d .git ]; then
        git config core.pager "less -x4"
    fi

    export LD_PRELOAD="${pkgs.libfaketime}/lib/libfaketime.so.1"

    # Export the dev prefix for compilation headers/import libs
    export WINDOWS_SDL_PREFIX="${windowsPkgs.sdl3}"

    # NEW: Capture the runtime package path where the real Windows DLL lives
    export WINDOWS_SDL_BIN="${windowsPkgs.sdl3.bin or windowsPkgs.sdl3}"

    echo "=================================================="
    echo " Kit-Cat Clock Cross-Platform Compiler Shell Active "
    echo "   -> Run 'make' to compile for native Linux"
    echo "   -> Run 'make format' to format code using WebKit style"
    echo "   -> Run 'make windows' to cross-compile for Windows (.exe)"
    echo "=================================================="
  '';
}
