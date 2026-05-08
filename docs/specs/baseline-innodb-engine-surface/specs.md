# Baseline InnoDB Engine Surface

## Status

This feature specifies the first narrow storage-engine compatibility surface for
MyLite baseline SQL. MyLite is embedded and does not implement multiple storage
engines. This slice accepts explicit `InnoDB` table-engine syntax for the
existing limited persistent `CREATE TABLE` path and exposes a descriptor-built
`SHOW ENGINES` result containing MyLite's single supported engine row.

The feature is intentionally not full MySQL storage-engine support. It does not
store per-table engine metadata, expose alternative engines, implement engine
plugins, or add `INFORMATION_SCHEMA.ENGINES`.

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
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL 8.4 Reference Manual, `SHOW ENGINES`:
  https://dev.mysql.com/doc/refman/8.4/en/show-engines.html
- MySQL 8.4 Reference Manual, `SHOW CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/show-create-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_innodb_engine_surface_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `SHOW ENGINES` and `SHOW STORAGE ENGINES` return columns `Engine`, `Support`,
  `Comment`, `Transactions`, `XA`, and `Savepoints`.
- The MySQL 8.4.9 `InnoDB` row reports `Support = DEFAULT`, comment
  `Supports transactions, row-level locking, and foreign keys`, and `YES` for
  transactions, XA, and savepoints.
- `SHOW ENGINES` does not accept `LIKE` or `WHERE` clauses.
- `CREATE TABLE t (...) ENGINE=InnoDB` and `CREATE TABLE t (...) ENGINE InnoDB`
  both create InnoDB tables.
- Engine names are case-insensitive for `InnoDB`.
- MySQL 8.4.9 accepts identifier, quoted identifier, single-quoted string, and
  double-quoted string spellings for the engine name while `ANSI_QUOTES` is not
  active.
- `SHOW CREATE TABLE` renders explicit InnoDB tables with
  `ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci`.
- A nonexistent or empty string engine name fails with error `1286`, SQLSTATE
  `42000`, and an `Unknown storage engine` message.

## Scope

The implementation must add:

- parser and AST support for an optional single `ENGINE [=] engine_name` table
  option after the existing limited `CREATE TABLE (...)` column definition
  list;
- analyzer/runtime validation that an admitted engine option names `InnoDB`,
  compared ASCII case-insensitively after unquoting;
- acceptance of unquoted identifiers, backtick-quoted identifiers, and ordinary
  string literals for `InnoDB` engine names;
- deterministic rejection of any non-`InnoDB` engine name with MyLite's
  InnoDB-only storage-engine diagnostic;
- parser and AST support for `SHOW ENGINES` and `SHOW STORAGE ENGINES`;
- a planned result builder for `SHOW ENGINES` with the MySQL 8.4.9 InnoDB row
  shape and values;
- tests comparing the admitted MySQL-visible behavior with MySQL 8.4.9;
- compatibility documentation for the exact limited surface.

## Non-Goals

This feature must not implement:

- multiple storage engines or engine plugin loading;
- MyISAM, MEMORY, CSV, ARCHIVE, BLACKHOLE, FEDERATED, NDB, Performance Schema,
  or other non-InnoDB storage behavior;
- a durable per-table engine descriptor or catalog migration;
- `INFORMATION_SCHEMA.ENGINES`, `SHOW ENGINE`, `SHOW TABLE STATUS`, or
  engine-specific status output;
- `CREATE TABLE` options other than the single optional `ENGINE` option;
- multiple table options, comma-separated table options, charset or collation
  table options, row format, comments, auto-increment options, partitioning,
  temporary tables, views, or engine attributes;
- warnings for unsupported engines, automatic engine substitution, or
  `NO_ENGINE_SUBSTITUTION` SQL mode behavior;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` continues to own the
  statement boundary and result handle lifetime.
- Statement context owns diagnostics reset, warning count, affected rows, and
  row-count state.
- Lexer/parser/AST own syntax admission for the additional table option and
  `SHOW ENGINES` statements. Parser code remains independent of runtime,
  catalog, storage, and SQLite.
- Analyzer/planner code validates that any admitted table engine is MyLite's
  embedded InnoDB surface before table descriptors or physical SQLite SQL are
  generated.
- The catalog remains authoritative for schemas, tables, and columns. This
  slice does not add a catalog engine field because all persistent base tables
  are currently MyLite InnoDB-compatible tables.
- Result builder code owns the `SHOW ENGINES` result shape and row values.
- SQLite owns only the physical rowid table b-tree created by the existing
  descriptor-driven `CREATE TABLE` path. The engine option is not interpolated
  into generated SQLite SQL.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload. This
  feature must not write before the shifted SQLite payload.

