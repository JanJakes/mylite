# Baseline Select Order Limit Lifecycle

## Status

This feature specifies the next narrow user-visible query slice for file-backed
`.mylite` handles. It adds descriptor-driven `ORDER BY` and `LIMIT` handling to
the existing single-table `SELECT` path on top of `mylite_execute()`,
statement context, parser scaffolding, shifted `.mylite` storage, durable
catalog descriptors, create/drop/rename table lifecycle, integer/`NULL` row
values, and baseline descriptor-driven `WHERE`.

The feature is intentionally not full MySQL ordering or limiting support. It
supports one persistent base table, descriptor-backed projections, the existing
optional single-column predicate subset, documented descriptor-backed order
keys, and decimal integer limit literals in a signed 64-bit physical binding
range.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- SQLite connection bootstrap policy:
  `docs/specs/sqlite-connection-bootstrap-policy/specs.md`
- File-backed MyLite opening VFS:
  `docs/specs/file-backed-mylite-opening-vfs/specs.md`
- MyLite file-format preamble:
  `docs/specs/mylite-file-format/specs.md`
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline basic table lifecycle:
  `docs/specs/baseline-basic-table-lifecycle/specs.md`
- Baseline table rename lifecycle:
  `docs/specs/baseline-table-rename-lifecycle/specs.md`
- Baseline row values lifecycle:
  `docs/specs/baseline-row-values-lifecycle/specs.md`
- Baseline select where lifecycle:
  `docs/specs/baseline-select-where-lifecycle/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, sorting rows:
  https://dev.mysql.com/doc/refman/8.4/en/sorting-rows.html
- MySQL 8.4 Reference Manual, `NULL` values:
  https://dev.mysql.com/doc/refman/8.4/en/null-values.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## Scope

The implementation must add:

- parser and AST support for optional `ORDER BY` and `LIMIT` clauses on the
  existing descriptor-backed table `SELECT`;
- `SELECT *` and explicit unqualified projection column lists over a single
  persistent base table;
- the existing unqualified and schema-qualified source-table resolution policy;
- the existing optional baseline `WHERE` predicate subset;
- descriptor-column `ORDER BY` keys resolved from MyLite column descriptors;
- the WordPress-shaped boolean order expression
  `descriptor_string_column LIKE string_literal`;
- optional `ASC` and `DESC`, with omitted direction meaning ascending;
- MySQL-compatible `NULL` placement for supported integer order keys:
  `NULL` values first for ascending order and last for descending order;
- `LIMIT row_count`, `LIMIT row_count OFFSET offset`, and
  `LIMIT offset, row_count`;
- unsigned decimal integer limit and offset literals in the range
  `0` through `9223372036854775807`;
- MyLite-owned limit/offset conversion before SQLite binding;
- generated SQLite SQL built only from descriptors and stable physical table
  names;
- prepared-statement binding for predicate, limit, and offset values;
- result rows, column names, duplicate projection behavior, `NULL` value
  behavior, affected rows, and warning count matching the existing result API;
- tests and MySQL 8.4.9 expectation artifacts for supported behavior and
  deliberately rejected wider MySQL forms.

## Non-Goals

This feature must not implement:

- arbitrary expression `ORDER BY`, ordinal `ORDER BY`, aliases,
  table-qualified order columns, collations, or order-by expressions beyond
  the documented descriptor, `FIELD()`, `RAND()`, cast/convert, and narrow
  `column LIKE string_literal` subsets;
- signed limit literals, parameters, local variables, stored-program limit
  expressions, string/decimal/float/hex/bit limit literals, or unsigned 64-bit
  limit values above the signed 64-bit SQLite binding range;
- joins, grouping, `HAVING`, `WITH ROLLUP`, subqueries, CTEs, set operations,
  window functions, aggregate functions, locking clauses, query modifiers,
  `DISTINCT`, expression projection, aliases, or arbitrary SQLite pass-through;
- indexes, helper sort keys, generalized comparison hooks, collation-aware
  sorting, protocol-grade metadata, or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public call
  validation, result-handle ownership, public misuse behavior, and failure
  cleanup.
- Statement context owns the top-level statement boundary: diagnostics reset,
  warning count, affected rows, and backend execution status. Sorted and
  limited reads do not mutate statement transaction state.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves schemas, table descriptors, projection
  columns, predicate columns, and ordering columns against MyLite catalog
  descriptors; rejects unsupported clause shapes; converts supported
  predicate, limit, and offset literals; and generates a physical SQLite query
  plan.
- The catalog module owns `_mylite_catalog_*` rows, descriptor versions,
  catalog generation, and descriptor-cache invalidation. Sorted and limited
  reads do not mutate catalog rows, descriptor versions, generation, caches, or
  `sqlite_schema_generation`.
- The result builder owns descriptor-driven result column names and copied text
  values. SQL `NULL` remains represented as a `NULL` pointer returned by
  `mylite_result_value_text()`.
- SQLite owns physical b-tree row storage and scan/sort/limit execution for
  generated descriptor-safe SQL. SQLite schema text, `sqlite_master`, and
  `PRAGMA` output are not metadata authority.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Sorted and limited reads must not write through byte range `[0, 4096)`.

## Supported SQL Grammar

The feature admits one top-level statement per `mylite_execute()` call.

Supported table `SELECT` subset:

```sql
SELECT * FROM table_name [WHERE predicate] [ORDER BY order_key [ASC | DESC]] [limit_clause]
SELECT column_name[, column_name ...]
  FROM table_name [WHERE predicate] [ORDER BY order_key [ASC | DESC]] [limit_clause]
