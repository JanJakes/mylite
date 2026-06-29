# Window functions

Window-only ranking, distribution, navigation, and frame-value functions.

The parser also admits MySQL-shaped named windows, `OVER window_name`,
expression and multi-key partition/order clauses, and `ROWS` / `RANGE` frame
clauses, including parser-only interval bounds in `RANGE` frames. Named-window
definitions are executable when their resolved clauses fit the current
row-scalar projection window baseline. Broader forms are parse-level
compatibility only unless they fit the executable baseline described per
function below. Top-level `ORDER BY` window-function keys parse, but execution
is rejected outside the current projection envelope.
Core aggregate-window forms `COUNT(*)`, `COUNT(value)`, and integer-domain
`SUM()` / `AVG()` / `MIN()` / `MAX()` execute in the documented projection
baseline. Integer-domain `BIT_AND()` / `BIT_OR()` / `BIT_XOR()` windows execute
in the documented bitwise aggregate-window baseline. `DISTINCT` aggregate
windows return MySQL's `1235` unsupported diagnostic. JSON, statistical,
`GROUP_CONCAT()`, and broader aggregate-window placeholders remain parse-level
compatibility only; see
[parser aggregate window syntax](../specs/parser-aggregate-window-syntax/specs.md),
[baseline core aggregate window functions](../specs/baseline-core-aggregate-window-functions/specs.md),
[baseline bitwise aggregate window functions](../specs/baseline-bitwise-aggregate-window-functions/specs.md),
[parser corpus aggregate/window surfaces](../specs/parser-corpus-aggregate-window-surfaces/specs.md),
[parser corpus JSON/statistical aggregate window surfaces](../specs/parser-corpus-json-stat-aggregate-window-surfaces/specs.md),
and [parser corpus order expression residuals](../specs/parser-corpus-order-expression-residuals/specs.md).
Value and navigation functions admit MySQL-shaped `RESPECT NULLS` and
`IGNORE NULLS` clauses. `RESPECT NULLS` follows the default behavior inside the
current executable envelopes, while `IGNORE NULLS` returns MySQL's unsupported
diagnostic because MySQL 8.4.9 parses but does not implement that behavior.

