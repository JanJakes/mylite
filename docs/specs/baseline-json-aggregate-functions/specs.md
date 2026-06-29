# Baseline JSON Aggregate Functions

## Scope

This slice implements the non-window aggregate forms of `JSON_ARRAYAGG()` and
`JSON_OBJECTAGG()` for MyLite's current aggregate-select envelopes. The feature
is specified against the MySQL 8.4 Reference Manual aggregate-function
descriptions and MySQL 8.4.9 runtime probes.

Reference: https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html

## Syntax

MyLite already admits these function names through the generic-function parser
surface. The execution planner recognizes the following aggregate forms:

```text
json_arrayagg_function ::= JSON_ARRAYAGG '(' expression ')'
json_objectagg_function ::= JSON_OBJECTAGG '(' expression ',' expression ')'
```

`OVER` clauses are covered by
`docs/specs/baseline-json-aggregate-window-functions/specs.md`.

## Semantics

`JSON_ARRAYAGG(expr)` evaluates one argument per input row and returns a JSON
array. SQL `NULL` contributes a JSON `null` element. If no rows match, the
aggregate returns SQL `NULL`.

`JSON_OBJECTAGG(key, value)` evaluates one key and one value per input row and
returns a JSON object. Key values are converted with the same baseline key rules
as `JSON_OBJECT()`: integer and Boolean keys become decimal text keys, string
keys remain strings, and SQL `NULL` keys raise MySQL error `3158` with SQLSTATE
`22032`. SQL `NULL` values contribute JSON `null`. Duplicate keys use MySQL's
normalized JSON object behavior where the last value for the key wins. If no
rows match, the aggregate returns SQL `NULL`.

The returned value is exposed as JSON text in MyLite's current result-cell
model. JSON descriptor values supplied as aggregate values are inserted as JSON
documents after validation and normalization through the existing JSON
constructor implementation.

## Supported Envelopes

The executable subset covers:

- no-source and `DUAL` aggregate select forms without `WHERE`;
- one descriptor-backed table with the existing aggregate `WHERE` predicate
  support;
- grouped aggregate selects over the current descriptor-backed grouping subset;
- mixed aggregate select lists supported by the existing aggregate planner.

## Compatibility Limits

The following remain outside this baseline slice:

- aggregate-window execution beyond
  `docs/specs/baseline-json-aggregate-window-functions/specs.md`;
- aggregate-local `ORDER BY` or deterministic ordering beyond the source order
  delivered by SQLite for the supported query;
- `DISTINCT`;
- arbitrary argument expressions beyond the current `JSON_ARRAY()` /
  `JSON_OBJECT()` row-scalar constructor argument subset;
- full MySQL binary JSON storage, metadata, and collation semantics;
- exact nondeterministic duplicate-key ordering for unordered sources.

Unsupported forms must fail with existing MyLite unsupported diagnostics rather
than silently producing incompatible results.

## MySQL 8.4.9 Observations

Runtime probes established these baseline expectations:

- `SELECT JSON_ARRAYAGG(NULL), JSON_OBJECTAGG('a', 1);` returns
  `[null]` and `{"a": 1}`.
- `JSON_ARRAYAGG()` includes SQL `NULL` values as JSON `null`.
- `JSON_OBJECTAGG()` returns SQL `NULL` for empty input.
- duplicate object keys keep the last encountered value.
- SQL `NULL` object keys raise `ERROR 3158 (22032): JSON documents may not
  contain NULL member names.`

The companion expectation script records the exact tabular results used by the
C runtime test.
