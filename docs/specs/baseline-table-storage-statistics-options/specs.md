# Baseline Table Storage And Statistics Options

## Status

This feature adds a narrow descriptor-owned `CREATE TABLE` table-option surface
for storage and optimizer-statistics metadata that applications commonly leave
in MySQL DDL. It is not a storage-engine implementation. MyLite continues to use
SQLite rowid tables for physical storage and records only the MySQL-visible
metadata needed by descriptor-driven introspection.

## Compatibility Authority

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline InnoDB engine surface:
  `docs/specs/baseline-innodb-engine-surface/specs.md`
- Baseline table comment option:
  `docs/specs/baseline-create-table-comment-option/specs.md`
- Baseline SHOW CREATE TABLE:
  `docs/specs/baseline-show-create-table/specs.md`
- Baseline SHOW TABLE STATUS metadata:
  `docs/specs/baseline-show-table-status-metadata/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `CREATE TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
- MySQL 8.4 Reference Manual, `SHOW CREATE TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-create-table.html>
- MySQL 8.4 Reference Manual, `SHOW TABLE STATUS`:
  <https://dev.mysql.com/doc/refman/8.4/en/show-table-status.html>
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TABLES`:
  <https://dev.mysql.com/doc/refman/8.4/en/information-schema-tables-table.html>
- Observed MySQL 8.4.9 runtime behavior recorded in
  `packages/libmylite/tests/mysql_baseline_table_storage_statistics_options_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

Runtime probes against local container `mylite-mysql-849` observed:

- `ROW_FORMAT=DYNAMIC`, `COMPACT`, `REDUNDANT`, and `COMPRESSED` are accepted
  for persistent `InnoDB` tables and render in `SHOW CREATE TABLE`.
- `ROW_FORMAT=DEFAULT` is accepted and omitted from `SHOW CREATE TABLE` and
  `CREATE_OPTIONS`; `SHOW TABLE STATUS.Row_format` reports `Dynamic`.
- `ROW_FORMAT=FIXED` fails under default `innodb_strict_mode = 1` with error
  `1031 / HY000` and a message containing
  `Table storage engine for '<table>' doesn't have this option`.
- `KEY_BLOCK_SIZE=1`, `2`, `4`, `8`, and `16` are accepted and render as
  `KEY_BLOCK_SIZE=N`. Positive key-block size makes
  `SHOW TABLE STATUS.Row_format` and `INFORMATION_SCHEMA.TABLES.ROW_FORMAT`
  report `Compressed` when no explicit compatible row format is present.
- `KEY_BLOCK_SIZE=0` is accepted and omitted.
- invalid positive key-block sizes fail with `1031 / HY000`.
- `KEY_BLOCK_SIZE` combined with `ROW_FORMAT=DYNAMIC` or `ROW_FORMAT=COMPACT`
  fails with `1031 / HY000`.
- `PACK_KEYS=0` and `PACK_KEYS=1` are accepted and rendered; `PACK_KEYS=DEFAULT`
  is accepted and omitted.
- `CHECKSUM=0` is accepted and omitted; nonzero integer values such as
  `CHECKSUM=2` are accepted and normalize to `CHECKSUM=1` / `checksum=1`.
- `STATS_PERSISTENT=0|1`, `STATS_AUTO_RECALC=0|1`, and
  `STATS_SAMPLE_PAGES=1..65535` are accepted and rendered. Their `DEFAULT`
  forms are accepted and omitted.
- `STATS_SAMPLE_PAGES=0` fails with `1064 / 42000` and a message containing
  `valid range for stats_sample_pages`.
- If duplicate supported options occur, the last effective value wins. Later
  `DEFAULT` values clear the rendered option.
- MySQL accepts comma-separated table options as well as whitespace-separated
  options.
