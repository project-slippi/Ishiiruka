# Building from Checkout
```sh
# netplay
$ nix-build -A netplay
# playback
$ nix-build -A playback
```

# Adding to nixos-config-file
```nix
let slippi = (import (builtins.fetchGit {
  url = "https://github.com/project-slippi/Ishiiruka";
  rev = "<commit-id>";
  ref = "<branch>";
})) { };
in { 
  environment.systemPackages = [
    slippi.netplay
    slippi.playback
  ];
}
```

# Updating Dependencies
```sh
$ nix-env -iA nixpkgs.niv
$ niv update
Updating all packages
  Package: nixpkgs
  Package: niv
Done: Updating all packages
```

```diff
$ git diff nix/sources.json
diff --git a/nix/sources.json b/nix/sources.json
index ac280d63d..dc54fcea3 100644
--- a/nix/sources.json
+++ b/nix/sources.json
@@ -5,10 +5,10 @@
         "homepage": "https://github.com/nmattia/niv",
         "owner": "nmattia",
         "repo": "niv",
-        "rev": "febd3530f0c2f2fb74752ee4d9dd2518d302f618",
-        "sha256": "1gifi50k4h6wk9ix0yvp66p7jk8rrqgr39r5rf4lyha6pbs7dbk6",
+        "rev": "ab9cc41caf44d1f1d465d8028e4bc0096fd73238",
+        "sha256": "17k52n8zwp832cqifsc4458mhy4044wmk22f807171hf6p7l4xvr",
         "type": "tarball",
-        "url": "https://github.com/nmattia/niv/archive/febd3530f0c2f2fb74752ee4d9dd2518d302f618.tar.gz",
+        "url": "https://github.com/nmattia/niv/archive/ab9cc41caf44d1f1d465d8028e4bc0096fd73238.tar.gz",
         "url_template": "https://github.com/<owner>/<repo>/archive/<rev>.tar.gz"
     },
     "nixpkgs": {
@@ -17,10 +17,10 @@
         "homepage": "https://github.com/NixOS/nixpkgs",
         "owner": "NixOS",
         "repo": "nixpkgs-channels",
-        "rev": "6148f6360310366708dff42055a0ba0afa963101",
-        "sha256": "1j91hxfak1kark776klszakvg0a9yv77p7lnrvj7g32v6g20qdsk",
+        "rev": "b8c367a7bd05e3a514c2b057c09223c74804a21b",
+        "sha256": "0y17zxhwdw0afml2bwkmhvkymd9fv242hksl3l3xz82gmlg1zks4",
         "type": "tarball",
-        "url": "https://github.com/NixOS/nixpkgs-channels/archive/6148f6360310366708dff42055a0ba0afa963101.tar.gz",
+        "url": "https://github.com/NixOS/nixpkgs-channels/archive/b8c367a7bd05e3a514c2b057c09223c74804a21b.tar.gz",
         "url_template": "https://github.com/<owner>/<repo>/archive/<rev>.tar.gz"
     }
 }
```

# Notes
Use the `User folder path` flag to save user configuration:
```sh
$ mkdir -p ~/.slippi && slippi-netplay -u ~/.slippi
```
