#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>

extern "C" {

/// This is the entry point for the library - the C++ (Dolphin) side of things should call this
/// and pass the appropriate pointers and functions. At that point, everything on the Rust side
/// is its own universe, and should be told to shut down (at whatever point) via the corresponding
/// `slprs_exi_device_destroy` function.
///
/// The returned pointer from this should be used for any of the subsequent `slprs_exi_device_`
/// calls, and should *not* be used after calling `slprs_exi_device_destroy`.
uintptr_t slprs_exi_device_create(const uint8_t *ram_offset,
                                  void (*logger_fn)(int, const char*, int, const char*));

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

/// The "exit point" for the library - the C++ (Dolphin) side of things should call this to
/// notify the Rust side that it can safely shut down and clean up.
void slprs_exi_device_destroy(uintptr_t exi_device_instance);

} // extern "C"
