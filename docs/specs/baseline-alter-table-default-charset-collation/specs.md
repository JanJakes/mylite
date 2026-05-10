# Baseline ALTER TABLE Default Charset And Collation

## Status

This feature specifies a narrow `ALTER TABLE` table-option slice for MyLite's
current persistent base-table descriptors:

- `ALTER TABLE table_name [DEFAULT] CHARSET [=] utf8mb4`
- `ALTER TABLE table_name [DEFAULT] CHARACTER SET [=] utf8mb4`
- `ALTER TABLE table_name [DEFAULT] COLLATE [=] utf8mb4_0900_ai_ci`

The admitted options are fixed metadata no-ops. MyLite already exposes the
same default table charset and collation in descriptor-driven
`SHOW CREATE TABLE`; this slice accepts the corresponding MySQL syntax for
existing persistent base tables without adding string storage, mutable table
charset metadata, or collation semantics.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Baseline table charset and collation surface:
  `docs/specs/baseline-table-charset-collation-surface/specs.md`
- Existing table lifecycle, alter-table, row-values, DML, and introspection
  specs under `docs/specs/`
- MySQL lexer and parser scaffold specs:
  `docs/specs/mysql-lexer/specs.md`,
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/alter-table.html
- MySQL 8.4 Reference Manual, `CREATE TABLE` table options:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, character sets and collations:
  https://dev.mysql.com/doc/refman/8.4/en/charset.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_alter_table_default_charset_collation_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `ALTER TABLE t DEFAULT CHARSET=utf8mb4` succeeds.
- `ALTER TABLE t DEFAULT CHARACTER SET utf8mb4` succeeds.
- `ALTER TABLE t CHARACTER SET=utf8mb4` succeeds.
- `ALTER TABLE t CHARSET utf8mb4` succeeds.
- `ALTER TABLE t COLLATE=utf8mb4_0900_ai_ci` succeeds and leaves the table
  rendered as `DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci`.
- `ALTER TABLE t DEFAULT COLLATE=utf8mb4_0900_ai_ci` succeeds.
- Charset and collation option names are accepted as unquoted identifiers,
  backtick-quoted identifiers, single-quoted strings, and double-quoted
  strings under the default SQL mode.
- `=` is optional for the admitted charset and collation option forms.
- Repeating the same fixed charset option succeeds.
- Successful statements leave warning count `0`, error count `0`, and make the
  following `ROW_COUNT()` return `0`.
- Existing row data remains readable after the accepted statements.
- Schema-qualified targets work without a selected default database.
- Unqualified targets without a selected database fail with error `1046`,
  SQLSTATE `3D000`.
- Unknown explicit schemas fail with error `1049`, SQLSTATE `42000`.
- Unknown tables fail with error `1146`, SQLSTATE `42S02`.
- `DEFAULT CHARSET=DEFAULT`, `CHARACTER SET DEFAULT`, and `COLLATE=DEFAULT`
  are syntax errors.
- A nonexistent charset fails with error `1115`, SQLSTATE `42000`, and an
  `Unknown character set` message.
- A nonexistent collation fails with error `1273`, SQLSTATE `HY000`, and an
  `Unknown collation` message.
- A collation that does not belong to an explicitly selected charset fails with
  error `1253`, SQLSTATE `42000`.
- Conflicting charset declarations in the same statement fail with error
  `1302`, SQLSTATE `HY000`.

## Scope

The implementation must add:

- parser and AST support for one `ALTER TABLE` action containing one or more
  fixed table default charset/collation options;
- unqualified and schema-qualified target table resolution through the
  existing selected/default schema policy;
- reserved `_mylite_*` target-name rejection before any physical SQL is
  generated;
- persistent base-table descriptor validation through the existing catalog
  ownership boundary;
- analyzer/runtime validation for the admitted default table character set
  `utf8mb4`;
- analyzer/runtime validation for the admitted default table collation
  `utf8mb4_0900_ai_ci`;
- ASCII case-insensitive matching for admitted charset and collation names;
- acceptance of unquoted identifiers, backtick-quoted identifiers, and string
  literals for admitted names;
- acceptance of optional `DEFAULT`, optional `=`, `CHARSET`,
  `CHARACTER SET`, and `COLLATE` syntax for the admitted default options;
- deterministic rejection of non-default charsets and collations using the
  existing limited charset/collation diagnostics;
- successful non-row statement result reporting with `affected_rows == 0` and
  `warning_count == 0`;
- tests comparing admitted MySQL-visible behavior with MySQL 8.4.9;
- compatibility documentation for the exact limited surface.

## Non-Goals

