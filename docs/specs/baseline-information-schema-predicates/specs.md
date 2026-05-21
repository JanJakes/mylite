# Baseline INFORMATION_SCHEMA Predicates

## Summary

This phase widens the synthetic `INFORMATION_SCHEMA` query path with common
metadata predicates used by application bootstrap and schema-diff code:

- `metadata_column LIKE string_pattern`
- `metadata_column [NOT] IN (literal[, ...])`
- `metadata_column [NOT] BETWEEN lower_literal AND upper_literal`
- MySQL-shaped three-valued predicate evaluation for supported metadata
  predicates, including `NULL` operands under `NOT`, `AND`, `OR`, and `XOR`.

The feature stays inside the current descriptor-owned metadata path. MyLite
continues to build `INFORMATION_SCHEMA` rows from MyLite catalog descriptors and
fixed system-view definitions, then filters those small synthetic row sets in
MyLite. It does not create physical SQLite `information_schema` tables or pass
metadata queries through to SQLite.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing metadata and predicate slices:
  - `docs/specs/baseline-information-schema-core/specs.md`
  - `docs/specs/baseline-like-predicates/specs.md`
  - `docs/specs/baseline-where-in-predicates/specs.md`
  - `docs/specs/baseline-where-between-predicates/specs.md`
  - `docs/specs/baseline-where-and-predicates/specs.md`
  - `docs/specs/baseline-where-or-predicates/specs.md`
  - `docs/specs/baseline-where-xor-predicates/specs.md`
  - `docs/specs/baseline-where-not-predicates/specs.md`
- Compatibility docs:
  - `COMPATIBILITY.md`
  - `docs/compatibility/metadata-information-schema.md`
  - `docs/compatibility/sql-query-expressions.md`
  - `docs/compatibility/operators.md`
- Official MySQL 8.4 Reference Manual:
  - `INFORMATION_SCHEMA` introduction:
    <https://dev.mysql.com/doc/refman/8.4/en/information-schema.html>
  - `INFORMATION_SCHEMA` table reference:
    <https://dev.mysql.com/doc/refman/8.4/en/information-schema-table-reference.html>
  - comparison functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html>
  - string comparison functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/string-comparison-functions.html>
  - expressions:
    <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
  - SQL modes:
    <https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html>
- MySQL runtime probes are captured in:
  `packages/libmylite/tests/mysql_baseline_information_schema_predicates_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## Runtime Observations

MySQL 8.4.9 behavior used for this slice:

- `INFORMATION_SCHEMA` metadata predicates use normal SQL `WHERE` truth rules:
  only true rows pass; false and unknown rows are filtered out.
- `NOT` of an unknown predicate remains unknown. For example,
  `NOT ENGINE = 'InnoDB'` does not match `INFORMATION_SCHEMA.TABLES` rows whose
  `ENGINE` value is `NULL`.
- `<=> NULL` matches metadata rows whose left value is `NULL`; `= NULL` and
  `NOT = NULL` do not match rows.
- `COLUMN_NAME LIKE 'ID%'` matches a column named `id` under the verified
  metadata collation.
- In default SQL mode, `LIKE 'wp\\_%'` treats the backslash as a pattern escape
  and matches `wp_options` but not `wpa`.
- Under `NO_BACKSLASH_ESCAPES`, the same `LIKE 'wp\\_%'` pattern does not match
  `wp_options`.
- `TABLE_NAME NOT LIKE 'wp%'` filters out matching rows and returns rows such
  as `t`.
- Alias-qualified predicate columns such as `c.TABLE_SCHEMA` and
  `c.COLUMN_NAME` resolve when the metadata table has alias `c`.
- `COLUMN_NAME IN ('ID', 'v', 'missing')` matches `id` and `v` under the
  verified case-insensitive metadata collation.
- `COLUMN_NAME NOT IN ('id', 'v')` matches other non-`NULL` column names.
- `COLUMN_NAME NOT IN ('missing', NULL)` matches no rows.
- `TABLE_NAME BETWEEN 't' AND 'wp_options'` uses metadata text collation
  ordering and includes the bounds.
- `ORDINAL_POSITION BETWEEN 1 AND 2` is inclusive.
- `ORDINAL_POSITION IN ('01', 3)` matches positions `1` and `3`.
- Supported metadata predicate statements produce no warnings.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` and result ownership conventions do
  not change.
- Statement context: owns diagnostics, warnings, and statement read-row
  reporting. This phase adds no public statement state.
- Lexer/parser/AST: the existing predicate AST already represents `LIKE`,
  `IN`, `BETWEEN`, `NOT`, `AND`, `OR`, `XOR`, and parentheses. No new AST node
  is required.
- Analyzer/planner: resolves every predicate column against the selected
  `INFORMATION_SCHEMA` table definition, not SQLite metadata. Literal values
  are decoded through MyLite-owned string/integer/`NULL` conversion before row
  evaluation.
- Catalog module: remains the authority for metadata rows. Predicate evaluation
  is read-only and must not mutate catalog rows, descriptor caches, descriptor
  versions, catalog generation, or `sqlite_schema_generation`.