```

`table_name` uses the existing table lifecycle subset:

```sql
table_name:
    identifier
  | identifier.identifier
```

The supported predicate subset is exactly the subset from
`baseline-select-where-lifecycle`.

Supported ordering subset:

```sql
order_key:
    column_name
  | column_name LIKE string_literal
```

The parser may admit a qualified identifier as an order key so the analyzer can
reject it with a deterministic unsupported diagnostic, but the supported
semantic subset requires one unqualified descriptor column.

Supported limit subset:

```sql
limit_clause:
    LIMIT row_count
  | LIMIT row_count OFFSET offset
  | LIMIT offset, row_count

row_count:
    unsigned_decimal_integer_literal

offset:
    unsigned_decimal_integer_literal
```

`+1`, `-1`, decimal, float, string, hex, bit, parameter, variable, function, and
expression limit forms are rejected in this phase.

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar extension, not MySQL's full
grammar:

```lemon
select_statement ::=
    SELECT select_item_list FROM table_name where_clause_opt order_clause_opt limit_clause_opt.
select_statement ::=
    SELECT STAR FROM table_name where_clause_opt order_clause_opt limit_clause_opt.

where_clause_opt ::= .
where_clause_opt ::= WHERE predicate.

order_clause_opt ::= .
order_clause_opt ::= ORDER BY select_order_key order_direction_opt.

select_order_key ::= qualified_identifier.
select_order_key ::= qualified_identifier LIKE string_literal.

order_direction_opt ::= .
order_direction_opt ::= ASC.
order_direction_opt ::= DESC.

limit_clause_opt ::= .
limit_clause_opt ::= LIMIT limit_integer.
limit_clause_opt ::= LIMIT limit_integer OFFSET limit_integer.
limit_clause_opt ::= LIMIT limit_integer COMMA limit_integer.

limit_integer ::= INTEGER.
```

For `LIMIT offset, row_count`, the parser builds an AST limit node whose first
semantic child is `row_count` and whose optional second semantic child is
`offset`. This keeps runtime planning independent of the spelling used in the
SQL input.

## Schema And Table Resolution

`SELECT ... FROM t ORDER BY ... LIMIT ...` uses the connection's selected
schema. If no selected schema exists, the statement fails with MySQL error
`1046`, SQLSTATE `3D000`, and message `No database selected`.

`SELECT ... FROM db.t ORDER BY ... LIMIT ...` resolves `db` against existing
MyLite schema descriptors regardless of the selected schema. Unknown schemas
fail with MySQL error `1049`, SQLSTATE `42000`, and message
`Unknown database '<schema>'`.

The source table descriptor must exist and must have
`kind == MYLITE_CATALOG_TABLE_KIND_BASE`. Unknown tables fail with MySQL error
`1146`, SQLSTATE `42S02`, and message
`Table '<schema>.<table>' doesn't exist`. Unsupported object kinds are rejected
with a deterministic unsupported diagnostic until view and temporary-table
descriptors are specified.

