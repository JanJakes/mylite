# Baseline SHOW LIKE Filters

## Status

This feature specifies a narrow `LIKE 'pattern'` filter slice for descriptor
introspection statements. It builds on `mylite_execute()`, statement context,
the MyLite parser scaffold, file-backed `.mylite` opening, durable catalog
descriptors, schema/table lifecycle, baseline row storage, and the existing
descriptor-driven `SHOW DATABASES`, `SHOW TABLES`, and `SHOW COLUMNS` result
builders.

The feature is intentionally not general `LIKE` expression support. It admits
only the optional `LIKE 'pattern'` clause on already supported `SHOW`
statements. It does not add `WHERE` filters, `DESCRIBE table wild`,
`EXPLAIN table wild`, `SHOW FULL`, `SHOW EXTENDED`, general `LIKE` predicates,
collation descriptors, `INFORMATION_SCHEMA`, privileges, or optimizer
metadata.

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
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline schema, table, row, query, write, count, and introspection slices:
  `docs/specs/baseline-schema-lifecycle/specs.md`,
  `docs/specs/baseline-basic-table-lifecycle/specs.md`,
  `docs/specs/baseline-table-rename-lifecycle/specs.md`,
  `docs/specs/baseline-row-values-lifecycle/specs.md`,
  `docs/specs/baseline-select-where-lifecycle/specs.md`,
  `docs/specs/baseline-select-order-limit-lifecycle/specs.md`,
  `docs/specs/baseline-delete-lifecycle/specs.md`,
  `docs/specs/baseline-update-lifecycle/specs.md`,
  `docs/specs/baseline-count-aggregate/specs.md`,
  `docs/specs/baseline-show-columns-introspection/specs.md`, and
  `docs/specs/baseline-explain-table-introspection/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `SHOW` statements:
  https://dev.mysql.com/doc/refman/8.4/en/show.html
- MySQL 8.4 Reference Manual, `SHOW DATABASES`:
  https://dev.mysql.com/doc/refman/8.4/en/show-databases.html
- MySQL 8.4 Reference Manual, `SHOW TABLES`:
  https://dev.mysql.com/doc/refman/8.4/en/show-tables.html
- MySQL 8.4 Reference Manual, `SHOW COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-columns.html
- MySQL 8.4 Reference Manual, extensions to `SHOW` statements:
  https://dev.mysql.com/doc/refman/8.4/en/extended-show.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime using TCP:

- `SHOW DATABASES LIKE 'pattern'` and `SHOW SCHEMAS LIKE 'pattern'` filter the
  database list. The result column is `Database (pattern)`, using the decoded
  pattern value.
- `SHOW TABLES LIKE 'pattern'`, `SHOW TABLES FROM db LIKE 'pattern'`, and
  `SHOW TABLES IN db LIKE 'pattern'` filter the table list. The result column
  is `Tables_in_db (pattern)`, using the decoded pattern value.
- `SHOW COLUMNS ... LIKE 'pattern'` and `SHOW FIELDS ... LIKE 'pattern'`
  filter table-column rows but keep the standard six result columns:
  `Field`, `Type`, `Null`, `Key`, `Default`, and `Extra`.
- `%` matches zero or more characters. `_` matches one character.
- A backslash in the decoded pattern escapes `%`, `_`, and backslash for the
  admitted names in this slice. For example, `LIKE 'a\_b'` matches `a_b` but
  not `a%b`, and `LIKE 'a\%b'` matches `a%b` but not `a_b`.
- With `@@lower_case_table_names = 0` in the MySQL 8.4.9 Linux runtime,
  database and table `SHOW ... LIKE` matching is case-sensitive.
- Column `SHOW COLUMNS ... LIKE` matching is case-insensitive in the observed
  runtime for ASCII identifiers: `LIKE 'mixedcase'` matches column
  `MixedCase`.
- Unsupported non-string pattern forms such as `LIKE 1`, `LIKE NULL`, and
  `LIKE N'a%'` are syntax errors.
- MySQL 8.4.9 accepts NUL-producing string escapes such as `LIKE 'a\0%'`.
  This slice deliberately rejects those escapes because current MyLite public
  result names and catalog identifiers are NUL-terminated text. The MySQL
  expectation artifact pins this as a known divergence.
- Name-resolution errors are evaluated before the filter can hide them:
  missing default schema remains `1046` / `3D000`, unknown schemas remain
  `1049` / `42000`, and unknown tables remain `1146` / `42S02`.
- Successful filtered `SHOW` statements return result sets, leave warning
  count `0`, and make the following `ROW_COUNT()` return `-1`.
- MySQL also accepts `WHERE` filters on these `SHOW` statements and accepts
  `DESCRIBE table wild`/`EXPLAIN table wild`; those remain outside this slice.

## Scope

The implementation must add:

