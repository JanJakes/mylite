# Baseline Table Charset and Collation Surface

## Status

This feature specifies a narrow `CREATE TABLE` table-option slice for MyLite's
current persistent base-table lifecycle. MyLite already renders
`ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci` in
descriptor-driven `SHOW CREATE TABLE`; this slice accepts the corresponding
default charset and collation table options on input without adding a full
character-set catalog or string collation semantics.

The feature is intentionally not full MySQL charset or collation support. It
does not store per-table charset/collation metadata, implement string types,
change comparison behavior, expose `SHOW CHARACTER SET`, expose
`SHOW COLLATION`, or add `INFORMATION_SCHEMA` charset/collation tables.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Baseline basic table lifecycle:
  `docs/specs/baseline-basic-table-lifecycle/specs.md`
- Baseline SHOW CREATE TABLE:
  `docs/specs/baseline-show-create-table/specs.md`
- Baseline InnoDB engine surface:
  `docs/specs/baseline-innodb-engine-surface/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- Collation compatibility catalog:
  `docs/compatibility/collations.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, table character set and collation:
  https://dev.mysql.com/doc/refman/8.4/en/charset-table.html
- MySQL 8.4 Reference Manual, character sets and collations:
  https://dev.mysql.com/doc/refman/8.4/en/charset.html
- MySQL 8.4 Reference Manual, `SHOW CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/show-create-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_table_charset_collation_surface_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `CREATE TABLE t (...) DEFAULT CHARSET=utf8mb4` succeeds.
- `CREATE TABLE t (...) DEFAULT CHARACTER SET utf8mb4` succeeds.
- `CREATE TABLE t (...) CHARSET=utf8mb4` succeeds.
- `CREATE TABLE t (...) COLLATE=utf8mb4_0900_ai_ci` succeeds and implies the
  `utf8mb4` table character set.
- `CREATE TABLE t (...) DEFAULT COLLATE=utf8mb4_0900_ai_ci` succeeds.
- Charset and collation option names are accepted as unquoted identifiers,
  backtick-quoted identifiers, single-quoted strings, and double-quoted
  strings under the default SQL mode.
- `=` is optional for admitted charset and collation option forms.
- The options may appear alongside `ENGINE=InnoDB`; MySQL accepts
  `ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci` and also
  accepts the same options in a different order.
- Successful creates with these default options leave warning count `0` and
  make the following `ROW_COUNT()` return `0`.
- `SHOW CREATE TABLE` for the admitted default charset/collation forms renders
  `ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci`.
- `DEFAULT CHARSET=DEFAULT` and `COLLATE=DEFAULT` are syntax errors.
- A nonexistent charset fails with error `1115`, SQLSTATE `42000`, and an
  `Unknown character set` message.
- A nonexistent collation fails with error `1273`, SQLSTATE `HY000`, and an
  `Unknown collation` message.
- A collation that does not belong to an explicitly selected charset fails with
  error `1253`, SQLSTATE `42000`.
- Repeating the same charset option succeeds; conflicting charset declarations
  fail with error `1302`, SQLSTATE `HY000`.

## Scope

The implementation must add:

- parser and AST support for a table-option list after the existing limited
  `CREATE TABLE (...)` column definition list;
- support for the existing `ENGINE [=] InnoDB` option inside that list, so
  engine and charset/collation options can be combined;
- analyzer/runtime validation for the admitted default table character set
  `utf8mb4`;
- analyzer/runtime validation for the admitted default table collation
  `utf8mb4_0900_ai_ci`;
- ASCII case-insensitive matching for admitted charset and collation names;
- acceptance of unquoted identifiers, backtick-quoted identifiers, and string
  literals for admitted charset and collation names;
- acceptance of optional `DEFAULT`, optional `=`, `CHARSET`,
  `CHARACTER SET`, and `COLLATE` syntax for the admitted default options;
- deterministic rejection of non-default charsets and collations with
  MyLite's deliberately limited charset/collation diagnostics;
- tests comparing admitted MySQL-visible behavior with MySQL 8.4.9;
- compatibility documentation for the exact limited surface.

## Non-Goals

This feature must not implement:

- string, text, enum, set, binary, or blob column types;
- per-table charset or collation catalog storage;
- per-column charset/collation options;
- non-`utf8mb4` table character sets;
- non-`utf8mb4_0900_ai_ci` table collations, including valid MySQL collations
  such as `utf8mb4_bin`;
- database-level charset or collation defaults;
- `CREATE DATABASE ... CHARACTER SET`, `ALTER DATABASE`, or `ALTER TABLE`
  charset/collation forms;
- `SHOW CHARACTER SET`, `SHOW COLLATION`, charset/collation
  `INFORMATION_SCHEMA` tables, or `mysql` system charset tables;
- string comparison, collation coercibility, `SET NAMES`, `SET CHARACTER SET`,
  client character-set state, SQL-mode-dependent charset behavior, or warning
  demotion;
- other table options such as row format, comments, tablespaces, statistics,
  encryption, storage, partitioning, temporary tables, views, or engine
  attributes;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` continues to own the
  statement boundary and result handle lifetime.
