# Control-flow functions

Conditional expression helpers.

| Function | Status | Notes |
| --- | --- | --- |
| `COALESCE()` | 🟡 | Limited no-source and `FROM DUAL` scalar `SELECT COALESCE(value[, ...])` over signed-64 decimal integer, `TRUE`/`FALSE`, `NULL`, nested `COALESCE()` operands, supported nested `IFNULL()` operands, supported nested `IF()` operands, supported nested `NULLIF()` operands, and supported nested `ISNULL()` operands; no table-backed evaluation, predicates, DML assignments, string/decimal/float/hex/bit operands, arithmetic arguments, subqueries, side effects, or expression metadata |
| `IF()` | 🟡 | Limited no-source and `FROM DUAL` scalar `SELECT IF(condition, true_value, false_value)` over signed-64 decimal integer, `TRUE`/`FALSE`, `NULL`, nested `IF()` operands, supported nested `IFNULL()` operands, supported nested `COALESCE()` operands, supported nested `NULLIF()` operands, and supported nested `ISNULL()` operands; no table-backed evaluation, predicates, DML assignments, string/decimal/float/hex/bit operands, arithmetic arguments, subqueries, side effects, or expression metadata |
| `IFNULL()` | 🟡 | Limited no-source and `FROM DUAL` scalar `SELECT IFNULL(value, fallback)` over signed-64 decimal integer, `TRUE`/`FALSE`, `NULL`, nested `IFNULL()` operands, supported nested `IF()` operands, supported nested `COALESCE()` operands, supported nested `NULLIF()` operands, and supported nested `ISNULL()` operands; no table-backed evaluation, predicates, DML assignments, string/decimal/float/hex/bit operands, arithmetic arguments, subqueries, side effects, or expression metadata |
| `NULLIF()` | 🟡 | Limited no-source and `FROM DUAL` scalar `SELECT NULLIF(left_value, right_value)` over signed-64 decimal integer, `TRUE`/`FALSE`, `NULL`, nested `NULLIF()` operands, supported nested `IF()` operands, supported nested `IFNULL()` operands, supported nested `COALESCE()` operands, and supported nested `ISNULL()` operands; no table-backed evaluation, predicates, DML assignments, string/decimal/float/hex/bit operands, arithmetic arguments, subqueries, side effects, first-argument double-evaluation semantics, or expression metadata |

[Back to compatibility overview](../../COMPATIBILITY.md)