- `CREATE TABLE ... LIKE` copies the effective rendered option metadata.
- `CREATE TEMPORARY TABLE` accepts most metadata options, but rejects
  `ROW_FORMAT=COMPRESSED` and positive `KEY_BLOCK_SIZE` with `3500 / HY000`.
  MyLite keeps temporary-table storage-option support outside this slice except
  where existing table-option behavior is unaffected.

## Supported Surface

This slice admits the following options on explicit-column persistent
`CREATE TABLE` statements that already fit the current table-definition subset:

```sql
CREATE TABLE [IF NOT EXISTS] table_name (
    create_table_item [, create_table_item] ...
)
    [existing_supported_table_option ...]
    [ROW_FORMAT [=] {DEFAULT | DYNAMIC | COMPACT | REDUNDANT | COMPRESSED}]
    [KEY_BLOCK_SIZE [=] nonnegative_integer_literal]
    [PACK_KEYS [=] {DEFAULT | 0 | 1}]
    [CHECKSUM [=] nonnegative_integer_literal]
    [STATS_PERSISTENT [=] {DEFAULT | 0 | 1}]
    [STATS_AUTO_RECALC [=] {DEFAULT | 0 | 1}]
    [STATS_SAMPLE_PAGES [=] {DEFAULT | positive_integer_literal}]
    [existing_supported_table_option ...]
```

Supported table options may be separated by whitespace or a comma. Existing
supported table options include `ENGINE=InnoDB`, table character set/collation,
`AUTO_INCREMENT=N`, and `COMMENT='text'`.

`CREATE TABLE ... LIKE source` copies the source table's currently supported
storage/statistics option metadata into the new table descriptor and resets only
the existing auto-increment counter behavior.

## MyLite Lemon Syntax

The grammar extension is independently authored for MyLite:

```lemon
table_option_list(A) ::= table_option(B). {
    A = mylite_sql_parser_make_table_option_list(state, B);
}
table_option_list(A) ::= table_option_list(B) table_option(C). {
    A = mylite_sql_parser_append_table_option(state, B, C);
}
table_option_list(A) ::= table_option_list(B) COMMA table_option(C). {
    A = mylite_sql_parser_append_table_option(state, B, C);
}

table_option(A) ::= ROW_FORMAT(T) equal_opt row_format_value(V). {
    A = mylite_sql_parser_make_table_row_format_option(state, T, V);
}
table_option(A) ::= KEY_BLOCK_SIZE(T) equal_opt INTEGER(V). {
    A = mylite_sql_parser_make_table_key_block_size_option(state, T,
        mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_INTEGER));
}
table_option(A) ::= PACK_KEYS(T) equal_opt table_default_or_bool_value(V). {
    A = mylite_sql_parser_make_table_pack_keys_option(state, T, V);
}
table_option(A) ::= CHECKSUM(T) equal_opt INTEGER(V). {
    A = mylite_sql_parser_make_table_checksum_option(state, T,
        mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_INTEGER));
}
table_option(A) ::= STATS_PERSISTENT(T) equal_opt table_default_or_bool_value(V). {
    A = mylite_sql_parser_make_table_stats_persistent_option(state, T, V);
}
table_option(A) ::= STATS_AUTO_RECALC(T) equal_opt table_default_or_bool_value(V). {
    A = mylite_sql_parser_make_table_stats_auto_recalc_option(state, T, V);
}
table_option(A) ::= STATS_SAMPLE_PAGES(T) equal_opt table_default_or_integer_value(V). {
    A = mylite_sql_parser_make_table_stats_sample_pages_option(state, T, V);
}

row_format_value(A) ::= DEFAULT(T). { A = mylite_sql_parser_make_identifier(state, T); }
row_format_value(A) ::= DYNAMIC(T). { A = mylite_sql_parser_make_identifier(state, T); }
row_format_value(A) ::= COMPACT(T). { A = mylite_sql_parser_make_identifier(state, T); }
row_format_value(A) ::= REDUNDANT(T). { A = mylite_sql_parser_make_identifier(state, T); }
row_format_value(A) ::= COMPRESSED(T). { A = mylite_sql_parser_make_identifier(state, T); }
row_format_value(A) ::= FIXED(T). { A = mylite_sql_parser_make_identifier(state, T); }

table_default_or_bool_value(A) ::= DEFAULT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
table_default_or_bool_value(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}

table_default_or_integer_value(A) ::= DEFAULT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
table_default_or_integer_value(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
```

