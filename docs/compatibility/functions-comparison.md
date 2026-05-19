# Comparison functions

Expression helpers driven by comparison, ordering, or NULL-test semantics.

| Function | Status | Notes |
| --- | --- | --- |
| `GREATEST()` | 🟡 | Limited no-source, `DUAL`, `DO`, and single-table row-scalar `SELECT` projection over flat all-string or all-integer argument lists with `NULL`, ASCII string literals, signed-64 integer/boolean literals, and supported descriptor columns; returns `NULL` if any argument is `NULL`, otherwise the maximum argument using numeric or ASCII `utf8mb4_0900_ai_ci` comparison; no mixed domains, decimals/floats, binary strings, non-ASCII collation parity, predicates, DML assignments, ordering/grouping expressions, or general expression metadata |
| `INTERVAL()` | ❌ | Argument interval index |
| `ISNULL()` | 🟡 | Limited no-source and `FROM DUAL` scalar `SELECT ISNULL(value)` plus limited `DO` expression execution over signed-64 decimal integer, `TRUE`/`FALSE`, `NULL`, nested `ISNULL()` operands, and supported nested `IF()`, `IFNULL()`, `COALESCE()`, and `NULLIF()` operands; limited single-table row-scalar projection over descriptor integer, nonbinary string, `YEAR`, and temporal columns, scalar literals, session scalar values, system variables, and one nested supported row control-flow layer; returns integer `1` for `NULL` and `0` otherwise; no predicates, ordering/grouping expressions, DML assignments, binary string/`BIT`/approximate/JSON column operands, arithmetic row arguments, table-backed subqueries, warning-producing expressions, or expression metadata |
| `LEAST()` | 🟡 | Limited no-source, `DUAL`, `DO`, and single-table row-scalar `SELECT` projection over flat all-string or all-integer argument lists with `NULL`, ASCII string literals, signed-64 integer/boolean literals, and supported descriptor columns; returns `NULL` if any argument is `NULL`, otherwise the minimum argument using numeric or ASCII `utf8mb4_0900_ai_ci` comparison; no mixed domains, decimals/floats, binary strings, non-ASCII collation parity, predicates, DML assignments, ordering/grouping expressions, or general expression metadata |

[Back to compatibility overview](../../COMPATIBILITY.md)
