# Comparison functions

Expression helpers driven by comparison, ordering, or NULL-test semantics.

| Function | Status | Notes |
| --- | --- | --- |
| `GREATEST()` | ❌ | Return largest argument |
| `INTERVAL()` | ❌ | Argument interval index |
| `ISNULL()` | 🟡 | Limited no-source and `FROM DUAL` scalar `SELECT ISNULL(value)` plus limited `DO` expression execution over signed-64 decimal integer, `TRUE`/`FALSE`, `NULL`, nested `ISNULL()` operands, and supported nested `IF()`, `IFNULL()`, `COALESCE()`, and `NULLIF()` operands; returns integer `1` for `NULL` and `0` otherwise; no table-backed evaluation, predicates, DML assignments, string/decimal/float/hex/bit operands, arithmetic arguments, subqueries, warning-producing expressions, or expression metadata |
| `LEAST()` | ❌ | Return smallest argument |

[Back to compatibility overview](../../COMPATIBILITY.md)
