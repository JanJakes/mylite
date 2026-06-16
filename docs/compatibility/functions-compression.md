# Compression functions

String compression and decompression helpers.

| Function | Status | Notes |
| --- | --- | --- |
| `COMPRESS()` | 🟡 | Scalar, row projection, and documented `WHERE` predicates; no row-scalar `IN`, ordering/grouping, or broad DML contexts |
| `UNCOMPRESS()` | 🟡 | Scalar, row projection, documented `WHERE` predicates, and MySQL warnings `1256`/`1259`; no row-scalar `IN`, ordering/grouping, or broad DML contexts |
| `UNCOMPRESSED_LENGTH()` | 🟡 | Scalar, row projection, documented `WHERE` predicates, and MySQL warning `1259`; no row-scalar `IN`, ordering/grouping, or broad DML contexts |

[Back to compatibility overview](../../COMPATIBILITY.md)
