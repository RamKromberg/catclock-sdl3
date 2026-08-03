{ pkgs ? import <nixpkgs> { config = { allowUnfree = true; }; } }:

let
  my-python-env = pkgs.python3.withPackages (ps: with ps; [
    opencv4
    numpy
    scipy
  ]); # find vertices for the svg outline
in

pkgs.mkShell {
  name = "catclock-sdl3-python-dev-env";

  nativeBuildInputs = with pkgs; [
    my-python-env
    imagemagick
    # compress pngs
    optipng # optipng
    zopfli # zopflipng
    exiftool # exiftool
  ];
}