User-authored schema and table names beginning with `_mylite_`, using ASCII
case-insensitive comparison after identifier unquoting, are rejected before any
SQLite SQL is generated.

## Column Resolution

Projection and predicate column resolution preserve the
`baseline-select-where-lifecycle` behavior:

- `*` expands to all descriptor columns in catalog ordinal order;
- explicit projection names resolve case-insensitively against descriptor
  column names;
- result column names are the descriptor names of selected columns;
- duplicate projected column names are allowed and produce duplicate result
  columns;
- unknown projection names fail with MySQL error `1054`, SQLSTATE `42S22`, and
  message `Unknown column '<column>' in 'field list'`;
- predicate columns are one unqualified descriptor column, with unknown names
  failing as `Unknown column '<column>' in 'where clause'`;
- table-qualified projection and predicate names remain unsupported.

Ordering column resolution uses the same descriptor list:

- supported descriptor order keys use unqualified descriptor column names;
- the order column does not need to be projected;
- `column LIKE string_literal` order keys resolve the left operand as an
  unqualified descriptor string-family column and use a literal pattern;
- unknown ordering names fail with MySQL error `1054`, SQLSTATE `42S22`, and
  message `Unknown column '<column>' in 'order clause'`;
- table-qualified order columns are rejected with a deterministic unsupported
  diagnostic before SQLite SQL is generated;
- aliases, ordinals, unsupported expressions, and collation modifiers are
  rejected unless they match a later documented order-key slice.

The current catalog stores `(table_id, name)` with SQLite binary uniqueness.
MyLite analysis compares the supported identifier subset with ASCII
case-insensitive folding, matching previous baseline column-resolution policy.

## Ordering Semantics

The admitted order column must be one of the baseline integer descriptor
families already stored as SQLite `INTEGER` or SQL `NULL`.

For supported `ORDER BY`:

- omitted direction and explicit `ASC` sort ascending;
- `DESC` sorts descending;
- SQL `NULL` values sort before non-`NULL` values for ascending order and
  after non-`NULL` values for descending order;
- duplicate sort-key values are ties. Without a second sort key, MyLite does
  not claim any compatibility contract for the relative order of tied rows;
- `column LIKE string_literal` produces MySQL-compatible boolean order keys
  for the supported string-family descriptor subset, with nonmatching rows
  ordering as `0`, matching rows ordering as `1`, and `NULL` inputs producing
  `NULL`;
- queries without `ORDER BY` have no general row-order compatibility contract.

SQLite's native ordering for `INTEGER` and `NULL` matches the supported MySQL
8.4.9 behavior for this descriptor-limited subset, so no SQLite fork hook or
custom collation is needed.

## Limit And Offset Semantics

For supported `LIMIT` forms:

- `LIMIT row_count` returns at most `row_count` rows after filtering and
  ordering, starting at offset `0`;
- `LIMIT row_count OFFSET offset` skips `offset` rows, then returns at most
  `row_count` rows;
- `LIMIT offset, row_count` has the same meaning as
  `LIMIT row_count OFFSET offset`;
- `LIMIT 0` returns no rows but preserves result column names, warning count,
  and affected-row semantics;
- a row count larger than the result set returns all remaining rows;
- an offset greater than or equal to the result-set size returns no rows;
- supported in-range reads produce no warnings.

MySQL 8.4.9 accepts unsigned limit values up to
`18446744073709551615`. MyLite deliberately admits only
`0` through `9223372036854775807` in this phase because generated SQLite
statements bind limit and offset through `sqlite3_bind_int64()`. Values outside
that range fail before SQLite SQL is prepared with a deterministic
MyLite-specific syntax diagnostic. This prevents silent modulo, signed-wrap, or
`LIMIT -1` behavior from becoming part of the public contract.

Signed limit literals are not admitted. Observed MySQL 8.4.9 rejects
`LIMIT +1` and `LIMIT -1` with syntax error `1064`, SQLSTATE `42000`.

## Physical SQLite Handling

