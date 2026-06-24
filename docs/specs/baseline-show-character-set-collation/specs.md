# Baseline SHOW CHARACTER SET and SHOW COLLATION

## Status

This feature specifies a narrow static introspection slice for
`SHOW CHARACTER SET` / `SHOW CHARSET` and `SHOW COLLATION`. It builds on
`mylite_execute()`, statement context, the parser scaffold, `SHOW LIKE`
pattern decoding, fixed `utf8mb4` / `utf8mb4_0900_ai_ci` table option
acceptance, `SHOW CREATE DATABASE`, `SHOW CREATE TABLE`, and
`SHOW TABLE STATUS`.

The feature is intentionally metadata-only. It exposes the MySQL 8.4.9 target
character-set and collation catalogs for introspection, while keeping charset
admission, conversion, comparison, coercibility, connection charset state,
`SET NAMES`, `SET CHARACTER SET`, and system data-dictionary storage separate
compatibility surfaces.

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
- File-backed MyLite opening and VFS:
  `docs/specs/file-backed-mylite-opening-vfs/specs.md`
- MyLite file format: `docs/specs/mylite-file-format/specs.md`
- Baseline catalog, table option, and introspection slices:
  `docs/specs/baseline-catalog-foundation/specs.md`,
  `docs/specs/baseline-table-charset-collation-surface/specs.md`,
  `docs/specs/baseline-show-like-filters/specs.md`,
  `docs/specs/baseline-show-create-database/specs.md`,
  `docs/specs/baseline-show-create-table/specs.md`, and
  `docs/specs/baseline-show-table-status-introspection/specs.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold: `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `SHOW` statements:
  https://dev.mysql.com/doc/refman/8.4/en/show.html
- MySQL 8.4 Reference Manual, `SHOW CHARACTER SET`:
  https://dev.mysql.com/doc/refman/8.4/en/show-character-set.html
- MySQL 8.4 Reference Manual, `SHOW COLLATION`:
  https://dev.mysql.com/doc/refman/8.4/en/show-collation.html
- MySQL 8.4 Reference Manual, extensions to `SHOW` statements:
  https://dev.mysql.com/doc/refman/8.4/en/extended-show.html
- MySQL 8.4 Reference Manual, supported character sets and collations:
  https://dev.mysql.com/doc/refman/8.4/en/charset-charsets.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLLATIONS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-collations-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime using TCP:

- `SELECT VERSION(), @@character_set_server, @@collation_server` reported
  `8.4.9`, `utf8mb4`, and `utf8mb4_0900_ai_ci`.
- `SHOW CHARACTER SET LIKE 'utf8mb4'` and
  `SHOW CHARSET LIKE 'utf8mb4'` returned one row:

```text
Charset  Description   Default collation   Maxlen
utf8mb4  UTF-8 Unicode utf8mb4_0900_ai_ci  4
```

- `SHOW COLLATION LIKE 'utf8mb4_0900_ai_ci'` returned one row:

```text
Collation           Charset Id  Default Compiled Sortlen Pad_attribute
utf8mb4_0900_ai_ci utf8mb4 255 Yes     Yes      0       NO PAD
```

- Successful statements return result sets, leave `@@warning_count == 0`, and
  make the following `ROW_COUNT()` return `-1`.
- `LIKE` matching for charset and collation names is case-insensitive in the
  observed runtime. `LIKE 'UTF8MB4'` matches `utf8mb4`, and
  `LIKE 'UTF8MB4_0900_AI_CI'` matches `utf8mb4_0900_ai_ci`.
- `%` and `_` wildcard matching follows normal `SHOW LIKE` behavior. Escaped
  underscores match literal underscores, for example
  `LIKE 'utf8mb4\_0900\_ai\_ci'`.
- A no-match `LIKE` pattern returns an empty result set with warning count `0`
  and following `ROW_COUNT() == -1`.
- MySQL 8.4.9 accepts `WHERE` filters on these statements.
- Unsupported forms observed as syntax errors include plural spellings
  (`SHOW CHARACTER SETS`, `SHOW CHARSETS`, `SHOW COLLATIONS`), non-string
  pattern expressions (`LIKE 1`, `LIKE NULL`), national-string patterns
  (`LIKE N'utf8mb4'`), and charset introducer patterns
  (`LIKE _utf8mb4'utf8mb4'`).
- MySQL's full default runtime exposed 41 character sets and 286 collations in
  the tested build. MyLite exposes those metadata rows without implying that
  every listed charset or collation is admitted for DDL, conversion, or
  comparison semantics.

`mysql --column-type-info -vvv` reported these visible column properties:

