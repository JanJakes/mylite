# SQL file output

Server-side SELECT file output statement compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `SELECT ... INTO OUTFILE` | ⚪ | Parsed as an unsupported utility statement returning `1064 / 42000`; `@@character_set_filesystem` readback/assignment metadata does not enable file-name conversion or output |
| `SELECT ... INTO DUMPFILE` | ⚪ | Parsed as an unsupported utility statement returning `1064 / 42000`; `@@character_set_filesystem` readback/assignment metadata does not enable binary file output |

[Back to compatibility overview](../../COMPATIBILITY.md)
