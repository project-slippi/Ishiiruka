use std::fs::File;
use std::io::{Read, Seek};

use crate::fst::{self, IsoKind};
use crate::Result;

// Constants for CISO images (compressed ISO)
const CISO_HEADER_SIZE: usize = 0x8000;
const CISO_BLOCK_MAP_SIZE: usize = CISO_HEADER_SIZE - 0x8;

// (Block Size, Block Map)
type CisoHeader = (u32, [u8; CISO_BLOCK_MAP_SIZE]);

/// Get the header of a ciso disc image. If the provided file is not a ciso,
/// `None` will be returned
pub(crate) fn get_ciso_header(iso: &mut File) -> Result<Option<CisoHeader>> {
    match fst::get_iso_kind(iso)? {
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
pub(crate) fn get_ciso_offset(header: &CisoHeader, offset: u32) -> Option<u32> {
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
