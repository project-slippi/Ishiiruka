use std::collections::HashMap;
use std::ffi::CStr;
use std::fs::File;
use std::io::{Read, Seek};

use crate::tracks::{get_track_id_by_filename, TrackId};
use crate::utils::read_from_file;
use crate::JukeboxError::*;
use crate::Result;

// Constants for CISO images (compressed ISO)
const CISO_HEADER_SIZE: usize = 0x8000;
const CISO_BLOCK_MAP_SIZE: usize = CISO_HEADER_SIZE - 0x8;

// (Block Size, Block Map)
type CisoHeader = (u32, [u8; CISO_BLOCK_MAP_SIZE]);

#[derive(Debug, Clone, Copy)]
enum IsoKind {
    Standard,
    Ciso,
    Unknown,
}

/// Produces a hashmap containing offsets and sizes of .hps files contained within the iso
/// These can be looked up by TrackId
pub(crate) fn create_track_map(iso: &mut File) -> Result<HashMap<TrackId, (u32, u32)>> {
    const RAW_FST_LOCATION_OFFSET: u32 = 0x424;
    const RAW_FST_SIZE_OFFSET: u32 = 0x0428;
    const FST_ENTRY_SIZE: usize = 0xC;

    // Get the CISO header (block size and block map) of the provided iso.
    // If the provided iso is not a CISO, this will be `None`
    let ciso_header = match get_iso_kind(iso)? {
        IsoKind::Standard => None,
        IsoKind::Ciso => get_ciso_header(iso)?,
        IsoKind::Unknown => return Err(UnsupportedIso),
    };

    // `get_true_offset` is a fn that takes an offset for a standard disc image, and
    // returns it's _true_ offset (which differs between standard and ciso)
    let get_true_offset = create_offset_locator_fn(ciso_header.as_ref());

    // Filesystem Table (FST)
    let fst_location_offset = get_true_offset(RAW_FST_LOCATION_OFFSET)
        .ok_or(FstParseError("FST location offset is missing from the ISO".to_string()))?;

    let fst_size_offset =
        get_true_offset(RAW_FST_SIZE_OFFSET).ok_or(FstParseError("FST size offset is missing from the ISO".to_string()))?;

    let fst_location = u32::from_be_bytes(
        read_from_file(iso, fst_location_offset as u64, 0x4)?
            .try_into()
            .map_err(|_| FstParseError("Unable to read FST offset as u32".to_string()))?,
    );
    let fst_location = get_true_offset(fst_location).ok_or(FstParseError("FST location is missing from the ISO".to_string()))?;

    if fst_location <= 0 {
        return Err(FstParseError("FST location is 0".to_string()));
    }

    let fst_size = u32::from_be_bytes(
        read_from_file(iso, fst_size_offset as u64, 0x4)?
            .try_into()
            .map_err(|_| FstParseError("Unable to read FST size as u32".to_string()))?,
    );

    let fst = read_from_file(iso, fst_location as u64, fst_size as usize)?;

    // FST String Table
    let str_table_offset = read_u32(&fst, 0x8)? as usize * FST_ENTRY_SIZE;

    // Collect the .hps file metadata in the FST into a hash map
    Ok(fst[..str_table_offset]
        .chunks(FST_ENTRY_SIZE)
        .filter_map(|entry| {
            let is_file = entry[0] == 0;
            let name_offset = str_table_offset + read_u24(entry, 0x1).ok()? as usize;
            let offset = read_u32(entry, 0x4).ok()?;
            let size = read_u32(entry, 0x8).ok()?;

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
        .ok_or(FstParseError("Too few bytes to read u24".to_string()))?;
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
            .ok_or(FstParseError("Too few bytes to read u32".to_string()))?
            .try_into()
            .unwrap_or_else(|_| unreachable!("u32::BITS / 8 is always 4")),
    ))
}

/// Given an iso file, determine what kind it is
fn get_iso_kind(iso: &mut File) -> Result<IsoKind> {
    // Get the first four bytes
    iso.rewind().map_err(IsoSeekError)?;
    let mut initial_bytes = [0; 4];
    iso.read_exact(&mut initial_bytes).map_err(IsoReadError)?;

    // Get the four bytes at 0x1c
    iso.seek(std::io::SeekFrom::Start(0x1c)).map_err(IsoSeekError)?;
    let mut dvd_magic_bytes = [0; 4];
    iso.read_exact(&mut dvd_magic_bytes).map_err(IsoReadError)?;

    match (initial_bytes, dvd_magic_bytes) {
        // DVD Magic Word
        (_, [0xc2, 0x33, 0x9F, 0x3D]) => Ok(IsoKind::Standard),
        // CISO header
        ([0x43, 0x49, 0x53, 0x4F], _) => Ok(IsoKind::Ciso),
        _ => Ok(IsoKind::Unknown),
    }
}

/// Get the header of a ciso disc image. If the provided file is not a ciso,
/// `None` will be returned
fn get_ciso_header(iso: &mut File) -> Result<Option<CisoHeader>> {
    match get_iso_kind(iso)? {
        IsoKind::Ciso => {
            // Get the block size
            let mut block_size = [0; 0x4];
            iso.seek(std::io::SeekFrom::Start(0x4))?;
            iso.read_exact(&mut block_size)?;
            let block_size = u32::from_le_bytes(block_size);

            // Get the block map
            let mut block_map = [0; CISO_BLOCK_MAP_SIZE];
            iso.seek(std::io::SeekFrom::Start(0x8))?;
            iso.read_exact(&mut block_map)?;

            Ok(Some((block_size, block_map)))
        },
        _ => Ok(None),
    }
}

// Given an offset for an standard disc image, return the offset for a ciso
// image
fn get_ciso_offset(header: &CisoHeader, offset: u32) -> Option<u32> {
    let block_size = header.0 as usize;
    let block_map = header.1;
    let offset = offset as usize;

    // Get position of the block that `offset` is contained in
    let block_pos = offset / block_size;

    // If the block map has a 0 (no data) at `block_pos` (or if the `block_pos`
    // is out of bounds), return None
    if block_pos >= CISO_BLOCK_MAP_SIZE || block_map[block_pos] == 0 {
        return None;
    }

    // Otherwise return the offset in the ciso by accounting for all of the
    // empty data blocks up until `block_pos`
    let empty_block_count = block_map[0..block_pos].iter().filter(|&b| *b == 0).count();
    let ciso_offset = offset + CISO_HEADER_SIZE - (block_size * empty_block_count);
    Some(ciso_offset as u32)
}

/// When we want to read data from an offset for a _standard_ disc image, we need
/// to know what the _actual_ offset is for the iso that we _have on hand_.
/// This can vary depending on the kind of disc image that we are dealing
/// with (standard vs ciso, for example)
///
/// This HoF returns a fn that can be used to locate the true offset
///
/// Example Usage:
/// ```
/// let get_true_offset = create_offset_locator(None);
/// let true_offset = get_true_offset(0x424);
/// ```
fn create_offset_locator_fn(ciso_header: Option<&CisoHeader>) -> impl Fn(u32) -> Option<u32> + '_ {
    move |offset| match ciso_header {
        Some(ciso_header) => get_ciso_offset(ciso_header, offset),
        None => Some(offset),
    }
}
