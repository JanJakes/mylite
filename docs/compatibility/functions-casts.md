# Cast functions

Explicit conversion functions.

| Function | Status | Notes |
| --- | --- | --- |
| `CAST()` | 🟡 | Limited no-source, `FROM DUAL`, and `DO` `CAST(value AS BINARY)` for ordinary string, decimal integer, `TRUE`/`FALSE`, and `NULL` scalar values with bare `BINARY` only; no length-bearing binary casts, other targets, table-backed casts, predicates, DML assignments, expression defaults, embedded-NUL delivery, or protocol-grade metadata |
| `CONVERT()` | ❌ | Cast a value as a certain type |

[Back to compatibility overview](../../COMPATIBILITY.md)
