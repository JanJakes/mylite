# Compression functions

String compression and decompression helpers.

| Function | Status | Notes |
| --- | --- | --- |
| `COMPRESS()` | 🟡 | Baseline MySQL compressed payload format with zlib for supported scalar and row-projection arguments |
| `UNCOMPRESS()` | 🟡 | Baseline decompression of `COMPRESS()` payloads, `NULL` handling, invalid zlib warning `1259`, and oversized stored-length warning `1256` |
| `UNCOMPRESSED_LENGTH()` | 🟡 | Baseline original-length reporting, including low-30-bit stored length prefixes and invalid short-input warning `1259` |

[Back to compatibility overview](../../COMPATIBILITY.md)
