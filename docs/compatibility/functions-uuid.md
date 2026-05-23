# UUID functions

UUID generation, validation, and binary/string conversion helpers.

| Function | Status | Notes |
| --- | --- | --- |
| `BIN_TO_UUID()` | 🟡 | Limited scalar and row-scalar conversion for 16-byte values, optional integer/boolean/`NULL` swap flag |
| `IS_UUID()` | 🟡 | Limited scalar and row-scalar validation for supported UUID string forms |
| `UUID()` | ❌ | Return a Universal Unique Identifier (UUID) |
| `UUID_SHORT()` | ❌ | Return an integer-valued universal identifier |
| `UUID_TO_BIN()` | 🟡 | Limited scalar and row-scalar conversion for supported UUID string forms, optional integer/boolean/`NULL` swap flag |

[Back to compatibility overview](../../COMPATIBILITY.md)
