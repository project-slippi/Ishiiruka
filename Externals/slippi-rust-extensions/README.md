## Slippi Rust Extensions
This external module houses various Slippi-specific bits of functionality and is ultimately linked into the
Dolphin executable and instrumented via the C FFI.

The core of it is a "shadow" EXI device that's bound to the lifetime of the C++ Slippi EXI Device subclass.

The build script in this repository automatically generates C bindings that get output to 
`includes/SlippiRustExtensions.h`, and the Dolphin CMake and Visual Studio projects are pre-configured to find
this header and link the necessary libraries and dlls.

### The _playback_ feature
There is an optional feature flag for this repository for playback-specific functionality. This will automatically be
set via either CMake or Visual Studio if you're building a Playback-enabled project, but if you're building and testing
out of band you may need to enable this flag, e.g:

```
cargo build --release --features playback
```
