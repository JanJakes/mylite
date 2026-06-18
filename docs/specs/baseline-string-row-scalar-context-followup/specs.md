# Baseline String Row-Scalar Context Follow-up

## Goal

This slice closes remaining context-routing gaps for string functions whose
core scalar and row-scalar implementations already exist. It does not add new
string semantics. It admits the existing row-scalar planner for these functions
in additional ordinary single-table contexts:

- `LOWER()` / `LCASE()` and `UPPER()` / `UCASE()`;
- `LTRIM()`, `RTRIM()`, and `TRIM()`;
- `QUOTE()`;
- `REVERSE()`;
- `SOUNDEX()`;
- `UNHEX()`.

The added contexts are non-grouped single-table `SELECT ... ORDER BY
function(column)` keys and compatible non-key single-table `UPDATE`
assignments. Existing row-scalar planning and target validation remain the
authority for accepted operands, warnings, result conversion, and unsupported
target categories.

## Compatibility Authority

Normative behavior comes from official MySQL 8.4 documentation and MySQL 8.4.9
runtime observations:

- `https://dev.mysql.com/doc/refman/8.4/en/expressions.html`
- `https://dev.mysql.com/doc/refman/8.4/en/string-functions.html`
- `https://dev.mysql.com/doc/refman/8.4/en/update.html`

Representative MySQL 8.4.9 probes:

```sql
CREATE TABLE t(id INT, v VARCHAR(20), outv VARCHAR(40));
INSERT INTO t VALUES (1, '  AbC  ', ''), (2, 'xYz', ''), (3, NULL, '');
SELECT id, TRIM(v) FROM t ORDER BY TRIM(v), id;
UPDATE t SET outv = UPPER(v) WHERE id = 2;
SELECT id, outv FROM t WHERE id = 2;
```

The committed MySQL expectation scripts record the exact result rows for each
function family in this slice.

## Semantics

For `ORDER BY`, the expression is evaluated per source row by the existing
row-scalar SQL builder and SQLite UDF registrations. MySQL-style `NULL`
ordering follows the current MyLite single-table ordering behavior. Direction,
secondary keys, and `LIMIT` continue to use the existing `SELECT ORDER BY`
planning path.

For `UPDATE`, the expression is planned only when it references descriptor row
values and the assignment target is compatible with MyLite's existing
row-scalar update contract. Indexed targets, `AUTO_INCREMENT` targets, and
unsupported storage families remain rejected through current diagnostics.

Bare truth predicates for text-valued functions are intentionally not widened
by this slice. MyLite still admits only documented comparison, `IS`, and range
predicate shapes for these functions, except where an existing function slice
already supports truth conversion.

## Syntax

No new grammar is introduced. The parser retry layer may replace a supported
function call in top-level predicate/order contexts with an identifier
placeholder so the existing Lemon grammar can parse the statement. The original
AST is then retained and planned by the runtime.

Intended admission shape:

```lemon
order_key ::= row_scalar_string_function_call.
where_predicate_subject ::= row_scalar_string_function_call.
update_assignment_value ::= row_scalar_string_function_call.
```

## SQLite Integration

This slice uses MyLite-side parsing/planning and existing SQLite scalar UDFs.
It does not need a SQLite fork change.

## Test Plan

- Extend function-specific C runtime tests with `ORDER BY function(column)` and
  row-backed `UPDATE` assignment cases.
- Extend function-specific MySQL expectation scripts with the same probes.
- Run focused CTest targets for the touched function families.
- Run shell syntax checks, MySQL expectation scripts, diff checks, formatting,
  and the full `cmake --workflow --preset check` release gate.

## Compatibility Impact

Compatibility notes should no longer list `ORDER BY` or compatible non-key
single-table `UPDATE` assignment as missing contexts for the covered functions.
Rows with independent semantic gaps remain yellow and should name those gaps
directly.
