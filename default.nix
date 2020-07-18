{ sources ? import ./nix/sources.nix }:
with rec {
  overlay = _: pkgs: { niv = import sources.niv { }; };
  pkgs = import sources.nixpkgs { overlays = [ overlay ]; };
  inherit sources;
}; {
  netplay = pkgs.callPackage ./nix/slippi.nix { playbackSlippi = false; };
  playback = pkgs.callPackage ./nix/slippi.nix { playbackSlippi = true; };
}