- Result builder: appends only rows accepted by the metadata predicate. Result
  column metadata follows existing `INFORMATION_SCHEMA` conventions.
- SQLite physical storage: not involved in metadata predicate filtering. This
  phase uses no SQLite fork patches and no generated SQLite predicate SQL.
- Storage/VFS: unchanged. `.mylite` preamble and shifted SQLite payload
  invariants are unaffected.

## Supported SQL

This phase applies to the existing single-source synthetic
`INFORMATION_SCHEMA` query envelope:

```sql
SELECT select_items
FROM INFORMATION_SCHEMA.metadata_table [AS] [alias]
[WHERE metadata_predicate]
[ORDER BY metadata_column [ASC|DESC]]
[LIMIT row_count]
```

New supported predicate atoms:

```sql
metadata_column LIKE string_literal
metadata_column NOT LIKE string_literal
metadata_column IN (metadata_literal[, metadata_literal]...)
metadata_column NOT IN (metadata_literal[, metadata_literal]...)
metadata_column BETWEEN metadata_literal AND metadata_literal
metadata_column NOT BETWEEN metadata_literal AND metadata_literal
```

Comparison right-hand values are limited to:

```sql
integer_literal
+ integer_literal
- integer_literal
TRUE
FALSE
string_literal
NULL
DATABASE()
SCHEMA()
```

`IN` list items and `BETWEEN` bounds are limited to literal values only:

```sql
integer_literal
+ integer_literal
- integer_literal
TRUE
FALSE
string_literal
NULL
```

`DATABASE()` and `SCHEMA()` remain comparison values only. `IN` and `BETWEEN`
do not admit functions, parameters, subqueries, or expressions.

The predicate may be composed with the existing supported `NOT`, `AND` / `&&`,
`OR` / `||`, `XOR`, and parenthesized predicate support.

### Lemon-Syntax Snippet

This describes MyLite's supported grammar surface, not MySQL's full grammar:

```lemon
metadata_predicate ::= metadata_predicate OR metadata_predicate.
metadata_predicate ::= metadata_predicate AND metadata_predicate.
metadata_predicate ::= metadata_predicate XOR metadata_predicate.
metadata_predicate ::= NOT metadata_predicate.
metadata_predicate ::= LPAREN metadata_predicate RPAREN.
metadata_predicate ::= metadata_predicate_atom.

metadata_predicate_atom ::= qualified_identifier comparison_operator metadata_comparison_value.
metadata_predicate_atom ::= qualified_identifier LIKE STRING.
metadata_predicate_atom ::= qualified_identifier NOT LIKE STRING.
metadata_predicate_atom ::= qualified_identifier IS NULL.
metadata_predicate_atom ::= qualified_identifier IS NOT NULL.
metadata_predicate_atom ::= qualified_identifier IN LPAREN metadata_literal_list RPAREN.
metadata_predicate_atom ::= qualified_identifier NOT IN LPAREN metadata_literal_list RPAREN.
metadata_predicate_atom ::= qualified_identifier BETWEEN metadata_literal AND metadata_literal.
metadata_predicate_atom ::= qualified_identifier NOT BETWEEN metadata_literal AND metadata_literal.

metadata_literal_list ::= metadata_literal.
metadata_literal_list ::= metadata_literal_list COMMA metadata_literal.

metadata_comparison_value ::= metadata_literal.
metadata_comparison_value ::= DATABASE LPAREN RPAREN.
metadata_comparison_value ::= SCHEMA LPAREN RPAREN.

metadata_literal ::= INTEGER.
metadata_literal ::= PLUS INTEGER.
metadata_literal ::= MINUS INTEGER.
metadata_literal ::= TRUE.
metadata_literal ::= FALSE.
metadata_literal ::= STRING.
metadata_literal ::= NULL.
```

The current parser already admits the relevant AST shapes. Unsupported values
inside a metadata context fail during information-schema planning or evaluation.

## Semantics

### Truth Values

The metadata predicate evaluator uses three values internally: true, false, and
unknown. Final `WHERE` acceptance is true only. False and unknown rows are not
returned.

Supported truth tables follow the verified MySQL 8.4.9 shape:

- `NOT true` is false, `NOT false` is true, and `NOT unknown` is unknown.
- `AND` returns false if either side is false, true if both sides are true, and
  unknown otherwise.
- `OR` returns true if either side is true, false if both sides are false, and
  unknown otherwise.
- `XOR` returns unknown if either side is unknown; otherwise it returns whether
  exactly one side is true.

### Comparisons

Existing supported comparisons keep their current behavior, with one
correction: non-null-safe comparisons involving `NULL` now evaluate to unknown
rather than false. `<=>` remains null-safe equality.

Metadata text equality, `IN`, `LIKE`, and `BETWEEN` bound checks use each
metadata column definition's declared collation for the current ASCII subset.
`_bin` columns compare case-sensitively; the currently admitted `_general_ci`,
`_tolower_ci`, `utf8mb4_0900_ai_ci`, `utf8mb4_unicode_ci`, and
`utf8mb4_unicode_520_ci` columns compare ASCII letters case-insensitively.
Standalone metadata string ordering comparisons such as `WORD < 'S'` remain
outside this slice.