No signed, decimal, hexadecimal, bit, string, identifier-name, expression,
parameter, or subquery option values are admitted in this slice. `ROW_FORMAT`
string literals remain syntax errors.

## Ownership And Architecture

- Public API: unchanged. Successful `CREATE TABLE` continues to return through
  existing non-row statement result conventions.
- Statement context: owns diagnostics, warning count, affected rows, and
  `ROW_COUNT()` state. Supported successful option metadata writes report
  `affected_rows == 0` and `warning_count == 0`.
- Lexer/parser/AST: own syntax admission and option nodes. Parser code remains
  independent of catalog, runtime, storage, and SQLite.
- Runtime planner: validates option values, normalizes duplicate/`DEFAULT`
  semantics, rejects unsupported combinations, and stores the final effective
  metadata in `planned_create_table`.
- Catalog: owns durable table-option descriptor fields for persistent base
  tables. Existing table descriptors read default empty/default values after
  migration.
- Temporary catalog: can carry option fields in the shared table descriptor
  structure, but this slice does not claim complete temporary-table option
  parity.
- Metadata renderers: `SHOW CREATE TABLE`, `SHOW TABLE STATUS`, and
  `INFORMATION_SCHEMA.TABLES` render from MyLite descriptors, not SQLite schema
  text.
- SQLite physical storage: unchanged. Generated SQLite DDL continues to create
  MyLite-owned rowid tables with stable physical table names. Table option text
  is never interpolated into SQLite SQL.
- Storage/VFS: unchanged. `.mylite` preamble and shifted SQLite payload
  invariants must remain intact.

## Descriptor Semantics

Stored table descriptor fields:

- `row_format_option`: empty for default, otherwise one of `DYNAMIC`,
  `COMPACT`, `REDUNDANT`, or `COMPRESSED`;
- `key_block_size`: `0`, `1`, `2`, `4`, `8`, or `16`;
- `pack_keys`: `-1` for default/omitted, otherwise `0` or `1`;
- `checksum`: `0` or `1`, with any admitted nonzero input normalized to `1`;
- `stats_persistent`: `-1` for default/omitted, otherwise `0` or `1`;
- `stats_auto_recalc`: `-1` for default/omitted, otherwise `0` or `1`;
- `stats_sample_pages`: `0` for default/omitted, otherwise `1..65535`.

Duplicate options are evaluated left to right. The final effective value is
stored. A final `DEFAULT` value clears the corresponding option.

`KEY_BLOCK_SIZE > 0` is allowed alone or with `ROW_FORMAT=COMPRESSED`; it is
rejected with `ROW_FORMAT=DYNAMIC`, `COMPACT`, or `REDUNDANT` in this slice.
`ROW_FORMAT=DEFAULT` clears the explicit row-format option and may coexist with
`KEY_BLOCK_SIZE`.

`ROW_FORMAT=FIXED` is parsed so the runtime can return a MySQL-shaped storage
engine diagnostic instead of a generic syntax error.

## Metadata Rendering

`SHOW CREATE TABLE` appends supported table options after the existing
`ENGINE`, `AUTO_INCREMENT`, charset/collation, and `COMMENT` suffixes in the
verified MySQL option order:

1. `PACK_KEYS=0|1`
2. `STATS_PERSISTENT=0|1`
3. `STATS_AUTO_RECALC=0|1`
4. `STATS_SAMPLE_PAGES=N`
5. `CHECKSUM=1`
6. `ROW_FORMAT=...`
7. `KEY_BLOCK_SIZE=N`

Default/omitted values are not rendered.