This feature must not implement:

- string, text, enum, set, binary, or blob column types;
- per-table charset or collation descriptor storage;
- per-column charset/collation options;
- non-`utf8mb4` table character sets;
- non-`utf8mb4_0900_ai_ci` table collations, including valid MySQL collations
  such as `utf8mb4_bin`;
- database-level charset or collation defaults;
- `ALTER DATABASE`, `CREATE DATABASE ... CHARACTER SET`, or mutable schema
  defaults;
- `ALTER TABLE ... CONVERT TO CHARACTER SET`;
- `ALTER TABLE ... ENGINE`, `FORCE`, `ORDER BY`, `ALGORITHM`, `LOCK`,
  `DISCARD TABLESPACE`, `IMPORT TABLESPACE`, partitioning, comments, storage
  options, row-format options, statistics options, or multi-action ALTER;
- `SHOW CHARACTER SET`, `SHOW COLLATION`, charset/collation
  `INFORMATION_SCHEMA` tables, or `mysql` system charset tables beyond the
  already supported static surfaces;
- string comparison, collation coercibility, client character-set state,
  SQL-mode-dependent charset behavior, privileges, metadata locks, binary
  logging, or implicit commit behavior;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` continues to own the
  statement boundary and result handle lifetime.
- Statement context owns diagnostics reset, warning count, affected rows, and
  row-count state.
- Lexer/parser/AST own syntax admission for this single ALTER action. Parser
  code remains independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code validates that admitted options are exactly MyLite's
  fixed baseline defaults after table resolution and before any result is
  returned.
- The catalog remains authoritative for schemas, tables, object kinds, and
  descriptors. This slice does not add charset/collation descriptor fields
  because the only admitted values are the fixed baseline defaults already
  emitted by `SHOW CREATE TABLE`.
- Runtime execution is catalog-driven and does not use SQLite schema text to
  decide whether the target table exists or what kind of object it is.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload. This
  feature must not write before the shifted SQLite payload.
- SQLite owns physical row storage. Successful statements do not require user
  table SQL, physical rebuilds, indexes, triggers, or fork hooks.

## Supported SQL Grammar

The feature extends the existing limited `ALTER TABLE` grammar with one
single-action form:

```sql
ALTER TABLE table_name alter_table_default_charset_collation_option ...

alter_table_default_charset_collation_option:
    [DEFAULT] CHARSET [=] option_name
  | [DEFAULT] CHARACTER SET [=] option_name
  | [DEFAULT] COLLATE [=] option_name

option_name:
    identifier
  | quoted_identifier
  | string_literal
```

`table_name` may be unqualified or schema-qualified. `option_name` must decode
without NUL bytes. Charset names must compare ASCII case-insensitively equal
to `utf8mb4`; collation names must compare ASCII case-insensitively equal to
`utf8mb4_0900_ai_ci`.

Options may repeat when they resolve to the same fixed value. MyLite rejects a
statement with mixed charset declarations as an unsupported wider form rather
than attempting partial MySQL conflict detection beyond the fixed admitted
values.

MyLite Lemon-syntax snippets:

```lemon
statement(A) ::= alter_table_default_charset_collation_statement(B). {
    A = B;
}

alter_table_default_charset_collation_statement(A) ::=
    ALTER(A1) TABLE table_name(T) alter_table_default_charset_collation_option_list(O). {
    A = mylite_sql_parser_make_alter_table_default_charset_collation_statement(
        state, A1, T, O);
}

alter_table_default_charset_collation_option_list(A) ::=
    alter_table_default_charset_collation_option(B). {
    A = mylite_sql_parser_make_table_option_list(state, B);
}
alter_table_default_charset_collation_option_list(A) ::=
    alter_table_default_charset_collation_option_list(B)
    alter_table_default_charset_collation_option(C). {
    A = mylite_sql_parser_append_table_option(state, B, C);
}