- Statement context owns diagnostics reset, warning count, affected rows, and
  row-count state.
- Lexer/parser/AST own syntax admission for table options. Parser code remains
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code validates that any admitted charset/collation table
  options are exactly MyLite's fixed baseline defaults before descriptors or
  physical SQLite SQL are generated.
- The catalog remains authoritative for schemas, tables, and columns. This
  slice does not add table charset/collation descriptor fields because the only
  admitted values are the fixed MyLite baseline defaults already emitted by
  `SHOW CREATE TABLE`.
- Runtime execution uses the existing descriptor-driven persistent base-table
  path after option validation. Table options are not interpolated into
  generated SQLite SQL.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload. This
  feature must not write before the shifted SQLite payload.

## Supported SQL Grammar

The feature extends the existing limited `CREATE TABLE` grammar:

```sql
CREATE TABLE table_name (
    column_name integer_type [NULL | NOT NULL]
    [, column_name integer_type [NULL | NOT NULL]] ...
) [table_option ...]

table_option:
    ENGINE [=] engine_name
  | [DEFAULT] CHARSET [=] charset_name
  | [DEFAULT] CHARACTER SET [=] charset_name
  | [DEFAULT] COLLATE [=] collation_name

engine_name:
    identifier
  | quoted_identifier
  | string_literal

charset_name:
    identifier
  | quoted_identifier
  | string_literal

collation_name:
    identifier
  | quoted_identifier
  | string_literal
```

`engine_name` must decode to `InnoDB`, `charset_name` must decode to
`utf8mb4`, and `collation_name` must decode to `utf8mb4_0900_ai_ci`, each
compared ASCII case-insensitively. Options may appear in any order. Repeating
the same admitted fixed-value option is accepted because it has no additional
state in this slice.

MyLite Lemon-syntax snippets:

```lemon
create_table_statement(A) ::=
    CREATE(C) TABLE table_name(T) LPAREN column_definition_list(L) RPAREN(R)
    table_option_list_opt(O). {
    A = mylite_sql_parser_make_create_table_statement(state, C, T, L, R, O);
}

table_option_list_opt(A) ::= . {
    A = NULL;
}
table_option_list_opt(A) ::= table_option_list(B). {
    A = B;
}

table_option_list(A) ::= table_option(B). {
    A = mylite_sql_parser_make_table_option_list(state, B);
}
table_option_list(A) ::= table_option_list(B) table_option(C). {
    A = mylite_sql_parser_append_table_option(state, B, C);
}

table_option(A) ::= ENGINE(E) equal_opt option_name(N). {
    A = mylite_sql_parser_make_table_engine_option(state, E, N);
}
table_option(A) ::= default_opt charset_keyword(K) equal_opt option_name(N). {
    A = mylite_sql_parser_make_table_charset_option(state, K, N);
}
table_option(A) ::= default_opt COLLATE(C) equal_opt option_name(N). {
    A = mylite_sql_parser_make_table_collation_option(state, C, N);
}

default_opt ::= .
default_opt ::= DEFAULT.

charset_keyword ::= CHARSET.
charset_keyword ::= CHARACTER SET.

equal_opt ::= .
equal_opt ::= EQUAL.

option_name(A) ::= identifier(B). {
    A = B;
}
option_name(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
```

