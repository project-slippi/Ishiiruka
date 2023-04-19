#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>

extern "C" {

/// Creates and leaks a shadow EXI device.
///
/// The C++ (Dolphin) side of things should call this and pass the appropriate arguments. At
/// that point, everything on the Rust side is its own universe, and should be told to shut
/// down (at whatever point) via the corresponding `slprs_jukebox_destroy` function.
///
/// The returned pointer from this should *not* be used after calling `slprs_exi_device_destroy`.
uintptr_t slprs_exi_device_create();

/// The "exit point" for the library - the C++ (Dolphin) side of things should call this to
/// notify the Rust side that it can safely shut down and clean up.
void slprs_exi_device_destroy(uintptr_t exi_device_instance);

/// This method should be called from the EXI device subclass shim that's registered on
/// the Dolphin side, corresponding to:
///
/// `virtual void DMAWrite(u32 _uAddr, u32 _uSize);`
void slprs_exi_device_dma_write(uintptr_t exi_device_instance,
                                const uint8_t *address,
                                const uint8_t *size);

/// This method should be called from the EXI device subclass shim that's registered on
/// the Dolphin side, corresponding to:
///
/// `virtual void DMARead(u32 _uAddr, u32 _uSize);`
void slprs_exi_device_dma_read(uintptr_t exi_device_instance,
                               const uint8_t *address,
                               const uint8_t *size);

/// Kicks off the Jukebox process. This needs to be called after the EXI device is created
/// in order for certain pieces of Dolphin to be properly initalized; this may change down
/// the road though and is not set in stone.
void slprs_exi_device_start_jukebox(uintptr_t exi_device_instance, const uint8_t *m_pRAM);

/// This should be called from the Dolphin LogManager initialization to ensure that
/// all logging needs on the Rust side are configured appropriately.
///
/// *Usually* you do not want a library installing a global logger, however our use case is
/// not so standard: this library does in a sense act as an application due to the way it's
/// called into, and we *want* a global subscriber.
///
/// Note that `logger_fn` cannot be type-aliased here, otherwise cbindgen will
/// mess up the header output. That said, the function type represents:
///
/// ```
/// void Log(level, log_type, filename, line_number, msg);
/// ```
void slprs_logging_init(void (*logger_fn)(int, int, const char*, int, const char*));

/// Registers a log container, which mirrors a Dolphin `LogContainer` (`RustLogContainer`).
///
/// This enables passing a configured log level and/or enabled status across the boundary from
/// Dolphin to our tracing subscriber setup. This is important as we want to short-circuit any
/// allocations during log handling that aren't necessary (e.g if a log is outright disabled).
void slprs_logging_register_container(const char *kind,
                                      int log_type,
                                      bool is_enabled,
                                      int default_log_level);

/// Sets a particular log container to a new enabled state. When a log container is in a disabled
/// state, no allocations will happen behind the scenes for any logging period.
void slprs_logging_update_container(const char *kind, bool enabled, int level);

} // extern "C"
