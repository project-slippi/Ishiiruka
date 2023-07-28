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
/// down (at whatever point) via the corresponding `slprs_exi_device_destroy` function.
///
/// The returned pointer from this should *not* be used after calling `slprs_exi_device_destroy`.
uintptr_t slprs_exi_device_create();

/// The C++ (Dolphin) side of things should call this to notify the Rust side that it
/// can safely shut down and clean up.
void slprs_exi_device_destroy(uintptr_t exi_device_instance_ptr);

/// This method should be called from the EXI device subclass shim that's registered on
/// the Dolphin side, corresponding to:
///
/// `virtual void DMAWrite(u32 _uAddr, u32 _uSize);`
void slprs_exi_device_dma_write(uintptr_t exi_device_instance_ptr,
                                const uint8_t *address,
                                const uint8_t *size);

/// This method should be called from the EXI device subclass shim that's registered on
/// the Dolphin side, corresponding to:
///
/// `virtual void DMARead(u32 _uAddr, u32 _uSize);`
void slprs_exi_device_dma_read(uintptr_t exi_device_instance_ptr,
                               const uint8_t *address,
                               const uint8_t *size);

/// Configures the Jukebox process. This needs to be called after the EXI device is created
/// in order for certain pieces of Dolphin to be properly initalized; this may change down
/// the road though and is not set in stone.
void slprs_exi_device_configure_jukebox(uintptr_t exi_device_instance_ptr,
                                        bool is_enabled,
                                        const uint8_t *m_p_ram,
                                        const char *iso_path,
                                        int (*get_dolphin_volume_fn)());

/// This should be called from the Dolphin LogManager initialization to ensure that
/// all logging needs on the Rust side are configured appropriately.
///
/// For more information, consult `dolphin_logger::init`.
///
/// Note that `logger_fn` cannot be type-aliased here, otherwise cbindgen will
/// mess up the header output. That said, the function type represents:
///
/// ```
/// void Log(level, log_type, msg);
/// ```
void slprs_logging_init(void (*logger_fn)(int, int, const char*));

/// Registers a log container, which mirrors a Dolphin `LogContainer` (`RustLogContainer`).
///
/// See `dolphin_logger::register_container` for more information.
void slprs_logging_register_container(const char *kind,
                                      int log_type,
                                      bool is_enabled,
                                      int default_log_level);

/// Updates the configuration for a registered logging container.
///
/// For more information, see `dolphin_logger::update_container`.
void slprs_logging_update_container(const char *kind, bool enabled, int level);

} // extern "C"
