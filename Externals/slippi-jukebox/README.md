## Slippi Jukebox
The real author can fill this in.

## Generating the bindings for Ishiiruka
You need cbindgen installed. For now, re-run this after any changes have been made to the library.

``` sh
cbindgen --config cbindgen.toml --crate slippi_jukebox --output includes/SlippiJukebox.h
```
