use std::fs::File;
use std::io::{Read, Seek};

use crate::fst::{self, IsoKind};
use crate::{JukeboxError::*, Result};

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
            iso.seek(std::io::SeekFrom::Start(0x4)).map_err(IsoSeek)?;
            iso.read_exact(&mut block_size).map_err(IsoRead)?;
            let block_size = u32::from_le_bytes(block_size);

            // Get the block map
            let mut block_map = [0; CISO_BLOCK_MAP_SIZE];
            iso.seek(std::io::SeekFrom::Start(0x8)).map_err(IsoSeek)?;
            iso.read_exact(&mut block_map).map_err(IsoRead)?;

            Ok(Some((block_size, block_map)))
        },
        _ => Ok(None),
    }
}

// Given an offset for an standard disc image, return the offset for a ciso
// image
pub(crate) fn get_ciso_offset(header: &CisoHeader, offset: u64) -> Option<u64> {
    let (block_size, block_map) = *header;

    // Get position of the block that `offset` is contained in
    let block_pos = offset as usize / block_size as usize;

    // If the block map has a 0 (no data) at `block_pos` (or if the `block_pos`
    // is out of bounds), return None
    if block_pos >= CISO_BLOCK_MAP_SIZE || block_map[block_pos] == 0 {
        return None;
    }

    // Otherwise return the offset in the ciso by accounting for all of the
    // empty data blocks up until `block_pos`
    let empty_block_count = block_map[0..block_pos].iter().filter(|&b| *b == 0).count();
    let ciso_offset = offset + CISO_HEADER_SIZE as u64 - (block_size as u64 * empty_block_count as u64);
    Some(ciso_offset)
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]

    fn doesnt_try_to_read_headers_from_non_ciso_files() {
        let mut file = File::open("test-data/misow.bin").unwrap();
        let header = get_ciso_header(&mut file).unwrap();
        assert!(header.is_none());
    }

    #[test]
    fn reads_ciso_header_block_size_correctly() {
        let mut file = File::open("test-data/ciso-header-1.bin").unwrap();
        let (block_size, _) = get_ciso_header(&mut file).unwrap().unwrap();
        assert_eq!(block_size, 0x200000);
    }

    #[test]
    fn converts_offsets_to_ciso_offsets_correctly() {
        let mut file = File::open("test-data/ciso-header-1.bin").unwrap();
        let header = get_ciso_header(&mut file).unwrap().unwrap();
        let offset_pairs = [(0x00, 0x8000), (0x424, 0x8424), (0x1EAFBB3D, 0x1DF03B3D)];
        for (original_offset, expected_ciso_offset) in offset_pairs {
            let ciso_offset = get_ciso_offset(&header, original_offset).unwrap();
            assert_eq!(ciso_offset, expected_ciso_offset);
        }

        let mut file = File::open("test-data/ciso-header-2.bin").unwrap();
        let header = get_ciso_header(&mut file).unwrap().unwrap();
        let offset_pairs = [
            (0x400D0C, 0x8D0C),
            (0xDEDEDE, 0x9F5EDE),
            (0xD00D1E, 0x908D1E),
            (0x32992BC0, 0x2219ABC0),
        ];
        for (original_offset, expected_ciso_offset) in offset_pairs {
            let ciso_offset = get_ciso_offset(&header, original_offset).unwrap();
            assert_eq!(ciso_offset, expected_ciso_offset);
        }
    }

    #[test]
    fn converts_offsets_to_none_if_block_is_zeroed() {
        let mut file = File::open("test-data/ciso-header-1.bin").unwrap();
        let header = get_ciso_header(&mut file).unwrap().unwrap();
        let offsets = [0x800000, 0xB00000, 0xD00000];
        for offset in offsets {
            let ciso_offset = get_ciso_offset(&header, offset);
            assert!(ciso_offset.is_none());
        }

        let mut file = File::open("test-data/ciso-header-2.bin").unwrap();
        let header = get_ciso_header(&mut file).unwrap().unwrap();
        let offsets = [0x3A0000, 0x133F8000, 0x31000000];
        for offset in offsets {
            let ciso_offset = get_ciso_offset(&header, offset);
            assert!(ciso_offset.is_none());
        }
    }
}
