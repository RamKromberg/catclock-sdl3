{ pkgs ? import <nixpkgs> { config = { allowUnfree = true; }; } }:

let
  windowsPkgs = pkgs.pkgsCross.mingwW64;

  # last stable version before 2024-01-29 and 2024-01-30 when 363d53e through 06913be introduced the massive Spring Cleaning Overhaul that renamed sg_pass to attachments, deleted context pooling, removed sg_begin_default_pass, and flattened stage array names.
  srcSokol = pkgs.fetchFromGitHub {
    owner = "floooh";
    repo = "sokol";
    rev = "f58a78539e6a972700579ee72cb3f2d66f07088f";
    sha256 = "sha256-QGg1XXFFMjVo4gNn00CHouowywZQQko9tYnBFYAedJA=";
  };
  #$ git log -1 f58a78539e6a972700579ee72cb3f2d66f07088f
  #commit f58a78539e6a972700579ee72cb3f2d66f07088f
  #Author: Andre Weissflog <floooh@gmail.com>
  #Date:   Wed Jan 24 19:13:52 2024 +0100
  #
  #    remove dead link from readme
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
    groff groff.perl # ./cmd2pdf.sh dump.pdf 'grep -A 40 "typedef struct sg_image_desc" sokol_gfx.h' 'grep -A 25 "sg_pixel_format" sokol_gfx.h'
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

    # Capture the runtime package path where the real Windows DLL lives
    export WINDOWS_SDL_BIN="${windowsPkgs.sdl3.bin or windowsPkgs.sdl3}"
    
    echo "=================================================="
    echo " Kit-Cat Clock Cross-Platform Compiler Shell Active "
    echo "   -> Run 'make' to compile for native Linux"
    echo "   -> Run 'make format' to format code using WebKit style"
    echo "   -> Run 'make windows' to cross-compile for Windows (.exe)"
    echo "=================================================="
  '';
}
