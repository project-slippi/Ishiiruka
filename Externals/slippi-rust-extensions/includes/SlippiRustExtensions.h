#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>

/// A type that mirrors a function over on the C++ side; because the Jukebox exists as
/// a dylib, it can't depend on any functions from the host application - but we _can_
/// pass in a hook/callback fn.
using ForeignLoggerFn = void(*)(int, const char*, int, const char*);

extern "C" {

/// The main entry point of the library - Ishiiruka calls into this and
/// passes the `ram_offset` to work with.
void start_slippi_jukebox(const uint8_t *ram_offset, ForeignLoggerFn logger_fn);

/// Call this to end the jukebox, I guess.
void shutdown_slippi_jukebox();

} // extern "C"
