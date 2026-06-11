# Window functions

Window-only ranking, distribution, navigation, and frame-value functions.

The parser also admits MySQL-shaped named windows, `OVER window_name`,
expression and multi-key partition/order clauses, and `ROWS` / `RANGE` frame
clauses, including parser-only interval bounds in `RANGE` frames. Those broader
forms are parse-level compatibility only unless they fit the executable
baseline described per function below. Top-level `ORDER BY` window-function
keys parse, but execution is rejected outside the current projection envelope.
Common aggregate-window forms such as `COUNT(*) OVER (...)`,
`SUM(column) OVER (...)`, and selected literal/arithmetic aggregate-window
placeholders are also parse-level compatibility only and are rejected on
execution. JSON and statistical aggregate-window placeholders such as
`JSON_ARRAYAGG(...) OVER ...` and `STDDEV_SAMP(...) OVER ...` follow the same
parser-only policy; see [parser aggregate window syntax](../specs/parser-aggregate-window-syntax/specs.md),
[parser corpus aggregate/window surfaces](../specs/parser-corpus-aggregate-window-surfaces/specs.md),
[parser corpus JSON/statistical aggregate window surfaces](../specs/parser-corpus-json-stat-aggregate-window-surfaces/specs.md),
and [parser corpus order expression residuals](../specs/parser-corpus-order-expression-residuals/specs.md).
Value and navigation functions admit MySQL-shaped `RESPECT NULLS` and
`IGNORE NULLS` clauses. `RESPECT NULLS` follows the default behavior inside the
current executable envelopes, while `IGNORE NULLS` returns MySQL's unsupported
diagnostic because MySQL 8.4.9 parses but does not implement that behavior.

| Function | Status | Notes |
| --- | --- | --- |
| `CUME_DIST()` | 🟡 | Limited projection-only `CUME_DIST() OVER (...)` in the baseline row-scalar window envelope; no named windows, explicit frames, expression keys, multiple keys, joins, grouped selects, predicates, DML contexts, or aggregate windows. See [baseline window rank and navigation functions](../specs/baseline-window-rank-navigation-functions/specs.md). |
| `DENSE_RANK()` | 🟡 | Limited projection-only `DENSE_RANK() OVER (...)` in the baseline row-scalar window envelope; no named windows, explicit frames, expression keys, multiple keys, joins, grouped selects, predicates, DML contexts, or aggregate windows. See [baseline window rank and navigation functions](../specs/baseline-window-rank-navigation-functions/specs.md). |
| `FIRST_VALUE()` | 🟡 | Limited projection-only `FIRST_VALUE(value) [RESPECT NULLS] OVER (...)` over scalar literals or descriptor columns in the baseline row-scalar window envelope; `IGNORE NULLS` parses and returns MySQL's unsupported diagnostic. No named windows, explicit frames, expression keys, multiple keys, joins, grouped selects, predicates, DML contexts, true ignore-null execution, or unsupported value types. See [baseline window rank and navigation functions](../specs/baseline-window-rank-navigation-functions/specs.md) and [parser corpus query-expression surfaces](../specs/parser-corpus-query-expression-surfaces/specs.md). |
| `LAG()` | 🟡 | Limited projection-only `LAG(value[, literal_offset[, literal_default]]) [RESPECT NULLS] OVER (...)` over scalar literals or descriptor columns in the baseline row-scalar window envelope; offsets/defaults are literal-only, and `IGNORE NULLS` parses and returns MySQL's unsupported diagnostic. Broader contexts and true ignore-null execution are deferred. See [baseline window rank and navigation functions](../specs/baseline-window-rank-navigation-functions/specs.md) and [parser corpus query-expression surfaces](../specs/parser-corpus-query-expression-surfaces/specs.md). |
| `LAST_VALUE()` | 🟡 | Limited projection-only `LAST_VALUE(value) [RESPECT NULLS] OVER (...)` over scalar literals or descriptor columns in the baseline row-scalar window envelope; `IGNORE NULLS` parses and returns MySQL's unsupported diagnostic. No named windows, explicit frames, expression keys, multiple keys, joins, grouped selects, predicates, DML contexts, true ignore-null execution, or unsupported value types. See [baseline window rank and navigation functions](../specs/baseline-window-rank-navigation-functions/specs.md) and [parser corpus query-expression surfaces](../specs/parser-corpus-query-expression-surfaces/specs.md). |
| `LEAD()` | 🟡 | Limited projection-only `LEAD(value[, literal_offset[, literal_default]]) [RESPECT NULLS] OVER (...)` over scalar literals or descriptor columns in the baseline row-scalar window envelope; offsets/defaults are literal-only, and `IGNORE NULLS` parses and returns MySQL's unsupported diagnostic. Broader contexts and true ignore-null execution are deferred. See [baseline window rank and navigation functions](../specs/baseline-window-rank-navigation-functions/specs.md) and [parser corpus query-expression surfaces](../specs/parser-corpus-query-expression-surfaces/specs.md). |
| `NTH_VALUE()` | 🟡 | Limited projection-only `NTH_VALUE(value, literal_index) [RESPECT NULLS] OVER (...)` over scalar literals or descriptor columns in the baseline row-scalar window envelope; index is literal-only, `IGNORE NULLS` parses and returns MySQL's unsupported diagnostic, and `FROM FIRST` / `FROM LAST` remains rejected as MySQL 8.4.9 syntax. Broader contexts and true ignore-null execution are deferred. See [baseline window rank and navigation functions](../specs/baseline-window-rank-navigation-functions/specs.md) and [parser corpus query-expression surfaces](../specs/parser-corpus-query-expression-surfaces/specs.md). |
| `NTILE()` | 🟡 | Limited projection-only `NTILE(literal_bucket_count) OVER (...)` in the baseline row-scalar window envelope; bucket count is literal-only and broader contexts are deferred. See [baseline window rank and navigation functions](../specs/baseline-window-rank-navigation-functions/specs.md). |
| `PERCENT_RANK()` | 🟡 | Limited projection-only `PERCENT_RANK() OVER (...)` in the baseline row-scalar window envelope; no named windows, explicit frames, expression keys, multiple keys, joins, grouped selects, predicates, DML contexts, or aggregate windows. See [baseline window rank and navigation functions](../specs/baseline-window-rank-navigation-functions/specs.md). |
| `RANK()` | 🟡 | Limited projection-only `RANK() OVER (...)` in the baseline row-scalar window envelope; no named windows, explicit frames, expression keys, multiple keys, joins, grouped selects, predicates, DML contexts, or aggregate windows. See [baseline window rank and navigation functions](../specs/baseline-window-rank-navigation-functions/specs.md). |
| `ROW_NUMBER()` | 🟡 | Limited projection-only `ROW_NUMBER() OVER (...)` for no-source, `DUAL`, and one descriptor-backed table source, with optional one descriptor-column `PARTITION BY`, optional one descriptor-column `ORDER BY` plus `ASC` / `DESC`, and existing row-scalar `WHERE` / outer `ORDER BY` / `LIMIT`; no named windows, frame clauses, expressions, multiple keys, joins, grouped selects, predicates, or DML contexts. See [baseline ROW_NUMBER window function](../specs/baseline-row-number-window-function/specs.md). |

[Back to compatibility overview](../../COMPATIBILITY.md)
