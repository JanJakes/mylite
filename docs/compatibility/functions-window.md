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
| `CUME_DIST()` | ✅ | Projection-only baseline window support, including rank/distribution frame clauses. See [rank/navigation](../specs/baseline-window-rank-navigation-functions/specs.md) and [frame clauses](../specs/baseline-window-rank-frame-clauses/specs.md). |
| `DENSE_RANK()` | ✅ | Projection-only baseline window support, including rank/distribution frame clauses. See [rank/navigation](../specs/baseline-window-rank-navigation-functions/specs.md) and [frame clauses](../specs/baseline-window-rank-frame-clauses/specs.md). |
| `FIRST_VALUE()` | ✅ | Projection-only baseline window support over supported row-scalar value expressions, including supported `ROWS` / `RANGE` frame clauses. See [rank/navigation](../specs/baseline-window-rank-navigation-functions/specs.md), [value arguments](../specs/baseline-window-value-argument-expressions/specs.md), and [value frame clauses](../specs/baseline-window-value-frame-clauses/specs.md). |
| `LAG()` | ✅ | Projection-only baseline support over supported row-scalar value/default expressions and literal offsets, including accepted/ignored baseline frame clauses; user-variable, parameter-marker, and local-variable offsets remain deferred. See [rank/navigation](../specs/baseline-window-rank-navigation-functions/specs.md), [value arguments](../specs/baseline-window-value-argument-expressions/specs.md), and [value frame clauses](../specs/baseline-window-value-frame-clauses/specs.md). |
| `LAST_VALUE()` | ✅ | Projection-only baseline window support over supported row-scalar value expressions, including supported `ROWS` / `RANGE` frame clauses. See [rank/navigation](../specs/baseline-window-rank-navigation-functions/specs.md), [value arguments](../specs/baseline-window-value-argument-expressions/specs.md), and [value frame clauses](../specs/baseline-window-value-frame-clauses/specs.md). |
| `LEAD()` | ✅ | Projection-only baseline support over supported row-scalar value/default expressions and literal offsets, including accepted/ignored baseline frame clauses; user-variable, parameter-marker, and local-variable offsets remain deferred. See [rank/navigation](../specs/baseline-window-rank-navigation-functions/specs.md), [value arguments](../specs/baseline-window-value-argument-expressions/specs.md), and [value frame clauses](../specs/baseline-window-value-frame-clauses/specs.md). |
| `NTH_VALUE()` | ✅ | Projection-only baseline window support over supported row-scalar value expressions, including literal indexes and supported `ROWS` / `RANGE` frame clauses. See [rank/navigation](../specs/baseline-window-rank-navigation-functions/specs.md), [value arguments](../specs/baseline-window-value-argument-expressions/specs.md), and [value frame clauses](../specs/baseline-window-value-frame-clauses/specs.md). |
| `NTILE()` | ✅ | Projection-only baseline window support, including rank/distribution frame clauses; bucket count is literal-only. See [rank/navigation](../specs/baseline-window-rank-navigation-functions/specs.md) and [frame clauses](../specs/baseline-window-rank-frame-clauses/specs.md). |
| `PERCENT_RANK()` | ✅ | Projection-only baseline window support, including rank/distribution frame clauses. See [rank/navigation](../specs/baseline-window-rank-navigation-functions/specs.md) and [frame clauses](../specs/baseline-window-rank-frame-clauses/specs.md). |
| `RANK()` | ✅ | Projection-only baseline window support, including rank/distribution frame clauses. See [rank/navigation](../specs/baseline-window-rank-navigation-functions/specs.md) and [frame clauses](../specs/baseline-window-rank-frame-clauses/specs.md). |
| `ROW_NUMBER()` | ✅ | Projection-only baseline window support for no-source, `DUAL`, and one descriptor-backed table source, including rank-style frame clauses. See [ROW_NUMBER](../specs/baseline-row-number-window-function/specs.md) and [frame clauses](../specs/baseline-window-rank-frame-clauses/specs.md). |

[Back to compatibility overview](../../COMPATIBILITY.md)