- parser support for optional `LIKE string_literal` on supported
  `SHOW DATABASES`/`SHOW SCHEMAS`, `SHOW TABLES`, and
  `SHOW COLUMNS`/`SHOW FIELDS` forms;
- descriptor-driven filtering over catalog schema names, table names, and
  column names;
- MySQL-compatible `%`, `_`, and backslash wildcard handling for this narrow
  pattern subset;
- MySQL-observed result-column names for filtered `SHOW DATABASES` and
  `SHOW TABLES`;
- preserved six-column result shape for filtered `SHOW COLUMNS`;
- deterministic diagnostics for unsupported pattern expressions and unresolved
  names;
- fast C tests and a MySQL 8.4.9 expectation artifact for supported behavior
  and deliberately deferred wider forms.

## Non-Goals

This feature must not implement:

- general `LIKE`, `NOT LIKE`, or `RLIKE` expressions;
- `SHOW ... WHERE` filters;
- `DESCRIBE table column_name`, `DESCRIBE table wild`, `EXPLAIN table
  column_name`, or `EXPLAIN table wild`;
- `SHOW FULL`, `SHOW EXTENDED`, hidden columns, privileges, comments,
  collations, generated invisible primary keys, `INFORMATION_SCHEMA`, views,
  temporary tables, indexes, defaults, generated columns, arbitrary SQLite
  metadata reads, arbitrary SQLite SQL pass-through, or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, statement-boundary row-count state, and failure
  cleanup.
- Statement context owns diagnostics reset, warning count, and statement
  completion. Successful filtered `SHOW` statements are result-set statements
  and therefore store `-1` as the connection-local previous row count.
- Lexer/parser/AST own syntax admission and source spans. They admit only
  string-literal `LIKE` filters on supported `SHOW` forms and stay independent
  of runtime, catalog, storage, and SQLite.
- Runtime owns decoding the admitted string literal into a MyLite-owned pattern
  and applying the small `SHOW LIKE` matcher before appending result rows.
- The catalog module remains authoritative for schema/table/column
  descriptors. This slice reads descriptors but does not mutate catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Result builders own the MySQL-visible column names, including the pattern
  suffix for filtered database and table lists.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Filtered `SHOW` reads catalog rows only and does not touch byte range
  `[0, 4096)`.

## Supported SQL Grammar

Supported subset:

```sql
SHOW {DATABASES | SCHEMAS} [LIKE 'pattern']
SHOW TABLES [{FROM | IN} schema_name] [LIKE 'pattern']
SHOW {COLUMNS | FIELDS} {FROM | IN} table_name
    [{FROM | IN} schema_name] [LIKE 'pattern']
```

`table_name` and `schema_name` use the existing identifier subset. The pattern
must be a single regular string literal token. National string literals,
charset introducers, expressions, functions, parameters, numeric literals,
`NULL`, concatenated strings, and nonliteral patterns are not admitted.

MyLite Lemon-syntax grammar snippets:

```lemon
show_databases_statement ::=
    SHOW show_databases_keyword show_like_clause_opt.

show_tables_statement ::=
    SHOW TABLES show_schema_clause_opt show_like_clause_opt.

show_columns_statement ::=
    SHOW show_columns_keyword show_table_name_keyword table_name
    show_schema_clause_opt show_like_clause_opt.

show_like_clause_opt ::= .
show_like_clause_opt ::= LIKE STRING.
```

The AST may store the `LIKE` string literal as an additional child on the
existing `SHOW` statement nodes. Runtime must distinguish optional schema
identifier children from optional pattern literal children by node kind.

## Pattern Decoding and Matching

The pattern literal is decoded from the MySQL string syntax already tokenized
by the lexer:

- doubled quote delimiters inside the string produce one quote byte;
- `\n`, `\r`, `\t`, `\b`, `\Z`, quoted delimiters, and `\\` follow the
  current MySQL default string escaping behavior;
- `\%` and `\_` preserve the backslash in the decoded pattern so the pattern
  matcher can treat the following wildcard as a literal;
- unknown backslash escapes drop the backslash and keep the following byte,
  matching observed MySQL string literal behavior;
- NUL-producing escapes such as `\0` are outside this slice and are rejected
  deterministically before pattern matching, despite MySQL accepting them.

The matcher operates over current MyLite identifier byte strings. This is
sufficient for the ASCII catalog names admitted by existing tests and
descriptors. `%` matches zero or more bytes and `_` matches exactly one byte.
Backslash escapes the following `%`, `_`, or backslash as a literal. A trailing
backslash in a pattern matches a literal backslash.

Database and table filters are case-sensitive under the current catalog and
observed MySQL runtime policy. Column filters are ASCII case-insensitive to
match observed MySQL behavior for current identifiers. This slice does not add
full collation descriptors or locale-aware case folding.

## Schema, Table, and Column Resolution

Name resolution is unchanged from the existing `SHOW` slices:

- unqualified `SHOW TABLES` resolves through the selected/default schema and
  fails with `1046` / `3D000` when no schema is selected;
- `SHOW TABLES FROM/IN schema` resolves the explicit schema and fails with
  `1049` / `42000` for unknown schemas;
- `SHOW COLUMNS` target resolution follows the existing selected/default,
  schema-qualified, and trailing explicit-schema policy, including the
  trailing-schema-wins rule already verified for `SHOW COLUMNS`;
- unknown table targets fail with `1146` / `42S02` before filtering;
- reserved `_mylite_*` schema or table names are rejected before descriptor
  lookup.

Filters apply only after a catalog descriptor set has been resolved. A filter
that matches no rows returns an empty result set with the same statement result
metadata conventions as other `SHOW` result sets.

## Result and Statement State

On success:

- filtered `SHOW DATABASES` returns one text column named `Database (pattern)`;
- filtered `SHOW TABLES` returns one text column named
  `Tables_in_schema (pattern)`;
- filtered `SHOW COLUMNS` keeps the existing six-column result shape;
- row order remains the existing catalog order for schemas, tables, and
  columns;
- `warning_count` is `0`;
- `affected_rows` remains `0` by existing result conventions for result-set
  statements;
- the connection-local previous row-count state is result-set state, so a
  following `SELECT ROW_COUNT()` returns `-1`;
- no catalog or physical row state is changed.

## Diagnostics

Diagnostics must be deterministic for:

- syntax errors and unsupported `SHOW LIKE` grammar;
- unsupported pattern literal categories;
- NUL-producing pattern escapes;
- missing default schema for unqualified `SHOW TABLES` or `SHOW COLUMNS`;
- unknown explicit schema;
- reserved `_mylite_*` schema or table names;
- unknown table in a known schema;
- unsupported object kind once non-base-table descriptors exist;
- unsupported descriptor column type once wider descriptors exist;
- result allocation failure;
- catalog read failure;
- physical SQLite failure while reading catalog state;
- public API misuse if an existing public validation path is exercised.

MyLite-specific unsupported diagnostics are acceptable for syntax that MySQL
accepts but this slice deliberately defers, provided tests lock the behavior
and compatibility docs do not overclaim support.

## Physical SQLite Handling

No user-table SQLite SQL is generated for this feature. The implementation
uses MyLite catalog APIs to iterate schema, table, and column descriptors, then
filters descriptor names before appending result rows. It must not query
`sqlite_schema`, use SQLite pragma output, inspect physical table SQL, or rely
on SQLite `LIKE` semantics.

The `.mylite` file preamble and shifted SQLite payload invariants are
unchanged. The feature must not require SQLite fork patches or new SQLite
extension points.

## Tests

Add tests covering:

- `SHOW DATABASES LIKE` and `SHOW SCHEMAS LIKE`;
- `SHOW TABLES LIKE`, `SHOW TABLES FROM schema LIKE`, and
  `SHOW TABLES IN schema LIKE`;
- `SHOW COLUMNS`/`SHOW FIELDS` `LIKE` with unqualified, schema-qualified, and
  trailing explicit-schema targets;
- `%`, `_`, escaped `%`, escaped `_`, escaped backslash, empty pattern, exact
  matches, and no-match result sets;
- case-sensitive database/table matching and ASCII case-insensitive column
  matching for the observed MySQL 8.4.9 runtime;
- column names for filtered `SHOW DATABASES` and `SHOW TABLES`, unchanged
  columns for filtered `SHOW COLUMNS`, warning count, affected rows, and
  following `ROW_COUNT() = -1`;
- missing default schema, unknown schema, unknown table, reserved target names,
  and unsupported non-string patterns;
- unsupported `WHERE`, `FULL`, `EXTENDED`, `DESCRIBE table wild`,
  `EXPLAIN table wild`, national string patterns, numeric patterns,
  `NULL` patterns, functions, parameters, and general `LIKE` expressions;
- reopen persistence, table rename/drop behavior, independent handles, and
  preamble preservation;
- existing parser, runtime lifecycle, catalog, storage, VFS, statement-context,
  result, and compatibility tests.

The MySQL expectation artifact must verify supported behavior and
MySQL-accepted-but-deferred wider forms against MySQL 8.4.9. A missing MySQL
8.4.9 runtime is a blocker for changing this user-visible surface.

## Compatibility Documentation

After implementation, update `COMPATIBILITY.md` and
`docs/compatibility/sql-show-statements.md` to mark only `LIKE 'pattern'`
filters on the supported `SHOW DATABASES`, `SHOW TABLES`, and `SHOW COLUMNS`
subsets. Update `docs/compatibility/sql-utility-statements.md` only to keep
`DESCRIBE`/`EXPLAIN` wildcard filters explicitly out of scope. Do not overclaim
`WHERE`, general expression `LIKE`, collations, privileges, views, full
metadata, or `INFORMATION_SCHEMA`.