Generated SQL is built only from descriptors:

- table names come from `_mylite_catalog_tables.physical_name`;
- projection, predicate, and ordering column names come from MyLite column
  descriptors;
- every SQLite identifier is double-quoted with embedded double quotes doubled;
- comparison predicate values, limit row count, and offset values are bound
  through SQLite prepared-statement parameters;
- generated SQL contains no interpolated user literals.

The generated query shape is:

```sql
SELECT "<projection_col1>", "<projection_col2>", ...
FROM "<physical_name>"
[WHERE "<predicate_col>" <op> ?1 | WHERE "<predicate_col>" IS [NOT] NULL]
[ORDER BY "<order_col>" ASC|DESC | ORDER BY ("<order_col>" LIKE ?n ESCAPE '\') ASC|DESC]
[LIMIT ?n [OFFSET ?n]]
```

Predicate binding, when present, keeps the existing first parameter. Limit and
offset parameters are numbered after any predicate parameter. For comma-limit
syntax, runtime binding still binds row count first and offset second because
the generated SQLite SQL uses `LIMIT ? OFFSET ?`.

No catalog rows, descriptor versions, descriptor caches, catalog generation, or
`sqlite_schema_generation` change for sorted or limited reads. The `.mylite`
preamble remains untouched because reads occur only through the shifted SQLite
payload.

## Result Behavior

Successful sorted and limited table `SELECT` returns a text result through the
existing public result API:

- `column_count` equals the selected descriptor column count;
- `row_count` equals the number of rows read after filtering, ordering, and
  limiting;
- `affected_rows == 0`;
- `warning_count == 0`;
- column names come from selected descriptors;
- wildcard order follows catalog ordinal position;
- duplicate projected columns are preserved as duplicate result columns;
- integer values are decimal strings;
- SQL `NULL` values return `NULL` from `mylite_result_value_text()`.

This phase does not claim protocol-grade metadata, column flags, charsets,
origin metadata, expression labels, or full MySQL type codes.

## Diagnostics

The public function return code indicates MyLite API status. SQL diagnostics
are stored on the database handle.

| Condition | Return | Diagnostic |
| --- | --- | --- |
| Success | `MYLITE_OK` | `0`, `00000`, `not an error` |
| Lexer or parser error | `MYLITE_ERROR` | `1064`, `42000`, MySQL-style syntax message |
| Unsupported select, order, limit, or predicate shape | `MYLITE_ERROR` | `1064`, `42000`, deterministic unsupported message |
| No selected schema | `MYLITE_ERROR` | `1046`, `3D000`, `No database selected` |
| Unknown schema | `MYLITE_ERROR` | `1049`, `42000`, `Unknown database '<schema>'` |
| Unknown table | `MYLITE_ERROR` | `1146`, `42S02`, `Table '<schema>.<table>' doesn't exist` |
| Reserved schema name | `MYLITE_ERROR` | `1102`, `42000`, `Incorrect database name '<name>'` |
| Reserved table name | `MYLITE_ERROR` | `1103`, `42000`, `Incorrect table name '<name>'` |
| Unknown projection column | `MYLITE_ERROR` | `1054`, `42S22`, `Unknown column '<column>' in 'field list'` |
| Unknown predicate column | `MYLITE_ERROR` | `1054`, `42S22`, `Unknown column '<column>' in 'where clause'` |
| Unknown ordering column | `MYLITE_ERROR` | `1054`, `42S22`, `Unknown column '<column>' in 'order clause'` |
| Unsupported order expression | `MYLITE_ERROR` | `1064`, `42000`, deterministic unsupported message |
| Unsupported limit expression or literal | `MYLITE_ERROR` | `1064`, `42000`, deterministic syntax or unsupported message |
| Limit or offset outside the supported signed 64-bit binding range | `MYLITE_ERROR` | `1064`, `42000`, `LIMIT literal is outside the supported range` |
| Predicate literal outside supported descriptor range | `MYLITE_ERROR` | `1264`, `22003`, `Out of range value for column '<column>' in WHERE` |
| Unsupported object kind | `MYLITE_ERROR` | `1064`, `42000`, deterministic unsupported message |
| Physical SQLite failure | `MYLITE_ERROR` | `1105`, `HY000`, deterministic internal failure message |
| Allocation failure | `MYLITE_NOMEM` | `MYLITE_NOMEM`, `HY001`, allocation message |

