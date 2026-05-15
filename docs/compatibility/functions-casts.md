# Cast functions

Explicit conversion functions.

| Function | Status | Notes |
| --- | --- | --- |
| `CAST()` | 🟡 | Limited no-source, `FROM DUAL`, and `DO` `CAST(value AS BINARY)` for ordinary string, decimal integer, `TRUE`/`FALSE`, and `NULL` scalar values with bare `BINARY` only; binary column storage is tracked separately under binary string types; no length-bearing binary casts, other targets, table-backed casts, predicates, DML assignments, expression defaults, embedded-NUL scalar delivery, or protocol-grade metadata |
| `CONVERT()` | 🟡 | Limited no-source, `FROM DUAL`, and `DO` `CONVERT('string' USING BINARY)` for ordinary NUL-free string literals only; no `CONVERT(expr, type)`, non-`BINARY` character sets, `NULL`/numeric/boolean/general expression operands, table-backed conversion, predicates, DML assignments, expression defaults, charset/collation metadata, embedded-NUL scalar delivery, or protocol-grade metadata |

[Back to compatibility overview](../../COMPATIBILITY.md)