| Function | Status | Notes |
| --- | --- | --- |
| Core aggregate windows | ✅ | Projection-only `COUNT(*)`, `COUNT(value)`, and integer-domain `SUM()` / `AVG()` / `MIN()` / `MAX()` windows over the current no-source, `DUAL`, one-table, named-window, partition/order/frame row-scalar window envelope. See [core aggregate windows](../specs/baseline-core-aggregate-window-functions/specs.md). |
| Bitwise aggregate windows | ✅ | Projection-only integer-domain `BIT_AND()` / `BIT_OR()` / `BIT_XOR()` windows over the current no-source, `DUAL`, one-table, named-window, partition/order/frame row-scalar window envelope. See [bitwise aggregate windows](../specs/baseline-bitwise-aggregate-window-functions/specs.md). |
| `CUME_DIST()` | ✅ | Projection-only baseline window support, including named windows and rank/distribution frame clauses. See [rank/navigation](../specs/baseline-window-rank-navigation-functions/specs.md), [named windows](../specs/baseline-named-window-definitions/specs.md), and [frame clauses](../specs/baseline-window-rank-frame-clauses/specs.md). |
| `DENSE_RANK()` | ✅ | Projection-only baseline window support, including named windows and rank/distribution frame clauses. See [rank/navigation](../specs/baseline-window-rank-navigation-functions/specs.md), [named windows](../specs/baseline-named-window-definitions/specs.md), and [frame clauses](../specs/baseline-window-rank-frame-clauses/specs.md). |
| `FIRST_VALUE()` | ✅ | Projection-only baseline window support over supported row-scalar value expressions, including named windows and supported `ROWS` / `RANGE` frame clauses. See [rank/navigation](../specs/baseline-window-rank-navigation-functions/specs.md), [named windows](../specs/baseline-named-window-definitions/specs.md), [value arguments](../specs/baseline-window-value-argument-expressions/specs.md), and [value frame clauses](../specs/baseline-window-value-frame-clauses/specs.md). |
| `LAG()` | ✅ | Projection-only baseline support over supported row-scalar value/default expressions and literal offsets, including named windows and accepted/ignored baseline frame clauses; user-variable, parameter-marker, and local-variable offsets remain deferred. See [rank/navigation](../specs/baseline-window-rank-navigation-functions/specs.md), [named windows](../specs/baseline-named-window-definitions/specs.md), [value arguments](../specs/baseline-window-value-argument-expressions/specs.md), and [value frame clauses](../specs/baseline-window-value-frame-clauses/specs.md). |
| `LAST_VALUE()` | ✅ | Projection-only baseline window support over supported row-scalar value expressions, including named windows and supported `ROWS` / `RANGE` frame clauses. See [rank/navigation](../specs/baseline-window-rank-navigation-functions/specs.md), [named windows](../specs/baseline-named-window-definitions/specs.md), [value arguments](../specs/baseline-window-value-argument-expressions/specs.md), and [value frame clauses](../specs/baseline-window-value-frame-clauses/specs.md). |
| `LEAD()` | ✅ | Projection-only baseline support over supported row-scalar value/default expressions and literal offsets, including named windows and accepted/ignored baseline frame clauses; user-variable, parameter-marker, and local-variable offsets remain deferred. See [rank/navigation](../specs/baseline-window-rank-navigation-functions/specs.md), [named windows](../specs/baseline-named-window-definitions/specs.md), [value arguments](../specs/baseline-window-value-argument-expressions/specs.md), and [value frame clauses](../specs/baseline-window-value-frame-clauses/specs.md). |
| `NTH_VALUE()` | ✅ | Projection-only baseline window support over supported row-scalar value expressions, including named windows, literal indexes, and supported `ROWS` / `RANGE` frame clauses. See [rank/navigation](../specs/baseline-window-rank-navigation-functions/specs.md), [named windows](../specs/baseline-named-window-definitions/specs.md), [value arguments](../specs/baseline-window-value-argument-expressions/specs.md), and [value frame clauses](../specs/baseline-window-value-frame-clauses/specs.md). |
| `NTILE()` | ✅ | Projection-only baseline window support, including named windows and rank/distribution frame clauses; bucket count is literal-only. See [rank/navigation](../specs/baseline-window-rank-navigation-functions/specs.md), [named windows](../specs/baseline-named-window-definitions/specs.md), and [frame clauses](../specs/baseline-window-rank-frame-clauses/specs.md). |
| `PERCENT_RANK()` | ✅ | Projection-only baseline window support, including named windows and rank/distribution frame clauses. See [rank/navigation](../specs/baseline-window-rank-navigation-functions/specs.md), [named windows](../specs/baseline-named-window-definitions/specs.md), and [frame clauses](../specs/baseline-window-rank-frame-clauses/specs.md). |
| `RANK()` | ✅ | Projection-only baseline window support, including named windows and rank/distribution frame clauses. See [rank/navigation](../specs/baseline-window-rank-navigation-functions/specs.md), [named windows](../specs/baseline-named-window-definitions/specs.md), and [frame clauses](../specs/baseline-window-rank-frame-clauses/specs.md). |
| `ROW_NUMBER()` | ✅ | Projection-only baseline window support for no-source, `DUAL`, and one descriptor-backed table source, including named windows and rank-style frame clauses. See [ROW_NUMBER](../specs/baseline-row-number-window-function/specs.md), [named windows](../specs/baseline-named-window-definitions/specs.md), and [frame clauses](../specs/baseline-window-rank-frame-clauses/specs.md). |

[Back to compatibility overview](../../COMPATIBILITY.md)
