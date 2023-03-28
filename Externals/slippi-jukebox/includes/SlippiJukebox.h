#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>

extern "C" {

/// The main entry point of the library - Ishiiruka calls into this and
/// passes the `ram_offset` to work with.
void start_slippi_jukebox(const uint8_t *ram_offset);

/// Call this to end the jukebox, I guess.
void shutdown_slippi_jukebox();

} // extern "C"
