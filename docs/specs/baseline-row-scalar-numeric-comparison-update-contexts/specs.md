# Baseline Row-Scalar Numeric and Comparison UPDATE Contexts

## Goal

This slice closes `UPDATE` assignment context gaps for existing row-scalar
comparison and numeric functions. It does not add new function semantics,
broaden operand domains, add grouped expression support, or change target
column conversion.

Covered functions:

- `GREATEST()` and `LEAST()`;
- `INTERVAL()` and `ISNULL()`;
- `CRC32()`;
- `FORMAT()` and `TRUNCATE()`;
- `MOD()`.

## Compatibility Authority

Normative behavior comes from official MySQL 8.4 documentation and MySQL 8.4.9
runtime observations:

- <https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html>
- <https://dev.mysql.com/doc/refman/8.4/en/mathematical-functions.html>
- <https://dev.mysql.com/doc/refman/8.4/en/encryption-functions.html>
- <https://dev.mysql.com/doc/refman/8.4/en/update.html>

Representative MySQL 8.4.9 probe:

```sql
CREATE TABLE t(id INT, i INT, d DECIMAL(10,2), s VARCHAR(32), out_i INT);
INSERT INTO t VALUES (1, 7, 12.34, 'alpha', NULL);
UPDATE t SET out_i = GREATEST(i, 5);
SELECT out_i FROM t;
```

The exact expected rows are captured in
`packages/libmylite/tests/mysql_baseline_row_scalar_numeric_comparison_update_contexts_expectations.sh`
and verified against MySQL 8.4.9.

## Semantics

MyLite may plan a covered function as a row-scalar `UPDATE` assignment when the
existing single-table assignment rules accept the target: the target must be a
compatible non-key column, must not be `AUTO_INCREMENT`, and must use the
existing target conversion path.

Function results and warnings are delegated to the existing row-scalar function
implementations. The slice intentionally does not widen operand domains:
`GREATEST()` / `LEAST()` stay within current same-domain integer or ASCII string
arguments, `INTERVAL()` stays within sorted integer thresholds, numeric helpers
stay within their documented row-scalar domains, and `MOD()` stays within the
current modulo expression support.

Joined and multi-table `UPDATE` statements remain outside this slice.

## Syntax

No new Lemon grammar is introduced. The parser retry layer may replace a direct
covered function call in a single-table `UPDATE` assignment value with a
row-scalar placeholder when the argument list contains a row operand. The
original AST is retained for runtime planning.

Intended admission shape:

```lemon
update_value(A) ::= row_scalar_numeric_or_comparison_function_call(B). {
    A = B;
}
```

Infix `%` and infix `MOD` assignment forms are not widened by this slice; the
covered modulo assignment surface is `MOD(left, right)`.

## SQLite Integration

This slice uses MyLite-side parsing/planning and existing SQLite scalar UDFs.
It does not require a SQLite fork hook.

## Test Plan

- Add a MySQL 8.4.9 expectation script covering successful all-row assignments,
  per-update changed-row counts, and final `ROW_COUNT()` / warning count state.
- Add a C runtime test with the same assignments and a joined-update regression
  proving the retry scanner remains single-table.
- Run the new test, focused comparison/numeric/parser retry/update tests, MySQL
  expectation script, diff checks, clang-tidy for touched C files, and the full
  `cmake --workflow --preset check` gate.

## Compatibility Impact

Detailed comparison and numeric rows should no longer list compatible non-key
single-table `UPDATE` assignments as missing for the covered functions. Rows
remain yellow where independent gaps still exist, including broader operand
coercion, binary/JSON/temporal inputs, grouped ordering/grouping expressions,
expression metadata, locale forms for `FORMAT()`, exact decimal metadata, or
general expression support.