No public ABI changes are planned, so existing public execution/result misuse
behavior is preserved.

## MySQL 8.4.9 Runtime Observations

The following behavior was checked on 2026-05-07 against the official
`mysql:8.4.9` Docker image with:

```sh
docker exec -i mylite-mysql-849 mysql -uroot --batch --raw --force --show-warnings
```

Observed behavior used by this feature:

| SQL | MySQL 8.4.9 observation |
| --- | --- |
| `SELECT i FROM numbers ORDER BY i` | Rows are sorted ascending by `i`; omitted direction matches `ASC`. |
| `SELECT i FROM numbers ORDER BY i DESC` | Rows are sorted descending by `i`. |
| `SELECT n FROM numbers ORDER BY n` | `NULL` sort keys appear before non-`NULL` sort keys. |
| `SELECT n FROM numbers ORDER BY n DESC` | `NULL` sort keys appear after non-`NULL` sort keys. |
| `SELECT id FROM numbers ORDER BY id LIMIT 0` | Returns no rows and no warnings. |
| `SELECT id FROM numbers ORDER BY id LIMIT 2` | Returns the first two ordered rows. |
| `SELECT id FROM numbers ORDER BY id LIMIT 2 OFFSET 1` | Skips the first ordered row and returns the next two. |
| `SELECT id FROM numbers ORDER BY id LIMIT 1, 2` | Equivalent to `LIMIT 2 OFFSET 1`. |
| `SELECT id FROM numbers ORDER BY id LIMIT 1, 0` | Returns no rows. |
| `SELECT id FROM numbers ORDER BY id LIMIT 0, 1` | Returns the first ordered row. |
| `SELECT id FROM numbers WHERE n IS NULL ORDER BY id DESC LIMIT 1` | `WHERE`, `ORDER BY`, and `LIMIT` compose in that clause order. |
| `SELECT id FROM numbers ORDER BY title LIKE '%foo%' DESC, nn DESC` | Matching rows sort before nonmatching rows, then the second descriptor key breaks ties. |
| `SELECT id FROM numbers ORDER BY id LIMIT +1` | Error `1064`, SQLSTATE `42000`. |
| `SELECT id FROM numbers ORDER BY id LIMIT -1` | Error `1064`, SQLSTATE `42000`. |
| `SELECT id FROM numbers ORDER BY id LIMIT 1.0` | Error `1064`, SQLSTATE `42000`. |
| `SELECT id FROM numbers ORDER BY id LIMIT '1'` | Error `1064`, SQLSTATE `42000`. |
| `SELECT id FROM numbers ORDER BY id LIMIT 0x1` | Error `1064`, SQLSTATE `42000`. |
| `SELECT id FROM numbers ORDER BY id LIMIT b'1'` | Error `1064`, SQLSTATE `42000`. |
| `SELECT id FROM numbers ORDER BY id LIMIT ?` outside prepared statements | Error `1064`, SQLSTATE `42000`. |
| Unknown `ORDER BY` column | Error `1054`, SQLSTATE `42S22`, `Unknown column '<column>' in 'order clause'`. |
| `ORDER BY` table-qualified columns, expressions, ordinals, aliases, and multiple keys | Accepted by MySQL for ordinary queries; intentionally rejected by MyLite in this phase. |
| `LIMIT 18446744073709551615` | Accepted by MySQL as a very large row count; MyLite rejects values above signed 64-bit range in this phase. |

The reproducible probe script for this phase is
`packages/libmylite/tests/mysql_baseline_select_order_limit_lifecycle_expectations.sh`.

## Compatibility Status

This feature moves only the exact supported subset to partial support after
implementation:

- `SELECT`: descriptor-driven single persistent base-table `SELECT *` and
  unqualified projection reads with optional baseline `WHERE`, documented
  `ORDER BY` keys, and limited `LIMIT`;
- `ORDER BY`: descriptor columns, multiple descriptor keys, and the narrow
  `column LIKE string_literal` boolean key, with optional `ASC` or `DESC`;
