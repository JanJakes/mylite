# Control-flow functions

Conditional expression helpers.

| Function | Status | Notes |
| --- | --- | --- |
| `COALESCE()` | ❌ | Return first non-NULL argument |
| `IF()` | 🟡 | Limited no-source and `FROM DUAL` scalar `SELECT IF(condition, true_value, false_value)` over signed-64 decimal integer, `TRUE`/`FALSE`, `NULL`, and nested `IF()` operands; no table-backed evaluation, predicates, DML assignments, string/decimal/float/hex/bit operands, arithmetic arguments, subqueries, or expression metadata |
| `IFNULL()` | ❌ | Null if/else construct |
| `NULLIF()` | ❌ | Return NULL if expr1 = expr2 |

[Back to compatibility overview](../../COMPATIBILITY.md)
