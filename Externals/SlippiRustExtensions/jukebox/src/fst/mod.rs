use std::collections::HashMap;
use std::ffi::CStr;
use std::fs::File;
use std::io::{Read, Seek};

use crate::tracks::{get_track_id_by_filename, TrackId};
use crate::utils::copy_bytes_from_file;
use crate::JukeboxError::*;
use crate::Result;

mod ciso;

#[derive(Debug, Clone, Copy)]
enum IsoKind {
    Standard,
    Ciso,
    Unknown,
}

/// Produces a hashmap containing offsets and sizes of .hps files contained within the iso.
/// These can be looked up by TrackId
///
/// e.g.
/// `TrackId => (offset in the iso, file size)`
pub(crate) fn create_track_map(iso_path: &str) -> Result<HashMap<TrackId, (u64, usize)>> {
    const RAW_FST_LOCATION_OFFSET: u64 = 0x424;
    const RAW_FST_SIZE_OFFSET: u64 = 0x428;
    const FST_ENTRY_SIZE: usize = 0xC;

    // `get_true_offset` is a fn that takes an offset for a standard disc image, and
    // returns it's _true_ offset (which differs between standard and ciso)
    let get_true_offset = create_offset_locator_fn(iso_path)?;
    let mut iso = File::open(iso_path)?;

    // Filesystem Table (FST)
    let fst_location_offset =
        get_true_offset(RAW_FST_LOCATION_OFFSET).ok_or(FstParse("FST location offset is missing from the ISO".to_string()))?;

    let fst_size_offset =
        get_true_offset(RAW_FST_SIZE_OFFSET).ok_or(FstParse("FST size offset is missing from the ISO".to_string()))?;

    let fst_location = u32::from_be_bytes(
        copy_bytes_from_file(&mut iso, fst_location_offset as u64, 0x4)?
            .try_into()
            .map_err(|_| FstParse("Unable to read FST offset as u32".to_string()))?,
    );
    let fst_location =
        get_true_offset(fst_location as u64).ok_or(FstParse("FST location is missing from the ISO".to_string()))?;

    if fst_location <= 0 {
        return Err(FstParse("FST location is 0".to_string()));
    }

    let fst_size = u32::from_be_bytes(
        copy_bytes_from_file(&mut iso, fst_size_offset as u64, 0x4)?
            .try_into()
            .map_err(|_| FstParse("Unable to read FST size as u32".to_string()))?,
    );

    let fst = copy_bytes_from_file(&mut iso, fst_location as u64, fst_size as usize)?;

    // FST String Table
    let str_table_offset = read_u32(&fst, 0x8)? as usize * FST_ENTRY_SIZE;

    // Collect the .hps file metadata in the FST into a hash map
    Ok(fst[..str_table_offset]
        .chunks(FST_ENTRY_SIZE)
        .filter_map(|entry| {
            let is_file = entry[0] == 0;
            let name_offset = str_table_offset + read_u24(entry, 0x1).ok()? as usize;
            let offset = read_u32(entry, 0x4).ok()? as u64;
            let size = read_u32(entry, 0x8).ok()? as usize;

            let name = CStr::from_bytes_until_nul(&fst[name_offset..]).ok()?.to_str().ok()?;

            if is_file && name.ends_with(".hps") {
                match get_track_id_by_filename(&name) {
                    Some(track_id) => {
                        let offset = get_true_offset(offset)?;
                        Some((track_id, (offset, size)))
                    },
                    None => None,
                }
            } else {
                None
            }
        })
        .collect())
}

/// Get an unsigned 24 bit integer from a byte slice
fn read_u24(bytes: &[u8], offset: usize) -> Result<u32> {
    let size = 3;
    let end = offset + size;
    let mut padded_bytes = [0; 4];
    let slice = &bytes
        .get(offset..end)
        .ok_or(FstParse("Too few bytes to read u24".to_string()))?;
    padded_bytes[1..4].copy_from_slice(slice);
    Ok(u32::from_be_bytes(padded_bytes))
}

/// Get an unsigned 32 bit integer from a byte slice
fn read_u32(bytes: &[u8], offset: usize) -> Result<u32> {
    let size = (u32::BITS / 8) as usize;
    let end: usize = offset + size;
    Ok(u32::from_be_bytes(
        bytes
            .get(offset..end)
            .ok_or(FstParse("Too few bytes to read u32".to_string()))?
            .try_into()
            .unwrap_or_else(|_| unreachable!("u32::BITS / 8 is always 4")),
    ))
}

/// Given an iso file, determine what kind it is
fn get_iso_kind(iso: &mut File) -> Result<IsoKind> {
    // Get the first four bytes
    iso.rewind().map_err(IsoSeek)?;
    let mut initial_bytes = [0; 4];
    iso.read_exact(&mut initial_bytes).map_err(IsoRead)?;

    // Get the four bytes at 0x1c
    iso.seek(std::io::SeekFrom::Start(0x1c)).map_err(IsoSeek)?;
    let mut dvd_magic_bytes = [0; 4];
    iso.read_exact(&mut dvd_magic_bytes).map_err(IsoRead)?;

    match (initial_bytes, dvd_magic_bytes) {
        // DVD Magic Word
        (_, [0xc2, 0x33, 0x9F, 0x3D]) => Ok(IsoKind::Standard),
        // CISO header
        ([0x43, 0x49, 0x53, 0x4F], _) => Ok(IsoKind::Ciso),
        _ => Ok(IsoKind::Unknown),
    }
}

/// When we want to read data from any given iso file, but we only know the
/// offset for a standard disc image, we need a way to be able to get the
/// _actual_ offset for the file we have on hand.
///
/// This can vary depending on the kind of disc image that we are dealing with
/// (standard vs ciso, for example)
///
/// This HoF returns a fn that can be used to locate the true offset.
///
/// Example Usage:
/// ```ignore
/// let get_true_offset = create_offset_locator_fn("/foo/bar.iso");
/// let offset = get_true_offset(0x424);
/// ```
fn create_offset_locator_fn(iso_path: &str) -> Result<impl Fn(u64) -> Option<u64> + '_> {
    let mut iso = File::open(iso_path)?;

    // Get the ciso header (block size and block map) of the provided file.
    // If the file is not a ciso, this will be `None`
    let ciso_header = match get_iso_kind(&mut iso)? {
        IsoKind::Standard => None,
        IsoKind::Ciso => ciso::get_ciso_header(&mut iso)?,
        IsoKind::Unknown => return Err(UnsupportedIso),
    };

    Ok(move |offset| match ciso_header {
        Some(ciso_header) => ciso::get_ciso_offset(&ciso_header, offset),
        None => Some(offset),
    })
}