## Supported SQL Grammar

The feature extends the existing one-statement execution model.

Supported `CREATE TABLE` extension:

```sql
CREATE TABLE table_name (
    column_name integer_type [NULL | NOT NULL]
    [, column_name integer_type [NULL | NOT NULL]] ...
) [ENGINE [=] engine_name]

engine_name:
    identifier
  | quoted_identifier
  | string_literal
```

`engine_name` must decode to `InnoDB`, compared ASCII case-insensitively. The
engine option may appear at most once and only after the closing column-list
parenthesis.

Supported engine introspection:

```sql
SHOW ENGINES
SHOW STORAGE ENGINES
```

MyLite Lemon-syntax snippets:

```lemon
statement(A) ::= show_engines_statement(B). {
    A = B;
}

create_table_statement(A) ::=
    CREATE(C) TABLE table_name(T) LPAREN column_definition_list(L) RPAREN(R)
    table_engine_option_opt(E). {
    A = mylite_sql_parser_make_create_table_statement(state, C, T, L, R, E);
}

table_engine_option_opt(A) ::= . {
    A = NULL;
}
table_engine_option_opt(A) ::= table_engine_option(B). {
    A = B;
}

table_engine_option(A) ::= ENGINE(E) equal_opt engine_name(N). {
    A = mylite_sql_parser_make_table_engine_option(state, E, N);
}

equal_opt ::= .
equal_opt ::= EQUAL.

engine_name(A) ::= identifier(B). {
    A = B;
}
engine_name(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}

show_engines_statement(A) ::= SHOW(S) ENGINES(E). {
    A = mylite_sql_parser_make_show_engines_statement(state, S, E);
}
show_engines_statement(A) ::= SHOW(S) STORAGE ENGINES(E). {
    A = mylite_sql_parser_make_show_engines_statement(state, S, E);
}
```

Unsupported table options remain syntax errors unless they already fail at the
lexer layer. Unsupported `SHOW ENGINES LIKE ...`, `SHOW ENGINES WHERE ...`,
`SHOW FULL ENGINES`, `SHOW ENGINE ...`, and similar forms remain syntax errors.

## Engine Name Decoding

Identifier engine names use the existing identifier decoding path. Backtick
doubling is decoded before comparison. String engine names use the existing
string literal escape decoding policy used by SHOW LIKE filters. `InnoDB`,
`innodb`, `` `InnoDB` ``, `'InnoDB'`, `'innodb'`, and `"InnoDB"` are accepted
under MyLite's current lexer behavior.

Decoded engine names other than `InnoDB` are rejected with error `1286`,
SQLSTATE `42000`, and `Unknown storage engine '<name>'`. This is
MyLite-specific for engines that the reference MySQL server may support, such
as MyISAM; it is the explicit embedded InnoDB-only compatibility decision for
this slice.

Malformed identifiers or unsupported string decoding failures use existing
parser/runtime diagnostics. `ENGINE=DEFAULT` remains a syntax error in this
slice, matching observed MySQL 8.4.9 behavior for this form.

## Runtime Semantics

`CREATE TABLE` without an engine option is unchanged. `CREATE TABLE` with
`ENGINE [=] InnoDB` follows the exact existing persistent base-table path:

- resolve unqualified and schema-qualified target table names through the
  selected/default schema policy;
- reject reserved `_mylite_*` schema, table, and column names before generated
  SQLite SQL;
- validate duplicate table and duplicate column names;
- insert table and column descriptors into MyLite catalog rows;
- create the stable physical SQLite rowid table using the generated
  `_mylite_user_table_<table_id>` name;
- commit or roll back catalog and physical schema changes atomically;
- leave `affected_rows == 0`, `warning_count == 0`, and `ROW_COUNT() == 0` on
  success.

The engine option must not affect physical SQLite SQL generation. The generated
SQLite table remains a MyLite-owned physical rowid table with integer storage
for the currently supported column descriptors.

`SHOW ENGINES` builds a new row-result object without reading or mutating the
catalog. Successful output:

| Column | Value |
| --- | --- |
| `Engine` | `InnoDB` |
| `Support` | `DEFAULT` |
| `Comment` | `Supports transactions, row-level locking, and foreign keys` |
| `Transactions` | `YES` |
| `XA` | `YES` |
| `Savepoints` | `YES` |

The result has one row, `affected_rows == 0`, `warning_count == 0`, and
`ROW_COUNT() == -1` through the existing row-result convention.

