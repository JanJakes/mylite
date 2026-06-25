# UUID functions

UUID generation, validation, and binary/string conversion helpers.

| Function | Status | Notes |
| --- | --- | --- |
| `BIN_TO_UUID()` | 🟡 | Limited scalar and row-scalar conversion for 16-byte values, optional integer/boolean/`NULL` swap flag |
| `IS_UUID()` | 🟡 | Limited scalar and row-scalar validation for supported UUID string forms |
| `UUID()` | 🟡 | Limited scalar, `FROM DUAL`, `DO`, and single-source row-scalar generation; returns MySQL-shaped lowercase version-1 UUID text with `utf8mb3_general_ci` metadata |
| `UUID_SHORT()` | 🟡 | Limited scalar, `FROM DUAL`, `DO`, and single-source row-scalar embedded monotonic unsigned 64-bit identifier; fixed MyLite server-id component and per-handle counter |
| `UUID_TO_BIN()` | 🟡 | Limited scalar and row-scalar conversion for supported UUID string forms, optional integer/boolean/`NULL` swap flag |

[Back to compatibility overview](../../COMPATIBILITY.md)
