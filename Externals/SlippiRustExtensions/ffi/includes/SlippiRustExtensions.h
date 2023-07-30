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

/// Initializes a new SlippiGameReporter and leaks it, returning the instance
/// pointer after doing so.
uintptr_t slprs_game_reporter_create(const char *uid, const char *play_key, const char *iso_path);

/// Moves ownership of the `GameReport` at the specified address to the
/// `SlippiGameReporter` at the corresponding address.
///
/// The reporter will manage the actual... reporting.
void slprs_game_reporter_start_report(uintptr_t instance_ptr, uintptr_t game_report_instance_ptr);

/// Initializes a new GameReport and leaks it, returning the instance pointer
/// after doing so.
///
/// This is expected to ultimately be passed to the game reporter, which will handle
/// destruction and cleanup.
uintptr_t slprs_game_report_create();

/// Takes ownership of the `PlayerReport` at the specified address, adding it to the
/// `GameReport` at the corresponding address.
void slprs_game_report_add_player_report(uintptr_t instance_ptr,
                                         uintptr_t player_report_instance_ptr);

/// Sets the `match_id` on the game report at the address of `instance_ptr`.
void slprs_game_report_set_match_id(uintptr_t instance_ptr, const char *match_id);

/// Sets the `duration_frames` on the game report at the address of `instance_ptr`.
void slprs_game_report_set_duration_frames(uintptr_t instance_ptr, uint32_t duration);

/// Sets the `game_index` on the game report at the address of `instance_ptr`.
void slprs_game_report_set_game_index(uintptr_t instance_ptr, uint32_t index);

/// Sets the `tie_break_index` on the game report at the address of `instance_ptr`.
void slprs_game_report_set_tie_break_index(uintptr_t instance_ptr, uint32_t index);

/// Sets the `winner_index` on the game report at the address of `instance_ptr`.
void slprs_game_report_set_winner_index(uintptr_t instance_ptr, int8_t index);

/// Sets the `game_end_method` on the game report at the address of `instance_ptr`.
void slprs_game_report_set_game_end_method(uintptr_t instance_ptr, uint8_t method);

/// Sets the `lras_initiator` on the game report at the address of `instance_ptr`.
void slprs_game_report_set_lras_initiator(uintptr_t instance_ptr, int8_t initiator);

/// Sets the `stage_id` on the game report at the address of `instance_ptr`.
void slprs_game_report_set_stage_id(uintptr_t instance_ptr, int32_t stage_id);

/// Initializes a new PlayerReport and leaks it, returning the instance pointer
/// after doing so.
uintptr_t slprs_player_report_create();

/// Sets the `uid` on the player report at the address of `instance_ptr`.
void slprs_player_report_set_uid(uintptr_t instance_ptr, const char *uid);

/// Sets the `slot_type` on the player report at the address of `instance_ptr`.
void slprs_player_report_set_slot_type(uintptr_t instance_ptr, uint8_t slot_type);

/// Sets the `damage_done` on the player report at the address of `instance_ptr`.
void slprs_player_report_set_damage_done(uintptr_t instance_ptr, double damage);

/// Sets the `stocks_remaining` on the player report at the address of `instance_ptr`.
void slprs_player_report_set_stocks_remaining(uintptr_t instance_ptr, uint8_t stocks);

/// Sets the `character_id` on the player report at the address of `instance_ptr`.
void slprs_player_report_set_character_id(uintptr_t instance_ptr, uint8_t character_id);

/// Sets the `color_id` on the player report at the address of `instance_ptr`.
void slprs_player_report_set_color_id(uintptr_t instance_ptr, uint8_t color_id);

/// Sets the `starting_stocks` on the player report at the address of `instance_ptr`.
void slprs_player_report_set_starting_stocks(uintptr_t instance_ptr, int64_t stocks);

/// Sets the `starting_percent` on the player report at the address of `instance_ptr`.
void slprs_player_report_set_starting_percent(uintptr_t instance_ptr, int64_t percent);

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
