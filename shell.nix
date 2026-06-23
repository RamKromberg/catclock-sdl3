{ pkgs ? import <nixpkgs> { config = { allowUnfree = true; }; } }:

let
  windowsPkgs = pkgs.pkgsCross.mingwW64;
  
  # Tracks the specified Sokol master revision commit
  srcSokol = pkgs.fetchFromGitHub {
    owner = "floooh";
    repo = "sokol";
    rev = "28f9d8d44d92dab8536791a9f7d13d7e911a2b39";
    sha256 = "sha256-2KdUPf0ceUeh8Fd+VDoOdJKmE6ZjjZnW8S5apDxniDk=";
  };
in

pkgs.mkShell {
  nativeBuildInputs = with pkgs; [
    pkg-config
    gcc
    gnumake
    sdl3
    libfaketime
    imagemagick
    clang-tools # $ make format
    bc # ./track.sh
    osslsigncode openssl # ./gen_cert.sh
    qpdf # ./pack_source.sh
    # --- GPU MONITORING PACKAGES ---
    # for Intel xe graphics driver, the track.sh script does the job well enough.
    #nvtopPackages.intel
    #nvtopPackages.amd
    #nvtopPackages.full
    #intel-gpu-tools
    #amdgpu_top
    #sokol needs:
    libGL.dev
    libx11.dev
    wayland.dev
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
    
    # Create atomic soft symbolic targets for compiler search routes
    if [ ! -d "./sokol" ]; then
      ln -sfn "${srcSokol}" ./sokol
    fi
    if [ ! -f "sokol_gfx.h" ]; then
      ln -sfn ./sokol/sokol_gfx.h ./sokol_gfx.h
    fi
    if [ ! -f "sokol_log.h" ]; then
      ln -sfn ./sokol/sokol_log.h ./sokol_log.h
    fi

    if [ -d .git ]; then
        git config core.pager "less -x4"
    fi
    export NIX_CFLAGS_COMPILE="-I${pkgs.libGL.dev}/include -I${pkgs.libx11.dev}/include $NIX_CFLAGS_COMPILE"

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
