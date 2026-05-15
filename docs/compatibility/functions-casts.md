# Cast functions

Explicit conversion functions.

| Function | Status | Notes |
| --- | --- | --- |
| `CAST()` | 🟡 | Limited no-source, `FROM DUAL`, and `DO` `CAST(value AS BINARY)` for ordinary string, decimal integer, `TRUE`/`FALSE`, and `NULL` scalar values with bare `BINARY` only; binary column storage is tracked separately under binary string types; no length-bearing binary casts, other targets, table-backed casts, predicates, DML assignments, expression defaults, embedded-NUL scalar delivery, or protocol-grade metadata |
| `CONVERT()` | 🟡 | Limited no-source, `FROM DUAL`, and `DO` `CONVERT(value, BINARY)` for ordinary string, decimal integer in the current 81-significant-digit scalar envelope, `TRUE`/`FALSE`, and `NULL` scalar values, existing `CONVERT('string' USING BINARY)` for ordinary NUL-free string literals, and `CONVERT(value USING utf8mb4)` for ordinary string, decimal integer in the same scalar envelope, `TRUE`/`FALSE`, and `NULL` scalar values; no length-bearing `BINARY(N)`, `CHAR`/signed/temporal/other targets, non-`utf8mb4` character-set conversion except the existing bare `USING BINARY` string-literal subset, table-backed conversion, predicates, DML assignments, expression defaults, charset/collation metadata, embedded-NUL scalar delivery, or protocol-grade metadata |

[Back to compatibility overview](../../COMPATIBILITY.md)
