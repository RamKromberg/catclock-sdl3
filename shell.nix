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
    imagemagick
    osslsigncode
    openssl
  ];

  buildInputs = [
    windowsPkgs.stdenv.cc
    windowsPkgs.sdl3
  ];

  shellHook = ''
    # Export the dev prefix for compilation headers/import libs
    export WINDOWS_SDL_PREFIX="${windowsPkgs.sdl3}"

    # NEW: Capture the runtime package path where the real Windows DLL lives
    export WINDOWS_SDL_BIN="${windowsPkgs.sdl3.bin or windowsPkgs.sdl3}"

    echo "=============================================================="
    echo " Kit-Cat Clock Cross-Platform Compiler Shell Active "
    echo "   -> Run 'make' to compile for native Linux"
    echo "   -> Run 'make windows' to cross-compile for Windows (.exe)"
    echo "=============================================================="
  '';
}
