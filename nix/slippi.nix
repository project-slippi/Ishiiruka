{ stdenv, lib, gcc, slippiDesktopApp ? false, playbackSlippi ? false
, mesa_drivers, mesa_glu, mesa, pkgconfig, cmake, bluez, ffmpeg, libao, libGLU
, gtk2, gtk3, glib, gettext, xorg, readline, openal, libevdev, portaudio, libusb
, libpulseaudio, libudev, gnumake, wxGTK31, gdk-pixbuf, soundtouch, miniupnpc
, mbedtls, curl, lzo, sfml, enet, xdg_utils, hidapi }:
stdenv.mkDerivation rec {
  pname = "slippi-ishiiruka";
  version = "2.2.1";
  name =
    "${pname}-${version}-${if playbackSlippi then "playback" else "netplay"}";
  src = builtins.path {
    path = ../.;
    inherit name;
  };

  cc = gcc;

  enableParallelBuilding = true;
  outputs = [ "out" ];
  makeFlags = [ "VERSION=us" "-s" "VERBOSE=1" ];
  hardeningDisable = [ "format" ];

  cmakeFlags = [
    "-DLINUX_LOCAL_DEV=true"
    "-DGTK2_GLIBCONFIG_INCLUDE_DIR=${glib.out}/lib/glib-2.0/include"
    "-DGTK2_GDKCONFIG_INCLUDE_DIR=${gtk2.out}/lib/gtk-2.0/include"
    "-DGTK2_INCLUDE_DIRS=${gtk2.out}/lib/gtk-2.0"
    "-DENABLE_LTO=True"
  ] ++ lib.optional (playbackSlippi) "-DIS_PLAYBACK=true";

  postBuild = with lib;
    optionalString playbackSlippi ''
      rm -rf ../Data/Sys/GameSettings
      cp -r "${slippiDesktopApp}/app/dolphin-dev/overwrite/Sys/GameSettings" ../Data/Sys
    '' + ''
      touch Binaries/portable.txt
      cp -r -n ../Data/Sys/ Binaries/
      cp -r Binaries/ $out
      mkdir -p $out/bin
    '';

  installPhase = if playbackSlippi then ''
    ln -s $out/dolphin-emu $out/bin/slippi-playback
  '' else ''
    ln -s $out/dolphin-emu $out/bin/slippi-netplay
  '';

  nativeBuildInputs = [ pkgconfig cmake ];
  buildInputs = [
    mesa_drivers
    mesa_glu
    mesa
    pkgconfig
    bluez
    ffmpeg
    libao
    libGLU
    gtk2
    gtk3
    glib
    gettext
    xorg.libpthreadstubs
    xorg.libXrandr
    xorg.libXext
    xorg.libX11
    xorg.libSM
    readline
    openal
    libevdev
    xorg.libXdmcp
    portaudio
    libusb
    libpulseaudio
    libudev
    gnumake
    wxGTK31
    gdk-pixbuf
    soundtouch
    miniupnpc
    mbedtls
    curl
    lzo
    sfml
    enet
    xdg_utils
    hidapi
  ];
}
