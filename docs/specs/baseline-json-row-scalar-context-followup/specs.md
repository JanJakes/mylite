# Baseline JSON Row-Scalar Context Follow-up

## Goal

This slice closes remaining context-routing gaps for JSON functions whose core
scalar and row-scalar implementations already exist. It does not add new JSON
semantics, JSON path grammar, JSON storage optimizations, or a SQLite JSON1
dependency.

The added contexts are:

- non-grouped single-table `SELECT ... ORDER BY json_function(row_value)` keys
  for scalar-producing JSON functions already backed by MySQL-compatible scalar
  sort behavior;
- compatible non-key single-table `UPDATE` assignments for the covered JSON
  function families.

Covered `ORDER BY` functions:

- `JSON_EXTRACT()` and `JSON_UNQUOTE()`;
- `JSON_QUOTE()`;
- `JSON_TYPE()` and `JSON_LENGTH()`.

Covered `UPDATE` assignment functions:

- `JSON_EXTRACT()` and `JSON_UNQUOTE()`;
- `JSON_QUOTE()`;
- `JSON_TYPE()`, `JSON_LENGTH()`, and `JSON_KEYS()`;
- `JSON_ARRAY()` and `JSON_OBJECT()`;
- `JSON_SET()`, `JSON_INSERT()`, `JSON_REPLACE()`, and `JSON_REMOVE()`.

Existing row-scalar planning remains the authority for operands, target
conversion, target-key restrictions, diagnostics, warnings, and result
serialization.

## Compatibility Authority

Normative behavior comes from official MySQL 8.4 documentation and MySQL 8.4.9
runtime observations:

- <https://dev.mysql.com/doc/refman/8.4/en/json-function-reference.html>
- <https://dev.mysql.com/doc/refman/8.4/en/json-creation-functions.html>
- <https://dev.mysql.com/doc/refman/8.4/en/json-search-functions.html>
- <https://dev.mysql.com/doc/refman/8.4/en/json-attribute-functions.html>
- <https://dev.mysql.com/doc/refman/8.4/en/json-modification-functions.html>
- <https://dev.mysql.com/doc/refman/8.4/en/update.html>

Representative MySQL 8.4.9 probes:

```sql
CREATE TABLE t(id INT, j JSON, s VARCHAR(64), n INT, out_json JSON);
INSERT INTO t VALUES
  (1, '{"a":2,"b":"x"}', 'plain', 7, NULL),
  (2, '{"a":1,"b":"y"}', 'quoted', 3, NULL);

SELECT id, JSON_TYPE(j) FROM t ORDER BY JSON_TYPE(j), id;
UPDATE t SET out_json = JSON_SET(j, '$.n', n) WHERE id = 1;
SELECT id, out_json FROM t ORDER BY id;
```

The exact expected rows are captured in
`packages/libmylite/tests/mysql_baseline_json_row_scalar_contexts_expectations.sh`
and verified against MySQL 8.4.9.

## Semantics

For `ORDER BY`, MyLite evaluates the JSON function per source row through the
existing row-scalar SQL builder and registered SQLite scalar functions. Current
single-table sort behavior handles `NULL` ordering, direction, secondary keys,
and `LIMIT` for the covered scalar results.

This slice intentionally does not add `ORDER BY` support for nonscalar JSON
values, including `JSON_KEYS()`, `JSON_ARRAY()`, `JSON_OBJECT()`, and mutation
functions that return objects or arrays. MySQL documents nonscalar JSON sorting
as unsupported and warning-producing; MyLite should not advertise such order
keys until it can match the warning behavior and avoid false ordering
guarantees.

For `UPDATE`, MyLite plans the JSON function as a row-scalar assignment only
when the assignment target is non-keyed, not `AUTO_INCREMENT`, and compatible
with existing target storage conversion. This is expression assignment, not
MySQL's partial in-place JSON storage optimization.

The slice intentionally does not widen predicate admission for construction or
mutation functions. Existing JSON predicate support remains limited to the
functions and predicate forms documented by earlier slices.

## Syntax

No new Lemon grammar is introduced. The parser retry layer may replace a
supported JSON function call in a top-level `ORDER BY` key or single-table
`UPDATE` assignment value with a placeholder so the existing grammar can parse
the statement. The original AST is retained and planned by the runtime.

Intended admission shape:

```lemon
select_order_key(A) ::= row_scalar_json_scalar_function_call(B). {
    A = B;
}

update_value(A) ::= row_scalar_json_function_call(B). {
    A = B;
}
```

The retry scanner admits only direct function calls whose argument list contains
a row operand. Joined and multi-table `UPDATE` statements remain outside this
slice.

## SQLite Integration

This slice uses MyLite-side parsing/planning and existing SQLite scalar UDFs.
It does not require a SQLite fork hook.

## Test Plan

- Add a dedicated C runtime test covering:
  - ordering by each covered scalar JSON function family;
  - single-table assignment for extraction, unquote, quote, introspection,
    construction, and mutation functions;
  - joined-update regression to prove the retry scanner remains single-table.
- Add a MySQL 8.4.9 expectation script for the same successful context probes.
- Run the new test, existing focused JSON and parser retry tests, MySQL
  expectation scripts, diff checks, clang-tidy for touched C files, and the
  full `cmake --workflow --preset check` gate.

## Compatibility Impact

Detailed JSON function rows should no longer list compatible non-key
single-table `UPDATE` assignments as missing for the covered functions.
Scalar-producing covered rows should also no longer list non-grouped
single-table `ORDER BY` expression keys as missing. Rows remain yellow where
independent gaps still exist, such as arbitrary expression operands, wildcard
paths, full JSON number grammar, binary input handling, nonscalar JSON ordering,
grouped ordering, broad predicate support, partial JSON storage updates, or
expression metadata.
