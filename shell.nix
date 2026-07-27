{ pkgs ? import <nixpkgs> { config = { allowUnfree = true; }; } }:

let
  windowsPkgs = pkgs.pkgsCross.mingwW64;

  # 2026-07-29 Runtime Engine Source Tree
  sokolSrc = pkgs.fetchFromGitHub {
    owner = "floooh";
    repo = "sokol";
    rev = "f26aaf6deeee5a4a07d83f3cd7151516bafa09ad";
    sha256 = "sha256-qhvjnC6qn14SBsq39efZrCfpIgKPZBnteMBINDssf9c=";
  };

  # 2026-07-11 Static Compiler Tool Binary Extraction Derivation
  sokolCompiler = pkgs.stdenv.mkDerivation {
    pname = "sokol-shdc";
    version = "2026-06-13";

    src = pkgs.fetchFromGitHub {
      owner = "floooh";
      repo = "sokol-tools-bin";
      rev = "9adef5465d8b9e7f412b0ffd48017e2741628c27";
      sha256 = "sha256-WySvVT2rDfWIHcKqz/niVkNpbdPEWsr33k1VyoaRJ/A=";
    };

    dontBuild = true;
    dontConfigure = true;

    installPhase = ''
      mkdir -p $out/bin
      cp bin/linux/sokol-shdc $out/bin/sokol-shdc
      chmod +x $out/bin/sokol-shdc
    '';
  };
in

pkgs.mkShell {
  name = "catclock-sdl3-dev-env";

  nativeBuildInputs = with pkgs; [
    pkg-config
    gcc
    gnumake
    libfaketime
    imagemagick
    clang-tools # $ make format
    bc # ./track.sh
    osslsigncode openssl # ./gen_cert.sh
    qpdf # ./pack_source.sh
    groff groff.perl # ./cmd2pdf.sh dump.pdf 'grep -A 40 "typedef struct sg_image_desc" sokol/sokol_gfx.h' 'grep -A 25 "sg_pixel_format" sokol/sokol_gfx.h'
    pdftk
    # --- GPU MONITORING PACKAGES ---
    # for Intel xe graphics driver, the track.sh script does the job well enough.
    #nvtopPackages.intel
    #nvtopPackages.amd
    #nvtopPackages.full
    #intel-gpu-tools
    #amdgpu_top
    sdl3
    sokolCompiler
    glsl_analyzer # formatter. use `#pragma sokol` to prefix sokol tags ( https://github.com/floooh/sokol-tools/blob/master/docs/sokol-shdc.md )
    libGL.dev libGL libGLU
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
    
    # Automated Flat Standalone Extraction Pass
    mkdir -p ./freetype
    SRC_TARBALL="${pkgs.freetype.src}"
    if [ ! -f ./freetype/ftgrays.c ]; then
      echo "Extracting detached micro-rasterizer assets..."
      TMP_UNPACK=$(mktemp -d)
      tar -xf "$SRC_TARBALL" -C "$TMP_UNPACK" --strip-components=1
      cp "$TMP_UNPACK/src/smooth/ftgrays.c" ./freetype/ftgrays.c
      cp "$TMP_UNPACK/src/smooth/ftgrays.h" ./freetype/ftgrays.h
      cp "$TMP_UNPACK/include/freetype/ftimage.h" ./freetype/ftimage.h
      rm -rf "$TMP_UNPACK"
    fi

    export LD_LIBRARY_PATH="${pkgs.lib.makeLibraryPath [
      pkgs.libGL
      pkgs.glib
      pkgs.libx11
    ]}:$LD_LIBRARY_PATH"

    if [ ! -d "./sokol" ]; then
      ln -sfn "${sokolSrc}" ./sokol
    fi

    if [ -d .git ]; then
        git config core.pager "less -x4"
    fi

    export NIX_CFLAGS_COMPILE="-I${pkgs.libGL.dev}/include -I${pkgs.libx11.dev}/include $NIX_CFLAGS_COMPILE"

    export LD_PRELOAD="${pkgs.libfaketime}/lib/libfaketime.so.1"

    export WINDOWS_SDL_PREFIX="${windowsPkgs.sdl3}"
    export WINDOWS_SDL_OUT="${windowsPkgs.sdl3.out}"

    echo "=================================================="
    echo " Kit-Cat Clock Cross-Platform Compiler Shell Active "
    echo "   -> Run 'make' to compile for native Linux"
    echo "   -> Run 'make format' to format code using WebKit style"
    echo "   -> Run 'make windows' to cross-compile for Windows (.exe)"
    echo "=================================================="
  '';
}
