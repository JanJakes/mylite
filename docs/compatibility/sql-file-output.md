# SQL file output

Server-side SELECT file output statement compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `SELECT ... INTO OUTFILE` | ❌ | File export syntax diagnostics; scalar `@@character_set_filesystem` is a fixed placeholder only and does not enable file-name conversion or output |
| `SELECT ... INTO DUMPFILE` | ❌ | Binary file export syntax diagnostics; scalar `@@character_set_filesystem` is a fixed placeholder only and does not enable file-name conversion or output |

[Back to compatibility overview](../../COMPATIBILITY.md)