```text
SHOW CHARACTER SET:
Charset            VAR_STRING len 64   NOT_NULL
Description        VAR_STRING len 2048 NOT_NULL
Default collation  VAR_STRING len 64   NOT_NULL
Maxlen             LONG unsigned len 10 NOT_NULL

SHOW COLLATION:
Collation      VAR_STRING len 64 NOT_NULL
Charset        VAR_STRING len 64 NOT_NULL
Id             LONGLONG unsigned len 20 NOT_NULL
Default        VAR_STRING len 3  NOT_NULL
Compiled       VAR_STRING len 3  NOT_NULL
Sortlen        LONG unsigned len 10 NOT_NULL
Pad_attribute  STRING enum len 9 NOT_NULL
```

MyLite's current public result metadata remains name-oriented and does not yet
expose full MySQL wire metadata. This slice pins the result column names and
cell values, while broader result metadata compatibility remains tracked
separately.

## Scope

The implementation must add:

- parser and AST support for `SHOW CHARACTER SET`, `SHOW CHARSET`, and
  `SHOW COLLATION`;
- optional existing `LIKE 'pattern'` filter reuse;
- limited `WHERE` filters over displayed catalog columns;
- static MySQL 8.4.9 target catalog rows for all 41 character sets and 286
  collations observed by the expectation artifact;
- MySQL 8.4.9 result column names and visible cell values for those rows;
- case-insensitive ASCII `LIKE` and documented `WHERE` matching for these
  catalog names and displayed metadata values;
- result-set warning and row-count behavior matching observed MySQL 8.4.9 and
  existing MyLite result conventions;
- deterministic syntax rejection for unsupported wider forms;
- fast C tests and a MySQL 8.4.9 expectation artifact for supported behavior
  and deliberately deferred wider forms.

## Non-Goals

This feature must not implement:

- charset or collation DDL admission merely because a metadata row exists;
- string conversion, comparison, ordering, grouping, or coercibility semantics
  for every cataloged charset or collation;
- direct `mysql` data-dictionary tables for charset/collation storage;
- connection character-set state, `SET NAMES`, `SET CHARACTER SET`,
  `character_set_*` variables, or `collation_*` variables;
- string types, string comparison semantics, collation coercibility,
  trailing-space comparison behavior, or expression metadata;
- privilege filtering, system schema integration, arbitrary SQLite metadata
  reads, arbitrary SQLite SQL pass-through, or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, statement-boundary row-count state, and failure
  cleanup.
- Statement context owns diagnostics reset, warning count, and statement
  completion. Successful statements are result-set statements and therefore
  store `-1` as the connection-local previous row count.
- Lexer/parser/AST own syntax admission and source spans. They admit only the
  supported statement forms plus optional string-literal `LIKE` clauses or
  `WHERE` predicates over the emitted result columns.
- Runtime owns decoding the optional `LIKE` literal through the existing
  `SHOW LIKE` filter, evaluating supported `WHERE` predicates, applying ASCII
  case-insensitive matching, and building the static result rows.
- The catalog module remains authoritative for schema/table/column
  descriptors. This feature does not add charset/collation descriptor rows and
  does not mutate catalog rows, descriptor versions, descriptor caches,
  catalog generation, or `sqlite_schema_generation`.
- Result builders own the MySQL-visible column names and row values.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This static introspection does not read or write SQLite tables and must not
  touch byte range `[0, 4096)`.

## Supported SQL Grammar

Supported subset:

```sql
SHOW {CHARACTER SET | CHARSET} [LIKE 'pattern']
SHOW COLLATION [LIKE 'pattern']
SHOW {CHARACTER SET | CHARSET} WHERE output_column_predicate
SHOW COLLATION WHERE output_column_predicate
```

The `LIKE` pattern must be one regular string literal token accepted by the
existing `SHOW LIKE` filter decoder. National string literals, charset
introducers, numeric literals, `NULL`, concatenated strings, expressions,
functions, and parameters are not admitted for `LIKE`.

`WHERE` predicates are evaluated against the output column names. MyLite
supports string and `NULL` literal comparisons, `LIKE`, `IN`, `IS NULL`,
`IS NOT NULL`, `NOT`, `AND`, and `OR`. String comparisons are ASCII
case-insensitive. Unsupported predicate forms fail with a deterministic
compatibility error rather than being passed through to SQLite.

MyLite Lemon-syntax grammar snippets:

```lemon
statement ::= show_character_set_statement.
statement ::= show_collation_statement.

show_character_set_statement ::=
    SHOW CHARACTER SET show_catalog_filter_opt.

show_character_set_statement ::=
    SHOW CHARSET show_catalog_filter_opt.

show_collation_statement ::=
    SHOW COLLATION show_catalog_filter_opt.

show_catalog_filter_opt ::= .
show_catalog_filter_opt ::= LIKE STRING.
show_catalog_filter_opt ::= WHERE predicate.
```

The AST stores the optional `LIKE` literal or `WHERE` clause as the first child
of the dedicated statement node when present. Runtime treats absence of a child
as an unfiltered static catalog request.

## Representative Catalog Rows

`SHOW CHARACTER SET` emits these columns for each catalog row. The target
default `utf8mb4` row is:

| Column | MyLite value |
| --- | --- |
| `Charset` | `utf8mb4` |
| `Description` | `UTF-8 Unicode` |
| `Default collation` | `utf8mb4_0900_ai_ci` |
| `Maxlen` | `4` |

`SHOW COLLATION` emits these columns for each catalog row. The target default
`utf8mb4_0900_ai_ci` row is:

| Column | MyLite value |
| --- | --- |
| `Collation` | `utf8mb4_0900_ai_ci` |
| `Charset` | `utf8mb4` |
| `Id` | `255` |
| `Default` | `Yes` |
| `Compiled` | `Yes` |
| `Sortlen` | `0` |
| `Pad_attribute` | `NO PAD` |

Catalog values are returned through the existing text-cell result
representation. The `SHOW` statements use the MySQL 8.4.9 target metadata
values pinned by the runtime expectation artifact and C tests.

## LIKE Matching

The existing `SHOW LIKE` decoder is reused. NUL-producing escapes remain a
deliberate MyLite rejection for current NUL-terminated public result and
catalog text handling, matching the existing `SHOW LIKE` policy.

The matcher is invoked with ASCII case-insensitive comparison for charset and
collation catalog names:

- `LIKE 'utf8mb4'` and `LIKE 'UTF8MB4'` match the `utf8mb4` character set;
- broader wildcard patterns match the catalog rows they match in the target
  MySQL 8.4.9 metadata set;
- `LIKE 'utf8mb4\_0900\_ai\_ci'` matches
  `utf8mb4_0900_ai_ci`;
- no-match filters return empty result sets with the normal result metadata.

This matching policy is for the static charset/collation introspection rows
only. It does not add general expression collation semantics.

## Result and Statement State

On success:

- a row-producing result is returned;
- `affected_rows` remains `0` by the existing public result conventions for
  row-producing statements;
- `warning_count == 0`;
- the connection-local previous row-count state is result-set state, so a
  following `SELECT ROW_COUNT()` returns `-1`;
- no user schema, catalog descriptor, physical row, SQLite schema, or file
  preamble state is changed.

Unfiltered MyLite statements return the target MySQL 8.4.9 metadata catalogs.
This remains metadata-only and is documented separately from charset
admission, conversion, comparison, and mutable session/server charset state.

## Diagnostics

The following diagnostics are required:

- public API misuse remains under existing `mylite_execute()` validation;
- parser syntax errors are reported for unsupported grammar, including plural
  forms, non-string `LIKE` patterns, national-string patterns, charset
  introducer patterns, unsupported expressions, and trailing tokens;
- invalid `SHOW LIKE` strings, including NUL-producing escapes, use the
  existing deterministic `SHOW LIKE` diagnostic;
- allocation failures preserve the existing `HY001` out-of-memory diagnostic;
- there are no unknown-schema, missing-default-schema, unknown-table, or
  reserved-name diagnostics because these statements do not resolve user
  schema or table names.

Supported in-range statements produce no warnings.

## SQLite and Storage Handling

This feature is a MyLite-side static result builder. It does not use SQLite
metadata, `sqlite_schema`, PRAGMA output, user physical tables, custom SQLite
functions, virtual tables, or targeted SQLite fork hooks. It does not add
files to the MyLite single-file format and does not change the shifted SQLite
payload invariant.

## Tests

Fast C tests must cover:

- `SHOW CHARACTER SET`, `SHOW CHARSET`, and `SHOW COLLATION`;
- result column names, catalog row counts, and representative row values;
- case-insensitive `LIKE`, wildcard `LIKE`, escaped wildcard `LIKE`, and
  no-match filters;
- documented `WHERE` filters over displayed catalog columns;
- warning count, absence of affected rows beyond result-set conventions, and
  following `ROW_COUNT() == -1`;
- no dependency on selected schema, with and without user schemas/tables;
- no mutation of catalog generation, SQLite schema generation, user rows, or
  the MyLite preamble;
- persistence/reopen and independent handles;
- unsupported plural forms, non-string `LIKE`, national-string `LIKE`, charset
  introducer `LIKE`, and trailing tokens;
- existing lexer, parser, runtime handle, diagnostics, statement context,
  result metadata, SQLite bootstrap policy, file-backed opening, VFS, catalog,
  table option, and SHOW lifecycle tests still pass.

The MySQL expectation artifact must verify MySQL 8.4.9 behavior for accepted
forms, row values, result-state status, case-insensitive `LIKE`, no-match
results, accepted `WHERE`, syntax rejections, and the target catalog counts.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/sql-show-statements.md`,
`docs/compatibility/character-sets.md`, and
`docs/compatibility/collations.md` for this metadata-only introspection
surface. Do not mark charset admission/conversion, string semantics,
collation comparison behavior, `SET NAMES`, mutable system variables, or
direct `mysql` data-dictionary storage as implemented merely because catalog
rows are visible.