Unsupported table options remain syntax errors unless they are admitted and
then rejected by validation. Comma-separated table options, `DEFAULT
CHARSET=DEFAULT`, `COLLATE=DEFAULT`, `CHARACTER SET` without a following name,
and all other wider `CREATE TABLE` syntax remain outside this slice.

## Option Name Decoding

Identifier option names use the existing identifier decoding path. Backtick
doubling is decoded before comparison. String option names use the same
string-literal escape policy as the InnoDB engine-name slice. Raw NUL bytes or
decoded `\0` escapes in option names are rejected with deterministic
unsupported-syntax diagnostics before any C-string comparison is performed.

Decoded charset names other than `utf8mb4` are rejected with error `1115`,
SQLSTATE `42000`, and `Unknown character set: '<name>'`. This is
MyLite-specific for MySQL-supported charsets such as `latin1`; MyLite's
supported charset catalog for this slice contains only the fixed baseline
`utf8mb4` table default.

Decoded collation names other than `utf8mb4_0900_ai_ci` are rejected with
error `1273`, SQLSTATE `HY000`, and `Unknown collation: '<name>'`. This is
MyLite-specific for MySQL-supported collations such as `utf8mb4_bin`; MyLite's
supported collation catalog for this slice contains only the fixed baseline
collation.

## Runtime Semantics

`CREATE TABLE` without table options is unchanged. `CREATE TABLE` with admitted
default charset/collation options follows the existing persistent base-table
path after validation:

- resolve unqualified and schema-qualified target table names through the
  selected/default schema policy;
- reject reserved `_mylite_*` schema, table, and column names before generated
  SQLite SQL;
- validate the optional InnoDB engine, charset, and collation table options;
- validate duplicate table and duplicate column names;
- insert table and column descriptors into MyLite catalog rows;
- create the stable physical SQLite rowid table using the generated
  `_mylite_user_table_<table_id>` name;
- commit or roll back catalog and physical schema changes atomically;
- leave `affected_rows == 0`, `warning_count == 0`, and `ROW_COUNT() == 0` on
  success.

The charset and collation options must not affect physical SQLite SQL
generation. They do not change descriptor storage, integer value storage,
predicate behavior, ordering behavior, or existing row readback because this
slice has no string-bearing column types.

`SHOW CREATE TABLE` remains unchanged and continues to render the fixed suffix
`ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci` for all
current persistent base tables, regardless of whether the create statement
spelled the fixed defaults explicitly or omitted them.

## Diagnostics

This slice must cover:

- syntax errors for unsupported table option forms;
- missing default schema, unknown schema, duplicate table, duplicate columns,
  reserved names, and physical SQLite failures through the existing table
  lifecycle diagnostics;
- unknown or unsupported charset names: error `1115`, SQLSTATE `42000`,
  message `Unknown character set: '<name>'`;
- unknown or unsupported collation names: error `1273`, SQLSTATE `HY000`,
  message `Unknown collation: '<name>'`;
- raw NUL or decoded `\0` in charset/collation names: deterministic
  unsupported-syntax diagnostics;
- allocation failure while building AST nodes, decoded option names, generated
  SQL, or result rows: `HY001` where a handle is available;
- public API misuse unchanged from the existing execution/result API.

