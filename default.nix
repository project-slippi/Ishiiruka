{ sources ? import ./nix/sources.nix }:
with rec {
  overlay = _: pkgs: { niv = import sources.niv { }; };
  pkgs = import sources.nixpkgs { overlays = [ overlay ]; };
  slippiDesktopApp = sources.slippi-desktop-app;
  inherit sources;
}; {
  netplay = pkgs.callPackage ./nix/slippi.nix { };
  playback = pkgs.callPackage ./nix/slippi.nix {
    playbackSlippi = true;
    inherit slippiDesktopApp;
  };
}