`SHOW TABLE STATUS.Row_format` and `INFORMATION_SCHEMA.TABLES.ROW_FORMAT`
render:

- `Compressed` when `row_format_option = COMPRESSED` or no explicit row format
  is stored and `key_block_size > 0`;
- `Dynamic`, `Compact`, or `Redundant` for matching explicit row formats;
- `Dynamic` for default/omitted values.

`SHOW TABLE STATUS.Create_options` and
`INFORMATION_SCHEMA.TABLES.CREATE_OPTIONS` render a space-separated string in
the verified MySQL order:

1. `row_format=...`
2. `stats_sample_pages=N`
3. `stats_auto_recalc=0|1`
4. `KEY_BLOCK_SIZE=N`
5. `stats_persistent=0|1`
6. `pack_keys=0|1`
7. `checksum=1`

Default/omitted values are not rendered. The `CHECKSUM` metadata column remains
`NULL`, matching current MySQL 8.4.9 observations for these InnoDB tables.

## Diagnostics

Supported successful forms report no warnings.

Diagnostics for this slice:

- unsupported or unknown storage option AST shape: existing parse error;
- `ROW_FORMAT=FIXED`: `1031 / HY000`, message contains
  `Table storage engine for '<table>' doesn't have this option`;
- invalid `KEY_BLOCK_SIZE` values outside `0`, `1`, `2`, `4`, `8`, and `16`:
  `1031 / HY000`, same storage-engine option message;
- incompatible `KEY_BLOCK_SIZE > 0` with non-compressed explicit row formats:
  `1031 / HY000`, same storage-engine option message;
- `PACK_KEYS`, `STATS_PERSISTENT`, or `STATS_AUTO_RECALC` values other than
  `DEFAULT`, `0`, or `1`: syntax-shaped `1064 / 42000` parse diagnostic;
- `STATS_SAMPLE_PAGES=0` or `>65535`: syntax-shaped `1064 / 42000` diagnostic
  containing `valid range for stats_sample_pages`;
- string, decimal, signed, hex, bit, expression, function, parameter, or
  subquery option values: syntax error or current unsupported-expression
  diagnostic;
- unsupported temporary-table combinations may use deterministic MyLite
  diagnostics until a full temporary-table option slice is specified;
- allocation, catalog, or SQLite physical failures use existing MyLite
  internal/runtime diagnostics.

## Tests

Tests must cover:

- parser acceptance for each supported option, whitespace-separated and
  comma-separated option lists, and duplicate options;
- parser/runtime rejection for unsupported value forms;
- successful persistent `CREATE TABLE` with `ROW_FORMAT`, `KEY_BLOCK_SIZE`,
  `PACK_KEYS`, `CHECKSUM`, and `STATS_*` options;
- metadata through `SHOW CREATE TABLE`, `SHOW TABLE STATUS`, and
  `INFORMATION_SCHEMA.TABLES`;
- default-clearing behavior and duplicate last-value-wins behavior;
- `KEY_BLOCK_SIZE` compression metadata and invalid-combination diagnostics;
- `CREATE TABLE ... LIKE` metadata cloning;
- close/reopen persistence and independent file-backed handles;
- `.mylite` preamble preservation;
- existing parser, catalog, DDL, metadata, and runtime tests still passing.

## Deferred

- `ALTER TABLE` storage/statistics table options;
- full temporary-table storage/statistics option parity;
- `MAX_ROWS`, `MIN_ROWS`, `AVG_ROW_LENGTH`, `TABLESPACE`, `INSERT_METHOD`,
  `UNION`, `PASSWORD`, encryption, secondary engine attributes, engine
  attributes, NDB options, and partition options;
- physical row-format changes, compression, storage-engine statistics,
  histograms, optimizer effects, tablespaces, or alternate engines;
- SQL-mode or `innodb_strict_mode` configurability beyond the verified default
  behavior;
- storage-engine warnings before errors;
- SQLite fork patches.