Supported in-range behavior produces no warnings. This slice does not implement
warnings, SQL-mode-dependent behavior, or character-set conversion warnings.

## Compatibility Decisions

MyLite exposes a fixed `utf8mb4` / `utf8mb4_0900_ai_ci` table default because
that is the MySQL 8.4 default and already appears in MyLite's
descriptor-driven `SHOW CREATE TABLE` output. Accepting only these values keeps
catalog state lean and prevents MyLite from accepting metadata it cannot yet
preserve or expose accurately.

Because there are no supported string columns in this baseline, the table
charset and collation have no data-conversion or comparison effect. Future
string-type work can add descriptor fields or database defaults with explicit
catalog migrations and compatibility tests.

## SQLite Handling

This feature is MyLite parser/runtime validation work. It uses no SQLite
extension APIs and requires no SQLite fork patches. Existing generated physical
SQLite `CREATE TABLE` remains descriptor-driven and quotes every physical
identifier. Charset and collation names are never interpolated into SQLite SQL.

## Tests

Add a fast C runtime test, preferably
`runtime_table_charset_collation_surface`, and extend parser tests.

Parser tests must cover:

- supported `DEFAULT CHARSET=utf8mb4`;
- supported `DEFAULT CHARACTER SET utf8mb4`;
- supported `CHARSET utf8mb4`;
- supported `COLLATE=utf8mb4_0900_ai_ci`;
- supported `DEFAULT COLLATE=utf8mb4_0900_ai_ci`;
- supported string and backtick option names;
- supported combinations with `ENGINE=InnoDB` and option-order variation;
- unsupported `DEFAULT CHARSET=DEFAULT`, `COLLATE=DEFAULT`, unrelated table
  options, comma-separated options, and incomplete `CHARACTER SET` forms.

Runtime tests must cover:

- successful create forms for each admitted charset/collation spelling;
- combined engine, charset, and collation options in MySQL-observed order and
  in at least one different order;
- string-literal and backtick-quoted option names;
- case-insensitive option names;
- no selected schema failure behavior unchanged;
- unqualified and schema-qualified create targets;
- `SHOW CREATE TABLE` output for explicitly declared default table options;
- row insertion/select persistence through close/reopen for explicitly
  declared default table-option tables;
- unknown charset and unsupported non-default charset diagnostics;
- unknown collation and unsupported non-default collation diagnostics;
- `DEFAULT CHARSET=DEFAULT` and `COLLATE=DEFAULT` syntax errors;
- raw NUL and escaped `\0` diagnostics for charset and collation option names;
- warning count, affected rows, and `ROW_COUNT()` for successful creates;
- preamble preservation and independent file-backed handles.

Run the MySQL expectation script before implementation and after implementation
to keep expected output tied to MySQL 8.4.9.

## Compatibility Documentation

Update `COMPATIBILITY.md` and detailed docs only for this partial surface:

- `CREATE TABLE`: mention optional explicit fixed default charset/collation
  options;
- `Table options: charset/collation`: partial/limited for explicit
  `utf8mb4` / `utf8mb4_0900_ai_ci` only;
- `Default collation selection`: remain unsupported except for accepting the
  fixed baseline table option values.

Do not overclaim full charset/collation catalogs, database defaults, column
charset/collation options, string comparison semantics, client character-set
state, or `INFORMATION_SCHEMA` support.

## Verification

Before marking the feature done:

1. Run the MySQL 8.4.9 expectation script.
2. `cmake --build --preset dev`
3. Run the new CTest entry plus parser, table lifecycle, show-create, InnoDB
   engine surface, row-values, select, delete, update, and truncate lifecycle
   tests.
4. `cmake --workflow --preset check`
5. Review the diff for grammar independence, MySQL 8.4.9 evidence, parser
   scope, option diagnostics, catalog authority, file-format safety, no SQLite
   fork changes, compatibility documentation accuracy, and test relevance.