Numeric metadata comparisons use signed 64-bit integer conversion for the
supported metadata values and literals.

### LIKE

`LIKE` decodes the pattern as a MyLite SQL string literal, rejects embedded NUL
bytes and non-ASCII pattern text, and matches the row value as metadata text.

`%` matches zero or more bytes and `_` matches one byte in the current ASCII
subset. Case sensitivity follows the left metadata column's declared collation.

When `NO_BACKSLASH_ESCAPES` is disabled, backslash escapes `%`, `_`, and a
literal backslash in the pattern. When `NO_BACKSLASH_ESCAPES` is enabled,
backslash is treated as an ordinary pattern byte.

If the metadata value is `NULL`, `LIKE` evaluates to unknown.

### IN

`IN` evaluates the left metadata value against each list item:

- If the left value is `NULL`, the result is unknown.
- If any non-`NULL` list item compares equal, the result is true.
- If no item matches and at least one list item is `NULL`, the result is
  unknown.
- Otherwise the result is false.

Text metadata columns use metadata collation comparison. Numeric metadata
columns use the same signed integer comparison conversion as existing metadata
comparisons. Numeric string list items such as `'01'` compare numerically for
numeric metadata columns.

### BETWEEN

`BETWEEN` is inclusive. It evaluates as the left metadata value being greater
than or equal to the lower bound and less than or equal to the upper bound under
metadata text collation ordering or numeric conversion, depending on the left
metadata column. If the left value or either bound is `NULL`, the result is
unknown. `NOT BETWEEN` is represented as `NOT` over `BETWEEN`.

## Unsupported

Deferred until broader query-expression support:

- `IN (subquery)` and row constructors;
- expression, function, parameter, variable, user-variable, cast, collation, or
  arithmetic predicate operands;
- explicit `LIKE ... ESCAPE`;
- standalone string ordering comparisons outside `BETWEEN`;
- `REGEXP` / `RLIKE` in `INFORMATION_SCHEMA` predicates;
- expression projection or ordering;
- joins, CTEs, unions, grouping other than the existing `COUNT(*)`, window
  functions, derived tables, and optimizer hints;
- non-ASCII pattern and metadata collation parity beyond the current ASCII
  subset;
- warning-producing numeric conversion for metadata predicates;
- privilege filtering, roles, metadata locks, and complete MySQL system
  catalogs.

Unsupported shapes must fail deterministically before any unrelated execution
path is used.

## Diagnostics

Supported diagnostics:

- unknown metadata table: MySQL-compatible `1109 / 42S02`;
- unknown projection, predicate, or order column: MySQL-compatible
  `1054 / 42S22` with the existing clause-specific message;
- unsupported metadata predicate syntax: deterministic MyLite unsupported
  diagnostic;
- unsupported literal or expression value: deterministic MyLite unsupported
  diagnostic;
- unsupported `IN` subquery in metadata predicates: deterministic MyLite
  unsupported diagnostic;
- integer value out of signed 64-bit range: existing numeric out-of-range
  diagnostic;
- `LIKE` pattern with embedded NUL or non-ASCII bytes: deterministic MyLite
  unsupported diagnostic;
- allocation failure: `MYLITE_NOMEM` and handle-owned diagnostic;
- public API misuse: unchanged.

## Tests

The C test coverage must include:

- `LIKE` over metadata table names and column names, including case-insensitive
  matching and escaped wildcard behavior;
- `LIKE` under `NO_BACKSLASH_ESCAPES`;
- `IN` and `NOT IN` over text metadata columns;
- `IN` over numeric metadata columns, including numeric string coercion;
- `IN` and `NOT IN` with `NULL` list items;
- `BETWEEN` and `NOT BETWEEN` over numeric metadata columns;
- `NULL` predicate truth under `= NULL`, `NOT = NULL`, `<=> NULL`, `NOT LIKE`,
  and boolean composition;
- aliases and qualified metadata predicate columns;
- unsupported `IN` subquery and unsupported explicit `LIKE ... ESCAPE`;
- unknown predicate columns still produce the existing MySQL-compatible
  diagnostics;
- warning count is zero for supported queries;
- reopen behavior and descriptor updates remain visible through supported
  metadata predicates.

The MySQL expectation script records the MySQL 8.4.9 behavior for these
user-visible outcomes.

## Verification

Required before marking done:

1. `cmake --build --preset dev`
2. `ctest --preset dev --output-on-failure -R 'libmylite\\.(parser|runtime\\.information_schema_.*)$'`
3. `MYLITE_MYSQL_BIN=/opt/homebrew/opt/mysql@8.4/bin/mysql MYLITE_MYSQL_SOCKET=/tmp/mylite-mysql-849.jsgoZE/mysql.sock ./packages/libmylite/tests/mysql_baseline_information_schema_predicates_expectations.sh`
4. `cmake --workflow --preset check`