## Diagnostics

This slice must cover:

- syntax errors for unsupported `CREATE TABLE` options and unsupported
  `SHOW ENGINES` modifiers;
- missing default schema, unknown schema, duplicate table, duplicate columns,
  reserved names, and physical SQLite failures through the existing table
  lifecycle diagnostics;
- unknown or unsupported storage engine names: error `1286`, SQLSTATE `42000`,
  message `Unknown storage engine '<name>'`;
- allocation failure while building AST nodes, decoded engine names, generated
  SQL, or result rows: `HY001` where a handle is available;
- public API misuse unchanged from the existing execution/result API.

Supported in-range behavior produces no warnings. This slice does not implement
engine-substitution warnings or SQL-mode-dependent warning/error demotion.

## Compatibility Decisions

MyLite exposes only an InnoDB-compatible embedded storage surface. It does not
claim to implement InnoDB internals, locks, MVCC, crash recovery, foreign keys,
or plugin behavior in this slice. The `SHOW ENGINES` row uses the MySQL 8.4.9
InnoDB row text because applications commonly use this statement to detect
transactional default-engine availability. Compatibility documentation must use
partial wording and must not claim full storage-engine support.

The fixed `SHOW CREATE TABLE` suffix from the previous slice remains the only
engine metadata exposed for tables. Because all MyLite persistent base tables
currently share the same embedded InnoDB-compatible surface, storing an engine
column in `_mylite_catalog_tables` would add migration weight without adding
state. A future multi-engine or compatibility-placeholder slice can add a
catalog field with an explicit migration.

## SQLite Handling

This feature is MyLite parser/runtime/result-builder work. It uses no SQLite
extension APIs and requires no SQLite fork patches. The existing generated
physical SQLite `CREATE TABLE` remains descriptor-driven and quotes every
physical identifier. Engine names are never interpolated into SQLite SQL.

## Tests

Add a fast C runtime test, preferably `runtime_innodb_engine_surface`, and
extend parser tests.

Parser tests must cover:

- supported `CREATE TABLE ... ENGINE=InnoDB`;
- supported `CREATE TABLE ... ENGINE InnoDB`;
- supported identifier, quoted identifier, and string engine names;
- supported `SHOW ENGINES` and `SHOW STORAGE ENGINES`;
- unsupported multiple or unrelated table options;
- unsupported `SHOW ENGINES LIKE`, `SHOW ENGINES WHERE`, `SHOW FULL ENGINES`,
  and `SHOW ENGINE ...` forms.

Runtime tests must cover:

- successful explicit InnoDB creation with no selected schema failure behavior
  unchanged;
- unqualified and schema-qualified create targets;
- `ENGINE=InnoDB`, `ENGINE InnoDB`, case-insensitive names, backtick-quoted
  names, and string-literal names;
- `SHOW CREATE TABLE` output for explicitly declared InnoDB tables;
- row insertion/select persistence through close/reopen for explicitly
  declared InnoDB tables;
- unknown engine diagnostics for arbitrary names and for MySQL-supported but
  MyLite-unsupported engines such as `MyISAM`;
- `SHOW ENGINES` and `SHOW STORAGE ENGINES` result columns, row values,
  warning count, affected rows, and `ROW_COUNT()`;
- unsupported syntax diagnostics for `SHOW ENGINES` modifiers and unsupported
  table options;
- preamble preservation and independent file-backed handles.

Run the MySQL expectation script before implementation and after implementation
to keep expected output tied to MySQL 8.4.9.

## Compatibility Documentation

Update `COMPATIBILITY.md` and detailed docs only for this partial surface:

- `InnoDB-only engine surface`: partial/limited;
- `Table engine options`: partial/limited for explicit `InnoDB` only;
- `SHOW ENGINES`: partial/limited one-row InnoDB result.

Do not overclaim full storage-engine plugins, alternative engines,
`INFORMATION_SCHEMA.ENGINES`, `SHOW ENGINE`, `SHOW TABLE STATUS`, table option
families, or SQL-mode-dependent engine substitution.

## Verification

Before marking the feature done:

1. Run the MySQL 8.4.9 expectation script.
2. `cmake --build --preset dev`
3. Run the new CTest entry plus parser and existing table lifecycle,
   show-create, row-values, select, delete, update, and truncate lifecycle
   tests.
4. `cmake --workflow --preset check`
5. Review the diff for grammar independence, MySQL 8.4.9 evidence, parser
   scope, engine diagnostics, result metadata, catalog authority, file-format
   safety, no SQLite fork changes, compatibility documentation accuracy, and
   test relevance.
