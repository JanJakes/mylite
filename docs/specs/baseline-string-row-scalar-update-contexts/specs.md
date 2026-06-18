# Baseline String Row-Scalar UPDATE Contexts

## Goal

This slice closes `UPDATE` assignment context gaps for existing string,
search, Base64/hex, regular-expression, and bitmask-style row-scalar functions.
It does not add new function semantics, operand domains, grouping support,
target conversion behavior, or key-target assignment support. It also preserves
the existing duplicate-key row-scalar assignment surface by fixing parameter
ordering when an `INSERT ... ON DUPLICATE KEY UPDATE` assignment uses a
covered row-scalar expression.

Covered functions:

- `CONCAT()` and `CONCAT_WS()`;
- `FIELD()` and `FIND_IN_SET()`;
- `HEX()`, `TO_BASE64()`, and `FROM_BASE64()`;
- `LEFT()`, `RIGHT()`, `SUBSTRING()`, `SUBSTR()`, `MID()`,
  `SUBSTRING_INDEX()`, `LPAD()`, `RPAD()`, `REPEAT()`, `SPACE()`,
  `LOCATE()`, `INSTR()`, `POSITION()`, `REPLACE()`, and `INSERT()`;
- `STRCMP()`;
- `REGEXP_LIKE()`, `REGEXP_INSTR()`, `REGEXP_SUBSTR()`, and
  `REGEXP_REPLACE()`;
- `EXPORT_SET()` and `MAKE_SET()`.

## Compatibility Authority

Normative behavior comes from official MySQL 8.4 documentation and MySQL 8.4.9
runtime observations:

- <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
- <https://dev.mysql.com/doc/refman/8.4/en/regexp.html>
- <https://dev.mysql.com/doc/refman/8.4/en/update.html>

The exact expected rows and per-statement changed-row counts are captured in
`packages/libmylite/tests/mysql_baseline_string_row_scalar_update_contexts_expectations.sh`
and verified against MySQL 8.4.9.

## Semantics

MyLite may plan a covered function as a row-scalar single-table `UPDATE`
assignment when the existing assignment rules accept the target: the target
must be a compatible non-key column, must not be `AUTO_INCREMENT`, and must use
the existing target conversion path.

Function values, `NULL` behavior, warnings, diagnostics, and string/regex
semantics are delegated to the existing row-scalar function implementations.
The slice intentionally stays inside their current documented operand subset:
ASCII/nonbinary string-family operands, supported integer arguments, current
Base64/hex byte handling, and the current ASCII regular-expression subset.

Joined and multi-table `UPDATE` statements remain outside this slice.

## Syntax

The parser retry layer may replace a direct covered function call in a
single-table `UPDATE` assignment value with a row-scalar placeholder when the
argument list contains a row operand. The same placeholder admission applies to
the existing compatible duplicate-key update assignment path. The original AST
is retained for runtime planning.

Intended admission shape:

```lemon
update_value(A) ::= row_scalar_string_function_call(B). {
    A = B;
}
```

For duplicate-key assignments, the retry layer uses an integer placeholder
accepted by the existing `insert_value` grammar and replaces that placeholder
with the retained original AST after parsing. This keeps ordinary identifier
copy assignments outside this slice.

`POSITION(substr IN str)` is included as the MySQL spelling of the supported
two-argument `LOCATE()` subset, while `SUBSTR()` and `MID()` remain synonyms for
the covered `SUBSTRING()` subset.

## SQLite Integration

This slice uses MyLite-side parsing/planning and existing SQLite scalar UDFs.
It does not require a SQLite fork hook.

## Test Plan

- Add a MySQL 8.4.9 expectation script covering successful all-row assignments,
  per-update changed-row counts, final rows, and final diagnostic counts.
- Add a C runtime test with the same assignments and a joined-update regression
  proving the retry scanner remains single-table.
- Add a duplicate-key regression for a covered row-scalar assignment whose
  function arguments consume more than one SQLite parameter.
- Run the new test, focused string/parser retry/update tests, MySQL expectation
  script, diff checks, clang-tidy for touched C files, and the full
  `cmake --workflow --preset check` gate.

## Compatibility Impact

Detailed string rows should no longer list compatible non-key single-table
`UPDATE` assignments as missing for the covered functions. Rows remain yellow
where independent gaps still exist, including binary-string typing, non-ASCII
collation/regex behavior, optional regex arguments, grouped ordering/grouping,
parameters, unsupported operand domains, and expression metadata.
