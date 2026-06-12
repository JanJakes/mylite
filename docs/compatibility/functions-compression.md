# Compression functions

String compression and decompression helpers.

| Function | Status | Notes |
| --- | --- | --- |
| `COMPRESS()` | 🟡 | Baseline MySQL compressed payload format with zlib for supported scalar and row-projection arguments |
| `UNCOMPRESS()` | 🟡 | Baseline decompression of `COMPRESS()` payloads, `NULL` handling, and invalid-input warning `1259` |
| `UNCOMPRESSED_LENGTH()` | 🟡 | Baseline original-length reporting with invalid-input warning `1259` |

[Back to compatibility overview](../../COMPATIBILITY.md)
