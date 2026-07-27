{ pkgs ? import <nixpkgs> { config = { allowUnfree = true; }; } }:

let
  my-python-env = pkgs.python3.withPackages (ps: with ps; [
    opencv4
    numpy
    scipy
  ]); # precompute_clockface: approximate the clockface lame curve and bake it into the c source instead of wasting cycles on it during pre-bake/live rendering
in

pkgs.mkShell {
  name = "catclock-sdl3-python-dev-env";

  nativeBuildInputs = with pkgs; [
    qpdf # ./pack_source.sh
    groff groff.perl # ./cmd2pdf.sh dump.pdf 'grep -A 40 "typedef struct sg_image_desc" sokol/sokol_gfx.h' 'grep -A 25 "sg_pixel_format" sokol/sokol_gfx.h'
    pdftk
    my-python-env
  ];

  shellHook = ''
    if [ -d .git ]; then
        git config core.pager "less -x4"
    fi
  '';
}