- `LIMIT` / `OFFSET`: decimal unsigned integer literals only, in the signed
  64-bit SQLite binding range, for `LIMIT row_count`,
  `LIMIT row_count OFFSET offset`, and `LIMIT offset, row_count`;
- numeric literals: decimal integer literals additionally serve as supported
  limit and offset inputs in this narrow query clause.

Full `ORDER BY`, full `LIMIT/OFFSET`, expression ordering, ordinal ordering,
aliases, collation-aware sorting, joins, grouping, subqueries, query modifiers,
protocol-grade metadata, and arbitrary SQLite pass-through remain unsupported.

## Tests

Add fast plain C tests under `packages/libmylite/tests/`, registered with a
dotted CTest name such as
`libmylite.runtime.select_order_limit_lifecycle`.

Coverage must include:

- parser/AST acceptance for wildcard and explicit projection selects with
  `ORDER BY`, `ASC`, `DESC`, `LIMIT row_count`,
  `LIMIT row_count OFFSET offset`, and `LIMIT offset, row_count`;
- successful `SELECT *` and `SELECT col, col2` ordered reads over `INT`,
  `INTEGER`, `BIGINT`, `INT UNSIGNED`, `INTEGER UNSIGNED`, and
  `BIGINT UNSIGNED` columns within the supported physical range;
- default ascending order, explicit `ASC`, explicit `DESC`, nullable integer
  columns, `NULL` ordering, duplicate sort-key ties without overclaiming tie
  order, and duplicate projected columns;
- multiple descriptor sort keys and the narrow `column LIKE string_literal`
  boolean sort key;
- `WHERE` combined with `ORDER BY` and `LIMIT`;
- `LIMIT 0`, exact row counts, row counts larger than the result set, offset
  forms, and offset beyond the result set;
- signed and unsigned boundary values where sorting or limit conversion matters;
- schema-qualified and unqualified target table resolution, no selected schema,
  unknown schema, unknown table, and reserved `_mylite_*` source names;
- projection column names, row count, text values, `NULL` values, warning
  count, and affected-row semantics;
- unknown ordering columns, unknown predicate columns, and unknown projected
  columns;
- unsupported order/limit syntax rejected deterministically: table-qualified
  order columns, expression order keys outside documented narrow subsets,
  ordinal order keys, aliases, string/decimal/float/hex/bit limit literals,
  parameters, functions, joins, grouping, `HAVING`, subqueries, locking clauses,
  and query modifiers;
- reopen persistence, table rename visibility, and drop behavior;
- physical SQLite payload behavior without touching the MyLite preamble;
- independent file-backed handles with independent sorted/limited row state;
- zero-initialized cleanup for any new planner/result objects;
- existing lexer, parser, runtime handle, diagnostics, statement context,
  result metadata, SQLite bootstrap policy, file-backed opening, VFS, catalog
  foundation, basic create/drop lifecycle, table rename lifecycle, row values
  lifecycle, select-where lifecycle, client-data, and registration tests still
  pass.

## Build Integration

Add any new runtime/analyzer/planner/catalog SQL execution sources and tests to
`packages/libmylite/CMakeLists.txt`. First-party warning and clang-tidy policy
must apply to new code. Vendored SQLite warning policy must remain unchanged.

## Verification

Before marking the feature done, run:

```sh
cmake --build --preset dev
ctest --preset dev -R '^libmylite\.runtime\.select_order_limit_lifecycle$' --output-on-failure
ctest --preset dev -R '^libmylite\.(parser|runtime\.basic_table_lifecycle|runtime\.table_rename_lifecycle|runtime\.row_values_lifecycle|runtime\.select_where_lifecycle)$' --output-on-failure
./packages/libmylite/tests/mysql_baseline_select_order_limit_lifecycle_expectations.sh
cmake --workflow --preset check
```

Then review the final diff for architecture boundaries, public ABI stability,
independently authored grammar/spec text, MySQL 8.4.9 evidence, catalog
authority, descriptor-driven physical sorting/limiting, integer conversion
correctness for the admitted limit/offset subset, `NULL` ordering correctness,
file-format safety, VFS preservation, zero-init safety, cleanup on failure,
scope control, compatibility-matrix accuracy, and test relevance.