alter_table_default_charset_collation_option(A) ::=
    default_opt CHARSET(C) equal_opt option_name(N). {
    A = mylite_sql_parser_make_table_charset_option(state, C, N);
}
alter_table_default_charset_collation_option(A) ::=
    default_opt CHARACTER(C) SET equal_opt option_name(N). {
    A = mylite_sql_parser_make_table_charset_option(state, C, N);
}
alter_table_default_charset_collation_option(A) ::=
    default_opt COLLATE(C) equal_opt option_name(N). {
    A = mylite_sql_parser_make_table_collation_option(state, C, N);
}
```

The grammar intentionally excludes comma-separated alter actions, `ENGINE`,
`FORCE`, `CONVERT TO CHARACTER SET`, `DEFAULT CHARSET=DEFAULT`, and
`COLLATE=DEFAULT`.

## Resolution Semantics

Unqualified table names require the currently selected schema. Schema-qualified
table names use the explicit schema and do not require a selected schema.
Missing default schema, unknown schema, and unknown table diagnostics must
match the existing table lifecycle policy for supported `ALTER TABLE` slices.

Target schemas and tables with reserved `_mylite_*` names are rejected before
catalog lookup or physical SQL generation. Only persistent base-table
descriptors are supported. Once non-base object descriptors exist, this
statement must reject them with the same deterministic unsupported-object
diagnostic used by adjacent ALTER slices.

Descriptor catalog identifier matching follows MyLite's current
case-insensitive catalog name policy. SQLite schema text is not consulted.

## Runtime Semantics

For the admitted fixed options, the statement is a metadata no-op:

- no catalog row is inserted, updated, or deleted;
- table descriptor version, catalog generation, and SQLite schema generation
  do not change;
- physical rows remain untouched;
- `SHOW CREATE TABLE` continues to render
  `ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci`;
- existing supported `SELECT`, `INSERT`, `UPDATE`, `DELETE`, `REPLACE`,
  `TRUNCATE`, and `ALTER TABLE` behavior is unchanged.

The successful public result is the existing non-row statement result:

- zero result columns;
- zero result rows;
- `affected_rows == 0`;
- `warning_count == 0`;
- the following `ROW_COUNT()` returns `0`.

## Diagnostics

The implementation must cover:

- syntax errors for missing option names, `DEFAULT` as an option name,
  comma-separated actions, `CONVERT TO CHARACTER SET`, `ENGINE`, `FORCE`,
  `ORDER BY`, `ALGORITHM`, `LOCK`, and other wider ALTER forms;
- missing default schema;
- unknown explicit schema;
- unknown target table;
- reserved target schema/table names;
- unsupported object kind once such descriptors exist;
- unsupported charset names;
- unsupported collation names;
- NUL bytes in decoded option names;
- allocation failures;
- physical SQLite failures while opening or reading catalog state;
- public API misuse through the existing public execution/result conventions.

Where MyLite intentionally narrows MySQL's larger charset/collation catalog,
diagnostics may use the existing MyLite-specific limited charset/collation
messages. Supported in-range fixed-default statements must not emit warnings.

## SQLite And Storage Handling

This slice uses MyLite wrapper/translation code and public SQLite APIs only.
It does not require a SQLite extension point or fork patch. It should not
generate SQLite DDL for user tables and should not rebuild physical tables.

File-backed handles must preserve the `.mylite` preamble and shifted SQLite
payload invariants. Independent file-backed handles must keep independent row
state; the fixed no-op ALTER must not leak state between handles.

## Tests

Add a focused plain C runtime test, or extend the table charset/collation test
only if that remains clearer, covering:

- parser acceptance for the admitted fixed forms;
- parser rejection for `DEFAULT`, comma-separated actions, `ENGINE`, `FORCE`,
  `CONVERT TO CHARACTER SET`, `ALGORITHM`, `LOCK`, and missing option names;
- successful full fixed-option ALTER over persistent base tables;
- unqualified and schema-qualified target resolution;
- missing default schema, unknown schema, unknown table, and reserved names;
- accepted unquoted, quoted, single-quoted, double-quoted, and uppercase names;
- repeated same fixed charset/collation options;
- unsupported charset and collation diagnostics;
- NUL-containing option-name diagnostics;
- result shape, affected rows, warning count, and following `ROW_COUNT()`;
- unchanged row data after ALTER;
- reopen persistence and unchanged `SHOW CREATE TABLE`;
- table rename/drop interactions where applicable;
- preamble preservation for file-backed databases;
- independent file-backed handles;
- zero-initialized cleanup for any new AST/runtime objects;
- no regressions in existing parser, runtime table lifecycle, table
  charset/collation, SHOW CREATE TABLE, row values, DML lifecycle, file-backed
  opening, VFS, catalog foundation, diagnostics, and statement-context tests.

Add
`packages/libmylite/tests/mysql_baseline_alter_table_default_charset_collation_expectations.sh`
with MySQL 8.4.9 runtime observations for every user-visible behavior
introduced by this slice.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/sql-table-ddl.md`,
`docs/compatibility/character-sets.md`, and
`docs/compatibility/collations.md` only for the exact fixed no-op
`ALTER TABLE` table-default charset/collation subset. Do not claim full
charset/catalog/collation support, conversion, string semantics, or general
ALTER table-option support.
