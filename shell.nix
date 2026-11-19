{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  packages = with pkgs; [
    cmake
    gcc
    just
    python3
  ];

  shellHook = ''
    echo "SPL development shell"
    echo "Run 'just' to build and test, or 'just example' to run the example."
  '';
}
