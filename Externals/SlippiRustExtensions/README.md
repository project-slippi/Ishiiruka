## Slippi Rust Extensions
This external module houses various Slippi-specific bits of functionality and is ultimately linked into the Dolphin executable and instrumented via the C FFI.

### Project Structure

#### ffi
The core library. Exposes C FFI functions for Dolphin to call.

The build script in this crate automatically generates C bindings that get output to `ffi/includes/SlippiRustExtensions.h`, and the Dolphin CMake and Visual Studio projects are pre-configured to find this header and link the necessary libraries and dlls.

#### exi
An EXI device that receives forwarded calls from the Dolphin EXI device.

#### logger
A library that connects Dolphin `LogContainer`s and `tracing`.

### The _playback_ feature
There is an optional feature flag for this repository for playback-specific functionality. This will automatically be set via either CMake or Visual Studio if you're building a Playback-enabled project, but if you're building and testing out of band you may need to enable this flag, e.g:

```
cargo build --release --features playback
```
